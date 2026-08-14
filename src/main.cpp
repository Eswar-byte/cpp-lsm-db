#include "db.h"
#include <iostream>
#include <string>

int main() {
    std::cout << "==========================================\n";
    std::cout << " Starting LSM-Tree Key-Value Storage Engine\n";
    std::cout << "==========================================\n";
    
    // Initialize the DB in the "data" directory
    lsm::KVStore db("data");

    // Basic Operations
    std::cout << "\n[*] Executing basic PUT operations...\n";
    db.put("ticker_AAPL", "150.25");
    db.put("ticker_GOOGL", "2750.10");
    db.put("status", "ACTIVE");

    std::cout << "[*] Executing GET operations...\n";
    auto val = db.get("ticker_AAPL");
    if (val) std::cout << "    -> ticker_AAPL: " << *val << "\n";
    
    auto missing = db.get("ticker_TSLA");
    if (!missing) std::cout << "    -> ticker_TSLA: (Not Found)\n";

    // Stress Test to trigger SSTable Flush
    std::cout << "\n[*] Initiating High-Volume Write Load (500 records)...\n";
    for (int i = 0; i < 500; ++i) {
        db.put("quant_data_key_" + std::to_string(i), 
               "simulated_market_payload_block_data_" + std::to_string(i));
    }

    // Verify reading from disk (SSTable) after flush
    std::cout << "\n[*] Verifying read from flushed SSTable on disk...\n";
    auto disk_val = db.get("quant_data_key_10");
    if (disk_val) {
        std::cout << "    -> quant_data_key_10: " << *disk_val << "\n";
    }

    std::cout << "\n==========================================\n";
    std::cout << " Engine Shutdown Successfully.\n";
    std::cout << "==========================================\n";
    
    return 0;
}