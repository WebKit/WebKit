/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

// The fuzzer RSS grows. This tries to investigate the corpus.
// ~/Build/Release/ANGLETranslatorFuzzerStats CorpusNew3/* |grep -v "diff: 0" > rss-increases.txt
// sort rss-increases.txt |less
// ~/Build/Release/ANGLETranslatorFuzzerStats CorpusNew3/0d994bf8eb288db4fb26be209a3ae3e048f989e

#include <fstream>
#include <iostream>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size);

// In order to link with the fuzzer that uses LLVMFuzzerMutate, provide a dummy symbol for the
// function.
extern "C" size_t LLVMFuzzerMutate(uint8_t *, size_t, size_t)
{
    exit(1);
    return 0;
}

static size_t getRSSKB()
{
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage))
    {
        return 0;
    }
    return static_cast<size_t>(usage.ru_maxrss >> 10L);
}

static const int iterations = 100;

int main(int argc, const char *argv[])
{
    std::vector<uint8_t> fileData;
    for (int j = 0; j < iterations; ++j)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::streampos fileSize;
            {
                std::ifstream file{argv[i], std::ios::binary};
                file.seekg(0, std::ios::end);
                fileSize = file.tellg();
                file.seekg(0, std::ios::beg);
                if (fileData.size() < static_cast<size_t>(fileSize))
                {
                    fileData.resize(static_cast<size_t>(fileSize));
                }

                file.read(reinterpret_cast<char *>(&fileData[0]), fileSize);
            }
            size_t rss = getRSSKB();
            LLVMFuzzerTestOneInput(&fileData[0], fileSize);
            size_t newRSS = getRSSKB();
            if (iterations == 1 || i != 0)
            {
                std::cout << argv[i] << " rss: " << rss << "kb rss diff: " << newRSS - rss << "kb"
                          << std::endl;
            }
        }
    }
    return 0;
}
