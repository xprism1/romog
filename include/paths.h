#ifndef PATHS_H
#define PATHS_H

/*
 * Paths to files/folders required by romorganizer
 *
 * Notes:
 *     Paths to directories must end with a forwardslash
*/
inline std::string config_path = "/home/xp/.config/romog/config.yaml"; // config file
inline std::string links_path = "/home/xp/.config/romog/links.txt"; // links to DATs for DAT updater to use
inline std::string sort_xsl_path = "/home/xp/.config/romog/sort.xsl"; // don't edit sort.xsl
inline std::string www_path = "/home/xp/.config/romog/www.txt"; // links to profile.xmls in clrmamepro WWW profiler format
inline std::string backup_path = "/home/xp/Desktop/romog/backup/"; // files that don't match DAT from scanner will be moved here
inline std::string cache_path = "/home/xp/Desktop/romog/cache/"; // .cache files are stored here
inline std::string dats_path = "/home/xp/Desktop/romog/dats/"; // place your DATs here
inline std::string dats_new_path = "/home/xp/Desktop/romog/dats-new/"; // place new DATs here
inline std::string fix_path = "/home/xp/Desktop/romog/fix/"; // fixDATs are output here
inline std::string rebuild_path = "/home/xp/Desktop/romog/rebuild/"; // places your files to be rebuilt here
inline std::string tmp_path = "/home/xp/Desktop/romog/tmp/"; // temporary place where files are stored for further operations (should be empty)

#endif
