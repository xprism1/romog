#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <filesystem>
#include <regex>

#include <pugixml.hpp>
#include <xalanc/Include/PlatformDefinitions.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xalanc/XalanTransformer/XalanTransformer.hpp>

#include <paths.h>
#include <dir2dat.h>
#include <gethashes.h>

namespace filesys = std::filesystem;

using xercesc_3_2::XMLPlatformUtils;
using xalanc_1_11::XalanTransformer;

/*
 * Gets a list of all files in given directory and its subdirectories.
 *
 * Arguments:
 *     dirPath : Path of directory
 *
 * Returns:
 *     listofFiles : Vector containing paths of all the files in given directory and its sub directories
 *
 * Notes:
 *     Taken from https://thispointer.com/c-get-the-list-of-all-files-in-a-given-directory-and-its-sub-directories-using-boost-c17/, but I added if(!(filesys::is_directory(iter->path().string()))){}
 */
std::vector<std::string> getAllFilesInDir(const std::string &dirPath) {
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
        if(!(filesys::is_directory(iter->path().string()))){ // prevents path to directories from getting added to listOfFiles
          listOfFiles.push_back(iter->path().string()); // Add the name in vector
        }

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
 * Gets filename and filename without extension from a path
 *
 * Arguments:
 *     path : Path to a file
 *
 * Returns:
 *     info : Tuple containing filename, and filename without extension (in that order)
 */
std::tuple<std::string, std::string> getFileName(std::string path){
  std::string fname = path.substr(path.find_last_of("/\\") + 1); // fname: filename with extension
  std::string::size_type const p(fname.find_last_of('.'));
  std::string fname_noe = fname.substr(0, p); // fname_noe: filename without extension

  std::tuple<std::string, std::string> info = std::make_tuple(fname, fname_noe);
  return info;
}

/*
 * Gets directory name from a path
 *
 * Arguments:
 *     path : Path to a directory
 *
 * Returns:
 *     dname : Directory name
 */
std::string getDirName(std::string path){
  if(path.back() == '/'){ // if last character is a forwardslash
    path.pop_back(); // remove it
  }

  std::string dname = path.substr(path.find_last_of("/\\") + 1); // dname: folder name of directory

  return dname;
}

/*
 * Gets DAT name, DAT name without extension, DAT name without extension and date, and DAT date from a path
 *
 * Arguments:
 *     path : Path to a DAT file
 *
 * Returns:
 *     info : Tuple containing DAT name, DAT name without extension, DAT name without extension and date, and DAT date (in that order)
 */
std::tuple<std::string, std::string, std::string, std::string> getDatName(std::string path){
  std::string fname = path.substr(path.find_last_of("/\\") + 1); // fname: filename with extension
  std::string::size_type const p(fname.find_last_of('.'));
  std::string fname_noe = fname.substr(0, p); // fname_noe: filename without extension

  std::string::size_type const q(fname.find_last_of('('));
  std::string fname_noed = fname.substr(0, q); // fname_noed: filename without extension and without date
  std::string fname2 = fname; // copy fname to fname2
  fname2.erase(fname2.find(fname_noed),fname_noed.size()); // remove fname_noed from fname2

  if(fname_noed.back() == ' '){ // if last character is a space
    fname_noed.pop_back(); // remove it
  }

  if(fname_noed.find('(') != std::string::npos){ // if fname_noed still has '(' in it (e.g. for a Redump DAT, at this stage, fname_noed would be Microsoft - Xbox 360 - Datfile (2222) or No-Intro DAT with ROM format like such: Atari - Jaguar (J64))
    std::string brackets = fname_noed.substr(fname_noed.find_first_of('('), fname_noed.find_first_of(')')); // get the portion in the brackets
    brackets = brackets.substr(1,brackets.size()-2); // remove first and last character of portion ('(' and ')')
    if(brackets.find_first_not_of("0123456789") == std::string::npos){ // if that portion consists of only digits
      fname_noed = fname_noed.substr(0,fname_noed.find_first_of('(')-1); // we remove the brackets from fname_noed
    }
  }

  std::string date = std::regex_replace(fname2, std::regex(R"([\D])"), ""); // R"([\D])" = select non-numeric characters, and remove them from fname2

  std::tuple<std::string, std::string, std::string, std::string> info = std::make_tuple(fname, fname_noe, fname_noed, date);
  return info;
}

/*
 * Formats date from YYYYMMDDHHMMSS to YYYYMMDD-HHMMSS
 *
 * Arguments:
 *     date : Date in YYYYMMDDHHMMSS/YYYYMMDD
 *
 * Returns:
 *     date_f : Formated date (YYYYMMDD-HHMMSS)
 */
std::string formatDate(std::string date){
  std::string date_f; // date_f: date formatted to YYYYMMDD/YYYYMMDD-HHMMSS
  if(date.size() == 8){ // date only has YYYYMMDD
    date_f = date;
  } else {
    date_f = date.substr(0,8) + "-" + date.substr(8,14);
  }
  return date_f;
}

/*
 * Sorts DAT by <game>
 *
 * Arguments:
 *     dat_path : Path to DAT
 *
 * Returns:
 *     parent_path : Parent path of DAT (for fixSortedDAT() to use)
 */
std::string sortDat(std::string dat_path){
  filesys::path path = dat_path;
  std::string parent_path = path.parent_path(); // gets parent path of dat_path (i.e. dat_path without filename)
  std::string sorted_dat_path;

  if(parent_path.length() == 0){ // length of parent_path == 0 means dat_path is just the filename
    sorted_dat_path = "sorted_.dat";
  } else {
    sorted_dat_path = parent_path + "/sorted_.dat";
  }
  
  XMLPlatformUtils::Initialize(); // initialises xerces
  XalanTransformer::initialize(); // initialises xalan

  {
  XalanTransformer theXalanTransformer; // creates a XalanTransformer

  // does the transformation
  xalanc_1_11::XSLTInputSource xmlIn(dat_path.c_str());
  xalanc_1_11::XSLTInputSource xslIn(sort_xsl_path.c_str());
  xalanc_1_11::XSLTResultTarget xmlOut(sorted_dat_path.c_str());
  int theResult = theXalanTransformer.transform(xmlIn,xslIn,xmlOut);

  if(theResult != 0){
    std::cout << "DAT sorting failed!" << std::endl;
  }
  }

  XalanTransformer::terminate(); // shut down xalan
  XMLPlatformUtils::Terminate(); // shut down xerces
  XalanTransformer::ICUCleanUp();

  return parent_path; // returns parent_path for fixSortedDat() to use
}

/*
 * Fixes minor identation issues with sorted DAT
 *
 * Arguments:
 *     parent_path : Parent path of DAT
 * 
 * Returns:
 *     output_dat_path : Path to the sorted and fixed DAT
 */
std::string fixSortedDat(std::string parent_path){
  std::string sorted_dat_path;
  std::string output_dat_path;
  if(parent_path.length() == 0){ // length of parent_path == 0 means sorted_dat_path is just the filename
    sorted_dat_path = "sorted_.dat";
    output_dat_path = "sorted.dat";
  } else {
    sorted_dat_path = parent_path + "/sorted_.dat";
    output_dat_path = parent_path + "/sorted.dat";
  }

  std::ifstream file_in(sorted_dat_path);
  std::ofstream file_out(output_dat_path); // filename of the finalised sorted DAT
  std::string str;
  int line_count = 0;
  while (std::getline(file_in, str)){ // str = each line in input file
    line_count++; // increment line counter for each line read from file

    if (line_count == 1) {
      // writes the standard datfile header
      file_out << "<?xml version=\"1.0\"?>" << std::endl;
      file_out << "<!DOCTYPE datafile PUBLIC \"-//Logiqx//DTD ROM Management Datafile//EN\" \"http://www.logiqx.com/Dats/datafile.dtd\">" << std::endl;
    } else if (str.find("</datafile>") != std::string::npos) { // check if </datafile> exists in line
      str.erase(std::remove(str.begin(), str.end(), '\t'), str.end()); // if so, remove indent from that line
      file_out << str << std::endl;
    } else if (line_count == 3 || str.substr(0,11) == "<game name="){ // check if line_count == 3 (aka the line with <header>). also checks if first 11 characters of that line is "<game name="
      file_out << "\t" << str << std::endl; // if so, indent that line
    } else {
      file_out << str << std::endl; // write the line without modification to output file
    }
  }
  file_in.close();
  file_out.close();
  filesys::remove(sorted_dat_path); // removes file made by sortDat()

  return output_dat_path;
}

/*
 * Calculates hashes of files and adds them into a DAT. Creates a DAT file if the specified DAT path is a non-existent file. Sorts the DAT if wanted.
 * 
 * How entries in DAT are made:
 * folder_path
 * ├─ a.rom       -> 1 entry with set name = a, rom name = a.rom
 * └─ test        -> 1 entry with set name = test
 *    ├─ b.rom    -> sub-entry: rom name = b.rom
 *    └─ test2
 *       └─ c.rom -> sub-entry: rom name = test2\\c.rom
 * 
 * Description is set to set name.
 * 
 * Arguments:
 *     folder_path : Path to folder containing files
 *     dat_path : Path to DAT
 *     toSort : true to sort the DAT file, false to not sort the DAT
 * 
 * Notes:
 *     Taken from https://stackoverflow.com/a/37494654
 *     pugixml will automatically handle HTML entities so no encoding is required
 */
void dir2dat(std::string folder_path, std::string dat_path, bool toSort){
  // checks
  if(!(filesys::is_directory(folder_path))){
    std::cout << folder_path << " is not a directory!" << std::endl;
    exit(0);
  }
  
  pugi::xml_document doc;
  pugi::xml_node root;
  pugi::xml_parse_result result;

  bool dat_exists = true;
  if(!(filesys::exists(dat_path))){ // if dat_path does not exist, we create a new DAT file
    dat_exists = false;

    // creating empty DAT

    // adding XML declaration
    pugi::xml_node declarationNode = doc.append_child(pugi::node_declaration);
    declarationNode.append_attribute("version") = "1.0";

    // adding header
    root = doc.append_child("datafile");
    pugi::xml_node header = root.append_child("header");
    pugi::xml_node header_name = header.append_child("name");
    pugi::xml_node header_desc = header.append_child("description");
    pugi::xml_node header_version = header.append_child("version");
    pugi::xml_node header_author = header.append_child("author");
    pugi::xml_node header_homepage = header.append_child("homepage");
    pugi::xml_node header_url = header.append_child("url");

    header_name.append_child(pugi::node_pcdata).set_value(getDirName(folder_path).c_str()); // folder name of folder_path
    header_desc.append_child(pugi::node_pcdata).set_value(getDirName(folder_path).c_str()); // folder name of folder_path

    // getting YYYYMMDD-HHMMSS (No-Intro style time)
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];

    time (&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer,80,"%Y%m%d-%H%M%S",timeinfo);
    std::string date_and_time(buffer);

    header_version.append_child(pugi::node_pcdata).set_value(date_and_time.c_str());

    // getting username
    char username[32]; // max username length = 32
    cuserid(username);

    header_author.append_child(pugi::node_pcdata).set_value(username);
    header_homepage.append_child(pugi::node_pcdata).set_value("Unknown");
    header_url.append_child(pugi::node_pcdata).set_value("Unknown");
  } else {
    // load DAT file
    result = doc.load_file(dat_path.c_str());
    root = doc.child("datafile");
  }
  
  for(const auto & entry : filesys::directory_iterator(folder_path)){ // only iterating through top level files in folder_path
    std::string item_path = entry.path(); // can be directory or file

    if(filesys::is_directory(item_path)){
      std::vector<std::string> files_in_folder = getAllFilesInDir(item_path);

      // add entry with set name to DAT
      std::string set_name = getDirName(item_path); // set name: name of directory
      pugi::xml_node game = root.append_child("game");
      game.prepend_attribute("name") = set_name.c_str();
      pugi::xml_node game_desc = game.append_child("description");
      game_desc.append_child(pugi::node_pcdata).set_value(set_name.c_str()); // description = set name

      for(auto i: files_in_folder){
        // get rom name
        std::string rom_name = i.substr(item_path.length()+1); // e.g. if item_path = "/path/to/folder", i = "/path/to/folder/abc.zip" or "/path/to/folder/test/def.zip", rom name will be = "abc.zip" or "test/def.zip" respectively (+1 because item_path won't end with forwardslash and we need to remove that from i)

        // get rom info
        std::vector<std::string> rom_info = getHashes(i);
        
        // inserting rom info into DAT
        std::replace(rom_name.begin(), rom_name.end(), '/', '\\'); // handle rom name with path; replace all occurences of "/" to "\" (for compatibility with existing rom managers)

        pugi::xml_node rom = game.append_child("rom");
        rom.append_attribute("name") = rom_name.c_str();
        rom.append_attribute("size") = rom_info[0].c_str();
        rom.append_attribute("crc") = rom_info[1].c_str();
        rom.append_attribute("md5") = rom_info[2].c_str();
        rom.append_attribute("sha1") = rom_info[3].c_str();
      }
    } else {
      // get set name and rom name
      std::string rom_name = std::get<0>(getFileName(item_path)); // rom_name: filename with extension
      std::string set_name = std::get<1>(getFileName(item_path)); // set_name: filename without extension

      // get rom info
      std::vector<std::string> rom_info = getHashes(item_path);
      
      // add entry with set name to DAT
      pugi::xml_node game = root.append_child("game");
      game.prepend_attribute("name") = set_name.c_str();

      // inserting rom info into DAT
      pugi::xml_node game_desc = game.append_child("description");
      game_desc.append_child(pugi::node_pcdata).set_value(set_name.c_str()); // description = set name

      pugi::xml_node rom = game.append_child("rom");
      rom.append_attribute("name") = rom_name.c_str();
      rom.append_attribute("size") = rom_info[0].c_str();
      rom.append_attribute("crc") = rom_info[1].c_str();
      rom.append_attribute("md5") = rom_info[2].c_str();
      rom.append_attribute("sha1") = rom_info[3].c_str();
    }
  }

  doc.save_file(dat_path.c_str()); // overwrite existing DAT

  if(toSort){
    std::string parent_path = sortDat(dat_path);
    std::string output_path = fixSortedDat(parent_path);
    filesys::remove(dat_path);
    filesys::rename(output_path, dat_path); // rename output of fixSortedDAT() to dat_path
  }

  if(dat_exists){
    std::cout << "Added to " << dat_path << std::endl;
  } else {
    std::cout << "Created " << dat_path << std::endl;
  }
}