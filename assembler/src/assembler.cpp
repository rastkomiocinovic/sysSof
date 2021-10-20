#include <iomanip>
#include "../inc/assembler.h"

Assembler::Assembler(const string& inputFile, const string& outputFile) : inputFile(inputFile), outputFile(outputFile), inputStream(*new ifstream), outputStream(*new ofstream) {

}

bool Assembler::assemble() {
    symbolTable["UNDEFINED"].section = symbolTable["UNDEFINED"].name = "UNDEFINED";
    symbolTable["ABSOLUTE"].section = symbolTable["ABSOLUTE"].name = "ABSOLUTE";
    sectionTable["UNDEFINED"].name = "UNDEFINED";
    sectionTable["ABSOLUTE"].name = "ABSOLUTE";

    if(!firstPass()) return false;   
    if(!secondPass()) return false;

    outputStream.open(outputFile, ofstream::out | ofstream::trunc);
    if(!outputStream.is_open()) {
        cout << "Couldn't open output file!" << endl;
        return false;
    }
    createTxt(outputStream);
    outputStream.close();

    outputStream.open("bin_" + outputFile, ofstream::out | ofstream::binary | ofstream::trunc);
    if(!outputStream.is_open()) {
        cout << "Couldn't open output binary file!" << endl;
        return false;
    }
    createBin(outputStream);

    cout << "ASSEMBLY SUCCESS" << endl;
    return true;
}

bool Assembler::firstPass() {
    // Open File
    inputStream.open(inputFile);
    if(!inputStream.is_open()) {
        cout << "Couldn't open input file!" << endl;
        return false;
    }

    string line;
    while(getline(inputStream, line)) {
        lineNum++;
        line = parser.clearLine(line);
        if(line.length() == 0) continue; // skip empty lines      

        //cout << position << "\t|" << line << endl;

        if(parser.containsLabel(line)) {    // process labels
            if(!handleLabel(parser.getLabel(line))) return false;

            if(parser.labelOnly(line)) {
                continue;
            }

            line = parser.removeLabel(line);
        }

        lines.push_back(line); // push only lines with something to do into second pass

        if(parser.isDirective(line)) {  // process directives
            string dir = parser.getLeft(line);
            string symbs = parser.getRight(line);
            
            if(dir == ".global") {
                string symb;
                while ((symb = parser.getFirstBeforeComma(symbs)).length() != 0) {
                    symbs = parser.getAfterComma(symbs);
                    if(!handleGlobal(symb)) return false;
                }
            } else
            if(dir == ".extern") {
                string symb;
                while ((symb = parser.getFirstBeforeComma(symbs)).length() != 0) {
                    symbs = parser.getAfterComma(symbs);
                    if(!handleExtern(symb)) return false;
                }
            } else
            if(dir == ".section") {
                if(!handleSection(symbs)) return false;
            } else
            if(dir == ".word") {
                if(!handleWordFirstPass()) return false;
            } else
            if(dir == ".skip") {
                if(!handleSkipFirstPass(symbs)) return false;
            } else
            if(dir == ".equ") {
                string symb = parser.getFirstBeforeComma(symbs);
                string val = parser.getAfterComma(symbs);
                if(!handleEqu(symb, val)) return false;
            } else
            if(dir == ".end") {
                if(currentSection != "UNDEFINED") {
                    sectionTable[currentSection].size = position;
                }
                break; // end of first pass
            }

            continue;
        }
        
        // Line must be instruction if not continued
        if(!handleInstructionFirstPass(line)) return false;
    }

    return true;
}

bool Assembler::secondPass() {
    currentSection = "UNDEFINED";
    lineNum = 0;
    position = 0;
    for(string& line : lines) {
        //cout << line << endl;
        string left = parser.getLeft(line);
        string right = parser.getRight(line);
        if (left == ".global" || left == ".extern" || left == ".equ") // skip second pass
            continue;
        if (left == ".section") {
            currentSection = right;
            position = 0;
            //cout << "SECTION " << currentSection << endl;
            continue;
        }
        if (left == ".skip") {
            handleSkipSecondPass(parser.getNumberFromLiteral(right));
            continue;
        }
        if (left == ".end") // end second pass
            break;
        
        if (left == ".word") {
            handleWordSecondPass(right);
            continue;
        }

        // Instruction
        handleInstructionSecondPass(line);  
    }
    return true;
}

bool Assembler::handleWordSecondPass(const string& arg) {
    if(parser.isSymbol(arg)) {
        SymbolEntry se = symbolTable[arg];
        //cout << "WORD SYMBOL" << endl;
        if(se.isDefined) {
            if(se.section == "ABSOLUTE") {  // ABSOLUTE symbs don't need relocation
                sectionTable[currentSection].offsets.push_back(position);
                sectionTable[currentSection].data.push_back(se.value & 0xff);
                sectionTable[currentSection].data.push_back((se.value >> 8) & 0xff);
                return 0;
            }

            if(!se.isGlobal) {
                sectionTable[currentSection].offsets.push_back(position);
                sectionTable[currentSection].data.push_back(se.value & 0xff);
                sectionTable[currentSection].data.push_back((se.value >> 8) & 0xff);
                RelocationEntry re;
                re.isData = true;
                re.type = "R_SS_16";
                re.section = currentSection;
                re.offset = position;
                re.symbolName = se.section;
                relocationTable.push_back(re);
            } else { //Global => Fill with zeroes
                sectionTable[currentSection].offsets.push_back(position);
                sectionTable[currentSection].data.push_back(0 & 0xff);
                sectionTable[currentSection].data.push_back((0 >> 8) & 0xff);

                RelocationEntry re;
                re.isData = true;
                re.type = "R_SS_16";
                re.section = currentSection;
                re.offset = position;
                re.symbolName = se.name;
                relocationTable.push_back(re);
            }

            position += 2;
            return true;
        }
        //symbol is undefined => Fill with zeroes
        sectionTable[currentSection].offsets.push_back(position);
        sectionTable[currentSection].data.push_back(0 & 0xff);
        sectionTable[currentSection].data.push_back((0 >> 8) & 0xff);

        RelocationEntry re;
        re.isData = true;
        re.type = "R_SS_16";
        re.section = currentSection;
        re.offset = position;
        re.symbolName = se.name;
        relocationTable.push_back(re);

        position += 2;
        return true;
    }
    // arg is a literal
    uint16_t dat = parser.getNumberFromLiteral(arg);
    //cout << endl << arg << endl;
    sectionTable[currentSection].offsets.push_back(position);
    sectionTable[currentSection].data.push_back(dat & 0xff);
    sectionTable[currentSection].data.push_back((dat >> 8) & 0xff);

    position += 2;
    return true;
}

bool Assembler::handleSkipSecondPass(const int& len) {
    //cout << "SKIP " << len << endl;
    sectionTable[currentSection].offsets.push_back(position);
    for(int i = 0; i < len; i++) sectionTable[currentSection].data.push_back(0);
    position += len;
    return true;
}

bool Assembler::handleGlobal(const string& symb) {
    symbolTable[symb].isGlobal = true;
    symbolTable[symb].section = "UNDEFINED";
    symbolTable[symb].name = symb;
    return true;
}

bool Assembler::handleExtern(const string& symb) {
    if(symbolTable[symb].isDefined || symbolTable[symb].isGlobal) {
        cout << lineNum << "\tExtern symbol defined/global." << endl; 
        return false;
    }
    symbolTable[symb].isExtern = true;
    symbolTable[symb].section = "UNDEFINED";
    symbolTable[symb].name = symb;
    return true;
}

bool Assembler::handleSection(const string& section) {
    if(currentSection != "UNDEFINED") {
        sectionTable[currentSection].size = position;
    }

    //create new section
    symbolTable[section].section = section;
    symbolTable[section].name = section;
    symbolTable[section].isDefined = true;

    sectionTable[section].name = section;

    currentSection = section;
    position = 0;

    return true;
}

bool Assembler::handleWordFirstPass() {
    if(currentSection == "UNDEFINED") {
        cout << lineNum << "\t.word outside of section!" << endl;
        return false;
    }

    position += 2;
    return true;
}

bool Assembler::handleSkipFirstPass(const string& value) {
    if(currentSection == "UNDEFINED") {
        cout << lineNum << "\t.skip outside of section!" << endl;
        return false;
    }

    position += parser.getNumberFromLiteral(value);
    return true;
}

bool Assembler::handleEqu(const string& name, const string& value) {
    if(symbolTable[name].isDefined || symbolTable[name].isExtern) {
        cout << lineNum << "\t.equ used with symbol that i already defined/extern." << endl;
        return false;
    }
    int val = parser.getNumberFromLiteral(value);

    symbolTable[name].isDefined = true;
    symbolTable[name].section = "ABSOLUTE";
    symbolTable[name].value = val;
    symbolTable[name].name = name;

    sectionTable["ABSOLUTE"].offsets.push_back(sectionTable["ABSOLUTE"].size);
    sectionTable["ABSOLUTE"].size += 2;
    sectionTable["ABSOLUTE"].data.push_back(0xff & val);
    sectionTable["ABSOLUTE"].data.push_back(0xff & (val >> 8));

    return true;
}

bool Assembler::handleLabel(const string& name) {
    if(symbolTable[name].isDefined || symbolTable[name].isExtern) {
        cout << lineNum << "\tSymbol is already defined/extern." << endl;
        return false;
    }

    symbolTable[name].isDefined = true;
    symbolTable[name].section = currentSection;
    symbolTable[name].value = position;
    symbolTable[name].name = name;

    return true;
}

bool Assembler::handleInstructionFirstPass(const string& line) {
    string instr = parser.getLeft(line);
    string oprnds = parser.getRight(line);
    if(currentSection == "UNDEFINED") {
        cout << "Instruction not in section!" << endl;
        return false;
    }

    if(parser.noOperInstr(instr)) {
        position += 1;
        return true;
    }

    //cout << "!No" << endl;

    if(parser.oneOperRegInstr(instr)) {
        if(instr == "int" || instr == "not")
            position+=2;
        else
        if(instr == "push" || instr == "pop")
            position+=3;

        return true;
    }

    //cout << "!OneReg" << endl;

    if(parser.oneOperAllInstr(instr)) {
        if(parser.absAddress(oprnds) || parser.pcRelAddress(oprnds) || parser.regIndDispAddress(oprnds) || parser.memDirAddress(oprnds))
            position+=5;
        else
        if(parser.regDirAddress(oprnds) || parser.regIndAddress(oprnds))
            position+=3;
        
        return true;
    }

    //cout << "!OneOper" << endl;

    if(parser.twoOperAllInstr(instr)) {
        string oprnd = parser.getAfterComma(oprnds);
        if(parser.absAddress(oprnd) || parser.pcRelAddress(oprnd) || parser.regIndDispAddress(oprnd) || parser.memDirAddress(oprnd))
            position+=5;
        else
        if(parser.regDirAddress(oprnd) || parser.regIndAddress(oprnd))
            position+=3;
        
        return true;
    }

    //cout << "!TwoAll" << endl;

    if(parser.twoOperRegInstr(instr)) {
        position+=2;
        
        return true;
    }
    //cout << "!TwoReg" << endl;

    return true;
}

bool Assembler::handleInstructionSecondPass(const string& line) {
    string left = parser.getLeft(line);
    string right = parser.getRight(line);

    if(parser.noOperInstr(left)) {
        if(left == "halt") {
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(0x00);
        } else
        if(left == "iret") {
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(0x20);
        } else
        if(left == "ret") {
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(0x40);
        }
        position += 1;
    } else
    if(parser.oneOperRegInstr(left)) {
        int regNum = parser.getReg(right);

        if(left == "int") {
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(0x10);
            sectionTable[currentSection].data.push_back((regNum << 4) + 15);
            position += 2;
        } else
        if(left == "push") {
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(0xb0);
            sectionTable[currentSection].data.push_back((regNum << 4) + 6);
            sectionTable[currentSection].data.push_back(0x12);
            position += 3;
        } else
        if(left == "pop") {
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(0xa0);
            sectionTable[currentSection].data.push_back((regNum << 4) + 6);
            sectionTable[currentSection].data.push_back(0x42);
            position += 3;
        } else
        if(left == "not") {
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(0x80);
            sectionTable[currentSection].data.push_back((regNum << 4) + 15);
            position += 2;
        }
    } else
    if(parser.oneOperAllInstr(left)) {
        int instrDescr;
        int regDescr = 0xf0;
        int adrMode;
        if(left == "call") instrDescr = 0x30; else
        if(left == "jmp") instrDescr = 0x50; else
        if(left == "jeq") instrDescr = 0x51; else
        if(left == "jne") instrDescr = 0x52; else
        if(left == "jgt") instrDescr = 0x53;

        if(parser.absAddressJmp(right)) {
            regDescr |= 0xf;
            adrMode = 0;
            int val;

            if(parser.isSymbol(right)) {
                val = handleAbsoluteSymbol(right);
            } else {
                val = parser.getNumberFromLiteral(right);
                
            }

            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDescr);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            sectionTable[currentSection].data.push_back((val >> 8) & 0xff);
            sectionTable[currentSection].data.push_back(val & 0xff);
            position += 5;
        } else if(parser.pcRelAddress(right)) {
            regDescr |= 0x7;
            adrMode = 0x05;
            int val = handlePcRelSymbol(right);

            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDescr);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            sectionTable[currentSection].data.push_back((val >> 8) & 0xff);
            sectionTable[currentSection].data.push_back(val & 0xff);
            position += 5;
        } else if(parser.regDirAddress(right)) {
            int regNum = parser.getReg(right);
            regDescr |= regNum;
            adrMode = 0x01;

            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDescr);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            position += 3;
        } else if(parser.regIndAddress(right)) {
            int regNum = parser.getReg(right);
            regDescr |= regNum;
            adrMode = 0x02;

            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDescr);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            position += 3;
        } else if(parser.regIndDispAddress(right)) {
            regDescr |= parser.getReg(right);
            adrMode = 0x03;
            string disp = parser.getSymOrLit(right);
            int val;
            if(parser.isSymbol(disp)) {
                val = handleAbsoluteSymbol(disp);
            } else {
                val = parser.getNumberFromLiteral(disp);
            }
            
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDescr);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            sectionTable[currentSection].data.push_back((val >> 8) & 0xff);
            sectionTable[currentSection].data.push_back(val & 0xff);
            position += 5;
        } else if(parser.memDirAddress(right)) {
            regDescr |= 0xf;
            adrMode = 0x04;
            int val;
            if(parser.isSymbol(right)) {
                val = handleAbsoluteSymbol(right);
            } else {
                val = parser.getNumberFromLiteral(right);
            }
            
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDescr);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            sectionTable[currentSection].data.push_back((val >> 8) & 0xff);
            sectionTable[currentSection].data.push_back(val & 0xff);
            position += 5;
        }
    } else
    if(parser.twoOperAllInstr(left)) {
        string reg = parser.getFirstBeforeComma(right);
        string oprnd = parser.getAfterComma(right);

        char instrDesc;
        char regDescr;
        char adrMode;

        if(left == "ldr") instrDesc = 0xa0; else
        if(left == "str") instrDesc = 0xb0;

        regDescr = parser.getReg(reg) << 4;

        if(parser.absAddress(oprnd)) {
            oprnd =  parser.getSymOrLit(oprnd);
            if(parser.isSymbol(oprnd)) {
                regDescr |= 0xf;
                adrMode = 0;
                int val = handleAbsoluteSymbol(oprnd);
                sectionTable[currentSection].offsets.push_back(position);
                sectionTable[currentSection].data.push_back(instrDesc);
                sectionTable[currentSection].data.push_back(regDescr);
                sectionTable[currentSection].data.push_back(adrMode);
                sectionTable[currentSection].data.push_back(0xff & (val >> 8));
                sectionTable[currentSection].data.push_back(val & 0xff);
                position+=5;
            } else {
                regDescr |= 0xf;
                adrMode = 0;
                int val = parser.getNumberFromLiteral(oprnd);
                sectionTable[currentSection].offsets.push_back(position);
                sectionTable[currentSection].data.push_back(instrDesc);
                sectionTable[currentSection].data.push_back(regDescr);
                sectionTable[currentSection].data.push_back(adrMode);
                sectionTable[currentSection].data.push_back(0xff & (val >> 8));
                sectionTable[currentSection].data.push_back(val & 0xff);
                position+=5;
            }
        } else
        if(parser.pcRelAddress(oprnd)) {
            regDescr |= 0x7;
            adrMode = 0x03;
            int val = handlePcRelSymbol(oprnd);
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDesc);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            sectionTable[currentSection].data.push_back(0xff & (val >> 8));
            sectionTable[currentSection].data.push_back(val & 0xff);
            position+=5;
        } else
        if(parser.regDirAddress(oprnd)) {
            regDescr |= parser.getReg(oprnd);
            adrMode = 0x01;
            int val = handlePcRelSymbol(oprnd);
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDesc);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            position+=3;
        } else
        if(parser.regIndAddress(oprnd)) {
            regDescr |= parser.getReg(oprnd);
            adrMode = 0x02;
            int val = handlePcRelSymbol(oprnd);
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDesc);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            position+=3;
        } else
        if(parser.regIndDispAddress(oprnd)) {
            regDescr |= parser.getReg(oprnd);
            adrMode = 0x03;
            int val;
            if(parser.isSymbol(oprnd)) {
                val = handleAbsoluteSymbol(oprnd);
            } else {
                val = parser.getNumberFromLiteral(oprnd);
            }
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDesc);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            sectionTable[currentSection].data.push_back(0xff & (val >> 8));
            sectionTable[currentSection].data.push_back(val & 0xff);
            position+=5;
        } else
        if(parser.memDirAddress(oprnd)) {
            regDescr |= 0xf;
            adrMode = 0x04;
            int val;
            if(parser.isSymbol(oprnd)) {
                val = handleAbsoluteSymbol(oprnd);
            } else {
                val = parser.getNumberFromLiteral(oprnd);
            }
            sectionTable[currentSection].offsets.push_back(position);
            sectionTable[currentSection].data.push_back(instrDesc);
            sectionTable[currentSection].data.push_back(regDescr);
            sectionTable[currentSection].data.push_back(adrMode);
            sectionTable[currentSection].data.push_back(0xff & (val >> 8));
            sectionTable[currentSection].data.push_back(val & 0xff);
            position+=5;
        }
    } else
    if(parser.twoOperRegInstr(left)) {
        string regD = parser.getFirstBeforeComma(right);
        string regS = parser.getAfterComma(right);
        
        char instrDescr;
        if(left == "xchg") instrDescr = 0x60; else
        if(left == "add") instrDescr = 0x70; else
        if(left == "sub") instrDescr = 0x71; else
        if(left == "mul") instrDescr = 0x72; else
        if(left == "div") instrDescr = 0x73; else
        if(left == "cmp") instrDescr = 0x74; else
        if(left == "and") instrDescr = 0x81; else
        if(left == "or") instrDescr = 0x82; else
        if(left == "xor") instrDescr = 0x83; else
        if(left == "test") instrDescr = 0x84; else
        if(left == "shl") instrDescr = 0x90; else
        if(left == "shr") instrDescr = 0x91;


        char regDescr = (parser.getReg(regD) << 4) | parser.getReg(regS);

        sectionTable[currentSection].offsets.push_back(position);
        sectionTable[currentSection].data.push_back(instrDescr);
        sectionTable[currentSection].data.push_back(regDescr);
        position += 2;
    }

    return true;
}


int Assembler::handlePcRelSymbol(const string& symbol) {
    SymbolEntry& se = symbolTable[parser.getSymOrLit(symbol)];
    //cout << symbol << endl;

    if(se.section == "ABSOLUTE") {
        RelocationEntry re;
        re.isData = false;
        re.offset = position + 4;
        re.section = currentSection;
        re.type = "R_SS_16_PC";
        re.symbolName = se.name;
        relocationTable.push_back(re);
        return -2;
    }

    //Regular section
    RelocationEntry re;
    re.isData = false;
    re.offset = position + 4;
    re.section = currentSection;
    re.type = "R_SS_16_PC";
    re.symbolName = se.name;

    if(se.isGlobal || se.isExtern) {
        re.symbolName = se.name;
        relocationTable.push_back(re);
        return -2; // leave the value to the linker
    }  

    if(currentSection == se.section) {
        return se.value - position - 5;
    }

    re.symbolName = se.section;
    relocationTable.push_back(re);
    return se.value - 2;
}

int Assembler::handleAbsoluteSymbol(const string& symbol) {
    SymbolEntry& se = symbolTable[symbol];

    if(se.section == "ABSOLUTE") return se.value;

    RelocationEntry re; // Create relocation entry as not abs
    re.isData = false;
    re.offset = position + 4; // opcode, regs, ua, higher, lower
    re.section = currentSection;
    re.type = "R_SS_16";

    // value not needed
    if(se.isGlobal || se.isExtern) {
        re.symbolName = se.name;
        relocationTable.push_back(re);
        return 0;
    }

    // put real data
    re.symbolName = se.section;
    relocationTable.push_back(re);
    return se.value;
}

void Assembler::createTxt(ostream& out) {
    out << "SECTION TABLE" << endl
        << left << setw(10) << "NAME"
        << left << setw(10) << "SIZE" 
        << endl;
    map<string, SectionEntry>::iterator it2;
    for (it2 = sectionTable.begin(); it2 != sectionTable.end(); it2++)
    {
        SectionEntry& se = it2->second;
        out << left << setw(10) << se.name
            << right << setw(4) << setfill('0') << hex << se.size
            << endl  << setfill(' ');
    }

    out << endl << "SYMBOL TABLE" << endl
        << left << setw(14) << "NAME"
        << left << setw(10) << "SECTION"
        << left << setw(10) << "VALUE"
        << left << setw(10) << "TYPE"
        << endl;
    map<string, SymbolEntry>::iterator it;
    for (it = symbolTable.begin(); it != symbolTable.end(); it++)
    {
        SymbolEntry& se = it->second;
        if(se.name == "") continue;
        out << left << setw(14) << se.name 
            << left << setw(10) << se.section
            << right << setw(4) << setfill('0') << hex << se.value << "      "
            << left << (se.isGlobal ? "G" : (se.isExtern ? "E" : "L"))
            << endl << setfill(' ');
    }

    map<string, SectionEntry>::iterator secIt;
    for (secIt = sectionTable.begin(); secIt != sectionTable.end(); secIt++)
    {
        SectionEntry& se = secIt->second;
        out << endl << endl << "SECTION " << se.name << endl;
        
        out << "RELOCATION:" << endl;
        out << left << setw(12) << "SYMBOL" << setw(10) << "OFFSET" << setw(12) << "R_TYPE" << setw(14) << "DAT/INSTR" << endl;
        for(auto& re : relocationTable) {
            if(re.section != se.name) continue;
            out << left << setw(12) << re.symbolName << setw(10) << re.offset << setw(12) << re.type << setw(14) << (re.isData ? "DAT" : "INS") << endl;
        }

        out << "DATA:" << endl;
        if(se.offsets.size() == 0) continue; // skip printout if empty
        for(int i = 0; i < se.offsets.size() - 1; i++) {
            int currOff = se.offsets[i];
            int nextOff = se.offsets[i+1];
            out << right << setw(4) << setfill('0') << hex << (0xffff & currOff) << ": ";
            for (int j = currOff; j < nextOff; j++)
            {
                char c = se.data[j];
                out << hex << setw(2) << (0xff & c) << " ";
            }
            out << endl;
        }
        //last one
        out << right << hex << setw(4) << (0xffff & se.offsets[se.offsets.size() - 1]) << ": ";
        for (int j = se.offsets[(int)se.offsets.size() - 1]; j < se.data.size(); j++)
        {
            char c = se.data[j];
            out << hex << setw(2) << (0xff & c) << " ";
        }
        out << endl << setfill(' ');
    }
    out << dec;
}

void Assembler::createBin(ofstream& out) {
    //write sym table size
    size_t symTSize = symbolTable.size();
    out.write((char*)&symTSize, sizeof(symTSize));
    //write sym table
    for(auto& se : symbolTable) {
        //name
        size_t len = se.second.name.length();
        out.write((char*)&len, sizeof(len));
        out.write(se.second.name.c_str(), len);

        //section
        len = se.second.section.length();
        out.write((char*)&len, sizeof(len));
        out.write(se.second.section.c_str(), len);

        //value
        out.write((char*)&se.second.value, sizeof(se.second.value));
        //defined
        out.write((char*)&se.second.isDefined, sizeof(se.second.isDefined));
        //global
        out.write((char*)&se.second.isGlobal, sizeof(se.second.isGlobal));
        //extern
        out.write((char*)&se.second.isExtern, sizeof(se.second.isExtern));
    }

    //write section table size
    size_t secTSize = sectionTable.size();
    out.write((char*)&secTSize, sizeof(secTSize));
    //write section table
    for(auto& se : sectionTable) {
        //name
        size_t len = se.second.name.length();
        out.write((char*)&len, sizeof(len));
        out.write(se.second.name.c_str(), len);

        out.write((char*)&se.second.size, sizeof(se.second.size)); // write size
        for(auto& dat : se.second.data) {   // write data
            out.write((char*)&dat, sizeof(dat));
        }
        size_t offSize = se.second.offsets.size();
        out.write((char*)&offSize, sizeof(offSize)); // write size of offsets
        for(auto& off : se.second.offsets) {    // write offsets
            out.write((char*)&off, sizeof(off));
        }
    }

    //write reloc table size
    size_t relTSize = relocationTable.size();
    out.write((char*)&relTSize, sizeof(relTSize));
    //write reloc table
    for(auto& re : relocationTable) {
        size_t len;
        len = re.section.length();
        out.write((char*)&len, sizeof(len));
        out.write(re.section.c_str(), len);
        
        out.write((char*)&re.size, sizeof(re.size));

        out.write((char*)&re.offset, sizeof(re.offset));

        len = re.type.length();
        out.write((char*)&len, sizeof(len));
        out.write(re.type.c_str(), len);

        len = re.symbolName.length();
        out.write((char*)&len, sizeof(len));
        out.write(re.symbolName.c_str(), len);

        out.write((char*)&re.isData, sizeof(re.isData));

        // string section;
        // size_t size;
        // int offset;
        // string type;
        // string symbolName;
        // bool isData;
    }

    

    out.close();
}