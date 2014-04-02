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

#include "Benchmark.h"
#include "CommandLine.h"
#include <iostream>
#include <map>
#include <sstream>
#include <string>

using namespace std;

int main(int argc, char** argv)
{
    CommandLine commandLine(argc, argv);
    if (!commandLine.isValid()) {
        commandLine.printUsage();
        exit(1);
    }

    Benchmark benchmark(commandLine.benchmarkName(), commandLine.isParallel(), commandLine.measureHeap(), commandLine.heapSize());
    if (!benchmark.isValid()) {
        cout << "Invalid benchmark: " << commandLine.benchmarkName() << endl << endl;
        benchmark.printBenchmarks();
        exit(1);
    }

    string parallel = commandLine.isParallel() ? string(" [ parallel ]") : string(" [ not parallel ]");
    stringstream heapSize;
    if (commandLine.heapSize())
        heapSize << " [ heap: " << commandLine.heapSize() / 1024 / 1024 << "MB ]";
    else
        heapSize << " [ heap: 0MB ]";
    cout << "Running " << commandLine.benchmarkName() << parallel << heapSize.str() << "..." << endl;
    benchmark.run();
    benchmark.printReport();
        
    return 0;
}
