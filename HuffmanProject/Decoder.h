#pragma once
#include "Encoder.h"

/// <summary>
/// A structure to store a single file metadata
/// </summary>
struct fileInfo {
	std::string path;
	std::string name;
	uint32_t size;
	uint32_t checksum;
	uint32_t startPos;
	uint32_t endPos;
};

/// <summary>
/// Specifies codes for instructions for the decoder
/// </summary>
enum class commandCode {
	extract = 0,
	extractOne,
	info,
	update
};

class Decoder {
	size_t treeDepth = 0;
	bitVector rawBits;
public:
	//exctracts one or more files from an archive
	bool decode(const std::string& srcPath, const std::string& destPath, commandCode code = commandCode::extract, const std::string& fileName = "", Encoder* enc = nullptr);
	//checks if file has been corrupted
	bool checkIntegrity(const std::string& srcPath);
private:
	void printInfo(const std::vector<fileInfo>& files) const;
	void readMetaData(std::ifstream& inFile, std::vector<fileInfo>& files);
	void extractFiles(const std::vector<fileInfo>& files, std::ifstream& inFile, const std::string& destPath);
	void setupFilePath(const std::string& filePath, std::string& fullPath);
	void decodeFile(std::ofstream& outFile, std::ifstream& srcFile, const size_t& start, const size_t& end, const size_t& size);
	bool extractOneFile(std::ifstream& file, const std::string& fileName, std::string destPath, const std::vector<fileInfo>& files);
	long long binarySearchFile(const std::vector<fileInfo>& files, long long left, long long right, const std::string& name);
	void printFileInfo(const fileInfo& file) const;
	void updateFile(const std::string& archivedPath, std::ifstream& archivedFile, const std::string& newFilePath, const std::vector<fileInfo>& files, Encoder& enc);
	void copyFileContents(std::ofstream& outFile, std::ifstream& archivedFile, const size_t upperBound);
	void changeMetadata(const uint32_t index, const uint32_t newEndPos, const uint32_t newCheckSum, const uint32_t newSize,
						const uint32_t oldEndPos, std::ifstream& inFile, std::ofstream& outFile, const std::vector<fileInfo>& files);


	bool readTree(tree*& t, std::ifstream& file, size_t& idx, size_t& treeStorageSize);
	void readTreeRec(tree*& t, size_t& idx);
	unsigned char readTreeSym(size_t& idx);
	size_t readFileChunk(std::ifstream& file, std::unique_ptr<char[]>& buffer, size_t storageSize);
	unsigned char readSym(const tree* t, size_t& idx);
	void decodeFilePaths(std::string& paths, const tree* t, std::ifstream& file, const size_t& storageSize, size_t& idx);
	void ensureBitsInVector(const size_t bitsCnt, size_t& idx, std::ifstream& file, std::unique_ptr<char[]>& buffer, size_t storageSize);
	void getTreeDepth(const tree* t, size_t depth, size_t& maxDepth);

};