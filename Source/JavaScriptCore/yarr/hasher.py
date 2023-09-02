#!/usr/bin/env python3

# Copyright (C) 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
# Copyright (C) 2006 Anders Carlsson <andersca@mac.com>
# Copyright (C) 2006, 2007 Samuel Weinig <sam@webkit.org>
# Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
# Copyright (C) 2006-2023 Apple Inc. All rights reserved.
# Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
# Copyright (C) Research In Motion Limited 2010. All rights reserved.
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
# Copyright (C) 2011 Patrick Gansterer <paroga@webkit.org>
# Copyright (C) 2012 Ericsson AB. All rights reserved.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

mask64 = 2**64 - 1
mask32 = 2**32 - 1
secret = [11562461410679940143, 16646288086500911323, 10285213230658275043, 6384245875588680899]


def stringHash(str, isMac):
    strLen = len(str)
    if isMac:
        if strLen <= 48:
            return superFastHash(str)
        return wyhash(str)
    else:
        return superFastHash(str)


def avalancheBits(value):
    value &= mask32

    # Force "avalanching" of lower 32 bits
    value ^= (value << 3)
    value += (value >> 5)
    value &= mask32
    value ^= ((value << 2) & mask32)
    value += (value >> 15)
    value &= mask32
    value ^= ((value << 10) & mask32)
    return value


def maskTop8BitsAndAvoidZero(value):
    value &= mask32

    # Save 8 bits for StringImpl to use as flags.
    value &= 0xffffff

    # This avoids ever returning a hash code of 0, since that is used to
    # signal "hash not computed yet". Setting the high bit maintains
    # reasonable fidelity to a hash code of 0 because it is likely to yield
    # exactly 0 when hash lookup masks out the high bits.
    if not value:
        value = 0x800000
    return value


def superFastHash(str):
    # Implements Paul Hsieh's SuperFastHash - http://www.azillionmonkeys.com/qed/hash.html
    # LChar data is interpreted as Latin-1-encoded (zero extended to 16 bits).
    stringHashingStartValue = 0x9E3779B9

    hash = stringHashingStartValue

    strLength = len(str)
    characterPairs = int(strLength / 2)
    remainder = strLength & 1

    # Main loop
    while characterPairs > 0:
        hash += ord(str[0])
        tmp = (ord(str[1]) << 11) ^ hash
        hash = ((hash << 16) & mask32) ^ tmp
        str = str[2:]
        hash += (hash >> 11)
        hash &= mask32
        characterPairs = characterPairs - 1

    # Handle end case
    if remainder:
        hash += ord(str[0])
        hash ^= ((hash << 11) & mask32)
        hash += (hash >> 17)

    hash = avalancheBits(hash)
    return maskTop8BitsAndAvoidZero(hash)


def wyhash(string):
    # https://github.com/wangyi-fudan/wyhash
    def add64(a, b):
        return (a + b) & mask64

    def multi64(a, b):
        return a * b & mask64

    def wymum(A, B):
        ha = A >> 32
        hb = B >> 32
        la = A & mask32
        lb = B & mask32
        rh = multi64(ha, hb)
        rm0 = multi64(ha, lb)
        rm1 = multi64(hb, la)
        rl = multi64(la, lb)
        t = add64(rl, (rm0 << 32))
        c = int(t < rl)

        lo = add64(t, (rm1 << 32))
        c += int(lo < t)
        hi = add64(rh, add64((rm0 >> 32), add64((rm1 >> 32), c)))
        return lo, hi

    def wymix(A, B):
        A, B = wymum(A, B)
        return A ^ B

    def convert32BitTo64Bit(v):
        v = (v | (v << 16)) & 0x0000_ffff_0000_ffff
        return (v | (v << 8)) & 0x00ff_00ff_00ff_00ff

    def convert16BitTo32Bit(v):
        return (v | (v << 8)) & 0x00ff_00ff

    charCount = len(string)
    byteCount = charCount << 1
    charIndex = 0
    seed = 0
    move1 = ((byteCount >> 3) << 2) >> 1

    seed ^= wymix(seed ^ secret[0], secret[1])
    a = 0
    b = 0

    def c2i(i):
        return ord(string[i])

    def wyr8(i):
        v = c2i(i) | (c2i(i + 1) << 8) | (c2i(i + 2) << 16) | (c2i(i + 3) << 24)
        return convert32BitTo64Bit(v)

    def wyr4(i):
        v = c2i(i) | (c2i(i + 1) << 8)
        return convert16BitTo32Bit(v)

    def wyr2(i):
        return c2i(i) << 16

    if byteCount <= 16:
        if byteCount >= 4:
            a = (wyr4(charIndex) << 32) | wyr4(charIndex + move1)
            charIndex = charIndex + charCount - 2
            b = (wyr4(charIndex) << 32) | wyr4(charIndex - move1)
        elif byteCount > 0:
            a = wyr2(charIndex)
            b = 0
        else:
            a = b = 0
    else:
        i = byteCount
        if i > 48:
            see1 = seed
            see2 = seed
            while True:
                seed = wymix(wyr8(charIndex) ^ secret[1], wyr8(charIndex + 4) ^ seed)
                see1 = wymix(wyr8(charIndex + 8) ^ secret[2], wyr8(charIndex + 12) ^ see1)
                see2 = wymix(wyr8(charIndex + 16) ^ secret[3], wyr8(charIndex + 20) ^ see2)
                charIndex += 24
                i -= 48
                if i <= 48:
                    break
            seed ^= see1 ^ see2
        while i > 16:
            seed = wymix(wyr8(charIndex) ^ secret[1], wyr8(charIndex + 4) ^ seed)
            i -= 16
            charIndex += 8
        move2 = i >> 1
        a = wyr8(charIndex + move2 - 8)
        b = wyr8(charIndex + move2 - 4)
    a ^= secret[1]
    b ^= seed

    (a, b) = wymum(a, b)
    hashValue = wymix(a ^ secret[0] ^ byteCount, b ^ secret[1]) & mask32

    return maskTop8BitsAndAvoidZero(hashValue)


def ceilingToPowerOf2(v):
    v -= 1
    v |= v >> 1
    v |= v >> 2
    v |= v >> 4
    v |= v >> 8
    v |= v >> 16
    v += 1
    return v


# This is used to compute CompactHashIndex in JSDollarVM.cpp,
# where the indexMask in the corresponding HashTable should
# be numEntries - 1.
def createHashTable(keys, hashTableName):
    def createHashTableHelper(keys, hashTableName, isMac):
        table = {}
        links = {}
        compactSize = ceilingToPowerOf2(len(keys))
        maxDepth = 0
        collisions = 0
        numEntries = compactSize

        i = 0
        for key in keys:
            depth = 0
            hashValue = stringHash(key, isMac) % numEntries
            while hashValue in table:
                if hashValue in links:
                    hashValue = links[hashValue]
                    depth += 1
                else:
                    collisions += 1
                    links[hashValue] = compactSize
                    hashValue = compactSize
                    compactSize += 1
            table[hashValue] = i
            i += 1
            if depth > maxDepth:
                maxDepth = depth

        string = "static const struct CompactHashIndex {}[{}] = {{\n".format(hashTableName, compactSize)
        for i in range(compactSize):
            T = -1
            if i in table:
                T = table[i]
            L = -1
            if i in links:
                L = links[i]
            string += '    {{ {}, {} }},\n'.format(T, L)
        string += '};\n'
        return string

    hashTableForMacOS = createHashTableHelper(keys, hashTableName, True)
    hashTableForIOS = createHashTableHelper(keys, hashTableName, False)
    result = hashTableForMacOS
    if hashTableForMacOS != hashTableForIOS:
        result = "#if PLATFORM(MAC)\n{}#else\n{}#endif".format(hashTableForMacOS, hashTableForIOS)
    print(result)

