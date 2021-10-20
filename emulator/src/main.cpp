#include <iostream>
#include <string>

#include "../inc/emulator.h"

using namespace std;

int main(int argc, const char *argv[])
{
    if(argc < 2) {
        cout << "Memory context needed!!" << endl;
        return -1;
    }

    string memFile = argv[1];
    
    Emulator emu(memFile);
    emu.startEmulation();

    return 0;
}
