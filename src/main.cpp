#include <iostream>
#include <string>
#include "account_system.h"
#include "book_system.h"
#include "command_parser.h"

int main() {
    // Initialize systems
    AccountSystem account_system;
    BookSystem book_system(&account_system);

    account_system.initialize();
    book_system.initialize();

    CommandParser parser(&account_system, &book_system);

    // Main loop
    std::string line;
    while (parser.isRunning() && std::getline(std::cin, line)) {
        parser.execute(line);
    }

    return 0;
}
