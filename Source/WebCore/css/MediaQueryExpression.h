/*
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
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

#include "CSSParserTokenRange.h"
#include "CSSValue.h"
#include <memory>

namespace WTF {
class TextStream;
}

namespace WebCore {
    
struct MediaQueryParserContext;

class MediaQueryExpression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit MediaQueryExpression(const String& mediaFeature, CSSParserTokenRange&, MediaQueryParserContext&);

    const AtomString& mediaFeature() const;
    CSSValue* value() const;

    bool isValid() const;

    String serialize() const;

    bool operator==(const MediaQueryExpression&) const;

private:
    AtomString m_mediaFeature;
    RefPtr<CSSValue> m_value;
    bool m_isValid { false };
    mutable String m_serializationCache;
};

inline const AtomString& MediaQueryExpression::mediaFeature() const
{
    return m_mediaFeature;
}

inline CSSValue* MediaQueryExpression::value() const
{
    return m_value.get();
}

inline bool MediaQueryExpression::operator==(const MediaQueryExpression& other) const
{
    return other.m_mediaFeature == m_mediaFeature
        && ((!m_value && !other.m_value) || (m_value && other.m_value && *m_value == *other.m_value));
}

inline bool MediaQueryExpression::isValid() const
{
    return m_isValid;
}

WTF::TextStream& operator<<(WTF::TextStream&, const MediaQueryExpression&);

} // namespace
