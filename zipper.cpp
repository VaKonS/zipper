
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
unsigned passes, detect_threshold;
size_t mem_limit, mem_use;
std::string SevenZip, zipTempDir, zipInputDir, zipOutputDir, arcname, mmt;
std::vector<std::string> dir_list;
DIR * dir_handle;
dirent * dir_entry;
std::ifstream file_check;
std::ofstream outfile;
std::vector<std::vector<char>> zip_passes;
char path_buf[2048];
MEMORYSTATUS mem_stat;


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
    TCLAP::CmdLine cmd("Zipper: checks different number of compression passes for 7-Zip's zip archives.", ' ', "1.0");

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
    mem_limit          = cmdMemLimit.getValue() << 20;
    mmt                = cmdMMT.getValue();
    show_passes        = cmdShowPasses.getValue();
    detect_threshold   = cmdDetect.getValue();

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

    zip_passes.resize(passes);
    mem_stat.dwLength = sizeof(mem_stat);
    for (unsigned i = 0; i < dir_list.size(); i++) if ((dir_list[i] != ".") && (dir_list[i] != "..")) {
        std::cout << "File: " << dir_list[i] << std::endl;
        file_check = std::ifstream(zipInputDir + dir_list[i], std::ifstream::binary);
        if (file_check) {
            file_check.close();
            for (unsigned p = 0; p < passes; p++) zip_passes[p].clear();
            zip_passes.clear();
            zip_passes.resize(passes);
            mem_use = 0;
            unsigned match_counter = 0;
            arcname = zipTempDir + dir_list[i] + zipExt;
            std::cout << "Testing: " << arcname << std::endl;
            for (unsigned p = 0; p < passes; p++) {
                std::cout << "-------------------------------------\nPasses: " << p + 1 << "/" << passes << std::endl;
                //@for %%i in ("*.rdr" "*.geerdr" "*.drawinghand") do @for /L %%k in (1,1,80) do @"c:\Program Files\7-Zip\7z.exe" a -tzip -mx=9 -mmt=off -mtc=off -mfb=258 -mpass=%%k "%%~di%%~pizip\%%~ni%%~xi.%%k.zip" "%%i"
                std::string zip_cmd = SevenZip + " a -tzip -mx=9 -mmt=" + mmt + " -mtc=off -mfb=258 -mpass=" + std::to_string(p + 1) +
                " \"" + arcname + "\" \"" + zipInputDir + dir_list[i] + "\"";
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
                        zip_passes[p].resize(zip_length); // allocate memory
                        mem_use += zip_length;
                        file_check.read(zip_passes[p].data(), zip_length); // read zip to memory
                    }
                    file_check.close();
                } else
                    std::cerr << "\nCan not open archive \"" << arcname << "\"." << std::endl;
                for (unsigned c = 0; c < p; c++) {
                    if (zip_passes[c].size() == zip_passes[p].size()) {
                        if (memcmp(zip_passes[c].data(), zip_passes[p].data(), zip_passes[c].size()) == 0) {
                            match_counter++;
                            std::cout << "Matched archives: " << match_counter << "/" << detect_threshold << std::endl;
                            if (match_counter == detect_threshold) { goto passes_checked; } else { goto pass_matched; };
                        }
                    }
                }
                match_counter = 0;
pass_matched:
                match_counter = match_counter; // "nop", because label before "}" causes error
            }
passes_checked:
            unsigned min_zip_length = 1 << 30;
            unsigned zip_index = 0;
            match_counter = 0;
            for (int p = passes - 1; p >= 0; p--) if (zip_passes[p].size() != 0) {
                if (match_counter == 0) match_counter = p + 1;
                if (zip_passes[p].size() <= min_zip_length) {
                    min_zip_length = zip_passes[p].size();
                    zip_index = p;
                }
            }
            std::cout << "Minimum archive size: " << min_zip_length << " bytes." << std::endl;
            std::cout << "Removing \"" << arcname.c_str() << "\"." << std::endl;
            remove(arcname.c_str());

            arcname = zipOutputDir + dir_list[i];
            if (show_passes) {
                int d = std::floor(std::log10(passes)) + 1;
                std::string f = "%0" + std::to_string(d) + "u";
                snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), (zip_index + 1));
                arcname = arcname + ".best" + std::string(path_buf);
                snprintf((char*)&path_buf, sizeof(path_buf), f.c_str(), match_counter);
                arcname = arcname + ".of" + std::string(path_buf);
            }
            arcname += zipExt;
            if (min_zip_length != (unsigned)(1 << 30)) {
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
    for (unsigned p = 0; p < passes; p++) zip_passes[p].clear();
    zip_passes.clear();
    return 0;
}
