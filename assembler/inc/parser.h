#ifndef PARSER_H
#define PARSER_H
#include <string>
#include <regex>

using namespace std;

class Parser {
private:
    void removeComments(string& line);
    void removeExtraSpaces(string& line);

    string literal = "(-?[0-9]+)|(0x[0-9A-F]+)";
    string symbol = "[a-zA-Z][a-zA-Z0-9_]*";
    string registerRange = "[0-7]";

    // combined helper strings
    string symbolOrLiteral = symbol + "|" + literal;

public:
    string clearLine(string line);
    string removeLabel(string line);
    string getLeft(const string& line);
    string getRight(const string& line);
    string getFirstBeforeComma(const string& line);
    string getAfterComma(const string& line);
    string getLabel(const string& line);
    int getNumberFromLiteral(const string& literal);
    bool containsLabel(const string& line);
    bool labelOnly(const string& line);
    bool isDirective(const string& line);
    bool noOperInstr(const string& instr);
    bool isSymbol(const string& symb);

    //Operands
    bool oneOperRegInstr(const string& instr);
    bool oneOperAllInstr(const string& instr);
    bool twoOperInstr(const string& instr);
    bool twoOperAllInstr(const string& instr);
    bool twoOperRegInstr(const string& instr);

    //Address
    bool absAddress(const string& oprnd);
    bool absAddressJmp(const string& oprnd);
    bool memDirAddress(const string& oprnd);
    bool pcRelAddress(const string& oprnd);
    bool regDirAddress(const string& oprnd);
    bool regIndAddress(const string& oprnd);
    bool regIndDispAddress(const string& oprnd);

    string getSymOrLit(const string& oprnd);
    int getReg(const string& oprnd);
};
#endif //PARSER_H