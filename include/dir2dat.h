#ifndef DIR2DAT_H
#define DIR2DAT_H

std::vector<std::string> getAllFilesInDir(const std::string &dirPath);
std::tuple<std::string, std::string> getFileName(std::string path);
std::string getDirName(std::string path);
std::tuple<std::string, std::string, std::string, std::string> getDatName(std::string path);
std::string formatDate(std::string date);
std::string sortDat(std::string dat_path);
std::string fixSortedDat(std::string parent_path);
void dir2dat(std::string folder_path, std::string dat_path, bool toSort);

#endif