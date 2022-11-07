/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

#if defined(NDEBUG)
// Compile the test but disable it for non-Debug builds.
namespace WTF::Detail  {
std::atomic<int> wtfStringCopyCount;
};
#define MAYBE_COPY_COUNT_TEST(test) DISABLED_##test
#else
#define MAYBE_COPY_COUNT_TEST(test) test
#endif

namespace TestWebKitAPI {

#define EXPECT_N_WTF_STRING_COPIES(count, expr) \
    do { \
        WTF::Detail::wtfStringCopyCount = 0; \
        String __testString = expr; \
        (void)__testString; \
        EXPECT_EQ(count, WTF::Detail::wtfStringCopyCount) << #expr; \
    } while (false)

TEST(WTF, MAYBE_COPY_COUNT_TEST(StringOperators))
{
    String string("String"_s);
    AtomString atomString("AtomString"_s);
    ASCIILiteral literal { "ASCIILiteral"_s };

    String stringViewBacking { "StringView"_s };
    StringView stringView { stringViewBacking };

    WTF::Detail::wtfStringCopyCount = 0;

    EXPECT_N_WTF_STRING_COPIES(2, string + string);
    EXPECT_N_WTF_STRING_COPIES(2, string + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, atomString + string);
    EXPECT_N_WTF_STRING_COPIES(2, atomString + atomString);
    EXPECT_N_WTF_STRING_COPIES(1, stringView + string);
    EXPECT_N_WTF_STRING_COPIES(1, string + stringView);
    EXPECT_N_WTF_STRING_COPIES(1, stringView + atomString);
    EXPECT_N_WTF_STRING_COPIES(1, atomString + stringView);

    EXPECT_N_WTF_STRING_COPIES(1, "C string" + string);
    EXPECT_N_WTF_STRING_COPIES(1, string + "C string");
    EXPECT_N_WTF_STRING_COPIES(1, "C string" + atomString);
    EXPECT_N_WTF_STRING_COPIES(1, atomString + "C string");
    EXPECT_N_WTF_STRING_COPIES(0, "C string" + stringView);
    EXPECT_N_WTF_STRING_COPIES(0, stringView + "C string");

    EXPECT_N_WTF_STRING_COPIES(1, literal + string);
    EXPECT_N_WTF_STRING_COPIES(1, string + literal);
    EXPECT_N_WTF_STRING_COPIES(1, literal + atomString);
    EXPECT_N_WTF_STRING_COPIES(1, atomString + literal);
    EXPECT_N_WTF_STRING_COPIES(0, literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(0, stringView + literal);

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + "C string" + string);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + "C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + ("C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, string + "C string" + string + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, string + ("C string" + string + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (string + "C string") + (string + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + string + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (string + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + (literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, string + literal + string + literal);
    EXPECT_N_WTF_STRING_COPIES(2, string + (literal + string + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (string + literal) + (string + literal));

    EXPECT_N_WTF_STRING_COPIES(2, literal + string + "C string" + string);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (string + "C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + ("C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + (literal + string));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomString + "C string" + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomString + "C string" + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomString) + ("C string" + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomString + literal + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomString + literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomString) + (literal + atomString));

    EXPECT_N_WTF_STRING_COPIES(0, literal + stringView + "C string" + stringView);
    EXPECT_N_WTF_STRING_COPIES(0, literal + (stringView + "C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(0, (literal + stringView) + ("C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(0, "C string" + stringView + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(0, "C string" + (stringView + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(0, ("C string" + stringView) + (literal + stringView));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomString + "C string" + string + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomString + "C string" + string + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomString) + ("C string" + string) + (literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomString + literal + string + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomString + literal + string + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomString) + (literal + string) + (literal + stringView));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomString + "C string" + stringView + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomString + "C string" + stringView + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomString) + ("C string" + stringView) + (literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomString + literal + stringView + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomString + literal + stringView + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomString) + (literal + stringView) + (literal + string));

    EXPECT_N_WTF_STRING_COPIES(2, literal + string + "C string" + atomString + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (string + "C string" + atomString + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + ("C string" + atomString) + (literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + literal + atomString + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + literal + atomString + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + (literal + atomString) + (literal + stringView));

    EXPECT_N_WTF_STRING_COPIES(2, literal + string + "C string" + stringView + literal + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (string + "C string" + stringView + literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + ("C string" + stringView) + (literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + literal + stringView + literal + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + literal + stringView + literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + (literal + stringView) + (literal + atomString));

    EXPECT_N_WTF_STRING_COPIES(2, literal + stringView + "C string" + atomString + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (stringView + "C string" + atomString + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + stringView) + ("C string" + atomString) + (literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + stringView + literal + atomString + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (stringView + literal + atomString + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + stringView) + (literal + atomString) + (literal + string));

    EXPECT_N_WTF_STRING_COPIES(2, literal + stringView + "C string" + string + literal + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (stringView + "C string" + string + literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + stringView) + ("C string" + string) + (literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + stringView + literal + string + literal + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (stringView + literal + string + literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + stringView) + (literal + string) + (literal + atomString));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomString + "C string" + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomString + "C string" + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomString) + ("C string" + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, atomString + "C string" + atomString + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, atomString + ("C string" + atomString + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (atomString + "C string") + (atomString + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomString + literal + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomString + literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomString) + (literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, atomString + literal + atomString + literal);
    EXPECT_N_WTF_STRING_COPIES(2, atomString + (literal + atomString + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (atomString + literal) + (atomString + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + "C string" + atomString + "C string" + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + "C string" + atomString + "C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + ("C string" + atomString) + ("C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, string + "C string" + atomString + "C string" + stringView + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, string + ("C string" + atomString + "C string" + stringView + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (string + "C string") + (atomString + "C string") + (stringView + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + string + literal + atomString + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (string + literal + atomString + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + (literal + atomString) + (literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, string + literal + atomString + literal + stringView + literal);
    EXPECT_N_WTF_STRING_COPIES(2, string + (literal + atomString + literal + stringView + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (string + literal) + (atomString + literal) + (stringView + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + "C string" + stringView + "C string" + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + "C string" + stringView + "C string" + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + ("C string" + stringView) + ("C string" + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, string + "C string" + stringView + "C string" + atomString + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, string + ("C string" + stringView + "C string" + atomString + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (string + "C string") + (stringView + "C string") + (atomString + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + string + literal + stringView + literal + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (string + literal + stringView + literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + (literal + stringView) + (literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, string + literal + stringView + literal + atomString + literal);
    EXPECT_N_WTF_STRING_COPIES(2, string + (literal + stringView + literal + atomString + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (string + literal) + (stringView + literal) + (atomString + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomString + "C string" + string + "C string" + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomString + "C string" + string + "C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomString) + ("C string" + string) + ("C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, atomString + "C string" + string + "C string" + stringView + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, atomString + ("C string" + string + "C string" + stringView + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (atomString + "C string") + (string + "C string") + (stringView + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomString + literal + string + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomString + literal + string + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomString) + (literal + string) + (literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, atomString + literal + string + literal + stringView + literal);
    EXPECT_N_WTF_STRING_COPIES(2, atomString + (literal + string + literal + stringView + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (atomString + literal) + (string + literal) + (stringView + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomString + "C string" + stringView + "C string" + string);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomString + "C string" + stringView + "C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomString) + ("C string" + stringView) + ("C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, atomString + "C string" + stringView + "C string" + string + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, atomString + ("C string" + stringView + "C string" + string + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (atomString + "C string") + (stringView + "C string") + (string + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomString + literal + stringView + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomString + literal + stringView + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomString) + (literal + stringView) + (literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, atomString + literal + stringView + literal + string + literal);
    EXPECT_N_WTF_STRING_COPIES(2, atomString + (literal + stringView + literal + string + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (atomString + literal) + (stringView + literal) + (string + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + stringView + "C string" + atomString + "C string" + string);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (stringView + "C string" + atomString + "C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + stringView) + ("C string" + atomString) + ("C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, stringView + "C string" + atomString + "C string" + string + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, stringView + ("C string" + atomString + "C string" + string + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (stringView + "C string") + (atomString + "C string") + (string + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + stringView + literal + atomString + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (stringView + literal + atomString + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + stringView) + (literal + atomString) + (literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, stringView + literal + atomString + literal + string + literal);
    EXPECT_N_WTF_STRING_COPIES(2, stringView + (literal + atomString + literal + string + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (stringView + literal) + (atomString + literal) + (string + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + stringView + "C string" + string + "C string" + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (stringView + "C string" + string + "C string" + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + stringView) + ("C string" + string) + ("C string" + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, stringView + "C string" + string + "C string" + atomString + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, stringView + ("C string" + string + "C string" + atomString + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (stringView + "C string") + (string + "C string") + (atomString + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + stringView + literal + string + literal + atomString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (stringView + literal + string + literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + stringView) + (literal + string) + (literal + atomString));
    EXPECT_N_WTF_STRING_COPIES(2, stringView + literal + string + literal + atomString + literal);
    EXPECT_N_WTF_STRING_COPIES(2, stringView + (literal + string + literal + atomString + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (stringView + literal) + (string + literal) + (atomString + literal));
}

TEST(WTF, ConcatenateCharacterArrayAndEmptyString)
{
    String emptyString;
    EXPECT_EQ(static_cast<unsigned>(0), emptyString.length());

    UChar ucharArray[] = { 't', 'e', 's', 't', '\0' };
    String concatenation16 = ucharArray + emptyString;
    ASSERT_EQ(static_cast<unsigned>(4), concatenation16.length());
    ASSERT_TRUE(concatenation16 == String(ucharArray, 4));

    LChar lcharArray[] = { 't', 'e', 's', 't' };
    String concatenation8 = String(lcharArray, 4) + emptyString;
    ASSERT_EQ(static_cast<unsigned>(4), concatenation8.length());
    ASSERT_TRUE(concatenation8 == String(lcharArray, 4));
}

} // namespace TestWebKitAPI
