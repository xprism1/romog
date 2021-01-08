#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <vector>
#include <map>

#include "../libs/zip/src/zip.h"

#include <fcntl.h>
#include "/usr/include/archive.h"
#include <archive_entry.h>

/*
 * Gets info from a zip file
 *
 * Arguments:
 *     zip_path : Path to zip file
 * 
 * Returns:
 *     data : Map with key as file name and value as CRC32.
 * 
 * Notes:
 *     Uses library: https://github.com/kuba--/zip   
 */
std::map<std::string, std::string> getInfoFromZip(std::string zip_path){
  struct zip_t *zip = zip_open(zip_path.c_str(), 0, 'r');
  std::map<std::string, std::string> data;

  for (int i = 0; i < zip_total_entries(zip); i++) {
    zip_entry_openbyindex(zip, i);
    {
      const char *name = zip_entry_name(zip);
      std::string filename(name); // convert char* to string

      unsigned int crc32 = zip_entry_crc32(zip);
      unsigned int size = zip_entry_size(zip);
      std::stringstream CRC32string;
      CRC32string << std::hex << crc32; // convert int to hex
      std::string crc32sum = CRC32string.str();
      transform(crc32sum.begin(), crc32sum.end(), crc32sum.begin(), ::toupper); // converts to uppercase because hashes in DAT are uppercase
      if(!(std::to_string(size) == "0")){
        crc32sum.insert(0, 8 - crc32sum.size(), '0'); // add (8-crc32sum's length) '0's in front of crc32sum (because CRC32's in DAT are all 8 characters)
      } else {
        crc32sum = ""; // account for blank files in DATs
      }
      
      if(zip_entry_isdir(zip) == 0){ // current zip entry is not a directory
        data[filename] = crc32sum;
      }
    }
    zip_entry_close(zip);
  }
  zip_close(zip);
  return data;
}

/*
 * Gets filenames from an archive (e.g. zip/7z)
 *
 * Arguments:
 *     filename : Path to archive
 * 
 * Returns:
 *     filenames : Vectors containing filenames in the archive
 * 
 * Notes:
 *     Taken from https://github.com/libarchive/libarchive/wiki/Examples#List_contents_of_Archive_stored_in_File, https://stackoverflow.com/questions/49702764/libarchive-returns-error-on-some-entries-while-7z-can-extract-normally
*/
std::vector<std::string> list_archive(std::string filename) {
  struct archive *a;
  struct archive_entry *entry;
  std::vector<std::string> filenames;

  // create new archive struct for the file
  a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);

  // open file
  int r = archive_read_open_filename(a, filename.c_str(), 102400);

  if (r != ARCHIVE_OK) {
    std::cout << "cannot read file: " << filename << std::endl;
    std::cout << "read error: " << archive_error_string(a) << std::endl;
  }

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    filenames.push_back(archive_entry_pathname(entry));
    archive_read_data_skip(a);
  }

  // we are done with the current archive, free it
  r = archive_read_free(a);
  if (r != ARCHIVE_OK) {
    std::cout << "Failed to free archive object. Error: " << archive_error_string(a) << std::endl;
    exit(1);
  }
  return filenames;
}

// needed for extract
int copy_data(struct archive *ar, struct archive *aw) {
  int r;
  const void *buff;
  size_t size;
  la_int64_t offset;

  for (;;) {
    r = archive_read_data_block(ar, &buff, &size, &offset);
    if (r == ARCHIVE_EOF)
      return (ARCHIVE_OK);
    if (r < ARCHIVE_OK)
      return (r);
    r = archive_write_data_block(aw, buff, size, offset);
    if (r < ARCHIVE_OK) {
      fprintf(stderr, "%s\n", archive_error_string(aw));
      return (r);
    }
  }
}

/*
 * Extracts files from an archive (e.g. zip/7z)
 *
 * Arguments:
 *     filename : Path to archive
 *     destination : Path to folder where you want the extracted file to go (*Path must end with a forward slash)
 * 
 * Notes:
 *     Taken from https://github.com/libarchive/libarchive/wiki/Examples#A_Complete_Extractor, https://stackoverflow.com/questions/8384266/libarchive-extract-to-specified-directory
 *     archive_read_support_compression_all() is deprecated, replaced with archive_read_support_filter_all() as per https://github.com/google/file-system-stress-testing/commit/0b4cb3e9c42d67c1d40bc66f72d45152cd5c563b
*/
void extract(std::string filename, std::string destination) {
  struct archive *a;
  struct archive *ext;
  struct archive_entry *entry;
  int flags;
  int r;

  // select which attributes we want to restore
  flags = ARCHIVE_EXTRACT_TIME;
  flags |= ARCHIVE_EXTRACT_PERM;
  flags |= ARCHIVE_EXTRACT_ACL;
  flags |= ARCHIVE_EXTRACT_FFLAGS;

  a = archive_read_new();
  archive_read_support_format_all(a);
  archive_read_support_filter_all(a);
  ext = archive_write_disk_new();
  archive_write_disk_set_options(ext, flags);
  archive_write_disk_set_standard_lookup(ext);
  if ((r = archive_read_open_filename(a, filename.c_str(), 10240)))
    exit(1);
  for (;;) {
    r = archive_read_next_header(a, &entry);
    if (r == ARCHIVE_EOF)
      break;
    if (r < ARCHIVE_OK)
      fprintf(stderr, "%s\n", archive_error_string(a));
    if (r < ARCHIVE_WARN)
      exit(1);

    // extract to specified directory
    const char* currentFile = archive_entry_pathname(entry);
    const std::string fullOutputPath = destination + currentFile;
    archive_entry_set_pathname(entry, fullOutputPath.c_str());
    
    r = archive_write_header(ext, entry);
    if (r < ARCHIVE_OK)
      fprintf(stderr, "%s\n", archive_error_string(ext));
    else if (archive_entry_size(entry) > 0) {
      r = copy_data(a, ext);
      if (r < ARCHIVE_OK)
        fprintf(stderr, "%s\n", archive_error_string(ext));
      if (r < ARCHIVE_WARN)
        exit(1);
    }
    r = archive_write_finish_entry(ext);
    if (r < ARCHIVE_OK)
      fprintf(stderr, "%s\n", archive_error_string(ext));
    if (r < ARCHIVE_WARN)
      exit(1);
  }
  archive_read_close(a);
  archive_read_free(a);
  archive_write_close(ext);
  archive_write_free(ext);
}

/*
 * Compresses files to a .zip file using deflate
 *
 * Arguments:
 *     destination : Path to compressed file
 *     filenames : Vector containing strings of file paths
 *     rootfolder : Root folder of file paths. (*path must end with a forward slash) E.g. If filenames = ["/path/to/folder/abc.zip","/path/to/folder/test/def.zip"], rootfolder = "/path/to/folder/"
 *     compression_level : Compression level to use; "1" or "2" or "3" etc to "9"
 * 
 * Notes:
 *     Taken from https://github.com/libarchive/libarchive/wiki/Examples#A_Basic_Write_Example
*/
void write_zip(std::string destination, std::vector<std::string> filenames, std::string rootfolder, std::string compression_level) {
  struct archive *a;
  struct archive_entry *entry;
  struct stat st;
  char buff[8192];
  int len;
  int fd;

  a = archive_write_new();
  archive_write_set_format_zip(a); // create zip file
  std::string options = "zip:compression=deflate,compression-level=" + compression_level; // use deflate
  archive_write_set_options(a, options.c_str());
  archive_write_open_filename(a, destination.c_str());
  for(auto i: filenames) {
    stat(i.c_str(), &st);
    entry = archive_entry_new();
    std::string filename = i.substr(rootfolder.length()); // make filenames in zip a relative path; removes first X characters from string i, where X = rootfolder.length()
    archive_entry_set_pathname(entry, filename.c_str());
    archive_entry_set_size(entry, st.st_size);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    fd = open(i.c_str(), O_RDONLY);
    len = read(fd, buff, sizeof(buff));
    while (len > 0) {
      archive_write_data(a, buff, len);
      len = read(fd, buff, sizeof(buff));
    }
    close(fd);
    archive_entry_free(entry);
  }
  archive_write_close(a);
  archive_write_free(a);
}

/*
 * Compresses files to a .7z file using LZMA2
 *
 * Arguments:
 *     destination : Path to compressed file
 *     filenames : Vector containing strings of file paths
 *     rootfolder : Root folder of file paths. (*path must end with a forward slash) E.g. If filenames = ["/path/to/folder/abc.zip","/path/to/folder/test/def.zip"], rootfolder = "/path/to/folder/"
 *     compression_level : Compression level to use; "0" or "1" or "2" etc to "9"
 * 
 * Notes:
 *     Taken from https://github.com/libarchive/libarchive/wiki/Examples#A_Basic_Write_Example
*/
void write_7z(std::string destination, std::vector<std::string> filenames, std::string rootfolder, std::string compression_level) {
  struct archive *a;
  struct archive_entry *entry;
  struct stat st;
  char buff[8192];
  int len;
  int fd;

  a = archive_write_new();
  archive_write_set_format_7zip(a); // create 7z file
  std::string options = "7zip:compression=lzma2,compression-level=" + compression_level; // use LZMA2
  archive_write_set_options(a, options.c_str());
  archive_write_open_filename(a, destination.c_str());
  for(auto i: filenames) {
    stat(i.c_str(), &st);
    entry = archive_entry_new();
    std::string filename = i.substr(rootfolder.length()); // make filenames in zip a relative path; removes first X characters from string i, where X = rootfolder.length()
    archive_entry_set_pathname(entry, filename.c_str());
    archive_entry_set_size(entry, st.st_size);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    fd = open(i.c_str(), O_RDONLY);
    len = read(fd, buff, sizeof(buff));
    while (len > 0) {
      archive_write_data(a, buff, len);
      len = read(fd, buff, sizeof(buff));
    }
    close(fd);
    archive_entry_free(entry);
  }
  archive_write_close(a);
  archive_write_free(a);
}