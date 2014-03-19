/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "KeyedDecoder.h"

#include <wtf/text/WTFString.h>

namespace WebKit {

KeyedDecoder::KeyedDecoder(const uint8_t* data, size_t size)
{
    auto cfData = adoptCF(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, data, size, kCFAllocatorNull));
    auto rootDictionary = adoptCF(CFPropertyListCreateWithData(kCFAllocatorDefault, cfData.get(), kCFPropertyListImmutable, nullptr, nullptr));

    if (rootDictionary && CFGetTypeID(rootDictionary.get()) == CFDictionaryGetTypeID())
        m_rootDictionary = adoptCF(static_cast<CFDictionaryRef>(rootDictionary.leakRef()));
    else
        m_rootDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    m_dictionaryStack.append(m_rootDictionary.get());
}

KeyedDecoder::~KeyedDecoder()
{
    ASSERT(m_dictionaryStack.size() == 1);
    ASSERT(m_dictionaryStack.last() == m_rootDictionary);
    ASSERT(m_arrayStack.isEmpty());
    ASSERT(m_arrayIndexStack.isEmpty());
}

bool KeyedDecoder::decodeBool(const String& key, bool& result)
{
    CFBooleanRef boolean = static_cast<CFBooleanRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!boolean || CFGetTypeID(boolean) != CFBooleanGetTypeID())
        return false;

    result = CFBooleanGetValue(boolean);
    return true;
}

bool KeyedDecoder::decodeUInt32(const String& key, uint32_t& result)
{
    return decodeInt32(key, reinterpret_cast<int32_t&>(result));
}

bool KeyedDecoder::decodeInt32(const String& key, int32_t& result)
{
    CFNumberRef number = static_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number || CFGetTypeID(number) != CFNumberGetTypeID())
        return false;

    return CFNumberGetValue(number, kCFNumberSInt32Type, &result);
}

bool KeyedDecoder::decodeInt64(const String& key, int64_t& result)
{
    CFNumberRef number = static_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number || CFGetTypeID(number) != CFNumberGetTypeID())
        return false;

    return CFNumberGetValue(number, kCFNumberSInt64Type, &result);
}

bool KeyedDecoder::decodeFloat(const String& key, float& result)
{
    CFNumberRef number = static_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number || CFGetTypeID(number) != CFNumberGetTypeID())
        return false;

    return CFNumberGetValue(number, kCFNumberFloatType, &result);
}

bool KeyedDecoder::decodeDouble(const String& key, double& result)
{
    CFNumberRef number = static_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number || CFGetTypeID(number) != CFNumberGetTypeID())
        return false;

    return CFNumberGetValue(number, kCFNumberDoubleType, &result);
}

bool KeyedDecoder::decodeString(const String& key, String& result)
{
    CFStringRef string = static_cast<CFStringRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!string || CFGetTypeID(string) != CFStringGetTypeID())
        return false;

    result = string;
    return true;
}

bool KeyedDecoder::beginObject(const String& key)
{
    CFDictionaryRef dictionary = static_cast<CFDictionaryRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!dictionary || CFGetTypeID(dictionary) != CFDictionaryGetTypeID())
        return false;

    m_dictionaryStack.append(dictionary);
    return true;
}

void KeyedDecoder::endObject()
{
    m_dictionaryStack.removeLast();
}

bool KeyedDecoder::beginArray(const String& key)
{
    CFArrayRef array = static_cast<CFArrayRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!array || CFGetTypeID(array) != CFArrayGetTypeID())
        return false;

    for (CFIndex i = 0; i < CFArrayGetCount(array); ++i) {
        CFTypeRef object = CFArrayGetValueAtIndex(array, i);
        if (CFGetTypeID(object) != CFDictionaryGetTypeID())
            return false;
    }

    m_arrayStack.append(array);
    m_arrayIndexStack.append(0);
    return true;
}

bool KeyedDecoder::beginArrayElement()
{
    if (m_arrayIndexStack.last() >= CFArrayGetCount(m_arrayStack.last()))
        return false;

    CFDictionaryRef dictionary = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(m_arrayStack.last(), m_arrayIndexStack.last()++));
    m_dictionaryStack.append(dictionary);
    return true;
}

void KeyedDecoder::endArrayElement()
{
    m_dictionaryStack.removeLast();
}

void KeyedDecoder::endArray()
{
    m_arrayStack.removeLast();
    m_arrayIndexStack.removeLast();
}

} // namespace WebKit
