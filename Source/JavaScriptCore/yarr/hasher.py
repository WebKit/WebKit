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
secret = [3257665815644502181, 10067880064238660809, 5418857496715711651]


def stringHash(str):
    return rapidhash(str)


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


def rapidhash(string):
    # https://github.com/Nicoshev/rapidhash
    # Hashes raw ASCII bytes (1 byte per character).
    def add64(a, b):
        return (a + b) & mask64

    def multi64(a, b):
        return a * b & mask64

    def rapid_mul128(A, B):
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

    def rapid_mix(A, B):
        A, B = rapid_mul128(A, B)
        return A ^ B

    length = len(string)
    seed = rapid_mix(0 ^ secret[0], secret[1]) ^ length
    a = 0
    b = 0

    def read64(i):
        return (ord(string[i])
                | (ord(string[i + 1]) << 8)
                | (ord(string[i + 2]) << 16)
                | (ord(string[i + 3]) << 24)
                | (ord(string[i + 4]) << 32)
                | (ord(string[i + 5]) << 40)
                | (ord(string[i + 6]) << 48)
                | (ord(string[i + 7]) << 56))

    def read32(i):
        return (ord(string[i])
                | (ord(string[i + 1]) << 8)
                | (ord(string[i + 2]) << 16)
                | (ord(string[i + 3]) << 24))

    def readSmall(i, k):
        return ((ord(string[i]) << 56)
                | (ord(string[i + (k >> 1)]) << 32)
                | ord(string[i + k - 1]))

    if length <= 16:
        if length >= 4:
            delta = 4 if length >= 8 else 0
            a = (read32(0) << 32) | read32(length - 4)
            b = (read32(delta) << 32) | read32(length - 4 - delta)
        elif length > 0:
            a = readSmall(0, length)
            b = 0
        else:
            a = b = 0
    else:
        i = length
        off = 0
        if i > 48:
            see1 = seed
            see2 = seed
            while True:
                seed = rapid_mix(read64(off) ^ secret[0], read64(off + 8) ^ seed)
                see1 = rapid_mix(read64(off + 16) ^ secret[1], read64(off + 24) ^ see1)
                see2 = rapid_mix(read64(off + 32) ^ secret[2], read64(off + 40) ^ see2)
                off += 48
                i -= 48
                if i < 48:
                    break
            seed ^= see1 ^ see2
        if i > 16:
            seed = rapid_mix(read64(off) ^ secret[2], read64(off + 8) ^ seed ^ secret[1])
            if i > 32:
                seed = rapid_mix(read64(off + 16) ^ secret[2], read64(off + 24) ^ seed)
        a = read64(off + i - 16)
        b = read64(off + i - 8)
    a ^= secret[1]
    b ^= seed

    (a, b) = rapid_mul128(a, b)
    hashValue = rapid_mix(a ^ secret[0] ^ length, b ^ secret[1]) & mask32

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
    def createHashTableHelper(keys, hashTableName):
        table = {}
        links = {}
        compactSize = ceilingToPowerOf2(len(keys))
        maxDepth = 0
        collisions = 0
        numEntries = compactSize

        i = 0
        for key in keys:
            depth = 0
            hashValue = stringHash(key) % numEntries
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

        string = "static constinit const struct CompactHashIndex {}[{}] = {{\n".format(hashTableName, compactSize)
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

    hashTableForRapidHash = createHashTableHelper(keys, hashTableName)
    print(hashTableForRapidHash)

