/*
 * Copyright (C) 2006, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#pragma once

#include <wtf/text/WTFString.h>

namespace WTF {
class URL;
}

namespace WebCore {

class ContentType {
public:
    NODELETE WEBCORE_EXPORT explicit ContentType(RefPtr<StringImpl>&& type);
    NODELETE WEBCORE_EXPORT explicit ContentType(StringImpl* type);
    NODELETE WEBCORE_EXPORT ContentType(StringImpl* type, bool); // Use by the IPC serializer.
    ContentType() = default;

    WEBCORE_EXPORT static ContentType fromURL(const WTF::URL&);

    WEBCORE_EXPORT static const RefPtr<StringImpl>& codecsParameter();
    static const RefPtr<StringImpl>& profilesParameter();

    WEBCORE_EXPORT RefPtr<StringImpl> parameter(StringImpl* parameterName) const;
    WEBCORE_EXPORT RefPtr<StringImpl> containerType() const;
    Vector<RefPtr<StringImpl>> codecs() const;
    Vector<RefPtr<StringImpl>> profiles() const;
    const RefPtr<StringImpl>& raw() const LIFETIME_BOUND { return m_type; }
    bool isEmpty() const { return WTF::isEmpty(m_type); }

    bool typeWasInferredFromExtension() const { return m_typeWasInferredFromExtension; }

    WEBCORE_EXPORT RefPtr<StringImpl> toJSONString() const;
    bool operator==(const ContentType& other) const { return equal(raw(), other.raw()); }

    ContentType isolatedCopy() const & { return { WTF::isolatedCopy(m_type), m_typeWasInferredFromExtension }; }
    ContentType isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(m_type)), m_typeWasInferredFromExtension }; }

private:
    RefPtr<StringImpl> m_type;
    bool m_typeWasInferredFromExtension { false };
};

} // namespace WebCore

namespace WTF {
template<typename Type> struct LogArgument;

template <>
struct LogArgument<WebCore::ContentType> {
    static RefPtr<StringImpl> toString(const WebCore::ContentType& type)
    {
        return type.toJSONString();
    }
};

} // namespace WTF
