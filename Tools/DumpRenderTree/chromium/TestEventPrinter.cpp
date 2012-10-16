/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestEventPrinter.h"
#include "v8.h"

#include <stdio.h>
#include <stdlib.h>
#include <wtf/Assertions.h>
#include <wtf/text/Base64.h>

TestEventPrinter::TestEventPrinter()
    : m_encodeBinary(false)
{
}

TestEventPrinter::~TestEventPrinter()
{
}

// ----------------------------------------------------------------

void TestEventPrinter::handleTestHeader(const char*) const
{
}

void TestEventPrinter::handleTimedOut() const
{
    fprintf(stderr, "FAIL: Timed out waiting for notifyDone to be called\n");
    fprintf(stdout, "FAIL: Timed out waiting for notifyDone to be called\n");
}

void TestEventPrinter::handleDumpMemoryHeader() const
{
    v8::HeapStatistics heapStatistics;
    printf("DumpJSHeap: %zu\n", heapStatistics.used_heap_size());
}

void TestEventPrinter::handleTextHeader() const
{
    printf("Content-Type: text/plain\n");
}

void TestEventPrinter::handleTextFooter() const
{
    printf("#EOF\n");
    fprintf(stderr, "#EOF\n");
}

void TestEventPrinter::handleAudio(const void* audioData, size_t audioSize) const
{
    printf("Content-Type: audio/wav\n");
    handleBinary(audioData, audioSize);
}

void TestEventPrinter::handleAudioFooter() const
{
    printf("#EOF\n");
    fprintf(stderr, "#EOF\n");
}

void TestEventPrinter::handleImage(const char* actualHash, const char* expectedHash, const void* imageData, size_t imageSize) const
{
    ASSERT(actualHash);
    printf("\nActualHash: %s\n", actualHash);
    if (expectedHash && expectedHash[0])
        printf("\nExpectedHash: %s\n", expectedHash);
    if (imageData && imageSize) {
        printf("Content-Type: image/png\n");
        handleBinary(imageData, imageSize);
    }
}

void TestEventPrinter::handleTestFooter(bool) const
{
    printf("#EOF\n");
}

void TestEventPrinter::handleBinary(const void* data, size_t size) const
{
    Vector<char> base64;
    if (m_encodeBinary) {
        base64Encode(static_cast<const char*>(data), size, base64, Base64InsertLFs);
        data = base64.data();
        size = base64.size();
        printf("Content-Transfer-Encoding: base64\n");
    }
    // Printf formatting for size_t on 32-bit, 64-bit, and on Windows is hard so just cast to an int.
    printf("Content-Length: %d\n", static_cast<int>(size));
    if (fwrite(data, 1, size, stdout) != size) {
        fprintf(stderr, "Short write to stdout.\n");
        exit(1);
    }
}
