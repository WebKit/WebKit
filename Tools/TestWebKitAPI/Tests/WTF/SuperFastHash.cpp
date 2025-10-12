/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
#include <wtf/text/SuperFastHash.h>

namespace TestWebKitAPI {

static const Latin1Character nullLatin1Characters[2] = { 0, 0 };
static const char16_t nullUTF16CodeUnits[2] = { 0, 0 };

static const unsigned emptyStringHash = 0x4EC889EU;
static const unsigned singleNullCharacterHash = 0x3D3ABF44U;

static const Latin1Character testALatin1Characters[6] = { 0x41, 0x95, 0xFF, 0x50, 0x01, 0 };
static const char16_t testAUTF16CodeUnits[6] = { 0x41, 0x95, 0xFF, 0x50, 0x01, 0 };
static const char16_t testBUTF16CodeUnits[6] = { 0x41, 0x95, 0xFFFF, 0x1080, 0x01, 0 };

static const unsigned testAHash1 = 0xEA32B004;
static const unsigned testAHash2 = 0x93F0F71E;
static const unsigned testAHash3 = 0xCB609EB1;
static const unsigned testAHash4 = 0x7984A706;
static const unsigned testAHash5 = 0x0427561F;

static const unsigned testBHash1 = 0xEA32B004;
static const unsigned testBHash2 = 0x93F0F71E;
static const unsigned testBHash3 = 0x59EB1B2C;
static const unsigned testBHash4 = 0xA7BCCC0A;
static const unsigned testBHash5 = 0x79201649;

TEST(WTF, SuperFastHash)
{
    SuperFastHash hasher;

    // The initial state of the hasher.
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
}

TEST(WTF, SuperFastHash_addCharacter)
{
    SuperFastHash hasher;

    // Hashing a single character.
    hasher = SuperFastHash();
    hasher.addCharacter(0);
    ASSERT_EQ(singleNullCharacterHash, hasher.hash());
    ASSERT_EQ(singleNullCharacterHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing five characters, checking the intermediate state after each is added.
    hasher = SuperFastHash();
    hasher.addCharacter(testAUTF16CodeUnits[0]);
    ASSERT_EQ(testAHash1, hasher.hash());
    ASSERT_EQ(testAHash1 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacter(testAUTF16CodeUnits[1]);
    ASSERT_EQ(testAHash2, hasher.hash());
    ASSERT_EQ(testAHash2 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacter(testAUTF16CodeUnits[2]);
    ASSERT_EQ(testAHash3, hasher.hash());
    ASSERT_EQ(testAHash3 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacter(testAUTF16CodeUnits[3]);
    ASSERT_EQ(testAHash4, hasher.hash());
    ASSERT_EQ(testAHash4 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacter(testAUTF16CodeUnits[4]);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing a second set of five characters, including non-Latin-1 characters.
    hasher = SuperFastHash();
    hasher.addCharacter(testBUTF16CodeUnits[0]);
    ASSERT_EQ(testBHash1, hasher.hash());
    ASSERT_EQ(testBHash1 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacter(testBUTF16CodeUnits[1]);
    ASSERT_EQ(testBHash2, hasher.hash());
    ASSERT_EQ(testBHash2 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacter(testBUTF16CodeUnits[2]);
    ASSERT_EQ(testBHash3, hasher.hash());
    ASSERT_EQ(testBHash3 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacter(testBUTF16CodeUnits[3]);
    ASSERT_EQ(testBHash4, hasher.hash());
    ASSERT_EQ(testBHash4 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacter(testBUTF16CodeUnits[4]);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
}

TEST(WTF, SuperFastHash_addCharacters)
{
    SuperFastHash hasher;

    // Hashing zero characters.
    hasher = SuperFastHash();
    hasher.addCharacters(static_cast<Latin1Character*>(0), 0);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(nullLatin1Characters, 0);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(nullLatin1Characters);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(static_cast<char16_t*>(0), 0);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(nullUTF16CodeUnits, 0);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(nullUTF16CodeUnits);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing one character.
    hasher = SuperFastHash();
    hasher.addCharacters(nullLatin1Characters, 1);
    ASSERT_EQ(singleNullCharacterHash, hasher.hash());
    ASSERT_EQ(singleNullCharacterHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(nullUTF16CodeUnits, 1);
    ASSERT_EQ(singleNullCharacterHash, hasher.hash());
    ASSERT_EQ(singleNullCharacterHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing five characters, all at once.
    hasher = SuperFastHash();
    hasher.addCharacters(testALatin1Characters, 5);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testALatin1Characters);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testAUTF16CodeUnits, 5);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testAUTF16CodeUnits);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testBUTF16CodeUnits, 5);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testBUTF16CodeUnits);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing five characters, in groups of two, then the last one.
    hasher = SuperFastHash();
    hasher.addCharacters(testALatin1Characters, 2);
    ASSERT_EQ(testAHash2, hasher.hash());
    ASSERT_EQ(testAHash2 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testALatin1Characters + 2, 2);
    ASSERT_EQ(testAHash4, hasher.hash());
    ASSERT_EQ(testAHash4 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testALatin1Characters + 4, 1);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testALatin1Characters, 2);
    hasher.addCharacters(testALatin1Characters + 2, 2);
    hasher.addCharacters(testALatin1Characters + 4);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testAUTF16CodeUnits, 2);
    ASSERT_EQ(testAHash2, hasher.hash());
    ASSERT_EQ(testAHash2 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testAUTF16CodeUnits + 2, 2);
    ASSERT_EQ(testAHash4, hasher.hash());
    ASSERT_EQ(testAHash4 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testAUTF16CodeUnits + 4, 1);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testAUTF16CodeUnits, 2);
    hasher.addCharacters(testAUTF16CodeUnits + 2, 2);
    hasher.addCharacters(testAUTF16CodeUnits + 4);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testBUTF16CodeUnits, 2);
    ASSERT_EQ(testBHash2, hasher.hash());
    ASSERT_EQ(testBHash2 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testBUTF16CodeUnits + 2, 2);
    ASSERT_EQ(testBHash4, hasher.hash());
    ASSERT_EQ(testBHash4 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testBUTF16CodeUnits + 4, 1);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testBUTF16CodeUnits, 2);
    hasher.addCharacters(testBUTF16CodeUnits + 2, 2);
    hasher.addCharacters(testBUTF16CodeUnits + 4);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing five characters, the first three, then the last two.
    hasher = SuperFastHash();
    hasher.addCharacters(testALatin1Characters, 3);
    ASSERT_EQ(testAHash3, hasher.hash());
    ASSERT_EQ(testAHash3 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testALatin1Characters + 3, 2);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testALatin1Characters, 3);
    ASSERT_EQ(testAHash3, hasher.hash());
    ASSERT_EQ(testAHash3 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testALatin1Characters + 3);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testAUTF16CodeUnits, 3);
    ASSERT_EQ(testAHash3, hasher.hash());
    ASSERT_EQ(testAHash3 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testAUTF16CodeUnits + 3, 2);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testAUTF16CodeUnits, 3);
    ASSERT_EQ(testAHash3, hasher.hash());
    ASSERT_EQ(testAHash3 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testAUTF16CodeUnits + 3, 2);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testBUTF16CodeUnits, 3);
    ASSERT_EQ(testBHash3, hasher.hash());
    ASSERT_EQ(testBHash3 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacters(testBUTF16CodeUnits + 3, 2);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharacters(testBUTF16CodeUnits, 3);
    hasher.addCharacters(testBUTF16CodeUnits + 3);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing zero characters after hashing other characters.
    hasher = SuperFastHash();
    hasher.addCharacters(nullLatin1Characters, 0);
    hasher.addCharacters(nullLatin1Characters, 0);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    hasher = SuperFastHash();
    hasher.addCharacters(testALatin1Characters, 1);
    hasher.addCharacters(nullLatin1Characters, 0);
    ASSERT_EQ(testAHash1, hasher.hash());
    ASSERT_EQ(testAHash1 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    hasher = SuperFastHash();
    hasher.addCharacters(testALatin1Characters, 2);
    hasher.addCharacters(nullLatin1Characters, 0);
    ASSERT_EQ(testAHash2, hasher.hash());
    ASSERT_EQ(testAHash2 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    hasher = SuperFastHash();
    hasher.addCharacters(testAUTF16CodeUnits, 3);
    hasher.addCharacters(nullLatin1Characters, 0);
    ASSERT_EQ(testAHash3, hasher.hash());
    ASSERT_EQ(testAHash3 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    hasher = SuperFastHash();
    hasher.addCharacters(testALatin1Characters, 4);
    hasher.addCharacters(nullLatin1Characters, 0);
    ASSERT_EQ(testAHash4, hasher.hash());
    ASSERT_EQ(testAHash4 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    hasher = SuperFastHash();
    hasher.addCharacters(testALatin1Characters, 5);
    hasher.addCharacters(nullLatin1Characters, 0);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
}

TEST(WTF, SuperFastHash_addCharactersAssumingAligned)
{
    SuperFastHash hasher;

    // Hashing zero characters.
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(static_cast<Latin1Character*>(0), 0);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(nullLatin1Characters, 0);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(static_cast<char16_t*>(0), 0);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(nullUTF16CodeUnits, 0);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(nullUTF16CodeUnits);
    ASSERT_EQ(emptyStringHash, hasher.hash());
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing one character.
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(nullLatin1Characters, 1);
    ASSERT_EQ(singleNullCharacterHash, hasher.hash());
    ASSERT_EQ(singleNullCharacterHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(nullUTF16CodeUnits, 1);
    ASSERT_EQ(singleNullCharacterHash, hasher.hash());
    ASSERT_EQ(singleNullCharacterHash & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing five characters, all at once.
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testALatin1Characters, 5);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testALatin1Characters);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testAUTF16CodeUnits, 5);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testAUTF16CodeUnits);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testBUTF16CodeUnits, 5);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testBUTF16CodeUnits);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing five characters, in groups of two, then the last one.
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testALatin1Characters, 2);
    ASSERT_EQ(testAHash2, hasher.hash());
    ASSERT_EQ(testAHash2 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharactersAssumingAligned(testALatin1Characters + 2, 2);
    ASSERT_EQ(testAHash4, hasher.hash());
    ASSERT_EQ(testAHash4 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharactersAssumingAligned(testALatin1Characters + 4, 1);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testALatin1Characters, 2);
    hasher.addCharactersAssumingAligned(testALatin1Characters + 2, 2);
    hasher.addCharactersAssumingAligned(testALatin1Characters + 4);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testAUTF16CodeUnits, 2);
    ASSERT_EQ(testAHash2, hasher.hash());
    ASSERT_EQ(testAHash2 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharactersAssumingAligned(testAUTF16CodeUnits + 2, 2);
    ASSERT_EQ(testAHash4, hasher.hash());
    ASSERT_EQ(testAHash4 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharactersAssumingAligned(testAUTF16CodeUnits + 4, 1);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testAUTF16CodeUnits, 2);
    hasher.addCharactersAssumingAligned(testAUTF16CodeUnits + 2, 2);
    hasher.addCharactersAssumingAligned(testAUTF16CodeUnits + 4);
    ASSERT_EQ(testAHash5, hasher.hash());
    ASSERT_EQ(testAHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testBUTF16CodeUnits, 2);
    ASSERT_EQ(testBHash2, hasher.hash());
    ASSERT_EQ(testBHash2 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharactersAssumingAligned(testBUTF16CodeUnits + 2, 2);
    ASSERT_EQ(testBHash4, hasher.hash());
    ASSERT_EQ(testBHash4 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharactersAssumingAligned(testBUTF16CodeUnits + 4, 1);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher = SuperFastHash();
    hasher.addCharactersAssumingAligned(testBUTF16CodeUnits, 2);
    hasher.addCharactersAssumingAligned(testBUTF16CodeUnits + 2, 2);
    hasher.addCharactersAssumingAligned(testBUTF16CodeUnits + 4);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());

    // Hashing five characters, first two characters one at a time,
    // then two more, then the last one.
    hasher = SuperFastHash();
    hasher.addCharacter(testBUTF16CodeUnits[0]);
    ASSERT_EQ(testBHash1, hasher.hash());
    ASSERT_EQ(testBHash1 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharacter(testBUTF16CodeUnits[1]);
    ASSERT_EQ(testBHash2, hasher.hash());
    ASSERT_EQ(testBHash2 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharactersAssumingAligned(testBUTF16CodeUnits[2], testBUTF16CodeUnits[3]);
    ASSERT_EQ(testBHash4, hasher.hash());
    ASSERT_EQ(testBHash4 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
    hasher.addCharactersAssumingAligned(testBUTF16CodeUnits + 4);
    ASSERT_EQ(testBHash5, hasher.hash());
    ASSERT_EQ(testBHash5 & 0xFFFFFF, hasher.hashWithTop8BitsMasked());
}

TEST(WTF, SuperFastHash_computeHash)
{
    // Use `zeroLength` instead of hardcoding `0` to work around around llvm bug:
    // Ambiguous constructor call error when calling std::span<T>(T*, 0).
    // https://bugs.llvm.org/show_bug.cgi?id=49295
    static constexpr size_t zeroLength = 0;
    ASSERT_EQ(emptyStringHash, SuperFastHash::computeHash(std::span { static_cast<const Latin1Character*>(0), zeroLength }));
    ASSERT_EQ(emptyStringHash, SuperFastHash::computeHash(std::span { nullLatin1Characters, zeroLength }));
    ASSERT_EQ(emptyStringHash, SuperFastHash::computeHash(std::span { static_cast<const char16_t*>(0), zeroLength }));
    ASSERT_EQ(emptyStringHash, SuperFastHash::computeHash(std::span { nullUTF16CodeUnits, zeroLength }));

    ASSERT_EQ(singleNullCharacterHash, SuperFastHash::computeHash(std::span { nullLatin1Characters, 1 }));
    ASSERT_EQ(singleNullCharacterHash, SuperFastHash::computeHash(std::span { nullUTF16CodeUnits, 1 }));

    ASSERT_EQ(testAHash5, SuperFastHash::computeHash(std::span { testALatin1Characters, 5 }));
    ASSERT_EQ(testAHash5, SuperFastHash::computeHash(std::span { testAUTF16CodeUnits, 5 }));
    ASSERT_EQ(testBHash5, SuperFastHash::computeHash(std::span { testBUTF16CodeUnits, 5 }));
}

TEST(WTF, SuperFastHash_computeHashAndMaskTop8Bits)
{
    // Use `zeroLength` instead of hardcoding `0` to work around around llvm bug:
    // Ambiguous constructor call error when calling std::span<T>(T*, 0).
    // https://bugs.llvm.org/show_bug.cgi?id=49295
    static constexpr size_t zeroLength = 0;
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, SuperFastHash::computeHashAndMaskTop8Bits(std::span { static_cast<const Latin1Character*>(0), zeroLength }));
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, SuperFastHash::computeHashAndMaskTop8Bits(std::span { nullLatin1Characters, zeroLength }));
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, SuperFastHash::computeHashAndMaskTop8Bits(std::span { static_cast<const char16_t*>(0), zeroLength }));
    ASSERT_EQ(emptyStringHash & 0xFFFFFF, SuperFastHash::computeHashAndMaskTop8Bits(std::span { nullUTF16CodeUnits, zeroLength }));

    ASSERT_EQ(singleNullCharacterHash & 0xFFFFFF, SuperFastHash::computeHashAndMaskTop8Bits(std::span { nullLatin1Characters, 1 }));
    ASSERT_EQ(singleNullCharacterHash & 0xFFFFFF, SuperFastHash::computeHashAndMaskTop8Bits(std::span { nullUTF16CodeUnits, 1 }));

    ASSERT_EQ(testAHash5 & 0xFFFFFF, SuperFastHash::computeHashAndMaskTop8Bits(std::span { testALatin1Characters, 5 }));
    ASSERT_EQ(testAHash5 & 0xFFFFFF, SuperFastHash::computeHashAndMaskTop8Bits(std::span { testAUTF16CodeUnits, 5 }));
    ASSERT_EQ(testBHash5 & 0xFFFFFF, SuperFastHash::computeHashAndMaskTop8Bits(std::span { testBUTF16CodeUnits, 5 }));
}

} // namespace TestWebKitAPI
