#include "account_system.h"
#include "file_manager.h"
#include <fstream>
#include <iostream>
#include <algorithm>

// Simple index file for accounts
class AccountIndex {
private:
    std::string filename_;
    std::vector<std::pair<std::string, int>> index_; // user_id -> position

    void load() {
        index_.clear();
        std::ifstream file(filename_);
        if (!file.good()) return;

        std::string id;
        int pos;
        while (file >> id >> pos) {
            index_.push_back({id, pos});
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
    AccountIndex(const std::string& filename) : filename_(filename) {
        load();
    }

    int find(const std::string& user_id) {
        for (const auto& p : index_) {
            if (p.first == user_id) return p.second;
        }
        return -1;
    }

    void insert(const std::string& user_id, int pos) {
        if (find(user_id) >= 0) return; // Already exists
        index_.push_back({user_id, pos});
        save();
    }

    void remove(const std::string& user_id) {
        auto it = std::remove_if(index_.begin(), index_.end(),
            [&](const std::pair<std::string, int>& p) { return p.first == user_id; });
        if (it != index_.end()) {
            index_.erase(it, index_.end());
            save();
        }
    }
};

// Global storage
static SimpleStorage<Account>* g_account_storage = nullptr;
static AccountIndex* g_account_index = nullptr;

AccountSystem::AccountSystem()
    : account_data_file_("account.dat"),
      account_index_file_("account.idx"),
      initialized_(false) {
}

AccountSystem::~AccountSystem() {
    delete g_account_storage;
    delete g_account_index;
    g_account_storage = nullptr;
    g_account_index = nullptr;
}

void AccountSystem::initialize() {
    if (initialized_) return;

    g_account_storage = new SimpleStorage<Account>(account_data_file_);
    g_account_storage->initialize();

    g_account_index = new AccountIndex(account_index_file_);

    // Check if root account exists
    if (g_account_index->find("root") < 0) {
        // Create root account
        Account root("root", "sjtu", "", 7);
        int pos = g_account_storage->write(root);
        g_account_index->insert("root", pos);
    }

    initialized_ = true;
}

Account* AccountSystem::findAccount(const std::string& user_id) {
    static Account temp_account;
    int pos = g_account_index->find(user_id);
    if (pos < 0) return nullptr;

    if (g_account_storage->read(pos, temp_account)) {
        return &temp_account;
    }
    return nullptr;
}

bool AccountSystem::isAccountLoggedIn(const std::string& user_id) {
    for (const auto& session : login_stack_) {
        if (session.user_id == user_id) return true;
    }
    return false;
}

bool AccountSystem::login(const std::string& user_id, const std::string& password) {
    Account* acc = findAccount(user_id);
    if (!acc) return false;

    if (acc->password.str() != password) return false;

    // Create new session
    login_stack_.push_back(LoginSession(user_id, acc->privilege));
    return true;
}

bool AccountSystem::loginWithoutPassword(const std::string& user_id) {
    if (!isLoggedIn()) return false;

    int current_priv = getCurrentPrivilege();
    Account* acc = findAccount(user_id);
    if (!acc) return false;

    // Can only skip password if current privilege is higher
    if (current_priv <= acc->privilege) return false;

    login_stack_.push_back(LoginSession(user_id, acc->privilege));
    return true;
}

bool AccountSystem::logout() {
    if (login_stack_.empty()) return false;
    login_stack_.pop_back();
    return true;
}

bool AccountSystem::registerAccount(const std::string& user_id,
                                   const std::string& password,
                                   const std::string& username) {
    // Check if user already exists
    if (g_account_index->find(user_id) >= 0) return false;

    Account new_acc(user_id, password, username, 1);
    int pos = g_account_storage->write(new_acc);
    g_account_index->insert(user_id, pos);
    return true;
}

bool AccountSystem::changePassword(const std::string& user_id,
                                  const std::string& new_password) {
    // Only for privilege 7 (root)
    int pos = g_account_index->find(user_id);
    if (pos < 0) return false;

    Account acc;
    if (!g_account_storage->read(pos, acc)) return false;

    acc.password = FixedString<Account::PASS_SIZE - 1>(new_password);
    g_account_storage->update(pos, acc);
    return true;
}

bool AccountSystem::changePassword(const std::string& user_id,
                                  const std::string& current_password,
                                  const std::string& new_password) {
    int pos = g_account_index->find(user_id);
    if (pos < 0) return false;

    Account acc;
    if (!g_account_storage->read(pos, acc)) return false;

    if (acc.password.str() != current_password) return false;

    acc.password = FixedString<Account::PASS_SIZE - 1>(new_password);
    g_account_storage->update(pos, acc);
    return true;
}

bool AccountSystem::addAccount(const std::string& user_id,
                              const std::string& password,
                              int privilege,
                              const std::string& username) {
    if (!isLoggedIn()) return false;

    // Cannot create account with equal or higher privilege
    if (privilege >= getCurrentPrivilege()) return false;

    // Check if user already exists
    if (g_account_index->find(user_id) >= 0) return false;

    Account new_acc(user_id, password, username, privilege);
    int pos = g_account_storage->write(new_acc);
    g_account_index->insert(user_id, pos);
    return true;
}

bool AccountSystem::deleteAccount(const std::string& user_id) {
    // Cannot delete if logged in
    if (isAccountLoggedIn(user_id)) return false;

    int pos = g_account_index->find(user_id);
    if (pos < 0) return false;

    g_account_index->remove(user_id);
    return true;
}

LoginSession* AccountSystem::getCurrentSession() {
    if (login_stack_.empty()) return nullptr;
    return &login_stack_.back();
}

int AccountSystem::getCurrentPrivilege() const {
    if (login_stack_.empty()) return 0;
    return login_stack_.back().privilege;
}

bool AccountSystem::isLoggedIn() const {
    return !login_stack_.empty();
}

void AccountSystem::setSelectedBookPosition(int pos) {
    if (!login_stack_.empty()) {
        login_stack_.back().selected_book_pos = pos;
        login_stack_.back().has_selected_book = true;
    }
}

int AccountSystem::getSelectedBookPosition() {
    if (login_stack_.empty() || !login_stack_.back().has_selected_book) {
        return -1;
    }
    return login_stack_.back().selected_book_pos;
}

void AccountSystem::clearSelectedBook() {
    if (!login_stack_.empty()) {
        login_stack_.back().has_selected_book = false;
        login_stack_.back().selected_book_pos = -1;
    }
}

std::string AccountSystem::getCurrentUserId() {
    if (login_stack_.empty()) return "";
    return login_stack_.back().user_id;
}
