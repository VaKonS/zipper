
#define UNICODE

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cmath>
#include <thread>
#include "tclap/CmdLine.h"
#include "dirent.h"
#include <windows.h>
#include <io.h>


// ----------------------------------------------------------
// initialized data
const std::string zipExt = ".zip";
bool auto_passes = false;


// uninitialized data
bool show_passes, show_full, is_full, old_detection;
unsigned passes, begin, detect_threshold;
size_t mem_limit, mem_use;
std::string zipTempDir, zipInputDir, zipOutputDir, arcname_out, mmt, redefine, zipSingle, zip_cmd;
std::string arg_string[4]; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
std::vector<std::string> dir_list;
std::vector<bool> dir_subdirs;
DIR * dir_handle;
dirent * dir_entry;
std::ifstream file_check;
std::ofstream outfile;
std::vector<std::vector<char>> zip_storage;
std::vector<int> zip_indices; // zip_storage indices
std::vector<char> zip_pass1;
std::vector<unsigned> cycleN_count;
std::vector<bool> cycleN_match;
std::vector<unsigned> cycleN_sizes;
char path_buf[2048];
MEMORYSTATUS mem_stat;
std::vector<int> params; // arg_string[] indices, -1 for pass number
std::vector<size_t> positions;


// ----------------------------------------------------------
size_t path_length(std::string i) { // drive's "current directory" (i. e. "a:" without slash) will not be found
    size_t s = i.find_last_of('/');
    size_t b = i.find_last_of('\\');
    if (s == std::string::npos) s = b;
    if (b != std::string::npos) if (b > s) s = b;
    if (s != std::string::npos) s++; else s = 0;
    return s;
}

std::string ShortPath(std::string pn) {
    std::string p = pn.substr(0, path_length(pn));
    if (GetShortPathNameA(p.c_str(), (LPSTR) &path_buf, sizeof(path_buf)) > sizeof(path_buf)) { //(lpszLongPath,lpszShortPath,cchBuffer)
        std::cerr << "Path buffer is too small." << std::endl;
        std::exit(-1);
    }
    return std::string(path_buf);
}

std::string trailSlash(std::string s) {
    unsigned c = s.size();
    if (c == 0) return "";
    c = s[c - 1];
    if ((c == '\\') or (c == '/')) return s;
    return s + "\\";
}

std::string papl(unsigned n) {return (n == 1) ? " pass" : " passes";}
std::string bypl(unsigned n) {return (n == 1) ? "byte" : "bytes";}

int main(int argc, char** argv) {

    // definition of command line arguments   abcdefghijklmnopqrstuvwxyz
    //                                        abcd.f..i..lmnop.rst......
    TCLAP::CmdLine cmd("Zipper: checks different number of compression passes for 7-Zip ZIP archives.", ' ', "Zipper v1.42");

    TCLAP::ValueArg<std::string> cmdInputDir("i", "input-mask",
                    "Directory with files to compress.\nRun Zipper from this directory to avoid paths inside archives. [.]", false,
                    ".", "string", cmd);

    TCLAP::ValueArg<std::string> cmdOutputDir("o", "output-dir",
                    "Directory for zipped files.\nUse \"/\" or \"\\\\\" instead of \"\\\".", true,
                    "", "string", cmd);

    TCLAP::ValueArg<std::string> cmdSingle("n", "single",
                    "If present, compress files to single archive in output-dir. [\"\"]", false,
                    "", "string", cmd);

    TCLAP::ValueArg<std::string> cmdTempDir("t", "temp-dir",
                    "Directory for temporary archives.\nBest option is to use RAM drive. [same as output-dir]", false,
                    "", "string", cmd);

    TCLAP::ValueArg<std::string> cmd7Zip("c", "compressor",
                    "Pathname to 7-Zip archiver. [c:\\Program Files\\7-Zip\\7z.exe]", false,
                    "c:\\Program Files\\7-Zip\\7z.exe", "string", cmd);

    TCLAP::ValueArg<int> cmdPasses("p", "passes",
                    "Passes limit, set to 100 for relatively fast, but incomplete check. [0 = auto]", false,
                    0, "integer", cmd);

    TCLAP::ValueArg<int> cmdStart("b", "begin",
                    "Start passes value. Useful to continue interrupted test. [1]", false,
                    1, "integer", cmd);

    TCLAP::ValueArg<int> cmdMemLimit("l", "memory-limit",
                    "Limit of memory usage, Mb. [512]", false,
                    512, "integer", cmd);

    TCLAP::ValueArg<int> cmdDetect("d", "detect-cycling",
                    "Number of identical archives to stop.\nMore than 12 doesn't seem to make sense. [12]", false,
                    12, "integer", cmd);

    TCLAP::ValueArg<bool> cmdShowPasses("s", "show-passes",
                    "Show number of found and checked passes in \"name.best#.of#.zip\". [1 = yes]", false,
                    true, "boolean", cmd);

    TCLAP::ValueArg<bool> cmdShowFull("f", "show-fullness",
                    "Show \"cycle size / unfinished search\" in names. [1 = yes]", false,
                    true, "boolean", cmd);

    TCLAP::ValueArg<bool> cmdOld("a", "alternative-detection",
                    "Use old cycling detection algorithm. [0 = no]", false,
                    false, "boolean", cmd);

    TCLAP::ValueArg<std::string> cmdMMT("m", "multithreading",
                    "7-Zip multithreading = off/on/N threads. [on]", false,
                    "on", "string", cmd);

    TCLAP::ValueArg<std::string> cmdRedefine("r", "redefine",
                    "Fully redefine command line.\nPass arguments as %c (7z.exe), \\\"%i\\\" (file), \\\"%o\\\" (archive), %p (passes).\nUse %% to pass % character before c, i, o, p without substitution.", false,
                    "", "string", cmd);

    // parse command line arguments
    try {
        cmd.parse(argc, argv);
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Error : cmd.parse() threw exception" << std::endl;
        std::exit(-1);
    }

    // command line accepted, begin console processing
    arg_string[0]      = cmd7Zip.getValue(); // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
    zipTempDir         = cmdTempDir.getValue();
    zipInputDir        = cmdInputDir.getValue();
    zipOutputDir       = cmdOutputDir.getValue();
    passes             = std::max(0, cmdPasses.getValue());
    begin              = std::max(cmdStart.getValue(), 1);
    mem_limit          = cmdMemLimit.getValue() << 20;
    mmt                = cmdMMT.getValue();
    show_passes        = cmdShowPasses.getValue();
    show_full          = cmdShowFull.getValue();
    redefine           = cmdRedefine.getValue();
    zipSingle          = cmdSingle.getValue();
    old_detection      = cmdOld.getValue();
    detect_threshold   = std::max(1, std::min(cmdDetect.getValue(), ((passes != 0) ? static_cast<int>(passes) : (int) -1)));
    if (passes < 1) {
        if (!old_detection) {
            auto_passes = true;
            passes = std::max(1728, (int) detect_threshold) * 3; //1728 = 24*24*3
        } else {
            std::cerr << "Old detection does not find cycles. Setting passes to 1728." << std::endl;
            passes = 1728;
        }
    }

    if (begin > passes) {
        std::cerr << "\nStarting number of passes is greater than total number (" << begin << " > " << passes << "). Stop." << std::endl;
        std::exit(-1);
    }

    arg_string[0] = ShortPath(arg_string[0]) + "7z.exe"; //archiver's short pathname
    std::cout << "Archiver: \"" << arg_string[0] << "\"." << std::endl;

    arg_string[3] = "%"; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %

    // parsing command template
    params.clear();
    positions.clear();
    if (redefine.size() != 0) {
        size_t l = 0;
        while (l < redefine.npos) {
            size_t a = redefine.find("%i", l);
            size_t b = redefine.find("%o", l);
            size_t c = redefine.find("%p", l);
            size_t d = redefine.find("%c", l);
            size_t e = redefine.find("%%", l);
            l = std::min(std::min(std::min(std::min(a, b), c), d), e);
            if (l < redefine.npos) {
                if      (l == a) params.push_back(1);  // input
                else if (l == b) params.push_back(2);  // temp archive
                else if (l == c) params.push_back(-1); // pass
                else if (l == d) params.push_back(0);  // archiver
                else if (l == e) params.push_back(3);  // %
                positions.push_back(l);
                l = l + 2;
            }
        }
    }

    zipInputDir = trailSlash(zipInputDir);
    zipOutputDir = trailSlash(zipOutputDir);
    if (zipTempDir.size() == 0) zipTempDir = zipOutputDir; else zipTempDir = trailSlash(zipTempDir);

    // making recursive input directory listing
    dir_list.push_back("");
    dir_subdirs.push_back(true);
    int subdir_index = 0;
    while (subdir_index != -1) {
        std::string recursiveDir = trailSlash(zipInputDir + dir_list[subdir_index]);
        dir_handle = opendir(recursiveDir.c_str());
        if (dir_handle != NULL) {
            std::cout << "Opened directory \"" << recursiveDir << "\"." << std::endl;
            while ((dir_entry = readdir(dir_handle)) != NULL) {
                std::string n(dir_entry->d_name);
                //TODO: Unicode?
                int t = GetFileAttributesA((recursiveDir + n).c_str());
                if (t == -1) {
                    closedir(dir_handle);
                    std::cerr << "\nError reading attributes of \"" << (recursiveDir + n) << "\"." << std::endl;
                    return EXIT_FAILURE;
                }
                if (((t & FILE_ATTRIBUTE_DIRECTORY) == 0) || ((n != ".") && (n != ".."))) { // file or dir except ./..
                    dir_list.push_back(trailSlash(dir_list[subdir_index]) + n);
                    dir_subdirs.push_back((t & FILE_ATTRIBUTE_DIRECTORY) != 0);
                }
            }
            closedir(dir_handle);
        } else {
            std::cerr << "\nError listing directory \"" << recursiveDir << "\"." << std::endl;
            return EXIT_FAILURE;
        }
        dir_list.erase(dir_list.begin() + subdir_index);
        dir_subdirs.erase(dir_subdirs.begin() + subdir_index);
        subdir_index = -1;
        for (unsigned i = 0; i < dir_subdirs.size(); i++) if (dir_subdirs[i]) {
            subdir_index = i;
            break;
        }
    }
    std::cout << "Found " << dir_list.size() << " file(s)." << std::endl;
    dir_subdirs.clear();


    mem_stat.dwLength = sizeof(mem_stat);
    zip_indices.resize(passes);
    if (!old_detection) {
        cycleN_count.resize(passes);
        cycleN_match.resize(passes);
        cycleN_sizes.resize(passes);
    }

    // processing file list
    for (unsigned i = 0; i < dir_list.size(); i++) {
        arg_string[1] = zipInputDir + dir_list[i]; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
        std::cout << "\n-------------------------------------\nFile: " << arg_string[1] << std::endl;
        file_check = std::ifstream(arg_string[1], std::ifstream::binary);
        if (file_check) {
            file_check.close();

            arg_string[2] = zipTempDir + dir_list[i].substr(path_length(dir_list[i])) + zipExt; // input name without path
            std::cout << "Testing: " << arg_string[2] << std::endl;

            for (unsigned p = 0; p < zip_storage.size(); p++) zip_storage[p].clear(); // freeing memory
            zip_storage.clear();
            zip_indices.assign(passes, -1); // resetting indices
            mem_use = 0;
            is_full = false;
            unsigned minimal_zip_length = (unsigned) -1;
            unsigned minimal_zip_passes = 0;
            unsigned match_counter = 0;
            unsigned pass_counter = 0;
            unsigned cycle_size = 0;
            unsigned max_cycle = 0;
            unsigned max_cycle_start = 0;
            if (!old_detection) {
                cycleN_count.assign(passes, 0);
                cycleN_sizes.assign(passes, 0);
            }
            unsigned p = begin - 1;
            while (p < passes) {
                std::cout << "-------------------------------------\nPass: " << (p + 1) << "/" << passes;
                if (minimal_zip_passes == 0) {
                    std::cout << std::endl;
                } else {
                    std::cout << ", minimal packed size: " << minimal_zip_length << " " << bypl(minimal_zip_length) << " (" << (minimal_zip_passes) << papl(minimal_zip_passes) << ")." << std::endl;
                }
                if (redefine.size() == 0) {
                    zip_cmd = arg_string[0] + " a -tzip -mx=9 -mmt=" + mmt + " -mtc=off -mfb=258 -mpass=" + std::to_string(p + 1)
                              + " \"" + arg_string[2] + "\" \"" + arg_string[1] + "\"";
                } else {
                    zip_cmd = redefine;
                    for (int a = positions.size() - 1; a >= 0; a--) {
                        zip_cmd.erase(positions[a], 2);
                        if (params[a] != -1) {
                            zip_cmd.insert(positions[a], arg_string[params[a]]);
                        } else {
                            zip_cmd.insert(positions[a], std::to_string(p + 1));
                        }
                    }
                }
                std::cout << zip_cmd.c_str() << std::endl;
                system(zip_cmd.c_str());
                file_check = std::ifstream(arg_string[2], std::ifstream::binary);
                if (file_check) {
                    file_check.seekg(0, file_check.end); // get length
                    int zip_length = file_check.tellg();
                    if (zip_length < 0) {
                        std::cout << "\nCan not get length of archive." << std::endl;
                        zip_length = 0;
                    }
                    GlobalMemoryStatus(&mem_stat);
                    if (((mem_use + zip_length) > mem_limit) || (static_cast<size_t>(zip_length) > mem_stat.dwAvailPhys)) {
                        std::cout << "\nNo memory left to store archives.\nChoosing from what was saved so far..." << std::endl;
                        file_check.close();
                        if (zipSingle.size() != 0) { // when packing to single archive, memory storage is not used and smaller size can still be packed
                            if (zip_length > 0) if ((unsigned) zip_length < minimal_zip_length) {
                                pass_counter = p + 1;
                                minimal_zip_length = zip_length;
                                minimal_zip_passes = pass_counter;
                            }
                        }
                        goto passes_checked;
                    }
                    unsigned m = std::max(std::min( (static_cast<int>(mem_limit) - static_cast<int>(mem_use) - zip_length), (static_cast<int>(mem_stat.dwAvailPhys) - zip_length) ), 0) >> 20;
                    std::cout << "Memory left: " << m << " mega" << bypl(m) << std::endl;
                    if (zip_length > 0) {
                        pass_counter = p + 1;
                        file_check.seekg(0, file_check.beg);
                        zip_pass1.resize(zip_length);
                        file_check.read(zip_pass1.data(), zip_length); // reading zip to memory
                        file_check.close();
                        int add_index = -1;
                        if (!old_detection) cycleN_match.assign(passes, false);
                        for (int c = p - 1; c >= 0; c--) {
                            if (zip_indices[c] != -1) { // for skipped passes
                                if (zip_storage[zip_indices[c]].size() == static_cast<size_t>(zip_length)) {
                                    if (memcmp(zip_storage[zip_indices[c]].data(), zip_pass1.data(), zip_length) == 0) {
                                        add_index = c;
                                        if (old_detection) {
                                            match_counter++;
                                            std::cout << "Matched archives: " << match_counter << "/" << detect_threshold << std::endl;
                                            cycle_size = detect_threshold;
                                            if (match_counter == detect_threshold) {
                                                is_full = true;
                                                goto passes_checked;
                                            } else {
                                                //std::cout << "Match, referencing previous copy." << std::endl;
                                                zip_indices[p] = zip_indices[add_index];
                                                goto sample_added;
                                            }
                                        } else { // new detection
                                            cycle_size = p - c;
                                            cycleN_match[cycle_size - 1] = true;
                                            if (cycle_size >= detect_threshold) {
                                                int cycle_start = c - cycle_size + 1; // 1st match already found
                                                if (cycle_start >= 0) {
                                                    unsigned dc = 1;
                                                    for (int d = cycle_start; d < c; d++) {
                                                        int r  = zip_indices[d];
                                                        if (r == -1) goto wrong_cycle;
                                                        int r1 = zip_indices[d + cycle_size];
                                                        if (r1 == -1) goto wrong_cycle;
                                                        if (zip_storage[r].size() != zip_storage[r1].size()) goto wrong_cycle;
                                                        if (memcmp(zip_storage[r].data(), zip_storage[r1].data(), zip_storage[r].size()) != 0) goto wrong_cycle;
                                                        dc++;
                                                    }
                                                    std::cout << //"Cycle: " << dc << "/" << cycle_size << ".\n"
                                                                 "Compression cycling detected, " << cycle_size << " archives. More passes should not be necessary." << std::endl;
                                                    is_full = true;
                                                    goto passes_checked;
                                                }
                                            }
wrong_cycle:                                ; //nop
                                        }
                                    }
                                }
                            }
                        }
                        if (!old_detection) { // matches in old detection already bypassed this part
                            if (add_index != -1) {
                                bool line_start = true;
                                for (unsigned c = 0; c < passes; c++) {
                                    if (cycleN_match[c]) {
                                        //std::cout << "cycleN_match[" << c + 1 << "]" << std::endl;
                                        cycleN_count[c]++;
                                        //std::cout << "cycleN_count[c]: " << cycleN_count[c] << std::endl;
                                        if (line_start) {
                                            if (auto_passes) {
                                                // checking minimal cycle sizes array
                                                //24  48  72  96  120 144 168 192 216 240 264 288 312 336 360 384 408 432 456 480 504 528 552 576 600 624 648 672 696 720 744 ... 1440
                                                //                        *1                          *2                          *3                          *4                          --------
                                                //                    *1                      *2                      *3                      *4                      *5                  --------
                                                //                *1                  *2                  *3                  *4                  *5                  *6          *
                                                //            *1              *2              *3              *4              *5              *6              *7                  *       --------
                                                //        *1          *2          *3          *4          *5          *6          *7          *8          *9          *10         *
                                                //    *1      *2      *3      *4      *5      *6      *7      *8      *9      *10     *11     *12     *13     *14     *15         *       --------
                                                //*1  *2  *3  *4  *5  *6  *7  *8  *9  *10 *11 *12 *13 *14 *15 *16 *17 *18 *19 *20 *21 *22 *23 *24 *25 *26 *27 *28 *29 *30 *       *
                                                //------------------------------------------------------------------------------------------------------------------------------------
                                                //                *1                  *2                  *3                  *4                  *5                  *6          *
                                                //        *1          *2          *3          *4          *5          *6          *7          *8          *9          *10         *
                                                for (unsigned k = 0; k < passes; k++) {
                                                    unsigned cs = cycleN_sizes[k];
                                                    //std::cout << "cycleN_sizes[" << k << "]: " << cs << std::endl;
                                                    if (cs == 0) { // end of array, new minimal cycle found
                                                        cs = c + 1;
                                                        cycleN_sizes[k] = cs;
                                                        // searching minimal multiple of all sizes
                                                        unsigned multiple = cs;
                                                        for (unsigned l = 0; l < k; l++) {
                                                            for (unsigned n = 1; n <= cs; n++) {
                                                                unsigned m = cycleN_sizes[l] * n;
                                                                if ((m % cs) == 0) {
                                                                    if (m > multiple) multiple = m;
                                                                    break;
                                                                }
                                                            }
                                                        }
                                                        //std::cout << "Found multiple: " << multiple << std::endl;
                                                        if (multiple > max_cycle) {
                                                            max_cycle = multiple;
                                                            if (max_cycle_start == 0) max_cycle_start = p - cs;
                                                            //std::cout << "Found max_cycle: " << max_cycle << ", start: " << max_cycle_start << ", passes: " << passes << std::endl;
                                                            if ((max_cycle * 3) > passes) {
                                                                unsigned prev_passes = passes;
                                                                passes = max_cycle * 7;
                                                                zip_indices.resize(passes);
                                                                cycleN_count.resize(passes);
                                                                cycleN_match.resize(passes);
                                                                cycleN_sizes.resize(passes);
                                                                for (unsigned l = prev_passes; l < passes; l++) {
                                                                    zip_indices[l] = -1;
                                                                    //cycleN_count[l] = 0;
                                                                    //cycleN_match[l] = 0;
                                                                    //cycleN_sizes[l] = 0;
                                                                }
                                                                //std::cout << "New passes maximum: " << passes << std::endl;
                                                            }
                                                        }
                                                        break;
                                                    }
                                                    if (cs == (c + 1)) break; // minimal cycle size is already present
                                                }
                                            }
                                            line_start = false;
                                            if (!auto_passes) std::cout << "Possible cycle(s): ";
                                        } else
                                            if (!auto_passes) std::cout << ", ";
                                        if (!auto_passes) std::cout << cycleN_count[c] << "/" << (c + 1);
                                    } else
                                        cycleN_count[c] = 0;
                                }
                                if (!line_start) if (!auto_passes) std::cout << "." << std::endl;
                                //std::cout << "Matched sample, referencing previous copy." << std::endl;
                                zip_indices[p] = zip_indices[add_index];
                                goto sample_added;
                            } else
                                cycleN_count.assign(passes, 0); // resetting matches counters
                        } else
                            match_counter = 0; // new detection does not use match_counter
                        //std::cout << "Adding new archive." << std::endl;
                        zip_indices[p] = zip_storage.size();
                        zip_storage.emplace_back(); // empty sample
                        zip_storage.back().swap(zip_pass1); // moving new sample to storage
                        mem_use += zip_length;
                        if ((unsigned) zip_length < minimal_zip_length) {
                            minimal_zip_length = zip_length;
                            minimal_zip_passes = pass_counter; // p + 1
                        }
sample_added:           if (auto_passes) std::cout << "Estimated cycle: " << max_cycle << ", total passes: " << (max_cycle_start + max_cycle * 2) << std::endl;
                    } else
                        file_check.close();
                } else
                    std::cerr << "\nCan not open archive \"" << arg_string[2] << "\"." << std::endl;
                p++;
            }
passes_checked:
            if (minimal_zip_passes != 0) {
                std::cout << "Minimum archive size: " << minimal_zip_length << " " << bypl(minimal_zip_length) << " (" << (minimal_zip_passes) << papl(minimal_zip_passes) << ")." << std::endl;
            } else {
                std::cout << "No archives were created." << std::endl;
            }
            std::cout << "Removing \"" << arg_string[2] << "\"." << std::endl;
            remove(arg_string[2].c_str());

            if (zipSingle.size() == 0) {
                arcname_out = zipOutputDir + dir_list[i];
                if (minimal_zip_passes != 0) {
                    int d = std::floor(std::log10(passes)) + 1;
                    std::string f = "%0" + std::to_string(d) + "u";
                    if (show_passes) {
                        snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), minimal_zip_passes);
                        arcname_out = arcname_out + ".best" + std::string(path_buf);
                        snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), pass_counter);
                        arcname_out = arcname_out + ".of" + std::string(path_buf);
                    }
                    if (show_full) {
                        if (old_detection) {
                            snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), match_counter);
                            arcname_out = arcname_out + ".match" + (is_full ? std::string(path_buf) : "-");
                        } else {
                            snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), cycle_size);
                            arcname_out = arcname_out + ".cycle" + (is_full ? std::string(path_buf) : "-");
                        }
                    }
                    arcname_out += zipExt;
                    std::string out_subdir = arcname_out.substr(0, path_length(arcname_out) - 1);
                    if (GetFileAttributesA(out_subdir.c_str()) == (unsigned) -1) {
                        std::cout << "Creating subdirectory \"" << out_subdir << "\"." << std::endl;
                        if (mkdir(out_subdir.c_str()) != 0) {
                            std::cerr << "\nError making directory \"" << out_subdir << "\"." << std::endl;
                        }
                    }
                    std::cout << "Writing \"" << arcname_out << "\"." << std::endl;
                    outfile = std::ofstream(arcname_out, std::ofstream::binary);
                    if (!outfile) {
                        std::cerr << "\nError writing archive \"" << arcname_out << "\"." << std::endl;
                    } else {
                        outfile.write(zip_storage[zip_indices[minimal_zip_passes - 1]].data(), minimal_zip_length); // save smallest archive
                        outfile.close();
                    }
                } else
                    std::cerr << "\nError compressing archive \"" << arcname_out << ".#.#.#" << zipExt << "\"." << std::endl;
            } else { // single archive
                if (minimal_zip_passes != 0) {
                    arcname_out = zipOutputDir + zipSingle;
                    if (redefine.size() == 0) {
                        //TODO: how to add *single* "path\file" to the archive with relative path kept?
                        zip_cmd = arg_string[0] + " a -tzip -mx=9 -mmt=" + mmt + " -mtc=off -mfb=258 -mpass=" + std::to_string(minimal_zip_passes)
                                  + " \"" + arcname_out + "\" \"" + arg_string[1] + "\"";
                    } else {
                        zip_cmd = redefine;
                        for (int a = positions.size() - 1; a >= 0; a--) {
                            zip_cmd.erase(positions[a], 2);
                            switch (params[a]) {
                            case -1: // pass
                                zip_cmd.insert(positions[a], std::to_string(minimal_zip_passes));
                            break;
                            case 2:  // output archive
                                zip_cmd.insert(positions[a], arcname_out);
                            break;
                            default: // archiver, input file, %
                                zip_cmd.insert(positions[a], arg_string[params[a]]);
                            break;
                            }
                        }
                    }
                    std::cout << "Adding to \"" << arcname_out << "\"." << std::endl;
                    std::cout << zip_cmd.c_str() << std::endl;
                    system(zip_cmd.c_str());
                } else {
                    std::cerr << "\nError compressing file \"" << arg_string[1] << "\"." << std::endl;
                    break; // interrupt creation of single archive
                }
            }
        } else
            std::cerr << "\nCan not open file \"" << arg_string[1] << "\"." << std::endl;
    }

//clean_end:
    zip_indices.clear();
    cycleN_count.clear();
    cycleN_match.clear();
    for (unsigned p = 0; p < zip_storage.size(); p++) zip_storage[p].clear();
    zip_storage.clear();
    params.clear();
    positions.clear();
    return 0;
}
