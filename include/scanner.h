#include <set>

#ifndef SCANNER_H
#define SCANNER_H

std::set<std::string> getAllFilesInDir2(const std::string &dirPath);
void removeEmptyDirs(const std::string &dirPath);
std::vector<std::string> diff(std::set<std::string> s1, std::set<std::string> s2, int req);
std::tuple<int, int, int, int> countSetsRoms(cacheData cache_data);
void scan(std::string dat_path, std::string folder_path);

#endif