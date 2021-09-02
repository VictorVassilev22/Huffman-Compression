#pragma once

#include<bitset>
#include<iostream>
#include<fstream>
#include <vector>

const uint32_t BYTE_SIZE = 8; //byte size in bits
const uint32_t BOOL_VEC_CAPACITY = 1024;
const uint32_t WRITE_DATA_SIZE = sizeof(uint32_t) * BYTE_SIZE; // size of ulong file writable data

/// <summary>
/// allows to push back single bits and write huge chunks to file
/// </summary>
class bitVector {
	std::vector<std::bitset<WRITE_DATA_SIZE>> vec;
	uint32_t vec_size= 0;
public:
	//constructor reserves more vector capacity (currently 4KB) in order to avoid multiple memory allocations
	bitVector();
	//operation push_back pushes a bit to the end
	void push_back(const bool bit); 
	//operation size
	uint32_t size() const;
	
	// frees specified number of 32b chunks
	void free(const uint32_t chunksCount);
	//frees exact number of bits
	void free(const size_t bits, bool exact);
	//frees all the bits in the bitvector
	void free();
	 //operation write to file returns how many bytes are written to the file
	uint32_t writeToFile(std::ofstream& file);
	//operator[] read only
	bool operator[](long);
};