#include <string>
#include <iostream>
#include <iterator>
#include "../inc/parser.h"

string Parser::clearLine(string line) {
    string curr = line;

    removeComments(line);
    removeExtraSpaces(line);
    return line;
}

string Parser::removeLabel(string line) {
    regex reg(R"(.*:)");
    line = regex_replace(line, reg, "");
    return line;
}

string Parser::getSymOrLit(const string& oprnd) {
    regex filter(R"((\**.* \+ )|\]|\$|\%)");
    return regex_replace(oprnd, filter, "");
}

void Parser::removeComments(string& line) {
    regex reg(R"(#.*)");
    line = regex_replace(line, reg, "");
}

void Parser::removeExtraSpaces(string& line) {
    regex multipleSpaces(R"( +)");
    regex leadingSpace(R"(^ )");
    regex trailingSpace(R"( $)");
    regex columnWithSpaces(R"( *: *)");
    regex commaWithSpaces(R"( *, *)");
    line = regex_replace(line, multipleSpaces, " ");
    line = regex_replace(line, leadingSpace, "");
    line = regex_replace(line, trailingSpace, "");
    line = regex_replace(line, columnWithSpaces, ":");
    line = regex_replace(line, commaWithSpaces, ",");
}

bool Parser::containsLabel(const string& line) {
    regex filter(R"(.*:.*)");
    return line != regex_replace(line, filter, "");
}

bool Parser::labelOnly(const string& line) {
    regex filter(R"(:.*)");
    return line == regex_replace(line, filter, "") + ":";
}

bool Parser::isDirective(const string& line) {
    return line.at(0) == '.';
}

bool Parser::noOperInstr(const string& instr) {
    regex filter(R"(^(halt|iret|ret))");
    return instr != regex_replace(instr, filter, "");
}

bool Parser::oneOperRegInstr(const string& instr) {
    regex filter(R"(int|push|pop|not)");
    return instr != regex_replace(instr, filter, "");
}

bool Parser::oneOperAllInstr(const string& instr) {
    regex filter(R"(call|jmp|jeq|jne|jgt)");
    return instr != regex_replace(instr, filter, "");
}

bool Parser::twoOperInstr(const string& instr) {
    regex filter(R"(xchg|add|sub|mul|div|cmp|and|or|xor|test|shl|shr|ldr|str)");
    return instr != regex_replace(instr, filter, "");
}

bool Parser::twoOperAllInstr(const string& instr) {
    regex filter(R"(ldr|str)");
    return instr != regex_replace(instr, filter, "");
}

bool Parser::twoOperRegInstr(const string& instr) {
    regex filter(R"(xchg|add|sub|mul|div|cmp|and|or|xor|test|shl|shr)");
    return instr != regex_replace(instr, filter, "");
}

bool Parser::isSymbol(const string& symb) {
    regex filter("^(" + symbol + ")$");
    return regex_match(symb, filter);
}

bool Parser::absAddress(const string& oprnd) {
    regex filter(R"(^\$)");
    //cout << oprnd << endl;
    return oprnd != regex_replace(oprnd, filter, "");
}

bool Parser::absAddressJmp(const string& oprnd) {
    regex filter("^(" + symbolOrLiteral + ")$");
    //cout << oprnd << endl;
    return oprnd != regex_replace(oprnd, filter, "");
}

bool Parser::memDirAddress(const string& oprnd) {   // TODO: Check this
    regex filter("^(" + symbolOrLiteral + ")$");
    return oprnd != regex_replace(oprnd, filter, "");
}

bool Parser::pcRelAddress(const string& oprnd) {
    regex filter(R"(^%)");
    return oprnd != regex_replace(oprnd, filter, "");
}

bool Parser::regDirAddress(const string& oprnd) {
    regex filter(R"(^(r[0-7]|psw)$)");
    return oprnd != regex_replace(oprnd, filter, "");
}

bool Parser::regIndAddress(const string& oprnd) {
    regex filter(R"(^\[(r[0-7]|psw)\]$)");
    return oprnd != regex_replace(oprnd, filter, "");
}

bool Parser::regIndDispAddress(const string& oprnd) {   // TODO: Check this
    regex filter(R"(^\**\[.*]$)");
    return oprnd != regex_replace(oprnd, filter, "");
}

string Parser::getLeft(const string& line) {
    regex filter(R"( .*)");
    return regex_replace(line, filter, "");
}

string Parser::getRight(const string& line) {
    regex filter(R"(^[^\s]* )");
    return regex_replace(line, filter, "");
}

string Parser::getLabel(const string& line) {
    regex filter(R"(:.*)");
    return regex_replace(line, filter, "");
}

string Parser::getFirstBeforeComma(const string& line) {
    regex filter(R"(,.*)");
    regex comma(R"(,)");
    string s = regex_replace(line, filter, "");
    s = regex_replace(s, comma, "");
    return s;
}
string Parser::getAfterComma(const string& line) {
    regex filter(R"(.*?,)");
    string s = regex_replace(line, filter, "");
    return line != s ? s : "";
}

int Parser::getNumberFromLiteral(const string& literal) {
    int ret;
    regex pref(R"(^0x)");
    string noPref = regex_replace(literal, pref, "");
    if(noPref != literal) { // hex
        stringstream ss;
        ss << noPref;
        ss >> hex >> ret;
        return ret;
    }
    // dec
    return stoi(noPref);
}

int Parser::getReg(const string& oprnd) {
    regex pref(R"(r|(\*)|(\[)|(\])|( \+.*))");
    string reg = regex_replace(oprnd, pref, "");
    return stoi((reg == "psw") ? "8" : reg);
}