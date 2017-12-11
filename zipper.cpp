
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


// uninitialized data
bool show_passes, show_full, is_full, old_detection;
unsigned passes, begin, detect_threshold;
size_t mem_limit, mem_use;
std::string zipTempDir, zipInputDir, zipOutputDir, arcname_out, mmt, redefine, zipSingle, zip_cmd;
std::string arg_string[4]; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
std::vector<std::string> dir_list;
DIR * dir_handle;
dirent * dir_entry;
std::ifstream file_check;
std::ofstream outfile;
std::vector<std::vector<char>> zip_samples, zip_pass1;
std::vector<unsigned> cycleN_count;
char path_buf[2048];
MEMORYSTATUS mem_stat;
std::vector<int> params; // arg_string[] indexes, -1 for pass number
std::vector<size_t> positions;


// ----------------------------------------------------------
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
    TCLAP::CmdLine cmd("Zipper: checks different number of compression passes for 7-Zip ZIP archives.", ' ', "Zipper v1.3");

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
                    "Passes limit. [100]", false,
                    100, "integer", cmd);

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
    passes             = cmdPasses.getValue();
    begin              = std::max(cmdStart.getValue(), 1);
    mem_limit          = cmdMemLimit.getValue() << 20;
    mmt                = cmdMMT.getValue();
    show_passes        = cmdShowPasses.getValue();
    show_full          = cmdShowFull.getValue();
    detect_threshold   = std::max(1, std::min(cmdDetect.getValue(), static_cast<int>(passes)));
    redefine           = cmdRedefine.getValue();
    zipSingle          = cmdSingle.getValue();
    old_detection      = cmdOld.getValue();

    if (begin > passes) {
        std::cerr << "\nStarting number of passes is greater than total number (" << begin << " > " << passes << "). Stop." << std::endl;
	std::exit(-1);
    }

    //archiver short path
    size_t tailSlash = arg_string[0].find_last_of('/'); // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
    size_t tailBackslash = arg_string[0].find_last_of('\\');
    if (tailSlash == std::string::npos) tailSlash = tailBackslash;
    if (tailBackslash != std::string::npos) if (tailBackslash > tailSlash) tailSlash = tailBackslash;
    if (tailSlash != std::string::npos) arg_string[0].erase(tailSlash + 1, arg_string[0].size());
    if (GetShortPathNameA(arg_string[0].c_str(), (LPSTR) &path_buf, sizeof(path_buf)) > sizeof(path_buf)) { //(lpszLongPath,lpszShortPath,cchBuffer)
        std::cerr << "Path buffer is too small." << std::endl;
	std::exit(-1);
    }
    arg_string[0] = std::string(path_buf) + "7z.exe"; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
    std::cout << "Archiver: \"" << arg_string[0] << "\"." << std::endl;

    arg_string[3] = "%"; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %

    zipInputDir = trailSlash(zipInputDir);
    zipOutputDir = trailSlash(zipOutputDir);
    if (zipTempDir == "") { zipTempDir = zipOutputDir; } else zipTempDir = trailSlash(zipTempDir);

    dir_handle = opendir(zipInputDir.c_str());
    if (dir_handle != NULL) {
        std::cout << "Opened directory \"" << zipInputDir << "\"." << std::endl;
        while ((dir_entry = readdir(dir_handle)) != NULL)
            dir_list.push_back(std::string(dir_entry->d_name));
        closedir(dir_handle);
    } else {
        std::cerr << "\nError listing directory \"" << zipInputDir << "\"." << std::endl;
	return EXIT_FAILURE;
    }

    mem_stat.dwLength = sizeof(mem_stat);
    for (unsigned i = 0; i < dir_list.size(); i++) if ((dir_list[i] != ".") && (dir_list[i] != "..")) {
        is_full = false;
        arg_string[1] = zipInputDir + dir_list[i]; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
        std::cout << "\n-------------------------------------\nFile: " << arg_string[1] << std::endl;
        file_check = std::ifstream(arg_string[1], std::ifstream::binary);
        if (file_check) {
            file_check.close();

            // freeing memory
            for (unsigned p = 0; p < zip_samples.size(); p++) zip_samples[p].clear();
            zip_samples.clear();
            params.clear();
            positions.clear();
            mem_use = 0;

            arg_string[2] = zipTempDir + dir_list[i] + zipExt; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
            std::cout << "Testing: " << arg_string[2] << std::endl;
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

            std::vector<char> minimal_zip_sample;
            unsigned minimal_zip_length = (unsigned) -1;
            unsigned minimal_zip_passes = 0;
            unsigned match_counter = 0;
            unsigned pass_counter = 0;
            unsigned cycle_size = 0;
            cycleN_count.clear();
            unsigned cycle_size_max = 0;
            zip_samples.resize(begin - 1); // initialize skipped passes with empty samples
            for (unsigned p = begin - 1; p < passes; p++) {
                std::cout << "-------------------------------------\nPass: " << (p + 1) << "/" << passes;
                if (minimal_zip_passes == 0) {
                    std::cout << std::endl;
                } else {
                    std::cout << ", minimal packed size: " << minimal_zip_length << " " << bypl(minimal_zip_length) << " (" << (minimal_zip_passes) << papl(minimal_zip_passes) << ")." << std::endl;
                }
                if (redefine.size() == 0) {
                    //@for %%i in ("*.rdr" "*.geerdr" "*.drawinghand") do @for /L %%k in (1,1,80) do @"c:\Program Files\7-Zip\7z.exe" a -tzip -mx=9 -mmt=off -mtc=off -mfb=258 -mpass=%%k "%%~di%%~pizip\%%~ni%%~xi.%%k.zip" "%%i"
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
                        if (zipSingle.size() != 0) {
                            // when packing to single archive, memory storage is not used and smaller size can still be packed
                            if (zip_length > 0) if ((unsigned) zip_length < minimal_zip_length) {
                                minimal_zip_length = zip_length;
                                minimal_zip_passes = p + 1;
                                pass_counter = minimal_zip_passes;
                            }
                        }
                        goto passes_checked;
                    }
                    unsigned m = std::max(std::min( (static_cast<int>(mem_limit) - static_cast<int>(mem_use) - zip_length), (static_cast<int>(mem_stat.dwAvailPhys) - zip_length) ), 0) >> 20;
                    std::cout << "Memory left: " << m << " mega" << bypl(m) << std::endl;
                    if (zip_length > 0) {
                        pass_counter = p + 1;
                        file_check.seekg(0, file_check.beg);
                        zip_pass1.resize(1);
                        zip_pass1[0].resize(zip_length); // allocating memory
                        file_check.read(zip_pass1[0].data(), zip_length); // reading zip to memory
                        file_check.close();
                        int t, e;
                        bool line_start = true;
                        if (old_detection) {
                            t = 1;
                            e = 0;
                        } else {
                            t = detect_threshold;
                            e = zip_samples.size() >> 1;
                        }
                        int add_index = -1;
                        for (int c = (zip_samples.size() - t); c >= e; c--) {
                            if (zip_samples[c].size() == static_cast<size_t>(zip_length)) {
                                if (memcmp(zip_samples[c].data(), zip_pass1[0].data(), zip_length) == 0) {
                                    add_index = c;
                                    if (old_detection) {
                                        match_counter++;
                                        std::cout << "Matched archives: " << match_counter << "/" << detect_threshold << std::endl;
                                        cycle_size = detect_threshold;
                                        if (match_counter == detect_threshold) {
                                            is_full = true;
                                            zip_pass1[0].clear();
                                            zip_pass1.clear();
                                            goto passes_checked;
                                        } else
                                            break;
                                    } else { // new detection
                                        cycle_size = p - c;
                                        if (cycle_size_max < cycle_size) {
                                            cycle_size_max = cycle_size;
                                            cycleN_count.resize(cycle_size_max);
                                        }
                                        cycleN_count[cycle_size - 1]++;
                                        unsigned dc = 1;
                                        int cycle_start = c - cycle_size + 1;
                                        for (int d = cycle_start; d < c; d++) {
                                            if (zip_samples[d].size() != zip_samples[d + cycle_size].size()) goto wrong_cycle;
                                            if (memcmp(zip_samples[d].data(), zip_samples[d + cycle_size].data(), zip_samples[d].size()) != 0) goto wrong_cycle;
                                            dc++;
                                        }
                                        std::cout << (line_start ? "Cycle: " : ", ") << dc << "/" << cycle_size << ".\n"
                                                     "Compression cycling detected, " << cycle_size << " archives. More passes should not be necessary." << std::endl;
                                        is_full = true;
                                        zip_pass1[0].clear();
                                        zip_pass1.clear();
                                        goto passes_checked;
wrong_cycle:
                                        if (line_start) {
                                            std::cout << "Possible cycle(s): ";
                                            line_start = false;
                                        } else {
                                            std::cout << ", ";
                                        }
                                        std::cout << cycleN_count[cycle_size - 1] << "/" << cycle_size;
                                    }
                                }
                            }
                        }
                        if (add_index != -1) {
                            zip_pass1[0].clear();
                            zip_pass1.clear();
                            zip_samples.push_back(zip_samples[add_index]); // same sample, referencing previous copy
                            goto sample_added;
			} else {
                            for (unsigned c = 0; c < zip_samples.size(); c++) {
                                if (zip_samples[c].size() == static_cast<size_t>(zip_length)) {
                                    if (memcmp(zip_samples[c].data(), zip_pass1[0].data(), zip_length) == 0) {
                                        //std::cout << "Matched sample, referencing previous copy." << std::endl;
                                        zip_pass1[0].clear();
                                        zip_pass1.clear();
                                        zip_samples.push_back(zip_samples[c]);
                                        goto sample_added;
                                    }
                                }
                            }
                        }
                        //std::cout << "Adding new archive." << std::endl;
                        zip_samples.push_back(zip_pass1[0]);
                        zip_pass1.clear();
                        mem_use += zip_length;
                        if ((unsigned) zip_length < minimal_zip_length) {
                            minimal_zip_sample = zip_samples.back(); //zip_samples[zip_samples.size() - 1];
                            minimal_zip_length = zip_length;
                            minimal_zip_passes = pass_counter; // p + 1
                        }
                        match_counter = 0;
                        cycleN_count.clear(); // reset all matches counters
                        cycle_size_max = 0;
sample_added:
                        if (!line_start) std::cout << "." << std::endl;
                    } else
                        file_check.close();
                } else
                    std::cerr << "\nCan not open archive \"" << arg_string[2] << "\"." << std::endl;
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
                            snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), detect_threshold);
                            arcname_out = arcname_out + ".match" + (is_full ? std::string(path_buf) : "-");
                        } else {
                            snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), cycle_size);
                            arcname_out = arcname_out + ".cycle" + (is_full ? std::string(path_buf) : "-");
                        }
                    }
                    arcname_out += zipExt;
                    std::cout << "Writing \"" << arcname_out << "\"." << std::endl;
                    outfile = std::ofstream(arcname_out, std::ofstream::binary);
                    if (!outfile) {
                        std::cerr << "\nError writing archive \"" << arcname_out << "\"." << std::endl;
                    } else {
                        outfile.write(minimal_zip_sample.data(), minimal_zip_length); // save smallest archive
                        outfile.close();
                    }
                } else
                    std::cerr << "\nError compressing archive \"" << arcname_out << ".#.#.#" << zipExt << "\"." << std::endl;
            } else { // single archive
                if (minimal_zip_passes != 0) {
                    arcname_out = zipOutputDir + zipSingle;
                    if (redefine.size() == 0) {
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
                            case 2:  // output instead of temporary archive
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
            minimal_zip_sample.clear();
        } else
            std::cerr << "\nCan not open file \"" << arg_string[1] << "\"." << std::endl;
    }

//clean_end:
    for (unsigned p = 0; p < zip_samples.size(); p++) zip_samples[p].clear();
    zip_samples.clear();
    params.clear();
    positions.clear();
    return 0;
}
