#include <iostream>
#include "../inc/assembler.h"

using namespace std;

int main(int argc, const char *argv[])
{
    // Handle arguments
    if(argc < 2) {
        cout << "Invalid arguments" << endl;
        return -1;
    }

    string arg = argv[1];
    if(arg == "-o" && argc != 4) {
        cout << "Invalid arguments" << endl;
        return -1;
    }

    string outputPath = "out.o";
    if(arg == "-o") {
        outputPath = argv[2];
    }
    
    string inputPath = argv[argc-1];


    Assembler assembler(inputPath, outputPath);
    assembler.assemble();


    return 0;
}
