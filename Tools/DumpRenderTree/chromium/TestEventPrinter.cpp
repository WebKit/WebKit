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

#include <stdio.h>
#include <stdlib.h>
#include <wtf/Assertions.h>

class DRTPrinter : public TestEventPrinter {
public:
    DRTPrinter() {}
    void handleTestHeader(const char* url) const;
    void handleTimedOut() const;
    void handleTextHeader() const;
    void handleTextFooter() const;
    void handleImage(const char* actualHash, const char* expectedHash, const unsigned char* imageData, size_t imageSize, const char* fileName) const;
    void handleImageFooter() const;
    void handleTestFooter(bool dumpedAnything) const;
};

class TestShellPrinter : public TestEventPrinter {
public:
    TestShellPrinter() {}
    void handleTestHeader(const char* url) const;
    void handleTimedOut() const;
    void handleTextHeader() const;
    void handleTextFooter() const;
    void handleImage(const char* actualHash, const char* expectedHash, const unsigned char* imageData, size_t imageSize, const char* fileName) const;
    void handleImageFooter() const;
    void handleTestFooter(bool dumpedAnything) const;
};

TestEventPrinter* TestEventPrinter::createDRTPrinter()
{
    return new DRTPrinter;
}

TestEventPrinter* TestEventPrinter::createTestShellPrinter()
{
    return new TestShellPrinter;
}

// ----------------------------------------------------------------

void DRTPrinter::handleTestHeader(const char*) const
{
}

void DRTPrinter::handleTimedOut() const
{
    fprintf(stderr, "FAIL: Timed out waiting for notifyDone to be called\n");
    fprintf(stdout, "FAIL: Timed out waiting for notifyDone to be called\n");
}

void DRTPrinter::handleTextHeader() const
{
    printf("Content-Type: text/plain\n");
}

void DRTPrinter::handleTextFooter() const
{
    printf("#EOF\n");
}

void DRTPrinter::handleImage(const char* actualHash, const char* expectedHash, const unsigned char* imageData, size_t imageSize, const char*) const
{
    ASSERT(actualHash);
    printf("\nActualHash: %s\n", actualHash);
    if (expectedHash && expectedHash[0])
        printf("\nExpectedHash: %s\n", expectedHash);
    if (imageData && imageSize) {
        printf("Content-Type: image/png\n");
        // Printf formatting for size_t on 32-bit, 64-bit, and on Windows is hard so just cast to an int.
        printf("Content-Length: %d\n", static_cast<int>(imageSize));
        if (fwrite(imageData, 1, imageSize, stdout) != imageSize) {
            fprintf(stderr, "Short write to stdout.\n");
            exit(1);
        }
    }
}

void DRTPrinter::handleImageFooter() const
{
    printf("#EOF\n");
}

void DRTPrinter::handleTestFooter(bool) const
{
}

// ----------------------------------------------------------------

void TestShellPrinter::handleTestHeader(const char* url) const
{
    printf("#URL:%s\n", url);
}

void TestShellPrinter::handleTimedOut() const
{
    puts("#TEST_TIMED_OUT\n");
}

void TestShellPrinter::handleTextHeader() const
{
}

void TestShellPrinter::handleTextFooter() const
{
}

void TestShellPrinter::handleImage(const char* actualHash, const char*, const unsigned char* imageData, size_t imageSize, const char* fileName) const
{
    ASSERT(actualHash);
    if (imageData && imageSize) {
        ASSERT(fileName);
        FILE* fp = fopen(fileName, "wb");
        if (!fp) {
            perror(fileName);
            exit(EXIT_FAILURE);
        }
        if (fwrite(imageData, 1, imageSize, fp) != imageSize) {
            perror(fileName);
            fclose(fp);
            exit(EXIT_FAILURE);
        }
        fclose(fp);
    }
    printf("#MD5:%s\n", actualHash);
}

void TestShellPrinter::handleImageFooter() const
{
}

void TestShellPrinter::handleTestFooter(bool dumpedAnything) const
{
    if (dumpedAnything)
        printf("#EOF\n");
}
