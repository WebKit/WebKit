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

#define WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING() (++wtfStringCopyCount)

static int wtfStringCopyCount;

#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

#define EXPECT_N_WTF_STRING_COPIES(count, expr) \
    do { \
        wtfStringCopyCount = 0; \
        String __testString = expr; \
        (void)__testString; \
        EXPECT_EQ(count, wtfStringCopyCount) << #expr; \
    } while (false)

TEST(WTF, StringOperators)
{
    String string("String");
    AtomicString atomicString("AtomicString");
    ASCIILiteral literal { "ASCIILiteral"_s };

    String stringViewBacking { "StringView" };
    StringView stringView { stringViewBacking };

    EXPECT_EQ(0, wtfStringCopyCount);

    EXPECT_N_WTF_STRING_COPIES(2, string + string);
    EXPECT_N_WTF_STRING_COPIES(2, string + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + string);
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + atomicString);
    EXPECT_N_WTF_STRING_COPIES(1, stringView + string);
    EXPECT_N_WTF_STRING_COPIES(1, string + stringView);
    EXPECT_N_WTF_STRING_COPIES(1, stringView + atomicString);
    EXPECT_N_WTF_STRING_COPIES(1, atomicString + stringView);

    EXPECT_N_WTF_STRING_COPIES(1, "C string" + string);
    EXPECT_N_WTF_STRING_COPIES(1, string + "C string");
    EXPECT_N_WTF_STRING_COPIES(1, "C string" + atomicString);
    EXPECT_N_WTF_STRING_COPIES(1, atomicString + "C string");
    EXPECT_N_WTF_STRING_COPIES(0, "C string" + stringView);
    EXPECT_N_WTF_STRING_COPIES(0, stringView + "C string");

    EXPECT_N_WTF_STRING_COPIES(1, literal + string);
    EXPECT_N_WTF_STRING_COPIES(1, string + literal);
    EXPECT_N_WTF_STRING_COPIES(1, literal + atomicString);
    EXPECT_N_WTF_STRING_COPIES(1, atomicString + literal);
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

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomicString + "C string" + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomicString + "C string" + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomicString) + ("C string" + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomicString + literal + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomicString + literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomicString) + (literal + atomicString));

    EXPECT_N_WTF_STRING_COPIES(0, literal + stringView + "C string" + stringView);
    EXPECT_N_WTF_STRING_COPIES(0, literal + (stringView + "C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(0, (literal + stringView) + ("C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(0, "C string" + stringView + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(0, "C string" + (stringView + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(0, ("C string" + stringView) + (literal + stringView));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomicString + "C string" + string + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomicString + "C string" + string + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomicString) + ("C string" + string) + (literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomicString + literal + string + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomicString + literal + string + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomicString) + (literal + string) + (literal + stringView));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomicString + "C string" + stringView + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomicString + "C string" + stringView + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomicString) + ("C string" + stringView) + (literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomicString + literal + stringView + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomicString + literal + stringView + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomicString) + (literal + stringView) + (literal + string));

    EXPECT_N_WTF_STRING_COPIES(2, literal + string + "C string" + atomicString + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (string + "C string" + atomicString + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + ("C string" + atomicString) + (literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + literal + atomicString + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + literal + atomicString + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + (literal + atomicString) + (literal + stringView));

    EXPECT_N_WTF_STRING_COPIES(2, literal + string + "C string" + stringView + literal + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (string + "C string" + stringView + literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + ("C string" + stringView) + (literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + literal + stringView + literal + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + literal + stringView + literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + (literal + stringView) + (literal + atomicString));

    EXPECT_N_WTF_STRING_COPIES(2, literal + stringView + "C string" + atomicString + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (stringView + "C string" + atomicString + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + stringView) + ("C string" + atomicString) + (literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + stringView + literal + atomicString + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (stringView + literal + atomicString + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + stringView) + (literal + atomicString) + (literal + string));

    EXPECT_N_WTF_STRING_COPIES(2, literal + stringView + "C string" + string + literal + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (stringView + "C string" + string + literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + stringView) + ("C string" + string) + (literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + stringView + literal + string + literal + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (stringView + literal + string + literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + stringView) + (literal + string) + (literal + atomicString));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomicString + "C string" + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomicString + "C string" + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomicString) + ("C string" + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + "C string" + atomicString + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + ("C string" + atomicString + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (atomicString + "C string") + (atomicString + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomicString + literal + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomicString + literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomicString) + (literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + literal + atomicString + literal);
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + (literal + atomicString + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (atomicString + literal) + (atomicString + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + "C string" + atomicString + "C string" + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + "C string" + atomicString + "C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + ("C string" + atomicString) + ("C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, string + "C string" + atomicString + "C string" + stringView + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, string + ("C string" + atomicString + "C string" + stringView + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (string + "C string") + (atomicString + "C string") + (stringView + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + string + literal + atomicString + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (string + literal + atomicString + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + (literal + atomicString) + (literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, string + literal + atomicString + literal + stringView + literal);
    EXPECT_N_WTF_STRING_COPIES(2, string + (literal + atomicString + literal + stringView + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (string + literal) + (atomicString + literal) + (stringView + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + string + "C string" + stringView + "C string" + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (string + "C string" + stringView + "C string" + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + string) + ("C string" + stringView) + ("C string" + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, string + "C string" + stringView + "C string" + atomicString + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, string + ("C string" + stringView + "C string" + atomicString + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (string + "C string") + (stringView + "C string") + (atomicString + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + string + literal + stringView + literal + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (string + literal + stringView + literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + string) + (literal + stringView) + (literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, string + literal + stringView + literal + atomicString + literal);
    EXPECT_N_WTF_STRING_COPIES(2, string + (literal + stringView + literal + atomicString + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (string + literal) + (stringView + literal) + (atomicString + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomicString + "C string" + string + "C string" + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomicString + "C string" + string + "C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomicString) + ("C string" + string) + ("C string" + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + "C string" + string + "C string" + stringView + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + ("C string" + string + "C string" + stringView + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (atomicString + "C string") + (string + "C string") + (stringView + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomicString + literal + string + literal + stringView);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomicString + literal + string + literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomicString) + (literal + string) + (literal + stringView));
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + literal + string + literal + stringView + literal);
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + (literal + string + literal + stringView + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (atomicString + literal) + (string + literal) + (stringView + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + atomicString + "C string" + stringView + "C string" + string);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (atomicString + "C string" + stringView + "C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + atomicString) + ("C string" + stringView) + ("C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + "C string" + stringView + "C string" + string + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + ("C string" + stringView + "C string" + string + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (atomicString + "C string") + (stringView + "C string") + (string + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + atomicString + literal + stringView + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (atomicString + literal + stringView + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + atomicString) + (literal + stringView) + (literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + literal + stringView + literal + string + literal);
    EXPECT_N_WTF_STRING_COPIES(2, atomicString + (literal + stringView + literal + string + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (atomicString + literal) + (stringView + literal) + (string + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + stringView + "C string" + atomicString + "C string" + string);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (stringView + "C string" + atomicString + "C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + stringView) + ("C string" + atomicString) + ("C string" + string));
    EXPECT_N_WTF_STRING_COPIES(2, stringView + "C string" + atomicString + "C string" + string + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, stringView + ("C string" + atomicString + "C string" + string + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (stringView + "C string") + (atomicString + "C string") + (string + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + stringView + literal + atomicString + literal + string);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (stringView + literal + atomicString + literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + stringView) + (literal + atomicString) + (literal + string));
    EXPECT_N_WTF_STRING_COPIES(2, stringView + literal + atomicString + literal + string + literal);
    EXPECT_N_WTF_STRING_COPIES(2, stringView + (literal + atomicString + literal + string + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (stringView + literal) + (atomicString + literal) + (string + literal));

    EXPECT_N_WTF_STRING_COPIES(2, "C string" + stringView + "C string" + string + "C string" + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, "C string" + (stringView + "C string" + string + "C string" + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, ("C string" + stringView) + ("C string" + string) + ("C string" + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, stringView + "C string" + string + "C string" + atomicString + "C string");
    EXPECT_N_WTF_STRING_COPIES(2, stringView + ("C string" + string + "C string" + atomicString + "C string"));
    EXPECT_N_WTF_STRING_COPIES(2, (stringView + "C string") + (string + "C string") + (atomicString + "C string"));

    EXPECT_N_WTF_STRING_COPIES(2, literal + stringView + literal + string + literal + atomicString);
    EXPECT_N_WTF_STRING_COPIES(2, literal + (stringView + literal + string + literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, (literal + stringView) + (literal + string) + (literal + atomicString));
    EXPECT_N_WTF_STRING_COPIES(2, stringView + literal + string + literal + atomicString + literal);
    EXPECT_N_WTF_STRING_COPIES(2, stringView + (literal + string + literal + atomicString + literal));
    EXPECT_N_WTF_STRING_COPIES(2, (stringView + literal) + (string + literal) + (atomicString + literal));
}

TEST(WTF, ConcatenateCharacterArrayAndEmptyString)
{
    String emptyString;
    EXPECT_EQ(static_cast<unsigned>(0), emptyString.length());

    UChar ucharArray[] = { 't', 'e', 's', 't', '\0' };
    String concatenation16 = ucharArray + emptyString;
    ASSERT_EQ(static_cast<unsigned>(4), concatenation16.length());
    ASSERT_TRUE(concatenation16 == String(ucharArray));

    LChar lcharArray[] = { 't', 'e', 's', 't', '\0' };
    String concatenation8 = lcharArray + emptyString;
    ASSERT_EQ(static_cast<unsigned>(4), concatenation8.length());
    ASSERT_TRUE(concatenation8 == String(lcharArray));
}

} // namespace TestWebKitAPI
