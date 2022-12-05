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
#include "CachedFont.h"
#include "CachedFontLoadRequest.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "FontCustomPlatformData.h"
#include "SVGFontFaceElement.h"
#include "ScriptExecutionContext.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSFontFaceSrcLocalValue::CSSFontFaceSrcLocalValue(AtomString&& fontFaceName)
    : CSSValue(FontFaceSrcLocalClass)
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
    m_element = &element;
}

String CSSFontFaceSrcLocalValue::customCSSText() const
{
    return makeString("local(", serializeString(m_fontFaceName), ')');
}

bool CSSFontFaceSrcLocalValue::equals(const CSSFontFaceSrcLocalValue& other) const
{
    return m_fontFaceName == other.m_fontFaceName;
}

CSSFontFaceSrcResourceValue::CSSFontFaceSrcResourceValue(ResolvedURL&& location, String&& format, LoadedFromOpaqueSource source)
    : CSSValue(FontFaceSrcResourceClass)
    , m_location(WTFMove(location))
    , m_format(WTFMove(format))
    , m_loadedFromOpaqueSource(source)
{
}

Ref<CSSFontFaceSrcResourceValue> CSSFontFaceSrcResourceValue::create(ResolvedURL location, String format, LoadedFromOpaqueSource source)
{
    return adoptRef(*new CSSFontFaceSrcResourceValue { WTFMove(location), WTFMove(format), source });
}

std::unique_ptr<FontLoadRequest> CSSFontFaceSrcResourceValue::fontLoadRequest(ScriptExecutionContext& context, bool isInitiatingElementInUserAgentShadowTree)
{
    if (m_cachedFont)
        return makeUnique<CachedFontLoadRequest>(*m_cachedFont);

    bool isFormatSVG;
    if (m_format.isEmpty()) {
        // In order to avoid conflicts with the old WinIE style of font-face, if there is no format specified,
        // we check to see if the URL ends with .eot. We will not try to load those.
        if (m_location.resolvedURL.lastPathComponent().endsWithIgnoringASCIICase(".eot"_s) && !m_location.resolvedURL.protocolIsData())
            return nullptr;
        isFormatSVG = false;
    } else {
        isFormatSVG = equalLettersIgnoringASCIICase(m_format, "svg"_s);
        if (!isFormatSVG && !FontCustomPlatformData::supportsFormat(m_format))
            return nullptr;
    }

    auto request = context.fontLoadRequest(m_location.resolvedURL.string(), isFormatSVG, isInitiatingElementInUserAgentShadowTree, m_loadedFromOpaqueSource);
    if (auto* cachedRequest = dynamicDowncast<CachedFontLoadRequest>(request.get()))
        m_cachedFont = &cachedRequest->cachedFont();

    return request;
}

bool CSSFontFaceSrcResourceValue::customTraverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    return m_cachedFont && handler(*m_cachedFont);
}

String CSSFontFaceSrcResourceValue::customCSSText() const
{
    if (m_format.isEmpty())
        return serializeURL(m_location.specifiedURLString);
    return makeString(serializeURL(m_location.specifiedURLString), " format(", serializeString(m_format), ')');
}

bool CSSFontFaceSrcResourceValue::equals(const CSSFontFaceSrcResourceValue& other) const
{
    return m_location.specifiedURLString == m_location.specifiedURLString && m_format == other.m_format;
}

}
