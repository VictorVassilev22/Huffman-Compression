#include "Decoder.h"

/// <summary>
/// Can exctract, update, or display info about an archive based on command
/// </summary>
/// <param name="srcPath">Path of the archive</param>
/// <param name="destPath">destination folder for the to be exctracted files</param>
/// <param name="code">command for the decoder</param>
/// <param name="fileName">name of a file to be exctracted (or updated with, when updated full path is required)</param>
/// <param name="enc">encoder needed for update instruction</param>
/// <returns>if instructions are executed successfully</returns>
bool Decoder::decode(const std::string& srcPath, const std::string& destPath,  commandCode code, const std::string& fileName, Encoder* enc)
{
	auto begin = std::chrono::high_resolution_clock::now();
	//path setup
	if (!fs::exists(srcPath)) {
		std::cout << "Such file does not exist!" << std::endl;
		return false;
	}

	if (fs::file_size(srcPath) > MAX_FILE_SIZE) {
		std::cout << "Some of the specified files may be too large. File size must be less than "
			<< (double)MAX_FILE_SIZE / (1024 * 1024 * 1024) << " GB" << std::endl;
		return false;
	}

	if (!checkIntegrity(srcPath)) {
		std::cout << "File is not safe for extraction or is not huffman compressed archive!" << std::endl;
		return false;
	}
	rawBits.free();

	std::ifstream file(srcPath, std::ios::out | std::ios::binary);

	std::vector < fileInfo > files; //used to store the metadata
	readMetaData(file, files);

	if (code == commandCode::extract) {
		extractFiles(files, file, destPath);
	}
	else if (code == commandCode::extractOne) { //check filename exists
		extractOneFile(file, fileName, destPath, files);
	}
	else if (code == commandCode::info) {
		printInfo(files);
	}
	else if (code == commandCode::update) {

		if (!fs::exists(fileName)) {
			std::cout << "Such file does not exist! Please specify a valid file u want to update!" << std::endl;
			return false;
		}

		if (fs::file_size(fileName) > MAX_FILE_SIZE) {
			std::cout << "Some of the specified files may be too large. File size must be less than "
				<< (double)MAX_FILE_SIZE / (1024 * 1024 * 1024) << " GB" << std::endl;
			return false;
		}

		updateFile(srcPath, file, fileName, files, *enc);
	}
	
	auto end = std::chrono::high_resolution_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
	std::cout << "Time measured: " << elapsed.count() * 1e-9 << " seconds"<<std::endl;
	return true;
}

/// <summary>
/// Prints Info about all files
/// </summary>
/// <param name="files">files metadata</param>
void Decoder::printInfo(const std::vector<fileInfo>& files) const
{
	std::cout << "Files list: " << std::endl;
	size_t size = files.size();
	for (size_t i = 0; i < size; i++)
	{
		printFileInfo(files[i]);
	}
}

/// <summary>
/// Checks if file has been corrupted,
/// if so, the file and its checksum would be different and the result of extraction will be unexpected
/// </summary>
/// <param name="srcPath">source archived file path</param>
/// <returns>true - if file is OK, false - if file has been broken</returns>
bool Decoder::checkIntegrity(const std::string& srcPath)
{
	if (!fs::exists(srcPath))
		throw fs::filesystem_error("Compressed file specified does not exist!", std::error_code());

	if(fs::file_size(srcPath)>=MAX_FILE_SIZE)
		throw fs::filesystem_error("File is too big and cannot be a huffman compressed file", std::error_code());

	uint32_t fileCrc = 0;
	uintmax_t fileSize = fs::file_size(srcPath);
	std::ifstream fileIn(srcPath, std::ios::in | std::ios::binary);
	//reading the archived file checksum
	uint32_t crc = crc_32::getFileChecksum(fileIn, fileSize - sizeof(uint32_t));
	fileIn.clear();
	fileIn.seekg(fileSize - sizeof(uint32_t), std::ios::beg);
	//getting the early saved checksum of the file
	fileIn.read(reinterpret_cast<char*>(&fileCrc), sizeof(fileCrc));
	std::streamsize read = fileIn.gcount();

	if (read != sizeof(fileCrc))
		throw std::exception("Could not read crc32.");

	if (crc == fileCrc) //if the two checksums are equal, then the file is intact
		return true;

	return false;
}

/// <summary>
/// Reads metadata obout the files and stores it
/// </summary>
/// <param name="inFile">input file stream</param>
/// <param name="files">Stores metadata about files</param>
void Decoder::readMetaData(std::ifstream& inFile, std::vector<fileInfo>& files)
{
	uint32_t pathsEndPos = 0;
	inFile.read(reinterpret_cast<char*>(&pathsEndPos), sizeof(pathsEndPos));
	inFile.seekg(0, std::ios::end);

	if (inFile.tellg() >= MAX_FILE_SIZE) {
		throw std::exception("File is too big!");
	}

	uint32_t fileSize = (uint32_t)inFile.tellg();

	if (pathsEndPos > fileSize) {
		throw std::exception("File is corrupted and cant be extracted!");
	}

	inFile.seekg(sizeof(pathsEndPos), std::ios::beg);

	std::string strPaths = "";
	size_t idx = 0; //indicates index in the bit vector to know from where to read
	size_t treeStorageSize = 0;
	uint32_t filesCnt = 0;

	//read tree and decode
	uint32_t filesStrSize = 0;
	inFile.read(reinterpret_cast<char*>(&filesStrSize), sizeof(filesStrSize));

	tree* t = nullptr;
	if (!readTree(t, inFile, idx, treeStorageSize))
	{
		Encoder::freeTree(t);
		std::cout << "Tree reading was NOT successful. Cannot continue the extraction." << std::endl;
		return;
	}

	//read filesStrSize bytes and decode into string
	decodeFilePaths(strPaths, t, inFile, filesStrSize, idx);

	inFile.clear();
	inFile.seekg(pathsEndPos, std::ios::beg);

	//saving the metadata of all files
	inFile.read(reinterpret_cast<char*>(&filesCnt), sizeof(filesCnt));
	files.reserve(filesCnt);
	
	std::istringstream iss(strPaths);
	std::string path;
	uint32_t size, start, checksum, end;
	while (std::getline(iss, path, EON))
	{
		inFile.read(reinterpret_cast<char*>(&size), sizeof(size));
		inFile.read(reinterpret_cast<char*>(&start), sizeof(start));
		inFile.read(reinterpret_cast<char*>(&checksum), sizeof(checksum));
		inFile.read(reinterpret_cast<char*>(&end), sizeof(end));

		std::string name;
		Encoder::getFileName(path, name);
		files.push_back(fileInfo{path, name, size, checksum, start, end});
	}
	Encoder::freeTree(t);
}

/// <summary>
/// Extracts all files from the archive into their paths
/// (paths begin from the dest path, then follow file path)
/// </summary>
/// <param name="files">metadata</param>
/// <param name="inFile">archive file stream</param>
/// <param name="destPath">extraction destination path</param>
void Decoder::extractFiles(const std::vector<fileInfo>& files, std::ifstream& inFile, const std::string& destPath)
{

	std::string fileFullPath;
	size_t filesCnt = files.size();
	for (size_t i = 0; i < filesCnt; i++)
	{
		fileFullPath = destPath;
		setupFilePath(files[i].path, fileFullPath);
		std::ofstream outFile(fileFullPath, std::ios::out | std::ios::binary);
		decodeFile(outFile, inFile, files[i].startPos, files[i].endPos, files[i].size);
	}
}

/// <summary>
/// Begining from the source directory, creates all directories from the file path string
/// </summary>
/// <param name="filePath">path of the file</param>
/// <param name="fullPath">source path</param>
void Decoder::setupFilePath(const std::string& filePath, std::string& fullPath)
{
	std::istringstream iss(filePath);
	std::string directory;
	while (std::getline(iss, directory, '\\'))
	{
		if (!fs::is_directory(fs::status(fullPath))) {
			fs::create_directory(fullPath);
		}

		fullPath += '\\';
		fullPath.append(directory);
	}
}

/// <summary>
/// Exctracts a file from an archive
/// </summary>
/// <param name="outFile">destination file stream (extracted file)</param>
/// <param name="srcFile">archived file stream</param>
/// <param name="start">start position of encoded file in archive</param>
/// <param name="end">end position of encoded file in archive</param>
/// <param name="size">size of the file before compression</param>
void Decoder::decodeFile(std::ofstream& outFile, std::ifstream& srcFile, const size_t& start, const size_t& end, const size_t& size)
{
	rawBits.free();
	srcFile.clear();
	srcFile.seekg(start, std::ios::beg);
	tree* t = nullptr;
	size_t idx = 0;
	size_t treeStorageSize = 0;
	if (!readTree(t, srcFile, idx, treeStorageSize)){
		Encoder::freeTree(t);
		std::cout << "Tree reading was NOT successful. Cannot continue the extraction." << std::endl;
		return;
	}
	std::unique_ptr<char[]> buffer(new char[BUFF_SIZE]);
	unsigned char ch = 0;
	size_t cnt = 0;
	readFileChunk(srcFile, buffer, BUFF_SIZE);

	while(cnt < size)
	{
		ensureBitsInVector(treeDepth, idx, srcFile, buffer, BUFF_SIZE);
		ch = readSym(t, idx);
		cnt++;
		outFile.write((char*)&ch, sizeof(ch));
	}

}

/// <summary>
/// Exctracts specified file from an archive (if exists)
/// uses binary search in sorted data to find the file's positions in archive
/// </summary>
/// <param name="file">archived file stream</param>
/// <param name="fileName">name of the file (without path)</param>
/// <param name="destPath">destination full path</param>
/// <param name="files">list of all files (metadata)</param>
/// <returns>if the file has been found and extracted or not</returns>
bool Decoder::extractOneFile(std::ifstream& file, const std::string& fileName, std::string destPath, const std::vector<fileInfo>& files)
{
	long long index = binarySearchFile(files, 0, files.size() - 1, fileName);

	if (index == -1)
		return false;

	setupFilePath(fileName, destPath);
	std::ofstream outFile(destPath, std::ios::out | std::ios::binary);
	decodeFile(outFile, file, files[index].startPos, files[index].endPos, files[index].size);

	return true;
}

/// <summary>
/// Assuming data about files is sorted by file name, finds needed file
/// </summary>
/// <param name="files">data about files (metadata)</param>
/// <param name="left">left index</param>
/// <param name="right">right index (size-1)</param>
/// <param name="name">search key (file name)</param>
/// <returns>int indexing file / -1 if not found</returns>
long long Decoder::binarySearchFile(const std::vector<fileInfo>& files, long long left, long long right, const std::string& name)
{
	if (right < left)
		return -1;

	long long mid = (left + right) / 2;
	
	std::string thisName = files[mid].name;
	int compResult = name.compare(thisName);

	if (compResult==0)
		return mid;

	if (compResult < 0)
		return binarySearchFile(files, left, mid - 1, name);

	return binarySearchFile(files, mid + 1, right, name);
}

/// <summary>
/// Given one fileInfo object prints information about its compression level
/// </summary>
/// <param name="file">Info for the file</param>
void Decoder::printFileInfo(const fileInfo& file) const
{
	size_t compressedSize = (file.endPos - file.startPos);
	std::cout << "Name: " << file.name << " | "
		<< "Size: " << file.size << " bytes. | "
		<< "Compressed to: " << compressedSize << " bytes. | "
		<< "Compression rate: " << 100.f - ((float)compressedSize / file.size) * 100 << '%' << std::endl;
}

/// <summary>
/// finds needed file to be updated,
/// creates dummy temp file and copies unchanged archive contents into it
/// compresses only the updated path anew and writes the new compressed version into the temp file
/// deletes the old archive and renames the temp file with its name
/// computes checksum of the new archive file
/// </summary>
/// <param name="archivedPath">path of the archived file</param>
/// <param name="archivedFile">input stream of the archived file</param>
/// <param name="newFilePath">path of the new(updated) file </param>
/// <param name="files">metadata for files</param>
/// <param name="enc">encryptor used to compress the new version of the file</param>
void Decoder::updateFile(const std::string& archivedPath, std::ifstream& archivedFile, const std::string& newFilePath, const std::vector<fileInfo>& files, Encoder& enc)
{

	//check if such file exists, get index
	std::string newFileName = "";
	Encoder::getFileName(newFilePath, newFileName);
	long long index = binarySearchFile(files, 0, files.size()-1, newFileName);
	if (index < 0) {
		std::cout << "File " << newFileName << " not found in the archive!" << std::endl;
		return;
	}

	const fileInfo& file = files[index];

	//check if file has changed or not
	uint32_t currentCheckSum = file.checksum;
	std::ifstream newFileStream(newFilePath, std::ios::in | std::ios::binary);

	newFileStream.seekg(0, std::ios::end);
	if (newFileStream.tellg() >= MAX_FILE_SIZE) {
		throw std::exception("New file is too big for compression!");
	}

	uint32_t size = newFileStream.tellg();
	newFileStream.seekg(0, std::ios::beg);
	uint32_t newCheckSum = crc_32::getFileChecksum(newFileStream);

	if (currentCheckSum == newCheckSum && file.size == size) {
		std::cout << "File is up to date!" << std::endl;
		return;
	}

	//create new file and write until start position
	std::string newArchivedPath = archivedPath;
	Encoder::pathStepBack(newArchivedPath);
	newArchivedPath += '\\';
	newArchivedPath.append("temp.bin");
	uint32_t startFilePos = file.startPos;
	uint32_t oldEndPos = file.endPos;
	archivedFile.clear();
	archivedFile.seekg(0, std::ios::beg);

	std::ofstream outNewArchived(newArchivedPath, std::ios::out | std::ios::binary);
	copyFileContents(outNewArchived, archivedFile, startFilePos);

	//write the new compressed file
	uint32_t confirmCrc = enc.compressAndWrite(newFilePath, outNewArchived, newFileStream);
	if (confirmCrc != newCheckSum || outNewArchived.tellp() >= MAX_FILE_SIZE) {
		remove(archivedPath.c_str());
		throw std::exception("Error occured compressing newer version of file!");
	}

	uint32_t newEndPos = outNewArchived.tellp();

	//write the rest of the files
	archivedFile.seekg(0, std::ios::end);
	size_t upperBound = (size_t)archivedFile.tellg() - oldEndPos;
	archivedFile.seekg(oldEndPos, std::ios::beg);
	copyFileContents(outNewArchived, archivedFile, upperBound);

	std::ifstream inNewArchived(newArchivedPath, std::ios::in | std::ios::binary);
	changeMetadata(index, newEndPos, newCheckSum, size, oldEndPos, inNewArchived, outNewArchived, files);

	std::string archivedFileName = "";
	Encoder::getFileName(archivedPath, archivedFileName);
	std::string tempName = newArchivedPath;
	Encoder::pathStepBack(newArchivedPath);
	newArchivedPath += '\\';
	newArchivedPath.append(archivedFileName);

	archivedFile.close();
	newFileStream.close();
	outNewArchived.close();
	inNewArchived.close();

	//delete old file
	remove(archivedPath.c_str());
	//rename new file
	if (rename(tempName.c_str(), newArchivedPath.c_str()) != 0) {
		std::cout << "Error renaming file" << std::endl;
	}

	//compute checksum
	enc.appendCheckSumToFile(newArchivedPath);
}

/// <summary>
/// Copies file contents from one file to another
/// </summary>
/// <param name="outFile">outut file stream</param>
/// <param name="inFile">input file stream</param>
/// <param name="upperBound"> how many bytes to copy </param>
void Decoder::copyFileContents(std::ofstream& outFile, std::ifstream& inFile, const size_t upperBound)
{
	size_t buffSize = std::min((size_t)BUFF_SIZE, upperBound);
	char* buf = new char[buffSize];
	size_t bytesLeft = upperBound;

	do {
		inFile.read(buf, buffSize);
		bytesLeft -= buffSize;
		buffSize = std::min(bytesLeft, buffSize);
		outFile.write(buf, inFile.gcount());
	} while (inFile.gcount() > 0 && buffSize > 0);

	delete[] buf;
}

/// <summary>
/// changes metadata from one file (updated) to the end of the files info list
/// </summary>
/// <param name="index">index of the updated file</param>
/// <param name="newEndPos">new end position for updated file</param>
/// <param name="newCheckSum">new checksum for updated file</param>
/// <param name="newSize">new size for updated file</param>
/// <param name="oldEndPos">old end position before updating the file</param>
/// <param name="inFile">input file stream of the archive</param>
/// <param name="outFile">output file stream of the archive</param>
/// <param name="files">list of files (metadata)</param>
void Decoder::changeMetadata(const uint32_t index, const uint32_t newEndPos, const uint32_t newCheckSum, const uint32_t newSize,
								const uint32_t oldEndPos, std::ifstream& inFile, std::ofstream& outFile, const std::vector<fileInfo>& files)
{
	inFile.clear();
	outFile.clear();

	inFile.seekg(0, std::ios::beg);

	uint32_t filesStrEndPos = 0;
	uint32_t filesCnt  = 0;
	inFile.read((char*)&filesStrEndPos, sizeof(filesStrEndPos));

	//read files cnt
	inFile.seekg(filesStrEndPos);
	inFile.read((char*)&filesCnt, sizeof(filesCnt));

	uint32_t fileChangePos = filesStrEndPos + 4 * index * sizeof(uint32_t) + sizeof(filesCnt);

	outFile.seekp(fileChangePos);
	outFile.write((char*)&newSize, sizeof(newSize));

	fileChangePos += 2 * sizeof(uint32_t); // over size and startpos
	outFile.seekp(fileChangePos);
	outFile.write((char*)&newCheckSum, sizeof(newCheckSum));
	outFile.write((char*)&newEndPos, sizeof(newEndPos));
	fileChangePos += 2 * sizeof(uint32_t); // over checksum and endPos
	long diff = (long)newEndPos - oldEndPos;
	uint32_t fileEndPos = 0;
	uint32_t fileStratPos = 0;

	for (size_t i = index+1; i < filesCnt; i++)
	{
		fileStratPos = files[i].startPos + diff;
		fileEndPos = files[i].endPos + diff;
		fileChangePos += sizeof(uint32_t); //over filesize
		outFile.seekp(fileChangePos);
		outFile.write((char*)&fileStratPos, sizeof(fileStratPos));
		fileChangePos += 2*sizeof(uint32_t); //over startpos and checksum
		outFile.seekp(fileChangePos);
		outFile.write((char*)&fileEndPos, sizeof(fileEndPos));
		fileChangePos += sizeof(uint32_t); //over endpos
	}
}

/// <summary>
/// reads tree from archived file.
/// also computes tree depth
/// </summary>
/// <param name="t">tree node for the result</param>
/// <param name="file">input file stream</param>
/// <param name="idx">index in bit vector</param>
/// <param name="treeStorageSize">tree srorage size in bytes</param>
/// <returns>wether the tree has been successfully read</returns>
bool Decoder::readTree(tree*& t, std::ifstream& file, size_t& idx, size_t& treeStorageSize)
{
	uint32_t treeSize = 0; //size of the tree in bits
	size_t treeStorage = 0; //tree stored in bytes

	//reading the size of the tree
	file.read((char*)(&treeSize), sizeof(treeSize));

	if (treeSize > MAX_TREE_SIZE) {
		throw std::exception("Size of tree is not correct. File has been corrupted!");
	}
	//bits to bytes
	if (treeSize % BYTE_SIZE != 0) {
		treeStorage = (size_t)(treeSize / BYTE_SIZE) + 1;
	}
	else {
		treeStorage /= BYTE_SIZE;
	}

	treeStorageSize = treeStorage;

	//reading the tree itself
	std::unique_ptr<char[]> treeBuffer(new char[treeStorageSize]);
	size_t treeDepth = 0;

	readFileChunk(file, treeBuffer, treeStorageSize);
	readTreeRec(t, idx);
	//getting tree depth
	getTreeDepth(t, 0, treeDepth);
	this->treeDepth = treeDepth;
	//free bitvector and reset index
	rawBits.free();
	idx = 0;

	//read end of tree symbol to ensure tree is read correctly
	unsigned char ch = 0;
	file.read((char*)(&ch), sizeof(ch));

	return (ch == EOT);
}

/// <summary>
/// reads tree
/// </summary>
/// <param name="t">current tree node</param>
/// <param name="idx">index in bit vector</param>
void Decoder::readTreeRec(tree*& t, size_t& idx)
{
	tree* left = nullptr;
	tree* right = nullptr;

	bool bit = rawBits[idx];
	idx++;

	// if its not a leaf
	if (bit == 1) {
		readTreeRec(left, idx);
		readTreeRec(right, idx);
		t = new tree(0, 0, left, right);
	}
	else { //it is a leaf
		//read next 8 bits
		unsigned char sym = readTreeSym(idx);
		t = new tree(0, sym);
	}

}

/// <summary>
/// reads symbol stored in tree
/// </summary>
/// <param name="idx">index indicating position in bit vector</param>
/// <returns>needed byte (symbol)</returns>
unsigned char Decoder::readTreeSym(size_t& idx)
{
	std::bitset<TREE_DATA_SIZE> bitSym;
	for (size_t i = 0; i < TREE_DATA_SIZE; i++)
	{
		bitSym[i] = rawBits[idx];
		idx++;
	}
	return (unsigned char)bitSym.to_ulong();
}

/// <summary>
/// given a buffer and a file loads the bits of the file into a vector
/// </summary>
/// <param name="file">stream of the file</param>
/// <param name="buffer">The buffer</param>
/// <param name="storageSize">The size of data to be put in the buffer</param>
/// <param name="rawBits">the product vector of bits</param>
size_t Decoder::readFileChunk(std::ifstream& file, std::unique_ptr<char[]>& buffer, size_t storageSize)
{
	file.read(buffer.get(), storageSize);
	size_t charsCnt = file.gcount();

	std::vector<std::bitset<BYTE_SIZE>> bits;
	//read every byte into this vector
	for (size_t i = 0; i < charsCnt; i++)
	{
		bits.push_back(std::bitset<BYTE_SIZE>(buffer[i]));
	}
	//load all into bitVector
	for (size_t i = 0; i < charsCnt; i++)
	{
		for (size_t j = 0; j < BYTE_SIZE; j++)
		{
			rawBits.push_back(bits[i][j]);
		}
	}
	
	return charsCnt;
}

/// <summary>
/// decodes symbol from bit vector using the tree
/// </summary>
/// <param name="t">the huffman coding tree</param>
/// <param name="idx">index in bit vector</param>
/// <returns>decoded byte (symbol)</returns>
unsigned char Decoder::readSym(const tree* t, size_t& idx)
{
	if (!(t->left || t->right))
		return t->sym;

	if (rawBits[idx] == 0)
		return readSym(t->left, ++idx);

	return readSym(t->right, ++idx);
}

/// <summary>
/// Used to decode file paths metadata
/// </summary>
/// <param name="paths">result path as whole string</param>
/// <param name="t">huffman coding tree</param>
/// <param name="file">input file stream of archive</param>
/// <param name="storageSize">storage size of the string paths metadata</param>
/// <param name="idx">index position in bit vector</param>
void Decoder::decodeFilePaths(std::string& paths, const tree* t, std::ifstream& file, const size_t& storageSize, size_t& idx)
{
	char ch = 0;
	//size_t size = std::min(storageSize, BUFF_SIZE);
	std::unique_ptr<char[]> buffer(new char[BUFF_SIZE]);
	readFileChunk(file, buffer, BUFF_SIZE);
	for (size_t i = 0; i < storageSize; i++)
	{
		ensureBitsInVector(treeDepth, idx, file, buffer, BUFF_SIZE);
		ch = readSym(t, idx);
		paths += ch;
	}
	rawBits.free();
	idx = 0;
}

/// <summary>
/// makes sure bit vector has at least bitsCnt bits for the next read request
/// usually I check with the depth of the tree as the maximum length of encoded byte
/// </summary>
/// <param name="bitsCnt">bits needed to be available for the next read</param>
/// <param name="idx">index of the next bit to be read</param>
/// <param name="file">input file stream</param>
/// <param name="buffer">buffer needed to read new data</param>
/// <param name="storageSize">how many bytes to read next (or size of buffer)</param>
void Decoder::ensureBitsInVector(const size_t bitsCnt, size_t& idx, std::ifstream& file, std::unique_ptr<char[]>& buffer, size_t storageSize)
{
	if (idx >= rawBits.size() - bitsCnt) {
		rawBits.free(idx, true);
		idx = 0;
		readFileChunk(file, buffer, storageSize);
	}
}

/// <summary>
/// computes depth of huffman encoding tree
/// </summary>
/// <param name="t">huffman encoding tree</param>
/// <param name="depth">temp value</param>
/// <param name="maxDepth">result value</param>
void Decoder::getTreeDepth(const tree* t, size_t depth, size_t& maxDepth)
{
	if (!t)
		return;

	if (Encoder::isLeaf(t)) {
		maxDepth = std::max(depth, maxDepth);
	}
	else {
		getTreeDepth(t->left, ++depth, maxDepth);
		depth--;
		getTreeDepth(t->right, ++depth, maxDepth);
		depth--;
	}

}
