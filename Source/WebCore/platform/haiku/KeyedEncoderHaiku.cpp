/*
 * Copyright (C) 2017 Haiku, inc.
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
#include "KeyedEncoderHaiku.h"

#include "SharedBuffer.h"
#include <wtf/text/CString.h>

namespace WebCore {

std::unique_ptr<KeyedEncoder> KeyedEncoder::encoder()
{
    return std::make_unique<KeyedEncoderHaiku>();
}

KeyedEncoderHaiku::KeyedEncoderHaiku()
{
    currentMessage = &root;
    m_messageStack.append(std::make_pair("root", currentMessage));
}

KeyedEncoderHaiku::~KeyedEncoderHaiku()
{
}

RefPtr<WebCore::SharedBuffer> KeyedEncoderHaiku::finishEncoding()
{
    class SharedBufferIO: public BDataIO {
        public:
        SharedBufferIO(SharedBuffer* buffer)
            : fBuffer(buffer)
        {
        }

        ssize_t Write(const void* data, size_t size) override {
            fBuffer->append((const char*)data, size);
            return size;
        }

        private:
            SharedBuffer* fBuffer;
    };


    RefPtr<WebCore::SharedBuffer> buffer = SharedBuffer::create();
    SharedBufferIO sio(buffer.get());
    root.Flatten(&sio);

    return buffer;
}

void KeyedEncoderHaiku::encodeBytes(const String& key, const uint8_t* data, size_t size)
{
    currentMessage->AddData(key.utf8().data(), B_RAW_TYPE, data, size, false);
}

void KeyedEncoderHaiku::encodeBool(const String& key, bool value)
{
    currentMessage->AddBool(key.utf8().data(), value);
}

void KeyedEncoderHaiku::encodeUInt32(const String& key, uint32_t value)
{
    currentMessage->AddUInt32(key.utf8().data(), value);
}

void KeyedEncoderHaiku::encodeInt32(const String& key, int32_t value)
{
    currentMessage->AddInt32(key.utf8().data(), value);
}

void KeyedEncoderHaiku::encodeUInt64(const String& key, uint64_t value)
{
    currentMessage->AddUInt64(key.utf8().data(), value);
}

void KeyedEncoderHaiku::encodeInt64(const String& key, int64_t value)
{
    currentMessage->AddInt64(key.utf8().data(), value);
}

void KeyedEncoderHaiku::encodeFloat(const String& key, float value)
{
    currentMessage->AddFloat(key.utf8().data(), value);
}

void KeyedEncoderHaiku::encodeDouble(const String& key, double value)
{
    currentMessage->AddDouble(key.utf8().data(), value);
}

void KeyedEncoderHaiku::encodeString(const String& key, const String& value)
{
    currentMessage->AddString(key.utf8().data(), value.utf8().data());
}


void KeyedEncoderHaiku::beginObject(const String& key)
{
    currentMessage = new BMessage('kobj');
    m_messageStack.append(std::make_pair(key, currentMessage));
}

void KeyedEncoderHaiku::endObject()
{
    std::pair<String, BMessage*> pair = m_messageStack.takeLast();
    BMessage* parentMessage = m_messageStack.last().second;

    parentMessage->AddMessage(pair.first.utf8().data(), pair.second);
    delete currentMessage;
    currentMessage = parentMessage;
}

void KeyedEncoderHaiku::beginArray(const String& key)
{
    m_messageStack.append(std::make_pair(key, nullptr));
}

void KeyedEncoderHaiku::beginArrayElement()
{
    currentMessage = new BMessage('karr');
    m_messageStack.last().second = currentMessage;
}

void KeyedEncoderHaiku::endArrayElement()
{
    std::pair<String, BMessage*> pair = m_messageStack.last();

    BMessage* parentMessage = m_messageStack.at(m_messageStack.size() - 2).second;

    parentMessage->AddMessage(pair.first.utf8().data(), pair.second);
    delete currentMessage;
    currentMessage = parentMessage;
}

void KeyedEncoderHaiku::endArray()
{
    m_messageStack.removeLast();
    currentMessage = m_messageStack.last().second;
}

} // namespace WebCore
