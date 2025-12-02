/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CSSFontFaceSrcValue.h"

#include "CSSMarkup.h"
#include "CSSSerializationContext.h"
#include "CachedFont.h"
#include "CachedFontLoadRequest.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "FontCustomPlatformData.h"
#include "SVGFontFaceElement.h"
#include "ScriptExecutionContext.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSFontFaceSrcLocalValue::CSSFontFaceSrcLocalValue(AtomString&& fontFaceName)
    : CSSValue(ClassType::FontFaceSrcLocal)
    , m_fontFaceName(WTFMove(fontFaceName))
{
}

Ref<CSSFontFaceSrcLocalValue> CSSFontFaceSrcLocalValue::create(AtomString fontFaceName)
{
    return adoptRef(*new CSSFontFaceSrcLocalValue { WTFMove(fontFaceName) });
}

CSSFontFaceSrcLocalValue::~CSSFontFaceSrcLocalValue() = default;

SVGFontFaceElement* CSSFontFaceSrcLocalValue::svgFontFaceElement() const
{
    return m_element.get();
}

void CSSFontFaceSrcLocalValue::setSVGFontFaceElement(SVGFontFaceElement& element)
{
    m_element = element;
}

String CSSFontFaceSrcLocalValue::customCSSText(const CSS::SerializationContext&) const
{
    return makeString("local("_s, serializeString(m_fontFaceName), ')');
}

bool CSSFontFaceSrcLocalValue::equals(const CSSFontFaceSrcLocalValue& other) const
{
    return m_fontFaceName == other.m_fontFaceName;
}

CSSFontFaceSrcResourceValue::CSSFontFaceSrcResourceValue(CSS::URL&& location, String&& format, Vector<FontTechnology>&& technologies)
    : CSSValue(ClassType::FontFaceSrcResource)
    , m_location(CSS::resolve(WTFMove(location)))
    , m_format(WTFMove(format))
    , m_technologies(WTFMove(technologies))
{
}

Ref<CSSFontFaceSrcResourceValue> CSSFontFaceSrcResourceValue::create(CSS::URL location, String format, Vector<FontTechnology>&& technologies)
{
    return adoptRef(*new CSSFontFaceSrcResourceValue { WTFMove(location), WTFMove(format), WTFMove(technologies) });
}

RefPtr<FontLoadRequest> CSSFontFaceSrcResourceValue::fontLoadRequest(ScriptExecutionContext& context, bool isInitiatingElementInUserAgentShadowTree)
{
    if (m_cachedFont)
        return CachedFontLoadRequest::create(*m_cachedFont, context);

    bool isFormatSVG;
    if (m_format.isEmpty()) {
        // In order to avoid conflicts with the old WinIE style of font-face, if there is no format specified,
        // we check to see if the URL ends with .eot. We will not try to load those.
        if (m_location.resolved.lastPathComponent().endsWithIgnoringASCIICase(".eot"_s) && !m_location.resolved.protocolIsData())
            return nullptr;
        isFormatSVG = false;
    } else {
        isFormatSVG = equalLettersIgnoringASCIICase(m_format, "svg"_s);
        if (!FontCustomPlatformData::supportsFormat(m_format))
            return nullptr;
    }

    if (!m_technologies.isEmpty()) {
        for (auto technology : m_technologies) {
            if (!FontCustomPlatformData::supportsTechnology(technology))
                return nullptr;
        }
    }

    RefPtr request = context.fontLoadRequest(m_location.resolved.string(), isFormatSVG, isInitiatingElementInUserAgentShadowTree, m_location.modifiers.loadedFromOpaqueSource);
    if (RefPtr cachedRequest = dynamicDowncast<CachedFontLoadRequest>(request.get()))
        m_cachedFont = &cachedRequest->cachedFont();

    return request;
}

bool CSSFontFaceSrcResourceValue::customTraverseSubresources(NOESCAPE const Function<bool(const CachedResource&)>& handler) const
{
    return m_cachedFont && handler(*m_cachedFont);
}

bool CSSFontFaceSrcResourceValue::customMayDependOnBaseURL() const
{
    return WebCore::CSS::mayDependOnBaseURL(m_location);
}

String CSSFontFaceSrcResourceValue::customCSSText(const CSS::SerializationContext& context) const
{
    StringBuilder builder;

    CSS::serializationForCSS(builder, context, m_location);

    if (!m_format.isEmpty())
        builder.append(" format("_s, serializeString(m_format), ')');
    if (!m_technologies.isEmpty()) {
        builder.append(" tech("_s);
        for (size_t i = 0; i < m_technologies.size(); ++i) {
            if (i)
                builder.append(", "_s);
            builder.append(cssTextFromFontTech(m_technologies[i]));
        }
        builder.append(')');
    }

    return builder.toString();
}

bool CSSFontFaceSrcResourceValue::equals(const CSSFontFaceSrcResourceValue& other) const
{
    return m_location == other.m_location
        && m_format == other.m_format
        && m_technologies == other.m_technologies;
}

}
