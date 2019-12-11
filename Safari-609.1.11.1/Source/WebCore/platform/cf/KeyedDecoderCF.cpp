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
#include "KeyedDecoderCF.h"

#include <wtf/cf/TypeCastsCF.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

std::unique_ptr<KeyedDecoder> KeyedDecoder::decoder(const uint8_t* data, size_t size)
{
    return makeUnique<KeyedDecoderCF>(data, size);
}

KeyedDecoderCF::KeyedDecoderCF(const uint8_t* data, size_t size)
{
    auto cfData = adoptCF(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, data, size, kCFAllocatorNull));
    auto cfPropertyList = adoptCF(CFPropertyListCreateWithData(kCFAllocatorDefault, cfData.get(), kCFPropertyListImmutable, nullptr, nullptr));

    if (dynamic_cf_cast<CFDictionaryRef>(cfPropertyList.get()))
        m_rootDictionary = adoptCF(static_cast<CFDictionaryRef>(cfPropertyList.leakRef()));
    else
        m_rootDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    m_dictionaryStack.append(m_rootDictionary.get());
}

KeyedDecoderCF::~KeyedDecoderCF()
{
    ASSERT(m_dictionaryStack.size() == 1);
    ASSERT(m_dictionaryStack.last() == m_rootDictionary);
    ASSERT(m_arrayStack.isEmpty());
    ASSERT(m_arrayIndexStack.isEmpty());
}

bool KeyedDecoderCF::decodeBytes(const String& key, const uint8_t*& bytes, size_t& size)
{
    auto data = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!data)
        return false;

    bytes = CFDataGetBytePtr(data);
    size = CFDataGetLength(data);
    return true;
}

bool KeyedDecoderCF::decodeBool(const String& key, bool& result)
{
    auto boolean = dynamic_cf_cast<CFBooleanRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!boolean)
        return false;

    result = CFBooleanGetValue(boolean);
    return true;
}

bool KeyedDecoderCF::decodeUInt32(const String& key, uint32_t& result)
{
    return decodeInt32(key, reinterpret_cast<int32_t&>(result));
}
    
bool KeyedDecoderCF::decodeUInt64(const String& key, uint64_t& result)
{
    return decodeInt64(key, reinterpret_cast<int64_t&>(result));
}

bool KeyedDecoderCF::decodeInt32(const String& key, int32_t& result)
{
    auto number = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number)
        return false;

    return CFNumberGetValue(number, kCFNumberSInt32Type, &result);
}

bool KeyedDecoderCF::decodeInt64(const String& key, int64_t& result)
{
    auto number = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number)
        return false;

    return CFNumberGetValue(number, kCFNumberSInt64Type, &result);
}

bool KeyedDecoderCF::decodeFloat(const String& key, float& result)
{
    auto number = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number)
        return false;

    return CFNumberGetValue(number, kCFNumberFloatType, &result);
}

bool KeyedDecoderCF::decodeDouble(const String& key, double& result)
{
    auto number = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number)
        return false;

    return CFNumberGetValue(number, kCFNumberDoubleType, &result);
}

bool KeyedDecoderCF::decodeString(const String& key, String& result)
{
    auto string = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!string)
        return false;

    result = string;
    return true;
}

bool KeyedDecoderCF::beginObject(const String& key)
{
    auto dictionary = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!dictionary)
        return false;

    m_dictionaryStack.append(dictionary);
    return true;
}

void KeyedDecoderCF::endObject()
{
    m_dictionaryStack.removeLast();
}

bool KeyedDecoderCF::beginArray(const String& key)
{
    auto array = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!array)
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

bool KeyedDecoderCF::beginArrayElement()
{
    if (m_arrayIndexStack.last() >= CFArrayGetCount(m_arrayStack.last()))
        return false;

    auto dictionary = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(m_arrayStack.last(), m_arrayIndexStack.last()++));
    m_dictionaryStack.append(dictionary);
    return true;
}

void KeyedDecoderCF::endArrayElement()
{
    m_dictionaryStack.removeLast();
}

void KeyedDecoderCF::endArray()
{
    m_arrayStack.removeLast();
    m_arrayIndexStack.removeLast();
}

} // namespace WebCore
