#!/usr/bin/env python

# Copyright (C) 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
# Copyright (C) 2006 Anders Carlsson <andersca@mac.com>
# Copyright (C) 2006, 2007 Samuel Weinig <sam@webkit.org>
# Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
# Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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


def stringHash(str):
    # Implements Paul Hsieh's SuperFastHash - http://www.azillionmonkeys.com/qed/hash.html
    # LChar data is interpreted as Latin-1-encoded (zero extended to 16 bits).
    stringHashingStartValue = 0x9E3779B9
    twoTo32 = 4294967296

    hash = stringHashingStartValue

    strLength = len(str)
    characterPairs = int(strLength / 2)
    remainder = strLength & 1

    # Main loop
    while characterPairs > 0:
        hash = hash + ord(str[0])
        tmp = (ord(str[1]) << 11) ^ hash
        hash = ((hash << 16) % twoTo32) ^ tmp
        str = str[2:]
        hash = hash + (hash >> 11)
        hash = hash % twoTo32
        characterPairs = characterPairs - 1

    # Handle end case
    if remainder:
        hash = hash + ord(str[0])
        hash = hash ^ ((hash << 11) % twoTo32)
        hash = hash + (hash >> 17)

    hash = hash ^ (hash << 3)
    hash = hash + (hash >> 5)
    hash = hash % twoTo32
    hash = hash ^ ((hash << 2) % twoTo32)
    hash = hash + (hash >> 15)
    hash = hash % twoTo32
    hash = hash ^ ((hash << 10) % twoTo32)

    hash = hash & 0xffffff

    if not hash:
        hash = 0x800000

    return hash
