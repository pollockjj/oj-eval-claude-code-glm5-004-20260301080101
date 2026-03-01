#ifndef BOOK_SYSTEM_H
#define BOOK_SYSTEM_H

#include <string>
#include <vector>
#include <set>
#include "account_system.h"

// Book structure for file storage
struct Book {
    static const int ISBN_SIZE = 21;
    static const int NAME_SIZE = 61;
    static const int AUTHOR_SIZE = 61;
    static const int KEYWORD_SIZE = 61;

    FixedString<ISBN_SIZE - 1> isbn;
    FixedString<NAME_SIZE - 1> name;
    FixedString<AUTHOR_SIZE - 1> author;
    FixedString<KEYWORD_SIZE - 1> keywords;
    long long quantity;
    long long price_cents; // Store in cents for precision

    Book() : quantity(0), price_cents(0) {}

    std::string getPriceString() const {
        long long dollars = price_cents / 100;
        long long cents = price_cents % 100;
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld.%02lld", dollars, cents);
        return std::string(buf);
    }

    void setPrice(const std::string& price_str) {
        // Parse price string like "49.99"
        size_t dot = price_str.find('.');
        if (dot == std::string::npos) {
            price_cents = std::stoll(price_str) * 100;
        } else {
            long long dollars = std::stoll(price_str.substr(0, dot));
            std::string cent_str = price_str.substr(dot + 1);
            while (cent_str.length() < 2) cent_str += "0";
            if (cent_str.length() > 2) cent_str = cent_str.substr(0, 2);
            price_cents = dollars * 100 + std::stoll(cent_str);
        }
    }

    std::vector<std::string> getKeywordList() const {
        std::vector<std::string> result;
        std::string kw = keywords.str();
        if (kw.empty()) return result;

        size_t start = 0, end;
        while ((end = kw.find('|', start)) != std::string::npos) {
            result.push_back(kw.substr(start, end - start));
            start = end + 1;
        }
        result.push_back(kw.substr(start));
        return result;
    }
};

// Finance record for tracking transactions - POD type for file storage
struct FinanceRecord {
    long long income_cents;
    long long expense_cents;

    FinanceRecord() : income_cents(0), expense_cents(0) {}
};

class BookSystem {
private:
    std::string book_data_file_;
    std::string book_isbn_index_file_;
    std::string book_name_index_file_;
    std::string book_author_index_file_;
    std::string book_keyword_index_file_;
    std::string finance_file_;
    std::string log_file_;

    AccountSystem* account_system_;

    // Parse keywords and validate
    bool parseKeywords(const std::string& kw_str, std::vector<std::string>& keywords);

    // Check for duplicate keywords
    bool hasDuplicateKeywords(const std::string& kw_str);

public:
    BookSystem(AccountSystem* account_system);
    ~BookSystem();

    void initialize();

    // Book commands
    std::vector<Book> showBooks(const std::string& isbn_filter,
                                const std::string& name_filter,
                                const std::string& author_filter,
                                const std::string& keyword_filter);
    long long buyBook(const std::string& isbn, long long quantity); // Returns total cost in cents
    int selectBook(const std::string& isbn); // Returns position
    bool modifyBookByPosition(int book_pos, const std::string& new_isbn,
                   const std::string& new_name, const std::string& new_author,
                   const std::string& new_keywords, const std::string& new_price,
                   bool has_isbn, bool has_name, bool has_author,
                   bool has_keywords, bool has_price);
    bool importBookByPosition(int book_pos, long long quantity, long long total_cost);

    // Finance operations
    void recordSale(long long amount_cents);
    void recordImport(long long amount_cents);
    std::pair<long long, long long> getFinance(int count); // Returns (income, expense) in cents
    int getTotalTransactionCount();

    // Logging
    void logOperation(const std::string& operation);
    std::string generateLogReport();
    std::string generateFinanceReport();
    std::string generateEmployeeReport();
};

#endif // BOOK_SYSTEM_H
