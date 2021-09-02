#include "Encoder.h"

/// <summary>
/// Creates archive from paths of files and folders
/// </summary>
/// <param name="srcPath">String listing files and folders</param>
/// <param name="destPath">Path of archive includingh name</param>
/// <returns> (bool) Wether of not the function has created the whole archive</returns>
bool Encoder::encode(const std::string& srcPath, const std::string& destPath) {
	std::cout << "Encoding..." << std::endl;
	auto begin = std::chrono::high_resolution_clock::now();

	//write all file paths as compressed string metadata
	std::vector < std::string > fullPaths;
	std::vector < std::string > allFiles;
	std::vector < std::string > allFilesTrimmed;
	std::vector < std::string > allFilesNames;

	formatAllPaths(srcPath, fullPaths);

	uint32_t pathsSize = fullPaths.size();
	std::string pathsStr = ""; //in this string all paths will be stored as metadata

	std::cout << "Contents: " << std::endl;
	for (size_t i = 0; i < pathsSize ; i++)
	{
		if (!readFilePaths(fullPaths[i], allFilesTrimmed, allFiles))
			return false;
	}

	std::string name;
	for (size_t i = 0; i < filesCnt; i++)
	{
		getFileName(allFilesTrimmed[i], name);
		allFilesNames.push_back(name);
	}

	//sort allFilesTrimmed, allFiles and all files names according to allFilesNames
	quickSort(allFilesNames, allFilesTrimmed, allFiles, 0, filesCnt-1);
	//then make strSize
	vectorToFilesStr(allFilesTrimmed, pathsStr);

	if (pathsStr.size() >= MAX_FILE_SIZE) {
		throw std::exception("File path description was too large!");
	}

	std::ofstream destFile(destPath, std::ios::out | std::ios::binary);
	//at the very beggining of the file write number (later is rewritten to end position of paths metadata)
	destFile.write((char*)&pathsSize, sizeof(pathsSize));
	posCnt += sizeof(pathsSize);
	
	//encoding the string metadata using the Huffman algorithm
	readStringFrequencies(pathsStr); //counting frequencies of each byte
	tree* huffmanTree = buildHuffmanTree(); //building the Huffman tree
	size_t trDpth = 0;
	std::vector<bool> tmpVec;
	extractCodes(huffmanTree, tmpVec, trDpth); // computing the code of every byte
	writeCompressedStringToFile(huffmanTree, pathsStr, destFile); //writing the compressed string
	writeEnd(destFile); //writing last remaining bytes
	freeTree(huffmanTree); //free tree memory

	destFile.seekp(0);
	destFile.write((char*)&posCnt, sizeof(posCnt)); //write in the beginning where string metadata ends
	destFile.seekp(posCnt);

	destFile.write((char*)&filesCnt, sizeof(filesCnt)); //writing how many files are in the archive
	posCnt += sizeof(filesCnt);
	fileMetaPos = posCnt; //setting the position of the files metadata to the current position in file

	//reserving space for size, startpos, checksum and endpos for every file in metadata
	uint32_t dataCount = 4 * filesCnt;
	for (size_t i = 0; i < dataCount; i++)
		destFile.write((char*)&dataCount, sizeof(dataCount));
	posCnt += dataCount * sizeof(dataCount);
	
	for (size_t i = 0; i < filesCnt; i++)
	{
		if (!writeCompressedFile(allFiles[i], destFile))
			return false;
	}

	destFile.close();
	appendCheckSumToFile(destPath);

	auto end = std::chrono::high_resolution_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
	std::cout << "Time measured: "<< elapsed.count() * 1e-9  << " seconds"<<std::endl;

	clearData();
	return true;
}

/// <summary>
/// writes tree and compressed file to archive using Huffman algorithm and calculates file checksum
/// </summary>
/// <param name="srcPath">string path of the file</param>
/// <param name="destFile">output stream</param>
/// <param name="srcFile">input stream of the file we read from</param>
/// <returns>checksum of the file</returns>
uint32_t Encoder::compressAndWrite(const std::string& srcPath, std::ofstream& destFile, std::ifstream& srcFile)
{
	//clear bincode, frequencies and map
	binCode.free();
	clearFrequencies();
	clearCodes();
	//compute frequencies
	computeFrequencies(srcPath);
	//build tree
	tree* t = buildHuffmanTree();
	//extract codes
	std::vector<bool> tempVec;
	tempVec.reserve(CHARS_CNT);
	size_t depth = 0;
	extractCodes(t, tempVec, depth);
	//write tree
	writeTreeToFile(t, destFile);
	//write file
	uint32_t crc = writeFileToVector(srcFile, destFile);

	//write end
	writeEnd(destFile);
	//free tree from memory
	freeTree(t);
	return crc;
}

/// <summary>
/// Given a tree node tells if its leaf or not (Works for Huffman type tree nodes only)
/// </summary>
/// <param name="t">ponter to tree node</param>
/// <returns>(bool) is leaf or not</returns>
bool Encoder::isLeaf(const tree* t)
{
	if (!(t->left || t->right)) {
		return true;
	}

	return false;
}

/// <summary>
/// deletes last folder/file from the path
/// </summary>
/// <param name="path">A source path</param>
void Encoder::pathStepBack(std::string& path)
{
	int lastIdx = path.size() - 1;
	while (path[lastIdx] != '\\') {
		lastIdx--;
		if (lastIdx < 0)
			throw fs::filesystem_error(std::string("Invalid path occured during processing"), std::error_code());
		path.pop_back();
	}

	path.pop_back();
}

/// <summary>
/// Transforms input string to full paths
/// </summary>
/// <param name="str"> raw input string </param>
/// <param name="result"> vector of full paths </param>
void Encoder::formatAllPaths(const std::string& str, std::vector<std::string>& result)
{
	std::string temp;
	std::istringstream ss(str);

	std::vector<std::string> paths;

	while (std::getline(ss, temp, pathDelimeter)) {
		paths.push_back(temp);
	}

	bool isFirst = true;
	size_t dSize = paths.size();
	std::string tempSrc;
	for (size_t i = 0; i < dSize; i++)
	{
		std::istringstream sp(paths[i]);
		while (std::getline(sp, temp, fileDelimeter)) {
			if (isFirst) {
				result.push_back(temp);
				pathStepBack(temp);
				tempSrc = temp;
				isFirst = false;
			}
			else {
				tempSrc += '\\';
				tempSrc.append(temp);
				result.push_back(tempSrc);
				pathStepBack(tempSrc);
			}
		}
		isFirst = true;
	}

}

/// <summary>
/// Reads all paths and stores them as paths beginning from the specified directory (not full paths). Also reads the number of files.
/// </summary>
/// <param name="path">Full path of the file (folder)</param>
/// <param name="result">vector which stores trimmed (only from specified directory) file paths after function is executed</param>
/// <param name="files">vector which stores full file paths after function is executed</param>
/// <returns>(bool) whether function has read and written all the files</returns>
bool Encoder::readFilePaths(const std::string& path, std::vector<std::string>& result, std::vector<std::string>& files)
{
	if (!fs::exists(path)) {
		std::cout << "A file or directory specified does not exist!" << std::endl;
		return false;
	}

	std::string fileName;
	if (fs::status(path).type() != fs::file_type::directory) {
		getFileName(path, fileName);
		result.push_back(fileName);
		files.push_back(path);
		filesCnt++;
		std::cout << path << '\n';
		return true;
	}

	size_t lastIdx = path.size() - 1;

	for (auto i = fs::recursive_directory_iterator(path);
		i != fs::recursive_directory_iterator();
		++i) {

		fs::path currPath = i->path();
		fs::file_status status = fs::status(currPath);
		fs::file_type type = status.type();

		
		if (type != fs::file_type::directory) {

			size_t depth = i.depth() + 1;
			for (size_t i = 0; i < depth; i++)
			{
				while (path[lastIdx] != '\\')
					lastIdx--;
			}

			lastIdx++;

			std::string trimmedPath = currPath.string().substr(lastIdx);
			result.push_back(trimmedPath);
			filesCnt++;
			files.push_back(currPath.string());
		}

		std::cout << currPath << '\n';
	}
	return true;
}

/// <summary>
/// If the path is to a file, compute file bytes frequencies
/// </summary>
/// <param name="path">Source file path</param>
void Encoder::computeFrequencies(const std::string& path)
{
	fs::path thisPath(path);
	if (fs::status(thisPath).type() != fs::file_type::directory)
		readFileFrequencies(thisPath);
}

/// <summary>
/// clears file frequencies
/// </summary>
void Encoder::clearFrequencies()
{
	for (size_t i = 0; i < CHARS_CNT; i++)
	{
		freq[i] = 0;
	}
}

void Encoder::clearCodes()
{
	for (size_t i = 0; i < CHARS_CNT; i++)
	{
		huffmanCodes[i].clear();
	}
}

/// <summary>
/// Counts byte frequencies from the file
/// </summary>
/// <param name="path">Source path</param>
void Encoder::readFileFrequencies(const fs::path& path)
{
	std::ifstream file(path, std::ios::in | std::ios::binary);
	std::unique_ptr<char[]> buffer(new char[BUFF_SIZE]);
	while (!file.eof())
	{
		file.read(buffer.get(), BUFF_SIZE);
		size_t size = file.gcount();
		for (size_t i = 0; i < size; i++)
		{
			freq[(unsigned char)buffer[i]]++;
		}
	}
}

//conts the frequency of each character in the string
void Encoder::readStringFrequencies(const std::string& str)
{
	size_t size = str.size();
	for (size_t i = 0; i < size; i++)
	{
		freq[(unsigned char)str[i]]++;
	}
}

//returns how many bytes is written during the string decoding(not all)
void Encoder::writeCompressedStringToFile(const tree* t, const std::string& str, std::ofstream& destFile)
{
	uint32_t strSize = str.size();
	destFile.write((char*)&strSize, sizeof(strSize)); 
	posCnt += sizeof(strSize);
	writeTreeToFile(t, destFile);
	for (size_t i = 0; i < strSize; i++)
	{
		writeSymbolToVector(str[i]);
		if (binCode.size() >= BOOL_VEC_CAPACITY*WRITE_DATA_SIZE - treeDepth)
			posCnt += binCode.writeToFile(destFile);
	}
	posCnt += binCode.writeToFile(destFile);
}

/// <summary>
/// Builds Huffman tree using the Huffman algorithm (with priority queue)
/// </summary>
/// <returns>Pointer to the root of the huffman tree</returns>
tree* Encoder::buildHuffmanTree()
{
	//create the huffman forest
	//in priority queue
	std::priority_queue<tree*, std::vector<tree*>, compareTrees> pq;
	for (size_t i = 0; i < CHARS_CNT; i++)
	{
		if (freq[i] != 0) {
			pq.push(new tree(freq[i], i));
		}
	}

	//reduce to huffman tree
	int freq1 = 0;
	int freq2 = 0;
	while (pq.size() != 1) {
		tree* newT = new tree();
		tree* prev1 = pq.top();
		pq.pop();
		tree* prev2 = pq.top();
		pq.pop();

		newT->left = prev1;
		newT->right = prev2;

		newT->freq = prev1->freq + prev2->freq;
		pq.push(newT);
	}

	//return pointer to the huffman tree
	tree* newT = pq.top();
	pq.pop();
	return newT;
}


//frees the tree from memory
void Encoder::freeTree(tree* t)
{
	if (!t)
		return;

	if (isLeaf(t)) {
		delete t;
		t = nullptr;
	}
	else {
		freeTree(t->left);
		freeTree(t->right);
		delete t;
		t = nullptr;
	}
}

//exctracts binary code for each byte depending on its position in the tree
void Encoder::extractCodes(const tree* t, std::vector<bool>& tempVec, size_t& trDpth)
{
	if (!t)
		return;
	
	trDpth++;

	if (isLeaf(t)) {
		huffmanCodes[(unsigned char)t->sym] = tempVec;
		treeDepth = std::max(treeDepth, trDpth);
	}
	else { //if we turn left write 0
		tempVec.push_back(0);
		extractCodes(t->left, tempVec, trDpth);
		trDpth--;
		tempVec.pop_back();
		//if we turn left write 1
		tempVec.push_back(1);
		extractCodes(t->right, tempVec, trDpth);
		trDpth--;
		tempVec.pop_back();
	}

}

/// <summary>
/// Writes Huffman tree, compressed file and fills metadata with file size, start and
/// end positions and checksum
/// </summary>
/// <param name="srcPath"></param>
/// <param name="destFile"></param>
/// <returns></returns>
bool Encoder::writeCompressedFile(const std::string& srcPath, std::ofstream& destFile)
{
	//read size of file
	if (fs::file_size(srcPath) > MAX_FILE_SIZE) {
		std::cout << "Some of the specified files may be too large. File size must be less than "
			<< (double)MAX_FILE_SIZE / (1024 * 1024 * 1024) << " GB" << std::endl;
		return false;
	}
	uint32_t fileSize = fs::file_size(srcPath);
	//create ifstream
	std::ifstream srcFile(srcPath, std::ios::in | std::ios::binary);
	//write size of file
	destFile.seekp(fileMetaPos);
	destFile.write((char*)&fileSize, sizeof(fileSize));
	fileMetaPos += sizeof(fileSize);
	//write start position
	destFile.write((char*)&posCnt, sizeof(posCnt));
	fileMetaPos += sizeof(posCnt);
	destFile.seekp(posCnt);
	uint32_t crc = compressAndWrite(srcPath, destFile, srcFile);
	//write checksum to file
	destFile.seekp(fileMetaPos);
	destFile.write((char*)&crc, sizeof(crc));
	fileMetaPos += sizeof(crc);
	destFile.write((char*)&posCnt, sizeof(posCnt));
	fileMetaPos += sizeof(posCnt);
	destFile.seekp(posCnt);
	return true;
}

/// <summary>
/// writes the tree to the bit vector (recursive)
/// </summary>
/// <param name="t">root node</param>
void Encoder::writeTreeToVec(const tree* t)
{
	if (t) {
		if (isLeaf(t)) {
			binCode.push_back(0);
			writeSymRaw(t->sym);
		}
		else {
			binCode.push_back(1);
			writeTreeToVec(t->left);
			writeTreeToVec(t->right);
		}
	}
}

/// <summary>
/// Writing the Huffman tree to file
/// </summary>
/// <param name="t">tree node</param>
/// <param name="destFile">destination file stream</param>
void Encoder::writeTreeToFile(const tree* t, std::ofstream& destFile)
{
	//write tree to bin vector
	writeTreeToVec(t);
	uint32_t treeSize = binCode.size(); //size of tree in bits 
	if (treeSize > MAX_TREE_SIZE) {
		throw std::exception("Size of tree exceeded maximum size of bits. Cannot compress file.");
	}
	destFile.write((char*)&treeSize, sizeof(treeSize));
	posCnt += sizeof(treeSize);

	while (binCode.size() % BYTE_SIZE != 0) {
		binCode.push_back(0);
	}

	//write end of tree
	writeSymRaw(EOT);
	//writing the tree to the file
	posCnt += binCode.writeToFile(destFile);
}

/// <summary>
/// Writes symbol into bit vector as it is (not the compressed but the original)
/// </summary>
/// <param name="sym"></param>
void Encoder::writeSymRaw(char sym)
{
	std::bitset<TREE_DATA_SIZE> bits(sym);

	for (size_t i = 0; i < TREE_DATA_SIZE; i++)
	{
		binCode.push_back(bits[i]);
	}
}

/// <summary>
/// bitVectors writes whole 32b values to the file, so there is a little left from it as remainder
/// pushing zeroes untill we have full byte and write the remainder byte-by-byte
/// </summary>
/// <param name="file">output stream</param>
/// <returns>how many bytes are written</returns>
uint32_t Encoder::writeEnd(std::ofstream& file)
{	
	while (binCode.size() % BYTE_SIZE != 0) {
		binCode.push_back(0);
	}

	uint32_t bytesCnt = binCode.size() / BYTE_SIZE;
	uint32_t idx = 0;
	std::bitset<BYTE_SIZE> byte;

	for (size_t i = 0; i < bytesCnt; i++)
	{
		for (size_t j = 0; j < BYTE_SIZE; j++)
		{
			byte[j] = binCode[idx];
			idx++;
		}
		unsigned long num = byte.to_ulong();
		unsigned char c = static_cast<unsigned char>(num);
		file.write((char*)&c, sizeof(c));
		posCnt += sizeof(c);
	}
	
	return bytesCnt;
}

/// <summary>
/// computes checksum of the file and writes it at the end
/// </summary>
/// <param name="path">Path to the cource file</param>
void Encoder::appendCheckSumToFile(const std::string& path)
{
	std::ifstream fileIn(path, std::ios::in | std::ios::binary);
	uint32_t crc = crc_32::getFileChecksum(fileIn);
	std::ofstream fileOut(path, std::ios::out | std::ios::app | std::ios::binary);
	fileOut.write(reinterpret_cast<const char*>(&crc), sizeof(crc));

}

/// <summary>
/// Writes a symbol's huffman code into the bit vector
/// </summary>
/// <param name="sym"></param>
void Encoder::writeSymbolToVector(unsigned char sym)
{
	const size_t size = huffmanCodes[sym].size();
	for (size_t i = 0; i < size; i++)
	{
		binCode.push_back(huffmanCodes[sym][i]);
	}
}

/// <summary>
/// Given input and output streams writes compressed code to bitvector
/// (if vector is filled the data is written to the file and the vector is emptied)
/// </summary>
/// <param name="file">input file stream</param>
/// <param name="destFile">output file stream</param>
/// <returns>Crc_32 checksum of the file</returns>
uint32_t Encoder::writeFileToVector(std::ifstream& file, std::ofstream& destFile)
{
	uint32_t crc = 0xFFFFFFFF; // crc checksum of the file
	unsigned char b = 0;

	std::unique_ptr<char[]> buffer(new char[BUFF_SIZE]);

	size_t bytesRead = 1;
	file.clear();
	file.seekg(0);
	while (bytesRead != 0)
	{
		file.read(buffer.get(), BUFF_SIZE);
		bytesRead = file.gcount();

		for (size_t i = 0; i < bytesRead; i++)
		{
			b = (unsigned char)buffer[i];
			writeSymbolToVector(b);
			crc_32::updateCRC(crc, b);
		}

		if (binCode.size() >= BOOL_VEC_CAPACITY*WRITE_DATA_SIZE - treeDepth) {
			posCnt += binCode.writeToFile(destFile);
		}
	}

	crc ^= 0xFFFFFFFF;
	return crc;
}

//ordinary move swap for strings
void Encoder::moveSwap(std::string& a, std::string& b)
{
	std::string temp = std::move(a);
	a = std::move(b);
	b = std::move(temp);
}

//partition for the quicksort
int Encoder::partition(std::vector<std::string>& vec, std::vector<std::string>& vec2, std::vector<std::string>& vec3, int left, int right)
{
	std::string& pivot = vec[right];
	int idx = left - 1;

	for (size_t i = left; i < right; i++)
	{
		if (vec[i].compare(pivot) <= 0) {
			idx++;
			moveSwap(vec[idx], vec[i]);
			moveSwap(vec2[idx], vec2[i]);
			moveSwap(vec3[idx], vec3[i]);
		}
	}
	moveSwap(vec[idx+1], vec[right]);
	moveSwap(vec2[idx+1], vec2[right]);
	moveSwap(vec3[idx+1], vec3[right]);
	return idx+1;
}

/// <summary>
/// Sorts three vectors of strings according to the data of the first one
/// </summary>
/// <param name="vec"> primary vector (the data to be sorted right)</param>
/// <param name="vec2">secondary</param>
/// <param name="vec3">tertiary</param>
/// <param name="left">from</param>
/// <param name="right">to</param>
void Encoder::quickSort(std::vector<std::string>& vec, std::vector<std::string>& vec2, std::vector<std::string>& vec3, int left, int right)
{
	if (left >= right)
		return;

	int pivot = partition(vec, vec2, vec3, left, right);
	quickSort(vec, vec2, vec3, left, pivot - 1);
	quickSort(vec, vec2, vec3, pivot+1, right);
}

/// <summary>
/// appends all filepaths to a single string with a symbol as separator
/// </summary>
/// <param name="paths">vector of paths</param>
/// <param name="pathsStr">resulting string</param>
void Encoder::vectorToFilesStr(const std::vector<std::string>& paths, std::string& pathsStr)
{
	size_t size = paths.size();
	for (size_t i = 0; i < size; i++)
	{
		pathsStr.append(paths[i]);
		pathsStr += EON;
	}
}

/// <summary>
/// clears the object's data 
/// </summary>
void Encoder::clearData()
{
	clearFrequencies();
	clearCodes();
	binCode.free();

	treeDepth = 0;
	posCnt = 0;
	filesCnt = 0;
	fileMetaPos = 0;
}

/// <summary>
/// Given a path, extracts only the name of the final file (folder)
/// </summary>
/// <param name="path">The source path</param>
/// <param name="result">The result name</param>
void Encoder::getFileName(const std::string& path, std::string& result)
{
	size_t size = path.size();
	size_t lastIdx = 0;
	for (size_t i = 0; i < size; i++)
	{
		if (path[i] == '\\')
			lastIdx = i;
	}
	if (lastIdx != 0)
		result = path.substr(lastIdx + 1);
	else
		result = path;
}
