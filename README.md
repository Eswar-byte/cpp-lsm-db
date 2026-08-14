# C++ LSM-Tree Key-Value Database Engine ⚙️

A high-performance, embedded Key-Value storage engine built from scratch in C++17. This project implements a **Log-Structured Merge-Tree (LSM-Tree)** architecture, the same fundamental storage engine design underlying enterprise databases like Google's LevelDB, Facebook's RocksDB, and Apache Cassandra.

Designed to prioritize write-throughput and zero data loss, bypassing slow in-place disk updates in favor of sequential append-only I/O.

## 🧠 Core Architecture

* **Write-Ahead Log (WAL):** Guarantees database durability (ACID compliance). Every transaction is immediately appended to a binary log file before memory insertion, allowing complete recovery of unwritten data in the event of a crash or power failure.
* **In-Memory MemTable:** Utilizes a Red-Black Tree (`std::map`) to absorb high-volume incoming writes, maintaining keys in sorted order for blazing-fast $O(\log N)$ point reads.
* **SSTable Compaction:** When the MemTable hits a defined memory threshold, the engine freezes it and sequentially flushes the data to disk as an immutable **Sorted String Table (SSTable)**, preventing heap exhaustion and RAM bottlenecks.
* **Layered Retrieval:** Search operations query the hot MemTable in memory first, gracefully falling back to reverse-chronological SSTable disk scans if a cache miss occurs.
* **Thread-Safe I/O:** Core engine operations are protected by `std::mutex` to ensure safe, concurrent data ingestion across threads.

## 🛠️ Build & Run

**Requirements:** A C++17 compatible compiler (GCC/Clang). No external dependencies.

```bash
# Clone the repository
git clone [https://github.com/Eswar-byte/cpp-lsm-db.git](https://github.com/Eswar-byte/cpp-lsm-db.git)
cd cpp-lsm-db

# Compile the engine and test driver
make

# Run the stress test and trigger disk flushes
make run