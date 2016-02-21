
#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>

enum IOError {
	SUCCESS,
	IO_ERROR_IS_DIRECTORY,
	IO_ERROR_NOT_DIRECTORY,
	IO_ERROR_NOT_FOUND
};

IOError loadFileContent(const std::string &fileName, std::string &out);
std::string pathCombine(const std::string &part1, const std::string &part2);
int is_directory(const std::string& path);
int is_regular(const std::string& path);
std::vector<std::string> enumerateFilesInDirectory(const std::string &pattern);

#endif // UTILS_H
