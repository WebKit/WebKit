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

IDBKeyPathElement ExpectedToken(const String& identifier, bool isIndexed, int index)
{
    IDBKeyPathElement expected;
    if (isIndexed) {
        expected.type = IDBKeyPathElement::IsIndexed;
        expected.index = index;
    } else {
        expected.type = IDBKeyPathElement::IsNamed;
        expected.identifier = identifier;
    }
    return expected;
}

void checkKeyPath(const String& keyPath, const Vector<IDBKeyPathElement>& expected, int parserError)
{

    IDBKeyPathParseError error;
    Vector<IDBKeyPathElement> idbKeyPathElements;
    IDBParseKeyPath(keyPath, idbKeyPathElements, error);
    ASSERT_EQ(parserError, error);
    if (error != IDBKeyPathParseErrorNone)
        return;
    ASSERT_EQ(expected.size(), idbKeyPathElements.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        ASSERT_TRUE(expected[i].type == idbKeyPathElements[i].type) << i;
        if (expected[i].type == IDBKeyPathElement::IsIndexed)
            ASSERT_EQ(expected[i].index, idbKeyPathElements[i].index) << i;
        else if (expected[i].type == IDBKeyPathElement::IsNamed)
            ASSERT_TRUE(expected[i].identifier == idbKeyPathElements[i].identifier) << i;
        else
            ASSERT_TRUE(false) << "Invalid IDBKeyPathElement type";
    }
}

TEST(IDBKeyPathTest, ValidKeyPath0)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("foo.bar.zoo");
    expected.append(ExpectedToken("foo", false, 0));
    expected.append(ExpectedToken("bar", false, 0));
    expected.append(ExpectedToken("zoo", false, 0));
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, ValidKeyPath1)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("a[34][20].foo[2].bar");
    expected.append(ExpectedToken("a", false, 0));
    expected.append(ExpectedToken(String(), true, 34));
    expected.append(ExpectedToken(String(), true, 20));
    expected.append(ExpectedToken("foo", false, 0));
    expected.append(ExpectedToken(String(), true, 2));
    expected.append(ExpectedToken("bar", false, 0));
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, ValidKeyPath2)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("foo[ 34 ].Zoo_[00023]\t._c");
    expected.append(ExpectedToken("foo", false, 0));
    expected.append(ExpectedToken(String(), true, 34));
    expected.append(ExpectedToken("Zoo_", false, 0));
    expected.append(ExpectedToken(String(), true, 23));
    expected.append(ExpectedToken("_c", false, 0));
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, ValidKeyPath3)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("foo[ 34 ]");
    expected.append(ExpectedToken("foo", false, 0));
    expected.append(ExpectedToken(String(), true, 34));
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, ValidKeyPath4)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("[ 34 ]");
    expected.append(ExpectedToken(String(), true, 34));
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, InvalidKeyPath2)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("a[[34]].b[2].c");
    expected.append(ExpectedToken("a", false, 0));
    checkKeyPath(keyPath, expected, 3);
}

TEST(IDBKeyPathTest, InvalidKeyPath3)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("a[[34].b[2].c");
    expected.append(ExpectedToken("a", false, 0));
    checkKeyPath(keyPath, expected, 3);
}

TEST(IDBKeyPathTest, InvalidKeyPath5)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("a[[34.b[2].c");
    expected.append(ExpectedToken("a", false, 0));
    checkKeyPath(keyPath, expected, 3);
}

TEST(IDBKeyPathTest, InvalidKeyPath6)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("+a[34].b[2].c");
    checkKeyPath(keyPath, expected, 1);
}

TEST(IDBKeyPathTest, InvalidKeyPath7)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("%a[34].b[2].c");
    checkKeyPath(keyPath, expected, 1);
}

TEST(IDBKeyPathTest, InvalidKeyPath8)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("a{[34]}.b[2].c");
    expected.append(ExpectedToken("a", false, 0));
    checkKeyPath(keyPath, expected, 2);
}

TEST(IDBKeyPathTest, InvalidKeyPath9)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("a..b[2].c");
    expected.append(ExpectedToken("a", false, 0));
    checkKeyPath(keyPath, expected, 5);
}

TEST(IDBKeyPathTest, InvalidKeyPath10)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("a[34]b.foo[2].bar");
    expected.append(ExpectedToken("a", false, 0));
    expected.append(ExpectedToken(String(), true, 34));
    checkKeyPath(keyPath, expected, 4);
}

TEST(IDBKeyPathTest, InvalidKeyPath11)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("a[-1]");
    expected.append(ExpectedToken("a", false, 0));
    checkKeyPath(keyPath, expected, 3);
}

TEST(IDBKeyPathTest, InvalidKeyPath12)
{
    Vector<IDBKeyPathElement> expected;
    String keyPath("a[9999999999999999999999999999999999]");
    expected.append(ExpectedToken("a", false, 0));
    checkKeyPath(keyPath, expected, 3);
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
