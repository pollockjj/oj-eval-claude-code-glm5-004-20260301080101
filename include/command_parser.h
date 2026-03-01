#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string>
#include <vector>
#include "account_system.h"
#include "book_system.h"

class CommandParser {
private:
    AccountSystem* account_system_;
    BookSystem* book_system_;
    bool running_;

    // Parse command line into tokens
    std::vector<std::string> tokenize(const std::string& line);

    // Validate user input strings
    bool isValidUserId(const std::string& s);
    bool isValidPassword(const std::string& s);
    bool isValidUsername(const std::string& s);
    bool isValidPrivilege(const std::string& s);
    bool isValidISBN(const std::string& s);
    bool isValidBookName(const std::string& s);
    bool isValidKeyword(const std::string& s);
    bool isValidQuantity(const std::string& s);
    bool isValidPrice(const std::string& s);

    // Parse show/modify arguments
    bool parseShowArgs(const std::vector<std::string>& tokens, int start,
                      std::string& isbn, std::string& name,
                      std::string& author, std::string& keyword);
    bool parseModifyArgs(const std::vector<std::string>& tokens, int start,
                        std::string& isbn, std::string& name,
                        std::string& author, std::string& keyword,
                        std::string& price,
                        bool& has_isbn, bool& has_name,
                        bool& has_author, bool& has_keywords, bool& has_price);

    // Execute commands
    void executeSu(const std::vector<std::string>& tokens);
    void executeLogout();
    void executeRegister(const std::vector<std::string>& tokens);
    void executePasswd(const std::vector<std::string>& tokens);
    void executeUseradd(const std::vector<std::string>& tokens);
    void executeDelete(const std::vector<std::string>& tokens);
    void executeShow(const std::vector<std::string>& tokens);
    void executeBuy(const std::vector<std::string>& tokens);
    void executeSelect(const std::vector<std::string>& tokens);
    void executeModify(const std::vector<std::string>& tokens);
    void executeImport(const std::vector<std::string>& tokens);
    void executeShowFinance(const std::vector<std::string>& tokens);
    void executeLog();
    void executeReportFinance();
    void executeReportEmployee();

public:
    CommandParser(AccountSystem* account_system, BookSystem* book_system);

    // Parse and execute a single command
    void execute(const std::string& command);

    // Check if system should continue running
    bool isRunning() const;

    // Stop the system
    void stop();
};

#endif // COMMAND_PARSER_H
