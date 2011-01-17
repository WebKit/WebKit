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

#include <gtest/gtest.h>

#include "PlatformString.h"
#include "UniscribeHelper.h"

using namespace WebCore;

namespace {

class UniscribeTest : public testing::Test {
public:
    UniscribeTest()
    {
    }

    // Returns an HFONT with the given name. The caller does not have to free
    // this, it will be automatically freed at the end of the test. Returns 0
    // on failure. On success, the
    HFONT MakeFont(const wchar_t* fontName, SCRIPT_CACHE** cache)
    {
        LOGFONT lf;
        memset(&lf, 0, sizeof(LOGFONT));
        lf.lfHeight = 20;
        wcscpy_s(lf.lfFaceName, fontName);

        HFONT hfont = CreateFontIndirect(&lf);
        if (!hfont)
            return 0;

        *cache = new SCRIPT_CACHE;
        **cache = 0;
        createdFonts.append(std::make_pair(hfont, *cache));
        return hfont;
    }

protected:
    // Default font properties structure for tests to use.
    SCRIPT_FONTPROPERTIES properties;

private:
    virtual void SetUp()
    {
        memset(&properties, 0, sizeof(SCRIPT_FONTPROPERTIES));
        properties.cBytes = sizeof(SCRIPT_FONTPROPERTIES);
        properties.wgBlank = ' ';
        properties.wgDefault = '?'; // Used when the char is not in the font.
        properties.wgInvalid = '#'; // Used for invalid characters.
    }

    virtual void TearDown()
    {
        // Free any allocated fonts.
        for (size_t i = 0; i < createdFonts.size(); i++) {
            DeleteObject(createdFonts[i].first);
            ScriptFreeCache(createdFonts[i].second);
            delete createdFonts[i].second;
        }
        createdFonts.clear();
    }

    // Tracks allocated fonts so we can delete them at the end of the test.
    // The script cache pointer is heap allocated and must be freed.
    Vector< std::pair<HFONT, SCRIPT_CACHE*> > createdFonts;
};

} // namespace

// This test tests giving Uniscribe a very large buffer, which will cause a
// failure.
TEST_F(UniscribeTest, TooBig)
{
    // Make a large string with an e with a zillion combining accents.
    String input(L"e");
    for (int i = 0; i < 100000; i++)
        input.append(static_cast<UChar>(0x301)); // Combining acute accent.

    SCRIPT_CACHE* scriptCache;
    HFONT hfont = MakeFont(L"Times New Roman", &scriptCache);
    ASSERT_TRUE(hfont);

    // Test a long string without the normal length protection we have. This
    // will cause shaping to fail.
    {
        UniscribeHelper uniscribe(
            input.characters(), static_cast<int>(input.length()),
            false, hfont, scriptCache, &properties);
        uniscribe.initWithOptionalLengthProtection(false);

        // There should be one shaping entry, with nothing in it.
        ASSERT_EQ(1, uniscribe.m_shapes.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_glyphs.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_logs.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_visualAttributes.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_advance.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_offsets.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_justify.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_abc.abcA);
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_abc.abcB);
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_abc.abcC);

        // The sizes of the other stuff should match the shaping entry.
        EXPECT_EQ(1, uniscribe.m_runs.size());
        EXPECT_EQ(1, uniscribe.m_screenOrder.size());

        // Check that the various querying functions handle the empty case
        // properly.
        EXPECT_EQ(0, uniscribe.width());
        EXPECT_EQ(0, uniscribe.firstGlyphForCharacter(0));
        EXPECT_EQ(0, uniscribe.firstGlyphForCharacter(1000));
        EXPECT_EQ(0, uniscribe.xToCharacter(0));
        EXPECT_EQ(0, uniscribe.xToCharacter(1000));
    }

    // Now test the very large string and make sure it is handled properly by
    // the length protection.
    {
        UniscribeHelper uniscribe(
            input.characters(), static_cast<int>(input.length()),
            false, hfont, scriptCache, &properties);
        uniscribe.initWithOptionalLengthProtection(true);

        // There should be 0 runs and shapes.
        EXPECT_EQ(0, uniscribe.m_runs.size());
        EXPECT_EQ(0, uniscribe.m_shapes.size());
        EXPECT_EQ(0, uniscribe.m_screenOrder.size());

        EXPECT_EQ(0, uniscribe.width());
        EXPECT_EQ(0, uniscribe.firstGlyphForCharacter(0));
        EXPECT_EQ(0, uniscribe.firstGlyphForCharacter(1000));
        EXPECT_EQ(0, uniscribe.xToCharacter(0));
        EXPECT_EQ(0, uniscribe.xToCharacter(1000));
    }
}
