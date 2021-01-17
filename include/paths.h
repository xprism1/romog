#ifndef PATHS_H
#define PATHS_H

/*
 * Paths to files/folders required by romorganizer
 * 
 * Notes:
 *     Paths to directories must end with a forwardslash
*/
extern std::string config_path; // config file
extern std::string links_path; // links to DATs for DAT updater to use
extern std::string sort_xsl_path; // don't edit sort.xsl
extern std::string www_path; // links to profile.xmls in clrmamepro WWW profiler format
extern std::string backup_path; // files that don't match DAT from scanner will be moved here
extern std::string cache_path; // .cache files are stored here
extern std::string dats_path; // place your DATs here
extern std::string dats_new_path; // place new DATs here
extern std::string fix_path; // fixDATs are output here
extern std::string headers_path; // place your header files here
extern std::string rebuild_path; // places your files to be rebuilt here
extern std::string tmp_path; // temporary place where files are stored for further operations (should be empty)

#endif