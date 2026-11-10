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
void appendLinkedFonts(WCHAR* linkedFonts, unsigned length, Vector<RefPtr<StringImpl>>* result);
}

namespace TestWebKitAPI {

#if USE(CAIRO)
// Test the appendLinkedFonts function with normal input data (string with pairs of font filenames and font names separated with \0).
TEST(WebCore, appendLinkedFontsTest)
{
    Vector<RefPtr<StringImpl>> result;
    WCHAR linkedFonts[] = L"MICROSS.TTF,Microsoft Sans Serif,128,140\0MICROSS.TTF,Microsoft Sans Serif\0MSGOTHIC.TTC,MS UI Gothic\0MINGLIU.TTC,PMingLiU\0SIMSUN.TTC,SimSun";

    appendLinkedFonts(linkedFonts,  sizeof(linkedFonts) / sizeof(WCHAR), &result);
    ASSERT(result.size() == 5);
    ASSERT(equal(result[0], "Microsoft Sans Serif,128,140"_s));
    ASSERT(equal(result[1], "Microsoft Sans Serif"_s));
    ASSERT(equal(result[2], "MS UI Gothic"_s));
    ASSERT(equal(result[3], "PMingLiU"_s));
    ASSERT(equal(result[4], "SimSun"_s));
}

// Test the appendLinkedFonts function when the input data does not contain the expected comma separator.
TEST(WebCore, appendLinkedFontsWithMissingCommaTest)
{
    Vector<RefPtr<StringImpl>> result;
    WCHAR linkedFonts[] = L"MICROSS.TTF Microsoft Sans Serif";

    appendLinkedFonts(linkedFonts, sizeof(linkedFonts) / sizeof(WCHAR), &result);
    ASSERT(!result.size());
}
#endif

} // namespace TestWebKitAPI
