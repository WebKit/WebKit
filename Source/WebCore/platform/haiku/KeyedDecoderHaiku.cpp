/*
 * Copyright (C) 2017 Haiku, inc
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
#include "KeyedDecoderHaiku.h"

#include <String.h>

#include <wtf/text/CString.h>

namespace WebCore {

std::unique_ptr<KeyedDecoder> KeyedDecoder::decoder(const uint8_t* data, size_t size)
{
    return std::make_unique<KeyedDecoderHaiku>(data, size);
}

KeyedDecoderHaiku::KeyedDecoderHaiku(const uint8_t* data, size_t size)
{
    currentMessage = new BMessage();
    currentMessage->Unflatten((const char*)data);
    m_messageStack.append(currentMessage);
}

KeyedDecoderHaiku::~KeyedDecoderHaiku()
{
    delete currentMessage;
}

bool KeyedDecoderHaiku::decodeBytes(const String& key, const uint8_t*& bytes, size_t& size)
{
    const void* storage;
    ssize_t storeSize;
    if (currentMessage->FindData(key.utf8().data(), B_RAW_TYPE, &storage, &storeSize) == B_OK)
    {
        bytes = (const uint8_t*)storage;
        size = storeSize;
        return true;
    }

    return false;
}

bool KeyedDecoderHaiku::decodeBool(const String& key, bool& result)
{
    return currentMessage->FindBool(key.utf8().data(), &result) == B_OK;
}

bool KeyedDecoderHaiku::decodeUInt32(const String& key, uint32_t& result)
{
    return currentMessage->FindUInt32(key.utf8().data(), (uint32*)&result) == B_OK;
}

bool KeyedDecoderHaiku::decodeInt32(const String& key, int32_t& result)
{
    return currentMessage->FindInt32(key.utf8().data(), (int32*)&result) == B_OK;
}

bool KeyedDecoderHaiku::decodeInt64(const String& key, int64_t& result)
{
    return currentMessage->FindInt64(key.utf8().data(), &result) == B_OK;
}

bool KeyedDecoderHaiku::decodeUInt64(const String& key, uint64_t& result)
{
    return currentMessage->FindUInt64(key.utf8().data(), &result) == B_OK;
}

bool KeyedDecoderHaiku::decodeFloat(const String& key, float& result)
{
    return currentMessage->FindFloat(key.utf8().data(), &result) == B_OK;
}

bool KeyedDecoderHaiku::decodeDouble(const String& key, double& result)
{
    return currentMessage->FindDouble(key.utf8().data(), &result) == B_OK;
}

bool KeyedDecoderHaiku::decodeString(const String& key, String& result)
{
    BString tmp;
    if (currentMessage->FindString(key.utf8().data(), &tmp) == B_OK) {
        result = tmp;
        return true;
    }

    return false;
}

bool KeyedDecoderHaiku::beginObject(const String& key)
{
    BMessage* message = new BMessage();
    if (currentMessage->FindMessage(key.utf8().data(), message) == B_OK) {
        m_messageStack.append(message);
        currentMessage = message;
        return true;
    }
    delete message;
    return false;
}

void KeyedDecoderHaiku::endObject()
{
    delete currentMessage;
    m_messageStack.removeLast();
    currentMessage = m_messageStack.last();
}

bool KeyedDecoderHaiku::beginArray(const String& key)
{
    if (!currentMessage->HasMessage(key.utf8().data()))
        return false;
    m_keyStack.append(std::make_pair(key, 0));
    return true;
}

bool KeyedDecoderHaiku::beginArrayElement()
{
    BMessage* message = new BMessage();
    if (currentMessage->FindMessage(m_keyStack.last().first.utf8().data(),
            m_keyStack.last().second, message) == B_OK) {
        m_messageStack.append(message);
        currentMessage = message;
        return true;
    }
    delete message;
    return false;
}

void KeyedDecoderHaiku::endArrayElement()
{
    m_keyStack.last().second++;
    delete currentMessage;
    m_messageStack.removeLast();
    currentMessage = m_messageStack.last();
}

void KeyedDecoderHaiku::endArray()
{
    m_keyStack.removeLast();
}

} // namespace WebCore

