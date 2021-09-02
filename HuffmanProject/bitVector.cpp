#include "bitVector.h"

bitVector::bitVector()
{
	vec.reserve(BOOL_VEC_CAPACITY);
}

void bitVector::push_back(const bool bit)
{
	uint32_t needed = ((vec_size + 1) / WRITE_DATA_SIZE) + 1;

	if (vec.size() < needed)
		vec.push_back(std::bitset<WRITE_DATA_SIZE>(0));

	vec[vec_size / WRITE_DATA_SIZE][vec_size % WRITE_DATA_SIZE] = bit;
	vec_size++;
}

uint32_t bitVector::size() const
{
	return vec_size;
}

void bitVector::free(const uint32_t elements)
{
	if (elements > vec.size())
		throw std::out_of_range("Cannot free more than size of bitVector");

	vec.erase(vec.begin(), vec.begin() + elements);
	vec_size -= elements * WRITE_DATA_SIZE;
}

void bitVector::free(const size_t bitsCnt, bool exact)
{

	if (bitsCnt > vec_size)
		throw std::out_of_range("Cannot free more bits than size of bitVector");

	free(bitsCnt / WRITE_DATA_SIZE);
	uint32_t r = bitsCnt % WRITE_DATA_SIZE;
	uint32_t chunksCnt = vec.size();
	vec[0] >>= r;
	for (size_t i = 0; i < chunksCnt - 1; i++)
	{
		for (size_t j = WRITE_DATA_SIZE - r; j < WRITE_DATA_SIZE; j++)
		{
			vec[i][j] = vec[i + 1][j - WRITE_DATA_SIZE + r];
		}
		vec[i + 1] >>= r;
	}
	vec_size -= r;
}

void bitVector::free()
{
	if (vec_size % WRITE_DATA_SIZE == 0) {
		free(vec_size/WRITE_DATA_SIZE);
	}
	else {
		free(vec_size / WRITE_DATA_SIZE + 1);
	}
	vec_size = 0;
}

uint32_t bitVector::writeToFile(std::ofstream& file)
{
	uint32_t chunksCnt = vec_size / WRITE_DATA_SIZE;

	file.write(reinterpret_cast<const char*>(&vec[0]),
		(std::streamsize)chunksCnt * sizeof(std::bitset<WRITE_DATA_SIZE>));

	free(chunksCnt);
	return chunksCnt * (WRITE_DATA_SIZE / BYTE_SIZE);
}

bool bitVector::operator[](long index)
{
	if (index < 0 || index >= vec_size) {
		  throw std::out_of_range("Index of bitVector out of bounds");
	}

	return vec[index / WRITE_DATA_SIZE][index % WRITE_DATA_SIZE];
}
