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

namespace WTR {

WKDictionaryRef dictionaryValue(WKTypeRef);

bool booleanValue(WKTypeRef);
bool booleanValue(const WKRetainPtr<WKTypeRef>&);

double doubleValue(WKTypeRef);
Optional<double> optionalDoubleValue(WKTypeRef);

WKStringRef stringValue(WKTypeRef);
WTF::String toWTFString(WKTypeRef);

uint64_t uint64Value(WKTypeRef);
uint64_t uint64Value(const WKRetainPtr<WKTypeRef>&);

WKTypeRef value(WKDictionaryRef, const char* key);

bool booleanValue(WKDictionaryRef, const char* key);
double doubleValue(WKDictionaryRef, const char* key);
WKStringRef stringValue(WKDictionaryRef, const char* key);
uint64_t uint64Value(WKDictionaryRef, const char* key);

Optional<double> optionalDoubleValue(WKDictionaryRef, const char* key);

template<typename T> void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, const WKRetainPtr<T>& value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, bool value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, const char* value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, uint64_t value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, double value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, WKStringRef value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, JSStringRef value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, const WTF::String& value);
template<typename T> void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, const Optional<T>& value);
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, int value) = delete;
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, unsigned value) = delete;
void setValue(const WKRetainPtr<WKMutableDictionaryRef>&, const char* key, const void* value) = delete;

inline bool booleanValue(WKTypeRef value)
{
    return value && WKGetTypeID(value) == WKBooleanGetTypeID() && WKBooleanGetValue(static_cast<WKBooleanRef>(value));
}

inline bool booleanValue(const WKRetainPtr<WKTypeRef>& value)
{
    return booleanValue(value.get());
}

inline WKDictionaryRef dictionaryValue(WKTypeRef value)
{
    return value && WKGetTypeID(value) == WKDictionaryGetTypeID() ? static_cast<WKDictionaryRef>(value) : nullptr;
}

inline double doubleValue(WKTypeRef value)
{
    return value && WKGetTypeID(value) == WKDoubleGetTypeID() ? WKDoubleGetValue(static_cast<WKDoubleRef>(value)) : 0;
}

inline Optional<double> optionalDoubleValue(WKTypeRef value)
{
    return value && WKGetTypeID(value) == WKDoubleGetTypeID() ? makeOptional(WKDoubleGetValue(static_cast<WKDoubleRef>(value))) : WTF::nullopt;
}

inline WKStringRef stringValue(WKTypeRef value)
{
    return value && WKGetTypeID(value) == WKStringGetTypeID() ? static_cast<WKStringRef>(value) : nullptr;
}

inline WTF::String toWTFString(WKTypeRef value)
{
    return toWTFString(stringValue(value));
}

inline uint64_t uint64Value(WKTypeRef value)
{
    return value && WKGetTypeID(value) == WKUInt64GetTypeID() ? WKUInt64GetValue(static_cast<WKUInt64Ref>(value)) : 0;
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

inline uint64_t uint64Value(WKDictionaryRef dictionary, const char* key)
{
    return uint64Value(value(dictionary, key));
}

inline Optional<double> optionalDoubleValue(WKDictionaryRef dictionary, const char* key)
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

template<typename T> void setValue(const WKRetainPtr<WKMutableDictionaryRef>& dictionary, const char* key, const Optional<T>& value)
{
    if (value)
        setValue(dictionary, key, *value);
}

} // namespace WTR
