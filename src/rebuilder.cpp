#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <set>

#include "../libs/termcolor/termcolor.hpp"
#include "../libs/cpp_progress_bar/progress_bar.hpp"

#include <paths.h>
#include <gethashes.h>
#include <dir2dat.h>
#include <cache.h>
#include <dat.h>
#include "../include/archive.h"
#include <scanner.h>
#include <rebuilder.h>

namespace filesys = std::filesystem;

/*
 * Gets a list of paths to zip/rar/7z files
 *
 * Arguments:
 *     path : Path of directory
 * 
 * Returns:
 *     compressed_file_paths : Vector of paths to zip/rar/7z files in the given path; this vector is empty if the path does not contain any zip/rar/7z files
 */
std::vector<std::string> listOfCompressedFiles(std::string path){
  std::vector<std::string> files_in_path = getAllFilesInDir(path);
  std::vector<std::string> compressed_file_paths;

  for(auto i: files_in_path){
    if (i.find(".zip") != std::string::npos) { // if ".zip" exists in i
      compressed_file_paths.push_back(i);
    }
    if (i.find(".rar") != std::string::npos) { // if ".rar" exists in i
      compressed_file_paths.push_back(i);
    }
    if (i.find(".7z") != std::string::npos) { // if ".7z" exists in i
      compressed_file_paths.push_back(i);
    }
  }

  return compressed_file_paths;
}

/*
 * Recursively extracts files in path until there's no more compresed file left
 *
 * Arguments:
 *     path : Path to folder containing compressed files
*/
void recursiveExtractCompressedFiles(std::string path){
  bool has_compressed_file = true;
  while(has_compressed_file){
    std::vector<std::string> compressed_file_paths = listOfCompressedFiles(path);
    if(compressed_file_paths.size() == 0){ // path does not contain any zip/rar/7z files
      has_compressed_file = false;
      break;
    }
    for(auto i: compressed_file_paths){
      std::string::size_type const p(i.find_last_of('.'));
      std::string dir = i.substr(0, p); // e.g. if i = /a/b/c.zip, dir = /a/b/c
      dir.push_back('/'); // add "/" to dir
      filesys::create_directory(dir); // make a folder, named as the filename of the zip/rar/7z
      extract(i,dir); // extract the zip/rar/7z to that folder
      filesys::remove(i); // remove the file after extraction
    }
  }  
}

/*
 * Rebuilds files in rebuild folder that have CRC32, MD5, SHA1 in DAT to the romset. Also updates the entries in cache (if any). (*Scanner has to be run first to generate the cache)
 *
 * Arguments:
 *     dat_path : Path to DAT file
 *     folder_path : Path to folder to be scanned against the DAT file, i.e. path to the romset (*Path must end with a forward slash)
 *     toRemove : if true, files in rebuild folder that match DAT will be removed; if false, the files will not be removed
*/
void rebuild(std::string dat_path, std::string folder_path, bool toRemove){
  // checks
  if(!(filesys::exists(dat_path))){
    std::cout << dat_path << " does not exist!" << std::endl;
    exit(0);
  }

  if(!(filesys::is_directory(folder_path))){
    std::cout << folder_path << " is not a directory!" << std::endl;
    exit(0);
  }

  std::string cache_path = std::get<0>(getCachePath(dat_path)); // getting path to cache from DAT path
  if(!(filesys::exists(cache_path))){
    std::cout << "Cache does not exist, please run scanner first (with -s | --scan) to create it." << std::endl;
    exit(0);
  }

  std::cout << "Rebuilding " << dat_path << std::endl;

  // recursively extract files in rebuild_path until there's no more compressed file left
  recursiveExtractCompressedFiles(rebuild_path);
  std::cout << "Extracted all compressed archives (if any)" << std::endl;
  
  std::vector<std::string> files_in_path = getAllFilesInDir(rebuild_path);
  datData dat_data = getDataFromDAT(dat_path);
  cacheData cache_data = getDataFromCache(dat_path);
  std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>> toAddToCache; // set name, followed by rom name, CRC32, MD5, SHA1, status
  std::set<std::string> to_zip; // vector containing names of folders in tmp/ to zip; set so duplicates won't get inserted

  for(auto i: files_in_path){
    std::vector<std::string> file_info = getHashes(i); // rebuilder has to check all 3 hashes: CRC32, MD5, SHA1
    bool hashMatchInDAT = false;
    bool sha1_is_duped = false;
    std::string status;

    for(int j = 0; j < dat_data.sha1.size(); j++){
      if(dat_data.crc32[j] == file_info[1] && dat_data.md5[j] == file_info[2] && dat_data.sha1[j] == file_info[3]){
        hashMatchInDAT = true;

        if (std::find(dat_data.sha1_dupes.begin(), dat_data.sha1_dupes.end(), file_info[3]) != dat_data.sha1_dupes.end()){
          sha1_is_duped = true;
        }

        // getting status in cache
        for(int k = 0; k < cache_data.set_name.size(); k++){
          if(cache_data.set_name[k] == dat_data.set_name[j]){
            status = cache_data.status[k];
            break;
          }
        }

        if(status == "Passed"){ // file is already in romset
          filesys::remove(i); // remove file
          std::cout << "Deleted " << i << " (already in romset)" << std::endl;
        } else {
          std::string correct_set_name = dat_data.set_name[j];
          std::string correct_rom_name = dat_data.rom_name[j];

          std::string tmp_dir = tmp_path + correct_set_name + "/"; // we use correct_set_name as other files in rebuild_path could belong to that set; so we'd want it moved there
          if(!(filesys::exists(tmp_dir))){
            filesys::create_directory(tmp_dir); // create tmp dir if not present
          }

          if(correct_rom_name.find('/') != std::string::npos){ // if correct_rom_name has slash, make that directory structure in tmp dir (so that we can move the file to the correct directory)
            filesys::path p = correct_rom_name;
            std::string parentpath = p.parent_path(); // .parent_path(): gets parent path (eg: if correct_rom_name = "files/a.rom", parentpath = "files")
            
            if(!(filesys::exists(tmp_dir+parentpath))){
              filesys::create_directories(tmp_dir+parentpath);
            }
          }

          bool file_in_tmp_dir = false; // whether we moved/copied i to tmp_dir+correct_rom_name
          if(toRemove && !(sha1_is_duped)){ // if SHA1 is duplicated in DAT, we can't move the rom since we have to rebuild it to other sets too
            filesys::rename(i,tmp_dir+correct_rom_name); // rename file to correct name and move file to tmp dir
            file_in_tmp_dir = true;
          } else if (!(filesys::exists(tmp_dir+correct_rom_name))) { // if that path already exists, we don't copy it. e.g. set 1 has rom A and rom B, set 2 has rom C and rom D, rom A and rom C are identical (same CRC32, MD5, SHA1). after set 1 has been rebuilt, set 2's rom C would copy to set 1's rom A and error at filesys::copy_file since it already exists
            filesys::copy_file(i,tmp_dir+correct_rom_name);
            file_in_tmp_dir = true;
          }

          if(file_in_tmp_dir){
            std::cout << "Renamed " << i << " to " << correct_rom_name << std::endl;

            if(filesys::exists(folder_path+correct_set_name+".zip")){ // if existing file exists in romset
              extract(folder_path+correct_set_name+".zip",tmp_dir); // extract file to tmp dir
              filesys::remove(folder_path+correct_set_name+".zip"); // remove file
            }

            removeEmptyDirs(tmp_dir); // needed because e.g. if we move tmp_dir/a/Asteroids.a52 -> tmp_dir/files/Asteroids.a52 (correct location), we still need to get rid of the empty folder tmp_dir/a so it won't get zipped (if we move tmp_dir/files/AsteroidsWrongName.a52 -> tmp_dir/files/Asteroids.a52, then there's no need to remove any folder)

            to_zip.insert(correct_set_name);
            toAddToCache.push_back(std::make_tuple(correct_set_name, correct_rom_name, dat_data.crc32[j], dat_data.md5[j], dat_data.sha1[j], "Passed"));
          }
        }

        if(!(sha1_is_duped)){ // can only break if SHA1 is not duplicated in DAT
          break;
        }
      }
    }
    if(!(hashMatchInDAT)){ // CRC32, MD5, SHA1 not in DAT
      filesys::remove(i); // remove file
      std::cout << "Deleted " << i << " (does not match DAT)" << std::endl;
    }
  }

  // zipping files
  for(auto i: to_zip){
    std::vector<std::string> files_to_zip = getAllFilesInDir(tmp_path+i);
    write_zip(folder_path+i+".zip",files_to_zip,tmp_path+i+"/","2"); // writing zip file to folder  
    filesys::remove_all(tmp_path+i); // delete tmp dir
  }
  std::cout << "All files that match against DAT moved to romset" << std::endl;

  // adding new entries to cache
  cache_data = addToCache(dat_path,toAddToCache);

  if(toRemove){
    filesys::remove_all(rebuild_path); // delete rebuild folder
    filesys::create_directory(rebuild_path); // make a new rebuild folder
  }

  // counting number of sets and roms that are present
  cache_data = getDataFromCache(dat_path); // need to do this or else set/rom count will be wrong
  std::tuple<int, int, int, int> count = countSetsRoms(cache_data);

  // update cache with set/rom count
  updateCacheCount(dat_path, cache_path, folder_path, count);

  std::cout << std::endl;
  printCount(count);
}