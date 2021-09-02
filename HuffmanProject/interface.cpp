#include<iostream>
#include "Encoder.h"
#include "Decoder.h"
#include<string>


const char commandArch[] = "archive";
const char commandExctract[] = "extract";
const char commandExctreactAll[] = "all";
const char commandExctractOne[] = "one";
const char commandInfo[] = "info";
const char commandCheck[] = "check";
const char commandUpdate[] = "update";
const char commandExit[] = "exit";


int main() {
	Encoder enc;
	Decoder dec;

	std::cout << "Huffman compressor" << std::endl;

	std::string command;
	std::string extractCommand;
	std::string path;
	std::string destPath;
	std::string name;

	std::cout << "Enter command: " << std::endl;
	std::cin >> command;
	while (strcmp(command.c_str(), commandExit) != 0) {
		try {
			if (strcmp(command.c_str(), commandArch) == 0) {
				std::cout << "Specify path: ";
				std::cin.get();
				std::getline(std::cin, path); //reading the path
				std::cout << std::endl << "Choose compressed file destination: (include file name): ";
				std::cin >> destPath;

				if (enc.encode(path, destPath))
					std::cout << "The archive was successfully created!" << std::endl;
				else
					std::cout << "Something went wrong creating the archive!" << std::endl;

				std::cout << std::endl;
			}
			else if (strcmp(command.c_str(), commandExctract) == 0) {
				std::cout << "Specify one/all" << std::endl;
				std::cin >> extractCommand;
				if (strcmp(extractCommand.c_str(), commandExctreactAll) == 0) {
					std::cout << "Choose compressed file: ";
					std::cin.get();
					std::getline(std::cin, path);
					std::cout << std::endl << "Choose destination: ";
					std::cin >> destPath;
					if(dec.decode(path, destPath))
						std::cout << "Files extracted successfully!" << std::endl;
					else
						std::cout << "Something went wrong extracting the archive!" << std::endl;
					std::cout << std::endl;
				}
				else if (strcmp(extractCommand.c_str(), commandExctractOne) == 0) {
					std::cout << "Name of file: ";
					std::cin.get();
					std::getline(std::cin, name);
					std::cout << "Choose compressed file: ";
					std::getline(std::cin, path);
					std::cout << std::endl << "Choose destination: ";
					std::cin >> destPath;
					if(dec.decode(path, destPath, commandCode::extractOne, name))
						std::cout << "File extracted successfully!" << std::endl;
					else
						std::cout << "Something went wrong extracting the archive!" << std::endl;
					std::cout << std::endl;
				}
			}
			else if (strcmp(command.c_str(), commandInfo) == 0) {
				std::cout << "Specify huffman compressed file: ";
				std::cin.get();
				std::getline(std::cin, path);
				dec.decode(path, destPath, commandCode::info);
				std::cout << std::endl;
			}
			else if (strcmp(command.c_str(), commandCheck) == 0) {
				std::cout << "Specify huffman compressed file: ";
				std::cin.get();
				std::getline(std::cin, path);
				if (dec.checkIntegrity(path))
					std::cout << "The file is intact and safe for extraction!" << std::endl;
				else
					std::cout << "The file has been corrupted, some files may not be extracted correctly!" << std::endl;

				std::cout << std::endl;
			}
			else if (strcmp(command.c_str(), commandUpdate) == 0) {
				std::cout << "Path of updated file: ";
				std::cin.get();
				std::getline(std::cin, path);
				std::cout << "Specify compressed file location: ";
				std::getline(std::cin, destPath);
				if(dec.decode(destPath, destPath, commandCode::update, path, &enc))
					std::cout << "The file was successfully updated!" << std::endl;
				else
					std::cout << "The file was NOT updated the right way!" << std::endl;
				std::cout << std::endl;
			}
		}
		catch (std::filesystem::filesystem_error const& ex) {
			std::cout
				<< "what():  " << ex.what() << '\n'
				<< "path1(): " << ex.path1() << '\n'
				<< "path2(): " << ex.path2() << '\n'
				<< "code().value():    " << ex.code().value() << '\n'
				<< "code().message():  " << ex.code().message() << '\n'
				<< "code().category(): " << ex.code().category().name() << '\n';
		}
		catch (const std::exception& ex) {
			std::cout
				<< "what():  " << ex.what() << '\n';
		}
		std::cout << "Enter command: " << std::endl;
		std::cin >> command;
	}
	return 0;
}