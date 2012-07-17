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
#include <wtf/text/Base64.h>

TestEventPrinter::TestEventPrinter()
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

void TestEventPrinter::handleTextHeader() const
{
    printf("Content-Type: text/plain\n");
}

void TestEventPrinter::handleTextFooter() const
{
    printf("#EOF\n");
    fprintf(stderr, "#EOF\n");
}

void TestEventPrinter::handleAudioHeader() const
{
    printf("Content-Type: audio/wav\n");
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
#if OS(ANDROID)
        // On Android, the layout test driver needs to read the image data through 'adb shell' which can't
        // handle binary data properly. Need to encode the binary data into base64.
        // FIXME: extract this into a function so that we can also use it to output audio data. Will do when removing test_shell mode.
        Vector<char> base64;
        base64Encode(reinterpret_cast<const char*>(imageData), imageSize, base64, Base64InsertLFs);
        imageData = reinterpret_cast<const unsigned char*>(base64.data());
        imageSize = base64.size();
        printf("Content-Transfer-Encoding: base64\n");
#endif
        // Printf formatting for size_t on 32-bit, 64-bit, and on Windows is hard so just cast to an int.
        printf("Content-Length: %d\n", static_cast<int>(imageSize));
        if (fwrite(imageData, 1, imageSize, stdout) != imageSize) {
            fprintf(stderr, "Short write to stdout.\n");
            exit(1);
        }
    }
}

void TestEventPrinter::handleTestFooter(bool) const
{
    printf("#EOF\n");
}
