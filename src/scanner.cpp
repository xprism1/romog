#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <filesystem>
#include <algorithm>
#include <set>
#include <map>

#include <pugixml.hpp>

#include "../libs/termcolor/termcolor.hpp"

#include <paths.h>
#include <gethashes.h>
#include <dir2dat.h>
#include <cache.h>
#include <dat.h>
#include "../include/archive.h"
#include <scanner.h>

namespace filesys = std::filesystem;

/*
 * Gets a list of all files in given directory and its subdirectories.
 *
 * Arguments:
 *     dirPath : Path of directory
 *
 * Returns:
 *     listofFiles : Vector containing filenames of all the files in given directory (without extension) and its sub directories
 *
 * Notes:
 *     Modified the one in dir2dat.cpp to return a set with only filenames
 */
std::set<std::string> getAllFilesInDir2(const std::string &dirPath) {
  // Create a vector of strings
  std::set<std::string> listOfFiles;
  try {
    // Check if given path exists and points to a directory
    if (filesys::exists(dirPath) && filesys::is_directory(dirPath)) {
      // Create a Recursive Directory Iterator object and points to the starting of directory
      filesys::recursive_directory_iterator iter(dirPath);
      // Create a Recursive Directory Iterator object pointing to end
      filesys::recursive_directory_iterator end;
      // Iterate till end
      while (iter != end) {
        // Add the name in vector
        std::string path = iter->path().string();
        listOfFiles.insert(std::get<1>(getFileName(path))); // filename without extension

        std::error_code ec;
        // Increment the iterator to point to next entry in recursive iteration
        iter.increment(ec);
        if (ec) {
          std::cerr << "Error While Accessing : " << iter->path().string() << " :: " << ec.message() << '\n';
        }
      }
    }
  } catch (std::system_error & e) {
    std::cerr << "Exception :: " << e.what();
  }
  return listOfFiles;
}

/*
 * Removes empty directories and sub-directories.
 *
 * Arguments:
 *     dirPath : Path of directory
 *
 * Notes:
 *     Modified the one in dir2dat.cpp to remove empty directories
 */
void removeEmptyDirs(const std::string &dirPath) {
  // Create a vector of strings
  std::vector<std::string> listOfFiles;
  try {
    // Check if given path exists and points to a directory
    if (filesys::exists(dirPath) && filesys::is_directory(dirPath)) {
      // Create a Recursive Directory Iterator object and points to the starting of directory
      filesys::recursive_directory_iterator iter(dirPath);
      // Create a Recursive Directory Iterator object pointing to end
      filesys::recursive_directory_iterator end;
      // Iterate till end
      while (iter != end) {
        // Add the name in vector
        listOfFiles.push_back(iter->path().string());

        std::error_code ec;
        // Increment the iterator to point to next entry in recursive iteration
        iter.increment(ec);
        if (ec) {
          std::cerr << "Error While Accessing : " << iter->path().string() << " :: " << ec.message() << '\n';
        }
      }
    }
  } catch (std::system_error & e) {
    std::cerr << "Exception :: " << e.what();
  }

  for (auto it = listOfFiles.rbegin(); it != listOfFiles.rend(); ++it){ // iterates vector backwards. e.g. listOfFiles = ["/a/file.txt","/a/b", "/a/b/c"], /a/b and /a/b/c are empty directories. we need to remove /a/b/c first before removing /a/b, so vector is iterated backwards
    filesys::path p = *it; // *it is the current element in the vector
    if(filesys::is_directory(p) && filesys::is_empty(p)){ // checks if its a directory; don't remove empty files as that might be a placeholder ROM (of size 0)
      filesys::remove(p);
    }
  }
}

/*
 * Gets elements unique to each set
 *
 * Arguments:
 *     s1 : First set
 *     s2 : Second set
 *     req : 1 if you want elements only in s1, 2 if you want elements only in s2
 *
 * Returns:
 *     onlyinX : Vector containing elements only in s1/s2
 *
 * Notes:
 *     Taken from https://en.cppreference.com/w/cpp/algorithm/set_symmetric_difference, http://www.cplusplus.com/reference/algorithm/set_intersection/, https://stackoverflow.com/questions/13448064/how-to-find-the-intersection-of-two-stdset-in-c/13448094#13448094
 */
std::vector<std::string> diff(std::set<std::string> s1, std::set<std::string> s2, int req) { 
  std::set<std::string> sSymDiff;

  std::set_symmetric_difference(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(sSymDiff,sSymDiff.begin())); // sSymDiff is a set containing elements unique to s1 and s2

  std::vector<std::string> onlyinX; // only in 1 or 2 depending on input

  if(req == 1){ // to get elements unique to s1, we take intersection of s1 and sSymDiff
    std::set_intersection(s1.begin(), s1.end(), sSymDiff.begin(), sSymDiff.end(), std::inserter(onlyinX,onlyinX.begin()));
  } else if(req == 2){ // to get elements unique to s2, we take intersection of s2 and sSymDiff
    std::set_intersection(s2.begin(), s2.end(), sSymDiff.begin(), sSymDiff.end(), std::inserter(onlyinX,onlyinX.begin()));
  }

  return onlyinX;
}

/*
 * Counts number of sets have/total and roms have/total from cache
 *
 * Arguments:
 *     cache_data : Struct containing cache data (see definition in cache.h)
 *
 * Returns:
 *     count : Tuple containing set have, set total, rom have, rom total (in that order)
 */
std::tuple<int, int, int, int> countSetsRoms(cacheData cache_data){
  std::vector<std::tuple<std::string,int,int>> set_count; // each tuple consists of the set name, roms have for that set, roms total for that set
  bool repeated_entry = false; // true if there's an element in set_count with the same set name; false if otherwise
  int index;
  int roms_have = 0;
  int roms_total = 0;

  for(int i = 0; i < cache_data.status.size(); i++){
    for(int j = 0; j < set_count.size(); j++){
      if(std::get<0>(set_count[j]) == cache_data.set_name[i]){
        repeated_entry = true;
        index = j; // index of that set name in set_count
        break;
      } else {
        repeated_entry = false;
      }
    }

    if(cache_data.status[i] == "Passed"){
      roms_have += 1;
      roms_total += 1;
      if(repeated_entry){ // add 1 to roms have and roms total (for that set) in set_count
        std::get<1>(set_count[index]) += 1;
        std::get<2>(set_count[index]) += 1;
      } else {
        set_count.push_back(std::make_tuple(cache_data.set_name[i],1,1)); // make a new entry in set_count with that set name
      }
    } else {
      roms_total += 1;
      if(repeated_entry){ // add 1 to roms total (for that set) in set_count, don't touch roms have (for that set) in set_count since its missing
        std::get<2>(set_count[index]) += 1;
      } else {
        set_count.push_back(std::make_tuple(cache_data.set_name[i],0,1)); // make a new entry in set_count with that set name
      }
    }
  }

  int sets_have = 0;
  int sets_total = 0;
  for(auto i: set_count){
    if(std::get<1>(i) == std::get<2>(i)){ // if roms have == roms total (for that set) (i.e. we have all the roms in that set), we add 1 to sets_have and sets_total
      sets_have += 1;
      sets_total += 1;
    } else { // if roms have != roms total (for that set) (i.e. the set is incomplete), we add 1 to sets_total and don't touch sets_have
      sets_total += 1;
    }
  }

  std::tuple<int, int, int, int> count = std::make_tuple(sets_have,sets_total,roms_have,roms_total);
  return count;
}

/*
 * Updates sets have/total and roms have/total in cache
 *
 * Arguments:
 *     dat_path : Path to DAT file
 *     cache_path : Path to cache file
 *     folder_path : Path to folder to be scanned against the DAT file
 *     count : Tuple containing set have, set total, rom have, rom total (in that order)
 */
void updateCacheCount(std::string dat_path, std::string cache_path, std::string folder_path, std::tuple<int, int, int, int> count){
  int sets_have = std::get<0>(count);
  int sets_total = std::get<1>(count);
  int roms_have = std::get<2>(count);
  int roms_total = std::get<3>(count);
  
  std::ifstream is(cache_path);
  std::ofstream ofs("temp.txt");
  int line_no = 0;
  std::string line;
  while(getline(is,line)){
    line_no++;
    if (line_no == 2){
      ofs << "\"" << dat_path << "\" \"" << folder_path << "\" \"" << sets_have << "\" \"" << sets_total << "\" \"" << roms_have << "\" \"" << roms_total << "\"" << std::endl;
    } else {
      ofs << line << std::endl;
    }
  }
  is.close();
  ofs.close();
  filesys::remove(cache_path);
  filesys::rename("temp.txt",cache_path);
}

/*
 * Prints sets have/total and roms have/total (in color)
 * 
 * Arguments:
 *     count : Tuple containing set have, set total, rom have, rom total (in that order)
 */
void printCount(std::tuple<int, int, int, int> count){
  int sets_have = std::get<0>(count);
  int sets_total = std::get<1>(count);
  int roms_have = std::get<2>(count);
  int roms_total = std::get<3>(count);

  if(sets_have == 0){
    std::cout << termcolor::cyan << "Sets have:    " << termcolor::red << sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Sets missing: " << termcolor::red << sets_total-sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Roms have:    " << termcolor::red << roms_have << "/" << roms_total << std::endl;
    std::cout << termcolor::cyan << "Roms missing: " << termcolor::red << roms_total-roms_have << "/" << roms_total << std::endl;
  } else if (sets_have < sets_total){
    std::cout << termcolor::cyan << "Sets have:    " << termcolor::color<255,165,0> << sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Sets missing: " << termcolor::color<255,165,0> << sets_total-sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Roms have:    " <<termcolor::color<255,165,0> << roms_have << "/" << roms_total << std::endl;
    std::cout << termcolor::cyan << "Roms missing: " << termcolor::color<255,165,0> << roms_total-roms_have << "/" << roms_total << std::endl;
  } else if (sets_have == sets_total){
    std::cout << termcolor::cyan << "Sets have:    " << termcolor::green << sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Sets missing: " << termcolor::green << sets_total-sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Roms have:    " << termcolor::green << roms_have << "/" << roms_total << std::endl;
    std::cout << termcolor::cyan << "Roms missing: " << termcolor::green << roms_total-roms_have << "/" << roms_total << std::endl;
  }
  std::cout << termcolor::reset << std::endl;
}

/*
 * Scans a romset, makes all set name, rom name and CRC32 of files in folder match DAT. Outputs sets have/missing, roms have/missing to terminal. Also keeps track of what is present (and what isin't) via a cache.
 * If a header skipper XML is present, header skipping support is enabled. (If <data> matches, hash will be calculated from start offset to end of file; if not, hash is calculated over the entire file). scan() looks for the header XML as such: e.g. if dat_path = dats_path + "/No-Intro/Atari - 7800 (date).dat", header_path = headers_path + "/No-Intro/Atari - 7800.xml"
 *
 * Arguments:
 *     dat_path : Path to DAT file
 *     folder_path : Path to folder to be scanned against the DAT file, i.e. path to the romset (*Path must end with a forward slash)
 */
void scan(std::string dat_path, std::string folder_path){
  // checks
  if(!(filesys::exists(dat_path))){
    std::cout << dat_path << " does not exist!" << std::endl;
    exit(0);
  }

  if(!(filesys::is_directory(folder_path))){
    std::cout << folder_path << " is not a directory!" << std::endl;
    exit(0);
  }

  std::cout << "Scanning " << dat_path << std::endl;

  // updating/creating cache
  std::tuple<std::string, std::string> cache_info = getCachePath(dat_path);
  std::string cache_path = std::get<0>(cache_info);
  if(filesys::exists(cache_path) && hasUpdate(dat_path)){
    std::cout << "Do you want to update the DAT file?" << std::endl;
    bool toUpdate;
    std::string input;
    std::cin >> input;
    if(input == "y"){
      updateCache(dat_path);
    }
  } else if (!(filesys::exists(cache_path))){
    createNewCache(dat_path,folder_path);
  }
  
  // getting info on files in folder, DAT, cache
  std::set<std::string> files_in_folder = getAllFilesInDir2(folder_path);
  datData dat_data = getDataFromDAT(dat_path);
  cacheData cache_data = getDataFromCache(dat_path);

  // headers
  std::string dat_path_copy = dat_path; // copy dat_path to dat_path_copy so the original dosen't get changed (we need it for later)
  std::string header_path = headers_path + dat_path_copy.erase(dat_path_copy.find(dats_path),dats_path.size()); // header_path = "/home/xp/Desktop/xp (My Tools)/romorganizer/debug/headers/" + dat_path without "/home/xp/Desktop/xp (My Tools)/romorganizer/debug/dats/"
  std::string parent_path = filesys::path(header_path).parent_path();
  header_path = parent_path + "/" + std::get<2>(getDatName(header_path)) + ".xml";

  bool scanningWithHeaders = false; // whether header support will be enabled for this scan
  pugi::xml_document doc;
  int start_offset;
  std::vector<std::tuple<int, std::string>> info;
  if(filesys::exists(header_path)){
    scanningWithHeaders = true;

    // getting header skipping info from XML in header_path
    pugi::xml_parse_result result = doc.load_file(header_path.c_str());
    pugi::xml_node root = doc.child("detector");

    pugi::xml_node rule = root.child("rule");
    start_offset = std::stol(rule.attribute("start_offset").value(),nullptr,16); // convert start_offset (in hex) to dec
    for(pugi::xml_node data = rule.child("data"); data != nullptr; data = data.next_sibling()){
      int offset = std::stol(data.attribute("offset").value(),nullptr,16); // convert offset (in hex) to dec
      std::string value = data.attribute("value").value();
      std::transform(value.begin(), value.end(), value.begin(), ::tolower);
      info.push_back(std::make_tuple(offset,value));
    }

    std::cout << "Using header skipper " << header_path << std::endl;
  }

  // comparing files in folder with files in DAT; making sure all CRCs of files in folder match DAT; moves non-matching files to backup folder
  std::set<std::string> to_zip; // vector containing names of folders in tmp/ to zip; set so duplicates won't get inserted

  for(auto i: files_in_folder){
    std::map<std::string, std::string> zipinfo;
    bool is_extracted = false;

    if(scanningWithHeaders){
      std::string tmp_dir = tmp_path + i + "/";
      extract(folder_path+i+".zip",tmp_dir);
      is_extracted = true;

      std::vector<std::string> files = getAllFilesInDir(tmp_dir);
      for(auto j: files){
        std::vector<std::string> fileinfo = getHashes(j, start_offset, info);
        std::string filename = filesys::path(j).filename();
        zipinfo[filename] = fileinfo[1];
      }
    } else {
      zipinfo = getInfoFromZip(folder_path+i+".zip");
    }

    for(auto j: zipinfo){
      std::string file_rom_name = j.first;
      std::string crc32 = j.second;
      bool inCache = false;
      for(int i = 0; i < cache_data.rom_name.size(); i++){
        if(cache_data.rom_name[i] == file_rom_name && cache_data.status[i] == "Passed"){ // if it's already in cache and "Passed", no need to check hash
          inCache = true;
          break;
        }
      }
      if(!(inCache)){
        if (!(hashInDAT(dat_path, crc32, "1"))){ // CRC32 does not exist in DAT, so move file to backup folder
          std::string tmp_dir = tmp_path + i + "/";
          if(!(filesys::exists(tmp_dir))){
            filesys::create_directory(tmp_dir);
          }
          if(!(is_extracted)){
            extract(folder_path+i+".zip",tmp_dir);
            is_extracted = true;
          }
          if(!(filesys::exists(backup_path+i))){
            filesys::create_directory(backup_path+i);
          }
          if(file_rom_name.find('/') != std::string::npos){ // if file_rom_name has slash, make that directory structure in backup dir (so that we can move the file to the correct directory)
            filesys::path p = file_rom_name;
            std::string parentpath = p.parent_path(); // .parent_path(): gets parent path (eg: if file_rom_name = "files/a.rom", parentpath = "files")
            
            if(!(filesys::exists(backup_path+i+"/"+parentpath))){
              filesys::create_directories(backup_path+i+"/"+parentpath);
            }
          }          
          
          filesys::rename(tmp_dir+file_rom_name, backup_path+i+"/"+file_rom_name);
          std::cout << "Moved " << file_rom_name << " in " << folder_path+i << ".zip" << " to backup folder" << std::endl;

          if(!(filesys::is_empty(tmp_dir))){ // if tmp_dir is empty directory, then we don't need to zip it (since the zip only has 1 rom with non-matching CRC)
            to_zip.insert(i);
          }
        } else if (std::find(dat_data.crc_dupes.begin(), dat_data.crc_dupes.end(), crc32) != dat_data.crc_dupes.end()){ // CRC is duplicated in DAT, so check SHA1
          std::string tmp_dir = tmp_path + i + "/";
          if(!(filesys::exists(tmp_dir))){
            filesys::create_directory(tmp_dir);
          }
          if(!(is_extracted)){
            extract(folder_path+i+".zip",tmp_dir);
            is_extracted = true;
          }

          std::vector<std::string> hashes;
          if(scanningWithHeaders){
            hashes = getHashes(tmp_dir+file_rom_name, start_offset, info);
          } else {
            hashes = getHashes(tmp_dir+file_rom_name);
          }

          if (!(hashInDAT(dat_path, hashes[3], "2"))){ // SHA1 does not exist in DAT, so move file to backup folder
            if(!(filesys::exists(backup_path+i))){
              filesys::create_directory(backup_path+i);
            }
            if(file_rom_name.find('/') != std::string::npos){ // if file_rom_name has slash, make that directory structure in backup dir (so that we can move the file to the correct directory)
              filesys::path p = file_rom_name;
              std::string parentpath = p.parent_path(); // .parent_path(): gets parent path (eg: if file_rom_name = "files/a.rom", parentpath = "files")
              
              if(!(filesys::exists(backup_path+i+"/"+parentpath))){
                filesys::create_directories(backup_path+i+"/"+parentpath);
              }
            }

            filesys::rename(tmp_dir+file_rom_name, backup_path+i+"/"+file_rom_name);
            std::cout << "Moved " << file_rom_name << " in " << folder_path+i << ".zip" << " to backup folder" << std::endl;

            if(!(filesys::is_empty(tmp_dir))){ // if tmp_dir is empty directory, then we don't need to zip it (since the zip only has 1 rom with non-matching SHA1)
              to_zip.insert(i);
            }
          }
        }
      }
    }
  }

  for(auto i: to_zip){
    std::vector<std::string> files_to_zip = getAllFilesInDir(tmp_path+i);
    if(filesys::exists(folder_path+i+".zip")){
      filesys::remove(folder_path+i+".zip");
    }
    write_zip(folder_path+i+".zip",files_to_zip,tmp_path+i+"/","2"); // writing zip file to folder  
    filesys::remove_all(tmp_path+i); // delete tmp dir
  }
  filesys::remove_all(tmp_path); // delete tmp folder (in case we extracted the zip to check SHA1 but all it's SHA1s match DAT so that folder dosen't get re-zipped and removed)
  filesys::create_directory(tmp_path); // make a new tmp folder

  std::cout << "All CRC32s (now) match DAT" << std::endl;

  // comparing files in folder with files in DAT; making sure all names of files in folder match DAT; unzips, renames and rezips wrongly named files
  std::set<std::string> files_in_folder_info;
  files_in_folder = getAllFilesInDir2(folder_path); // get list of files in folder again after moving files to backup
  for(auto i: files_in_folder){
    std::map<std::string, std::string> zipinfo = getInfoFromZip(folder_path+i+".zip");
    for(auto j: zipinfo){
      files_in_folder_info.insert("\""+i+"\" \""+j.first+"\""); // j.first = filename
    }
  }
  
  std::set<std::string> cache_info_combined;
  for(int i = 0; i < cache_data.set_name.size(); i++){
    if(cache_data.status[i] == "Passed"){
      cache_info_combined.insert("\""+cache_data.set_name[i]+"\" \""+cache_data.rom_name[i]+"\"");
    }
  }

  std::vector<std::string> x = diff(files_in_folder_info,cache_info_combined,1); // files in folder but not in cache
  std::set<std::string> x_set_names; // we use a set so duplicates won't get inserted
  std::string s;
  int counter = 0;
  for(auto i: x){
    std::stringstream ss(i);
    while (ss >> std::quoted(s)){
      if(counter % 2 == 0){
        x_set_names.insert(s);
      }
      counter += 1;
    }
  }

  std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>> toAddToCache; // set name, followed by rom name, CRC32, MD5, SHA1, status
  to_zip.clear(); // vector containing names of folders in tmp/ to zip; set so duplicates won't get inserted

  for(auto i: x_set_names){
    // getting correct set name and rom name for file
    std::map<std::string, std::string> zipinfo;
    bool is_extracted = false; // whether zip file is extracted to tmp dir
    if(scanningWithHeaders){
      std::string tmp_dir = tmp_path + i + "/";
      extract(folder_path+i+".zip",tmp_dir);
      is_extracted = true;

      std::vector<std::string> files = getAllFilesInDir(tmp_dir);
      for(auto j: files){
        std::vector<std::string> fileinfo = getHashes(j, start_offset, info);
        std::string filename = filesys::path(j).filename();
        zipinfo[filename] = fileinfo[1];
      }
    } else {
      zipinfo = getInfoFromZip(folder_path+i+".zip");
    }

    std::string file_rom_name;
    std::string crc32;
    std::string correct_set_name;
    std::string correct_rom_name;
    bool crc32_is_duped = false; // whether CRC is duplicated in DAT

    for(auto j: zipinfo){ // note: this code checks all the files in the zip if one or more files in zip isin't in cache
      file_rom_name = j.first;
      crc32 = j.second;
      int index;
      std::string sha1;

      if (std::find(dat_data.crc_dupes.begin(), dat_data.crc_dupes.end(), crc32) != dat_data.crc_dupes.end()){ // if CRC is duplicated in DAT
        crc32_is_duped = true;
        std::string tmp_dir = tmp_path + i + "/";
        if(!(filesys::exists(tmp_dir))){
          filesys::create_directory(tmp_dir);
        }
        if(!(is_extracted)){
          extract(folder_path+i+".zip",tmp_dir);
          is_extracted = true;
        }
        std::vector<std::string> hashes;
        if(scanningWithHeaders){
          hashes = getHashes(tmp_dir+file_rom_name, start_offset, info);
        } else {
          hashes = getHashes(tmp_dir+file_rom_name);
        }
        sha1 = hashes[3];
        bool sha1_is_duped = false;
        std::string dir_with_correct_name;

        for(int k = 0; k < dat_data.sha1_dupes.size(); k++){
          if(sha1 == dat_data.sha1_dupes[k]){ // both CRC and SHA1 are duplicated
            sha1_is_duped = true;
            correct_rom_name = "not_set";

            for(int l = 0; l < dat_data.sha1_dupes_rom_names[k].size(); l++){
              if(dat_data.sha1_dupes_rom_names[k][l] == file_rom_name){ // file_rom_name is in dat_data.sha1_dupes_rom_names[k]
                correct_rom_name = file_rom_name;
                correct_set_name = dat_data.sha1_dupes_set_names[k][l];
                dat_data.sha1_dupes_rom_names[k].erase(dat_data.sha1_dupes_rom_names[k].begin()+l); // remove dat_data.sha1_dupes_rom_names[k][l]
                dat_data.sha1_dupes_set_names[k].erase(dat_data.sha1_dupes_set_names[k].begin()+l); // remove dat_data.sha1_dupes_set_names[k][l]
                break;
              }
            }
            if(correct_rom_name == "not_set"){ // file_rom_name is not in dat_data.sha1_dupes_rom_names[k]
              correct_rom_name = dat_data.sha1_dupes_rom_names[k][0]; // set correct rom name to be first element
              correct_set_name = dat_data.sha1_dupes_set_names[k][0]; // set correct set name to be first element
              dat_data.sha1_dupes_rom_names[k].erase(dat_data.sha1_dupes_rom_names[k].begin()+0); // remove dat_data.sha1_dupes_rom_names[k][0]
              dat_data.sha1_dupes_set_names[k].erase(dat_data.sha1_dupes_set_names[k].begin()+0); // remove dat_data.sha1_dupes_set_names[k][0]
            }
            break;
          }
        }

        if(!(sha1_is_duped)){ // CRC is duplicated but SHA1 is not
          std::tuple<std::string, std::string> names = getNameFromHash(dat_path, sha1, "2");
          correct_set_name = std::get<0>(names);
          correct_rom_name = std::get<1>(names);
        }

        dir_with_correct_name = tmp_path+correct_set_name+"/";
        if(dir_with_correct_name != i){ // if the set is named incorrectly
          if(!(filesys::exists(dir_with_correct_name))){
            filesys::rename(tmp_dir,dir_with_correct_name); // rename tmp_dir's foldername to correct_set_name so we don't extract the zip twice
          } else {
            filesys::rename(tmp_dir+file_rom_name,dir_with_correct_name+file_rom_name); // move the rom to correct_set_name
          }
        }
      } else { // neither CRC nor SHA1 are duplicated
        std::tuple<std::string, std::string> names = getNameFromHash(dat_path, crc32, "1");
        correct_set_name = std::get<0>(names);
        correct_rom_name = std::get<1>(names);
      }

      if(!(i == correct_set_name && file_rom_name == correct_rom_name)) { // if filename or setname is incorrect
        std::string tmp_dir = tmp_path + correct_set_name + "/"; // we use correct_set_name as other files in other zips could belong to that set; so we'd want it moved there
        if(!(filesys::exists(tmp_dir))){
          filesys::create_directory(tmp_dir); // create tmp dir if not present
        }
        if(!(is_extracted)){
          extract(folder_path+i+".zip",tmp_dir); // extract file to tmp dir
          filesys::remove(folder_path+i+".zip"); // remove file
          is_extracted = true;
        }
        if(correct_rom_name.find('/') != std::string::npos){ // if correct_rom_name has slash, make that directory structure in tmp dir (so that we can move the file to the correct directory)
          filesys::path p = correct_rom_name;
          std::string parentpath = p.parent_path(); // .parent_path(): gets parent path (eg: if correct_rom_name = "files/a.rom", parentpath = "files")
          
          if(!(filesys::exists(tmp_dir+parentpath))){
            filesys::create_directories(tmp_dir+parentpath);
          }
        }
        
        filesys::rename(tmp_dir+file_rom_name,tmp_dir+correct_rom_name); // rename file to correct name (and move file to correct location)
        std::cout << "Renamed " << file_rom_name << " to " << correct_rom_name << std::endl;

        if(filesys::exists(folder_path+correct_set_name+".zip")){ // if existing file exists
          extract(folder_path+correct_set_name+".zip",tmp_dir); // extract file to tmp dir
          filesys::remove(folder_path+correct_set_name+".zip"); // remove file
        }

        removeEmptyDirs(tmp_dir); // needed because e.g. if we move tmp_dir/a/Asteroids.a52 -> tmp_dir/files/Asteroids.a52 (correct location), we still need to get rid of the empty folder tmp_dir/a so it won't get zipped (if we move tmp_dir/files/AsteroidsWrongName.a52 -> tmp_dir/files/Asteroids.a52, then there's no need to remove any folder)

        to_zip.insert(correct_set_name);
      }

      if(!(crc32_is_duped)){
        toAddToCache.push_back(std::make_tuple(correct_set_name, correct_rom_name, crc32, "-", "-", "Passed"));
      } else {
        toAddToCache.push_back(std::make_tuple(correct_set_name, correct_rom_name, crc32, "-", sha1, "Passed")); // SHA1 was checked so we reflect that in cache
      }
    }
  }
  for(auto i: to_zip){
    std::vector<std::string> files_to_zip = getAllFilesInDir(tmp_path+i);
    write_zip(folder_path+i+".zip",files_to_zip,tmp_path+i+"/","2"); // writing zip file to folder  
    filesys::remove_all(tmp_path+i); // delete tmp dir
  }
  filesys::remove_all(tmp_path); // delete tmp folder (in case we extracted the zip to check SHA1 but all it's roms are named correctly so that folder dosen't get re-zipped and removed)
  filesys::create_directory(tmp_path); // make a new tmp folder

  std::cout << "All set and rom names (now) match DAT" << std::endl;

  // adding new entries to cache
  cache_data = addToCache(dat_path,toAddToCache);

  // comparing cache with DAT; gets entries in DAT but not in cache, writes "Missing" entries to cache
  toAddToCache = getMissing(dat_path, cache_data);

  // adding new entries to cache
  cache_data = addToCache(dat_path,toAddToCache);

  // counting number of sets and roms that are present
  std::tuple<int, int, int, int> count = countSetsRoms(cache_data);

  // update cache with set/rom count
  updateCacheCount(dat_path, cache_path, folder_path, count);

  std::cout << std::endl;
  printCount(count);
}