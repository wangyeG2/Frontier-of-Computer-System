#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <random>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/options.h"
#include "lru_cache.h"
void ClearPageCache() {
    system("sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    sleep(2);
}

std::string FormatKey(int num) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "user_key_%07d", num);
    return std::string(buffer);
}

class ZipfGenerator {
private:
    std::mt19937 gen_;
    std::discrete_distribution<int> dist_;
public:
    ZipfGenerator(int N, double s = 1.2) : gen_(std::random_device{}()) {
        std::vector<double> weights(N);
        for (int i = 1; i <= N; ++i) {
            weights[i - 1] = 1.0 / std::pow(i, s);
        }
        dist_ = std::discrete_distribution<int>(weights.begin(), weights.end());
    }
    int Next() { return dist_(gen_) + 1; }
};

enum CacheType { BLOCK_CACHE, KV_CACHE };
enum TestType { POINT_GET, RANGE_SCAN, READ_WRITE_MIX };

struct Config {
    int num_keys = 1000000;    
    int value_size = 1024;
    int cache_capacity = 100000; 
    std::string db_path = "/tmp/leveldb_task3";
    CacheType cache_type = BLOCK_CACHE;
    TestType test_type = POINT_GET;
    int num_ops = 2000000;     
};

void RunTest(const Config& config) {
    ClearPageCache();
    leveldb::DestroyDB(config.db_path, leveldb::Options());

    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Cache* cache_ptr = nullptr; 
    if (config.cache_type == BLOCK_CACHE) {
        cache_ptr = leveldb::NewLRUCache(config.cache_capacity * config.value_size);
        options.block_cache = cache_ptr;
    } else {
        cache_ptr = leveldb::NewLRUCache(0);
        options.block_cache = cache_ptr;
    }
    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(options, config.db_path, &db);
    if (!status.ok()) {
        std::cerr << "Open DB failed: " << status.ToString() << std::endl;
        return;
    }

    LRUCache* kv_cache = nullptr;
    if (config.cache_type == KV_CACHE) {
        kv_cache = new LRUCache(config.cache_capacity);
    }

    std::cout << "Loading data..." << std::endl;
    std::string value(config.value_size, 'x');
    leveldb::WriteOptions write_opts;
    for (int i = 1; i <= config.num_keys; ++i) {
        db->Put(write_opts, FormatKey(i), value);
    }

    ClearPageCache();

    std::cout << "Running test..." << std::endl;
    ZipfGenerator zipf(config.num_keys);
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> op_dist(0.0, 1.0);

    leveldb::ReadOptions read_opts;
    if (config.cache_type == KV_CACHE) {
        read_opts.fill_cache = false;
    }

    std::string read_value;
    int get_count = 0, put_count = 0, cache_hit = 0;

    auto start = std::chrono::high_resolution_clock::now();

    if (config.test_type == POINT_GET) {
        for (int i = 0; i < config.num_ops; ++i) {
            std::string key = FormatKey(zipf.Next());
            bool found = false;

            if (kv_cache) {
                if (kv_cache->Get(key, &read_value)) {
                    found = true;
                    cache_hit++;
                }
            }

            if (!found) {
                status = db->Get(read_opts, key, &read_value);
                if (status.ok() && kv_cache) {
                    kv_cache->Put(key, read_value);
                }
            }
            get_count++;
        }
    } else if (config.test_type == RANGE_SCAN) {
        for (int i = 0; i < config.num_ops / 100; ++i) {
            int start_key = zipf.Next();
            if (start_key > config.num_keys - 100) start_key = config.num_keys - 100;

            leveldb::Iterator* it = db->NewIterator(read_opts);
            for (it->Seek(FormatKey(start_key)); it->Valid() && get_count < 100; it->Next()) {
                if (kv_cache) {
                }
                get_count++;
            }
            delete it;
        }
    } else {
        for (int i = 0; i < config.num_ops; ++i) {
            std::string key = FormatKey(zipf.Next());
            if (op_dist(rng) < 0.8) {
                bool found = false;
                if (kv_cache) {
                    if (kv_cache->Get(key, &read_value)) {
                        found = true;
                        cache_hit++;
                    }
                }
                if (!found) {
                    db->Get(read_opts, key, &read_value);
                    if (kv_cache) kv_cache->Put(key, read_value);
                }
                get_count++;
            } else {
                std::string new_val = "updated_" + std::to_string(i);
                db->Put(write_opts, key, new_val);
                if (kv_cache) kv_cache->Put(key, new_val);
                put_count++;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();
    double qps = (get_count + put_count) / duration;

    std::cout << "========================================" << std::endl;
    std::cout << "Cache Type: " << (config.cache_type == BLOCK_CACHE ? "Block Cache" : "KV Cache") << std::endl;
    std::cout << "Test Type: " << (config.test_type == POINT_GET ? "Point Get" :
                                      config.test_type == RANGE_SCAN ? "Range Scan" : "Read-Write Mix") << std::endl;
    std::cout << "Total Ops: " << (get_count + put_count) << std::endl;
    std::cout << "Duration: " << duration << "s" << std::endl;
    std::cout << "QPS: " << qps << std::endl;
    if (kv_cache) {
        std::cout << "KV Cache Hit Rate: " << (cache_hit * 100.0 / get_count) << "%" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    delete db;
    delete kv_cache;
    if (cache_ptr != nullptr) {
        delete cache_ptr; 
    }
}

int main(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cache_type=kv") {
            config.cache_type = KV_CACHE;
        } else if (arg == "--test=range_scan") {
            config.test_type = RANGE_SCAN;
        } else if (arg == "--test=read_write_mix") {
            config.test_type = READ_WRITE_MIX;
        }
    }

    RunTest(config);
    return 0;
}