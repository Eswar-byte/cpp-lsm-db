#ifndef DB_H
#define DB_H

#include <string>
#include <map>
#include <fstream>
#include <mutex>
#include <optional>
#include <vector>

namespace lsm {

class KVStore {
private:
    // The MemTable: In-memory sorted tree (Red-Black Tree via std::map)
    std::map<std::string, std::string> memtable;
    size_t memtable_size = 0;
    const size_t MAX_MEMTABLE_SIZE = 4096; // 4KB flush limit for demonstration

    // Storage Paths
    std::string db_dir;
    std::string wal_path;
    int sstable_count = 0;
    
    // Concurrency & File I/O
    std::ofstream wal_file;
    std::mutex db_mutex;

    // Core Internal Mechanisms
    void append_to_wal(const std::string& key, const std::string& value);
    void flush_memtable_to_sstable();
    void recover_from_wal();
    std::string search_sstables(const std::string& key);

public:
    explicit KVStore(const std::string& directory);
    ~KVStore();

    // Prevent copying
    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;

    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
};

} // namespace lsm

#endif