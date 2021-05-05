#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>

#include <pugixml.hpp>

#include <dat.h>

/*
 * Handle data in DAT that has to be manually fixed
 *
 * Arguments:
 *     rom_name : Rom name in DAT
 * 
 * Returns:
 *     rom_name : Fixed rom name
 */
std::string fixName(std::string rom_name){
  std::replace(rom_name.begin(), rom_name.end(), '\\', '/'); // handle rom name with path; replace all occurences of "\" to "/"
  return rom_name;
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
      rom_name = fixName(rom_name);

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
 * Checks whether CRC32/SHA1 is in DAT
 *
 * Arguments:
 *     dat_path : Path to DAT file
 *     hash : CRC32/SHA1
 *     hash_type : "1" if hash is a CRC32, "2" if hash is a SHA1
 * 
 * Returns:
 *     in_dat : True if hash is in DAT, false if not
 */
bool hashInDAT(std::string dat_path, std::string hash, std::string hash_type){
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(dat_path.c_str());
  pugi::xml_node root = doc.child("datafile");

  bool in_dat;

  for(pugi::xml_node game = root.child("game"); game != nullptr; game = game.next_sibling()) {
    for(pugi::xml_node rom = game.child("rom"); rom != nullptr; rom = rom.next_sibling()){
      if(hash_type == "1" && rom.attribute("crc").value() == hash){
        in_dat = true;
        goto stop;
      } else if (hash_type == "2" && rom.attribute("sha1").value() == hash){
        in_dat = true;
        goto stop;
      }
    }
  }

  stop:
    return in_dat;
}

/*
 * Gets first set name and rom name corresponding to the particular CRC32/SHA1 in DAT
 *
 * Arguments:
 *     dat_path : Path to DAT file
 *     hash : CRC32/SHA1
 *     hash_type : "1" if hash is a CRC32, "2" if hash is a SHA1
 * 
 * Returns:
 *     names : Tuple containing set name and rom name (in that order)
 */
std::tuple<std::string, std::string> getNameFromHash(std::string dat_path, std::string hash, std::string hash_type){
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(dat_path.c_str());
  pugi::xml_node root = doc.child("datafile");

  std::tuple<std::string, std::string> names;

  for(pugi::xml_node game = root.child("game"); game != nullptr; game = game.next_sibling()) {
    for(pugi::xml_node rom = game.child("rom"); rom != nullptr; rom = rom.next_sibling()){
      if(hash_type == "1" && rom.attribute("crc").value() == hash){
        std::string rom_name = rom.attribute("name").value();
        rom_name = fixName(rom_name);
        names = std::make_tuple(game.attribute("name").value(), rom_name);
        goto stop;
      } else if (hash_type == "2" && rom.attribute("sha1").value() == hash){
        std::string rom_name = rom.attribute("name").value();
        rom_name = fixName(rom_name);
        names = std::make_tuple(game.attribute("name").value(), rom_name);
        goto stop;
      }
    }
  }

  stop:
    return names;
}

/*
 * Gets CRC32/MD5/SHA1/size corresponding to the particular set name and rom name in DAT
 *
 * Arguments:
 *     dat_path : Path to DAT file
 *     names : Tuple containing set name and rom name (in that order)
 * 
 * Returns:
 *     hashes : Tuple containing CRC32, MD5, SHA1, size (in that order)
 */
std::tuple<std::string, std::string, std::string, std::string> getHashFromName(std::string dat_path, std::tuple<std::string, std::string> names){
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(dat_path.c_str());
  pugi::xml_node root = doc.child("datafile");

  std::tuple<std::string, std::string, std::string, std::string> hashes;

  for(pugi::xml_node game = root.child("game"); game != nullptr; game = game.next_sibling()) {
    for(pugi::xml_node rom = game.child("rom"); rom != nullptr; rom = rom.next_sibling()){
      std::string rom_name = rom.attribute("name").value();
      rom_name = fixName(rom_name);
      if(game.attribute("name").value() == std::get<0>(names) && rom_name == std::get<1>(names)){
        hashes = std::make_tuple(rom.attribute("crc").value(), rom.attribute("md5").value(), rom.attribute("sha1").value(), rom.attribute("size").value());
        goto stop;
      }
    }
  }

  stop:
    return hashes;
}