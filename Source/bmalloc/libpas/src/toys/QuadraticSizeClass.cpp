/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include <cstddef>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

namespace {

constexpr uintptr_t minAlignShift = 4;
constexpr uintptr_t minAlign = static_cast<uintptr_t>(1) << minAlignShift;
constexpr uintptr_t upperBoundExclusive = 1000;
constexpr uintptr_t sizeCellColumns = 4;

void printSizeCell(uintptr_t size)
{
    ostringstream sizeStringOut;
    sizeStringOut << size;

    string sizeString = sizeStringOut.str();
    while (sizeString.size() < sizeCellColumns)
        sizeString = " " + sizeString;

    cout << sizeString;
}

void printProgression(const string& name, function<uintptr_t(uintptr_t)> mapping)
{
    cout << name << "\n";
    
    cout << "    ";
    for (uintptr_t size = 0; size < upperBoundExclusive; size += minAlign)
        printSizeCell(size);
    cout << "\n";
    
    cout << "    ";
    for (uintptr_t size = 0; size < upperBoundExclusive; size += minAlign)
        printSizeCell(mapping(size));
    cout << "\n";
}

#define PRINT_PROGRESSION(mapping) \
    printProgression(#mapping, (mapping))

} // anonymous namespace

int main(int argc, char** argv)
{
    PRINT_PROGRESSION([] (uintptr_t size) { return (size + minAlign - 1) >> minAlignShift; });
    PRINT_PROGRESSION([] (uintptr_t size) { return ((size + minAlign - 1) >> minAlignShift) - size * size / 14000; });
    PRINT_PROGRESSION([] (uintptr_t size) {
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          return index - index * index / 64;
                      });
    PRINT_PROGRESSION([] (uintptr_t size) {
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          return index - index * index / 48;
                      });
    PRINT_PROGRESSION([] (uintptr_t size) {
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          if (index <= 6)
                              return index;
                          return index - index * index / 48;
                      });
    PRINT_PROGRESSION([] (uintptr_t size) {
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          if (index <= 6)
                              return index;
                          return index - index * index / 40;
                      });
    PRINT_PROGRESSION([] (uintptr_t size) {
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          return index - index * index / 32;
                      });
    PRINT_PROGRESSION([] (uintptr_t size) {
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          if (index <= 5)
                              return index;
                          return index - index * index / 32;
                      });
    PRINT_PROGRESSION([] (uintptr_t size) {
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          if (index <= 5)
                              return index;
                          uintptr_t square = index * index;
                          if (index <= 14)
                              return index - square / 32;
                          return index - square / 50 - 2;
                      });
    PRINT_PROGRESSION([] (uintptr_t size) {
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          if (index <= 5)
                              return index;
                          if (index <= 12)
                              return ((index - 1) >> 1) + 3;
                          return (index >> 2) + 6;
                      });
    PRINT_PROGRESSION([] (uintptr_t size) {
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          if (index <= 6)
                              return index;
                          if (index <= 12)
                              return ((index - 1) >> 1) + 4;
                          return (index >> 2) + 7;
                      });
    PRINT_PROGRESSION([] (uintptr_t size) -> uintptr_t {
                          if (!size)
                              return 0;
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          if (index <= 5)
                              return index - 1;
                          if (index <= 12)
                              return ((index - 1) >> 1) + 2;
                          return (index >> 2) + 5;
                      });
    PRINT_PROGRESSION([] (uintptr_t size) -> uintptr_t {
                          if (!size)
                              return 0;
                          uintptr_t index = (size + minAlign - 1) >> minAlignShift;
                          if (index <= 6)
                              return index - 1;
                          if (index <= 12)
                              return ((index - 1) >> 1) + 3;
                          return (index >> 2) + 6;
                      });
    
    return 0;
}

