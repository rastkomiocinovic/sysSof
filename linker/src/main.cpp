#include <iostream>
#include <map>
#include <string>
#include <regex>

#include "../inc/linker.h"

using namespace std;

int main(int argc, const char *argv[])
{
    string outputFile = "out.o";
    map<string, int> placement;
    bool hexOut = false;
    bool linkableOut = false;
    vector<string> inputFiles;

    regex placeRx(R"(-place=.+@.+)");
    regex notSect(R"((-place=)|(@.+))");
    regex notAddr(R"(.+@0x)");

    for(int i = 1; i < argc; i++) {
        string arg = argv[i];
        if(arg == "-o") {         // handle output file name   
            if(++i >= argc) {
                cout << "Invalid args" << endl;
                return -1;
            }
            outputFile = argv[i];
            continue;
        }
        if(regex_match(arg, placeRx)) {    // handle placement 
            string section = regex_replace(arg, notSect, "");
            stringstream ss;
            int address;
            ss << hex << regex_replace(arg, notAddr, "");
            ss >> address;

            placement[section] = address;            
            continue;
        }
        if(arg == "-hex") {
            hexOut = true;
            continue;
        }
        if(arg == "-linkable") {
            linkableOut = true;
            continue;
        }

        inputFiles.push_back(arg);
    }

    Linker linker(outputFile, placement, hexOut, linkableOut, inputFiles);
    linker.link();

    return 0;
}
