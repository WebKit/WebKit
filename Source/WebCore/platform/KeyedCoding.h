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

#ifndef KeyedCoding_h
#define KeyedCoding_h

#include <wtf/Forward.h>

namespace WebCore {

class KeyedEncoder {
protected:
    virtual ~KeyedEncoder() { }

public:
    virtual void encodeBytes(const String& key, const uint8_t*, size_t) = 0;
    virtual void encodeBool(const String& key, bool) = 0;
    virtual void encodeUInt32(const String& key, uint32_t) = 0;
    virtual void encodeInt32(const String& key, int32_t) = 0;
    virtual void encodeInt64(const String& key, int64_t) = 0;
    virtual void encodeFloat(const String& key, float) = 0;
    virtual void encodeDouble(const String& key, double) = 0;
    virtual void encodeString(const String& key, const String&) = 0;

    template<typename T>
    void encodeEnum(const String& key, T value)
    {
        static_assert(std::is_enum<T>::value, "T must be an enum type");

        encodeInt64(key, static_cast<int64_t>(value));
    }

    template<typename T, typename F>
    void encodeObject(const String& key, const T& object, F&& function)
    {
        beginObject(key);
        function(*this, object);
        endObject();
    }

    template<typename T, typename F>
    void encodeConditionalObject(const String& key, const T* object, F&& function)
    {
        if (!object)
            return;

        encodeObject(key, *object, std::forward<F>(function));
    }

    template<typename T, typename F>
    void encodeObjects(const String& key, T begin, T end, F&& function)
    {
        if (begin == end)
            return;

        beginArray(key);
        for (T it = begin; it != end; ++it) {
            beginArrayElement();
            function(*this, *it);
            endArrayElement();
        }
        endArray();
    }

private:
    virtual void beginObject(const String& key) = 0;
    virtual void endObject() = 0;

    virtual void beginArray(const String& key) = 0;
    virtual void beginArrayElement() = 0;
    virtual void endArrayElement() = 0;
    virtual void endArray() = 0;
};

} // namespace WebCore

#endif // KeyedCoding_h
