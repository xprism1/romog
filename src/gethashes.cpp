#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <sys/stat.h>

#include <openssl/md5.h>
#include <openssl/sha.h>

#include <gethashes.h>

namespace filesys = std::filesystem;

/*
 * Calculates CRC32, MD5 and SHA1 of a file, optionally skipping first X bytes before calculating hashes if certain criteria are met.
 *
 * Arguments:
 *     path : Path to a file
 *     start_offset (Optional) : Offset to start calculating hash from (in decimal)
 *     data (Optional) : Vector of tuples containing offset (in decimal) and their expected values (in lowercase); if all values at offsets of file matches expected values, hash is calculated from start_offset to the end of the file. If not, hash is calculated over the entire file.
 *
 * Returns:
 *     output : Vector containing file size, CRC32, MD5, SHA1 of the file (in that order)
 *
 * Notes:
 *     CRC32 function taken from: https://blog.csdn.net/xiaobin_HLJ80/article/details/19500207
 *     MD5 function taken from: https://stackoverflow.com/a/42958050
 *     SHA1 function is a slight modification of the MD5 function.
 *     Getting hex from file: Partially taken from https://stackoverflow.com/questions/29238697/char-hex-because-it-shows-ffffff#comment46683759_29238733
 *
 * E.g. for Atari 7800:
 * std::vector<std::tuple<int, std::string>> data;
 * data.push_back(std::make_tuple(1,"415441524937383030"));
 * data.push_back(std::make_tuple(96,"0000000041435455414c20434152542044415441205354415254532048455245"));
 * std::vector<std::string> output = getHashes("Asteroids (USA).a78",128,data);
 */
std::vector<std::string> getHashes(std::string path, int start_offset, std::vector<std::tuple<int, std::string>> data) {
  // checks
  if(!(filesys::exists(path))){
    std::cout << path << " does not exist!" << std::endl;
    exit(0);
  }

  std::ifstream file(path, std::ifstream::binary);

  // get file size
  std::uintmax_t filesize = filesys::file_size(path);

  // generate CRC32 table
  unsigned int crc32table[256];
  unsigned int crc;
  for(int i = 0; i < 256; i++) {
    crc = i;
    for(int j = 0; j < 8; j++) {
      if((crc & 1) == 1) {
          crc = (crc >> 1) ^ 0xEDB88320;
      } else {
          crc >>= 1;
      }
    }
    crc32table[i] = crc;
  }

  // call MD5_Init/SHA1_Init once
  MD5_CTX md5Context;
  MD5_Init(&md5Context);
  SHA_CTX sha1Context;
  SHA1_Init(&sha1Context);

  char buf[1024 * 16]; // create buffer
  char b[1024*16-start_offset]; // buf without header
  int num_bytes_read = 0; // number of bytes read
  std::vector<bool> checked; // vector of bools of whether each <data> matches
  bool skipping_header_on_this_run = false; // whether we are skipping header on this chunk of buffer
  bool skipped_header = false; // whether the header has been skipped

  unsigned int crc32 = 0xFFFFFFFF;
  while (file.good()) {
    file.read(buf, sizeof(buf)); // read file in chunks into buffer

    if(start_offset == -1 && data.size() == 0) { // not skipping header
      skipping_header_on_this_run = false;
    } else {
      num_bytes_read += file.gcount();
      for(int i = 0; i < data.size(); i++){
        if(i == 0 && checked.size() == 0 || checked.size() == i && num_bytes_read >= std::get<0>(data[i])){ // if (its the first time we're running the code or checked[i] dosen't exist) and number of bytes read >= data offset
          std::string to_check = "";
          for(int j = std::get<0>(data[i]); j < std::get<0>(data[i]) + std::get<1>(data[i]).size()/2; j++){ // e.g. std::get<0>(data[i]) = 1, std::get<1>(data[i]) = "4e45" ⇒ we need to get bytes from offsets 1,2,3,4 ⇒ for(int j = 1, j < 5; j++) ⇒ 1 (std::get<0>(data[i])) + 4 (std::get<1>(data[i]).size()/2) = 5
            std::stringstream ss;
            ss << std::hex << std::setfill('0') << std::setw(2) << (unsigned int)(unsigned char)buf[j];
            to_check += ss.str();
          }

          if(std::get<1>(data[i]) == to_check){ // if <data> matches
            checked.push_back(true);
          } else {
            checked.push_back(false);
          }
        }
      }

      if(num_bytes_read >= start_offset && !(skipped_header)){
        if (!(std::find(checked.begin(), checked.end(), false) != checked.end())){ // if "false" does not exist in checked (i.e. all of the rules are fufilled)
          std::copy(buf + start_offset, std::end(buf), b); // copy (buf+start_offset to end of buf) to b
          skipping_header_on_this_run = true;
        }
      }
    }

    // calculate crc32
    if(skipping_header_on_this_run){
      for(int i = 0; i < file.gcount()-start_offset; i++) {
        crc32 = (crc32 >> 8) ^ crc32table[(crc32 ^ b[i]) & 0xFF];
      }
    } else {
      for(int i = 0; i < file.gcount(); i++) {
        crc32 = (crc32 >> 8) ^ crc32table[(crc32 ^ buf[i]) & 0xFF];
      }
    }

    // call MD5_Update/SHA1_Update with each chunk of data you read from the file
    if(skipping_header_on_this_run){
      MD5_Update(&md5Context, b, file.gcount()-start_offset);
      SHA1_Update(&sha1Context, b, file.gcount()-start_offset);
    } else {
      MD5_Update(&md5Context, buf, file.gcount());
      SHA1_Update(&sha1Context, buf, file.gcount());
    }

    if(skipping_header_on_this_run){
      skipped_header = true;
      skipping_header_on_this_run = false;
    }
  }

  if(skipped_header){
    filesize -= start_offset; // subtract skipped bytes from file size
  }

  // call MD5_Final/SHA1_Final once done to get the result
  unsigned char resultMD5[MD5_DIGEST_LENGTH];
  unsigned char resultSHA1[SHA_DIGEST_LENGTH];
  MD5_Final(resultMD5, &md5Context);
  SHA1_Final(resultSHA1, &sha1Context);

  file.close();

  // converting to string
  std::stringstream filesizestring;
  std::stringstream CRC32string;
  std::stringstream MD5string;
  std::stringstream SHA1string;

  filesizestring << filesize;

  CRC32string << std::hex << (crc32 ^ ~0U);
  std::string crc32sum = CRC32string.str();

  MD5string << std::hex << std::uppercase << std::setfill('0');
  for (const auto &byte: resultMD5) {
    MD5string << std::setw(2) << (int)byte;
  }
  std::string md5sum = MD5string.str();

  SHA1string << std::hex << std::uppercase << std::setfill('0');
  for (const auto &byte: resultSHA1) {
    SHA1string << std::setw(2) << (int)byte;
  }
  std::string sha1sum = SHA1string.str();

  // convert hashes to uppercase
  std::transform(crc32sum.begin(), crc32sum.end(), crc32sum.begin(), ::toupper);
  std::transform(md5sum.begin(), md5sum.end(), md5sum.begin(), ::toupper);
  std::transform(sha1sum.begin(), sha1sum.end(), sha1sum.begin(), ::toupper);

  // account for blank files in DATs
  if(filesizestring.str() == "0"){
    crc32sum = "";
    md5sum = "";
    sha1sum = "";
  }

  // account for CRC32 with <8 characters
  if(!(crc32sum == "0")){
    crc32sum.insert(0, 8 - crc32sum.size(), '0'); // add (8-crc32sum's length) '0's in front of crc32sum (because CRC32's in DAT are all 8 characters)
  }

  // output
  std::vector<std::string> output;
  output.push_back(filesizestring.str());
  output.push_back(crc32sum);
  output.push_back(md5sum);
  output.push_back(sha1sum);
  return output;
}