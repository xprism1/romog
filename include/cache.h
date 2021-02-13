#include <set>

#ifndef CACHE_H
#define CACHE_H

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
cacheData getDataFromCache(std::string dat_path);
void updateCache(std::string dat_path);
cacheData addToCache(std::string dat_path, std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>> to_add_to_cache);
std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>> getMissing(std::string dat_path, cacheData cache_data);

#endif