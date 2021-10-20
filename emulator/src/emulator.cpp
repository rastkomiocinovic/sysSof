#include "../inc/emulator.h"
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <chrono>

void Emulator::startEmulation() {
    //cout << hex << "Emulation start" << endl;
    ifstream in(memFile, ios::binary);  // load memory
    if(in.fail()) {
        //cout << memFile << " could not be opened!" << endl;
        return;
    }
    for(int i = 0; i < 1 << 16; i++) {
        in.read((char*)&mem[i], sizeof(char));
    }

    // init values
    pc = readWord(0, true);
    sp = 0xff;
    reg[0] = 0;
    reg[1] = 1;
    setupTerminal();
    running = true;

    while(running) {    // main loop
        //cout << hex << (int)pc << endl;
        processInstruction();
        timer();
        getUserInput();
        handleInterrupts();
    }

    cout << endl;
}

void Emulator::processInstruction() {
    char inst = mem[pc++];
    char op = (inst >> 4) & 0xf;
    char mod = inst & 0xf;
    

    //cout << (int)op << endl;

    switch(op) {
        case 0x0: { // halt
            //cout << endl << "HALT" << endl;
            running = false;
            return;
        }
        case 0x1: { // int
            pc++;
            // TODO: push psw; pc<=mem16[(reg[DDDD] mod 8)*2];
            return;
        }
        case 0x2: { // iret
            short pswW = pop();
            pc = pop();
            //cout << "IRET TO: " << pc << endl << flush;
            psw.I = false;//(pswW >> 15) & 0xf;
            psw.T1 = false;//(pswW >> 14) & 0xf;
            psw.Tr = false;//(pswW >> 13) & 0xf;
            psw.N = false;//(pswW >> 3) & 0xf;
            psw.C = false;//(pswW >> 2) & 0xf;
            psw.O = false;//(pswW >> 1) & 0xf;
            psw.Z = false;//(pswW >> 0) & 0xf;
            //cout << "PSW: " << hex << pswW << endl << flush;
            return;
        }
        case 0x3: { // call
            //cout << endl << "CALL" << endl;
            push(pc);
            char regD = mem[pc++];
            short sRegN = regD & 0xf;
            char upAddrT = mem[pc++];
            char upT = (upAddrT >> 4) & 0xf;
            char adT = upAddrT & 0xf;
            short payload;
            if(adT == imm || adT ==  regIndDisp || adT == memDir) {
                payload = readWord(pc, false);
                pc+=2;
            }
            pc = getOperand(payload, sRegN, adT);             
            return;
        }
        case 0x4: { // ret
            //cout << endl << "RET" << endl;
            pc = pop();
            return;
        }
        case 0x5: { // jmp, jeq, jne, jgt
            //cout << endl << "JUMP" << endl;
            char regD = mem[pc++];
            char sRegN = regD & 0xf;
            char upAddrT = mem[pc++];
            char upT = (upAddrT >> 4) & 0xf;
            char adT = upAddrT & 0xf;
            //cout << (int)adT << endl;
            short payload = readWord(pc, false);
            pc+=2;
            if(mod == 0 | (mod == 1 && psw.Z) | (mod == 2 && !psw.Z) | (mod == 3 && psw.N)) pc = getOperand(payload, sRegN, adT);
            return;
        }
        case 0x6: { // xchg
            //cout << endl << "XCHG" << endl;
            char regD = mem[pc++];
            char sRegN = regD & 0xf;
            char dRegN = (regD >> 4) & 0xf;

            short temp = reg[sRegN];
            reg[dRegN] = reg[sRegN];
            reg[dRegN] = temp;
            return;
        }
        case 0x7: { // add, sub, mul, div, cmp
            //cout << endl << "ARITM" << endl;
            char regD = mem[pc++];
            char sRegN = regD & 0xf;
            char dRegN = (regD >> 4) & 0xf;

            switch(mod) {
                case 0:
                    reg[dRegN] += reg[sRegN];
                    break;
                case 1:
                    reg[dRegN] -= reg[sRegN];
                    break;
                case 2:
                    reg[dRegN] *= reg[sRegN];
                    break;
                case 3:
                    reg[dRegN] /= reg[sRegN];
                    break;
                case 4:
                    psw.N = (reg[dRegN] - reg[sRegN]) < 0;
                    psw.Z = (reg[dRegN] - reg[sRegN]) == 0;
                    break;
            }

            return;
        }
        case 0x8: { // not, and, or, xor, test
            //cout << endl << "LOGIC" << endl;
            char regD = mem[pc++];
            char sRegN = regD & 0xf;
            char dRegN = (regD >> 4) & 0xf;

            switch(mod) {
                case 0:
                    reg[dRegN] = ~reg[dRegN];
                    break;
                case 1:
                    reg[dRegN] &= reg[sRegN];
                    break;
                case 2:
                    reg[dRegN] |= reg[sRegN];
                    break;
                case 3:
                    reg[dRegN] ^= reg[sRegN];
                    break;
                case 4:
                    psw.N = (reg[dRegN] & reg[sRegN]) < 0;
                    psw.Z = (reg[dRegN] & reg[sRegN]) == 0;
                    break;
            }

            return;
        }
        case 0x9: { // shl, shr
            //cout << endl << "SHIFT" << endl;
            char regD = mem[pc++];
            char sRegN = regD & 0xf;
            char dRegN = (regD >> 4) & 0xf;

            switch(mod) {
                case 0:
                    reg[dRegN] <<= reg[sRegN];
                    break;
                case 1:
                    reg[dRegN] >>= reg[sRegN];
                    break;
            }

            return;
        }
        case 0xa: { // ldr
            //cout << endl << "LDR" << endl;
            char regD = mem[pc++];
            char sRegN = regD & 0xf;
            char dRegN = (regD >> 4) & 0xf;
            char upAddrT = mem[pc++];
            char upT = (upAddrT >> 4) & 0xf;
            char adT = upAddrT & 0xf;
            short payload = 0;
            if(adT == imm || adT ==  regIndDisp || adT == memDir) {
                payload = readWord(pc, false);
                pc+=2;
            }            
            //cout << hex << (int)regD << endl << flush; 
            updateRegPre(upT, sRegN);
            reg[dRegN] = getOperand(payload, sRegN, adT);
            updateRegPost(upT, sRegN);
            //if(payload == 0x6a) mem[payload] = reg[dRegN] & 0xf;
            return;
        }
        case 0xb: { // str
            //cout << endl << "STR" << endl;
            char regD = mem[pc++];
            char sRegN = regD & 0xf;
            char dRegN = (regD >> 4) & 0xf;
            //cout << hex << (int)regD << endl << flush;
            char upAddrT = mem[pc++];
            char upT = (upAddrT >> 4) & 0xf;
            char adT = upAddrT & 0xf;
            short payload = 0;
            if(adT == imm || adT ==  regIndDisp || adT == memDir) {
                payload = readWord(pc, false);
                pc+=2;
            }            
            
            updateRegPre(upT, sRegN);
            setOperand(payload, dRegN, sRegN, adT);
            //if(payload == 0x6a) cout << "wait" << reg[dRegN] << endl << flush;
            updateRegPost(upT, sRegN);
            return;
        }
    }
}

void Emulator::updateRegPre(char type, char regN) {
    switch(type) {
        case 0:
            return;
        case 1:
            //cout << "pre- " << (int)regN << endl << flush;
            reg[regN] -= 2;
            return;
        case 2:
            //cout << "pre+" << endl << flush;
            reg[regN] += 2;
            return;
    }
}

void Emulator::updateRegPost(char type, char regN) {
    switch(type) {
        case 0:
            return;
        case 3:
            //cout << "post-" << endl << flush;
            reg[regN] -= 2;
            return;
        case 4:
            //cout << "post+" << endl << flush;
            reg[regN] += 2;
            return;
    }
}

void Emulator::setOperand(short payload, char dRegN, char sRegN, char adType) {
    switch(adType) {
        case regDir:
            reg[dRegN] = reg[sRegN];
            break;
        case regInd:
            writeWord(reg[dRegN], reg[sRegN], true);
            break;
        case regIndDisp:
            writeWord(reg[dRegN], reg[sRegN] + payload, true);
            break;  
        case memDir:
            //cout << "MEMDIR" << reg[dRegN] << " " << hex << payload << endl << flush;
            writeWord(reg[dRegN], payload, true);
            break;   
    }
}

short Emulator::getOperand(short payload, char regN, char adType) {
    short ret;
    switch(adType) {
        case imm: 
            ret = payload;
            break;
        case regDir:
            ret = reg[regN];
            break;
        case regInd:
            //cout << "regInd" << endl << flush;
            ret = readWord(reg[regN], true);
            break;
        case regIndDisp:
            ret = readWord(reg[regN] + payload, true);
            break;  
        case memDir:            
            ret = readWord(payload, true);
            //cout << "READ " << hex << payload  << ret << reg[1]<< endl << flush;
            break;   
    }
    return ret;
}

void Emulator::push(short dat) {
    sp -= 2;
    //cout << "PUSH " << hex << dat << endl << flush;
    writeWord(dat, sp, true);
}

short Emulator::pop() {
    short ret = readWord(sp, true);
    sp += 2;
    return ret;
}

void Emulator::timer() {
    static long long int prevTime = 0;
    time = std::chrono::duration_cast<chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    //cout << hex << interval << endl << flush;
    int intervalTime = interval == 0 ? 500 : (interval == 1 ? 1000 : (interval == 2 ? 1500 : (interval == 3 ? 2000 : 
                            (interval == 4 ? 5000 : (interval == 5 ? 10000 : (interval == 6 ? 30000 : 60000))))));
    if(time - prevTime > intervalTime && interval != -1) {
        interrupts |= 4;
        prevTime = time;
    }
}

void Emulator::getUserInput() {
    char c;
    if(read(STDIN_FILENO, &c, 1) == 1) {
        writeWord(c, 0xff00, true); // term in
        interrupts |= 8; // entry 3
        if(c == '`') running = false;
    }
}

void Emulator::handleInterrupts() {
    if(interrupts & 8) { // terminal interrupt
        if(!psw.Tr) {
            interrupts = 0;
            short pswW;
            pswW |= (short)psw.I << 15;
            pswW |= (short)psw.T1 << 14;
            pswW |= (short)psw.Tr << 13;
            pswW |= (short)psw.N << 3;
            pswW |= (short)psw.C << 2;
            pswW |= (short)psw.O << 1;
            pswW |= (short)psw.Z << 0;

            push(pc);
            push(pswW);
            //cout << "INTERRUPT JUMP FROM: " << pc << endl << flush;
            pc = readWord(6, true); // 3*2
            //cout << "INTERRUPT JUMP TO: " << pc << endl << flush;
            psw.I = true;
            psw.Tr = true;
            psw.T1 = true;
        }
    }
    if(interrupts & 4) { // timer interrupt
        //cout << "TAJMER" << endl << flush;
        if(!psw.T1) {
            interrupts = 0;
            short pswW;
            pswW |= (short)psw.I << 15;
            pswW |= (short)psw.T1 << 14;
            pswW |= (short)psw.Tr << 13;
            pswW |= (short)psw.N << 3;
            pswW |= (short)psw.C << 2;
            pswW |= (short)psw.O << 1;
            pswW |= (short)psw.Z << 0;

            push(pc);
            push(pswW);
            //cout << "TIMER TICK " << pc << endl << flush;
            pc = readWord(4, true); // 2*2
            //cout << "INTERRUPT JUMP TO: " << pc << endl << flush;
            psw.I = true;
            psw.Tr = true;
            psw.T1 = true;
        }
    }   
}

short Emulator::readWord(short address, bool isData) {
    if(isData) {
        char l = mem[address];
        char h = mem[address + 1];
        return (short)((h << 8) + (0xff & l));
    }

    char h = mem[address];
    char l = mem[address + 1];
    return (short)((h << 8) + (0xff & l));
}

void Emulator::writeWord(short word, short address, bool isData) {
    //cout << hex << (int)address << endl << flush;
    if(address == (short)0xff00) { // Terminal out
        cout << (char)word << flush;
    }
    if(address == (short)0xff10) { // Timer
        //cout << "TIMER SET " << hex << address << " " << word << endl << flush;
        interval = word;
        //cout << (char)word << flush << endl;
    }
    if(isData) {
        char l = word & 0xff;
        char h = word >> 8;
        //cout << "WRITING: " << hex << (int)h << (int)l << endl << flush;
        mem[address] = l;
        mem[address + 1] = h;
        return;
    }

    char h = word & 0xff;
    char l = word >> 8;
    mem[address] = l;
    mem[address + 1] = h;
}

struct termios oldStdin;
void restoreTerminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldStdin);
}
void Emulator::setupTerminal() {
    tcgetattr(STDIN_FILENO, &oldStdin);
    static struct termios newStdin = oldStdin;
    newStdin.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
    newStdin.c_cc[VMIN] = 0;
    newStdin.c_cc[VTIME] = 0;
    newStdin.c_cflag &= ~(CSIZE | PARENB);
    newStdin.c_cflag |= CS8;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &newStdin);
    atexit(restoreTerminal);
}

