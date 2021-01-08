#include <set>

#ifndef CACHE_H
#define CACHE_H

/*
 * datData
 *
 * set_name_s: set containing set names of all entries in DAT
 * set_name: vector containing set names of all entries in DAT
 * rom_name: vector containing rom names of all entries in DAT
 * crc32_s: set containing crc32 of all entries in DAT
 * crc32: vector containing crc32 of all entries in DAT
 * md5: vector containing md5 of all entries in DAT
 * sha1: vector containing sha1 of all entries in DAT
 * sha1_s: set containing sha1 of all entries in DAT
 * size: vector containing size of all entries in DAT
 * crc_dupes: vector containing crc32s that are duplicated in DAT
 * sha1_dupes: vector containing sha1s that are duplicated in DAT
 * sha1_dupes_rom_names: element i of this vector is a vector containing all roms with sha1 of sha1_dupes[i]
 * sha1_dupes_set_names: element i of this vector is a vector containing all sets with sha1 of sha1_dupes[i]
 * 
 * e.g. sha1_dupes           =    {x,         y}
 *      sha1_dupes_rom_names = {{RN, RN}, {RN, RN}}
 *      sha1_dupes_set_names = {{SN, SN}, {SN, SN}}
 */
struct datData {
  std::set<std::string> set_name_s;
  std::vector<std::string> set_name;
  std::vector<std::string> rom_name;
  std::set<std::string> crc32_s;
  std::vector<std::string> crc32;
  std::vector<std::string> md5;
  std::vector<std::string> sha1;
  std::set<std::string> sha1_s;
  std::vector<std::string> size;
  std::vector<std::string> crc_dupes;
  std::vector<std::string> sha1_dupes;
  std::vector<std::vector<std::string>> sha1_dupes_rom_names;
  std::vector<std::vector<std::string>> sha1_dupes_set_names;
};

/*
 * cacheData
 *
 * info: vector containing {"romorganizer", "cache", "version", "1.0", dat_name, dat_path}
 * set_name: vector containing set names of all entries in cache
 * rom_name: vector containing rom names of all entries in cache
 * crc32: vector containing crc32 of all entries in cache
 * md5: vector containing md5 of all entries in cache
 * sha1: vector containing sha1 of all entries in cache
 * status: vector containing status of all entries in cache
 */
struct cacheData {
  std::vector<std::string> info;
  std::vector<std::string> set_name;
  std::vector<std::string> rom_name;
  std::vector<std::string> crc32;
  std::vector<std::string> md5;
  std::vector<std::string> sha1;
  std::vector<std::string> status;
};

std::tuple<std::string, std::string> getCachePath(std::string dat_path);
void createNewCache(std::string dat_path, std::string folder_path);
bool hasUpdate(std::string dat_path);
datData getDataFromDAT(std::string dat_path);
cacheData getDataFromCache(std::string dat_path);
void updateCache(std::string dat_path);
cacheData addToCache(std::string dat_path, std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>> to_add_to_cache);

#endif