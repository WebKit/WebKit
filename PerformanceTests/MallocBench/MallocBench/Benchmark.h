/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef Benchmark_h
#define Benchmark_h

#include <map>
#include <string>

typedef void (*BenchmarkFunction)(bool isParallel);
struct BenchmarkPair;

class Benchmark {
public:
    struct Memory {
        Memory()
            : resident()
            , residentMax()
        {
        }
        
        Memory(size_t resident, size_t residentMax)
            : resident(resident)
            , residentMax(residentMax)
        {
        }

        Memory operator-(const Memory& other)
        {
            return Memory(resident - other.resident, residentMax - other.residentMax);
        }
    
        size_t resident;
        size_t residentMax;
    };

    static double currentTimeMS();
    static Memory currentMemoryBytes();

    Benchmark(const std::string&, bool isParallel, bool measureHeap, size_t heapSize);
    
    bool isValid() { return m_benchmarkPair; }
    
    void printBenchmarks();
    void run();
    void printReport();

private:
    typedef std::map<std::string, BenchmarkFunction> MapType;
    
    void runOnce();

    MapType m_map;

    const BenchmarkPair* m_benchmarkPair;
    bool m_isParallel;
    bool m_measureHeap;
    size_t m_heapSize;

    Memory m_memory;
    double m_elapsedTime;
};

#endif // Benchmark_h
