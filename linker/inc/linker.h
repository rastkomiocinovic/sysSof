#ifndef LINKER_H
#define LINKER_H
#include <string>
#include <map>
#include <vector>

using namespace std;

class Linker {
private:
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
        size_t address = 0;
        bool placed = false;
    };

    struct RelocationEntry {
        string section;
        int offset;
        string type;
        string symbolName;
        bool isData;
        size_t size;
        string file;
    };

    struct SectionInfo {
        size_t size;
        size_t offset;
    };

    string outputFile;
    map<string, int> placement;
    bool hexOut;
    bool linkableOut;
    vector<string> inputFiles;
    map<string, map<string, SymbolEntry>> symbolTables; //[file][symbol]
    map<string, map<string, SectionEntry>> sectionTables; //[file][section]
    map<string, vector<RelocationEntry>> relocationTables; //[file]

    map<string, SymbolEntry> symbolTable;
    map<string, SectionEntry> sectionTable;
    vector<RelocationEntry> relocationTable;
    map<string, map<string, SectionInfo>> sectionInfoTable;//[file][section]

    void createTxt(ofstream& out);
    void createBin(ofstream& out);
public:
    Linker(string outputFile, map<string, int> placement, bool hexOut, bool linkableOut, vector<string> inputFiles) 
        : outputFile(outputFile), placement(placement), hexOut(hexOut), linkableOut(linkableOut), inputFiles(inputFiles) {}
    void link();
    bool loadData();
    bool createSections();
    bool createSymbolTable();
    bool createRelocationTable();
    bool relocate();
};

#endif