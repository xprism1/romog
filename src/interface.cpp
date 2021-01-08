#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <regex>

#include <yaml-cpp/yaml.h>
#include <curl/curl.h>
#include <pugixml.hpp>

#include "../libs/termcolor.hpp"
#include "../libs/libfort/src/fort.hpp"

#include <paths.h>
#include <dir2dat.h>
#include <cache.h>
#include <scanner.h>
#include <rebuilder.h>
#include <interface.h>

namespace filesys = std::filesystem;

int counter = 0; // no. of .dat files traversed
std::vector<std::tuple<std::string, std::string>> config_info;
std::vector<std::string> site_rdmp_names; // filenames without extension and date of DATs from redump site
std::vector<std::string> site_rdmp_dates; // date of DATs from redump site
std::vector<std::string> profilexml_names; // filenames without extension and date of DATs from profile.xmls
std::vector<std::string> profilexml_dates; // date of DATs from profile.xmls
int dat_up_to_date = 0; // no. of up-to-date .dat files
int dat_outdated = 0; // no. of outdated .dat files
int dat_unknown = 0; // no. of .dat files with unknown status

/*
 * Adds nodes to a YAML::Node
 * 
 * Arguments:
 *     node : A YAML::Node, cannot be blank
 *     begin : Iterator to first element of sequence
 *     end : Iterator to last element of sequence
 *     value : Value of last node
 * 
 * E.g. std::vector\<std::string> vec = {"a","b","c"}, value = "d"
 * Upon running addNodes(node,vec.begin(),vec.end(),"d"), the following will be added to node:
 * a:
 *  b:
 *   c: d
 * 
 * Notes:
 *     Taken from https://stackoverflow.com/a/49602621
*/
template<typename T, typename Iter>
void addNodes(YAML::Node node, Iter begin, Iter end, T value) {
  if (begin == end) {
    return;
  }
  const auto& tag = *begin;
  if (std::next(begin) == end) {
    node[tag] = value;
    return;
  }
  if (!node[tag]) {
    node[tag] = YAML::Node(YAML::NodeType::Map);
  }
  addNodes(node[tag], std::next(begin), end, value);
}

/*
 * Sorts paths by parent path length (longest first), then sorts the filenames in alphabetical order.
 * 
 * Arguments:
 *     paths : Vector containing paths to be sorted
 *
 * Returns:
 *     paths : Sorted vector
 * 
 * Notes:
 *     Taken from https://codereview.stackexchange.com/a/183315
*/
std::vector<std::string> sortPaths(std::vector<std::string> paths){
  std::sort(paths.begin(), paths.end(), [&](const std::string &str1, const std::string &str2) {
    filesys::path path1(str1);
    filesys::path path2(str2);

    if(path1.parent_path() != path2.parent_path()){
      return path1.parent_path() > path2.parent_path();
    } else {
      return path1.filename() < path2.filename();
    }
  });

  return paths;
}

/*
 * Generates a config file (YAML with DAT path and path to folder containing roms) if existing config does not exist. If an existing config exists, it will add new DAT paths in dats/ to it (if any).
 * The default folder path is "Insert path to romset here"; if info is provided, it will autofill folder paths for that DAT group, provided the DATs are in dats/<dat group name>. Note that autofilling will only work if the DAT isin't in the config.
 * 
 * Arguments:
 *     info (Optional) : Tuple containing name of DAT group to autofill and base path. E.g. if base path is /path/to/folder, folder path for dats/<dat group name>/a.dat will be set to /path/to/folder/a
*/
void genConfig(std::tuple<std::string, std::string> info){
  YAML::Node config;
  YAML::Node dats;

  bool autoFill = true;
  if(std::get<0>(info) == "not_set" && std::get<1>(info) == "not_set"){
    autoFill = false;
  }

  std::vector<std::string> path_to_dats = getAllFilesInDir(dats_path);
  path_to_dats = sortPaths(path_to_dats); // sort paths

  bool config_exists = false;
  if(filesys::exists(config_path)){ // if config file exists
    config = YAML::LoadFile(config_path);
    config_exists = true;
  }
  dats = config["dats"];

  for(auto i: path_to_dats){
    i = i.substr(dats_path.size()); // remove "/home/xp/Desktop/xp (My Tools)/romorganizer/debug/dats/" from i
    std::vector<std::string> decomposed_path;
    for(auto j: filesys::path(i)){ // e.g. if i == "/path/to/folder", decomposed_path = {"path","to","folder"}
      decomposed_path.push_back(j);
    }

    bool inConfig = false; // will always be false if config file dosen't exist
    std::string datfilename = std::get<0>(getFileName(i)); // datfilename: DAT filename with extension
    if(filesys::exists(config_path)){
      std::ifstream file(config_path);
      std::string line;
      while(std::getline(file,line)){
        if(line.find(datfilename) != std::string::npos){ // if DAT path exists in any line in config
          inConfig = true;
          break;
        }
      }
      file.close();
    }

    if(!(inConfig)){
      if(!(autoFill)){
        addNodes(dats,decomposed_path.begin(),decomposed_path.end(),"Insert path to romset here");
      } else {
        if(decomposed_path[0] == std::get<0>(info)){ // if DAT group matches what we want to autofill
          std::string fname_noed = std::get<2>(getDatName(decomposed_path.back())); // decomposed_path.back() = last element of decomposed_path
          addNodes(dats,decomposed_path.begin(),decomposed_path.end(),std::get<1>(info)+"/"+fname_noed); // path = base path + "/" + DAT filename without extension and date
        } else { // DAT group dosen't match what we want to autofill, so we just add the default folder path
          addNodes(dats,decomposed_path.begin(),decomposed_path.end(),"Insert path to romset here");
        }
      }
    }
  }

  std::ofstream output(config_path);
  output << config; // save to config file
  output.close();

  if(!(config_exists)){
    std::cout << "Generated config at " << config_path << std::endl;
  } else {
    std::cout << "Updated config at " << config_path << std::endl;
  }
  if(autoFill){
    std::cout << "Autofilled folder paths" << std::endl;
  }
}

/*
 * Recursively prints keys from key-value pairs in a YAML file. In this case, said YAML file is the config file, so it prints the DAT filenames.
 * The output is formatted as a tree, including profile number and <sets have>/<sets total>, with color. (Grey: unscanned, Red: sets have = 0, Orange: 0 < set have < set total, Green: you have all the sets)
 * 
 * Arguments:
 *     node : A YAML::Node, cannot be blank
 *     prefix : Character(s) in front of the filename
 * 
 * Notes:
 *     Taken from https://github.com/domfarolino/directory-tree-print, https://stackoverflow.com/a/9930074
 *     Uses library: https://github.com/ikalnytskyi/termcolor
*/
void unroll(const YAML::Node & node, std::string prefix) {
  for(auto it = node.begin(); it != node.end(); ++ it){
    std::string tmpprefix = prefix;
    tmpprefix.replace(tmpprefix.size()-3, 3, "├─");

    std::string name = it->first.as<std::string>();
    if(name.find(".dat") != std::string::npos){
      std::tuple<std::string, std::string> cache_info = getCachePath(dats_path+name);
      std::string cache_path = std::get<0>(cache_info);
      std::string set_have = "?";
      std::string set_total = "?";

      if(filesys::exists(cache_path)){ // if cache file exists
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
        set_have = cache_info[2];
        set_total = cache_info[3];
      }
      counter += 1;
      
      if(set_have == set_total && set_have != "?" && set_have != "0"){ // have all sets, so output dat name in green
        std::cout << termcolor::green << tmpprefix << termcolor::magenta << " [" << std::to_string(counter) << "] " << termcolor::green << name << termcolor::cyan << " [" << set_have << "/" << set_total << "]" << termcolor::reset << std::endl;
      } else if (set_have != set_total && set_have != "?" && set_have != "0"){ // have partial sets, so output dat name in orange
        std::cout << termcolor::color<255,165,0> << tmpprefix << termcolor::magenta << " [" << std::to_string(counter) << "] " << termcolor::color<255,165,0> << name << termcolor::cyan << " [" << set_have << "/" << set_total << "]" << termcolor::reset << std::endl;
      } else if (set_have == "0"){ // have no sets, so output dat name in red
        std::cout << termcolor::red << tmpprefix << termcolor::magenta << " [" << std::to_string(counter) << "] " << termcolor::red << name << termcolor::cyan << " [" << set_have << "/" << set_total << "]" << termcolor::reset << std::endl;
      } else if (set_have == "?" && set_total == "?"){ // no data in cache, so output dat name in gray
        std::cout << termcolor::color<169,169,169> << tmpprefix << termcolor::magenta << " [" << std::to_string(counter) << "] " << termcolor::color<169,169,169> << name << termcolor::cyan <<  " [" << set_have << "/" << set_total << "]" << termcolor::reset << std::endl;
      } // profile number is colored magenta, set have/set count is colored cyan

    } else { // if its not a dat file (e.g. name of folder)
      std::cout << tmpprefix << name << std::endl; // we print it without any coloring
    }

    const YAML::Node & data = it->second.as<YAML::Node>();
    unroll(data, prefix + "   │");
  }
}

/*
 * Calls unroll() with the correct settings
*/
void listProfiles(){
  YAML::Node config = YAML::LoadFile(config_path);
  YAML::Node dats = config["dats"];
  std::cout << "." << std::endl;
  unroll(dats, "│");
}

/*
 * Gets DAT path and folder path from profile number
 * 
 * Arguments:
 *     profile_no : Profile number (shown in square brackets before the DAT name, in listProfiles())
 *
 * Returns:
 *     paths : Tuple containing DAT path and folder path (in that order)
 * 
 * Notes:
 *     Taken from https://codereview.stackexchange.com/a/183315
*/
std::tuple<std::string, std::string> getPaths(std::string profile_no){
  std::ifstream file(config_path);
  std::string line;
  int dat_count = 0;

  while(std::getline(file,line)){
    if(line.find(".dat") != std::string::npos){ // if DAT path exists in any line in config
      dat_count += 1;
    }
    if(dat_count == std::stoi(profile_no)){
      break;
    }
  }
  file.close();

  // checks
  if(profile_no != std::to_string(dat_count) || profile_no == "0"){
    std::cout << "Profile number incorrect!" << std::endl;
    exit(0);
  }

  line = std::regex_replace(line, std::regex("^ +"), ""); // remove leading whitespace (indents in config file)

  std::stringstream ss(line);
  std::string s;
  std::vector<std::string> seglist;

  while(std::getline(ss, s, ':')){ // split line by ":" into seglist
    seglist.push_back(s);
  }

  seglist[1].erase(0,1); // remove first character after ":" (space)

  std::vector<std::string> dat_paths = getAllFilesInDir(dats_path);
  std::string dat_path;
  for(auto i: dat_paths){ // we have the filename of the DAT, but we need the full path
    if (i.find(seglist[0]) != std::string::npos) {
      dat_path = i;
      break;
    }
  }

  std::tuple<std::string, std::string> paths = std::make_tuple(dat_path,seglist[1]);
  return paths;
}

/*
 * Prints set name and rom name from DAT in a table. The row is colored green if you have the rom, red if not. Also prints set/rom count at the end.
 * Optionally, it can print CRC32/MD5/SHA1 hashes, as well as only entries that are "Missing" or "Passed".
 * 
 * Arguments:
 *     dat_path : Path to DAT
 *     hash (Optional) : "c" to print CRC32, "m" to print "MD5", "s" to print SHA1. Other accepted values are "cm", "cs", "ms", "cms". If this value is not passed, no hashes will be printed.
 *     show (Optional) : "p" to only print entries marked as "Passed"; "m" to only print entries marked as "Missing". If this value is not passed, all entries will be printed.
*/
void showInfo(std::string dat_path, std::string hash, std::string show){
  cacheData cache_data = getDataFromCache(dat_path);
  datData dat_data = getDataFromDAT(dat_path);
  std::vector<std::tuple<std::string,std::string,std::string>> cache_data_combined;
  for(int i = 0; i < cache_data.set_name.size(); i++){
    if(show == "p"){
      if(cache_data.status[i] == "Passed"){
        cache_data_combined.push_back(std::make_tuple(cache_data.set_name[i],cache_data.rom_name[i],cache_data.status[i]));
      }
    } else if (show == "m"){
      if(cache_data.status[i] == "Missing"){
        cache_data_combined.push_back(std::make_tuple(cache_data.set_name[i],cache_data.rom_name[i],cache_data.status[i]));
      }
    } else {
      cache_data_combined.push_back(std::make_tuple(cache_data.set_name[i],cache_data.rom_name[i],cache_data.status[i]));
    }
  }
  sort(cache_data_combined.begin(), cache_data_combined.end()); // sort cache_data_combined by first element of tuple (set name)

  fort::char_table table; // create table
  table.set_border_style(FT_SOLID_STYLE); // set table border style

  // fill table with data                                                  
  table << fort::header;

  if(hash == "not_set"){
    table.write_ln("Set Name", "Rom Name");
  } else if (hash == "c"){
    table.write_ln("Set Name", "Rom Name","CRC32");
  } else if (hash == "m"){
    table.write_ln("Set Name", "Rom Name","MD5");
  } else if (hash == "s"){
    table.write_ln("Set Name", "Rom Name","SHA1");
  } else if (hash == "cm"){
    table.write_ln("Set Name", "Rom Name","CRC32","MD5");
  } else if (hash == "cs"){
    table.write_ln("Set Name", "Rom Name","CRC32","SHA1");
  } else if (hash == "ms"){
    table.write_ln("Set Name", "Rom Name","MD5","SHA1");
  } else if (hash == "cms"){
    table.write_ln("Set Name", "Rom Name","CRC32","MD5","SHA1");
  }
  table.row(0).set_cell_content_fg_color(fort::color::cyan);
  table.row(0).set_cell_text_style(fort::text_style::bold);

  int counter = 0;
  for(auto i: cache_data_combined){
    if(hash != "not_set"){
      std::string crc32;
      std::string md5;
      std::string sha1;
      for(int j = 0; j < dat_data.rom_name.size(); j++){ // getting CRC32, MD5, SHA1 from DAT
        if(dat_data.rom_name[j] == std::get<1>(i) && dat_data.set_name[j] == std::get<0>(i)){
          crc32 = dat_data.crc32[j];
          md5 = dat_data.md5[j];
          sha1 = dat_data.sha1[j];
          break;
        }
      }

      if(hash == "c"){
        table.write_ln(std::get<0>(i),std::get<1>(i),crc32);
      } else if (hash == "m"){
        table.write_ln(std::get<0>(i),std::get<1>(i),md5);
      } else if (hash == "s"){
        table.write_ln(std::get<0>(i),std::get<1>(i),sha1);
      } else if (hash == "cm"){
        table.write_ln(std::get<0>(i),std::get<1>(i),crc32,md5);
      } else if (hash == "cs"){
        table.write_ln(std::get<0>(i),std::get<1>(i),crc32,sha1);
      } else if (hash == "ms"){
        table.write_ln(std::get<0>(i),std::get<1>(i),md5,sha1);
      } else if (hash == "cms"){
        table.write_ln(std::get<0>(i),std::get<1>(i),crc32,md5,sha1);
      }
    } else {
      table.write_ln(std::get<0>(i),std::get<1>(i));
    }

    counter += 1;
    if(std::get<2>(i) == "Passed"){
      table.row(counter).set_cell_content_fg_color(fort::color::green); // set that row's color to green
    } else if (std::get<2>(i) == "Missing"){
      table.row(counter).set_cell_content_fg_color(fort::color::red); // set that row's color to red
    }
  }

  // display table
  std::string datfilename = std::get<0>(getFileName(dat_path)); // datfilename: DAT filename with extension
  std::cout << termcolor::magenta << termcolor::bold << datfilename << ":" << termcolor::reset << std::endl;
  std::cout << table.to_string() << std::endl;

  // display set/rom count
  int sets_have = std::stoi(cache_data.info[6]);
  int sets_total = std::stoi(cache_data.info[7]);
  int roms_have = std::stoi(cache_data.info[8]);
  int roms_total = std::stoi(cache_data.info[9]);

  if(sets_have == 0){ // output count in red
    std::cout << termcolor::cyan << "Sets have:    " << termcolor::red << sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Sets missing: " << termcolor::red << sets_total-sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Roms have:    " << termcolor::red << roms_have << "/" << roms_total << std::endl;
    std::cout << termcolor::cyan << "Roms missing: " << termcolor::red << roms_total-roms_have << "/" << roms_total << std::endl;
  } else if (sets_have < sets_total){ // output count in orange
    std::cout << termcolor::cyan << "Sets have:    " << termcolor::color<255,165,0> << sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Sets missing: " << termcolor::color<255,165,0> << sets_total-sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Roms have:    " <<termcolor::color<255,165,0> << roms_have << "/" << roms_total << std::endl;
    std::cout << termcolor::cyan << "Roms missing: " << termcolor::color<255,165,0> << roms_total-roms_have << "/" << roms_total << std::endl;
  } else if (sets_have == sets_total){ // output count in green
    std::cout << termcolor::cyan << "Sets have:    " << termcolor::green << sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Sets missing: " << termcolor::green << sets_total-sets_have << "/" << sets_total << std::endl;
    std::cout << termcolor::cyan << "Roms have:    " << termcolor::green << roms_have << "/" << roms_total << std::endl;
    std::cout << termcolor::cyan << "Roms missing: " << termcolor::green << roms_total-roms_have << "/" << roms_total << std::endl;
  }


  std::cout << termcolor::reset << std::endl;
}

/*
 * A modification of unroll() which adds tuples (containing DAT path and folder path in config file) to a vector config_info
 * 
 * Arguments:
 *     node : A YAML::Node, cannot be blank
*/
void unroll2(const YAML::Node & node) {
  for(auto it = node.begin(); it != node.end(); ++ it){
    std::string name = it->first.as<std::string>();
    if(name.find(".dat") != std::string::npos){
      config_info.push_back(std::make_tuple(it->first.as<std::string>(),it->second.as<std::string>()));
    }
    const YAML::Node & data = it->second.as<YAML::Node>();
    unroll2(data);
  }
}

/*
 * Batch scans romsets by their DAT group, optionally rebuilding roms from rebuild path
 * 
 * Arguments:
 *     dat_group : DAT group name
 *     to_rebuild (Optional) : true to rebuild roms from rebuild path
*/
void batchScan(std::string dat_group, bool toRebuild){
  YAML::Node config = YAML::LoadFile(config_path);
  YAML::Node dats = config["dats"];
  YAML::Node datgrp = dats[dat_group];
  config_info.clear();

  unroll2(datgrp);
  for(auto i: config_info){
    std::vector<std::string> dat_paths = getAllFilesInDir(dats_path);
    std::string dat_path;
    for(auto j: dat_paths){ // we have the filename of the DAT, but we need the full path
      if (j.find(std::get<0>(i)) != std::string::npos) {
        dat_path = j;
        break;
      }
    }

    std::string folder_path = std::get<1>(i);
    if(folder_path.back() != '/'){ // if last character is not a forwardslash
      folder_path.push_back('/'); // add it
    }

    scan(dat_path, folder_path);

    if(toRebuild){
      rebuild(dat_path, folder_path, false); // false so rebuild folder won't get deleted
    }
  }
}

/*
 * Removes cache, optionally removes DAT, entry in config file and romset.
 * 
 * Arguments:
 *     dat_path : Path to DAT
 *     toRemoveEntry (Optional) : true to remove DAT, entry in config file and romset
*/
void deleteProfile(std::string dat_path, bool toRemoveEntry){
  std::tuple<std::string, std::string> cache_info = getCachePath(dat_path);
  std::string cache_path = std::get<0>(cache_info);

  // checks
  if(!(filesys::exists(cache_path))){
    std::cout << "Cache does not exist, not deleting anything." << std::endl;
    exit(0);
  }

  if(toRemoveEntry){
    if(!(filesys::exists(dat_path))){
      std::cout << "DAT does not exist, not deleting anything." << std::endl;
      exit(0);
    }

    filesys::remove(dat_path); // remove DAT
    std::cout << "Removed " << dat_path << std::endl;

    // removing entry in config file
    std::ifstream file_in(config_path);
    std::ofstream file_out("temp.txt");
    std::string line;
    std::string datfilename = std::get<0>(getFileName(dat_path)); // datfilename: DAT filename with extension

    while(std::getline(file_in,line)){
      if(!(line.find(datfilename) != std::string::npos)){ // if DAT path exists in any line in config
        file_out << line << std::endl;
      }
    }
    file_in.close();
    file_out.close();
    filesys::remove(config_path);
    filesys::rename("temp.txt",config_path);
    std::cout << "Removed entry in config" << std::endl;

    // removing romset
    // reading from cache
    std::ifstream file(cache_path);
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

    if(!(filesys::exists(cache_info[1]))){
      std::cout << "Romset does not exist, not deleting anything." << std::endl;
      exit(0);
    }

    filesys::remove_all(cache_info[1]);
    std::cout << "Removed " << cache_info[1] << std::endl;
  }

  filesys::remove(cache_path); // remove cache
  std::cout << "Removed " << cache_path << std::endl;
}

/*
 * Updates DAT files in DAT folder, by replacing old DATs with new DATs in newDAT folder. Also updates the relevant DAT name in config file. Optionally downloads DATs from the links text file to new-DATs-path/YYYYMMDD-HHMMSS (empty lines or lines beginning with '#' are ignored)
 * 
 * Arguments:
 *     download (Optional) : Whether to download new DATs from the links text file.
 * 
 * References:
 *     https://stackoverflow.com/a/35282907, https://unix.stackexchange.com/a/74337, https://daniel.haxx.se/blog/2020/09/10/store-the-curl-output-over-there/
*/
void updateDats(bool download){
  // checks
  if(filesys::is_empty(dats_new_path)){
    std::cout << dats_new_path << " is empty, add some DAT files to it before running (-u | --update-dats)" << std::endl;
    exit(0);
  }

  // downloading new DATs
  if(download){
    // getting YYYYMMDD-HHMMSS
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer,80,"%Y%m%d-%H%M%S",timeinfo);
    std::string date_and_time(buffer);
    if(!(filesys::exists(dats_new_path+date_and_time))){
      filesys::create_directory(dats_new_path+date_and_time);
    }

    std::cout << "Downloading new DATs from " << links_path << " to " << dats_new_path+date_and_time << "..." << std::endl;
    sleep(1);
    std::ifstream in(links_path);
    std::string line;

    while(std::getline(in, line)){
      if (!(line == "" || line.at(0) == '#')){ // ignore blank lines and lines that start with "#"
        int result = system(("curl -JLO --output-dir \"" + dats_new_path+date_and_time + "\" " + line).c_str()); // download from the link, save to dats_new_path/YYYYMMDD-HHMMSS
      }
    }
    in.close();
  }

  // recursively extract files in rebuild_path until there's no more compresed file left
  recursiveExtractCompressedFiles(dats_new_path);
  std::cout << "Extracted all compressed archives (if any)" << std::endl;
  std::cout << std::endl;

  // getting filename, date and path of new DATs
  std::vector<std::string> paths_to_new_dats = getAllFilesInDir(dats_new_path);
  std::set<std::tuple<std::string, std::string>> new_dats_info; // first element in tuple: DAT filename; second element in tuple: DAT date
  std::set<std::tuple<std::string, std::string>> new_dats_paths; // first element in tuple: DAT filename; second element in tuple: path to that DAT
  std::string previous_elem = "";
  for(auto i: paths_to_new_dats){
    if(i.substr(i.length() - 3) == "dat" || "xml"){ // if last 3 characters are "dat" or "xml"
      std::string fname_noed = std::get<2>(getDatName(i));
      std::string date = std::get<3>(getDatName(i));
      new_dats_info.insert(std::make_tuple(fname_noed,date));

      if(previous_elem != fname_noed){ // prevent duplicates from being added
        new_dats_paths.insert(std::make_tuple(fname_noed,i));
      }
      previous_elem = fname_noed;
    }
  }

  // spliting new_dats_info and new_dats_paths
  std::vector<std::string> new_dats_name;
  std::vector<std::string> new_dats_date;
  std::vector<std::string> new_dats_path;
  for(auto i: new_dats_info){
    new_dats_name.push_back(std::get<0>(i));
    new_dats_date.push_back(std::get<1>(i));
  }
  for(auto i: new_dats_paths){
    new_dats_path.push_back(std::get<1>(i));
  }

  // getting filename, date and path of old DATs; replacing the old DAT with the new one if its date is older
  std::vector<std::string> paths_to_dats = getAllFilesInDir(dats_path);
  std::vector<std::tuple<std::string, std::string>> old_dats; // first element in tuple: old DAT filename; second element in tuple: new DAT filename (to replace in config file)
  for(auto i: paths_to_dats){
    std::string fname_noed = std::get<2>(getDatName(i));
    std::string date = std::get<3>(getDatName(i));

    for(int j = 0; j < new_dats_name.size(); j++){
      if(new_dats_name[j] == fname_noed){
        if(std::stol(new_dats_date[j]) > std::stol(date)){ // convert new_dats_date[j] and date (both strings) to long, and compare the numbers
          filesys::path p(i);
          std::string parent_path = p.parent_path();
          std::string new_fname = std::get<0>(getFileName(new_dats_path[j]));
          std::string old_fname = std::get<0>(getFileName(i));

          filesys::rename(new_dats_path[j],parent_path+"/"+new_fname); // move new_dats_path[j] to directory with old DAT
          filesys::remove(i); // delete old DAT
          std::cout << new_dats_name[j] << ": " << formatDate(date) << " -> " << formatDate(new_dats_date[j]) << std::endl;

          old_dats.push_back(std::make_tuple(old_fname,new_fname));
        }
        break;
      }
    }
  }

  // update config file
  std::ifstream filein(config_path);
  std::ofstream fileout("temp.txt");
  std::string line;

  while(std::getline(filein,line)){
    for(auto i: old_dats){
      std::size_t p = line.find(std::get<0>(i));
      if (p != std::string::npos){
        line.replace(p, std::get<0>(i).length(), std::get<1>(i)); // replace std::get<0>(i) (old DAT filename) with std::get<1>(i) (new DAT filename) in line (only one replacement is done per line)
      }
    }
    fileout << line << std::endl;
  }
  filein.close();
  fileout.close();

  filesys::remove(config_path);
  filesys::rename("temp.txt",config_path);

  std::cout << std::endl;
  std::cout << "Updated config at " << config_path << std::endl;
}

/*
 * A modification of unroll() which prints DAT filenames in DAT path as well as the latest available DAT filename (if the DAT is outdated) (Red: the DAT is outdated, Green: the DAT is up-to-date, Grey: no data to tell whether DAT is up-to-date or not)
 * 
 * Arguments:
 *     node : A YAML::Node, cannot be blank
 *     prefix : Character(s) in front of the filename
*/
void unroll3(const YAML::Node & node, std::string prefix) {
  for(auto it = node.begin(); it != node.end(); ++ it){
    std::string tmpprefix = prefix;
    tmpprefix.replace(tmpprefix.size()-3, 3, "├─");
    std::string name = it->first.as<std::string>();

    std::string fname_noed;
    std::string date;
    std::string updated_date;
    if(name.find(".dat") != std::string::npos){
      fname_noed = std::get<2>(getDatName(name));
      date = std::get<3>(getDatName(name));

      int result = 0; // 0 = unknown, 1 = outdated, 2 = up-to-date
      for(int i = 0; i < site_rdmp_names.size(); i++){
        if(fname_noed == site_rdmp_names[i]){
          if(std::stol(site_rdmp_dates[i]) > std::stol(date)){ // convert site_rdmp_dates[j] and date (both strings) to long, and compare the numbers
            result = 1;
            updated_date = site_rdmp_dates[i];
          } else if (std::stol(site_rdmp_dates[i]) == std::stol(date)){ // convert site_rdmp_dates[j] and date (both strings) to long, and compare the numbers
            result = 2;
            updated_date = site_rdmp_dates[i];
          }
          break;
        }
      }
      for(int i = 0; i < profilexml_names.size(); i++){
        if(fname_noed == profilexml_names[i]){
          if(std::stol(profilexml_dates[i]) > std::stol(date)){ // convert profilexml_dates[j] and date (both strings) to long, and compare the numbers
            result = 1;
            updated_date = profilexml_dates[i];
          } else if (std::stol(profilexml_dates[i]) == std::stol(date)){ // convert profilexml_dates[j] and date (both strings) to long, and compare the numbers
            result = 2;
            updated_date = profilexml_dates[i];
          }
          break;
        }
      }
      counter += 1;

      if(result == 1){ // outdated, so output red
        dat_outdated += 1;
        std::cout << termcolor::red << tmpprefix << termcolor::magenta << " [" << std::to_string(counter) << "] " << termcolor::cyan << fname_noed << termcolor::red << " (" << formatDate(date) << ")" << termcolor::magenta << " -> " << termcolor::green << "(" << formatDate(updated_date) << ")" << termcolor::reset << std::endl;
      } else if (result == 2){ // up-to-date, so output green
        dat_up_to_date += 1;
        std::cout << termcolor::green << tmpprefix << termcolor::magenta << " [" << std::to_string(counter) << "] " << termcolor::cyan << fname_noed << termcolor::green << " (" << formatDate(date) << ")" << termcolor::reset << std::endl;
      } else { // unknown, so output grey
        dat_unknown += 1;
        std::cout << termcolor::color<169,169,169> << tmpprefix << termcolor::magenta << " [" << std::to_string(counter) << "] " << termcolor::cyan << fname_noed << termcolor::color<169,169,169> << " (" << formatDate(date) << ")" << termcolor::reset << std::endl;
      }
    } else { // if its not a dat file (e.g. name of folder)
      std::cout << tmpprefix << name << std::endl; // we print it without any coloring
    }

    const YAML::Node & data = it->second.as<YAML::Node>();
    unroll3(data, prefix + "   │");
  }
}

// needed for listProfilesWithDate()
static size_t OnReceiveData (void * pData, size_t tSize, size_t tCount, void * pmUser) {
  size_t length = tSize * tCount, index = 0;
  while (index < length) {
    unsigned char *temp = (unsigned char *)pData + index;
    if ((temp[0] == '\r') || (temp[0] == '\n'))
        break;
    index++;
  }

  std::string str((unsigned char*)pData, (unsigned char*)pData + index);
  std::map<std::string, std::string>* pmHeader = (std::map<std::string, std::string>*)pmUser;
  size_t pos = str.find(": ");
  if (pos != std::string::npos)
      pmHeader->insert(std::pair<std::string, std::string> (str.substr(0, pos), str.substr(pos + 2)));

  return (tCount);
}

/*
 * Queries redump.org links in links.txt to get the latest DAT name and date. Also downloads profile.xmls in www.txt to get the latest DAT name and date for those DAT groups. Then calls unroll3() with the correct settings.
 * 
 * Resources:
 *     https://stackoverflow.com/a/47100183, https://stackoverflow.com/a/61007544
*/
void listProfilesWithDate(){
  std::ifstream in(links_path);
  std::string line;
  CURL *curl = curl_easy_init();
  std::map<std::string, std::string> mHeader;
  site_rdmp_names.clear();
  site_rdmp_dates.clear();
  std::cout << "Querying redump.org ..." << std::endl;

  // queries HTTP headers from redump.org/datfile/consolename
  while(std::getline(in, line)){
    if (!(line == "" || line.at(0) == '#') && line.substr(0,18) == "http://redump.org/"){ // ignore blank lines and lines that start with "#". only gets links that start with "http://redump.org/"
      mHeader.clear();

      // you can make a call to curl_easy_perform() and then only change the url through curl_easy_setopt() and make subsequent calls
      curl_easy_setopt(curl, CURLOPT_URL, line.c_str());
      curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, OnReceiveData);
      curl_easy_setopt(curl, CURLOPT_HEADERDATA, &mHeader);
      curl_easy_setopt(curl, CURLOPT_NOBODY, true);
      curl_easy_perform(curl);

      std::string header;
      for (std::map<std::string, std::string>::const_iterator itt = mHeader.begin(); itt != mHeader.end(); itt++) {
        if (itt->first == "Content-Disposition"){
          header = itt->first + ": " + itt->second; // stores Content-Disposition in header
        }
      }

      // extract filename from Content-Disposition
      std::string filename;
      const std::string q1 { R"(filename=")" };
      if (const auto pos = header.find(q1); pos != std::string::npos) {
        const auto len = pos + q1.size();

        const std::string q2 { R"(")" };
        if (const auto pos = header.find(q2, len); pos != std::string::npos) {
          filename = header.substr(len, pos - len);
        }
      }

      site_rdmp_names.push_back(std::get<2>(getDatName(filename)));
      site_rdmp_dates.push_back(std::get<3>(getDatName(filename)));
    }
  }
  in.close();
  curl_easy_cleanup(curl);

  std::ifstream in2(www_path);
  profilexml_names.clear();
  profilexml_dates.clear();
  std::cout << "Querying profile.xmls ..." << std::endl;

  // downloads profile.xmls and gets DAT name and date
  while(std::getline(in2, line)){
    if (!(line == "" || line.at(0) == '#')){ // ignore blank lines and lines that start with "#"
      int result = system(("curl -JLO --output-dir \"" + tmp_path + "\" " + line).c_str()); // download from the link, save to tmp_path

      pugi::xml_document doc;
      pugi::xml_parse_result xmlresult = doc.load_file((tmp_path+"/profile.xml").c_str());
      pugi::xml_node root = doc.child("clrmamepro");

      for (pugi::xml_node datfile = root.child("datfile"); datfile != nullptr; datfile = datfile.next_sibling()) {
        profilexml_names.push_back(datfile.child_value("name"));

        std::string date = datfile.child_value("version");
        date.erase(std::remove(date.begin(), date.end(), '-'), date.end()); // remove '-' if present
        profilexml_dates.push_back(date);
      }

      filesys::remove(tmp_path+"/profile.xml");
    }
  }

  YAML::Node config = YAML::LoadFile(config_path);
  YAML::Node dats = config["dats"];
  std::cout << "." << std::endl;
  unroll3(dats, "│");

  // print number of dats outdated/up-to-date/unknown
  std::cout << std::endl;
  std::cout << termcolor::cyan << "No. of DATs outdated:   " << termcolor::red << dat_outdated << std::endl;
  std::cout << termcolor::cyan << "No. of DATs up-to-date: " << termcolor::green << dat_up_to_date << std::endl;
  std::cout << termcolor::cyan << "No. of DATs unknown:    " << termcolor::color<169,169,169> << dat_unknown << std::endl;
}