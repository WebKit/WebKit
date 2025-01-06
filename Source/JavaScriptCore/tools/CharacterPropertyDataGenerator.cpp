/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 The Chromium Authors. All rights reserved.
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

#include "config.h"
#include "CharacterPropertyDataGenerator.h"

#include <wtf/BitSet.h>
#include <wtf/DataLog.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/TextBreakIterator.h>

namespace JSC {

//
// Generate a line break pair table in `Source/WebCore/rendering/BreakLines.cpp`.
//
// See [UAX14](https://unicode.org/reports/tr14/).
//
class LineBreakData {
public:
    static constexpr UChar minChar = '!';
    static constexpr UChar maxChar = 0xFF;
    static constexpr unsigned numChars = maxChar - minChar + 1;
    static constexpr unsigned numCharsRoundUp8 = (numChars + 7) / 8 * 8;

    LineBreakData() = default;

    static void generate()
    {
        LineBreakData data;
        data.fill();
        data.fillASCII();
        data.dump();
    }

private:
    void fill()
    {
        for (UChar ch = minChar; ch <= maxChar; ++ch) {
            for (UChar chNext = minChar; chNext <= maxChar; ++chNext) {
                auto string = makeString(ch, chNext);
                CachedTextBreakIterator iterator(string, { }, WTF::TextBreakIterator::LineMode { WTF::TextBreakIterator::LineMode::Behavior::Default }, AtomString("en"_str));
                setPairValue(ch, chNext, iterator.isBoundary(1));
            }
        }
    }

    // Line breaking table for printable ASCII characters. Line breaking
    // opportunities in this table are as below:
    // - before opening punctuations such as '(', '<', '[', '{' after certain
    //   characters (compatible with Firefox 3.6);
    // - after '-' and '?' (backward-compatible, and compatible with Internet
    //   Explorer).
    // Please refer to <https://bugs.webkit.org/show_bug.cgi?id=37698> for line
    // breaking matrixes of different browsers and the ICU standard.
    void fillASCII()
    {
#define ALL_CHAR '!', 0x7F
        setPairValue(ALL_CHAR, ALL_CHAR, false);
        setPairValue(ALL_CHAR, '(', '(', true);
        setPairValue(ALL_CHAR, '<', '<', true);
        setPairValue(ALL_CHAR, '[', '[', true);
        setPairValue(ALL_CHAR, '{', '{', true);
        setPairValue('-', '-', ALL_CHAR, true);
        setPairValue('?', '?', ALL_CHAR, true);
        setPairValue(ALL_CHAR, '!', '!', false);
        setPairValue('-', '-', '!', '!', true);
        setPairValue('?', '?', '"', '"', false);
        setPairValue('?', '?', '\'', '\'', false);
        setPairValue(ALL_CHAR, ')', ')', false);
        setPairValue('-', '-', ')', ')', true);
        setPairValue(ALL_CHAR, ',', ',', false);
        setPairValue(ALL_CHAR, '.', '.', false);
        setPairValue(ALL_CHAR, '/', '/', false);
        setPairValue('-', '-', '/', '/', true);
        setPairValue('-', '-', '0', '9', false); // Note: Between '-' and '[0-9]' is hard-coded in `ShouldBreakFast()`.
        setPairValue(ALL_CHAR, ':', ':', false);
        setPairValue('-', '-', ':', ':', true);
        setPairValue(ALL_CHAR, ';', ';', false);
        setPairValue('-', '-', ';', ';', true);
        setPairValue(ALL_CHAR, '?', '?', false);
        setPairValue('-', '-', '?', '?', true);
        setPairValue(ALL_CHAR, ']', ']', false);
        setPairValue('-', '-', ']', ']', true);
        setPairValue(ALL_CHAR, '}', '}', false);
        setPairValue('-', '-', '}', '}', true);
        setPairValue('$', '$', ALL_CHAR, false);
        setPairValue('\'', '\'', ALL_CHAR, false);
        setPairValue('(', '(', ALL_CHAR, false);
        setPairValue('/', '/', ALL_CHAR, false);
        setPairValue('0', '9', ALL_CHAR, false);
        setPairValue('<', '<', ALL_CHAR, false);
        setPairValue('@', '@', ALL_CHAR, false);
        setPairValue('A', 'Z', ALL_CHAR, false);
        setPairValue('[', '[', ALL_CHAR, false);
        setPairValue('^', '`', ALL_CHAR, false);
        setPairValue('a', 'z', ALL_CHAR, false);
        setPairValue('{', '{', ALL_CHAR, false);
        setPairValue(0x7F, 0x7F, ALL_CHAR, false);
#undef ALL_CHAR
    }

    void dump()
    {

        dataLogLn("/*");
        dataLogLn(" * Copyright (C) 2005-2024 Apple Inc. All rights reserved.");
        dataLogLn(" * Copyright (C) 2011-2024 Google Inc. All rights reserved.");
        dataLogLn(" *");
        dataLogLn(" * Redistribution and use in source and binary forms, with or without");
        dataLogLn(" * modification, are permitted provided that the following conditions");
        dataLogLn(" * are met:");
        dataLogLn(" * 1. Redistributions of source code must retain the above copyright");
        dataLogLn(" *    notice, this list of conditions and the following disclaimer.");
        dataLogLn(" * 2. Redistributions in binary form must reproduce the above copyright");
        dataLogLn(" *    notice, this list of conditions and the following disclaimer in the");
        dataLogLn(" *    documentation and/or other materials provided with the distribution.");
        dataLogLn(" *");
        dataLogLn(" * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''");
        dataLogLn(" * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,");
        dataLogLn(" * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR");
        dataLogLn(" * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS");
        dataLogLn(" * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR");
        dataLogLn(" * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF");
        dataLogLn(" * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS");
        dataLogLn(" * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN");
        dataLogLn(" * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)");
        dataLogLn(" * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF");
        dataLogLn(" * THE POSSIBILITY OF SUCH DAMAGE.");
        dataLogLn(" */");
        dataLogLn("");
        dataLogLn("#include \"config.h\"");
        dataLogLn("#include \"BreakLines.h\"");
        dataLogLn("");
        dataLogLn("#include <wtf/ASCIICType.h>");
        dataLogLn("#include <wtf/StdLibExtras.h>");
        dataLogLn("#include <wtf/unicode/CharacterNames.h>");
        dataLogLn("");
        dataLogLn("// This file is generated from JSC's $vm.dumpLineBreakData()");
        dataLogLn("");
        dataLogLn("namespace WebCore {");
        dataLogLn("");

        // Define macros.
        dataLogLn("// Pack 8 bits into one byte");
        dataLogLn("#define B(a, b, c, d, e, f, g, h) \\");
        dataLogLn("    ((a) | ((b) << 1) | ((c) << 2) | ((d) << 3) | ((e) << 4) | ((f) << 5) | ((g) << 6) | ((h) << 7))");
        dataLogLn("");

        dataLogLn("const uint8_t BreakLines::LineBreakTable::breakTable[", numChars, "][", numCharsRoundUp8 / 8, "] = {");

        // Print the column comment.
        dataLog("           /*");
        for (UChar ch = minChar; ch <= maxChar; ++ch) {
            if (ch != minChar && (ch - minChar) % 8 == 0)
                dataLog("   ");
            dataLogF(ch < 0x7F ? " %c" : "%02X", ch);
        }
        dataLogLn(" */");

        // Print the data array.
        for (unsigned y = 0; y < numChars; ++y) {
            const UChar ch = y + minChar;
            dataLogF("/* %02X %c */ {B(", ch, ch < 0x7F ? ch : ' ');
            const char* prefix = "";
            for (unsigned x = 0; x < numCharsRoundUp8; ++x) {
                dataLogF("%s%u", prefix, static_cast<unsigned>(m_pair[y].get(x)));
                prefix = (x % 8 == 7) ? "),B(" : ",";
            }
            dataLogLn(")},");
        }
        dataLogLn("};");
        dataLogLn("");
        dataLogLn("#undef B");
        dataLogLn("");
        dataLogLn("} // namespace WebCore");
    }

    void setPairValue(UChar ch1Min, UChar ch1Max, UChar ch2Min, UChar ch2Max, bool value)
    {
        for (UChar ch1 = ch1Min; ch1 <= ch1Max; ++ch1) {
            for (UChar ch2 = ch2Min; ch2 <= ch2Max; ++ch2)
                setPairValue(ch1, ch2, value);
        }
    }

    // Set the breakability between `ch1` and `ch2`.
    void setPairValue(UChar ch1, UChar ch2, bool value)
    {
        RELEASE_ASSERT(ch1 >= minChar);
        RELEASE_ASSERT(ch1 <= maxChar);
        RELEASE_ASSERT(ch2 >= minChar);
        RELEASE_ASSERT(ch2 <= maxChar);
        m_pair[ch1 - minChar].set(ch2 - minChar, value);
    }

    std::array<WTF::BitSet<numCharsRoundUp8>, numChars> m_pair { };
};

void dumpLineBreakData()
{
    LineBreakData::generate();
}

} // namespace JSC
