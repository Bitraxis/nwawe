#include "nwawe/runtime.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("unable to open file: " + path);
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void printUsage() {
    std::cout << "usage: nwawe <script.nw>\n"
              << "       nwawe --repl\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            printUsage();
            return 0;
        }

        const std::string argument = argv[1];
        if (argument == "--repl") {
            std::cout << "nwawe repl. type :quit to exit\n";
            nwawe::Interpreter interpreter;
            interpreter.setInput(&std::cin);
            interpreter.setOutput(&std::cout);

            std::string line;
            while (std::getline(std::cin, line)) {
                if (line == ":quit") {
                    break;
                }
                if (line.empty()) {
                    continue;
                }
                nwawe::runSource(line, std::cin, std::cout);
            }
            return 0;
        }

        const std::string source = readFile(argument);
        return nwawe::runSource(source, std::cin, std::cout);
    } catch (const std::exception& exception) {
        std::cerr << "error: " << exception.what() << '\n';
        return 1;
    }
}
