/*
 * Copyright (C) 2006, 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Google Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ContentType_h
#define ContentType_h

#include <wtf/text/WTFString.h>

namespace WebCore {

class ContentType {
public:
    explicit ContentType(String&& type);
    explicit ContentType(const String& type);
    ContentType() = default;

    static const String& codecsParameter();
    static const String& profilesParameter();

    String parameter(const String& parameterName) const;
    String containerType() const;
    Vector<String> codecs() const;
    Vector<String> profiles() const;
    const String& raw() const { return m_type; }
    bool isEmpty() const { return m_type.isEmpty(); }

    String toJSONString() const;

private:
    String m_type;
};

} // namespace WebCore

namespace WTF {
template<typename Type> struct LogArgument;

template <>
struct LogArgument<WebCore::ContentType> {
    static String toString(const WebCore::ContentType& type)
    {
        return type.toJSONString();
    }
};

} // namespace WTF

#endif // ContentType_h
