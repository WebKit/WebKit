/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef APIString_h
#define APIString_h

#include "APIObject.h"
#include <wtf/Ref.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/UTF8Conversion.h>

namespace API {

class String final : public ObjectImpl<Object::Type::String> {
public:
    static Ref<String> createNull()
    {
        return adoptRef(*new String);
    }

    static Ref<String> create(WTF::String&& string)
    {
        return adoptRef(*new String(string.isNull() ? WTF::String(StringImpl::empty()) : string.isolatedCopy()));
    }

    static Ref<String> create(const WTF::String& string)
    {
        return create(string.isolatedCopy());
    }

    virtual ~String()
    {
    }

    WTF::StringView stringView() const { return m_string; }
    WTF::String string() const { return m_string.isolatedCopy(); }

private:
    String()
        : m_string()
    {
    }

    String(WTF::String&& string)
        : m_string(WTFMove(string))
    {
        ASSERT(!m_string.isNull());
        ASSERT(m_string.isSafeToSendToAnotherThread());
    }

    const WTF::String m_string;
};

} // namespace WebKit

#endif // APIString_h
