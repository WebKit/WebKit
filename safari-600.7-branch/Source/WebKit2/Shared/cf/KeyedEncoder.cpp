/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "KeyedEncoder.h"

#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

static RetainPtr<CFMutableDictionaryRef> createDictionary()
{
    return adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

KeyedEncoder::KeyedEncoder()
    : m_rootDictionary(createDictionary())
{
    m_dictionaryStack.append(m_rootDictionary.get());
}
    
KeyedEncoder::~KeyedEncoder()
{
    ASSERT(m_dictionaryStack.size() == 1);
    ASSERT(m_dictionaryStack.last() == m_rootDictionary);
    ASSERT(m_arrayStack.isEmpty());
}

void KeyedEncoder::encodeBytes(const String& key, const uint8_t* bytes, size_t size)
{
    auto data = adoptCF(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, bytes, size, kCFAllocatorNull));
    CFDictionarySetValue(m_dictionaryStack.last(), key.createCFString().get(), data.get());
}

void KeyedEncoder::encodeBool(const String& key, bool value)
{
    CFDictionarySetValue(m_dictionaryStack.last(), key.createCFString().get(), value ? kCFBooleanTrue : kCFBooleanFalse);
}

void KeyedEncoder::encodeUInt32(const String& key, uint32_t value)
{
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value));
    CFDictionarySetValue(m_dictionaryStack.last(), key.createCFString().get(), number.get());
}

void KeyedEncoder::encodeInt32(const String& key, int32_t value)
{
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value));
    CFDictionarySetValue(m_dictionaryStack.last(), key.createCFString().get(), number.get());
}

void KeyedEncoder::encodeInt64(const String& key, int64_t value)
{
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value));
    CFDictionarySetValue(m_dictionaryStack.last(), key.createCFString().get(), number.get());
}

void KeyedEncoder::encodeFloat(const String& key, float value)
{
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &value));
    CFDictionarySetValue(m_dictionaryStack.last(), key.createCFString().get(), number.get());
}

void KeyedEncoder::encodeDouble(const String& key, double value)
{
    auto number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &value));
    CFDictionarySetValue(m_dictionaryStack.last(), key.createCFString().get(), number.get());
}

void KeyedEncoder::encodeString(const String& key, const String& value)
{
    CFDictionarySetValue(m_dictionaryStack.last(), key.createCFString().get(), value.createCFString().get());
}

void KeyedEncoder::beginObject(const String& key)
{
    auto dictionary = createDictionary();
    CFDictionarySetValue(m_dictionaryStack.last(), key.createCFString().get(), dictionary.get());

    m_dictionaryStack.append(dictionary.get());
}

void KeyedEncoder::endObject()
{
    m_dictionaryStack.removeLast();
}

void KeyedEncoder::beginArray(const String& key)
{
    auto array = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
    CFDictionarySetValue(m_dictionaryStack.last(), key.createCFString().get(), array.get());

    m_arrayStack.append(array.get());
}

void KeyedEncoder::beginArrayElement()
{
    auto dictionary = createDictionary();
    CFArrayAppendValue(m_arrayStack.last(), dictionary.get());

    m_dictionaryStack.append(dictionary.get());
}

void KeyedEncoder::endArrayElement()
{
    m_dictionaryStack.removeLast();
}

void KeyedEncoder::endArray()
{
    m_arrayStack.removeLast();
}

PassRefPtr<SharedBuffer> KeyedEncoder::finishEncoding()
{
    RetainPtr<CFDataRef> data = adoptCF(CFPropertyListCreateData(kCFAllocatorDefault, m_rootDictionary.get(), kCFPropertyListBinaryFormat_v1_0, 0, nullptr));
    if (!data)
        return nullptr;

    return SharedBuffer::wrapCFData(data.get());
}

} // namespace WebKit
