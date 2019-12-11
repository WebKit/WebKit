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
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct WebPreferencesStore {
    WebPreferencesStore();

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, WebPreferencesStore&);

    // NOTE: The getters in this class have non-standard names to aid in the use of the preference macros.

    bool setStringValueForKey(const String& key, const String& value);
    String getStringValueForKey(const String& key) const;

    bool setBoolValueForKey(const String& key, bool value);
    bool getBoolValueForKey(const String& key) const;

    bool setUInt32ValueForKey(const String& key, uint32_t value);
    uint32_t getUInt32ValueForKey(const String& key) const;

    bool setDoubleValueForKey(const String& key, double value);
    double getDoubleValueForKey(const String& key) const;

    void setOverrideDefaultsStringValueForKey(const String& key, String value);
    void setOverrideDefaultsBoolValueForKey(const String& key, bool value);
    void setOverrideDefaultsUInt32ValueForKey(const String& key, uint32_t value);
    void setOverrideDefaultsDoubleValueForKey(const String& key, double value);

    void deleteKey(const String& key);

    // For WebKitTestRunner usage.
    static void overrideBoolValueForKey(const String& key, bool value);
    static void removeTestRunnerOverrides();

    using Value = Variant<String, bool, uint32_t, double>;

    typedef HashMap<String, Value> ValueMap;
    ValueMap m_values;
    ValueMap m_overriddenDefaults;

    static ValueMap& defaults();
};

} // namespace WebKit
