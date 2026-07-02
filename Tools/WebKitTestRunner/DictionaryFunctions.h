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

#include "StringFunctions.h"
#include <WebKit/WKCast.h>

namespace WTR {

template<typename T> void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, const WKRetainPtr<T>& value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, bool value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, const char* value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, uint64_t value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, double value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, WKStringRef value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, JSStringRef value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, const WTF::String& value);
template<typename T> void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, const std::optional<T>& value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, int value) = delete;
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, unsigned value) = delete;
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, const void* value) = delete;

inline bool booleanValue(WKTypeRef value)
{
    WKBooleanRef boolean = dynamic_wk_cast<WKBooleanRef>(value);
    return boolean && WKBooleanGetValue(boolean);
}

inline bool booleanValue(const WKRetainPtr<WKTypeRef>& value)
{
    return booleanValue(value.get());
}

inline WKDictionaryRef dictionaryValue(WKTypeRef value)
{
    return dynamic_wk_cast<WKDictionaryRef>(value);
}

inline WKArrayRef arrayValue(WKTypeRef value)
{
    return dynamic_wk_cast<WKArrayRef>(value);
}

inline double doubleValue(WKTypeRef value)
{
    WKDoubleRef d = dynamic_wk_cast<WKDoubleRef>(value);
    return d ? WKDoubleGetValue(d) : 0;
}

inline std::optional<double> optionalDoubleValue(WKTypeRef value)
{
    WKDoubleRef d = dynamic_wk_cast<WKDoubleRef>(value);
    return d ? std::make_optional(WKDoubleGetValue(d)) : std::nullopt;
}

inline WKStringRef stringValue(WKTypeRef value)
{
    return dynamic_wk_cast<WKStringRef>(value);
}

inline WTF::String toWTFString(WKTypeRef value)
{
    return toWTFString(stringValue(value));
}

inline uint64_t uint64Value(WKTypeRef value)
{
    WKUInt64Ref i = dynamic_wk_cast<WKUInt64Ref>(value);
    return i ? WKUInt64GetValue(i) : 0;
}

inline uint64_t uint64Value(const WKRetainPtr<WKTypeRef>& value)
{
    return uint64Value(value.get());
}

inline WKTypeRef value(WKDictionaryRef dictionary, const char* key)
{
    return dictionary ? WKDictionaryGetItemForKey(dictionary, toWK(key).get()) : nullptr;
}

inline bool booleanValue(WKDictionaryRef dictionary, const char* key)
{
    return booleanValue(value(dictionary, key));
}

inline double doubleValue(WKDictionaryRef dictionary, const char* key)
{
    return doubleValue(value(dictionary, key));
}

inline WKStringRef stringValue(WKDictionaryRef dictionary, const char* key)
{
    return stringValue(value(dictionary, key));
}

inline WKArrayRef arrayValue(WKDictionaryRef dictionary, const char* key)
{
    return arrayValue(value(dictionary, key));
}

inline uint64_t uint64Value(WKDictionaryRef dictionary, const char* key)
{
    return uint64Value(value(dictionary, key));
}

inline std::optional<double> optionalDoubleValue(WKDictionaryRef dictionary, const char* key)
{
    return optionalDoubleValue(value(dictionary, key));
}

template<typename T> void setValue(const WKRetainPtr<WKMutableDictionaryRef>& dictionary, const char* key, const WKRetainPtr<T>& value)
{
    WKDictionarySetItem(dictionary.get(), toWK(key).get(), value.get());
}

inline void setValue(const WKRetainPtr<WKMutableDictionaryRef>& dictionary, const char* key, JSStringRef value)
{
    setValue(dictionary, key, toWK(value));
}

inline void setValue(const WKRetainPtr<WKMutableDictionaryRef>& dictionary, const char* key, WKStringRef value)
{
    WKDictionarySetItem(dictionary.get(), toWK(key).get(), value);
}

inline void setValue(const WKRetainPtr<WKMutableDictionaryRef>& dictionary, const char* key, bool value)
{
    setValue(dictionary, key, adoptWK(WKBooleanCreate(value)));
}

inline void setValue(const WKRetainPtr<WKMutableDictionaryRef>& dictionary, const char* key, const char* value)
{
    setValue(dictionary, key, toWK(value));
}

inline void setValue(const WKRetainPtr<WKMutableDictionaryRef>& dictionary, const char* key, const WTF::String& value)
{
    setValue(dictionary, key, toWK(value));
}

inline void setValue(const WKRetainPtr<WKMutableDictionaryRef>& dictionary, const char* key, uint64_t value)
{
    setValue(dictionary, key, adoptWK(WKUInt64Create(value)));
}

inline void setValue(const WKRetainPtr<WKMutableDictionaryRef>& dictionary, const char* key, double value)
{
    setValue(dictionary, key, adoptWK(WKDoubleCreate(value)));
}

template<typename T> void setValue(const WKRetainPtr<WKMutableDictionaryRef>& dictionary, const char* key, const std::optional<T>& value)
{
    if (value)
        setValue(dictionary, key, *value);
}

} // namespace WTR
