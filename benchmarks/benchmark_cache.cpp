#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <latch>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "cache/lru_cache.h"

#ifndef CACHE_BUILD_TYPE
#define CACHE_BUILD_TYPE "unknown"
#endif

namespace {

struct Workload {
    const char* name;
    int getPercent;
};

struct Operation {
    bool isGet;
    int keyIndex;
};

struct Result {
    double seconds;
    double opsPerSecond;
    double averageNs;
    std::int64_t p50Ns;
    std::int64_t p95Ns;
    std::int64_t p99Ns;
    double hitRate;
};

struct Config {
    std::size_t capacity = 10000;
    int keySpace = 20000;
    int totalOps = 1000000;
    int maxThreads = 8;
};

std::string readCpuName() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("model name", 0) == 0) {
            auto colon = line.find(':');
            if (colon != std::string::npos && colon + 2 <= line.size()) {
                return line.substr(colon + 2);
            }
        }
    }
    return "unknown";
}

std::vector<Operation> generateOperations(int count, int getPercent, int keySpace, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> percent(0, 99);
    std::uniform_int_distribution<int> key(0, keySpace - 1);

    std::vector<Operation> ops;
    ops.reserve(count);
    for (int i = 0; i < count; ++i) {
        ops.push_back({percent(rng) < getPercent, key(rng)});
    }
    return ops;
}

std::int64_t percentile(const std::vector<std::int64_t>& sorted, double p) {
    // Nearest-rank method
    std::size_t rank = static_cast<std::size_t>(std::ceil(p / 100.0 * sorted.size()));
    if (rank == 0) {
        rank = 1;
    }
    return sorted[rank - 1];
}

Result runWorkload(const Config& config, const Workload& workload, int threadCount,
                   const std::vector<std::string>& keys, const std::string& value) {
    LRUCache cache(config.capacity);
    for (std::size_t i = 0; i < config.capacity; ++i) {
        cache.put(keys[i], value);
    }

    int opsPerThread = config.totalOps / threadCount;
    std::vector<std::vector<Operation>> threadOps;
    std::vector<std::vector<std::int64_t>> threadLatencies(threadCount);
    std::vector<int> threadHits(threadCount, 0);
    std::vector<int> threadGets(threadCount, 0);
    for (int t = 0; t < threadCount; ++t) {
        threadOps.push_back(generateOperations(opsPerThread, workload.getPercent, config.keySpace,
                                               1234u + static_cast<unsigned>(t)));
        threadLatencies[t].resize(opsPerThread);
    }

    std::latch ready(threadCount + 1);
    std::latch start(1);
    std::vector<std::thread> threads;
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&, t]() {
            const auto& ops = threadOps[t];
            auto& latencies = threadLatencies[t];
            int hits = 0;
            int gets = 0;

            ready.count_down();
            start.wait();

            for (int i = 0; i < opsPerThread; ++i) {
                const std::string& key = keys[ops[i].keyIndex];
                auto begin = std::chrono::steady_clock::now();
                if (ops[i].isGet) {
                    if (cache.get(key)) {
                        ++hits;
                    }
                    ++gets;
                } else {
                    cache.put(key, value);
                }
                auto end = std::chrono::steady_clock::now();
                latencies[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
            }
            threadHits[t] = hits;
            threadGets[t] = gets;
        });
    }

    ready.arrive_and_wait();
    auto wallStart = std::chrono::steady_clock::now();
    start.count_down();
    for (auto& thread : threads) {
        thread.join();
    }
    auto wallEnd = std::chrono::steady_clock::now();

    std::vector<std::int64_t> all;
    all.reserve(static_cast<std::size_t>(opsPerThread) * threadCount);
    int hits = 0;
    int gets = 0;
    for (int t = 0; t < threadCount; ++t) {
        all.insert(all.end(), threadLatencies[t].begin(), threadLatencies[t].end());
        hits += threadHits[t];
        gets += threadGets[t];
    }
    std::sort(all.begin(), all.end());

    double total = 0;
    for (auto ns : all) {
        total += static_cast<double>(ns);
    }

    Result result;
    result.seconds = std::chrono::duration<double>(wallEnd - wallStart).count();
    result.opsPerSecond = static_cast<double>(all.size()) / result.seconds;
    result.averageNs = total / static_cast<double>(all.size());
    result.p50Ns = percentile(all, 50);
    result.p95Ns = percentile(all, 95);
    result.p99Ns = percentile(all, 99);
    result.hitRate = gets > 0 ? static_cast<double>(hits) / gets : 0.0;
    return result;
}

void printUsage(const char* program) {
    std::printf("Usage: %s [--ops N] [--capacity N] [--keys N] [--max-threads N]\n", program);
}

bool parseArgs(int argc, char** argv, Config& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (i + 1 >= argc) {
            return false;
        }
        long number = std::strtol(argv[++i], nullptr, 10);
        if (number <= 0) {
            return false;
        }
        if (arg == "--ops") {
            config.totalOps = static_cast<int>(number);
        } else if (arg == "--capacity") {
            config.capacity = static_cast<std::size_t>(number);
        } else if (arg == "--keys") {
            config.keySpace = static_cast<int>(number);
        } else if (arg == "--max-threads") {
            config.maxThreads = static_cast<int>(number);
        } else {
            return false;
        }
    }
    return static_cast<std::size_t>(config.keySpace) >= config.capacity;
}

}  // namespace

int main(int argc, char** argv) {
    Config config;
    if (!parseArgs(argc, argv, config)) {
        printUsage(argv[0]);
        return 1;
    }

    std::vector<std::string> keys;
    keys.reserve(config.keySpace);
    for (int i = 0; i < config.keySpace; ++i) {
        keys.push_back("key:" + std::to_string(i));
    }
    const std::string value(32, 'v');

    const Workload workloads[] = {
        {"read-heavy (80% get / 20% put)", 80},
        {"balanced (50% get / 50% put)", 50},
        {"write-heavy (20% get / 80% put)", 20},
    };

    std::vector<int> threadCounts;
    for (int t = 1; t <= config.maxThreads; t *= 2) {
        threadCounts.push_back(t);
    }

    std::printf("compiler:        %s\n", __VERSION__);
    std::printf("build type:      %s\n", CACHE_BUILD_TYPE);
    std::printf("cpu:             %s\n", readCpuName().c_str());
    std::printf("hardware threads:%u\n", std::thread::hardware_concurrency());
    std::printf("cache capacity:  %zu\n", config.capacity);
    std::printf("key space:       %d\n", config.keySpace);
    std::printf("value size:      %zu bytes\n", value.size());
    std::printf("total ops/run:   %d\n\n", config.totalOps);

    for (const auto& workload : workloads) {
        std::printf("%s\n", workload.name);
        std::printf("%8s %10s %14s %10s %8s %8s %8s %8s\n",
                    "threads", "time(s)", "ops/sec", "avg(ns)", "p50", "p95", "p99", "hit%");
        for (int threads : threadCounts) {
            Result r = runWorkload(config, workload, threads, keys, value);
            std::printf("%8d %10.3f %14.0f %10.1f %8lld %8lld %8lld %7.1f%%\n",
                        threads, r.seconds, r.opsPerSecond, r.averageNs,
                        static_cast<long long>(r.p50Ns), static_cast<long long>(r.p95Ns),
                        static_cast<long long>(r.p99Ns), r.hitRate * 100.0);
        }
        std::printf("\n");
    }
    return 0;
}
