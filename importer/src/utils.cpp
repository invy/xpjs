

#include <string>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>

#include "utils.h"

IOError loadFileContent(const std::string &fileName, std::string &out) {
	std::cout << "load from file: " << fileName << "\n";
    std::ifstream scriptFile(fileName, std::ifstream::in);
	if(!scriptFile.good())
		return IOError::IO_ERROR_NOT_FOUND;
	out.assign((std::istreambuf_iterator<char>(scriptFile)), (std::istreambuf_iterator<char>()));
    return IOError::SUCCESS;
}

std::string pathCombine(const std::string &part1, const std::string &part2) {
	return part1 + "/" + part2;
}

std::vector<std::string> enumerateFilesInDirectory(const std::string &pattern) {
    glob_t glob_result;
    glob(pattern.c_str(),GLOB_TILDE,NULL,&glob_result);
    std::vector<std::string> files;
    for(unsigned int i=0;i<glob_result.gl_pathc;++i){
        files.push_back(std::string(glob_result.gl_pathv[i]));
    }
    globfree(&glob_result);
    return files;
}

int is_directory(const std::string& path)
{
    struct stat path_stat;
    stat(path.c_str(), &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

int is_regular(const std::string& path)
{
    struct stat path_stat;
    stat(path.c_str(), &path_stat);
    return S_ISREG(path_stat.st_mode);
}
