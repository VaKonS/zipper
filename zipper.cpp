
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
const unsigned output_locale = 866; //65001; //1251; // if anything but CP_UTF8, OEM codepage will be used
const std::wstring zipExt = L".zip";
bool auto_passes = false;
HANDLE file_check = INVALID_HANDLE_VALUE;
HANDLE outfile = INVALID_HANDLE_VALUE;


// uninitialized data
bool show_passes, show_full, is_full, old_detection;
unsigned passes, begin, detect_threshold;
size_t mem_limit, mem_use;
std::wstring zipTempDir, zipInputDir, zipOutputDir, arcname_out, mmt, redefine, zipSingle, zip_cmd;
std::wstring arg_string[4]; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
char * locale_name;
std::vector<std::wstring> dir_list;
std::vector<bool> dir_subdirs;
/*
DIR* __cdecl __MINGW_NOTHROW opendir (const char*);
struct dirent* __cdecl __MINGW_NOTHROW readdir (DIR*);
int __cdecl __MINGW_NOTHROW closedir (DIR*);
void __cdecl __MINGW_NOTHROW rewinddir (DIR*);
long __cdecl __MINGW_NOTHROW telldir (DIR*);
void __cdecl __MINGW_NOTHROW seekdir (DIR*, long);

_WDIR* __cdecl __MINGW_NOTHROW _wopendir (const wchar_t*);
struct _wdirent* __cdecl __MINGW_NOTHROW _wreaddir (_WDIR*);
int __cdecl __MINGW_NOTHROW _wclosedir (_WDIR*);
void __cdecl __MINGW_NOTHROW _wrewinddir (_WDIR*);
long __cdecl __MINGW_NOTHROW _wtelldir (_WDIR*);
void __cdecl __MINGW_NOTHROW _wseekdir (_WDIR*, long);
//*/
_WDIR * dir_handle;
_wdirent * dir_entry;
std::vector<std::vector<char>> zip_storage;
std::vector<int> zip_indices; // zip_storage indices
std::vector<char> zip_pass1;
std::vector<unsigned> cycleN_count;
std::vector<bool> cycleN_match;
std::vector<unsigned> cycleN_sizes;
wchar_t wpath_buf[32768];
const size_t wpath_buf_length = sizeof(wpath_buf) / sizeof(wchar_t);
wchar_t wchar_buf[32768];
char    char_buf[32768 * 4]; //since CreateProcessW can modify command line, some characters can be appended?

MEMORYSTATUS mem_stat;
std::vector<int> params; // arg_string[] indices, -1 for pass number
std::vector<size_t> positions;

STARTUPINFO si = {
    sizeof(STARTUPINFO),  //DWORD   cb
    NULL,                 //LPTSTR lpReserved
    NULL,                 //LPTSTR lpDesktop
    NULL,                 //LPTSTR lpTitle
    0,                    //DWORD dwX
    0,                    //DWORD dwY
    0,                    //DWORD dwXSize
    0,                    //DWORD dwYSize
    0,                    //DWORD dwXCountChars
    0,                    //DWORD dwYCountChars
    0,                    //DWORD dwFillAttribute
    0,                    //STARTF_USESTDHANDLES, //DWORD dwFlags
    0,                    //WORD wShowWindow
    0,                    //WORD cbReserved2
    NULL,                 //LPBYTE lpReserved2
    INVALID_HANDLE_VALUE, //HANDLE hStdInput
    INVALID_HANDLE_VALUE, //HANDLE hStdOutput
    INVALID_HANDLE_VALUE  //HANDLE hStdError
};

PROCESS_INFORMATION pi = {
    INVALID_HANDLE_VALUE, //HANDLE hProcess
    INVALID_HANDLE_VALUE, //HANDLE hThread
    0,                    //DWORD dwProcessId
    0                     //DWORD dwThreadId
};


// ----------------------------------------------------------
void Close_File(HANDLE * fh) {
    if (*fh != INVALID_HANDLE_VALUE) {
        CloseHandle(*fh);
        *fh = INVALID_HANDLE_VALUE;
    }
}

int Close_All_Return(int exit_code) {
    Close_File(&file_check);
    Close_File(&outfile);
    return exit_code;
}

/*
std::wstring string2wstring(std::string i) {
    int l = i.size() + 1; //terminating 0
    const int b = sizeof(wchar_buf) / sizeof(wchar_t);
    if (MultiByteToWideChar(((output_locale == CP_UTF8)?CP_UTF8:CP_OEMCP), 0, i.c_str(), l, NULL, 0) > b) { //(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar)
        std::cerr << "string2wstring: wide characters buffer is too small." << std::endl;
        std::exit(Close_All_Return(EXIT_FAILURE));
    }
    MultiByteToWideChar(((output_locale == CP_UTF8)?CP_UTF8:CP_OEMCP), 0, i.c_str(), l, (LPWSTR) &wchar_buf, b);
    return std::wstring(wchar_buf);
}
//*/

std::string wstring2string(std::wstring i) {
    int l = i.size() + 1; //terminating 0
    const int b = sizeof(char_buf);
    if (WideCharToMultiByte(((output_locale == CP_UTF8)?CP_UTF8:CP_OEMCP), 0, i.c_str(), l, NULL, 0, NULL, NULL) > b) { //(UINT CodePage, DWORD dwFlags, LPWSTR lpWideCharStr, int cchWideChar, LPCSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
        std::cerr << "wstring2string: characters buffer is too small." << std::endl;
        std::exit(Close_All_Return(EXIT_FAILURE));
    }
    WideCharToMultiByte(((output_locale == CP_UTF8)?CP_UTF8:CP_OEMCP), 0, i.c_str(), l, (LPSTR) &char_buf, b, NULL, NULL);
    return std::string(char_buf);
}

std::wstring oemstring2wstring(std::string i) {
    int l = i.size() + 1; //terminating 0
    const int b = sizeof(wchar_buf) / sizeof(wchar_t);

    if (MultiByteToWideChar(CP_OEMCP, 0, i.c_str(), l, NULL, 0) > b) { //(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar)
        std::cerr << "oemstring2wstring: wide characters buffer is too small." << std::endl;
        std::exit(Close_All_Return(EXIT_FAILURE));
    }
    MultiByteToWideChar(CP_OEMCP, 0, i.c_str(), l, (LPWSTR) &wchar_buf, b);
    return std::wstring(wchar_buf);
}

std::string wstring2oemstring(std::wstring i) {
    int l = i.size() + 1; //terminating 0
    const int b = sizeof(char_buf);
    if (WideCharToMultiByte(CP_OEMCP, 0, i.c_str(), l, NULL, 0, NULL, NULL) > b) { //(UINT CodePage, DWORD dwFlags, LPWSTR lpWideCharStr, int cchWideChar, LPCSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
        std::cerr << "wstring2oemstring: characters buffer is too small." << std::endl;
        std::exit(Close_All_Return(EXIT_FAILURE));
    }
    WideCharToMultiByte(CP_OEMCP, 0, i.c_str(), l, (LPSTR) &char_buf, b, NULL, NULL);
    return std::string(char_buf);
}

size_t wPath_length(std::wstring i) { // drive's "current directory" (i. e. "a:" without slash) will not be found
    size_t s = i.find_last_of(L'/');
    size_t b = i.find_last_of(L'\\');
    if (s == std::wstring::npos) s = b;
    if (b != std::wstring::npos) if (b > s) s = b;
    if (s != std::wstring::npos) s++; else s = 0;
    return s;
}

/*
std::wstring wShortPath(std::wstring pn) {
    std::wstring p = pn.substr(0, wPath_length(pn));
    if (GetShortPathNameW(p.c_str(), (LPWSTR) &wpath_buf, wpath_buf_length) > wpath_buf_length) { //(lpszLongPath,lpszShortPath,cchBuffer)
        std::cerr << "Path buffer is too small." << std::endl;
        std::exit(Close_All_Return(EXIT_FAILURE));
    }
    return std::wstring(wpath_buf);
}
//*/

std::wstring wTrailSlash(std::wstring s) {
    unsigned c = s.size();
    if (c == 0) return L"";
    c = s[c - 1];
    if ((c == L'\\') or (c == L'/')) return s;
    return s + L"\\";
}

std::string papl(unsigned n) {return (n == 1) ? " pass" : " passes";}
std::string bypl(unsigned n) {return (n == 1) ? "byte" : "bytes";}

int main(int argc, char** argv) {

    if (output_locale == CP_UTF8) {
        SetConsoleOutputCP(CP_UTF8);
        //SetConsoleCP(CP_UTF8);
        setlocale(LC_ALL, ".65001");
    } else {
        //SetConsoleOutputCP(GetOEMCP());
        //SetConsoleCP(GetOEMCP());
        setlocale(LC_ALL, ("." + std::to_string(GetOEMCP())).c_str());
    }
    //system("graftabl");
    locale_name = setlocale(LC_ALL, NULL);
    std::cout << "Locale: " << ((locale_name == NULL)?"error getting locale name.":std::string(locale_name)) << std::endl;

    if (GetEnvironmentVariableW(L"ProgramFiles", (LPWSTR)&wpath_buf, wpath_buf_length) >= wpath_buf_length) {
        std::cerr << "GetEnvironmentVariable: wide characters buffer is too small." << std::endl;
        return Close_All_Return(EXIT_FAILURE);
    };
    arg_string[0] = std::wstring(wpath_buf) + L"\\7-Zip\\7z.exe";

    // definition of command line arguments   abcdefghijklmnopqrstuvwxyz
    //                                        abcd.f..i..lmnop.rst......
    TCLAP::CmdLine cmd("Zipper: checks different number of compression passes for 7-Zip ZIP archives.", ' ', "Zipper v1.5");

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
                    "Pathname to 7-Zip archiver. [" + wstring2oemstring(arg_string[0]) + "]", false,
                    wstring2oemstring(arg_string[0]), "string", cmd);

    TCLAP::ValueArg<int> cmdPasses("p", "passes",
                    "Passes limit, set to 0 for unlimited search. [100]", false,
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
        return Close_All_Return(EXIT_FAILURE);
    }

    // command line accepted, begin console processing
    arg_string[0]      = oemstring2wstring(cmd7Zip.getValue()); // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
    zipTempDir         = oemstring2wstring(cmdTempDir.getValue());
    zipInputDir        = oemstring2wstring(cmdInputDir.getValue());
    zipOutputDir       = oemstring2wstring(cmdOutputDir.getValue());
    passes             = std::max(0, cmdPasses.getValue());
    begin              = std::max(cmdStart.getValue(), 1);
    mem_limit          = cmdMemLimit.getValue() << 20;
    mmt                = oemstring2wstring(cmdMMT.getValue());
    show_passes        = cmdShowPasses.getValue();
    show_full          = cmdShowFull.getValue();
    redefine           = oemstring2wstring(cmdRedefine.getValue());
    zipSingle          = oemstring2wstring(cmdSingle.getValue());
    old_detection      = cmdOld.getValue();
    detect_threshold   = std::max((unsigned) 1, std::min((unsigned) cmdDetect.getValue(), ((passes != 0) ? (unsigned) passes : (unsigned) -1)));
    if (passes < 1) {
        auto_passes = true;
        passes = std::max(576, (int) detect_threshold) * 3 + begin - 1; // 24*24*3
    }
    //std::cout << "passes: " << passes << std::endl;
    //std::cout << "detect_threshold: " << detect_threshold << std::endl;

    if (begin > passes) {
        std::cerr << "\nStarting number of passes is greater than total number (" << begin << " > " << passes << "). Stop." << std::endl;
        return Close_All_Return(EXIT_FAILURE);
    }

    //arg_string[0] = wShortPath(arg_string[0]) + L"7z.exe"; //archiver's short pathname
    std::cout << "Archiver: \"" << wstring2oemstring(arg_string[0]) << "\"." << std::endl;

    arg_string[3] = L"%"; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %

    // parsing command template
    params.clear();
    positions.clear();
    if (redefine.size() != 0) {
        size_t l = 0;
        while (l < redefine.npos) {
            size_t a = redefine.find(L"%i", l);
            size_t b = redefine.find(L"%o", l);
            size_t c = redefine.find(L"%p", l);
            size_t d = redefine.find(L"%c", l);
            size_t e = redefine.find(L"%%", l);
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

    zipInputDir = wTrailSlash(zipInputDir);
    zipOutputDir = wTrailSlash(zipOutputDir);
    if (zipTempDir.size() == 0) zipTempDir = zipOutputDir; else zipTempDir = wTrailSlash(zipTempDir);

    // making recursive input directory listing
    dir_list.push_back(L"");
    dir_subdirs.push_back(true);
    int subdir_index = 0;
    while (subdir_index != -1) {
        std::wstring recursiveDir = wTrailSlash(zipInputDir + dir_list[subdir_index]);
        dir_handle = _wopendir(recursiveDir.c_str());
        if (dir_handle != NULL) {
            std::cout << "Opened directory \"" << wstring2string(recursiveDir) << "\"." << std::endl;
            while ((dir_entry = _wreaddir(dir_handle)) != NULL) {
                std::wstring n(dir_entry->d_name);
                int t = GetFileAttributesW((recursiveDir + n).c_str());
                if (t == -1) {
                    _wclosedir(dir_handle);
                    std::cerr << "\nError reading attributes of \"" << wstring2string(recursiveDir + n) << "\"." << std::endl;
                    return Close_All_Return(EXIT_FAILURE);
                }
                if (((t & FILE_ATTRIBUTE_DIRECTORY) == 0) || ((n != L".") && (n != L".."))) { // file or dir except ./..
                    dir_list.push_back(wTrailSlash(dir_list[subdir_index]) + n);
                    dir_subdirs.push_back((t & FILE_ATTRIBUTE_DIRECTORY) != 0);
                }
            }
            _wclosedir(dir_handle);
        } else {
            std::cerr << "\nError listing directory \"" << wstring2string(recursiveDir) << "\"." << std::endl;
            return Close_All_Return(EXIT_FAILURE);
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
        std::cout << "\n-------------------------------------\nFile: " << wstring2string(arg_string[1]) << std::endl;
        file_check = CreateFileW(arg_string[1].c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); //(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDistribution, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
        if (file_check != INVALID_HANDLE_VALUE) {
            Close_File(&file_check);

            arg_string[2] = zipTempDir + dir_list[i].substr(wPath_length(dir_list[i])) + zipExt; // input name without path
            std::cout << "Testing: " << wstring2string(arg_string[2]) << std::endl;

            for (unsigned p = 0; p < zip_storage.size(); p++) zip_storage[p].clear(); // freeing memory
            zip_storage.clear();
            zip_indices.assign(passes, -1); // resetting indices
            mem_use = 0;
            is_full = false;
            unsigned minimal_zip_length = (unsigned) -1;
            unsigned minimal_zip_passes = 0;
            unsigned match_counter = 0;
            bool matched_once = false;
            unsigned pass_counter = 0;
            unsigned cycle_size = 0;
            unsigned max_cycle = 0;
            unsigned max_cycle_start = 0;
            unsigned max_cycle_multiple = 0;
            unsigned last_cycle_size = 0;
            unsigned last_cycle_start = 0;
            unsigned min_cycle = minimal_zip_length; // max
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
                    zip_cmd = L"\"" + arg_string[0] + L"\" a -tzip -mx=9 -mmt=" + mmt + L" -mtc=off -mfb=258 -mpass=" + std::to_wstring(p + 1)
                              + L" \"" + arg_string[2] + L"\" \"" + arg_string[1] + L"\"" + std::wstring(32768, L'\0');
                } else {
                    zip_cmd = redefine;
                    for (int a = positions.size() - 1; a >= 0; a--) {
                        zip_cmd.erase(positions[a], 2);
                        if (params[a] != -1) {
                            zip_cmd.insert(positions[a], arg_string[params[a]]);
                        } else {
                            zip_cmd.insert(positions[a], std::to_wstring(p + 1));
                        }
                    }
                    zip_cmd += std::wstring(32768, L'\0');
                }
                std::cout << (char*)wstring2string(zip_cmd).c_str() << std::endl;
                CreateProcessW(NULL, (LPWSTR)zip_cmd.c_str(), NULL, NULL, false, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi); //(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFO lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
                CloseHandle(pi.hThread);
                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hProcess);

                file_check = CreateFileW(arg_string[2].c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                if (file_check != INVALID_HANDLE_VALUE) {
                    unsigned zip_length = GetFileSize(file_check, NULL); //(HANDLE hFile, LPDWORD lpdwFileSizeHigh)
                    if (zip_length == (unsigned) -1) {
                        std::cerr << "\nCan not get length of archive." << std::endl;
                        zip_length = 0;
                    }
                    GlobalMemoryStatus(&mem_stat);
                    if (((mem_use + zip_length) > mem_limit) || (static_cast<size_t>(zip_length) > mem_stat.dwAvailPhys)) {
                        std::cout << "\nNo memory left to store archives.\nChoosing from what was saved so far..." << std::endl;
                        Close_File(&file_check);
                        if (zipSingle.size() != 0) { // when packing to single archive, memory storage is not used and smaller size can still be packed
                            if (zip_length > 0) if (zip_length < minimal_zip_length) {
                                pass_counter = p + 1;
                                minimal_zip_length = zip_length;
                                minimal_zip_passes = pass_counter;
                            }
                        }
                        goto passes_checked;
                    }
                    unsigned m = std::max((unsigned)0, std::min( (mem_limit - mem_use - zip_length), (static_cast<unsigned>(mem_stat.dwAvailPhys) - zip_length) ) ) >> 20;
                    std::cout << "Memory left: " << m << " mega" << bypl(m) << std::endl;
                    if (zip_length > 0) {
                        pass_counter = p + 1;
                        zip_pass1.resize(zip_length);
                        unsigned bytes_read;
                        ReadFile(file_check, zip_pass1.data(), zip_length, (LPDWORD)&bytes_read, NULL); //(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
                        Close_File(&file_check);
                        std::cout << "Removing \"" << wstring2string(arg_string[2]) << "\"." << std::endl;
                        DeleteFileW(arg_string[2].c_str());
                        int add_index = -1;
                        if (!old_detection) cycleN_match.assign(passes, false);
                        for (int c = p - 1; c >= 0; c--) {
                            if (zip_indices[c] != -1) { // for skipped passes
                                if (zip_storage[zip_indices[c]].size() == static_cast<size_t>(zip_length)) {
                                    if (memcmp(zip_storage[zip_indices[c]].data(), zip_pass1.data(), zip_length) == 0) {
                                        add_index = c;
                                        if (old_detection) {
                                            match_counter++;
                                            matched_once = true;
                                            cycle_size = detect_threshold;
                                            if (match_counter == detect_threshold) {
                                                is_full = true;
                                                std::cout << "Matched archives: " << match_counter << "/" << detect_threshold << ". Search complete." << std::endl;
                                                goto passes_checked;
                                            } else {
                                                //std::cout << "Match, referencing previous copy." << std::endl;
                                                zip_indices[p] = zip_indices[add_index];
                                                goto sample_added;
                                            }
                                        } else { // new detection
                                            cycle_size = p - c;
                                            if (cycle_size >= detect_threshold) {
                                                cycleN_match[cycle_size - 1] = true;
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
                                        unsigned c1 = c + 1;
                                        //std::cout << "cycleN_match[" << c1 << "]" << std::endl;
                                        cycleN_count[c]++;
                                        //std::cout << "cycleN_count[c]: " << cycleN_count[c] << std::endl;
                                        if (line_start) {
                                            // checking minimal cycle sizes array
                                            for (unsigned k = 0; k < passes; k++) {
                                                unsigned cs = cycleN_sizes[k];
                                                //std::cout << "cycleN_sizes[" << k << "]: " << cs << std::endl;
                                                if (cs == 0) { // end of array, new minimal cycle found
                                                    cycleN_sizes[k] = c1;
                                                    // searching minimal multiple of all sizes
                                                    unsigned multiple = c1;
                                                    for (unsigned l = 0; l < k; l++) {
                                                        for (unsigned n = 1; n <= c1; n++) {
                                                            unsigned m = cycleN_sizes[l] * n;
                                                            if ((m % c1) == 0) {
                                                                if (m > multiple) multiple = m;
                                                                break;
                                                            }
                                                        }
                                                    }
                                                    if (multiple > max_cycle) {
                                                        max_cycle = multiple;
                                                        if (multiple > max_cycle_multiple) max_cycle_multiple = multiple;
                                                        max_cycle_start = last_cycle_start; // for prediction use last minimal cycle start until it will be updated by match, it seems to be correct
                                                    }
                                                    break;
                                                }
                                                if (cs == c1) break; // minimal cycle size is already present
                                            }
                                            line_start = false;
                                            std::cout << "Possible cycle(s): ";
                                        } else
                                            std::cout << ", ";
                                        std::cout << cycleN_count[c] << "/" << c1;
                                        if (max_cycle == c1) { // cycle is estimated
                                            max_cycle_start = p - c - cycleN_count[c];
                                            if (cycleN_count[c] == 1) { // cycle is started
                                                if (c1 < min_cycle) min_cycle = c1; // minimal cycle size
                                                if (c1 > last_cycle_size) { // started 1st time
                                                    last_cycle_size = c1;
                                                    last_cycle_start = (p + 1) % min_cycle; last_cycle_start = ((p + 1) / min_cycle + (((last_cycle_start * 2) >= min_cycle) ? 1 : 0)) * min_cycle - c1 - 1; //round()
                                                    //std::cout << std::endl << last_cycle_start << ", ";
                                                    //last_cycle_start = std::round(double(p + 1) / min_cycle) * min_cycle - c1 - 1; //max_cycle_start; // minimal cycle start
                                                    //std::cout << last_cycle_start << ", " << max_cycle_start << std::endl;
                                                }
                                            }
                                        }
                                        if (c1 > max_cycle_multiple) if ((c1 % max_cycle) == 0) max_cycle_multiple = c1;
                                    } else
                                        cycleN_count[c] = 0;
                                }
                                if (!line_start) std::cout << "." << std::endl;
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
sample_added:           ; //nop
                    } else
                        Close_File(&file_check);
                } else
                    std::cerr << "\nCan not open archive \"" << wstring2string(arg_string[2]) << "\"." << std::endl;
                if (max_cycle > 0) std::cout << "Estimated cycle: " << max_cycle << ", total passes: " << (max_cycle_start + max_cycle_multiple * 2) << "." << std::endl;
                if (matched_once) std::cout << "Matched archives: " << ((match_counter == 0) ? "-" : std::to_string(match_counter)) << "/" << detect_threshold << std::endl;
                p++;
                if (auto_passes and (p >= passes)) {
                    unsigned prev_passes = passes;
                    passes = (passes + max_cycle_start + max_cycle) * 3;
                    //std::cout << "New passes maximum: " << passes << std::endl;
                    zip_indices.resize(passes);
                    for (unsigned l = prev_passes; l < passes; l++) zip_indices[l] = -1;
                    if (!old_detection) {
                        cycleN_count.resize(passes);
                        cycleN_match.resize(passes);
                        cycleN_sizes.resize(passes);
                        //for (unsigned l = prev_passes; l < passes; l++) {
                        //    cycleN_count[l] = 0;
                        //    cycleN_match[l] = 0;
                        //    cycleN_sizes[l] = 0;
                        //}
                    }
                }
            }
passes_checked:
            std::cout << "Cleaning \"" << wstring2string(arg_string[2]) << "\"." << std::endl;
            DeleteFileW(arg_string[2].c_str());
            if (minimal_zip_passes != 0) {
                std::cout << "Minimum archive size: " << minimal_zip_length << " " << bypl(minimal_zip_length) << " (" << (minimal_zip_passes) << papl(minimal_zip_passes) << ")." << std::endl;
            } else {
                std::cout << "No archives were created." << std::endl;
            }

            if (zipSingle.size() == 0) {
                arcname_out = zipOutputDir + dir_list[i];
                if (minimal_zip_passes != 0) {
                    int d = std::floor(std::log10(passes)) + 1;
                    if (auto_passes) d = std::max(8, d); //d = auto_passes ? std::max(8, d) : d;
                    std::wstring f = L"%0" + std::to_wstring(d) + L"u";
                    if (show_passes) {
                        swprintf((wchar_t*)&wpath_buf, wpath_buf_length, f.c_str(), minimal_zip_passes);
                        arcname_out = arcname_out + L".best" + std::wstring(wpath_buf);
                        swprintf((wchar_t*)&wpath_buf, wpath_buf_length, f.c_str(), pass_counter);
                        arcname_out = arcname_out + L".of" + std::wstring(wpath_buf);
                    }
                    if (show_full) {
                        if (old_detection) {
                            swprintf((wchar_t*)&wpath_buf, wpath_buf_length, f.c_str(), match_counter);
                            arcname_out = arcname_out + L".match" + (is_full ? std::wstring(wpath_buf) : L"-");
                        } else {
                            swprintf((wchar_t*)&wpath_buf, wpath_buf_length, f.c_str(), cycle_size);
                            arcname_out = arcname_out + L".cycle" + (is_full ? std::wstring(wpath_buf) : L"-");
                        }
                    }
                    arcname_out += zipExt;
                    std::wstring out_subdir = arcname_out.substr(0, wPath_length(arcname_out) - 1);
                    if (GetFileAttributesW(out_subdir.c_str()) == (unsigned) -1) {
                        std::cout << "Creating subdirectory \"" << wstring2string(out_subdir) << "\"." << std::endl;
                        if (CreateDirectoryW(out_subdir.c_str(), NULL) == 0) { //(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
                            std::cerr << "\nError making directory \"" << wstring2string(out_subdir) << "\"." << std::endl;
                        }
                    }
                    std::cout << "Writing \"" << wstring2string(arcname_out) << "\"." << std::endl;
                    outfile = CreateFileW(arcname_out.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                    if (outfile == INVALID_HANDLE_VALUE) {
                        std::cerr << "\nError writing archive \"" << wstring2string(arcname_out) << "\"." << std::endl;
                    } else {
                        unsigned bytes_written;
                        WriteFile(outfile, zip_storage[zip_indices[minimal_zip_passes - 1]].data(), minimal_zip_length, (LPDWORD)&bytes_written, NULL); //(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
                        Close_File(&outfile);
                    }
                } else
                    std::cerr << "\nError compressing archive \"" << wstring2string(arcname_out) << ".#.#.#.zip\"." << std::endl;
            } else { // single archive
                if (minimal_zip_passes != 0) {
                    arcname_out = zipOutputDir + zipSingle;
                    if (redefine.size() == 0) {
                        //TODO: how to add *single* "path\file" to the archive with relative path kept?
                        zip_cmd = L"\"" + arg_string[0] + L"\" a -tzip -mx=9 -mmt=" + mmt + L" -mtc=off -mfb=258 -mpass=" + std::to_wstring(minimal_zip_passes)
                                  + L" \"" + arcname_out + L"\" \"" + arg_string[1] + L"\"" + std::wstring(32768, L'\0');
                    } else {
                        zip_cmd = redefine;
                        for (int a = positions.size() - 1; a >= 0; a--) {
                            zip_cmd.erase(positions[a], 2);
                            switch (params[a]) {
                            case -1: // pass
                                zip_cmd.insert(positions[a], std::to_wstring(minimal_zip_passes));
                            break;
                            case 2:  // output archive
                                zip_cmd.insert(positions[a], arcname_out);
                            break;
                            default: // archiver, input file, %
                                zip_cmd.insert(positions[a], arg_string[params[a]]);
                            break;
                            }
                        }
                        zip_cmd += std::wstring(32768, L'\0');
                    }
                    std::cout << "Adding to \"" << wstring2string(arcname_out) << "\"." << std::endl;
                    std::cout << (char*)wstring2string(zip_cmd).c_str() << std::endl;
                    CreateProcessW(NULL, (LPWSTR)zip_cmd.c_str(), NULL, NULL, false, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi); //(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFO lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
                    CloseHandle(pi.hThread);
                    WaitForSingleObject(pi.hProcess, INFINITE);
                    CloseHandle(pi.hProcess);
                } else {
                    std::cerr << "\nError compressing file \"" << wstring2string(arg_string[1]) << "\"." << std::endl;
                    break; // interrupt creation of single archive
                }
            }
        } else
            std::cerr << "\nCan not open file \"" << wstring2string(arg_string[1]) << "\"." << std::endl;
    }

//clean_end:
    zip_indices.clear();
    cycleN_count.clear();
    cycleN_match.clear();
    for (unsigned p = 0; p < zip_storage.size(); p++) zip_storage[p].clear();
    zip_storage.clear();
    params.clear();
    positions.clear();
    if (output_locale == CP_UTF8) {
        SetConsoleOutputCP(GetOEMCP());
        //SetConsoleCP(GetOEMCP());
    }
    return Close_All_Return(0);;
}
