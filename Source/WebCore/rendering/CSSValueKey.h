/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "CSSValueKeywords.h"

namespace WebCore {

struct CSSValueKey {

    unsigned hash() const;

    unsigned cssValueID;
    bool useDarkAppearance;
    bool useElevatedUserInterfaceLevel;
};

inline bool operator==(const CSSValueKey& a, const CSSValueKey& b)
{
    return a.cssValueID == b.cssValueID && a.useDarkAppearance == b.useDarkAppearance && a.useElevatedUserInterfaceLevel == b.useElevatedUserInterfaceLevel;
}

inline unsigned CSSValueKey::hash() const
{
    return cssValueID;
}

} // namespace WebCore

namespace WTF {

struct CSSValueKeyHash {
    static unsigned hash(const WebCore::CSSValueKey& key) { return key.hash(); }
    static bool equal(const WebCore::CSSValueKey& a, const WebCore::CSSValueKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebCore::CSSValueKey> : GenericHashTraits<WebCore::CSSValueKey> {
    static WebCore::CSSValueKey emptyValue() { return WebCore::CSSValueKey { WebCore::CSSValueInvalid, false, false}; }
    static void constructDeletedValue(WebCore::CSSValueKey& slot) { new (NotNull, &slot) WebCore::CSSValueKey { WebCore::CSSValueInvalid, true, true}; }
    static bool isDeletedValue(const WebCore::CSSValueKey& slot) { return slot.cssValueID == WebCore::CSSValueInvalid && slot.useDarkAppearance && slot.useElevatedUserInterfaceLevel; }
};

template<> struct DefaultHash<WebCore::CSSValueKey> : CSSValueKeyHash { };

} // namespace WTF
