#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstring>

#include "../libs/termcolor.hpp"
#include "../libs/docopt/docopt.h"

#include <yaml-cpp/yaml.h>

#include <paths.h>
#include <dir2dat.h>
#include <interface.h>
#include <cache.h>
#include <scanner.h>
#include <rebuilder.h>
#include <fixdat.h>

namespace filesys = std::filesystem;

char username[32]; // max username length = 32

std::string config_path;
std::string links_path;
std::string sort_xsl_path;
std::string www_path;
std::string backup_path;
std::string cache_path;
std::string dats_path;
std::string dats_new_path;
std::string fix_path;
std::string rebuild_path;
std::string tmp_path;

static const char USAGE[] =
R"(romorganizer by xprism

    Usage:
      romog (-h | --help)
      romog (-v | --version)
      romog (-i | --init)
      romog (-d | --dir2dat) [ns | --nosort] <folder-path> <dat-path>
      romog (-g | --genconfig) [-a | --auto <dat-group> <base-path>]
      romog (-l | --list) [u]
      romog (-s | --scan) <profile-no> ...
      romog (-r | --rebuild) [nr | --noremove] <profile-no> ...
      romog (-G | --genfixdat) <profile-no> ...
      romog (-L | --list-roms) [-C | --crc32] [-M | --md5] [-S | --sha1] [-p | --passed] [-m | --missing] <profile-no> ...
      romog (-b | --batch-scan) [r] <dat-group>
      romog (-u | --update-dats) [d]
      romog (-D | --delete) [-e | --entry] <profile-no> ...

    Options:
      -h --help             Show this screen.
      -v --version          Show version.
      -i --init             Initializes config file's paths.
      -d --dir2dat          Creates a DAT file from a directory. If the specified DAT path is an existing file, it will append to it.
      ns --nosort           Disables sorting of the resultant file.
      -g --genconfig        Append new DATs to the config file (if any).
      -a --auto             Automatically insert folder paths for a DAT group. Note that this will remove all existing folder paths for said DAT group.
      -l --list             Lists DAT files with their profile number and set count.
      u                     Replace set count with latest DAT version from the DAT group's site.
      -s --scan             Scans romset(s).
      -r --rebuild          Rebuilds roms to romset(s).
      nr --noremove         Disables removal of files in rebuild path that match DAT.
      -G --genfixdat        Generates a fixDAT file based on the "Missing" entries in cache(s).
      -L --list-roms        Lists set names and rom names for a profile.
      -C --crc32            Shows CRC32 of roms.
      -M --md5              Shows MD5 of roms.
      -S --sha1             Shows SHA1 of roms.
      -p --passed           Only show roms with "Passed" in cache.
      -m --missing          Only show roms with "Missing" in cache.
      -b --batch-scan       Batch scans roms of a DAT group.
      r                     Runs the rebuilder after every set scanned.
      -u --update-dats      Updates DATs in DAT folder.
      d                     Downloads new DATs from the links text file.
      -D --delete           Deletes cache(s).
      -e --entry            Deletes romset(s), DAT(s) and entry(s) in config file.

)";

/*
 * Handles command line arguments passed.
 * 
 * Arguments:
 *     argc : Number of command line arguments passed by user (includes name of program)
 *     argv : Array of arguments
 * 
 * Notes:
 *     Uses library: https://github.com/docopt/docopt.cpp
*/
int main(int argc, char* argv[]){
  std::map<std::string, docopt::value> args = docopt::docopt(USAGE, {argv+1, argv+argc}, true, "romorganizer v1.0\nxprism 2020-2021");  // true: show help if requested, followed by version string

  cuserid(username); // getting username for config path
  if(!(filesys::is_directory("/home/" + std::string(username) + "/.config/romog/"))){
    filesys::create_directory("/home/" + std::string(username) + "/.config/romog/"); // create .config/romog/ if not present
  }

  config_path = "/home/" + std::string(username) + "/.config/romog/config.yaml"; // config file

  if((!(filesys::exists(config_path))) || args["--init"].asBool()){ // initializes config file with paths (YAML format)
    YAML::Node config;
    YAML::Node paths = config["paths"];
    paths["links"] = "Insert path here";
    paths["sort_xsl"] = "Insert path here";
    paths["www"] = "Insert path here";
    paths["backup"] = "Insert path here";
    paths["cache"] = "Insert path here";
    paths["dats"] = "Insert path here";
    paths["dats_new"] = "Insert path here";
    paths["fix"] = "Insert path here";
    paths["rebuild"] = "Insert path here";
    paths["tmp"] = "Insert path here";

    std::ofstream output(config_path);
    output << config; // save to config file
    output.close();
    std::cout << "Generated config at " << config_path << std::endl;
    std::cout << "Please replace \"Insert path here\" with the appropriate paths before continuing to use romog." << std::endl;
  } else {
    YAML::Node config = YAML::LoadFile(config_path);
    YAML::Node paths = config["paths"];
    int count = 0;

    for(auto it = paths.begin(); it != paths.end(); ++it){ // iterating through "paths:"
      std::string key = it->first.as<std::string>();
      std::string value = it->second.as<std::string>();
      count += 1;

      if(value == "Insert path here"){
        std::cout << "Please replace \"Insert path here\" with the appropriate paths." << std::endl;
        exit(0);
      } else if (!(filesys::exists(value))){
        std::cout << value << " does not exist, please create that file/directory." << std::endl;
        exit(0);
      } else { // get paths
        if(count > 3){ // count > 3 means we are now at the paths to directories
          if(value.back() != '/'){ // if last character is not a forwardslash
            value.push_back('/'); // add it
          }
        }

        switch (count) {
          case 1:
            links_path = value;
            break;
          case 2:
            sort_xsl_path = value;
            break;
          case 3:
            www_path = value;
            break;
          case 4:
            backup_path = value;
            break;
          case 5:
            cache_path = value;
            break;
          case 6:
            dats_path = value;
            break;
          case 7:
            dats_new_path = value;
            break;
          case 8:
            fix_path = value;
            break;
          case 9:
            rebuild_path = value;
            break;
          case 10:
            tmp_path = value;
            break;
        }
      }
    }
  }

  if(args["--dir2dat"].asBool()){
    if(args["ns"].asBool() || args["--nosort"].asBool()){
      dir2dat(args["<folder-path>"].asString(), args["<dat-path>"].asString(), false);
    } else {
      dir2dat(args["<folder-path>"].asString(), args["<dat-path>"].asString(), true);
    }
  } else if (args["--genconfig"].asBool()){
    if(args["--auto"].asBool()){
      std::string base_path = args["<base-path>"].asString();
      if(base_path.back() == '/'){ // if last character is a forwardslash
        base_path.pop_back(); // remove it
      }

      std::tuple<std::string, std::string> info = std::make_tuple(args["<dat-group>"].asString(),base_path);
      genConfig(info);
    } else {
      genConfig();
    }
  } else if (args["--list"].asBool()){
    if(args["u"].asBool()){
      listProfilesWithDate();
    } else {
      listProfiles();
    }
  } else if (args["--scan"].asBool()){
    std::vector<std::string> profile_nos = args["<profile-no>"].asStringList();
    for(auto i: profile_nos){
      std::tuple<std::string, std::string> paths = getPaths(i);
      std::string folder_path = std::get<1>(paths);
      if(folder_path.back() != '/'){ // if last character is not a forwardslash
        folder_path.push_back('/'); // add it
      }

      scan(std::get<0>(paths),folder_path);
    }
  } else if (args["--rebuild"].asBool()){
    std::vector<std::string> profile_nos = args["<profile-no>"].asStringList();
    bool toRemove = true;
    if(args["nr"].asBool() || args["--noremove"].asBool()){
      toRemove = false;
    }

    for(auto i: profile_nos){
      std::tuple<std::string, std::string> paths = getPaths(i);
      std::string folder_path = std::get<1>(paths);
      if(folder_path.back() != '/'){ // if last character is not a forwardslash
        folder_path.push_back('/'); // add it
      }
    
      if(toRemove){
        rebuild(std::get<0>(paths),folder_path, true);
      } else {
        rebuild(std::get<0>(paths),folder_path, false);
      }
    }
  } else if (args["--genfixdat"].asBool()){
    std::vector<std::string> profile_nos = args["<profile-no>"].asStringList();
    for(auto i: profile_nos){
      std::tuple<std::string, std::string> paths = getPaths(i);
      genFixDat(std::get<0>(paths));
    }
  } else if (args["--list-roms"].asBool() && !((args["--passed"]).asBool()) && !((args["--missing"]).asBool())){ // --list-roms
    std::vector<std::string> profile_nos = args["<profile-no>"].asStringList();
    for(auto i: profile_nos){
      std::tuple<std::string, std::string> paths = getPaths(i);

      if(args["--crc32"].asBool() && args["--md5"].asBool() && args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"cms");
      } else if (args["--crc32"].asBool() && args["--md5"].asBool()){
        showInfo(std::get<0>(paths),"cm");
      } else if (args["--crc32"].asBool() && args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"cs");
      } else if (args["--md5"].asBool() && args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"ms");
      } else if (args["--crc32"].asBool()){
        showInfo(std::get<0>(paths),"c");
      } else if (args["--md5"].asBool()){
        showInfo(std::get<0>(paths),"m");
      } else if (args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"s");
      } else {
        showInfo(std::get<0>(paths));
      }
    }
  } else if (args["--list-roms"].asBool() && (args["--passed"]).asBool() && !((args["--missing"]).asBool())){ // --list-roms --passed
    std::vector<std::string> profile_nos = args["<profile-no>"].asStringList();
    for(auto i: profile_nos){
      std::tuple<std::string, std::string> paths = getPaths(i);

      if(args["--crc32"].asBool() && args["--md5"].asBool() && args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"cms","p");
      } else if (args["--crc32"].asBool() && args["--md5"].asBool()){
        showInfo(std::get<0>(paths),"cm","p");
      } else if (args["--crc32"].asBool() && args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"cs","p");
      } else if (args["--md5"].asBool() && args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"ms","p");
      } else if (args["--crc32"].asBool()){
        showInfo(std::get<0>(paths),"c","p");
      } else if (args["--md5"].asBool()){
        showInfo(std::get<0>(paths),"m","p");
      } else if (args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"s","p");
      } else {
        showInfo(std::get<0>(paths),"not_set","p");
      }
    }
  } else if (args["--list-roms"].asBool() && !((args["--passed"]).asBool()) && (args["--missing"]).asBool()){ // --list-roms --missing
    std::vector<std::string> profile_nos = args["<profile-no>"].asStringList();
    for(auto i: profile_nos){
      std::tuple<std::string, std::string> paths = getPaths(i);

      if(args["--crc32"].asBool() && args["--md5"].asBool() && args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"cms","m");
      } else if (args["--crc32"].asBool() && args["--md5"].asBool()){
        showInfo(std::get<0>(paths),"cm","m");
      } else if (args["--crc32"].asBool() && args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"cs","m");
      } else if (args["--md5"].asBool() && args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"ms","m");
      } else if (args["--crc32"].asBool()){
        showInfo(std::get<0>(paths),"c","m");
      } else if (args["--md5"].asBool()){
        showInfo(std::get<0>(paths),"m","m");
      } else if (args["--sha1"].asBool()){
        showInfo(std::get<0>(paths),"s","m");
      } else {
        showInfo(std::get<0>(paths),"not_set","m");
      }
    }
  } else if (args["--batch-scan"].asBool()){
    if(argc == 3){
      batchScan(args["<dat-group>"].asString());
    } else if (args["r"].asBool() && argc == 4) {
      batchScan(args["<dat-group>"].asString(), true);
    }
  } else if (args["--delete"].asBool()){
    std::vector<std::string> profile_nos = args["<profile-no>"].asStringList();
    bool toRemoveEntry = false;
    if(args["--entry"].asBool()){
      toRemoveEntry = true;
    }

    for(auto i: profile_nos){
      std::tuple<std::string, std::string> paths = getPaths(i);
      if(toRemoveEntry){
        deleteProfile(std::get<0>(paths), true);
      } else {
        deleteProfile(std::get<0>(paths), false);
      }
    } 
  } else if (args["--update-dats"].asBool()){
    if(args["d"].asBool()){
      updateDats(true);
    } else {
      updateDats(false);
    }
  }

  return 0;
}