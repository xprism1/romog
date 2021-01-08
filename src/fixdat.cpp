#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <set>
#include <ctime>
#include <filesystem>

#include <pugixml.hpp>

#include <paths.h>
#include <dir2dat.h>
#include <cache.h>
#include <fixdat.h>

namespace filesys = std::filesystem;

/*
 * Generates a fixDAT file from the "Missing" entries in cache
 *
 * Arguments:
 *     dat_path : Path to DAT file
 *     fixdat_path (Optional) : Path to the fixDAT file. If not passed, fixDAT will be saved in fix/ and filename will be set to "fixDAT_" + DAT filename
 * 
 * References:
 *     http://www.gerald-fahrnholz.eu/sw/DocGenerated/HowToUse/html/group___grp_pugi_xml.html
 *     https://stackoverflow.com/a/40101025
*/
void genFixDat(std::string dat_path, std::string fixdat_path){
  // checks
  if(!(filesys::exists(dat_path))){
    std::cout << dat_path << " does not exist!" << std::endl;
    exit(0);
  }

  if(fixdat_path != "not_set" && filesys::is_directory(fixdat_path)){
    std::cout << fixdat_path << " is a directory, it needs to contain the name of the output file as well." << std::endl;
    exit(0);
  }

  // making fixDAT path if it is not passed as a paremeter
  if(fixdat_path == "not_set"){
    std::string dat_filename = filesys::path(dat_path).filename();
    fixdat_path = fix_path + "fixDAT_" + dat_filename;
  } 

  // getting <header> info from DAT
  pugi::xml_document dat_doc;
  pugi::xml_parse_result result = dat_doc.load_file(dat_path.c_str());
  pugi::xml_node dat_root = dat_doc.child("datafile");
  pugi::xml_node dat_header = dat_root.child("header");
  std::string dat_header_name = dat_header.child_value("name");
  std::string dat_header_desc = dat_header.child_value("description");

  // creating empty DAT
  pugi::xml_document doc; // generate new XML document within memory

  // adding XML declaration
  pugi::xml_node declarationNode = doc.append_child(pugi::node_declaration);
  declarationNode.append_attribute("version") = "1.0";

  // adding header
  pugi::xml_node root = doc.append_child("datafile");
  pugi::xml_node header = root.append_child("header");
  pugi::xml_node header_name = header.append_child("name");
  pugi::xml_node header_desc = header.append_child("description");
  pugi::xml_node header_cat = header.append_child("category");
  pugi::xml_node header_version = header.append_child("version");
  pugi::xml_node header_date = header.append_child("date");
  pugi::xml_node header_author = header.append_child("author");
  header_name.append_child(pugi::node_pcdata).set_value(dat_header_name.c_str());
  header_desc.append_child(pugi::node_pcdata).set_value(dat_header_desc.c_str());
  header_cat.append_child(pugi::node_pcdata).set_value("FIXDATFILE");

  // getting MM-DD-YYYY HH:MM:SS
  time_t rawtime;
  struct tm * timeinfo;
  char buffer[80];

  time (&rawtime);
  timeinfo = localtime(&rawtime);

  strftime(buffer,80,"%m-%d-%Y %H:%M:%S",timeinfo);
  std::string date_and_time(buffer);

  header_version.append_child(pugi::node_pcdata).set_value(date_and_time.c_str());
  header_date.append_child(pugi::node_pcdata).set_value(date_and_time.erase(10,19).c_str()); // only date
  header_author.append_child(pugi::node_pcdata).set_value("romorganizer");

  // getting missing roms from cache
  cacheData cache_data = getDataFromCache(dat_path);
  datData dat_data = getDataFromDAT(dat_path);

  // adding in missing roms
  std::vector<int> added; // list of index of entries that are added to fixdat
  for(int i = 0; i < cache_data.rom_name.size(); i++){
    if(cache_data.status[i] == "Missing" && (!(std::find(added.begin(), added.end(), i) != added.end()))){ // if status of entry is "Missing" and its index is not in std::vector<std::int> added
      pugi::xml_node game = root.append_child("game");
      game.prepend_attribute("name") = cache_data.set_name[i].c_str();

      pugi::xml_node game_desc = game.append_child("description");
      game_desc.append_child(pugi::node_pcdata).set_value(cache_data.set_name[i].c_str());

      // getting index of roms with same set names as cache_data.set_name[i]
      std::vector<int> index; // list of index of entries with same set name
      for(int j = 0; j < cache_data.set_name.size(); j++){
        if(cache_data.set_name[j] == cache_data.set_name[i] && cache_data.status[j] == "Missing"){
          index.push_back(j);
          added.push_back(j);
        }
      }
      for(auto j: index){
        std::string md5;
        std::string sha1;
        std::string size;

        // getting size, MD5, SHA1 from DAT
        for(int k = 0; k < dat_data.set_name.size(); k++){
          if(dat_data.rom_name[k] == cache_data.rom_name[j]){
            md5 = dat_data.md5[k];
            sha1 = dat_data.sha1[k];
            size = dat_data.size[k];
            break;
          }
        }

        // adding to file
        pugi::xml_node rom = game.append_child("rom");
        rom.append_attribute("name") = cache_data.rom_name[j].c_str();
        rom.append_attribute("size") = size.c_str();
        rom.append_attribute("crc") = cache_data.crc32[j].c_str();
        rom.append_attribute("md5") = md5.c_str();
        rom.append_attribute("sha1") = sha1.c_str();
      }
    }
  }

  // save XML tree to file; second optional param is indent string to be used, default indentation is tab character
  doc.save_file(fixdat_path.c_str(), PUGIXML_TEXT("  "));

  // sort fixDAT
  std::string parent_path = sortDat(fixdat_path);
  std::string output_path = fixSortedDat(parent_path);

  filesys::remove(fixdat_path);
  filesys::rename(output_path, fixdat_path); // rename output of fixSortedDAT() to fixdat_path

  std::cout << "Created " << fixdat_path << std::endl;
}