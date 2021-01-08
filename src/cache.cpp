#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>

#include <pugixml.hpp>

#include <paths.h>
#include <dir2dat.h>
#include <cache.h>

/*
 * Gets path to cache from DAT path
 *
 * Arguments:
 *     dat_path : Path to DAT file
 * 
 * Returns:
 *     info : Tuple containing path to cache and DAT filename
 */
std::tuple<std::string, std::string> getCachePath(std::string dat_path){
  std::string fname = std::get<0>(getDatName(dat_path));
  std::string fname_noed = std::get<2>(getDatName(dat_path));
  std::string set_cache_path = cache_path + fname_noed + ".cache";

  std::tuple<std::string, std::string> info = std::make_tuple(set_cache_path, fname);
  return info;
}

/*
 * Creates a new cache file
 *
 * Arguments:
 *     dat_path : Path to DAT file
 *     folder_path : Path to the folder containing ROMs
 * 
 * Notes:
 *     Name of cache is the name of the DAT file without date
 */
void createNewCache(std::string dat_path, std::string folder_path){
  std::string cache_path = std::get<0>(getCachePath(dat_path)); // getting path to cache from DAT path
  std::string datfilename = std::get<1>(getCachePath(dat_path));

  // writing blank cache
  std::ofstream file(cache_path);
  file << "romorganizer cache version 1.0" << std::endl;
  file << "\"" << datfilename << "\" \"" << folder_path << "\" \"" << "0" << "\" \"" << "0" << "\" \"" << "0" << "\" \"" << "0" << "\"" << std::endl;
  file << std::endl;
  file.close();
}

/*
 * Checks if a new DAT file is present, by comparing the name of the DAT file with the name in the cache.
 *
 * Arguments:
 *     dat_path : Path to DAT file
 * 
 * Returns:
 *     true if a new DAT file is present, and false if not
 */
bool hasUpdate(std::string dat_path){
  std::string cache_path = std::get<0>(getCachePath(dat_path)); // getting path to cache from DAT path
  std::string datfilename = std::get<1>(getCachePath(dat_path)); // filename of dat_path

  // reading from cache
  std::ifstream file(cache_path);
  std::string line;
  for(int i = 0; i < 2 && getline(file,line); i++) { // reads first 2 lines of cache
    continue;
  }
  file.close();
  
  // line is now the 2nd line of cache
  std::stringstream ss(line);
  std::string s;
  std::vector<std::string> cache_info;
  while (ss >> std::quoted(s)) {
    cache_info.push_back(s);
  }

  std::string cache_datfilename = std::get<0>(getFileName(cache_info[0])); // DAT filename in cache

  if(datfilename != cache_datfilename){
    return true;
  } else {
    return false;
  }
}

/*
 * Removes lines from cache file
 *
 * Arguments:
 *     cache_path : Path to cache file
 *     lines_to_keep : Vector containing line numbers to keep
 *     cache_info : Vector containing path to DAT file, path to folder containing roms, set have, set total, rom have, rom total (for second line of cache file)
 */
void onlyWriteCertainLines(const char *cache_path, std::vector<int> lines_to_keep, std::vector<std::string> cache_info){ 
  std::ifstream is(cache_path);
  std::ofstream ofs("temp.txt"); // write to temp file

  int line_no = 0;
  std::string line;
  while(getline(is,line)){
    line_no++;
    if (line_no == 2){ // update dat name in cache
      ofs << "\"" << cache_info[0] << "\" \"" << cache_info[1] << "\" \"" << cache_info[2] << "\" \"" << cache_info[3] << "\" \"" << cache_info[4] << "\" \"" << cache_info[5] << "\"" << std::endl;
    }
    for(int i = 0; i < lines_to_keep.size(); i++) {
      if (line_no == lines_to_keep[i]) {
        ofs << line << std::endl; // writes line from cache to output file if line number is in lines_to_keep
      }
    }
  }

  ofs.close(); 
  is.close(); 
  remove(cache_path);  // remove original file
  rename("temp.txt", cache_path); // rename temp file to original file's name
}

/*
 * Removes lines from cache file
 *
 * Arguments:
 *     cache_path : Path to cache file
 *     lines_to_remove : Vector containing line numbers to remove
 */
void removeLines(const char *cache_path, std::vector<int> lines_to_remove){ 
  std::ifstream is(cache_path);
  std::ofstream ofs("temp.txt"); // write to temp file

  int line_no = 0;
  std::string line;
  while(getline(is,line)){
    line_no++;
    if (!(std::find(lines_to_remove.begin(), lines_to_remove.end(), line_no) != lines_to_remove.end())){ // if line number is not in lines_to_remove
      ofs << line << std::endl; // writes line from cache to output file
    }
  }

  ofs.close(); 
  is.close(); 
  remove(cache_path);  // remove original file
  rename("temp.txt", cache_path); // rename temp file to original file's name
}

/*
 * Gets data from DAT file
 *
 * Arguments:
 *     dat_path : Path to DAT file
 * 
 * Returns:
 *     dat_data : Struct containing DAT data (see definition in cache.h)
 * 
 * Notes:
 *     pugixml will automatically handle HTML entities so no decoding is required
 */
datData getDataFromDAT(std::string dat_path){
  datData dat_data;
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(dat_path.c_str());
  pugi::xml_node root = doc.child("datafile");
  std::string set_name;
  std::string rom_name;

  for (pugi::xml_node game = root.child("game"); game != nullptr; game = game.next_sibling()) {
    for(pugi::xml_node rom = game.child("rom"); rom != nullptr; rom = rom.next_sibling()){
      set_name = game.attribute("name").value();
      rom_name = rom.attribute("name").value();
      std::replace(rom_name.begin(), rom_name.end(), '\\', '/'); // handle rom name with path; replace all occurences of "\" to "/"

      dat_data.set_name_s.insert(set_name);
      dat_data.set_name.push_back(set_name);
      dat_data.rom_name.push_back(rom_name);
      dat_data.crc32_s.insert(rom.attribute("crc").value());
      dat_data.crc32.push_back(rom.attribute("crc").value());
      dat_data.md5.push_back(rom.attribute("md5").value());
      dat_data.sha1_s.insert(rom.attribute("sha1").value());
      dat_data.sha1.push_back(rom.attribute("sha1").value());
      dat_data.size.push_back(rom.attribute("size").value());
    }
  }

  std::vector<std::string> crc32_sorted(dat_data.crc32.size()); // create vector crc32_sorted
  partial_sort_copy(std::begin(dat_data.crc32), std::end(dat_data.crc32), std::begin(crc32_sorted), std::end(crc32_sorted)); // sort dat_data.crc32 and put it in crc32_sorted
  std::set_difference(crc32_sorted.begin(), crc32_sorted.end(), dat_data.crc32_s.begin(), dat_data.crc32_s.end(), std::back_inserter(dat_data.crc_dupes)); // elements in crc32_sorted but not in dat_data.crc32_s are put into dat_data.crc_dupes

  std::vector<std::string> sha1_sorted(dat_data.sha1.size()); // create vector sha1_sorted
  partial_sort_copy(std::begin(dat_data.sha1), std::end(dat_data.sha1), std::begin(sha1_sorted), std::end(sha1_sorted)); // sort dat_data.sha1 and put it in sha1_sorted
  std::set_difference(sha1_sorted.begin(), sha1_sorted.end(), dat_data.sha1_s.begin(), dat_data.sha1_s.end(), std::back_inserter(dat_data.sha1_dupes)); // elements in sha1_sorted but not in dat_data.sha1_s are put into dat_data.sha1_dupes
  for(int i = 0; i < dat_data.sha1_dupes.size(); i++){
    std::vector<std::string> x;
    dat_data.sha1_dupes_rom_names.push_back(x);
    dat_data.sha1_dupes_set_names.push_back(x);
    for(int j = 0; j < dat_data.rom_name.size(); j++){
      if(dat_data.sha1[j] == dat_data.sha1_dupes[i]){
        dat_data.sha1_dupes_rom_names[i].push_back(dat_data.rom_name[j]);
        dat_data.sha1_dupes_set_names[i].push_back(dat_data.set_name[j]);
      }
    }
  }

  return dat_data;
}

/*
 * Gets data from cache
 *
 * Arguments:
 *     dat_path : Path to DAT file
 * 
 * Returns:
 *     cache_data : Struct containing cache data (see definition in cache.h)
 */
cacheData getDataFromCache(std::string dat_path){
  std::string cache_path = std::get<0>(getCachePath(dat_path)); // getting path to cache from DAT path
  cacheData cache_data;

  // reading from cache
  std::ifstream file(cache_path);
  std::string line;
  std::vector<std::string> cache;
  int no_of_lines = 0;

  while(getline(file,line)) {
    no_of_lines++;

    std::stringstream ss(line);
    std::string s;
    while (ss >> std::quoted(s)) {
      if(no_of_lines > 3){
        cache.push_back(s);
      } else {
        cache_data.info.push_back(s);
      }
    }
  }
  file.close();

  for(int i = 0; i < cache.size(); i++){ // spliting cache into set_name, rom_name, crc32, md5, sha1, status
    switch(i % 6){
      case 0:
        cache_data.set_name.push_back(cache[i]);
        break;
      case 1:
        cache_data.rom_name.push_back(cache[i]);
        break;
      case 2:
        cache_data.crc32.push_back(cache[i]);
        break;
      case 3:
        cache_data.md5.push_back(cache[i]);
        break;
      case 4:
        cache_data.sha1.push_back(cache[i]);
        break;
      case 5:
        cache_data.status.push_back(cache[i]);
    }
  }

  return cache_data;
}

/*
 * Removes entries from cache if it dosen't match the DAT file
 *
 * Arguments:
 *     dat_path : Path to DAT file
 */
void updateCache(std::string dat_path){
  std::string cache_path = std::get<0>(getCachePath(dat_path)); // getting path to cache from DAT path
  std::string datfilename = std::get<1>(getCachePath(dat_path));

  cacheData cache_data = getDataFromCache(dat_path); // reading from cache
  cache_data.info.erase(cache_data.info.begin(), cache_data.info.begin() + 4); // removes first 4 elements from cache_data.info (which are the strings from the first line of the cache)
  cache_data.info[0] = datfilename; // update dat name

  datData dat_data = getDataFromDAT(dat_path); // reading from DAT

  // comparisons
  std::vector<int> lines_to_keep;
  lines_to_keep.push_back(1); // keep first line
  lines_to_keep.push_back(3); // keep third line

  for(int i = 0; i < cache_data.set_name.size(); i++){
    for(int j = 0; j < dat_data.set_name.size(); j++){
      if(cache_data.md5[i] == "-" && cache_data.sha1[i] == "-"){ // checks if md5 and sha1 are marked as ignored
        if(cache_data.set_name[i] == dat_data.set_name[j] && cache_data.rom_name[i] == dat_data.rom_name[j] && cache_data.crc32[i] == dat_data.crc32[j]){ // if so, we only check set name, rom name, crc32
          lines_to_keep.push_back(i+4);
          break;
        }
      } else if (cache_data.set_name[i] == dat_data.set_name[j] && cache_data.rom_name[i] == dat_data.rom_name[j] && cache_data.crc32[i] == dat_data.crc32[j] && cache_data.md5[i] == dat_data.md5[j] && cache_data.sha1[i] == dat_data.sha1[j]){ // checks if set name, rom name, crc32, md5, sha1 match dat
        lines_to_keep.push_back(i+4);
        break;
      }
    }
  }

  // updates cache file
  onlyWriteCertainLines(cache_path.c_str(),lines_to_keep,cache_data.info);
}

/*
 * Adds entries to cache. If there is an existing entry with the same set name and rom name, it will be removed.
 *
 * Arguments:
 *     dat_path : Path to DAT file
 *     to_add_to_cache : Vector containing tuples of strings. Each tuple consists of the set name, followed by rom name, CRC32, MD5, SHA1, status.
 * 
 * Returns:
 *     cache_data : Struct containing cache data
 */
cacheData addToCache(std::string dat_path, std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>> to_add_to_cache){
  std::string cache_path = std::get<0>(getCachePath(dat_path)); // getting path to cache from DAT path

  cacheData cache_data = getDataFromCache(dat_path); // reading from cache

  std::vector<int> lines_to_remove;
  for(int i = 0; i < to_add_to_cache.size(); i++){
    for(int j = 0; j < cache_data.set_name.size(); j++){
      if(std::get<0>(to_add_to_cache[i]) == cache_data.set_name[j] && std::get<1>(to_add_to_cache[i]) == cache_data.rom_name[j]){
        lines_to_remove.push_back(j+4);
      }
    }
  }

  if(lines_to_remove.size() > 0) {
    removeLines(cache_path.c_str(),lines_to_remove); // removes existing entries with same set name and rom name
    for(auto i: lines_to_remove){ // update cache_data by removing the element with index i-4
      cache_data.set_name.erase(cache_data.set_name.begin() + i-4);
      cache_data.rom_name.erase(cache_data.rom_name.begin() + i-4);
      cache_data.crc32.erase(cache_data.crc32.begin() + i-4);
      cache_data.md5.erase(cache_data.md5.begin() + i-4);
      cache_data.sha1.erase(cache_data.sha1.begin() + i-4);
      cache_data.status.erase(cache_data.status.begin() + i-4);
    }
  }
  
  std::ofstream file(cache_path, std::ios_base::app); // open cache in append mode
  for(auto i: to_add_to_cache){
    file << "\"" << std::get<0>(i) << "\" \"" << std::get<1>(i) << "\" \"" << std::get<2>(i) << "\" \"" << std::get<3>(i) << "\" \"" << std::get<4>(i) << "\" \"" << std::get<5>(i) << "\"" << std::endl; // writes entries to cache

    // updates cache_data
    cache_data.set_name.push_back(std::get<0>(i));
    cache_data.rom_name.push_back(std::get<1>(i));
    cache_data.crc32.push_back(std::get<2>(i));
    cache_data.md5.push_back(std::get<3>(i));
    cache_data.sha1.push_back(std::get<4>(i));
    cache_data.status.push_back(std::get<5>(i));
  }
  file.close();
  
  return cache_data; 
}