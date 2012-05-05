/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "LocaleICU.h"
#include <gtest/gtest.h>
#include <wtf/PassOwnPtr.h>

using namespace WebCore;

void testNumberIsReversible(const char* localeString, const char* original, const char* shouldHave = 0)
{
    OwnPtr<ICULocale> locale = ICULocale::create(localeString);
    String localized = locale->convertToLocalizedNumber(original);
    if (shouldHave)
        EXPECT_TRUE(localized.contains(shouldHave));
    String converted = locale->convertFromLocalizedNumber(localized);
    EXPECT_EQ(original, converted);
}

void testNumbers(const char* localeString)
{
    testNumberIsReversible(localeString, "123456789012345678901234567890");
    testNumberIsReversible(localeString, "-123.456");
    testNumberIsReversible(localeString, ".456");
    testNumberIsReversible(localeString, "-0.456");
}

TEST(LocalizedNumberICUTest, Reversible)
{
    testNumberIsReversible("en_US", "123456789012345678901234567890");
    testNumberIsReversible("en_US", "-123.456", ".");
    testNumberIsReversible("en_US", ".456", ".");
    testNumberIsReversible("en_US", "-0.456", ".");

    testNumberIsReversible("fr", "123456789012345678901234567890");
    testNumberIsReversible("fr", "-123.456", ",");
    testNumberIsReversible("fr", ".456", ",");
    testNumberIsReversible("fr", "-0.456", ",");

    // Persian locale has a negative prefix and a negative suffix.
    testNumbers("fa");

    // Test some of major locales.
    testNumbers("ar");
    testNumbers("de_DE");
    testNumbers("es_ES");
    testNumbers("ja_JP");
    testNumbers("ko_KR");
    testNumbers("zh_CN");
    testNumbers("zh_HK");
    testNumbers("zh_TW");
}
