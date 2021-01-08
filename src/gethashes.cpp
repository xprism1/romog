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
 * Calculates CRC32, MD5 and SHA1 of a file
 *
 * Arguments:
 *     path : Path to a file
 *
 * Returns:
 *     output : Vector containing file size, CRC32, MD5, SHA1 of the file (in that order)
 *
 * Notes:
 *     CRC32 function taken from: https://blog.csdn.net/xiaobin_HLJ80/article/details/19500207
 *     MD5 function taken from: https://stackoverflow.com/a/42958050
 *     SHA1 function is a slight modification of the MD5 function.
 */
std::vector<std::string> getHashes(std::string path) {
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
  unsigned int crc32 = 0xFFFFFFFF;
  while (file.good()) {
    file.read(buf, sizeof(buf)); // read file in chunks into buffer

    // calculate crc32
    for(int i = 0; i < file.gcount(); i++) {
      crc32 = (crc32 >> 8) ^ crc32table[(crc32 ^ buf[i]) & 0xFF];
    }

    // call MD5_Update/SHA1_Update with each chunk of data you read from the file
    MD5_Update(&md5Context, buf, file.gcount());
    SHA1_Update(&sha1Context, buf, file.gcount());
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

  // output
  std::vector<std::string> output;
  output.push_back(filesizestring.str());
  output.push_back(crc32sum);
  output.push_back(md5sum);
  output.push_back(sha1sum);
  return output;
}
