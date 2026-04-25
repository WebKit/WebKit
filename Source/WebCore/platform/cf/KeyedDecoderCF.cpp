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

#include <wtf/TZoneMallocInlines.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(KeyedDecoderCF);

std::unique_ptr<KeyedDecoder> KeyedDecoder::decoder(std::span<const uint8_t> data)
{
    return makeUnique<KeyedDecoderCF>(data);
}

KeyedDecoderCF::KeyedDecoderCF(std::span<const uint8_t> data)
{
    auto cfData = toCFDataNoCopy(data, kCFAllocatorNull);
    auto cfPropertyList = adoptCF(CFPropertyListCreateWithData(kCFAllocatorDefault, cfData.get(), kCFPropertyListImmutable, nullptr, nullptr));

    if (dynamic_cf_cast<CFDictionaryRef>(cfPropertyList.get()))
        lazyInitialize(m_rootDictionary, adoptCF(static_cast<CFDictionaryRef>(cfPropertyList.leakRef())));
    else
        lazyInitialize(m_rootDictionary, adoptCF(CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)));
    m_dictionaryStack.append(m_rootDictionary.get());
}

KeyedDecoderCF::~KeyedDecoderCF()
{
    ASSERT(m_dictionaryStack.size() == 1);
    ASSERT(m_dictionaryStack.last() == m_rootDictionary);
    ASSERT(m_arrayStack.isEmpty());
    ASSERT(m_arrayIndexStack.isEmpty());
}

bool KeyedDecoderCF::decodeBytes(const String& key, std::span<const uint8_t>& bytes)
{
    RetainPtr data = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!data)
        return false;

    bytes = span(data.get());
    return true;
}

bool KeyedDecoderCF::decodeBool(const String& key, bool& result)
{
    RetainPtr boolean = dynamic_cf_cast<CFBooleanRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!boolean)
        return false;

    result = CFBooleanGetValue(boolean.get());
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
    RetainPtr number = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number)
        return false;

    return CFNumberGetValue(number.get(), kCFNumberSInt32Type, &result);
}

bool KeyedDecoderCF::decodeInt64(const String& key, int64_t& result)
{
    RetainPtr number = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number)
        return false;

    return CFNumberGetValue(number.get(), kCFNumberSInt64Type, &result);
}

bool KeyedDecoderCF::decodeFloat(const String& key, float& result)
{
    RetainPtr number = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number)
        return false;

    return CFNumberGetValue(number.get(), kCFNumberFloatType, &result);
}

bool KeyedDecoderCF::decodeDouble(const String& key, double& result)
{
    RetainPtr number = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!number)
        return false;

    return CFNumberGetValue(number.get(), kCFNumberDoubleType, &result);
}

bool KeyedDecoderCF::decodeString(const String& key, String& result)
{
    RetainPtr string = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!string)
        return false;

    result = string.get();
    return true;
}

bool KeyedDecoderCF::beginObject(const String& key)
{
    RetainPtr dictionary = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!dictionary)
        return false;

    m_dictionaryStack.append(dictionary.get());
    return true;
}

void KeyedDecoderCF::endObject()
{
    m_dictionaryStack.removeLast();
}

bool KeyedDecoderCF::beginArray(const String& key)
{
    RetainPtr array = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(m_dictionaryStack.last(), key.createCFString().get()));
    if (!array)
        return false;

    for (CFIndex i = 0; i < CFArrayGetCount(array.get()); ++i) {
        CFTypeRef object = CFArrayGetValueAtIndex(array.get(), i);
        if (CFGetTypeID(object) != CFDictionaryGetTypeID())
            return false;
    }

    m_arrayStack.append(array.get());
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
