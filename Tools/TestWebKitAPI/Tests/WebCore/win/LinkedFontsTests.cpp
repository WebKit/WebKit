/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <WebCore/FontCache.h>

using namespace WebCore;

namespace WebCore {
void appendLinkedFonts(WCHAR* linkedFonts, unsigned length, Vector<String>* result);
}

namespace TestWebKitAPI {

// Test the appendLinkedFonts function with normal input data (string with pairs of font filenames and font names separated with \0).
TEST(WebCore, appendLinkedFontsTest)
{
    Vector<String> result;
    WCHAR linkedFonts[] = L"MICROSS.TTF,Microsoft Sans Serif,128,140\0MICROSS.TTF,Microsoft Sans Serif\0MSGOTHIC.TTC,MS UI Gothic\0MINGLIU.TTC,PMingLiU\0SIMSUN.TTC,SimSun";

    appendLinkedFonts(linkedFonts,  sizeof(linkedFonts) / sizeof(WCHAR), &result);
    ASSERT(result.size() == 5);
    ASSERT(result[0] == String(L"Microsoft Sans Serif,128,140"));
    ASSERT(result[1] == String(L"Microsoft Sans Serif"));
    ASSERT(result[2] == String(L"MS UI Gothic"));
    ASSERT(result[3] == String(L"PMingLiU"));
    ASSERT(result[4] == String(L"SimSun"));
}

// Test the appendLinkedFonts function when the input data does not contain the expected comma separator.
TEST(WebCore, appendLinkedFontsWithMissingCommaTest)
{
    Vector<String> result;
    WCHAR linkedFonts[] = L"MICROSS.TTF Microsoft Sans Serif";

    appendLinkedFonts(linkedFonts, sizeof(linkedFonts) / sizeof(WCHAR), &result);
    ASSERT(!result.size());
}

} // namespace TestWebKitAPI
