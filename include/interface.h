#ifndef INTERFACE_H
#define INTERFACE_H

std::vector<std::string> sortPaths(std::vector<std::string> paths);
void genConfig(std::tuple<std::string, std::string> info = std::make_tuple("not_set","not_set"));
void listProfiles();
std::tuple<std::string, std::string> getPaths(std::string profile_no);
void showInfo(std::string dat_path, std::string hash = "not_set", std::string show = "not_set");
void batchScan(std::string dat_group, bool toRebuild = false);
void deleteProfile(std::string dat_path, bool toRemoveEntry = false);
void updateDats(bool download = false);
void listProfilesWithDate();

#endif