#include "file_manager.h"
#include "account_system.h"
#include "book_system.h"
#include <fstream>
#include <cstring>
#include <algorithm>

// SimpleStorage implementation
template<typename T>
SimpleStorage<T>::SimpleStorage(const std::string& filename)
    : filename_(filename), initialized_(false) {}

template<typename T>
SimpleStorage<T>::~SimpleStorage() {}

template<typename T>
void SimpleStorage<T>::initialize() {
    std::ifstream test(filename_);
    if (!test.good()) {
        // Create empty file
        std::ofstream create(filename_, std::ios::binary);
        int count = 0;
        create.write(reinterpret_cast<char*>(&count), sizeof(count));
        create.close();
    }
    initialized_ = true;
}

template<typename T>
int SimpleStorage<T>::write(const T& record) {
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.good()) return -1;

    // Read count
    int count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    // Append record
    file.seekp(0, std::ios::end);
    int pos = count;
    file.write(reinterpret_cast<const char*>(&record), sizeof(T));

    // Update count
    count++;
    file.seekp(0);
    file.write(reinterpret_cast<char*>(&count), sizeof(count));

    file.close();
    return pos;
}

template<typename T>
bool SimpleStorage<T>::read(int pos, T& record) {
    std::ifstream file(filename_, std::ios::binary);
    if (!file.good()) return false;

    int count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    if (pos < 0 || pos >= count) {
        file.close();
        return false;
    }

    file.seekg(sizeof(count) + pos * sizeof(T));
    file.read(reinterpret_cast<char*>(&record), sizeof(T));
    file.close();
    return true;
}

template<typename T>
bool SimpleStorage<T>::update(int pos, const T& record) {
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.good()) return false;

    int count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    if (pos < 0 || pos >= count) {
        file.close();
        return false;
    }

    file.seekp(sizeof(count) + pos * sizeof(T));
    file.write(reinterpret_cast<const char*>(&record), sizeof(T));
    file.close();
    return true;
}

template<typename T>
std::vector<T> SimpleStorage<T>::getAll() {
    std::vector<T> result;
    std::ifstream file(filename_, std::ios::binary);
    if (!file.good()) return result;

    int count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (int i = 0; i < count; i++) {
        T record;
        file.read(reinterpret_cast<char*>(&record), sizeof(T));
        result.push_back(record);
    }

    file.close();
    return result;
}

template<typename T>
int SimpleStorage<T>::count() {
    std::ifstream file(filename_, std::ios::binary);
    if (!file.good()) return 0;

    int count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    file.close();
    return count;
}

template<typename T>
void SimpleStorage<T>::clear() {
    std::ofstream file(filename_, std::ios::binary);
    int count = 0;
    file.write(reinterpret_cast<char*>(&count), sizeof(count));
    file.close();
}

// Explicit template instantiation for common types
template class SimpleStorage<Account>;
template class SimpleStorage<Book>;
template class SimpleStorage<FinanceRecord>;
