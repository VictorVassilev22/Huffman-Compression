#pragma once

#include "crc32.hpp"
#include "bitVector.h"
#include<unordered_map>
#include <filesystem>
#include<queue>
#include<stdexcept>
#include <chrono>
#include <sstream> 
#include <filesystem>


//flags:
#define END '|' //end of huffman code
#define SOT ':' //start of tree
#define EOT '/' //end of tree
#define EON '<' //end of name (names of files and directories)

const char pathDelimeter = '?';
const char fileDelimeter = '*';

const uint32_t CHARS_CNT = 256;
const uint32_t NUM_WRITE_SIZE = sizeof(uint32_t) * BYTE_SIZE;
const uint32_t TREE_DATA_SIZE = sizeof(char) * BYTE_SIZE; //size of data stored in the tree (as symbol codes)
const uint32_t MAX_TREE_SIZE = (BYTE_SIZE + 1) * CHARS_CNT + CHARS_CNT - 1; //max size of TREE in bits

namespace fs = std::filesystem;

struct tree {
	char sym = 0;
	uint32_t freq = 0;
	tree *left, *right;

	tree(uint32_t freq = 0, unsigned char sym = 0, tree* left = nullptr, tree* right = nullptr) {
		this->sym = sym;
		this->freq = freq;
		this->left = left;
		this->right = right;
	}
};

struct compareTrees {
	bool operator()(const tree* t1, const tree* t2) {
		return t1->freq > t2->freq;
	}
};

class Encoder {
	bitVector binCode;
	uint32_t freq[CHARS_CNT];
	std::vector<bool> huffmanCodes[CHARS_CNT];

	size_t treeDepth = 0;
	uint32_t filesCnt = 0;
	uint32_t posCnt = 0;
	uint32_t fileMetaPos = 0;
public:
	//creates the whole archive
	bool encode(const std::string& srcPath, const std::string& destPath);
	uint32_t compressAndWrite(const std::string& srcPath, std::ofstream& destFile, std::ifstream& srcFile);
	void appendCheckSumToFile(const std::string& path);
	static bool isLeaf(const tree* t);
	static void pathStepBack(std::string& path);
	static void freeTree(tree* t);
	//returns the last file/directory name from a path
	static void getFileName(const std::string& path, std::string& result);
private:
	//gets the input string and transforms if to full file paths
	void formatAllPaths(const std::string& str, std::vector<std::string>& result);
	//fills two vectors with trimmed and full paths of the files
	bool readFilePaths(const std::string& path, std::vector<std::string>& result, std::vector<std::string>& files);
	void computeFrequencies(const std::string& path);
	void readFileFrequencies(const fs::path& path);
	void readStringFrequencies(const std::string& str);
	void writeCompressedStringToFile(const tree* t, const std::string& str, std::ofstream& destFile);
	tree* buildHuffmanTree();
	void extractCodes(const tree* t, std::vector<bool>& tempVec, size_t& treeDepth);

	//writes compressed file, its tree and metadata
	bool writeCompressedFile(const std::string& srcPath, std::ofstream& destFile);
	void writeTreeToVec(const tree* t);
	void writeTreeToFile(const tree* t, std::ofstream& destFile);
	void writeSymRaw(char sym);
	uint32_t writeEnd(std::ofstream& file);


	void writeSymbolToVector(unsigned char sym);

	uint32_t writeFileToVector(std::ifstream& srCile, std::ofstream& destFile);

	void moveSwap(std::string& a, std::string& b);
	int partition(std::vector<std::string>& vec, std::vector<std::string>& vec2, std::vector<std::string>& vec3, int left, int right);
	void quickSort(std::vector<std::string>& vec, std::vector<std::string>& vec2, std::vector<std::string>& vec3, int left, int right);

	//writes all paths into single string (for metadata)
	void vectorToFilesStr(const std::vector<std::string>& paths, std::string& pathsStr);
	void clearFrequencies();
	void clearCodes();
	void clearData();
};