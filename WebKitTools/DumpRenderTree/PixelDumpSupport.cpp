/*
 * Copyright (C) 2009 Apple, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DumpRenderTree.h"
#include "LayoutTestController.h"
#include "PixelDumpSupport.h"
#include <wtf/Assertions.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(CG)
#include "PixelDumpSupportCG.h"
#elif PLATFORM(CAIRO)
#include "PixelDumpSupportCairo.h"
#endif

void dumpWebViewAsPixelsAndCompareWithExpected(const std::string& expectedHash)
{
    RefPtr<BitmapContext> context = createBitmapContextFromWebView(gLayoutTestController->testOnscreen(), gLayoutTestController->testRepaint(), gLayoutTestController->testRepaintSweepHorizontally(), gLayoutTestController->dumpSelectionRect());
    ASSERT(context);
    
    // Compute the hash of the bitmap context pixels
    char actualHash[33];
    computeMD5HashStringForBitmapContext(context.get(), actualHash);
    printf("\nActualHash: %s\n", actualHash);
    
    // Check the computed hash against the expected one and dump image on mismatch
    bool dumpImage = true;
    if (expectedHash.length() > 0) {
        ASSERT(expectedHash.length() == 32);
        
        printf("\nExpectedHash: %s\n", expectedHash.c_str());
        
        if (expectedHash == actualHash)     // FIXME: do case insensitive compare
            dumpImage = false;
    }
    
    if (dumpImage)
      dumpBitmap(context.get());
}

void printPNG(const unsigned char* data, const size_t dataLength)
{
    printf("Content-Type: %s\n", "image/png");
    printf("Content-Length: %lu\n", static_cast<unsigned long>(dataLength));

    const size_t bytesToWriteInOneChunk = 1 << 15;
    size_t dataRemainingToWrite = dataLength;
    while (dataRemainingToWrite) {
        size_t bytesToWriteInThisChunk = std::min(dataRemainingToWrite, bytesToWriteInOneChunk);
        size_t bytesWritten = fwrite(data, 1, bytesToWriteInThisChunk, stdout);
        if (bytesWritten != bytesToWriteInThisChunk)
            break;
        dataRemainingToWrite -= bytesWritten;
        data += bytesWritten;
    }
}
