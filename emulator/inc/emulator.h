#ifndef EMULATOR_H
#define EMULATOR_H
#include <string>
#include <fstream>

using namespace std;

class Emulator {
private:
    string memFile;

    char mem[1 << 16];
    bool running;

    struct pswStruct {
        bool Z = false;
        bool O = false;
        bool C = false;
        bool N = false;
        bool Tr = false;
        bool T1 = false;
        bool I = false;
    };

    short reg[8];
    short& pc = reg[7];
    short& sp = reg[6];
    pswStruct psw;
    short interrupts = 0;
    long long int time;
    short interval = -1;

    short readWord(short address, bool isData);
    void writeWord(short word, short address, bool isData);
    short pop();
    void push(short dat);
    short getOperand(short payload, char regN, char adType);
    void setOperand(short payload, char dRegN, char sRegN, char adType);
    void updateRegPre(char type, char regN);
    void updateRegPost(char type, char regN);
    void processInstruction();
    void timer();
    void getUserInput();
    void setupTerminal();
    void handleInterrupts();

    static const char imm = 0;
    static const char regDir = 1;
    static const char regDirDisp = 5;
    static const char regInd = 2;
    static const char regIndDisp = 3;
    static const char memDir = 4;

public:
    Emulator(string memFile) : memFile(memFile) {}
    void startEmulation();
};

#endif