#include <map>

#ifndef ARCHIVE_H
#define ARCHIVE_H

std::map<std::string, std::string> getInfoFromZip(std::string zip_path);
void extract(std::string filename, std::string destination);
void write_zip(std::string destination, std::vector<std::string> filenames, std::string rootfolder, std::string compression_level);

#endif