#ifndef ASSEMBLER_H
#define ASSEMBLER_H
#include <iostream>
#include <fstream>
#include <vector>
#include "parser.h"

using namespace std;

class Assembler {
private:
    string inputFile;
    string outputFile;

    ifstream& inputStream;
    ofstream& outputStream;

    vector<string> lines;

    Parser parser;

    size_t lineNum = 0;
    size_t position= 0;
    string currentSection = "UNDEFINED";

    struct SymbolEntry {
        string name;
        string section;
        int value;
        bool isDefined;
        bool isGlobal;
        bool isExtern;
    };

    struct SectionEntry {
        string name;
        size_t size;
        vector<char> data;
        vector<size_t> offsets;
    };

    struct RelocationEntry {
        string section;
        size_t size;
        int offset;
        string type;
        string symbolName;
        bool isData;
    };

    map<string, SymbolEntry> symbolTable;
    map<string, SectionEntry> sectionTable;
    vector<RelocationEntry> relocationTable;

    bool firstPass();
    bool secondPass();
    bool handleWordFirstPass();
    bool handleWordSecondPass(const string& arg);
    bool handleGlobal(const string& symb);
    bool handleEqu(const string& symb, const string& val);
    bool handleExtern(const string& symb);
    bool handleSkipFirstPass(const string& value);
    bool handleSkipSecondPass(const int& len);
    bool handleSection(const string& section);
    bool handleLabel(const string& label);
    bool handleInstructionFirstPass(const string& line);
    bool handleInstructionSecondPass(const string& line);
    int handleAbsoluteSymbol(const string& symbol);
    int handlePcRelSymbol(const string& symbol);
    void createTxt(ostream& out);
    void createBin(ofstream& out);
public:
    Assembler(const string& inputFile, const string& outputFile);
    bool assemble();
};

#endif //ASSEMBLER_H