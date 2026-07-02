/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// Compile (from $WEBKIT_ROOT, after a mac-dev-release build):
//   xcrun clang++ -o HashSetVariantsBenchmark \
//     Source/WTF/benchmarks/HashSetVariantsBenchmark.cpp \
//     -std=c++23 -O3 -DNDEBUG=1 -fno-exceptions -fno-rtti \
//     -ISource/WTF \
//     -IWebKitBuild/cmake-mac/Release \
//     -IWebKitBuild/cmake-mac/Release/WTF/DerivedSources \
//     -FWebKitBuild/cmake-mac/Release \
//     -framework JavaScriptCore -framework Foundation
//
// Run:
//   DYLD_FRAMEWORK_PATH=WebKitBuild/cmake-mac/Release ./HashSetVariantsBenchmark
//
// Optional first arg: filter substring (matches container, op, or key type).
// Optional second arg: a multiplier for repetitions (default 1.0).

#include "config.h"

#include <wtf/Atomics.h>
#include <wtf/DataLog.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OrderedHashSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace {

// Sizes are chosen so all three containers settle at the SAME bucket count
// and SAME load factor (~50%) after construction. Power-of-two sizes hit
// HashTable's post-insert expansion edge (final load 25%) and
// OrderedHashTable's pre-insert expansion edge (final load 50%) — different
// loads, so lookup measurements compare layouts AND probe counts. Sizes of
// 2^k - 1 sit just below both expansion triggers, equalizing both.
constexpr unsigned kCapacities[] = { 7, 63, 511, 4095, 32767, 262143 };

const char* g_filter = nullptr;
double g_repMultiplier = 1.0;

struct Row {
    const char* container;
    const char* op;
    const char* keyType;
    unsigned n;
    double nsPerOp;
};

WTF::Vector<Row> g_rows;

bool matchesFilter(const char* container, const char* op, const char* keyType)
{
    if (!g_filter)
        return true;
    auto contains = [](const char* haystack, const char* needle) {
        return strstr(haystack, needle) != nullptr;
    };
    return contains(container, g_filter) || contains(op, g_filter) || contains(keyType, g_filter);
}

unsigned scaledReps(unsigned baseReps)
{
    double scaled = baseReps * g_repMultiplier;
    if (scaled < 1.0)
        scaled = 1.0;
    return static_cast<unsigned>(scaled);
}

// Reps tuned so each cell takes roughly 50-300ms wall time.
unsigned repsForInsert(unsigned n)
{
    if (n <=     8) return scaledReps(2000000);
    if (n <=    64) return scaledReps(300000);
    if (n <=   512) return scaledReps(40000);
    if (n <=  4096) return scaledReps(4000);
    if (n <= 32768) return scaledReps(400);
    return scaledReps(40);
}

unsigned repsForLookup(unsigned n)
{
    if (n <=     8) return scaledReps(8000000);
    if (n <=    64) return scaledReps(1000000);
    if (n <=   512) return scaledReps(100000);
    if (n <=  4096) return scaledReps(10000);
    if (n <= 32768) return scaledReps(1000);
    return scaledReps(100);
}

unsigned repsForIterate(unsigned n)
{
    if (n <=     8) return scaledReps(8000000);
    if (n <=    64) return scaledReps(1000000);
    if (n <=   512) return scaledReps(100000);
    if (n <=  4096) return scaledReps(10000);
    if (n <= 32768) return scaledReps(1000);
    return scaledReps(100);
}

unsigned repsForChurn(unsigned n)
{
    if (n <=     8) return scaledReps(2000000);
    if (n <=    64) return scaledReps(300000);
    if (n <=   512) return scaledReps(40000);
    if (n <=  4096) return scaledReps(4000);
    if (n <= 32768) return scaledReps(400);
    return scaledReps(40);
}

template<typename T> T makeKey(unsigned i);

template<> uint32_t makeKey<uint32_t>(unsigned i) { return i + 1; }
template<> void* makeKey<void*>(unsigned i)
{
    // Multiply to spread bits more like real heap pointers, and avoid 0.
    return std::bit_cast<void*>(static_cast<uintptr_t>((i + 1) * 16));
}
template<> WTF::String makeKey<WTF::String>(unsigned i)
{
    return WTF::String::number(i + 1);
}

template<typename T>
WTF::Vector<T> makeKeys(unsigned n)
{
    WTF::Vector<T> v;
    v.reserveInitialCapacity(n);
    for (unsigned i = 0; i < n; ++i)
        v.append(makeKey<T>(i));
    return v;
}

// To avoid stale-pointer hazards with WTF::String, keep present strings around
// for the lifetime of "non-present" lookups too. We don't lookup absent keys
// here, but the scaffold supports it.

template<template<typename...> class SetTemplate, typename T>
NEVER_INLINE void benchInsert(unsigned n, const char* container, const char* keyType)
{
    if (!matchesFilter(container, "Insert", keyType))
        return;

    auto keys = makeKeys<T>(n);
    unsigned reps = repsForInsert(n);

    auto before = WTF::MonotonicTime::now();
    for (unsigned r = 0; r < reps; ++r) {
        SetTemplate<T> set;
        for (unsigned i = 0; i < n; ++i)
            set.add(keys[i]);
        WTF::compilerFence();
    }
    auto after = WTF::MonotonicTime::now();

    double totalNs = (after - before).nanoseconds();
    g_rows.append({ container, "Insert", keyType, n, totalNs / (static_cast<double>(reps) * n) });
}

template<template<typename...> class SetTemplate, typename T>
NEVER_INLINE void benchContainsHit(unsigned n, const char* container, const char* keyType)
{
    if (!matchesFilter(container, "ContainsHit", keyType))
        return;

    auto keys = makeKeys<T>(n);
    SetTemplate<T> set;
    for (unsigned i = 0; i < n; ++i)
        set.add(keys[i]);

    unsigned reps = repsForLookup(n);
    unsigned hitCount = 0;

    auto before = WTF::MonotonicTime::now();
    for (unsigned r = 0; r < reps; ++r) {
        for (unsigned i = 0; i < n; ++i) {
            if (set.contains(keys[i]))
                ++hitCount;
        }
        WTF::compilerFence();
    }
    auto after = WTF::MonotonicTime::now();

    if (hitCount != reps * n)
        WTF::dataLog("WARNING: hit mismatch (", container, "/", keyType, "/n=", n, ")\n");

    double totalNs = (after - before).nanoseconds();
    g_rows.append({ container, "ContainsHit", keyType, n, totalNs / (static_cast<double>(reps) * n) });
}

template<template<typename...> class SetTemplate, typename T>
NEVER_INLINE void benchIterate(unsigned n, const char* container, const char* keyType)
{
    if (!matchesFilter(container, "Iterate", keyType))
        return;

    auto keys = makeKeys<T>(n);
    SetTemplate<T> set;
    for (unsigned i = 0; i < n; ++i)
        set.add(keys[i]);

    unsigned reps = repsForIterate(n);
    uintptr_t sink = 0;

    auto before = WTF::MonotonicTime::now();
    for (unsigned r = 0; r < reps; ++r) {
        for (const auto& v : set) {
            if constexpr (std::is_same_v<T, WTF::String>)
                sink ^= v.impl() ? v.impl()->hash() : 0u;
            else if constexpr (std::is_same_v<T, void*>)
                sink ^= std::bit_cast<uintptr_t>(v);
            else
                sink ^= static_cast<uintptr_t>(v);
        }
        WTF::compilerFence();
    }
    auto after = WTF::MonotonicTime::now();

    // Prevent dead-code elimination.
    asm volatile("" : : "r"(sink) : "memory");

    double totalNs = (after - before).nanoseconds();
    g_rows.append({ container, "Iterate", keyType, n, totalNs / (static_cast<double>(reps) * n) });
}

// Churn: remove half the keys then re-insert them, repeated.
template<template<typename...> class SetTemplate, typename T>
NEVER_INLINE void benchChurn(unsigned n, const char* container, const char* keyType)
{
    if (!matchesFilter(container, "Churn", keyType))
        return;

    auto keys = makeKeys<T>(n);
    SetTemplate<T> set;
    for (unsigned i = 0; i < n; ++i)
        set.add(keys[i]);

    unsigned half = n / 2;
    if (!half)
        half = 1;
    unsigned reps = repsForChurn(n);

    auto before = WTF::MonotonicTime::now();
    for (unsigned r = 0; r < reps; ++r) {
        // Remove the second half.
        for (unsigned i = half; i < n; ++i)
            set.remove(keys[i]);
        // Reinsert them.
        for (unsigned i = half; i < n; ++i)
            set.add(keys[i]);
        WTF::compilerFence();
    }
    auto after = WTF::MonotonicTime::now();

    double totalNs = (after - before).nanoseconds();
    // Each rep does (n - half) removes + (n - half) inserts. Normalize per-op.
    double opsPerRep = 2.0 * static_cast<double>(n - half);
    g_rows.append({ container, "Churn", keyType, n, totalNs / (static_cast<double>(reps) * opsPerRep) });
}

template<template<typename...> class SetTemplate, typename T>
void runAllOps(const char* container, const char* keyType)
{
    for (unsigned n : kCapacities) {
        benchInsert<SetTemplate, T>(n, container, keyType);
        benchContainsHit<SetTemplate, T>(n, container, keyType);
        benchIterate<SetTemplate, T>(n, container, keyType);
        benchChurn<SetTemplate, T>(n, container, keyType);
    }
}

template<typename T> using HashSetT = WTF::HashSet<T>;
template<typename T> using UncheckedKeyHashSetT = WTF::UncheckedKeyHashSet<T>;
template<typename T> using OrderedHashSetT = WTF::OrderedHashSet<T>;
template<typename T> using ListHashSetT = WTF::ListHashSet<T>;

void printRows()
{
    WTF::dataLog("\n");
    WTF::dataLog("container         op           key       n         ns/op\n");
    WTF::dataLog("---------         --           ---       -         -----\n");
    for (const auto& row : g_rows) {
        char line[256];
        snprintf(line, sizeof(line),
            "%-16s  %-11s  %-8s  %7u  %10.2f\n",
            row.container, row.op, row.keyType, row.n, row.nsPerOp);
        WTF::dataLogF("%s", line);
    }
}

const char* prettyN(unsigned n, char* buf, size_t bufSize)
{
    if (n >= 1024 * 1024)
        snprintf(buf, bufSize, "%uM", n / (1024 * 1024));
    else if (n >= 1024)
        snprintf(buf, bufSize, "%uK", n / 1024);
    else
        snprintf(buf, bufSize, "%u", n);
    return buf;
}

const Row* findRow(const char* container, const char* op, const char* keyType, unsigned n)
{
    for (const auto& row : g_rows) {
        if (row.n == n
            && !strcmp(row.container, container)
            && !strcmp(row.op, op)
            && !strcmp(row.keyType, keyType))
            return &row;
    }
    return nullptr;
}

void printMatrix()
{
    static const char* kOps[] = { "Insert", "ContainsHit", "Iterate", "Churn" };
    static const char* kContainers[] = { "HashSet", "UncheckedHashSet", "OrderedHashSet", "ListHashSet" };
    static const char* kKeyTypes[] = { "uint32_t", "void*", "String" };

    for (const char* op : kOps) {
        WTF::dataLog("\n=== ", op, " (ns/op) ===\n");

        // Header.
        char line[512];
        int written = snprintf(line, sizeof(line), "%-16s  %-8s", "container", "key");
        for (unsigned n : kCapacities) {
            char nbuf[16];
            written += snprintf(line + written, sizeof(line) - written, "  %8s", prettyN(n, nbuf, sizeof(nbuf)));
        }
        snprintf(line + written, sizeof(line) - written, "\n");
        WTF::dataLogF("%s", line);

        // Separator.
        WTF::dataLog("----------------  --------");
        for (unsigned i = 0; i < std::size(kCapacities); ++i)
            WTF::dataLog("  --------");
        WTF::dataLog("\n");

        // Rows: outer loop key type, inner loop container, so that each container
        // group is contiguous within a key type.
        for (const char* keyType : kKeyTypes) {
            for (const char* container : kContainers) {
                if (!matchesFilter(container, op, keyType))
                    continue;
                written = snprintf(line, sizeof(line), "%-16s  %-8s", container, keyType);
                for (unsigned n : kCapacities) {
                    if (const Row* r = findRow(container, op, keyType, n))
                        written += snprintf(line + written, sizeof(line) - written, "  %8.2f", r->nsPerOp);
                    else
                        written += snprintf(line + written, sizeof(line) - written, "  %8s", "-");
                }
                snprintf(line + written, sizeof(line) - written, "\n");
                WTF::dataLogF("%s", line);
            }
        }
    }
}

} // anonymous namespace

int main(int argc, char** argv)
{
    WTF::initialize();

    if (argc >= 2)
        g_filter = argv[1];
    if (argc >= 3)
        g_repMultiplier = atof(argv[2]);

    WTF::dataLog("HashSetVariantsBenchmark — comparing WTF::HashSet, WTF::OrderedHashSet, WTF::ListHashSet\n");
    WTF::dataLog("Capacities: ");
    for (unsigned n : kCapacities)
        WTF::dataLog(n, " ");
    WTF::dataLog("\n");
    if (g_filter)
        WTF::dataLog("Filter: ", g_filter, "\n");
    if (g_repMultiplier != 1.0)
        WTF::dataLog("Rep multiplier: ", g_repMultiplier, "\n");

    auto wallStart = WTF::MonotonicTime::now();

    runAllOps<HashSetT, uint32_t>("HashSet", "uint32_t");
    runAllOps<UncheckedKeyHashSetT, uint32_t>("UncheckedHashSet", "uint32_t");
    runAllOps<OrderedHashSetT, uint32_t>("OrderedHashSet", "uint32_t");
    runAllOps<ListHashSetT, uint32_t>("ListHashSet", "uint32_t");

    runAllOps<HashSetT, void*>("HashSet", "void*");
    runAllOps<UncheckedKeyHashSetT, void*>("UncheckedHashSet", "void*");
    runAllOps<OrderedHashSetT, void*>("OrderedHashSet", "void*");
    runAllOps<ListHashSetT, void*>("ListHashSet", "void*");

    runAllOps<HashSetT, WTF::String>("HashSet", "String");
    runAllOps<UncheckedKeyHashSetT, WTF::String>("UncheckedHashSet", "String");
    runAllOps<OrderedHashSetT, WTF::String>("OrderedHashSet", "String");
    runAllOps<ListHashSetT, WTF::String>("ListHashSet", "String");

    auto wallEnd = WTF::MonotonicTime::now();

    printRows();
    printMatrix();

    WTF::dataLog("\nTotal benchmark wall time: ", (wallEnd - wallStart).seconds(), " s\n");
    return 0;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
