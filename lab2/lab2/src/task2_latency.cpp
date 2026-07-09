#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <random>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <unistd.h>
#include <cstdlib>
#include "leveldb/db.h"
#include "leveldb/options.h"

void ClearPageCache() {
    system("sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    sleep(2);
}

std::string FormatKey(int num) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "user_key_%07d", num);
    return std::string(buffer);
}

int main() {
    const int NUM_KEYS = 25000000;      // 3000万条，每条1KB，总数据量约30GB
    const int VALUE_SIZE = 1024;
    const std::string DB_PATH = "/tmp/leveldb_task2";
    const std::string LATENCY_LOG = "latency_log.txt";

    ClearPageCache();
    leveldb::DestroyDB(DB_PATH, leveldb::Options());

    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    options.write_buffer_size = 4 * 1024 * 1024;
    leveldb::Status status = leveldb::DB::Open(options, DB_PATH, &db);
    if (!status.ok()) {
        std::cerr << "Open DB failed: " << status.ToString() << std::endl;
        return -1;
    }

    std::vector<int> keys(NUM_KEYS);
    std::iota(keys.begin(), keys.end(), 1);
    std::mt19937 rng(std::random_device{}());
    std::shuffle(keys.begin(), keys.end(), rng);

    std::vector<double> latencies_us;
    latencies_us.reserve(NUM_KEYS);
    std::ofstream log_file(LATENCY_LOG);

    std::cout << "Starting high-speed random write..." << std::endl;
    std::string value(VALUE_SIZE, 'x');
    leveldb::WriteOptions write_opts;

    auto total_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_KEYS; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        db->Put(write_opts, FormatKey(keys[i]), value);
        auto end = std::chrono::high_resolution_clock::now();

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies_us.push_back(latency_us);
        log_file << latency_us << std::endl;

        if (i % 100000 == 0) {
            std::cout << "Written " << i << " keys..." << std::endl;
            ClearPageCache();
        }
    }

    auto total_end = std::chrono::high_resolution_clock::now();
    double total_duration = std::chrono::duration<double>(total_end - total_start).count();

    std::sort(latencies_us.begin(), latencies_us.end());
    int n = latencies_us.size();
    double p50 = latencies_us[n * 0.5];
    double p90 = latencies_us[n * 0.9];
    double p99 = latencies_us[n * 0.99];
    double p999 = latencies_us[n * 0.999];

    std::cout << "========================================" << std::endl;
    std::cout << "Total Write: " << NUM_KEYS << " keys" << std::endl;
    std::cout << "Total Time: " << total_duration << "s" << std::endl;
    std::cout << "Avg QPS: " << NUM_KEYS / total_duration << std::endl;
    std::cout << "P50 Latency: " << p50 << " us" << std::endl;
    std::cout << "P90 Latency: " << p90 << " us" << std::endl;
    std::cout << "P99 Latency: " << p99 << " us" << std::endl;
    std::cout << "P99.9 Latency: " << p999 << " us" << std::endl;
    std::cout << "Latency log saved to: " << LATENCY_LOG << std::endl;
    std::cout << "========================================" << std::endl;

    log_file.close();
    delete db;
    return 0;
}