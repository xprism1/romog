#include <vector>

#ifndef GETHASHES_H
#define GETHASHES_H

std::vector<std::string> getHashes(std::string path, int start_offset = -1, std::vector<std::tuple<int, std::string>> data = {});

#endif