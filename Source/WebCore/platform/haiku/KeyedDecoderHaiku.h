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

#ifndef KeyedDecoderHaiku_h
#define KeyedDecoderHaiku_h

#include "KeyedCoding.h"

#include <Message.h>

#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class KeyedDecoderHaiku final : public KeyedDecoder {
public:
    KeyedDecoderHaiku(const uint8_t* data, size_t);
    virtual ~KeyedDecoderHaiku() override;

private:
    virtual bool decodeBytes(const String& key, const uint8_t*&, size_t&) override;
    virtual bool decodeBool(const String& key, bool&) override;
    virtual bool decodeUInt32(const String& key, uint32_t&) override;
    virtual bool decodeInt32(const String& key, int32_t&) override;
    virtual bool decodeUInt64(const String& key, uint64_t&) override;
    virtual bool decodeInt64(const String& key, int64_t&) override;
    virtual bool decodeFloat(const String& key, float&) override;
    virtual bool decodeDouble(const String& key, double&) override;
    virtual bool decodeString(const String& key, String&) override;

    virtual bool beginObject(const String& key) override;
    virtual void endObject() override;

    virtual bool beginArray(const String& key) override;
    virtual bool beginArrayElement() override;
    virtual void endArrayElement() override;
    virtual void endArray() override;

    Vector<BMessage*, 16> m_messageStack;
    Vector<std::pair<String, int>, 16> m_keyStack;
    BMessage* currentMessage;
};

} // namespace WebCore

#endif // KeyedDecoderGlib_h

