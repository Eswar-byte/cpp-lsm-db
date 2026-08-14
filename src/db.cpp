#include "db.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace lsm {

KVStore::KVStore(const std::string& directory) : db_dir(directory) {
    if (!fs::exists(db_dir)) {
        fs::create_directory(db_dir);
    }
    
    wal_path = db_dir + "/wal.log";
    
    // Determine the current SSTable count to resume correctly
    for (const auto& entry : fs::directory_iterator(db_dir)) {
        if (entry.path().extension() == ".sst") {
            sstable_count++;
        }
    }

    recover_from_wal();
    wal_file.open(wal_path, std::ios::app | std::ios::binary);
}

KVStore::~KVStore() {
    if (!memtable.empty()) {
        flush_memtable_to_sstable();
    }
    if (wal_file.is_open()) {
        wal_file.close();
    }
}

void KVStore::append_to_wal(const std::string& key, const std::string& value) {
    wal_file << key.size() << " " << key << " " << value.size() << " " << value << "\n";
    wal_file.flush(); // Force write to disk for durability (ACID property)
}

void KVStore::recover_from_wal() {
    std::ifstream in_wal(wal_path);
    if (!in_wal.is_open()) return;

    size_t k_size, v_size;
    std::string key, value;
    
    while (in_wal >> k_size) {
        in_wal.ignore(); 
        key.resize(k_size);
        in_wal.read(&key[0], k_size);
        
        in_wal >> v_size;
        in_wal.ignore(); 
        value.resize(v_size);
        in_wal.read(&value[0], v_size);
        
        memtable[key] = value;
        memtable_size += key.size() + value.size();
    }
    std::cout << "[Recovery] Restored " << memtable.size() << " keys from WAL.\n";
}

void KVStore::flush_memtable_to_sstable() {
    std::string sst_path = db_dir + "/data_" + std::to_string(sstable_count++) + ".sst";
    std::ofstream sst(sst_path, std::ios::binary);
    
    // Write the sorted MemTable sequentially to disk
    for (const auto& [key, value] : memtable) {
        sst << key.size() << " " << key << " " << value.size() << " " << value << "\n";
    }
    sst.close();
    
    std::cout << "[Compaction] Flushed MemTable to " << sst_path << "\n";
    
    // Reset state
    memtable.clear();
    memtable_size = 0;
    
    // Rotate WAL
    wal_file.close();
    fs::remove(wal_path);
    wal_file.open(wal_path, std::ios::app | std::ios::binary);
}

void KVStore::put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    
    append_to_wal(key, value);
    
    memtable[key] = value;
    memtable_size += key.size() + value.size();
    
    if (memtable_size >= MAX_MEMTABLE_SIZE) {
        flush_memtable_to_sstable();
    }
}

std::string KVStore::search_sstables(const std::string& target_key) {
    // Search from newest SSTable to oldest
    for (int i = sstable_count - 1; i >= 0; --i) {
        std::string sst_path = db_dir + "/data_" + std::to_string(i) + ".sst";
        std::ifstream sst(sst_path, std::ios::binary);
        if (!sst.is_open()) continue;

        size_t k_size, v_size;
        std::string key, value;
        
        while (sst >> k_size) {
            sst.ignore();
            key.resize(k_size);
            sst.read(&key[0], k_size);
            
            sst >> v_size;
            sst.ignore();
            value.resize(v_size);
            sst.read(&value[0], v_size);
            
            if (key == target_key) return value;
        }
    }
    return "";
}

std::optional<std::string> KVStore::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    
    // 1. Search the fast in-memory MemTable
    auto it = memtable.find(key);
    if (it != memtable.end()) {
        return it->second;
    }
    
    // 2. Fallback to searching disk-based SSTables
    std::string disk_result = search_sstables(key);
    if (!disk_result.empty()) {
        return disk_result;
    }
    
    return std::nullopt;
}

} // namespace lsm