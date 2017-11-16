
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
std::string SevenZip, zipTempDir, zipInputDir, zipOutputDir, arcname, mmt, redefine;
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

int main(int argc, char** argv) {

    // definition of command line arguments
    TCLAP::CmdLine cmd("Zipper: checks different number of compression passes for 7-Zip ZIP archives.", ' ', "1.1");

    TCLAP::ValueArg<std::string> cmdInputDir("i", "input-mask",
                    "Directory with files to compress.\nRun Zipper from this directory to avoid paths inside archives. [.]", false,
                    ".", "string", cmd);

    TCLAP::ValueArg<std::string> cmdOutputDir("o", "output-dir",
                    "Directory for zipped files.\nUse \"/\" or \"\\\\\" instead of \"\\\".", true,
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
                    "Fully redefine command line.\nPass arguments as %c, \\\"%i\\\", \\\"%o\\\", \\\"%p\\\".", false,
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
        std::cout << "File: " << dir_list[i] << std::endl;
        file_check = std::ifstream(zipInputDir + dir_list[i], std::ifstream::binary);
        if (file_check) {
            file_check.close();
            for (unsigned p = 0; p < zip_passes.size(); p++) zip_passes[p].clear();
            zip_passes.clear(); // freeing memory
            zip_passes.resize(passes);
            mem_use = 0;
            unsigned match_counter = 0;
            unsigned pass_counter = 0;
            arcname = zipTempDir + dir_list[i] + zipExt;
            std::cout << "Testing: " << arcname << std::endl;

            if (redefine.size() != 0) {
                size_t l = 0;
                while (l < redefine.npos) {
                    size_t a = redefine.find("%i", l);
                    size_t b = redefine.find("%o", l);
                    size_t c = redefine.find("%p", l);
                    size_t d = redefine.find("%c", l);
                    //std::cout << "a, b, c, d: " << a << ", " << b << ", " << c << ", " << d << std::endl;
                    l = std::min(std::min(std::min(a, b), c), d);
                    //std::cout << "l: " << l << std::endl;
                    if (l < redefine.npos) {
                        if      (l == a) params.push_back(zipInputDir + dir_list[i]);
                        else if (l == b) params.push_back(arcname);
                        else if (l == c) params.push_back("");
                        else if (l == d) params.push_back(SevenZip);
                        positions.push_back(l);
                        l = l + 2;
                    }

                }
                //std::cout << "params.size(): " << params.size() << std::endl;
                //std::cout << "positions.size(): " << positions.size() << std::endl;
            }

            for (unsigned p = begin - 1; p < passes; p++) {
                pass_counter = p + 1;
                std::cout << "-------------------------------------\nPasses: " << pass_counter << "/" << passes << std::endl;
                std::string zip_cmd;
                if (redefine == "") {
                    //@for %%i in ("*.rdr" "*.geerdr" "*.drawinghand") do @for /L %%k in (1,1,80) do @"c:\Program Files\7-Zip\7z.exe" a -tzip -mx=9 -mmt=off -mtc=off -mfb=258 -mpass=%%k "%%~di%%~pizip\%%~ni%%~xi.%%k.zip" "%%i"
                    zip_cmd = SevenZip + " a -tzip -mx=9 -mmt=" + mmt + " -mtc=off -mfb=258 -mpass=" + std::to_string(p + 1)
                              + " \"" + arcname + "\" \"" + zipInputDir + dir_list[i] + "\"";
                } else {
                    //std::cout << "zip_cmd begin." << std::endl;
                    zip_cmd = redefine;
                    //std::cout << "         012345678901234567890123456789012345678901234567890123456789012345678901234567890" << std::endl;
                    //std::cout << "zip_cmd: " << zip_cmd << std::endl;
                    for (int a = positions.size() - 1; a >= 0; a--) {
                        //std::cout << "a: " << a << std::endl;
                        //std::cout << "positions[a]: " << positions[a] << std::endl;
                        //std::cout << "params[a]: " << params[a] << std::endl;
                        zip_cmd.erase(positions[a], 2);
                        //std::cout << "         012345678901234567890123456789012345678901234567890123456789012345678901234567890" << std::endl;
                        //std::cout << "zip_cmd: " << zip_cmd << std::endl;
                        if (params[a].size() != 0) {
                            zip_cmd.insert(positions[a], params[a]);
                        } else {
                            zip_cmd.insert(positions[a], std::to_string(p + 1));
                        }
                        //std::cout << "         012345678901234567890123456789012345678901234567890123456789012345678901234567890" << std::endl;
                        //std::cout << "zip_cmd: " << zip_cmd << std::endl;
                    }
                    //zip_cmd = SevenZip + " " + zip_cmd;
                }
                //std::cout << "zip_cmd done." << std::endl;
                std::cout << zip_cmd.c_str() << std::endl;
                system(zip_cmd.c_str());
                file_check = std::ifstream(arcname, std::ifstream::binary);
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
                        goto passes_checked;
                    }
                    std::cout << "Memory left: " << (std::max(std::min( (static_cast<int>(mem_limit) - static_cast<int>(mem_use) - zip_length), (static_cast<int>(mem_stat.dwAvailPhys) - zip_length) ), 0) >> 20) << " megabytes" << std::endl;
                    if (zip_length > 0) {
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
                        zip_passes[p] = zip_pass1; // new archive sample, adding to array
                        mem_use += zip_length;
                        match_counter = 0;
pass_matched:
                        match_counter = match_counter; // "nop", because label before "}" causes error
                    } else
                        file_check.close();
                } else
                    std::cerr << "\nCan not open archive \"" << arcname << "\"." << std::endl;
            }
passes_checked:
            unsigned min_zip_length = (unsigned) -1;
            unsigned zip_index = 0;
            for (int p = passes - 1; p >= 0; p--) if (zip_passes[p].size() != 0) {
                if (zip_passes[p].size() <= min_zip_length) {
                    min_zip_length = zip_passes[p].size();
                    zip_index = p;
                }
            }
            if (min_zip_length != (unsigned) -1) {
                std::cout << "Minimum archive size: " << min_zip_length << " bytes." << std::endl;
            } else {
                std::cout << "No archives were created." << std::endl;
            }
            std::cout << "Removing \"" << arcname.c_str() << "\"." << std::endl;
            remove(arcname.c_str());

            arcname = zipOutputDir + dir_list[i];
            if (show_passes) {
                int d = std::floor(std::log10(passes)) + 1;
                std::string f = "%0" + std::to_string(d) + "u";
                snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), (zip_index + 1));
                arcname = arcname + ".best" + std::string(path_buf);
                snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), pass_counter);
                arcname = arcname + ".of" + std::string(path_buf);
            }
            arcname += zipExt;
            if (min_zip_length != (unsigned) -1) {
                std::cout << "Writing \"" << arcname << "\"." << std::endl;
                outfile = std::ofstream(arcname, std::ofstream::binary);
                if (!outfile) {
                    std::cerr << "\nError writing archive \"" << arcname << "\"." << std::endl;
                } else {
                    outfile.write(zip_passes[zip_index].data(), min_zip_length); // save smallest archive
                    outfile.close();
                }
            } else
                std::cerr << "\nError compressing archive \"" << arcname << "\"." << std::endl;
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
