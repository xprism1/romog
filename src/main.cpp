#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstring>

#include "../libs/termcolor.hpp"
#include "../libs/docopt/docopt.h"

#include <dir2dat.h>
#include <interface.h>
#include <cache.h>
#include <scanner.h>
#include <rebuilder.h>
#include <fixdat.h>

namespace filesys = std::filesystem;

static const char USAGE[] =
R"(romorganizer by xprism

    Usage:
      romog (-h | --help)
      romog (-v | --version)
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
      -d --dir2dat          Creates a DAT file from a directory. If the specified DAT path is an existing file, it will append to it.
      ns --nosort           Disables sorting of the resultant file.
      -g --genconfig        Generates a config file for romorganizer. If the config file exists, it will append new DATs to it (if any).
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