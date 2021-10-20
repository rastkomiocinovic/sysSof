#include "../inc/linker.h"
#include <iostream>
#include <iomanip>
#include <fstream>

void Linker::link() {
    if(!loadData()) return;
    if(!createSections()) return;
    if(!createSymbolTable()) return;
    if(!createRelocationTable()) return;
    if(!relocate()) return;

    ofstream outputStream;
    outputStream.open(outputFile, ofstream::out | ofstream::trunc);
    if(!outputStream.is_open()) {
        cout << "Couldn't open output file!" << endl;
        return;
    }
    createTxt(outputStream);
    outputStream.close();

    if(hexOut) {
        outputStream.open("bin_" + outputFile, ofstream::out | ofstream::binary | ofstream::trunc);
        if(!outputStream.is_open()) {
            cout << "Couldn't open output binary file!" << endl;
            return;
        }
        createBin(outputStream);
        //outputStram.close();
    }
}

bool Linker::relocate() {

    for(auto& re : relocationTable) {
        SymbolEntry& se = symbolTable[re.symbolName];

        if(linkableOut) {
            if(se.name == se.section) continue; // No need to relocate sections
            
            short val;
            if(re.isData) { // Little Endian
                val = (short)sectionTable[re.section].data[re.offset + 1] << 8 
                    + (short)sectionTable[re.section].data[re.offset] 
                    + (short)sectionInfoTable[re.file][re.section].offset;
                sectionTable[re.section].data[re.offset] = val & 0xff;
                sectionTable[re.section].data[re.offset + 1] = (val >> 8) & 0xff;
            } else { // Big Endian
                val = (short)sectionTable[re.section].data[re.offset] << 8 
                    + (short)sectionTable[re.section].data[re.offset - 1] 
                    + (short)sectionInfoTable[re.file][re.section].offset;
                sectionTable[re.section].data[re.offset] = (val >> 8) & 0xff;
                sectionTable[re.section].data[re.offset - 1] = val & 0xff;
            }
        } else { // hex output
            SymbolEntry& seFromFile = symbolTables[re.file][re.symbolName];
            short val;
            if(!seFromFile.isGlobal) {
                val = symbolTable[re.symbolName].value;
            } else {
                val = sectionInfoTable[re.file][re.section].offset;
            }

            int pcRelOffset = 0;
            if(re.type == "R_SS_16_PC") {
                pcRelOffset = re.offset + sectionInfoTable[re.file][re.section].offset + (re.isData ? 0 : -1);
            }

            if(re.isData) {
                int val = (int)(sectionTable[re.section].data[re.offset] & 0xff + ((sectionTable[re.section].data[re.offset + 1] & 0xff) << 8))
                        + (int)symbolTable[re.symbolName].value
                        //+ (int)(symbolTable[re.symbolName].name == symbolTable[re.symbolName].section ? sectionInfoTable[re.file][re.section].offset : symbolTable[re.symbolName].value)
                        - pcRelOffset;
                sectionTable[re.section].data[re.offset] = val & 0xff;
                sectionTable[re.section].data[re.offset + 1] = (val >> 8) & 0xff;
            } else {
                int val = (int)(sectionTable[re.section].data[re.offset] & 0xff + ((sectionTable[re.section].data[re.offset - 1] & 0xff) << 8))
                        + (int)symbolTable[re.symbolName].value
                        //+ (int)(symbolTable[re.symbolName].name == symbolTable[re.symbolName].section ? sectionInfoTable[re.file][re.section].offset : symbolTable[re.symbolName].value)
                        - pcRelOffset;

                
                sectionTable[re.section].data[re.offset] = val & 0xff;
                sectionTable[re.section].data[re.offset - 1] = (val >> 8) & 0xff;
            }
        }
    }

    return true;
}

bool Linker::createSections() {
    for(auto& st : sectionTables) { // gather info
        for(auto& se : st.second) {
            SectionInfo si;
            si.size = se.second.size;
            si.offset = sectionTable[se.second.name].size;
            sectionTable[se.second.name].size += si.size;
            sectionTable[se.second.name].name = se.second.name;
            sectionInfoTable[st.first][se.second.name] = si;
        }
    }

    for(auto& stIt : sectionTables) { // copy data
        for(auto& seIt : stIt.second) {
            SectionEntry se = seIt.second;
            SectionInfo si = sectionInfoTable[stIt.first][se.name];
            //cout<< "SECTION " << seIt.first << " FILE " << stIt.first << endl;
            for(auto& off : se.offsets) {
                sectionTable[se.name].offsets.push_back(off + si.offset + sectionTable[se.name].address);
            }
            for(auto& dat : se.data) {
                sectionTable[se.name].data.push_back(dat);
            }
        }
    }

    if(hexOut) {
        int pos = 0;
        for(auto& p : placement) {  // first honor -place
            sectionTable[p.first].address = p.second;
            if(pos > sectionTable[p.first].address) { // not ideal, but it's almost midnight...
                cout << "Invalid positions!! Sections overlap!!" << endl;
                return false;
            }
            pos = sectionTable[p.first].address + sectionTable[p.first].size;
            sectionTable[p.first].placed = true;
        }
        for(auto& se : sectionTable) { // place the rest sequentally
            if(se.second.name == "ABSOLUTE" || se.second.name == "UNDEFINED" | se.second.placed) continue;
            se.second.address = pos;
            pos += se.second.data.size();
        }
    }

    return true;
}

bool Linker::createSymbolTable() {
    for(auto& secEntry : sectionTable) {    // add sections to table
        SymbolEntry sym;
        sym.name = secEntry.second.name;
        sym.section = secEntry.second.name;
        sym.value = secEntry.second.address;
        sym.isDefined = true;
        sym.isGlobal = false;
        symbolTable[sym.name] = sym;
        //cout << hex << secEntry.second.name << " " << secEntry.second.size << endl<< endl;
    }

    vector<string> externSyms;
    for(auto& stIt : symbolTables) {  // go through all files
        //cout << "FILE: " << stIt.first << endl;
        auto& st = stIt.second;
        for(auto& seIt : st) {  // every symbol
            auto& se = seIt.second;
            if(se.name == se.section) continue; // that is not a section
            if(se.isExtern) {
                externSyms.push_back(se.name);
                continue;
            }; // or extern
            if(!se.isDefined) {
                cout << "Multiple definition of symbol:" << se.name << endl;
                return false;
            }
            
            SymbolEntry sym;
            sym.name = se.name;
            sym.section = se.section;
            sym.isDefined = true;
            sym.isGlobal = se.isGlobal;
            sym.value = se.value + (hexOut ? sectionTable[se.section].address : 0); // hex needs abs address
            if(sym.section != "ABSOLUTE") {
                sym.value += sectionInfoTable[stIt.first][sym.section].offset;
            }
            symbolTable[sym.name] = sym; 
            //cout << sym.name << " " << sym.section << " " << sectionTable[sym.section].size << " " << sectionInfoTable[stIt.first][sym.section].offset << endl;
        }
    }
    for(auto& sym : externSyms) {
        if(!symbolTable[sym].isDefined) {
            cout << "Undefined extern symbol: " << sym << endl;
            return false;
        }
    }

    return true;
}

bool Linker::createRelocationTable() {
    for(auto& rtIt : relocationTables) {
        auto& rt = rtIt.second;
        for(auto& rel : rt) {
            RelocationEntry re;
            re.section = rel.section;
            re.offset = rel.offset + sectionInfoTable[rtIt.first][re.section].offset; // - address
            re.type = rel.type;
            re.symbolName = rel.symbolName;
            re.isData = rel.isData;
            re.file = rtIt.first;
            relocationTable.push_back(re);
        }
    }

    return true;
}

bool Linker::loadData() {
    for(auto& file : inputFiles) {
        ifstream in(file, ios::binary);
        if(in.fail()) {
            cout << file << " could not be opened!" << endl;
            return false;
        }

        map<string, SymbolEntry> symbolTable;
        map<string, SectionEntry> sectionTable;
        vector<RelocationEntry> relocationTable;


        //read sym table size
        size_t symTSize = 0;
        in.read((char*)&symTSize, sizeof(symTSize));
        //cout << symTSize << endl;
        //read sym table
        for(size_t i = 0; i < symTSize; i++) {
            SymbolEntry se;
            size_t len;
            // name
            in.read((char*)&len, sizeof(len));
            se.name.resize(len);
            in.read((char*)se.name.c_str(), len);

            // section
            in.read((char*)&len, sizeof(len));
            se.section.resize(len);
            in.read((char*)se.section.c_str(), len);

            // value
            in.read((char*)&se.value, sizeof(se.value));
            //defined
            in.read((char*)&se.isDefined, sizeof(se.isDefined));
            //global
            in.read((char*)&se.isGlobal, sizeof(se.isGlobal));
            //extern
            in.read((char*)&se.isExtern, sizeof(se.isExtern));
            
            symbolTable[se.name] = se;
        }

        //read sec table size
        size_t secTSize = 0;
        in.read((char*)&secTSize, sizeof(secTSize));
        //read sec table
        for(size_t i = 0; i < secTSize; i++) {
            SectionEntry se;
            size_t len;
            // name
            in.read((char*)&len, sizeof(len));
            se.name.resize(len);
            in.read((char*)se.name.c_str(), len);

            

            in.read((char*)&se.size, sizeof(se.size));
            for(int i = 0; i < se.size; i++) {
                char c;
                in.read(&c, sizeof(c));
                //cout << c << endl;
                se.data.push_back(c);
            }
            
            size_t offSize;
            in.read((char*)&offSize, sizeof(offSize));
            for(int i = 0; i < offSize; i++) {
                size_t off;
                in.read((char*)&off, sizeof(off));
                //cout << c << endl;
                se.offsets.push_back(off);
            }

            sectionTable[se.name] = se;
        }

        //read rel table size
        size_t relTSize = 0;
        in.read((char*)&relTSize, sizeof(relTSize));
        //read rel table
        for(size_t i = 0; i < relTSize; i++) {
            RelocationEntry re;
            size_t len;
            in.read((char*)&len, sizeof(len));
            re.section.resize(len);
            in.read((char*)re.section.c_str(), len);

            in.read((char*)&re.size, sizeof(re.size));
            in.read((char*)&re.offset, sizeof(re.offset));

            in.read((char*)&len, sizeof(len));
            re.type.resize(len);
            in.read((char*)re.type.c_str(), len);

            in.read((char*)&len, sizeof(len));
            re.symbolName.resize(len);
            in.read((char*)re.symbolName.c_str(), len);

            in.read((char*)&re.isData, sizeof(re.isData));

            relocationTable.push_back(re);
        }
        in.close();
        sectionTables[file] = sectionTable;
        symbolTables[file] = symbolTable;
        relocationTables[file] = relocationTable;
    }

    return true;
}

void Linker::createTxt(ofstream& out) {
    out << hex << endl << "SECTION TABLE" << endl
        << left << setw(14) << "NAME"
        << left << setw(10) << "SIZE"
        << left << setw(10) << "ADDRESS"
        << endl;
    for(auto& se : sectionTable) {
        out << left << setw(14) << se.second.name
             << left << setw(10) << se.second.size
             << left << setw(10) << se.second.address
             << endl;
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
    int pos = 0;
    for (secIt = sectionTable.begin(); secIt != sectionTable.end(); secIt++)
    {
        SectionEntry& se = secIt->second;
        out << endl << endl << "SECTION " << se.name << " " << secIt->first << endl;
        
        out << "RELOCATION:" << endl;
        out << left << setfill(' ') << setw(12) << "SYMBOL" << setw(10) << "OFFSET" << setw(12) << "R_TYPE" << setw(14) << "DAT/INSTR" << endl;
        for(auto& re : relocationTable) {
            if(re.section != se.name) continue;
            out << hex << left << setw(12) << re.symbolName << setw(10) << re.offset << setw(12) << re.type << setw(14) << (re.isData ? "DAT" : "INS") << endl;
        }

        out << "DATA:" << endl;
        if(linkableOut) {
            if(se.offsets.size() == 0) continue; // skip printout if empty
            for(int i = 0; i < se.offsets.size() - 1; i++) {
                int currOff = se.offsets[i];
                int nextOff = se.offsets[i+1];
                out << hex << right << setw(4) << setfill('0') << (0xffff & currOff) << ": ";
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
        } else {
            int cnt = 0;
            out << hex;
            pos = se.address;
            for(auto& dat : se.data) {
                if(cnt++ % 8 == 0) {
                    out << endl << right << setfill('0') << setw(4) << pos << ":";
                }
                out << hex << setw(2) << (0xff & dat) << " ";
                if(se.name != "ABSOLUTE" && se.name != "UNDEFINED") pos++;
            }
        }
        out << endl << setfill(' ') << dec << endl;
    }
}

void Linker::createBin(ofstream& out) {
    char mem[1 << 16];  // init mem to zero
    for(auto& c : mem) {
        c = 0;
    }

    for(auto& seIt : sectionTable) { // fill mem
        SectionEntry se = seIt.second;
        if(se.name == "ABSOLUTE" || se.name == "UNDEFINED") continue;
        int off = 0;
        for(auto& dat : se.data) {
            mem[se.address + off++] = dat;
        }
    }

    for(auto& c : mem) {
        out << c;
    }
}