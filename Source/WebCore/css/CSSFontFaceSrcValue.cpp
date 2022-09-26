/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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
#include "CachedResourceRequestInitiators.h"
#include "FontCustomPlatformData.h"
#include "FontLoadRequest.h"
#include "Node.h"
#include "SVGFontFaceElement.h"
#include "ScriptExecutionContext.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSFontFaceSrcValue::~CSSFontFaceSrcValue() = default;

bool CSSFontFaceSrcValue::isSVGFontFaceSrc() const
{
    return equalLettersIgnoringASCIICase(m_format, "svg"_s);
}

bool CSSFontFaceSrcValue::isSVGFontTarget() const
{
    return isSVGFontFaceSrc() || svgFontFaceElement();
}

bool CSSFontFaceSrcValue::isSupportedFormat() const
{
    // Normally we would just check the format, but in order to avoid conflicts with the old WinIE style of font-face,
    // we will also check to see if the URL ends with .eot. If so, we'll assume that we shouldn't load it.
    if (m_format.isEmpty()) {
        // Check for .eot.
        if (!protocolIs(m_resource, "data"_s) && m_resource.endsWithIgnoringASCIICase(".eot"_s))
            return false;
        return true;
    }

    return FontCustomPlatformData::supportsFormat(m_format) || isSVGFontFaceSrc();
}

SVGFontFaceElement* CSSFontFaceSrcValue::svgFontFaceElement() const
{
    return m_svgFontFaceElement.get();
}

void CSSFontFaceSrcValue::setSVGFontFaceElement(SVGFontFaceElement* element)
{
    m_svgFontFaceElement = element;
}

String CSSFontFaceSrcValue::customCSSText() const
{
    // FIXME: URLs should not be absolutized, but instead should be serialized exactly as they were specified.
    const char* prefix = isLocal() ? "local(" : "url(";
    if (m_format.isEmpty())
        return makeString(prefix, serializeString(m_resource), ")");
    return makeString(prefix, serializeString(m_resource), ")", " format(", serializeString(m_format), ")");
}

bool CSSFontFaceSrcValue::customTraverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    return m_cachedFont && handler(*m_cachedFont);
}

std::unique_ptr<FontLoadRequest> CSSFontFaceSrcValue::fontLoadRequest(ScriptExecutionContext* context, bool isSVG, bool isInitiatingElementInUserAgentShadowTree)
{
    if (m_cachedFont)
        return makeUnique<CachedFontLoadRequest>(*m_cachedFont);

    auto request = context->fontLoadRequest(m_resource, isSVG, isInitiatingElementInUserAgentShadowTree, m_loadedFromOpaqueSource);
    if (is<CachedFontLoadRequest>(request))
        m_cachedFont = &downcast<CachedFontLoadRequest>(request.get())->cachedFont();

    return request;
}

bool CSSFontFaceSrcValue::equals(const CSSFontFaceSrcValue& other) const
{
    return m_isLocal == other.m_isLocal && m_format == other.m_format && m_resource == other.m_resource;
}

}
