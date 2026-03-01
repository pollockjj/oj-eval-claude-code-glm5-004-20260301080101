#include "book_system.h"
#include "file_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>
#include <ctime>

// Book index files
class BookIndex {
private:
    std::string filename_;
    std::vector<std::pair<std::string, int>> index_; // key -> position

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
            index_.push_back({key, pos});
        }
        file.close();
    }

    void save() {
        std::ofstream file(filename_);
        for (const auto& p : index_) {
            file << p.first << " " << p.second << "\n";
        }
        file.close();
    }

public:
    BookIndex(const std::string& filename) : filename_(filename) {
        load();
    }

    std::vector<int> findAll(const std::string& key) {
        std::vector<int> result;
        for (const auto& p : index_) {
            if (p.first == key) result.push_back(p.second);
        }
        return result;
    }

    void insert(const std::string& key, int pos) {
        index_.push_back({key, pos});
        save();
    }

    void update(const std::string& old_key, const std::string& new_key, int pos) {
        for (auto& p : index_) {
            if (p.first == old_key && p.second == pos) {
                p.first = new_key;
                save();
                return;
            }
        }
    }

    void remove(int pos) {
        auto it = std::remove_if(index_.begin(), index_.end(),
            [&](const std::pair<std::string, int>& p) { return p.second == pos; });
        if (it != index_.end()) {
            index_.erase(it, index_.end());
            save();
        }
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
        if (segment.empty()) return false; // Empty segment
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

std::vector<Book> BookSystem::showBooks(const std::string& isbn_filter,
                                        const std::string& name_filter,
                                        const std::string& author_filter,
                                        const std::string& keyword_filter) {
    std::vector<Book> result;

    // If no filter, return all books sorted by ISBN
    if (isbn_filter.empty() && name_filter.empty() &&
        author_filter.empty() && keyword_filter.empty()) {
        std::vector<Book> all_books = g_book_storage->getAll();
        // Filter by index
        for (const auto& book : all_books) {
            bool found = false;
            // Check ISBN index
            auto isbn_positions = g_isbn_index->findAll(book.isbn.str());
            for (int pos : isbn_positions) {
                Book stored;
                if (g_book_storage->read(pos, stored) && stored.isbn.str() == book.isbn.str()) {
                    found = true;
                    break;
                }
            }
            if (found) {
                result.push_back(book);
            }
        }
        // Sort by ISBN
        std::sort(result.begin(), result.end(),
            [](const Book& a, const Book& b) { return a.isbn.str() < b.isbn.str(); });
        return result;
    }

    std::set<int> positions;

    // Filter by ISBN
    if (!isbn_filter.empty()) {
        auto pos = g_isbn_index->findAll(isbn_filter);
        positions.insert(pos.begin(), pos.end());
    }

    // Filter by name
    if (!name_filter.empty()) {
        auto pos = g_name_index->findAll(name_filter);
        if (positions.empty()) {
            positions.insert(pos.begin(), pos.end());
        } else {
            std::set<int> intersection;
            for (int p : pos) {
                if (positions.count(p)) intersection.insert(p);
            }
            positions = intersection;
        }
    }

    // Filter by author
    if (!author_filter.empty()) {
        auto pos = g_author_index->findAll(author_filter);
        if (positions.empty()) {
            positions.insert(pos.begin(), pos.end());
        } else {
            std::set<int> intersection;
            for (int p : pos) {
                if (positions.count(p)) intersection.insert(p);
            }
            positions = intersection;
        }
    }

    // Filter by keyword
    if (!keyword_filter.empty()) {
        auto pos = g_keyword_index->findAll(keyword_filter);
        if (positions.empty()) {
            positions.insert(pos.begin(), pos.end());
        } else {
            std::set<int> intersection;
            for (int p : pos) {
                if (positions.count(p)) intersection.insert(p);
            }
            positions = intersection;
        }
    }

    // Get books from positions
    for (int pos : positions) {
        Book book;
        if (g_book_storage->read(pos, book)) {
            result.push_back(book);
        }
    }

    // Sort by ISBN
    std::sort(result.begin(), result.end(),
        [](const Book& a, const Book& b) { return a.isbn.str() < b.isbn.str(); });

    return result;
}

long long BookSystem::buyBook(const std::string& isbn, long long quantity) {
    auto positions = g_isbn_index->findAll(isbn);
    if (positions.empty()) return -1;

    for (int pos : positions) {
        Book book;
        if (g_book_storage->read(pos, book)) {
            if (book.isbn.str() == isbn) {
                if (book.quantity < quantity) return -1;

                long long total_cost = book.price_cents * quantity;
                book.quantity -= quantity;
                g_book_storage->update(pos, book);

                // Record finance
                recordSale(total_cost);
                return total_cost;
            }
        }
    }
    return -1;
}

Book* BookSystem::selectBook(const std::string& isbn) {
    auto positions = g_isbn_index->findAll(isbn);

    // Check if book exists
    for (int pos : positions) {
        Book book;
        if (g_book_storage->read(pos, book) && book.isbn.str() == isbn) {
            account_system_->setSelectedBook(isbn);
            // Return a copy (caller will need to handle this)
            static Book selected_book;
            selected_book = book;
            return &selected_book;
        }
    }

    // Create new book with only ISBN
    Book new_book;
    new_book.isbn = FixedString<Book::ISBN_SIZE - 1>(isbn);
    int pos = g_book_storage->write(new_book);
    g_isbn_index->insert(isbn, pos);

    account_system_->setSelectedBook(isbn);

    static Book selected_book;
    selected_book = new_book;
    return &selected_book;
}

bool BookSystem::modifyBook(Book& book, const std::string& new_isbn,
                           const std::string& new_name,
                           const std::string& new_author,
                           const std::string& new_keywords,
                           const std::string& new_price,
                           bool has_isbn, bool has_name, bool has_author,
                           bool has_keywords, bool has_price) {
    // Find the book position
    auto positions = g_isbn_index->findAll(book.isbn.str());
    int book_pos = -1;
    for (int pos : positions) {
        Book stored;
        if (g_book_storage->read(pos, stored) && stored.isbn.str() == book.isbn.str()) {
            book_pos = pos;
            break;
        }
    }
    if (book_pos < 0) return false;

    // Get old values for index updates
    std::string old_isbn = book.isbn.str();
    std::string old_name = book.name.str();
    std::string old_author = book.author.str();
    std::string old_keywords = book.keywords.str();

    // Check for ISBN change to same value
    if (has_isbn && new_isbn == old_isbn) return false;

    // Check for ISBN change to existing ISBN
    if (has_isbn) {
        auto existing = g_isbn_index->findAll(new_isbn);
        for (int pos : existing) {
            Book stored;
            if (g_book_storage->read(pos, stored) && stored.isbn.str() == new_isbn) {
                return false; // ISBN already exists
            }
        }
    }

    // Check for duplicate keywords
    if (has_keywords && !new_keywords.empty()) {
        if (hasDuplicateKeywords(new_keywords)) return false;
    }

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
    }
    if (has_name && new_name != old_name) {
        g_name_index->remove(book_pos);
        if (!new_name.empty()) g_name_index->insert(new_name, book_pos);
    }
    if (has_author && new_author != old_author) {
        g_author_index->remove(book_pos);
        if (!new_author.empty()) g_author_index->insert(new_author, book_pos);
    }
    if (has_keywords && new_keywords != old_keywords) {
        g_keyword_index->remove(book_pos);
        // Insert new keywords
        std::vector<std::string> kw_list;
        parseKeywords(new_keywords, kw_list);
        for (const auto& kw : kw_list) {
            g_keyword_index->insert(kw, book_pos);
        }
    }

    // Update session selected book
    if (has_isbn) {
        account_system_->setSelectedBook(new_isbn);
    }

    return true;
}

bool BookSystem::importBook(Book& book, long long quantity, long long total_cost) {
    if (quantity <= 0 || total_cost <= 0) return false;

    auto positions = g_isbn_index->findAll(book.isbn.str());
    int book_pos = -1;
    for (int pos : positions) {
        Book stored;
        if (g_book_storage->read(pos, stored) && stored.isbn.str() == book.isbn.str()) {
            book_pos = pos;
            break;
        }
    }
    if (book_pos < 0) return false;

    book.quantity += quantity;
    g_book_storage->update(book_pos, book);

    // Record finance (import is expense)
    recordImport(total_cost);

    return true;
}

void BookSystem::recordSale(long long amount_cents) {
    FinanceRecord record;
    record.income_cents = amount_cents;
    record.expense_cents = 0;

    time_t now = time(nullptr);
    char* time_str = ctime(&now);
    if (time_str) record.time_info = time_str;

    std::stringstream ss;
    ss << "SALE: +" << (amount_cents / 100) << "." << std::setfill('0') << std::setw(2) << (amount_cents % 100);
    record.operation_info = ss.str();

    g_finance_storage->write(record);
    logOperation(record.operation_info);
}

void BookSystem::recordImport(long long amount_cents) {
    FinanceRecord record;
    record.income_cents = 0;
    record.expense_cents = amount_cents;

    time_t now = time(nullptr);
    char* time_str = ctime(&now);
    if (time_str) record.time_info = time_str;

    std::stringstream ss;
    ss << "IMPORT: -" << (amount_cents / 100) << "." << std::setfill('0') << std::setw(2) << (amount_cents % 100);
    record.operation_info = ss.str();

    g_finance_storage->write(record);
    logOperation(record.operation_info);
}

std::pair<long long, long long> BookSystem::getFinance(int count) {
    long long total_income = 0;
    long long total_expense = 0;

    auto all_records = g_finance_storage->getAll();
    int total = all_records.size();

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
