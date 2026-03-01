#include "command_parser.h"
#include "book_system.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <regex>

CommandParser::CommandParser(AccountSystem* account_system, BookSystem* book_system)
    : account_system_(account_system), book_system_(book_system), running_(true) {}

std::vector<std::string> CommandParser::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;

    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ' ' && !in_quotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

bool CommandParser::isValidUserId(const std::string& s) {
    if (s.empty() || s.length() > 30) return false;
    for (char c : s) {
        if (!isalnum(c) && c != '_') return false;
    }
    return true;
}

bool CommandParser::isValidPassword(const std::string& s) {
    if (s.empty() || s.length() > 30) return false;
    for (char c : s) {
        if (!isalnum(c) && c != '_') return false;
    }
    return true;
}

bool CommandParser::isValidUsername(const std::string& s) {
    if (s.empty() || s.length() > 30) return false;
    for (char c : s) {
        if (iscntrl((unsigned char)c)) return false;
    }
    return true;
}

bool CommandParser::isValidPrivilege(const std::string& s) {
    if (s.length() != 1) return false;
    char c = s[0];
    return c == '1' || c == '3' || c == '7';
}

bool CommandParser::isValidISBN(const std::string& s) {
    if (s.empty() || s.length() > 20) return false;
    for (char c : s) {
        if (iscntrl((unsigned char)c)) return false;
    }
    return true;
}

bool CommandParser::isValidBookName(const std::string& s) {
    if (s.empty() || s.length() > 60) return false;
    for (char c : s) {
        if (iscntrl((unsigned char)c) || c == '"') return false;
    }
    return true;
}

bool CommandParser::isValidKeyword(const std::string& s) {
    if (s.empty() || s.length() > 60) return false;
    for (char c : s) {
        if (iscntrl((unsigned char)c) || c == '"') return false;
    }
    return true;
}

bool CommandParser::isValidQuantity(const std::string& s) {
    if (s.empty() || s.length() > 10) return false;
    for (char c : s) {
        if (!isdigit(c)) return false;
    }
    long long qty = std::stoll(s);
    return qty > 0 && qty <= 2147483647LL;
}

bool CommandParser::isValidPrice(const std::string& s) {
    if (s.empty() || s.length() > 13) return false;
    int dot_count = 0;
    for (char c : s) {
        if (!isdigit(c) && c != '.') return false;
        if (c == '.') dot_count++;
    }
    if (dot_count > 1) return false;
    return true;
}

void CommandParser::execute(const std::string& command) {
    // Skip empty lines
    std::string trimmed = command;
    while (!trimmed.empty() && isspace((unsigned char)trimmed.front())) trimmed.erase(0, 1);
    while (!trimmed.empty() && isspace((unsigned char)trimmed.back())) trimmed.pop_back();

    if (trimmed.empty()) return;

    std::vector<std::string> tokens = tokenize(trimmed);
    if (tokens.empty()) return;

    const std::string& cmd = tokens[0];

    if (cmd == "quit" || cmd == "exit") {
        running_ = false;
        return;
    }

    // Account commands
    if (cmd == "su") {
        executeSu(tokens);
    } else if (cmd == "logout") {
        executeLogout();
    } else if (cmd == "register") {
        executeRegister(tokens);
    } else if (cmd == "passwd") {
        executePasswd(tokens);
    } else if (cmd == "useradd") {
        executeUseradd(tokens);
    } else if (cmd == "delete") {
        executeDelete(tokens);
    }

    // Book commands
    else if (cmd == "show") {
        if (tokens.size() > 1 && tokens[1] == "finance") {
            executeShowFinance(tokens);
        } else {
            executeShow(tokens);
        }
    } else if (cmd == "buy") {
        executeBuy(tokens);
    } else if (cmd == "select") {
        executeSelect(tokens);
    } else if (cmd == "modify") {
        executeModify(tokens);
    } else if (cmd == "import") {
        executeImport(tokens);
    }

    // Log commands
    else if (cmd == "log") {
        executeLog();
    } else if (cmd == "report") {
        if (tokens.size() > 1 && tokens[1] == "finance") {
            executeReportFinance();
        } else if (tokens.size() > 1 && tokens[1] == "employee") {
            executeReportEmployee();
        } else {
            std::cout << "Invalid\n";
        }
    }

    else {
        std::cout << "Invalid\n";
    }
}

bool CommandParser::isRunning() const {
    return running_;
}

void CommandParser::stop() {
    running_ = false;
}

void CommandParser::executeSu(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2 || tokens.size() > 3) {
        std::cout << "Invalid\n";
        return;
    }

    if (!isValidUserId(tokens[1])) {
        std::cout << "Invalid\n";
        return;
    }

    if (tokens.size() == 3) {
        if (!isValidPassword(tokens[2])) {
            std::cout << "Invalid\n";
            return;
        }
        if (!account_system_->login(tokens[1], tokens[2])) {
            std::cout << "Invalid\n";
        }
    } else {
        // No password - check if current privilege is higher
        if (!account_system_->isLoggedIn()) {
            std::cout << "Invalid\n";
            return;
        }
        if (!account_system_->loginWithoutPassword(tokens[1])) {
            std::cout << "Invalid\n";
        }
    }
}

void CommandParser::executeLogout() {
    if (account_system_->getCurrentPrivilege() < 1) {
        std::cout << "Invalid\n";
        return;
    }
    if (!account_system_->logout()) {
        std::cout << "Invalid\n";
    }
}

void CommandParser::executeRegister(const std::vector<std::string>& tokens) {
    if (tokens.size() != 4) {
        std::cout << "Invalid\n";
        return;
    }

    if (!isValidUserId(tokens[1]) || !isValidPassword(tokens[2]) || !isValidUsername(tokens[3])) {
        std::cout << "Invalid\n";
        return;
    }

    if (!account_system_->registerAccount(tokens[1], tokens[2], tokens[3])) {
        std::cout << "Invalid\n";
    }
}

void CommandParser::executePasswd(const std::vector<std::string>& tokens) {
    if (tokens.size() != 3 && tokens.size() != 4) {
        std::cout << "Invalid\n";
        return;
    }

    if (!isValidUserId(tokens[1]) || !isValidPassword(tokens.back())) {
        std::cout << "Invalid\n";
        return;
    }

    if (account_system_->getCurrentPrivilege() < 1) {
        std::cout << "Invalid\n";
        return;
    }

    bool success;
    if (tokens.size() == 3) {
        // Root can skip current password
        if (account_system_->getCurrentPrivilege() != 7) {
            std::cout << "Invalid\n";
            return;
        }
        success = account_system_->changePassword(tokens[1], tokens[2]);
    } else {
        if (!isValidPassword(tokens[2])) {
            std::cout << "Invalid\n";
            return;
        }
        success = account_system_->changePassword(tokens[1], tokens[2], tokens[3]);
    }

    if (!success) {
        std::cout << "Invalid\n";
    }
}

void CommandParser::executeUseradd(const std::vector<std::string>& tokens) {
    if (tokens.size() != 5) {
        std::cout << "Invalid\n";
        return;
    }

    if (!isValidUserId(tokens[1]) || !isValidPassword(tokens[2]) ||
        !isValidPrivilege(tokens[3]) || !isValidUsername(tokens[4])) {
        std::cout << "Invalid\n";
        return;
    }

    if (account_system_->getCurrentPrivilege() < 3) {
        std::cout << "Invalid\n";
        return;
    }

    int privilege = std::stoi(tokens[3]);
    if (!account_system_->addAccount(tokens[1], tokens[2], privilege, tokens[4])) {
        std::cout << "Invalid\n";
    }
}

void CommandParser::executeDelete(const std::vector<std::string>& tokens) {
    if (tokens.size() != 2) {
        std::cout << "Invalid\n";
        return;
    }

    if (!isValidUserId(tokens[1])) {
        std::cout << "Invalid\n";
        return;
    }

    if (account_system_->getCurrentPrivilege() < 7) {
        std::cout << "Invalid\n";
        return;
    }

    if (!account_system_->deleteAccount(tokens[1])) {
        std::cout << "Invalid\n";
    }
}

bool CommandParser::parseShowArgs(const std::vector<std::string>& tokens, int start,
                                 std::string& isbn, std::string& name,
                                 std::string& author, std::string& keyword) {
    for (size_t i = start; i < tokens.size(); i++) {
        const std::string& arg = tokens[i];

        if (arg.substr(0, 5) == "-ISBN=") {
            isbn = arg.substr(5);
            if (isbn.empty()) return false;
        } else if (arg.substr(0, 6) == "-name=") {
            name = arg.substr(6);
            if (name.empty()) return false;
        } else if (arg.substr(0, 8) == "-author=") {
            author = arg.substr(8);
            if (author.empty()) return false;
        } else if (arg.substr(0, 9) == "-keyword=") {
            keyword = arg.substr(9);
            if (keyword.empty()) return false;
            // Check for multiple keywords (contains |)
            if (keyword.find('|') != std::string::npos) return false;
        } else {
            return false;
        }
    }
    return true;
}

void CommandParser::executeShow(const std::vector<std::string>& tokens) {
    if (account_system_->getCurrentPrivilege() < 1) {
        std::cout << "Invalid\n";
        return;
    }

    std::string isbn, name, author, keyword;
    if (tokens.size() > 1) {
        if (!parseShowArgs(tokens, 1, isbn, name, author, keyword)) {
            std::cout << "Invalid\n";
            return;
        }
    }

    auto books = book_system_->showBooks(isbn, name, author, keyword);

    if (books.empty()) {
        std::cout << "\n";
        return;
    }

    for (const auto& book : books) {
        std::cout << book.isbn.c_str() << "\t"
                  << book.name.c_str() << "\t"
                  << book.author.c_str() << "\t"
                  << book.keywords.c_str() << "\t"
                  << book.getPriceString() << "\t"
                  << book.quantity << "\n";
    }
}

void CommandParser::executeBuy(const std::vector<std::string>& tokens) {
    if (tokens.size() != 3) {
        std::cout << "Invalid\n";
        return;
    }

    if (account_system_->getCurrentPrivilege() < 1) {
        std::cout << "Invalid\n";
        return;
    }

    if (!isValidISBN(tokens[1]) || !isValidQuantity(tokens[1])) {
        // ISBN might not be valid ISBN format, just check it's not empty
        if (tokens[1].empty() || tokens[1].length() > 20) {
            std::cout << "Invalid\n";
            return;
        }
    }

    if (!isValidQuantity(tokens[2])) {
        std::cout << "Invalid\n";
        return;
    }

    long long qty = std::stoll(tokens[2]);
    long long cost = book_system_->buyBook(tokens[1], qty);

    if (cost < 0) {
        std::cout << "Invalid\n";
        return;
    }

    // Output cost
    std::cout << (cost / 100) << "." << std::setfill('0') << std::setw(2) << (cost % 100) << "\n";
}

void CommandParser::executeSelect(const std::vector<std::string>& tokens) {
    if (tokens.size() != 2) {
        std::cout << "Invalid\n";
        return;
    }

    if (account_system_->getCurrentPrivilege() < 3) {
        std::cout << "Invalid\n";
        return;
    }

    // Validate ISBN
    if (tokens[1].empty() || tokens[1].length() > 20) {
        std::cout << "Invalid\n";
        return;
    }

    book_system_->selectBook(tokens[1]);
}

bool CommandParser::parseModifyArgs(const std::vector<std::string>& tokens, int start,
                                   std::string& isbn, std::string& name,
                                   std::string& author, std::string& keyword,
                                   std::string& price,
                                   bool& has_isbn, bool& has_name,
                                   bool& has_author, bool& has_keywords, bool& has_price) {
    has_isbn = has_name = has_author = has_keywords = has_price = false;

    for (size_t i = start; i < tokens.size(); i++) {
        const std::string& arg = tokens[i];

        if (arg.substr(0, 5) == "-ISBN=") {
            if (has_isbn) return false; // Duplicate
            isbn = arg.substr(5);
            if (isbn.empty()) return false;
            has_isbn = true;
        } else if (arg.substr(0, 6) == "-name=") {
            if (has_name) return false;
            name = arg.substr(6);
            if (name.empty()) return false;
            has_name = true;
        } else if (arg.substr(0, 8) == "-author=") {
            if (has_author) return false;
            author = arg.substr(8);
            if (author.empty()) return false;
            has_author = true;
        } else if (arg.substr(0, 9) == "-keyword=") {
            if (has_keywords) return false;
            keyword = arg.substr(9);
            if (keyword.empty()) return false;
            has_keywords = true;
        } else if (arg.substr(0, 7) == "-price=") {
            if (has_price) return false;
            price = arg.substr(7);
            if (price.empty()) return false;
            has_price = true;
        } else {
            return false;
        }
    }

    // At least one parameter must be present
    return has_isbn || has_name || has_author || has_keywords || has_price;
}

void CommandParser::executeModify(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        std::cout << "Invalid\n";
        return;
    }

    if (account_system_->getCurrentPrivilege() < 3) {
        std::cout << "Invalid\n";
        return;
    }

    std::string selected_isbn = account_system_->getSelectedBook();
    if (selected_isbn.empty()) {
        std::cout << "Invalid\n";
        return;
    }

    std::string new_isbn, new_name, new_author, new_keywords, new_price;
    bool has_isbn, has_name, has_author, has_keywords, has_price;

    if (!parseModifyArgs(tokens, 1, new_isbn, new_name, new_author, new_keywords, new_price,
                        has_isbn, has_name, has_author, has_keywords, has_price)) {
        std::cout << "Invalid\n";
        return;
    }

    // Validate new values
    if (has_isbn && (new_isbn.empty() || new_isbn.length() > 20)) {
        std::cout << "Invalid\n";
        return;
    }
    if (has_name && (new_name.empty() || new_name.length() > 60)) {
        std::cout << "Invalid\n";
        return;
    }
    if (has_author && (new_author.empty() || new_author.length() > 60)) {
        std::cout << "Invalid\n";
        return;
    }
    if (has_keywords && (new_keywords.empty() || new_keywords.length() > 60)) {
        std::cout << "Invalid\n";
        return;
    }
    if (has_price && !isValidPrice(new_price)) {
        std::cout << "Invalid\n";
        return;
    }

    // Get selected book
    Book* book = book_system_->selectBook(selected_isbn);
    if (!book) {
        std::cout << "Invalid\n";
        return;
    }

    if (!book_system_->modifyBook(*book, new_isbn, new_name, new_author, new_keywords, new_price,
                                  has_isbn, has_name, has_author, has_keywords, has_price)) {
        std::cout << "Invalid\n";
    }
}

void CommandParser::executeImport(const std::vector<std::string>& tokens) {
    if (tokens.size() != 3) {
        std::cout << "Invalid\n";
        return;
    }

    if (account_system_->getCurrentPrivilege() < 3) {
        std::cout << "Invalid\n";
        return;
    }

    std::string selected_isbn = account_system_->getSelectedBook();
    if (selected_isbn.empty()) {
        std::cout << "Invalid\n";
        return;
    }

    // Validate quantity and total cost
    if (tokens[1].empty() || tokens[1].length() > 10) {
        std::cout << "Invalid\n";
        return;
    }
    for (char c : tokens[1]) {
        if (!isdigit(c)) {
            std::cout << "Invalid\n";
            return;
        }
    }

    if (!isValidPrice(tokens[2])) {
        std::cout << "Invalid\n";
        return;
    }

    long long qty = std::stoll(tokens[1]);
    if (qty <= 0) {
        std::cout << "Invalid\n";
        return;
    }

    // Parse total cost
    size_t dot = tokens[2].find('.');
    long long total_cost;
    if (dot == std::string::npos) {
        total_cost = std::stoll(tokens[2]) * 100;
    } else {
        long long dollars = std::stoll(tokens[2].substr(0, dot));
        std::string cent_str = tokens[2].substr(dot + 1);
        while (cent_str.length() < 2) cent_str += "0";
        if (cent_str.length() > 2) cent_str = cent_str.substr(0, 2);
        total_cost = dollars * 100 + std::stoll(cent_str);
    }

    if (total_cost <= 0) {
        std::cout << "Invalid\n";
        return;
    }

    // Get selected book
    Book* book = book_system_->selectBook(selected_isbn);
    if (!book) {
        std::cout << "Invalid\n";
        return;
    }

    if (!book_system_->importBook(*book, qty, total_cost)) {
        std::cout << "Invalid\n";
    }
}

void CommandParser::executeShowFinance(const std::vector<std::string>& tokens) {
    if (account_system_->getCurrentPrivilege() < 7) {
        std::cout << "Invalid\n";
        return;
    }

    int count = -1; // -1 means all
    if (tokens.size() > 2) {
        std::cout << "Invalid\n";
        return;
    }

    if (tokens.size() == 2) {
        // Parse count
        if (tokens[1] == "finance") {
            // show finance (without count)
            count = -1;
        } else {
            std::cout << "Invalid\n";
            return;
        }
    } else if (tokens.size() == 3) {
        // show finance [Count]
        for (char c : tokens[2]) {
            if (!isdigit(c)) {
                std::cout << "Invalid\n";
                return;
            }
        }
        count = std::stoi(tokens[2]);
    }

    if (count == 0) {
        std::cout << "\n";
        return;
    }

    auto result = book_system_->getFinance(count);
    if (result.first < 0 && result.second < 0) {
        std::cout << "Invalid\n";
        return;
    }

    long long income = result.first;
    long long expense = result.second;

    std::cout << "+ " << (income / 100) << "." << std::setfill('0') << std::setw(2) << (income % 100)
              << " - " << (expense / 100) << "." << std::setfill('0') << std::setw(2) << (expense % 100)
              << "\n";
}

void CommandParser::executeLog() {
    if (account_system_->getCurrentPrivilege() < 7) {
        std::cout << "Invalid\n";
        return;
    }
    std::cout << book_system_->generateLogReport();
}

void CommandParser::executeReportFinance() {
    if (account_system_->getCurrentPrivilege() < 7) {
        std::cout << "Invalid\n";
        return;
    }
    std::cout << book_system_->generateFinanceReport();
}

void CommandParser::executeReportEmployee() {
    if (account_system_->getCurrentPrivilege() < 7) {
        std::cout << "Invalid\n";
        return;
    }
    std::cout << book_system_->generateEmployeeReport();
}
