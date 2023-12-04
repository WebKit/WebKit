/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include "RegistrableDomain.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

struct OrganizationStorageAccessQuirk {
    String organizationName;
    HashMap<RegistrableDomain, Vector<RegistrableDomain>> domainPairings;

    bool isHashTableDeletedValue() const { return organizationName.isHashTableDeletedValue(); }

    OrganizationStorageAccessQuirk(String organizationName, HashMap<RegistrableDomain, Vector<RegistrableDomain>> domainPairings)
        : organizationName { organizationName }, domainPairings { domainPairings } { }

    OrganizationStorageAccessQuirk(WTF::HashTableDeletedValueType)
        : organizationName { WTF::HashTableDeletedValue } { }

    OrganizationStorageAccessQuirk() = default;
};

static bool operator==(const OrganizationStorageAccessQuirk& a, const OrganizationStorageAccessQuirk& b)
{
    return a.organizationName == b.organizationName;
}

inline void add(Hasher& hasher, const OrganizationStorageAccessQuirk& quirk)
{
    add(hasher, quirk.organizationName);
}

struct OrganizationStorageAccessQuirkHashTraits : SimpleClassHashTraits<OrganizationStorageAccessQuirk> {
    static const bool hasIsEmptyValueFunction = true;
    static const bool emptyValueIsZero = false;
    static bool isEmptyValue(const OrganizationStorageAccessQuirk& quirk) { return quirk.organizationName.isNull(); }
};

struct OrganizationStorageAccessQuirkHash {
    static unsigned hash(const OrganizationStorageAccessQuirk& quirk) { return computeHash(quirk); }
    static bool equal(const OrganizationStorageAccessQuirk& a, const OrganizationStorageAccessQuirk& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::OrganizationStorageAccessQuirk> : WebCore::OrganizationStorageAccessQuirkHashTraits { };
template<> struct DefaultHash<WebCore::OrganizationStorageAccessQuirk> : WebCore::OrganizationStorageAccessQuirkHash { };

} // namespace WTF
