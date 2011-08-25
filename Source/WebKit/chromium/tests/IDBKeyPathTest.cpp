/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "IDBKeyPath.h"

#include <gtest/gtest.h>
#include <wtf/Vector.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;

namespace {

void checkKeyPath(const String& keyPath, const Vector<String>& expected, int parserError)
{
    IDBKeyPathParseError error;
    Vector<String> keyPathElements;
    IDBParseKeyPath(keyPath, keyPathElements, error);
    ASSERT_EQ(parserError, error);
    if (error != IDBKeyPathParseErrorNone)
        return;
    ASSERT_EQ(expected.size(), keyPathElements.size());
    for (size_t i = 0; i < expected.size(); ++i)
        ASSERT_TRUE(expected[i] == keyPathElements[i]) << i;
}

TEST(IDBKeyPathTest, ValidKeyPath0)
{
    Vector<String> expected;
    String keyPath("");
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, ValidKeyPath1)
{
    Vector<String> expected;
    String keyPath("foo");
    expected.append(String("foo"));
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, ValidKeyPath2)
{
    Vector<String> expected;
    String keyPath("foo.bar.baz");
    expected.append(String("foo"));
    expected.append(String("bar"));
    expected.append(String("baz"));
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, InvalidKeyPath0)
{
    Vector<String> expected;
    String keyPath(" ");
    checkKeyPath(keyPath, expected, 1);
}

TEST(IDBKeyPathTest, InvalidKeyPath1)
{
    Vector<String> expected;
    String keyPath("+foo.bar.baz");
    checkKeyPath(keyPath, expected, 1);
}

TEST(IDBKeyPathTest, InvalidKeyPath2)
{
    Vector<String> expected;
    String keyPath("foo bar baz");
    expected.append(String("foo"));
    checkKeyPath(keyPath, expected, 2);
}

TEST(IDBKeyPathTest, InvalidKeyPath3)
{
    Vector<String> expected;
    String keyPath("foo .bar .baz");
    expected.append(String("foo"));
    checkKeyPath(keyPath, expected, 2);
}

TEST(IDBKeyPathTest, InvalidKeyPath4)
{
    Vector<String> expected;
    String keyPath("foo. bar. baz");
    expected.append(String("foo"));
    checkKeyPath(keyPath, expected, 3);
}

TEST(IDBKeyPathTest, InvalidKeyPath5)
{
    Vector<String> expected;
    String keyPath("foo..bar..baz");
    expected.append(String("foo"));
    checkKeyPath(keyPath, expected, 3);
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
