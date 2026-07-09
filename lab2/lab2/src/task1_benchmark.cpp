#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <random>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include "leveldb/db.h"
#include "leveldb/options.h"
#include "leveldb/filter_policy.h"

void ClearPageCache() {
    system("sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    sleep(2);
}

std::string FormatKey(int num) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "user_key_%07d", num);
    return std::string(buffer);
}

enum TestType {
    SEQUENTIAL_WRITE,
    RANDOM_WRITE,
    RANDOM_READ
};

struct Config {
    int num_keys = 10000000;    
    int value_size = 1024;      
    std::string db_path = "/tmp/leveldb_task1";
    int write_buffer_size = 4;   
    int block_size = 4;          
    bool use_bloom_filter = true;
    TestType test_type = RANDOM_WRITE;
};

void RunTest(const Config& config) {
    ClearPageCache();

    leveldb::DestroyDB(config.db_path, leveldb::Options());

    leveldb::Options options;
    options.create_if_missing = true;
    options.write_buffer_size = config.write_buffer_size * 1024 * 1024;
    options.block_size = config.block_size * 1024;
    if (config.use_bloom_filter) {
        options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    }

    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(options, config.db_path, &db);
    if (!status.ok()) {
        std::cerr << "Open DB failed: " << status.ToString() << std::endl;
        return;
    }

    std::vector<int> keys(config.num_keys);
    std::iota(keys.begin(), keys.end(), 1);
    std::string value(config.value_size, 'x');

    if (config.test_type == RANDOM_WRITE || config.test_type == RANDOM_READ) {
        std::mt19937 rng(std::random_device{}());
        std::shuffle(keys.begin(), keys.end(), rng);
    }

    auto start = std::chrono::high_resolution_clock::now();
    leveldb::WriteOptions write_opts;
    leveldb::ReadOptions read_opts;
    std::string read_value;

    if (config.test_type == SEQUENTIAL_WRITE || config.test_type == RANDOM_WRITE) {
        std::cout << "Writing " << config.num_keys << " keys..." << std::endl;
        for (int i = 0; i < config.num_keys; ++i) {
            db->Put(write_opts, FormatKey(keys[i]), value);
        }
    } else {
        std::cout << "Loading data first..." << std::endl;
        for (int i = 0; i < config.num_keys; ++i) {
            db->Put(write_opts, FormatKey(keys[i]), value);
        }
        ClearPageCache();
        std::cout << "Reading " << config.num_keys << " keys..." << std::endl;
        std::shuffle(keys.begin(), keys.end(), std::mt19937(std::random_device{}()));
        for (int i = 0; i < config.num_keys; ++i) {
            db->Get(read_opts, FormatKey(keys[i]), &read_value);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();
    double qps = config.num_keys / duration;
    double avg_latency_ms = 1000.0 / qps;
    std::cout << "========================================" << std::endl;
    std::cout << "Test Type: " << (config.test_type == SEQUENTIAL_WRITE ? "Seq Write" :
                                      config.test_type == RANDOM_WRITE ? "Rand Write" : "Rand Read") << std::endl;
    std::cout << "Write Buffer Size: " << config.write_buffer_size << "MB" << std::endl;
    std::cout << "Block Size: " << config.block_size << "KB" << std::endl;
    std::cout << "Bloom Filter: " << (config.use_bloom_filter ? "On" : "Off") << std::endl;
    std::cout << "Duration: " << duration << "s" << std::endl;
    std::cout << "QPS: " << qps << std::endl;
    std::cout << "Avg Latency: " << avg_latency_ms << " ms" << std::endl;
    std::cout << "========================================" << std::endl;

    delete db;
}

int main(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--write_buffer_size=") == 0) {
            config.write_buffer_size = std::stoi(arg.substr(20));
        } else if (arg.find("--block_size=") == 0) {
            config.block_size = std::stoi(arg.substr(13));
        } else if (arg == "--bloom_filter=off") {
            config.use_bloom_filter = false;
        } else if (arg == "--test=seq_write") {
            config.test_type = SEQUENTIAL_WRITE;
        } else if (arg == "--test=rand_write") {
            config.test_type = RANDOM_WRITE;
        } else if (arg == "--test=rand_read") {
            config.test_type = RANDOM_READ;
        }
    }

    RunTest(config);
    return 0;
}