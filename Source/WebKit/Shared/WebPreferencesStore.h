/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#include "Decoder.h"
#include "Encoder.h"
#include <wtf/CheckedRef.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/HashTraits.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

// FIXME: WebPreferencesStore should be RefCounted. See usage in WebProcessPool.cpp.

struct WebPreferencesStore {
    using Value = Variant<RefPtr<StringImpl>, bool, uint32_t, double>;
    using ValueMap = MemoryCompactRobinHoodHashMap<RefPtr<StringImpl>, Value>;

    // NOTE: The getters in this class have non-standard names to aid in the use of the preference macros.

    bool setStringValueForKey(StringImpl* key, StringImpl* value);
    RefPtr<StringImpl> getStringValueForKey(StringImpl* key) const;

    bool setBoolValueForKey(StringImpl* key, bool value);
    NODELETE bool getBoolValueForKey(StringImpl* key) const;

    bool setUInt32ValueForKey(StringImpl* key, uint32_t value);
    NODELETE uint32_t getUInt32ValueForKey(StringImpl* key) const;

    bool setDoubleValueForKey(StringImpl* key, double value);
    NODELETE double getDoubleValueForKey(StringImpl* key) const;

    void setOverrideDefaultsStringValueForKey(StringImpl* key, RefPtr<StringImpl> value);
    void setOverrideDefaultsBoolValueForKey(StringImpl* key, bool value);
    void setOverrideDefaultsUInt32ValueForKey(StringImpl* key, uint32_t value);
    void setOverrideDefaultsDoubleValueForKey(StringImpl* key, double value);

    void deleteKey(StringImpl* key);

    void logNonDefaultValues() const;

    // For WebKitTestRunner usage.
    static void overrideBoolValueForKey(StringImpl* key, bool value);
    static void removeTestRunnerOverrides();

    ValueMap m_values { };
    ValueMap m_overriddenDefaults { };

    WebPreferencesStore isolatedCopy() const & { return { crossThreadCopy(m_values), crossThreadCopy(m_overriddenDefaults) }; }
    WebPreferencesStore isolatedCopy() && { return { crossThreadCopy(WTF::move(m_values)), crossThreadCopy(WTF::move(m_overriddenDefaults)) }; }

    static ValueMap& defaults();
};

} // namespace WebKit
