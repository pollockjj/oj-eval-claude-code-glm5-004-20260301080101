#ifndef ACCOUNT_SYSTEM_H
#define ACCOUNT_SYSTEM_H

#include <string>
#include <vector>
#include <cstring>

// Fixed-size string for file storage
template<size_t N>
class FixedString {
private:
    char data_[N + 1];
    size_t len_;

public:
    FixedString() : len_(0) { data_[0] = '\0'; }

    FixedString(const std::string& s) {
        size_t copy_len = std::min(s.length(), N);
        memcpy(data_, s.c_str(), copy_len);
        data_[copy_len] = '\0';
        len_ = copy_len;
    }

    FixedString(const char* s) {
        size_t slen = strlen(s);
        size_t copy_len = std::min(slen, N);
        memcpy(data_, s, copy_len);
        data_[copy_len] = '\0';
        len_ = copy_len;
    }

    const char* c_str() const { return data_; }
    std::string str() const { return std::string(data_); }
    size_t size() const { return len_; }
    bool empty() const { return len_ == 0; }

    bool operator<(const FixedString& other) const {
        return strcmp(data_, other.data_) < 0;
    }

    bool operator==(const FixedString& other) const {
        return strcmp(data_, other.data_) == 0;
    }

    bool operator!=(const FixedString& other) const {
        return strcmp(data_, other.data_) != 0;
    }
};

// Account structure for file storage
struct Account {
    static const int ID_SIZE = 31;
    static const int PASS_SIZE = 31;
    static const int NAME_SIZE = 31;

    FixedString<ID_SIZE - 1> user_id;
    FixedString<PASS_SIZE - 1> password;
    FixedString<NAME_SIZE - 1> username;
    int privilege;

    Account() : privilege(0) {}

    Account(const std::string& id, const std::string& pass,
            const std::string& name, int priv)
        : user_id(id), password(pass), username(name), privilege(priv) {}
};

// Login session info
struct LoginSession {
    std::string user_id;
    int privilege;
    std::string selected_isbn;
    bool has_selected_book;

    LoginSession() : privilege(0), has_selected_book(false) {}
    LoginSession(const std::string& id, int priv)
        : user_id(id), privilege(priv), has_selected_book(false) {}
};

class AccountSystem {
private:
    std::vector<LoginSession> login_stack_;
    std::string account_data_file_;
    std::string account_index_file_;
    bool initialized_;

    void initRootAccount();
    Account* findAccount(const std::string& user_id);
    bool isAccountLoggedIn(const std::string& user_id);

public:
    AccountSystem();
    ~AccountSystem();

    void initialize();

    // Account commands
    bool login(const std::string& user_id, const std::string& password);
    bool loginWithoutPassword(const std::string& user_id); // For high privilege login
    bool logout();
    bool registerAccount(const std::string& user_id, const std::string& password,
                        const std::string& username);
    bool changePassword(const std::string& user_id, const std::string& new_password);
    bool changePassword(const std::string& user_id, const std::string& current_password,
                       const std::string& new_password);
    bool addAccount(const std::string& user_id, const std::string& password,
                   int privilege, const std::string& username);
    bool deleteAccount(const std::string& user_id);

    // Session management
    LoginSession* getCurrentSession();
    int getCurrentPrivilege() const;
    bool isLoggedIn() const;
    void setSelectedBook(const std::string& isbn);
    std::string getSelectedBook();
    void clearSelectedBook();

    // Get current user info
    std::string getCurrentUserId();
};

#endif // ACCOUNT_SYSTEM_H
