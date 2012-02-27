/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(CSS_SHADERS)
#include "WebKitCSSShaderValue.h"

#include "CachedResourceLoader.h"
#include "CSSParser.h"
#include "Document.h"
#include "StyleCachedShader.h"
#include "StylePendingShader.h"

namespace WebCore {

WebKitCSSShaderValue::WebKitCSSShaderValue(const String& url)
    : CSSValue(WebKitCSSShaderClass)
    , m_url(url)
    , m_accessedShader(false)
{
}

WebKitCSSShaderValue::~WebKitCSSShaderValue()
{
}

StyleCachedShader* WebKitCSSShaderValue::cachedShader(CachedResourceLoader* loader)
{
    ASSERT(loader);

    if (!m_accessedShader) {
        m_accessedShader = true;

        ResourceRequest request(loader->document()->completeURL(m_url));
        if (CachedShader* cachedShader = loader->requestShader(request))
            m_shader = StyleCachedShader::create(cachedShader);
    }

    return (m_shader && m_shader->isCachedShader()) ? static_cast<StyleCachedShader*>(m_shader.get()) : 0;
}

StyleShader* WebKitCSSShaderValue::cachedOrPendingShader()
{
    if (!m_shader)
        m_shader = StylePendingShader::create(this);

    return m_shader.get();
}

String WebKitCSSShaderValue::customCssText() const
{
    return "url(" + quoteCSSURLIfNeeded(m_url) + ")";
}

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS)
