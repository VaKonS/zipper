
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
const int output_locale = 866; //65001; //1251; // if anything but CP_UTF8, OEM codepage will be used
const std::wstring SevenZipSubPath = L"7-Zip\\7z.exe";
const std::wstring zipExt = L".zip";
bool auto_passes = false, old_detection = false, full_check = false;
HANDLE file_check = INVALID_HANDLE_VALUE;
HANDLE outfile = INVALID_HANDLE_VALUE;
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
MEMORYSTATUS mem_stat = {
    sizeof(MEMORYSTATUS), //DWORD dwLength
    0,                    //DWORD dwMemoryLoad
    0,                    //DWORD dwTotalPhys
    0,                    //DWORD dwAvailPhys
    0,                    //DWORD dwTotalPageFile
    0,                    //DWORD dwAvailPageFile
    0,                    //DWORD dwTotalVirtual
    0                     //DWORD dwAvailVirtual
};


// uninitialized data (declared here to free stack)
bool show_passes, show_full, is_full, debug_output;
unsigned passes, begin, detect_threshold, detect_threshold_current;
unsigned minimal_zip_length, minimal_zip_passes;
unsigned pass_counter;
bool matched_once, matched_pass;
unsigned match_counter;
unsigned cycle_start, cycle_size, cycle_max_size, cycleNsizes_count;
size_t mem_limit, mem_use;
time_t StartTime;
std::wstring zipTempDir, zipInputDir, zipOutputDir, arcname_out, mmt, redefine, zipSingle, zip_cmd;
std::wstring arg_string[4]; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
std::vector<int> params; // arg_string[] indices, -1 for pass number
std::vector<size_t> positions;
_WDIR * dir_handle;
_wdirent * dir_entry;
std::vector<std::wstring> dir_list;
std::vector<bool> dir_subdirs;
char * locale_name;
std::vector<char> zip_pass1;
char     char_buf[32768 * 2];
wchar_t wchar_buf[32768];
const size_t wchar_buf_length = sizeof(wchar_buf) / sizeof(wchar_t);
std::vector<std::vector<char>> zip_storage;
std::vector<int> zip_indices; // zip_storage indices
std::vector<bool> cycleN_match;
std::vector<unsigned> cycleN_count, cycleN_sizes;
std::vector<int> cycleN_start;


// ----------------------------------------------------------
void resizeArraysToPasses() { // iczsm
    zip_indices.resize(passes);
    cycleN_count.resize(passes);
    cycleN_sizes.resize(passes);
    cycleN_start.resize(passes);
    cycleN_match.resize(passes);
}

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
    if (MultiByteToWideChar(((output_locale == CP_UTF8)?CP_UTF8:CP_OEMCP), 0, i.c_str(), l, NULL, 0) > wchar_buf_length) { //(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar)
        std::cerr << "string2wstring: wchar_buf is too small." << std::endl;
        std::exit(Close_All_Return(EXIT_FAILURE));
    }
    MultiByteToWideChar(((output_locale == CP_UTF8)?CP_UTF8:CP_OEMCP), 0, i.c_str(), l, (LPWSTR) &wchar_buf, wchar_buf_length);
    return std::wstring(wchar_buf);
}
//*/

std::string wstring2string(std::wstring i) {
    int l = i.size() + 1; //terminating 0
    const int b = sizeof(char_buf);
    if (WideCharToMultiByte(((output_locale == CP_UTF8)?CP_UTF8:CP_OEMCP), 0, i.c_str(), l, NULL, 0, NULL, NULL) > b) { //(UINT CodePage, DWORD dwFlags, LPWSTR lpWideCharStr, int cchWideChar, LPCSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
        std::cerr << "wstring2string: char_buf is too small." << std::endl;
        std::exit(Close_All_Return(EXIT_FAILURE));
    }
    WideCharToMultiByte(((output_locale == CP_UTF8)?CP_UTF8:CP_OEMCP), 0, i.c_str(), l, (LPSTR) &char_buf, b, NULL, NULL);
    return std::string(char_buf);
}

std::wstring oemstring2wstring(std::string i) {
    int l = i.size() + 1; //terminating 0
    if (MultiByteToWideChar(CP_OEMCP, 0, i.c_str(), l, NULL, 0) > (int)wchar_buf_length) { //(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar)
        std::cerr << "oemstring2wstring: wchar_buf is too small." << std::endl;
        std::exit(Close_All_Return(EXIT_FAILURE));
    }
    MultiByteToWideChar(CP_OEMCP, 0, i.c_str(), l, (LPWSTR) &wchar_buf, wchar_buf_length);
    return std::wstring(wchar_buf);
}

std::string wstring2oemstring(std::wstring i) {
    int l = i.size() + 1; //terminating 0
    const int b = sizeof(char_buf);
    if (WideCharToMultiByte(CP_OEMCP, 0, i.c_str(), l, NULL, 0, NULL, NULL) > b) { //(UINT CodePage, DWORD dwFlags, LPWSTR lpWideCharStr, int cchWideChar, LPCSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
        std::cerr << "wstring2oemstring: char_buf is too small." << std::endl;
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
    if (GetShortPathNameW(p.c_str(), (LPWSTR) &wchar_buf, wchar_buf_length) > wchar_buf_length) { //(lpszLongPath,lpszShortPath,cchBuffer)
        std::cerr << "wShortPath: wchar_buf is too small." << std::endl;
        std::exit(Close_All_Return(EXIT_FAILURE));
    }
    return std::wstring(wchar_buf);
}
//*/

std::wstring wTrailSlash(std::wstring s) {
    unsigned c = s.size();
    if (c == 0) return L"";
    c = s[c - 1];
    if ((c == L'\\') or (c == L'/')) return s;
    return s + L"\\";
}

//* multi-pass fastest variant
unsigned MinimalMultipleMP(std::vector<unsigned> * a, unsigned k, unsigned am, unsigned p) { // array, index of last used element, previous multiple
    unsigned new_a = (*a)[k];
    unsigned prev_mul = std::max(p, am); if (!prev_mul) prev_mul = 1;
    for (unsigned n = 1; n <= prev_mul; n++) {
        unsigned long long m = new_a * n;
        if (m > (unsigned long long) 0x7fffffff) {
            std::cerr << "\nMinimalMultipleMP does not fit to long integer." << std::endl;
            return unsigned(-1);
        }
        if (m >= prev_mul) {
            for (unsigned l = 0; l < k; l++) if (m % (*a)[l]) goto not_multiple;
            return m;
        }
        not_multiple: ;
    }
    // should not be here, probably an overflow
    std::cerr << "\nMinimalMultipleMP not found." << std::endl;
    return unsigned(-1);
}
//* single call fast variant
unsigned MinimalMultipleSP(std::vector<unsigned> * a, unsigned k, unsigned am) { // array, index of last used element, largest value
    //unsigned am = (*a)[0]; for (unsigned i = 1; i <= k; i++) am = std::max(am, (*a)[i]);
    unsigned multiple = std::max(am, 1u);
    for (unsigned i = 0; i <= k; i++) {
        unsigned new_a = (*a)[i];
        for (unsigned n = 1; n <= multiple; n++) {
            unsigned long long m = new_a * n;
            if (m > (unsigned long long) 0x7fffffff) {
                std::cerr << "\nMinimalMultipleSP does not fit to long integer." << std::endl;
                return unsigned(-1);
            }
            if (m >= multiple) {
                for (unsigned l = 0; l < i; l++) if (m % (*a)[l]) goto not_multiple;
                multiple = m;
                goto is_multiple;
            }
            not_multiple: ;
        }
        // should not be here, probably an overflow
        std::cerr << "\nMinimalMultipleSP not found." << std::endl;
        return unsigned(-1);
        is_multiple: ;
    }
    return multiple;
}
//* single call slowest variant
unsigned MinimalMultiple(std::vector<unsigned> * a, unsigned k, unsigned am) { // array, index of last used element, largest value
    unsigned long long multiple = std::max(am, 1u);
    while (multiple <= (unsigned long long) 0x7fffffff) {
        for (unsigned i = 0; i <= k; i++) if (multiple % (*a)[i]) goto not_multiple;
        return unsigned(multiple);
       not_multiple:
        multiple++;
    }
    std::cerr << "\nMinimalMultiple does not fit to long integer." << std::endl;
    return unsigned(-1);
}

std::string dhms(double s) {
    std::string r;
    int remain = std::ceil(s);
    int days = remain / 86400;
    if (days) {
        r = r + std::to_string(days) + " day";
        if (days != 1) r += "s";
    }
    remain %= 86400;
    int hours = remain / 3600;
    if (hours) {
        if (days) r += " ";
        r = r + std::to_string(hours) + " hour";
        if (hours != 1) r += "s";
        remain %= 3600;
    }
    int minutes = remain / 60;
    if (minutes) {
        if (days || hours) r += " ";
        r = r + std::to_string(minutes) + " minute";
        if (minutes != 1) r += "s";
        remain %= 60;
    }
    if (days || hours || minutes) r += " ";
    r = r + std::to_string(remain) + " second";
    if (remain != 1) r += "s";
    return r; // + " (" + std::to_string(s) + " second" + ((s != 1)?"s)":")");
}

std::string papl(unsigned n) {return (n == 1) ? " pass" : " passes";}
std::string bypl(unsigned n) {return (n == 1) ? "byte" : "bytes";}
std::string arpl(unsigned n) {return (n == 1) ? " archive":" archives";}


int main(int argc, char** argv) {
/* MinimalMultiple test
    cycleN_count.clear();
    unsigned pm = 0;
    unsigned am = 0;
    srand(time(NULL));
    for (unsigned p = 0; p < 100; p++) {
        unsigned r = std::rand() % 100 + 1;
        am = std::max(am, r);
        cycleN_count.push_back(r);
        std::cout << r << " > ";
        StartTime = time(NULL);
        pm = MinimalMultipleMP(&cycleN_count, p, am, pm);
        double pmt = difftime(time(NULL), StartTime);
        StartTime = time(NULL);
        unsigned a = MinimalMultipleSP(&cycleN_count, p, am);
        double at = difftime(time(NULL), StartTime);
        StartTime = time(NULL);
        unsigned b = MinimalMultiple(&cycleN_count, p, am);
        double bt = difftime(time(NULL), StartTime);
        if ((b == unsigned(-1)) || (a != b) || (b != pm)) {
            std::cout << "MinimalMultiple: " << b << ", MinimalMultipleSP: " << a << ", MinimalMultipleMP: " << pm << " (" << int(bt) << ", " << int(at) << ", " << int(pmt) << ")." << std::endl;
            return -1;
        }
        std::cout << b << " (" << int(bt) << ", " << int(at) << ", " << int(pmt) << ")." << std::endl;;
    }
    std::cout << "Done." << std::endl;
    return -1;
//*/
    if (output_locale == CP_UTF8) {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        setlocale(LC_ALL, ".65001");
    } else {
        //SetConsoleOutputCP(GetOEMCP());
        //SetConsoleCP(GetOEMCP());
        setlocale(LC_ALL, ("." + std::to_string(GetOEMCP())).c_str());
    }
    locale_name = setlocale(LC_ALL, NULL);

    if (GetEnvironmentVariableW(L"ProgramFiles", (LPWSTR)&wchar_buf, wchar_buf_length) >= wchar_buf_length) {
        std::cerr << "GetEnvironmentVariable: wchar_buf is too small." << std::endl;
        return Close_All_Return(EXIT_FAILURE);
    };
    arg_string[0] = wTrailSlash(std::wstring(wchar_buf)) + SevenZipSubPath;

    // definition of command line arguments   abcdefghijklmnopqrstuvwxyz
    //                                        abcdef.hi..lmnop.rst......
    TCLAP::CmdLine cmd("Zipper: checks different number of compression passes for 7-Zip ZIP archives.", ' ', "Zipper v1.6");

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
                    "Number of identical archives to detect compression cycling.\nMore than 12 doesn't seem to make sense. [12]", false,
                    12, "integer", cmd);

    TCLAP::ValueArg<bool> cmdShowPasses("s", "show-passes",
                    "Show number of found and checked passes in \"name.best#.of#.zip\". [1 = yes]", false,
                    true, "boolean", cmd);

    TCLAP::ValueArg<bool> cmdShowFull("f", "show-fullness",
                    "Show \"cycle size / unfinished search\" in names. [1 = yes]", false,
                    true, "boolean", cmd);

    TCLAP::ValueArg<int> cmdOld("a", "alternative-detection",
                    "0 - use normal cycling detection (1st cycle + matches); [0 = normal]\n"
                    "1 - use old cycling detection (any matches);\n"
                    "2 - force full 2nd cycle comparison.", false,
                    0, "integer", cmd);

    TCLAP::ValueArg<std::string> cmdMMT("m", "multithreading",
                    "7-Zip multithreading = off/on/N threads. [on]", false,
                    "on", "string", cmd);

    TCLAP::ValueArg<std::string> cmdRedefine("r", "redefine",
                    "Fully redefine command line.\nPass arguments as %c (7z.exe), \\\"%i\\\" (file), \\\"%o\\\" (archive), %p (passes).\nUse %% to pass % character before c, i, o, p without substitution.", false,
                    "", "string", cmd);

    TCLAP::ValueArg<bool> cmdDebug("e", "debug-output",
                    "Show more variables. [0 = no]", false,
                    false, "boolean", cmd);

    // parse command line arguments
    try {
        cmd.parse(argc, argv);
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Error : cmd.parse() threw exception" << std::endl;
        return Close_All_Return(EXIT_FAILURE);
    }

    // command line accepted, begin console processing
    std::cout << "Locale: " << ((locale_name == NULL)?"error getting locale name.":std::string(locale_name)) << std::endl;
    //system("graftabl");
    arg_string[0]      = oemstring2wstring(cmd7Zip.getValue()); // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
    zipTempDir         = oemstring2wstring(cmdTempDir.getValue());
    zipInputDir        = oemstring2wstring(cmdInputDir.getValue());
    zipOutputDir       = oemstring2wstring(cmdOutputDir.getValue());
    mmt                = oemstring2wstring(cmdMMT.getValue());
    redefine           = oemstring2wstring(cmdRedefine.getValue());
    zipSingle          = oemstring2wstring(cmdSingle.getValue());
    show_passes        = cmdShowPasses.getValue();
    show_full          = cmdShowFull.getValue();
    if (int a = cmdOld.getValue()) {
        old_detection = (a == 1);
        full_check    = (a == 2);
    }
    debug_output       = cmdDebug.getValue();
    mem_limit          = std::max(0, cmdMemLimit.getValue()) << 20;
    passes             = std::max(0, cmdPasses.getValue());
    detect_threshold   = std::max(unsigned(1), std::min(unsigned(cmdDetect.getValue()), (passes > 0)?passes:unsigned(-1))); // limit to 1...passes
    begin              = std::max(1, cmdStart.getValue());
    if (passes < 1) {
        auto_passes = true;
        passes = std::max(576, (int) detect_threshold) * 3 + begin - 1; // 24*24*3
    }
    if (begin > passes) {
        std::cerr << "\nStarting number of passes is greater than total number (" << begin << " > " << passes << "). Stop." << std::endl;
        return Close_All_Return(EXIT_FAILURE);
    }
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
    dir_subdirs.clear();
    std::cout << "Found " << dir_list.size() << " file(s)." << std::endl;


    resizeArraysToPasses(); // after release of temporary arrays

    // processing file list
    for (unsigned i = 0; i < dir_list.size(); i++) {
        arg_string[1] = zipInputDir + dir_list[i]; // 0 - 7z.exe, 1 - input file, 2 - temp name, 3 - %
        std::cout << "\n-------------------------------------\nFile: " << wstring2string(arg_string[1]) << std::endl;
        file_check = CreateFileW(arg_string[1].c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); //(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDistribution, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
        if (file_check != INVALID_HANDLE_VALUE) {
            Close_File(&file_check);
            arg_string[2] = zipTempDir + dir_list[i].substr(wPath_length(dir_list[i])) + zipExt; // input name without path
            std::cout << "Testing: " << wstring2string(arg_string[2]) << std::endl;

            StartTime = time(NULL);
            for (unsigned p = 0; p < zip_storage.size(); p++) zip_storage[p].clear(); // freeing memory
            zip_storage.clear();
            zip_indices.assign(passes, -1); // resetting indices, iczsm
            mem_use = 0;
            cycleN_count.assign(passes, 0); // continuous matches counters
            cycleN_sizes.assign(passes, 0); // array of minimal cycle sizes
            cycleN_start.assign(passes, 0); // 1-based array of cycle starts, negative for the 1st time
            cycleNsizes_count = 0;
            is_full = false;
            detect_threshold_current = detect_threshold;
            minimal_zip_length = unsigned(-1);
            minimal_zip_passes = 0;
            pass_counter = 0;
            matched_once = false;
            match_counter = 0;
            cycle_start = 0;
            cycle_size = 0;
            cycle_max_size = 0;
            unsigned p = begin - 1;
            while (p < passes) {
                unsigned p1 = p + 1; // 1-based counter
                std::cout << "-------------------------------------\nPass: " << p1 << "/" << passes;
                if (minimal_zip_passes == 0) {
                    std::cout << std::endl;
                } else {
                    std::cout << ", minimal packed size: " << minimal_zip_length << " " << bypl(minimal_zip_length) << " (" << (minimal_zip_passes) << papl(minimal_zip_passes) << ")." << std::endl;
                }
                if (redefine.size() == 0) {
                    zip_cmd = L"\"" + arg_string[0] + L"\" a -tzip -mx=9 -mmt=" + mmt + L" -mtc=off -mfb=258 -mpass=" + std::to_wstring(p1)
                              + L" \"" + arg_string[2] + L"\" \"" + arg_string[1] + L"\"";
                } else {
                    zip_cmd = redefine;
                    for (int a = positions.size() - 1; a >= 0; a--) {
                        zip_cmd.erase(positions[a], 2);
                        if (params[a] != -1) {
                            zip_cmd.insert(positions[a], arg_string[params[a]]);
                        } else {
                            zip_cmd.insert(positions[a], std::to_wstring(p1));
                        }
                    }
                }
                zip_cmd += std::wstring(32768, L'\0'); // CreateProcess can change command line, reserving space
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
                                pass_counter = p1;
                                minimal_zip_length = zip_length;
                                minimal_zip_passes = pass_counter;
                            }
                        }
                        goto passes_checked;
                    }
                    unsigned m = std::max((unsigned)0, std::min( (mem_limit - mem_use - zip_length), (static_cast<unsigned>(mem_stat.dwAvailPhys) - zip_length) ) ) >> 20;
                    std::cout << "Memory left: " << m << " mega" << bypl(m) << "." << std::endl;
                    if (zip_length > 0) {
                        pass_counter = p1;
                        zip_pass1.resize(zip_length);
                        unsigned bytes_read;
                        ReadFile(file_check, zip_pass1.data(), zip_length, (LPDWORD)&bytes_read, NULL); //(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
                        Close_File(&file_check);
                        std::cout << "Removing \"" << wstring2string(arg_string[2]) << "\"." << std::endl;
                        DeleteFileW(arg_string[2].c_str());

// filling matches map, cycleN_match[0],[1]... = cycle 1,2...
                        int add_index = -1;
                        cycleN_match.assign(passes, false);
                        matched_pass = false;
                        unsigned full_counter = 0;
                        for (int c = p - 1; c >= 0; c--) {
                            if (zip_indices[c] != -1) { // for skipped passes
                                if (zip_storage[zip_indices[c]].size() == static_cast<size_t>(zip_length)) {
                                    if (memcmp(zip_storage[zip_indices[c]].data(), zip_pass1.data(), zip_length) == 0) {
                                        add_index = c;
                                        if (!matched_pass) {
                                            match_counter++;
                                            matched_pass = true;
                                            matched_once = true;
                                        }
                                        unsigned tcs = p - c; // cycle size
                                        cycleN_match[tcs - 1] = true;
                                        int tcb = c - tcs + 1; // 1st match already found, cycle start + 1
                                        if (tcb >= 0) {
                                            unsigned dc = 1;
                                            for (int d = tcb; d < c; d++) {
                                                int r  = zip_indices[d];       if (r  == -1) goto wrong_cycle; // skipped pass
                                                int r1 = zip_indices[d + tcs]; if (r1 == -1) goto wrong_cycle;
                                                if (zip_storage[r].size() != zip_storage[r1].size()) goto wrong_cycle;
                                                if (memcmp(zip_storage[r].data(), zip_storage[r1].data(), zip_storage[r].size()) != 0) goto wrong_cycle;
                                                dc++;
                                            }
                                            if (!full_counter) {
                                                std::cout << "Full cycle(s): ";
                                            } else
                                                std::cout << ", ";
                                            full_counter = dc;
                                            std::cout << full_counter;
                                           wrong_cycle: ;
                                        } // cycle possible
                                    } // data matched
                                } // size matched
                            } // pass not skipped
                        }
                        if (full_counter) std::cout << arpl(full_counter) << "." << std::endl;

// calculating number of passes
                        if (add_index != -1) {
                            // filling cycle sizes array, displaying sizes, counting continuous cycles
                            bool line_start = true;
                            for (unsigned c = 0; c < passes; c++) {
                                if (cycleN_match[c]) {
                                    unsigned c1 = c + 1; // 1-based size of current cycle
                                    unsigned cm = cycleN_count[c] + 1; cycleN_count[c] = cm; // matches of current cycle
                                    // updating minimal cycle sizes array
                                    if (line_start) {
                                        for (unsigned k = 0; k < cycleNsizes_count; k++)
                                            if (cycleN_sizes[k] == c1) goto cycle_size_present;
                                        cycleN_sizes[cycleNsizes_count] = c1;
                                        if (c1 > cycle_max_size) cycle_max_size = c1;
                                        //if ((cycle_size = MinimalMultiple(&cycleN_sizes, cycleNsizes_count, cycle_max_size)) == unsigned(-1)) goto passes_checked; // error
                                        if ((cycle_size = MinimalMultipleMP(&cycleN_sizes, cycleNsizes_count, cycle_max_size, cycle_size)) == unsigned(-1)) goto passes_checked; // error
                                        cycleNsizes_count++;
                                       cycle_size_present:
                                        line_start = false;
                                        std::cout << "Possible cycle(s): ";
                                    } else
                                        std::cout << ", ";
                                    std::cout << cm << "/" << c1;
                                    // updating cycles' starts
                                    if (c1 == cycle_size) { // estimated match, direct start correction
                                        cycle_start = p1 - c1 - cm;
                                        cycleN_start[c] = cycle_start + 1;
if (debug_output) std::cout << "=" << cycle_start;
                                    } else if ((c1 < cycle_size) && ((cm == 1) || ((cm % c1) == 1))) { // cycle is started, correct start with minimal of stable cycles
                                        int cs = cycleN_start[c];
                                        int ns = p1 - c1;
                                        if (!cs) cs = -ns; // 1st time is not stable
                                        else if (cs < 0) {
                                           if (((ns + cs) % c1) == 0) cs = -cs; else cs =  ns; // if cycle is multiple, keep previous start
                                        }
                                        cycleN_start[c] = cs;
                                        ns = passes + 1; // max
                                        for (unsigned k = std::min(cycle_size, passes); k >= detect_threshold; k--) {
                                            cs = cycleN_start[k - 1];
                                            if (cs > 0) // skip unstable cycles
                                                if (cs < ns) ns = cs;
                                        }
                                        if (ns > int(passes)) // no stable cycles, trying just set ones
                                            for (unsigned k = std::min(cycle_size, passes); k >= detect_threshold; k--) {
                                                cs = -cycleN_start[k - 1];
                                                if (cs > 0) // skip stable cycles
                                                    if (cs < ns) ns = cs;
                                            }
                                        if ((!cycle_start && (ns <= int(passes))) || (ns <= int(cycle_start))) { // ns is set and cycle_start is greater or wasn't set
                                            cycle_start = ns - 1;
if (debug_output) std::cout << "-" << cycle_start;
                                        }
                                    }
                                } else
                                    cycleN_count[c] = 0;
                            }
                            if (!line_start) std::cout << "." << std::endl;
if (debug_output) {
std::cout << "Minimal cycles:";
for (unsigned c = 0; c < cycleNsizes_count; c++) if (cycleN_sizes[c])
    std::cout << " " << cycleN_sizes[c];
std::cout << ", cycle_max_size: " << cycle_max_size << "." << std::endl;
std::cout << "Cycle starts:";
for (unsigned c = 0; c < passes; c++) if (cycleN_start[c])
    std::cout << " " << (c + 1) << ":" << (cycleN_start[c] - ((cycleN_start[c] > 0)?1:-1));
std::cout << "." << std::endl;
std::cout << "cycle_start: " << int(cycle_start) << ", cycle_size: " << cycle_size << ", cycle_end: " << (cycle_start + cycle_size) << ", detect_threshold: " << detect_threshold << ", max passes: " << (cycle_start + cycle_size * 2) << "." << std::endl;
}
                            if (!old_detection) {
                                if (detect_threshold_current < cycle_size) detect_threshold_current = cycle_size;
                                unsigned m = (full_check)?detect_threshold_current:detect_threshold;
                                if ((detect_threshold_current <= passes) && (cycleN_count[detect_threshold_current - 1] >= m)) {
                                    std::cout << "Required " << m << arpl(m) << " matched. Search complete." << std::endl;
                                    is_full = true;
                                    goto passes_checked;
                                }
                            } else if (match_counter >= detect_threshold) {
                                std::cout << "Matched" << arpl(match_counter) << ": " << match_counter << "/" << detect_threshold << ". Search complete." << std::endl;
                                is_full = true;
                                goto passes_checked;
                            }

                            //std::cout << "Matched sample, referencing previous copy." << std::endl;
                            zip_indices[p] = zip_indices[add_index];
                        } else { // archive does not match any previous sample
                            // updating estimated number of passes
                            cycle_start++;
                            // resetting match counters
                            match_counter = 0;
                            cycleN_count.assign(passes, 0);
                            // adding archive to samples storage
                            zip_indices[p] = zip_storage.size();
                            zip_storage.emplace_back(); // empty sample
                            zip_storage.back().swap(zip_pass1); // moving new sample to storage
                            mem_use += zip_length;
                            if ((unsigned) zip_length < minimal_zip_length) {
                                minimal_zip_length = zip_length;
                                minimal_zip_passes = pass_counter;
                            }
                        }
                    } else
                        Close_File(&file_check);
                } else {
                    std::cerr << "\nCan not open archive \"" << wstring2string(arg_string[2]) << "\"." << std::endl;
                    break;
                }
                if (matched_once) std::cout << "Matched archives: " << ((match_counter == 0) ? "-" : std::to_string(match_counter)) << "/" << detect_threshold_current << "." << std::endl;
                if (cycle_size > 0) {
                    unsigned current_size; //current_size = std::max(cycle_size, detect_threshold_current);
                    if (!match_counter || (p1 < (cycle_start + detect_threshold_current + ((full_check)?detect_threshold_current:detect_threshold))))
                        current_size = detect_threshold_current;
                    else
                        current_size = cycle_size;
                    current_size = int(std::floor(double(p1 - cycle_start) / current_size / 2)) * current_size + current_size;
                    unsigned tp = cycle_start + current_size + ((full_check)?current_size:detect_threshold);
                    // passes = n*(n+1)/2
                    // p2/p1 = n2*(n2+1)/2/n1*(n1+1)*2 = n2*(n2+1)/n1/(n1+1)
                    double k = static_cast<double>(tp) * (tp + 1) / p1 / (p1 + 1);
                    //std::cout << "Time: " << dhms(difftime(time(NULL), StartTime)) << "." << std::endl;
                    std::cout << "Estimated cycle: " << cycle_size << ", total passes: " << tp
                              << ".\nTime left: " << dhms((k - 1) * difftime(time(NULL), StartTime)) << "." << std::endl; //k*CurrentSeconds-CurrentSeconds
                }
                p = p1; //p++;
                if (auto_passes and (p >= passes)) {
                    unsigned prev_passes = passes;
                    passes = (passes + cycle_start + cycle_size) * 3;
                    resizeArraysToPasses();
                    for (unsigned l = prev_passes; l < passes; l++) zip_indices[l] = -1;
                    //for (unsigned l = prev_passes; l < passes; l++) {
                    //    cycleN_count[l] = 0;
                    //    cycleN_match[l] = 0;
                    //    cycleN_sizes[l] = 0;
                    //    //...
                    //}
                }
            }
passes_checked:
            std::cout << "Cleaning \"" << wstring2string(arg_string[2]) << "\"." << std::endl;
            DeleteFileW(arg_string[2].c_str());
            if (minimal_zip_passes != 0) {
                std::cout << "Minimum archive size: " << minimal_zip_length << " " << bypl(minimal_zip_length) << " (" << (minimal_zip_passes) << papl(minimal_zip_passes) << ")." << std::endl;
                std::cout << "Time: " << dhms(difftime(time(NULL), StartTime)) << "." << std::endl;
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
                        swprintf((wchar_t*)&wchar_buf, wchar_buf_length, f.c_str(), minimal_zip_passes);
                        arcname_out = arcname_out + L".best" + std::wstring(wchar_buf);
                        swprintf((wchar_t*)&wchar_buf, wchar_buf_length, f.c_str(), pass_counter);
                        arcname_out = arcname_out + L".of" + std::wstring(wchar_buf);
                    }
                    if (show_full) {
                        if (is_full) {
                            if (old_detection) {
                                swprintf((wchar_t*)&wchar_buf, wchar_buf_length, f.c_str(), match_counter);
                                arcname_out += L".match";
                            } else {
                                swprintf((wchar_t*)&wchar_buf, wchar_buf_length, f.c_str(), cycle_size);
                                arcname_out += L".cycle";
                            }
                            arcname_out += std::wstring(wchar_buf);
                        } else
                            arcname_out += (old_detection ? L".match-" : L".cycle-");
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
                    unsigned bytes_written = 0;
                    if (outfile == INVALID_HANDLE_VALUE) {
archive_not_written:
                        std::cerr << "\nError writing archive \"" << wstring2string(arcname_out) << "\"." << std::endl;
                    } else {
                        WriteFile(outfile, zip_storage[zip_indices[minimal_zip_passes - 1]].data(), minimal_zip_length, (LPDWORD)&bytes_written, NULL); //(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
                        Close_File(&outfile);
                        if (bytes_written < minimal_zip_length) goto archive_not_written;
                    }
                } else // minimal_zip_passes == 0
                    std::cerr << "\nError compressing archive \"" << wstring2string(arcname_out) << ".#.#.#.zip\"." << std::endl;
            } else { // single archive
                if (minimal_zip_passes != 0) {
                    arcname_out = zipOutputDir + zipSingle;
                    if (redefine.size() == 0) {
                        //TODO: how to add *single* "path\file" to the archive with relative path kept?
                        zip_cmd = L"\"" + arg_string[0] + L"\" a -tzip -mx=9 -mmt=" + mmt + L" -mtc=off -mfb=258 -mpass=" + std::to_wstring(minimal_zip_passes)
                                  + L" \"" + arcname_out + L"\" \"" + arg_string[1] + L"\"";
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
                    }
                    zip_cmd += std::wstring(32768, L'\0');
                    std::cout << "Adding to \"" << wstring2string(arcname_out) << "\"." << std::endl;
                    std::cout << (char*)wstring2string(zip_cmd).c_str() << std::endl;
                    CreateProcessW(NULL, (LPWSTR)zip_cmd.c_str(), NULL, NULL, false, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi); //(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFO lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
                    CloseHandle(pi.hThread);
                    WaitForSingleObject(pi.hProcess, INFINITE);
                    CloseHandle(pi.hProcess);
                } else { // minimal_zip_passes == 0
                    std::cerr << "\nError compressing file \"" << wstring2string(arg_string[1]) << "\"." << std::endl;
                    break; // interrupt creation of single archive
                }
            }
        } else
            std::cerr << "\nCan not open file \"" << wstring2string(arg_string[1]) << "\"." << std::endl;
    }

// finished
    for (unsigned p = 0; p < zip_storage.size(); p++) zip_storage[p].clear();
    zip_storage.clear();
    if (output_locale == CP_UTF8) {
        SetConsoleOutputCP(GetOEMCP());
        SetConsoleCP(GetOEMCP());
    }
    return Close_All_Return(0);
}
