/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

// On Mac, you can build this like so:
// xcrun clang++ -o BitSetSpeedTest Source/WTF/benchmarks/BitSetSpeedTest.cpp -O3 -W -ISource/WTF -ISource/WTF/icu -ISource/WTF/benchmarks -ISource/bmalloc -ISource/bmalloc/bmalloc -ISource/bmalloc/libpas/src/libpas -LWebKitBuild/Debug -lWTF -lbmalloc -lpas -framework Foundation -framework Security -licucore -std=c++23 -fvisibility=hidden -arch arm64e

#include "config.h"

#include <wtf/BitSet.h>
#include <wtf/DataLog.h>
#include <wtf/MonotonicTime.h>

namespace {
[[noreturn]] void usage()
{
    dataLogF("BitSetSpeedTest <number of seconds to run each test> <number of times to run each test> [allzeros|allones|alternating|sparse|dense|random|all]\n");
    exit(1);
}

constexpr size_t numberOfBitPatterns = 6;
enum class BitPattern {
    AllZeros,
    AllOnes,
    Alternating,
    Sparse,
    Dense,
    Random
};

double secondsPerTest;
unsigned numberOfRuns;
BitPattern pattern { BitPattern::Random };
bool runAllBitPatterns { false };

const char* getPatternName()
{
    switch (pattern) {
    case BitPattern::AllZeros:
        return "allzeros";
    case BitPattern::AllOnes:
        return "allones";
    case BitPattern::Alternating:
        return "alternating";
    case BitPattern::Sparse:
        return "sparse";
    case BitPattern::Dense:
        return "sparse";
    case BitPattern::Random:
        return "random";
    };
}

void parseArgs(int argc, char** argv)
{
    if (argc < 3)
        usage();

    if (!sscanf(argv[1], "%lf", &secondsPerTest))
        usage();
    if (!sscanf(argv[2], "%u", &numberOfRuns))
        usage();

    if (argc > 3) {
        if (!strcmp(argv[3], "allones")) // NOLINT
            pattern = BitPattern::AllOnes;
        else if (!strcmp(argv[3], "allzeros")) // NOLINT
            pattern = BitPattern::AllZeros;
        else if (!strcmp(argv[3], "alternating")) // NOLINT
            pattern = BitPattern::Alternating;
        else if (!strcmp(argv[3], "sparse")) // NOLINT
            pattern = BitPattern::Sparse;
        else if (!strcmp(argv[3], "dense")) // NOLINT
            pattern = BitPattern::Dense;
        else if (!strcmp(argv[3], "random")) // NOLINT
            pattern = BitPattern::Random;
        else if (!strcmp(argv[3], "all")) // NOLINT
            runAllBitPatterns = true;
        else
            usage();
    }

}

ALWAYS_INLINE size_t iterationsForOperation(auto operation)
{
    // warm up cache
    WTF::MonotonicTime end = MonotonicTime::now() + WTF::Seconds(0.01);
    while (MonotonicTime::now() < end)
        operation();

    end = MonotonicTime::now() + WTF::Seconds(secondsPerTest);
    size_t iterations { 0 };

    while (MonotonicTime::now() < end) {
        operation();
        iterations++;
    }

    return iterations;
};

template <size_t T>
void initializeBitSet(WTF::BitSet<T>& bitSet)
{
    if (pattern == BitPattern::AllZeros) {
        bitSet.clearAll();
        return;
    }

    if (pattern == BitPattern::AllOnes) {
        bitSet.setAll();
        return;
    }

    if (pattern == BitPattern::Random) {
        uint32_t seed = 0x12345678;
        for (size_t i = 0; i < T; i++) {
            seed = seed * 1103515245 + 12345;
            if (seed & 1)
                bitSet.set(i);
        }
        return;
    }


    for (size_t i = 0; i < T; i++) {
        switch (pattern) {
        case BitPattern::Alternating:
            if (!(i % 2))
                bitSet.set(i);
            break;
        case BitPattern::Sparse:
            if (!(i % 8))
                bitSet.set(i);
            break;
        case BitPattern::Dense:
            if (i % 8)
                bitSet.set(i);
            break;
        default:
            break;
        }
    }
};

template <size_t T>
size_t benchmarkXorEquals()
{
    WTF::BitSet<T> bitSet1;
    WTF::BitSet<T> bitSet2;
    initializeBitSet(bitSet1);
    initializeBitSet(bitSet2);
    return iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        bitSet1 ^= bitSet2;
    });
}

template <size_t T>
size_t benchmarkAndEquals()
{
    WTF::BitSet<T> bitSet1;
    WTF::BitSet<T> bitSet2;
    initializeBitSet(bitSet1);
    initializeBitSet(bitSet2);
    WTF::BitSet<T> originalBitSet = bitSet1;
    return iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        bitSet1 &= bitSet2;
        bitSet1 = originalBitSet;
    });
}

template <size_t T>
size_t benchmarkOrEquals()
{
    WTF::BitSet<T> bitSet1;
    WTF::BitSet<T> bitSet2;
    initializeBitSet(bitSet1);
    initializeBitSet(bitSet2);
    WTF::BitSet<T> originalBitSet = bitSet1;
    return iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        bitSet1 |= bitSet2;
        bitSet1 = originalBitSet;
    });
}

template <size_t T>
size_t benchmarkEquality()
{
    WTF::BitSet<T> bitSet1;
    WTF::BitSet<T> bitSet2;
    initializeBitSet(bitSet1);
    initializeBitSet(bitSet2);
    volatile bool sink = false; // prevent optimization
    size_t iterations = iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        sink = (bitSet1 == bitSet2);
    });

    if (static_cast<int>(sink) == 3) [[unlikely]]
        dataLogF("impossible\n");

    return iterations;
}

template <size_t T>
size_t benchmarkSetAndClear()
{
    WTF::BitSet<T> bitSet1;
    WTF::BitSet<T> bitSet2;
    initializeBitSet(bitSet1);
    initializeBitSet(bitSet2);
    WTF::BitSet<T> originalBitSet1 = bitSet1;
    WTF::BitSet<T> originalBitSet2 = bitSet2;
    return iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        bitSet1.setAndClear(bitSet2);
        bitSet1 = originalBitSet1;
        bitSet2 = originalBitSet2;
    });
}

template <size_t T>
size_t benchmarkMergeAndClear()
{
    WTF::BitSet<T> bitSet1;
    WTF::BitSet<T> bitSet2;
    initializeBitSet(bitSet1);
    initializeBitSet(bitSet2);
    WTF::BitSet<T> originalBitSet1 = bitSet1;
    WTF::BitSet<T> originalBitSet2 = bitSet2;
    return iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        bitSet1.mergeAndClear(bitSet2);
        bitSet1 = originalBitSet1;
        bitSet2 = originalBitSet2;
    });
}

template <size_t T>
size_t benchmarkSubsumes()
{
    WTF::BitSet<T> bitSet1;
    WTF::BitSet<T> bitSet2;
    initializeBitSet(bitSet1);
    initializeBitSet(bitSet2);
    volatile bool sink = false; // prevent optimization
    size_t iterations = iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        sink = bitSet1.subsumes(bitSet2);
    });

    if (static_cast<int>(sink) == 3) [[unlikely]]
        dataLogF("impossible\n");

    return iterations;
}

template <size_t T>
size_t benchmarkExclude()
{
    WTF::BitSet<T> bitSet1;
    WTF::BitSet<T> bitSet2;
    initializeBitSet(bitSet1);
    initializeBitSet(bitSet2);
    WTF::BitSet<T> originalBitSet = bitSet1;
    return iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        bitSet1.exclude(bitSet2);
        bitSet1 = originalBitSet;
    });
}

template <size_t T>
size_t benchmarkIsFull()
{
    WTF::BitSet<T> bitSet;
    initializeBitSet(bitSet);
    volatile bool sink = false; // prevent optimization
    size_t iterations = iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        sink = bitSet.isFull();
    });

    if (static_cast<int>(sink) == 3) [[unlikely]]
        dataLogF("impossible\n");

    return iterations;
}

template <size_t T>
size_t benchmarkIsEmpty()
{
    WTF::BitSet<T> bitSet;
    initializeBitSet(bitSet);
    volatile bool sink = false; // prevent optimization
    size_t iterations = iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        sink = bitSet.isEmpty();
    });

    if (static_cast<int>(sink) == 3) [[unlikely]]
        dataLogF("impossible\n");

    return iterations;
}

template <size_t T>
size_t benchmarkInvert()
{
    WTF::BitSet<T> bitSet;
    initializeBitSet(bitSet);
    return iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        bitSet.invert();
        bitSet.invert(); // back to original
    });
}

template <size_t T>
size_t benchmarkFilter()
{
    WTF::BitSet<T> bitSet1;
    WTF::BitSet<T> bitSet2;
    initializeBitSet(bitSet1);
    initializeBitSet(bitSet2);
    WTF::BitSet<T> originalBitSet = bitSet1;
    return iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        bitSet1.filter(bitSet2);
        bitSet1 = originalBitSet;
    });
}

template <size_t T>
size_t benchmarkMerge()
{
    WTF::BitSet<T> bitSet1;
    WTF::BitSet<T> bitSet2;
    initializeBitSet(bitSet1);
    initializeBitSet(bitSet2);
    WTF::BitSet<T> originalBitSet = bitSet1;
    return iterationsForOperation([&] ALWAYS_INLINE_LAMBDA {
        bitSet1.merge(bitSet2);
        bitSet1 = originalBitSet;
    });
}

template <size_t T>
void reportResults(size_t iterations)
{
    double opsPerSecond = static_cast<double>(iterations) / secondsPerTest;
    double nsPerOp = (secondsPerTest * 1e9) / iterations;
    dataLogF("(%zu bits): %.2f Mops/s, %.2f ns/op\n", T, opsPerSecond / 1e6, nsPerOp);
}

template <size_t T>
void runFuncAndReport(auto func)
{
    std::vector<size_t> runs;
    for (size_t i = 0; i < numberOfRuns; i++) {
        size_t iterations = func.template operator()<T>();
        runs.push_back(iterations);
    }
    std::sort(runs.begin(), runs.end());
    size_t median = runs[runs.size() / 2];
    reportResults<T>(median);
}

template <typename Callback>
void runBenchmark(const char* name, Callback func)
{
    dataLogF("=============== %s ================\n", name);
    runFuncAndReport<64>(func); // doesn't use SIMD
    runFuncAndReport<512>(func);
    runFuncAndReport<511>(func); // doesn't fit evenly into SIMD vector
    runFuncAndReport<4096>(func);
    runFuncAndReport<262144>(func); // 32KB - L1/L2 boundary
    runFuncAndReport<2097152>(func); // 256KB - L2/L3 boundary
}
}

int main(int argc, char** argv)
{
    parseArgs(argc, argv);

    auto runBenchmarks = []() {
        runBenchmark("merge", []<size_t T>() {
            return benchmarkMerge<T>();
        });
        runBenchmark("filter", []<size_t T>() {
            return benchmarkFilter<T>();
        });
        runBenchmark("invert", []<size_t T>() {
            return benchmarkInvert<T>();
        });
        runBenchmark("isEmpty", []<size_t T>() {
            return benchmarkIsEmpty<T>();
        });
        runBenchmark("isFull", []<size_t T>() {
            return benchmarkIsFull<T>();
        });
        runBenchmark("exclude", []<size_t T>() {
            return benchmarkExclude<T>();
        });
        runBenchmark("subsumes", []<size_t T>() {
            return benchmarkSubsumes<T>();
        });
        runBenchmark("mergeAndClear", []<size_t T>() {
            return benchmarkMergeAndClear<T>();
        });
        runBenchmark("setAndClear", []<size_t T>() {
            return benchmarkSetAndClear<T>();
        });
        runBenchmark("equality", []<size_t T>() {
            return benchmarkEquality<T>();
        });
        runBenchmark("|=", []<size_t T>() {
            return benchmarkOrEquals<T>();
        });
        runBenchmark("&=", []<size_t T>() {
            return benchmarkAndEquals<T>();
        });
        runBenchmark("^=", []<size_t T>() {
            return benchmarkXorEquals<T>();
        });
    };

    auto printTitle = []() {
        dataLogF("seconds per test: %lf, number of runs per test: %u, bit pattern: %s\n", secondsPerTest, numberOfRuns, getPatternName());
    };

    dataLogF("============ STARTING BENCHMARK ==============\n");
    if (runAllBitPatterns) {
        for (size_t i = 0; i < numberOfBitPatterns; i++) {
            pattern = static_cast<BitPattern>(i);
            printTitle();
            runBenchmarks();
        }
    } else {
        printTitle();
        runBenchmarks();
    }

    return 0;
}
