
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
bool show_passes;
unsigned passes, begin, detect_threshold;
size_t mem_limit, mem_use;
std::string SevenZip, zipTempDir, zipInputDir, zipOutputDir, arcname_temp, arcname_out, mmt, redefine, zipSingle, zip_cmd;
std::vector<std::string> dir_list;
DIR * dir_handle;
dirent * dir_entry;
std::ifstream file_check;
std::ofstream outfile;
std::vector<std::vector<char>> zip_passes;
char path_buf[2048];
MEMORYSTATUS mem_stat;
std::vector<std::string> params;
std::vector<size_t> positions;


// ----------------------------------------------------------
std::string trailSlash(std::string s) {
    unsigned c = s.size();
    if (c == 0) return "";
    c = s[c - 1];
    if ((c == '\\') or (c == '/')) return s;
    return s + "\\";
}

std::string papl(unsigned n) {
    if (n == 1) return " pass";
    return " passes";
}

std::string bypl(unsigned n) {
    if (n == 1) return "byte";
    return "bytes";
}

int main(int argc, char** argv) {

    // definition of command line arguments
    TCLAP::CmdLine cmd("Zipper: checks different number of compression passes for 7-Zip ZIP archives.", ' ', "Zipper v1.111");

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
                    "Number of identical archives to stop.\nMore than 24 doesn't seem to make sense. [12]", false,
                    12, "integer", cmd);

    TCLAP::ValueArg<bool> cmdShowPasses("s", "show-passes",
                    "Show number of found and checked passes in \"name.best#.of#.zip\". [1 = yes]", false,
                    true, "boolean", cmd);

    TCLAP::ValueArg<std::string> cmdMMT("m", "multithreading",
                    "7-Zip multithreading = off/on/N threads. [on]", false,
                    "on", "string", cmd);

    TCLAP::ValueArg<std::string> cmdRedefine("r", "redefine",
                    "Fully redefine command line.\nPass arguments as %c, \\\"%i\\\", \\\"%o\\\", \\\"%p\\\".\nUse %% to pass % character before c, i, o, p without substitution.", false,
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
    SevenZip           = cmd7Zip.getValue();
    zipTempDir         = cmdTempDir.getValue();
    zipInputDir        = cmdInputDir.getValue();
    zipOutputDir       = cmdOutputDir.getValue();
    passes             = cmdPasses.getValue();
    begin              = std::max(cmdStart.getValue(), 1);
    mem_limit          = cmdMemLimit.getValue() << 20;
    mmt                = cmdMMT.getValue();
    show_passes        = cmdShowPasses.getValue();
    detect_threshold   = cmdDetect.getValue();
    redefine           = cmdRedefine.getValue();
    zipSingle          = cmdSingle.getValue();

    if (begin > passes) {
        std::cerr << "\nStarting number of passes is greater than total number (" << begin << " > " << passes << "). Stop." << std::endl;
	std::exit(-1);
    }

    //archiver short path
    size_t tailSlash = SevenZip.find_last_of('/');
    size_t tailBackslash = SevenZip.find_last_of('\\');
    if (tailSlash == SevenZip.npos) tailSlash = tailBackslash;
    if (tailBackslash != SevenZip.npos) if (tailBackslash > tailSlash) tailSlash = tailBackslash;
    if (tailSlash != SevenZip.npos) SevenZip.erase(tailSlash + 1, SevenZip.size());
    if (GetShortPathNameA(SevenZip.c_str(), (LPSTR) &path_buf, sizeof(path_buf)) > sizeof(path_buf)) { //(lpszLongPath,lpszShortPath,cchBuffer)
        std::cerr << "Path buffer is too small." << std::endl;
	std::exit(-1);
    }
    SevenZip = std::string(path_buf) + "7z.exe";
    std::cout << "Archiver: \"" << SevenZip << "\"." << std::endl;

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
        std::cout << "\n-------------------------------------\nFile: " << dir_list[i] << std::endl;
        file_check = std::ifstream(zipInputDir + dir_list[i], std::ifstream::binary);
        if (file_check) {
            file_check.close();
            for (unsigned p = 0; p < zip_passes.size(); p++) zip_passes[p].clear();
            zip_passes.clear(); // freeing memory
            params.clear();
            positions.clear();
            zip_passes.resize(passes);
            mem_use = 0;
            unsigned match_counter = 0;
            unsigned pass_counter = 0;
            arcname_temp = zipTempDir + dir_list[i] + zipExt;
            std::cout << "Testing: " << arcname_temp << std::endl;

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
                        if      (l == a) params.push_back(zipInputDir + dir_list[i]);
                        else if (l == b) params.push_back(arcname_temp);
                        else if (l == c) params.push_back("");
                        else if (l == d) params.push_back(SevenZip);
                        else if (l == e) params.push_back("%");
                        positions.push_back(l);
                        l = l + 2;
                    }
                }
            }

            unsigned min_zip_length = (unsigned) -1;
            unsigned min_zip_index = 0;
            for (unsigned p = begin - 1; p < passes; p++) {
                std::cout << "-------------------------------------\nPasses: " << (p + 1) << "/" << passes;
                if (min_zip_length == (unsigned) -1) {
                    std::cout << std::endl;
                } else {
                    std::cout << ", minimal packed size: " << min_zip_length << " " << bypl(min_zip_length) << " (" << (min_zip_index + 1) << papl(min_zip_index + 1) << ")." << std::endl;
                }
                if (redefine.size() == 0) {
                    //@for %%i in ("*.rdr" "*.geerdr" "*.drawinghand") do @for /L %%k in (1,1,80) do @"c:\Program Files\7-Zip\7z.exe" a -tzip -mx=9 -mmt=off -mtc=off -mfb=258 -mpass=%%k "%%~di%%~pizip\%%~ni%%~xi.%%k.zip" "%%i"
                    zip_cmd = SevenZip + " a -tzip -mx=9 -mmt=" + mmt + " -mtc=off -mfb=258 -mpass=" + std::to_string(p + 1)
                              + " \"" + arcname_temp + "\" \"" + zipInputDir + dir_list[i] + "\"";
                } else {
                    zip_cmd = redefine;
                    for (int a = positions.size() - 1; a >= 0; a--) {
                        zip_cmd.erase(positions[a], 2);
                        if (params[a].size() != 0) {
                            zip_cmd.insert(positions[a], params[a]);
                        } else {
                            zip_cmd.insert(positions[a], std::to_string(p + 1));
                        }
                    }
                }
                std::cout << zip_cmd.c_str() << std::endl;
                system(zip_cmd.c_str());
                file_check = std::ifstream(arcname_temp, std::ifstream::binary);
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
                            if (zip_length > 0) if ((unsigned) zip_length < min_zip_length) {
                                min_zip_length = zip_length;
                                min_zip_index = p;
                                pass_counter = p + 1;
                            }
                        }
                        goto passes_checked;
                    }
                    unsigned m = std::max(std::min( (static_cast<int>(mem_limit) - static_cast<int>(mem_use) - zip_length), (static_cast<int>(mem_stat.dwAvailPhys) - zip_length) ), 0) >> 20;
                    std::cout << "Memory left: " << m << " mega" << bypl(m) << std::endl;
                    if (zip_length > 0) {
                        pass_counter = p + 1;
                        file_check.seekg(0, file_check.beg);
                        std::vector<char> zip_pass1;
                        zip_pass1.resize(zip_length); // allocating memory
                        file_check.read(zip_pass1.data(), zip_length); // reading zip to memory
                        file_check.close();
                        for (unsigned c = 0; c < p; c++) {
                            if (zip_passes[c].size() == static_cast<size_t>(zip_length)) {
                                if (memcmp(zip_passes[c].data(), zip_pass1.data(), zip_length) == 0) {
                                    match_counter++;
                                    std::cout << "Matched archives: " << match_counter << "/" << detect_threshold << std::endl;
                                    zip_pass1.clear(); // sample is already present
                                    if (match_counter == detect_threshold) {
                                        goto passes_checked;
                                    } else
                                        goto pass_matched;
                                }
                            }
                        }
                        if ((unsigned) zip_length < min_zip_length) {
                            min_zip_length = zip_length;
                            min_zip_index = p;
                        }
                        zip_passes[p] = zip_pass1; // new archive sample, adding to array
                        mem_use += zip_length;
                        match_counter = 0;
pass_matched:
                        match_counter = match_counter; // "nop", because label before "}" causes error
                    } else
                        file_check.close();
                } else
                    std::cerr << "\nCan not open archive \"" << arcname_temp << "\"." << std::endl;
            }
passes_checked:
            if (min_zip_length != (unsigned) -1) {
                std::cout << "Minimum archive size: " << min_zip_length << " " << bypl(min_zip_length) << " (" << (min_zip_index + 1) << papl(min_zip_index + 1) << ")." << std::endl;
            } else {
                std::cout << "No archives were created." << std::endl;
            }
            std::cout << "Removing \"" << arcname_temp.c_str() << "\"." << std::endl;
            remove(arcname_temp.c_str());

            if (zipSingle.size() == 0) {
                if (min_zip_length != (unsigned) -1) {
                    arcname_out = zipOutputDir + dir_list[i];
                    if (show_passes) {
                        int d = std::floor(std::log10(passes)) + 1;
                        std::string f = "%0" + std::to_string(d) + "u";
                        snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), (min_zip_index + 1));
                        arcname_out = arcname_out + ".best" + std::string(path_buf);
                        snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), pass_counter);
                        arcname_out = arcname_out + ".of" + std::string(path_buf);
                    }
                    arcname_out += zipExt;
                    std::cout << "Writing \"" << arcname_out << "\"." << std::endl;
                    outfile = std::ofstream(arcname_out, std::ofstream::binary);
                    if (!outfile) {
                        std::cerr << "\nError writing archive \"" << arcname_out << "\"." << std::endl;
                    } else {
                        outfile.write(zip_passes[min_zip_index].data(), min_zip_length); // save smallest archive
                        outfile.close();
                    }
                } else
                    std::cerr << "\nError compressing archive \"" << arcname_out << "\"." << std::endl;
            } else { // single archive
                if (min_zip_length != (unsigned) -1) {
                    arcname_out = zipOutputDir + zipSingle;
                    if (redefine.size() == 0) {
                        zip_cmd = SevenZip + " a -tzip -mx=9 -mmt=" + mmt + " -mtc=off -mfb=258 -mpass=" + std::to_string(min_zip_index + 1)
                                  + " \"" + arcname_out + "\" \"" + zipInputDir + dir_list[i] + "\"";
                    } else {
                        zip_cmd = redefine;
                        for (int a = positions.size() - 1; a >= 0; a--) {
                            zip_cmd.erase(positions[a], 2);
                            if (params[a].size() != 0) {
                                if (params[a] == arcname_temp) {
                                    zip_cmd.insert(positions[a], arcname_out);
                                } else {
                                    zip_cmd.insert(positions[a], params[a]);
                                }
                            } else {
                                zip_cmd.insert(positions[a], std::to_string(min_zip_index + 1));
                            }
                        }
                    }
                    std::cout << "Adding to \"" << arcname_out << "\"." << std::endl;
                    std::cout << zip_cmd.c_str() << std::endl;
                    system(zip_cmd.c_str());
                } else {
                    std::cerr << "\nError compressing file \"" << zipInputDir + dir_list[i] << "\"." << std::endl;
                    break; // interrupt creation of single archive
                }
            }
        } else
            std::cerr << "\nCan not open file \"" << dir_list[i] << "\"." << std::endl;

    }

//clean_end:
    for (unsigned p = 0; p < zip_passes.size(); p++) zip_passes[p].clear();
    zip_passes.clear();
    params.clear();
    positions.clear();
    return 0;
}
