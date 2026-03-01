#include "book_system.h"
#include "file_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>
#include <ctime>
#include <map>

// Book index files
class BookIndex {
private:
    std::string filename_;
    std::map<std::string, std::set<int>> index_; // key -> set of positions

    void load() {
        index_.clear();
        std::ifstream file(filename_);
        if (!file.good()) return;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            size_t space = line.find_last_of(' ');
            if (space == std::string::npos) continue;
            std::string key = line.substr(0, space);
            int pos = std::stoi(line.substr(space + 1));
            index_[key].insert(pos);
        }
        file.close();
    }

    void save() {
        std::ofstream file(filename_);
        for (const auto& entry : index_) {
            for (int pos : entry.second) {
                file << entry.first << " " << pos << "\n";
            }
        }
        file.close();
    }

public:
    BookIndex(const std::string& filename) : filename_(filename) {
        load();
    }

    std::set<int> findAll(const std::string& key) {
        auto it = index_.find(key);
        if (it != index_.end()) return it->second;
        return {};
    }

    int findFirst(const std::string& key) {
        auto it = index_.find(key);
        if (it != index_.end() && !it->second.empty()) {
            return *it->second.begin();
        }
        return -1;
    }

    void insert(const std::string& key, int pos) {
        index_[key].insert(pos);
        save();
    }

    void update(const std::string& old_key, const std::string& new_key, int pos) {
        auto it = index_.find(old_key);
        if (it != index_.end()) {
            it->second.erase(pos);
            if (it->second.empty()) {
                index_.erase(it);
            }
        }
        index_[new_key].insert(pos);
        save();
    }

    void remove(const std::string& key, int pos) {
        auto it = index_.find(key);
        if (it != index_.end()) {
            it->second.erase(pos);
            if (it->second.empty()) {
                index_.erase(it);
            }
            save();
        }
    }

    void clear(int pos) {
        // Remove all entries for this position
        for (auto it = index_.begin(); it != index_.end(); ) {
            it->second.erase(pos);
            if (it->second.empty()) {
                it = index_.erase(it);
            } else {
                ++it;
            }
        }
        save();
    }
};

// Global storage
static SimpleStorage<Book>* g_book_storage = nullptr;
static SimpleStorage<FinanceRecord>* g_finance_storage = nullptr;
static BookIndex* g_isbn_index = nullptr;
static BookIndex* g_name_index = nullptr;
static BookIndex* g_author_index = nullptr;
static BookIndex* g_keyword_index = nullptr;
static std::ofstream* g_log_file = nullptr;
static std::string g_log_filename = "bookstore.log";

// Map to store book positions by ISBN for selected books
static std::map<std::string, int> g_book_positions;

BookSystem::BookSystem(AccountSystem* account_system)
    : account_system_(account_system),
      book_data_file_("book.dat"),
      book_isbn_index_file_("book_isbn.idx"),
      book_name_index_file_("book_name.idx"),
      book_author_index_file_("book_author.idx"),
      book_keyword_index_file_("book_keyword.idx"),
      finance_file_("finance.dat") {
}

BookSystem::~BookSystem() {
    delete g_book_storage;
    delete g_finance_storage;
    delete g_isbn_index;
    delete g_name_index;
    delete g_author_index;
    delete g_keyword_index;
    if (g_log_file) {
        g_log_file->close();
        delete g_log_file;
    }
}

void BookSystem::initialize() {
    g_book_storage = new SimpleStorage<Book>(book_data_file_);
    g_book_storage->initialize();

    g_finance_storage = new SimpleStorage<FinanceRecord>(finance_file_);
    g_finance_storage->initialize();

    g_isbn_index = new BookIndex(book_isbn_index_file_);
    g_name_index = new BookIndex(book_name_index_file_);
    g_author_index = new BookIndex(book_author_index_file_);
    g_keyword_index = new BookIndex(book_keyword_index_file_);

    g_log_file = new std::ofstream(g_log_filename, std::ios::app);
}

bool BookSystem::parseKeywords(const std::string& kw_str, std::vector<std::string>& keywords) {
    if (kw_str.empty()) {
        keywords.clear();
        return true;
    }

    std::string kw = kw_str;
    size_t start = 0, end;
    while ((end = kw.find('|', start)) != std::string::npos) {
        std::string segment = kw.substr(start, end - start);
        if (segment.empty()) return false;
        keywords.push_back(segment);
        start = end + 1;
    }
    std::string last = kw.substr(start);
    if (last.empty()) return false;
    keywords.push_back(last);
    return true;
}

bool BookSystem::hasDuplicateKeywords(const std::string& kw_str) {
    std::vector<std::string> keywords;
    if (!parseKeywords(kw_str, keywords)) return false;

    std::set<std::string> unique_kw(keywords.begin(), keywords.end());
    return unique_kw.size() != keywords.size();
}

// Helper function to find book position by ISBN
static int findBookPositionByISBN(const std::string& isbn) {
    auto positions = g_isbn_index->findAll(isbn);
    for (int pos : positions) {
        Book book;
        if (g_book_storage->read(pos, book) && book.isbn.str() == isbn) {
            return pos;
        }
    }
    return -1;
}

// Helper function to get book by position
static bool getBookByPosition(int pos, Book& book) {
    if (pos < 0) return false;
    return g_book_storage->read(pos, book);
}

std::vector<Book> BookSystem::showBooks(const std::string& isbn_filter,
                                        const std::string& name_filter,
                                        const std::string& author_filter,
                                        const std::string& keyword_filter) {
    std::vector<Book> result;
    std::set<int> candidate_positions;

    // If no filter, return all books sorted by ISBN
    if (isbn_filter.empty() && name_filter.empty() &&
        author_filter.empty() && keyword_filter.empty()) {
        // Get all books and check which ones are valid (have ISBN in index)
        std::vector<Book> all_books = g_book_storage->getAll();
        for (size_t i = 0; i < all_books.size(); i++) {
            Book book = all_books[i];
            // Verify book exists in ISBN index
            auto positions = g_isbn_index->findAll(book.isbn.str());
            for (int pos : positions) {
                Book stored;
                if (g_book_storage->read(pos, stored) && stored.isbn.str() == book.isbn.str()) {
                    result.push_back(book);
                    break;
                }
            }
        }
        // Sort by ISBN
        std::sort(result.begin(), result.end(),
            [](const Book& a, const Book& b) { return a.isbn.str() < b.isbn.str(); });
        return result;
    }

    // Start with ISBN filter if present
    if (!isbn_filter.empty()) {
        candidate_positions = g_isbn_index->findAll(isbn_filter);
    }

    // Filter by name
    if (!name_filter.empty()) {
        auto name_positions = g_name_index->findAll(name_filter);
        if (candidate_positions.empty()) {
            candidate_positions = name_positions;
        } else {
            std::set<int> intersection;
            for (int p : candidate_positions) {
                if (name_positions.count(p)) intersection.insert(p);
            }
            candidate_positions = intersection;
        }
    }

    // Filter by author
    if (!author_filter.empty()) {
        auto author_positions = g_author_index->findAll(author_filter);
        if (candidate_positions.empty()) {
            candidate_positions = author_positions;
        } else {
            std::set<int> intersection;
            for (int p : candidate_positions) {
                if (author_positions.count(p)) intersection.insert(p);
            }
            candidate_positions = intersection;
        }
    }

    // Filter by keyword
    if (!keyword_filter.empty()) {
        auto keyword_positions = g_keyword_index->findAll(keyword_filter);
        if (candidate_positions.empty()) {
            candidate_positions = keyword_positions;
        } else {
            std::set<int> intersection;
            for (int p : candidate_positions) {
                if (keyword_positions.count(p)) intersection.insert(p);
            }
            candidate_positions = intersection;
        }
    }

    // Get books from positions and verify
    std::set<std::string> seen_isbns;
    for (int pos : candidate_positions) {
        Book book;
        if (g_book_storage->read(pos, book)) {
            // Avoid duplicates
            if (seen_isbns.find(book.isbn.str()) == seen_isbns.end()) {
                seen_isbns.insert(book.isbn.str());
                result.push_back(book);
            }
        }
    }

    // Sort by ISBN
    std::sort(result.begin(), result.end(),
        [](const Book& a, const Book& b) { return a.isbn.str() < b.isbn.str(); });

    return result;
}

long long BookSystem::buyBook(const std::string& isbn, long long quantity) {
    if (quantity <= 0) return -1;

    int pos = findBookPositionByISBN(isbn);
    if (pos < 0) return -1;

    Book book;
    if (!getBookByPosition(pos, book)) return -1;

    if (book.quantity < quantity) return -1;

    long long total_cost = book.price_cents * quantity;
    book.quantity -= quantity;
    g_book_storage->update(pos, book);

    // Record finance
    recordSale(total_cost);
    return total_cost;
}

int BookSystem::selectBook(const std::string& isbn) {
    int pos = findBookPositionByISBN(isbn);

    if (pos < 0) {
        // Create new book with only ISBN
        Book new_book;
        new_book.isbn = FixedString<Book::ISBN_SIZE - 1>(isbn);
        pos = g_book_storage->write(new_book);
        g_isbn_index->insert(isbn, pos);
    }

    account_system_->setSelectedBookPosition(pos);
    g_book_positions[isbn] = pos;

    return pos;
}

bool BookSystem::modifyBookByPosition(int book_pos, const std::string& new_isbn,
                           const std::string& new_name,
                           const std::string& new_author,
                           const std::string& new_keywords,
                           const std::string& new_price,
                           bool has_isbn, bool has_name, bool has_author,
                           bool has_keywords, bool has_price) {
    // Get the book
    Book book;
    if (!getBookByPosition(book_pos, book)) return false;

    std::string old_isbn = book.isbn.str();

    // Check for ISBN change to same value
    if (has_isbn && new_isbn == old_isbn) return false;

    // Check for ISBN change to existing ISBN
    if (has_isbn) {
        int existing_pos = findBookPositionByISBN(new_isbn);
        if (existing_pos >= 0 && existing_pos != book_pos) {
            return false; // ISBN already exists
        }
    }

    // Check for duplicate keywords
    if (has_keywords && !new_keywords.empty()) {
        if (hasDuplicateKeywords(new_keywords)) return false;
    }

    // Get old values for index updates
    std::string old_name = book.name.str();
    std::string old_author = book.author.str();
    std::string old_keywords = book.keywords.str();

    // Update book
    if (has_isbn) book.isbn = FixedString<Book::ISBN_SIZE - 1>(new_isbn);
    if (has_name) book.name = FixedString<Book::NAME_SIZE - 1>(new_name);
    if (has_author) book.author = FixedString<Book::AUTHOR_SIZE - 1>(new_author);
    if (has_keywords) book.keywords = FixedString<Book::KEYWORD_SIZE - 1>(new_keywords);
    if (has_price) book.setPrice(new_price);

    // Update storage
    g_book_storage->update(book_pos, book);

    // Update indexes
    if (has_isbn && new_isbn != old_isbn) {
        g_isbn_index->update(old_isbn, new_isbn, book_pos);
        g_book_positions.erase(old_isbn);
        g_book_positions[new_isbn] = book_pos;
    }
    if (has_name) {
        if (!old_name.empty()) g_name_index->remove(old_name, book_pos);
        if (!new_name.empty()) g_name_index->insert(new_name, book_pos);
    }
    if (has_author) {
        if (!old_author.empty()) g_author_index->remove(old_author, book_pos);
        if (!new_author.empty()) g_author_index->insert(new_author, book_pos);
    }
    if (has_keywords) {
        // Remove old keywords
        std::vector<std::string> old_kw_list;
        parseKeywords(old_keywords, old_kw_list);
        for (const auto& kw : old_kw_list) {
            g_keyword_index->remove(kw, book_pos);
        }
        // Insert new keywords
        std::vector<std::string> new_kw_list;
        parseKeywords(new_keywords, new_kw_list);
        for (const auto& kw : new_kw_list) {
            g_keyword_index->insert(kw, book_pos);
        }
    }

    return true;
}

bool BookSystem::importBookByPosition(int book_pos, long long quantity, long long total_cost) {
    if (quantity <= 0 || total_cost <= 0) return false;
    if (book_pos < 0) return false;

    Book stored_book;
    if (!getBookByPosition(book_pos, stored_book)) return false;

    stored_book.quantity += quantity;
    g_book_storage->update(book_pos, stored_book);

    // Record finance (import is expense)
    recordImport(total_cost);

    return true;
}

void BookSystem::recordSale(long long amount_cents) {
    FinanceRecord record;
    record.income_cents = amount_cents;
    record.expense_cents = 0;

    g_finance_storage->write(record);

    std::stringstream ss;
    ss << "SALE: +" << (amount_cents / 100) << "." << std::setfill('0') << std::setw(2) << (amount_cents % 100);
    logOperation(ss.str());
}

void BookSystem::recordImport(long long amount_cents) {
    FinanceRecord record;
    record.income_cents = 0;
    record.expense_cents = amount_cents;

    g_finance_storage->write(record);

    std::stringstream ss;
    ss << "IMPORT: -" << (amount_cents / 100) << "." << std::setfill('0') << std::setw(2) << (amount_cents % 100);
    logOperation(ss.str());
}

std::pair<long long, long long> BookSystem::getFinance(int count) {
    long long total_income = 0;
    long long total_expense = 0;

    auto all_records = g_finance_storage->getAll();
    int total = all_records.size();

    if (count == -1) {
        // Return all transactions
        for (const auto& record : all_records) {
            total_income += record.income_cents;
            total_expense += record.expense_cents;
        }
        return {total_income, total_expense};
    }

    if (count > total) {
        // Operation fails
        return {-1, -1};
    }

    if (count == 0) {
        return {0, 0};
    }

    int start = total - count;
    for (int i = start; i < total; i++) {
        total_income += all_records[i].income_cents;
        total_expense += all_records[i].expense_cents;
    }

    return {total_income, total_expense};
}

int BookSystem::getTotalTransactionCount() {
    return g_finance_storage->count();
}

void BookSystem::logOperation(const std::string& operation) {
    if (g_log_file && g_log_file->good()) {
        time_t now = time(nullptr);
        char* time_str = ctime(&now);
        std::string user = account_system_->getCurrentUserId();

        *g_log_file << "[" << (time_str ? time_str : "unknown") << "] "
                   << "User: " << user << " - " << operation << std::endl;
    }
}

std::string BookSystem::generateLogReport() {
    std::stringstream ss;
    std::ifstream log_file(g_log_filename);
    if (log_file.good()) {
        ss << log_file.rdbuf();
    }
    return ss.str();
}

std::string BookSystem::generateFinanceReport() {
    std::stringstream ss;
    ss << "=== Finance Report ===" << std::endl;

    auto all_records = g_finance_storage->getAll();
    long long total_income = 0;
    long long total_expense = 0;

    for (const auto& record : all_records) {
        total_income += record.income_cents;
        total_expense += record.expense_cents;
    }

    ss << "Total Transactions: " << all_records.size() << std::endl;
    ss << "Total Income: " << (total_income / 100) << "."
       << std::setfill('0') << std::setw(2) << (total_income % 100) << std::endl;
    ss << "Total Expense: " << (total_expense / 100) << "."
       << std::setfill('0') << std::setw(2) << (total_expense % 100) << std::endl;
    ss << "Net Profit: " << ((total_income - total_expense) / 100) << "."
       << std::setfill('0') << std::setw(2) << ((total_income - total_expense) % 100) << std::endl;

    return ss.str();
}

std::string BookSystem::generateEmployeeReport() {
    std::stringstream ss;
    ss << "=== Employee Report ===" << std::endl;
    ss << "Employee operations logged." << std::endl;
    return ss.str();
}
