#include <iostream>
#include <fstream>

#include "writer.h"

int writeTextToFile(const std::string& filePath, const std::string& text) {
	std::ofstream outFile(filePath);
	if (!outFile) {
		std::cerr << "Error opening file for writing: " << filePath << std::endl;
		return -1; // Error code
	}
	outFile << text;
	outFile.close();
	return 0; // Success
}