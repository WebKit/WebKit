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

#ifndef MediaQueryExp_h
#define MediaQueryExp_h

#include "CSSValue.h"
#include "MediaFeatureNames.h"
#include <memory>
#include <wtf/BumpArena.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class CSSParserValueList;

class MediaQueryExp {
    WTF_MAKE_BUMPARENA_ALLOCATED;
public:
    explicit MediaQueryExp(const AtomicString& mediaFeature = emptyAtom, CSSParserValueList* values = nullptr);

    const AtomicString& mediaFeature() const;
    CSSValue* value() const;

    bool isValid() const;
    bool isViewportDependent() const;

    String serialize() const;

    bool operator==(const MediaQueryExp&) const;

private:
    AtomicString m_mediaFeature;
    RefPtr<CSSValue> m_value;
    bool m_isValid { false };
    mutable String m_serializationCache;
};

inline const AtomicString& MediaQueryExp::mediaFeature() const
{
    return m_mediaFeature;
}

inline CSSValue* MediaQueryExp::value() const
{
    return m_value.get();
}

inline bool MediaQueryExp::operator==(const MediaQueryExp& other) const
{
    return (other.m_mediaFeature == m_mediaFeature)
        && ((!other.m_value && !m_value)
            || (other.m_value && m_value && other.m_value->equals(*m_value)));
}

inline bool MediaQueryExp::isValid() const
{
    return m_isValid;
}

inline bool MediaQueryExp::isViewportDependent() const
{
    return m_mediaFeature == MediaFeatureNames::widthMediaFeature
        || m_mediaFeature == MediaFeatureNames::heightMediaFeature
        || m_mediaFeature == MediaFeatureNames::min_widthMediaFeature
        || m_mediaFeature == MediaFeatureNames::min_heightMediaFeature
        || m_mediaFeature == MediaFeatureNames::max_widthMediaFeature
        || m_mediaFeature == MediaFeatureNames::max_heightMediaFeature
        || m_mediaFeature == MediaFeatureNames::orientationMediaFeature
        || m_mediaFeature == MediaFeatureNames::aspect_ratioMediaFeature
        || m_mediaFeature == MediaFeatureNames::min_aspect_ratioMediaFeature
        || m_mediaFeature == MediaFeatureNames::max_aspect_ratioMediaFeature;
}

} // namespace

#endif
