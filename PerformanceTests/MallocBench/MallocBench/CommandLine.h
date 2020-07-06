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

#ifndef CommandLine_h
#define CommandLine_h

#include <string>

class CommandLine {
public:
    CommandLine(int argc, char** argv);

    bool isValid() { return m_benchmarkName.size(); }
    const std::string& benchmarkName() { return m_benchmarkName; }
    bool isParallel() { return m_isParallel; }
    bool useThreadID() { return m_useThreadID; }
    bool detailedReport() { return m_detailedReport; }
    bool warmUp() { return m_warmUp; }
    size_t heapSize() { return m_heapSize; }
    size_t runs() { return m_runs; }

    void printUsage();

private:
    static struct option longOptions[];

    char** m_argv;
    std::string m_benchmarkName;
    bool m_detailedReport;
    bool m_isParallel;
    bool m_useThreadID;
    bool m_warmUp;
    size_t m_heapSize;
    size_t m_runs;
};

#endif // CommandLine_h
