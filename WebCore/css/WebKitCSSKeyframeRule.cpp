/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebKitCSSKeyframeRule.h"

#include "CSSMutableStyleDeclaration.h"

namespace WebCore {

WebKitCSSKeyframeRule::WebKitCSSKeyframeRule(CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_key(0)
{
}

WebKitCSSKeyframeRule::~WebKitCSSKeyframeRule()
{
    if (m_style)
        m_style->setParent(0);
}

String WebKitCSSKeyframeRule::keyText() const
{
    return String::number(m_key) + "%";
}

void WebKitCSSKeyframeRule::setKeyText(const String& s)
{
    m_key = keyStringToFloat(s);
}

String WebKitCSSKeyframeRule::cssText() const
{
    String result = String::number(m_key);

    result += "% { ";
    result += m_style->cssText();
    result += "}";

    return result;
}

bool WebKitCSSKeyframeRule::parseString(const String& /*string*/, bool /*strict*/)
{
    // FIXME
    return false;
}

void WebKitCSSKeyframeRule::setDeclaration(PassRefPtr<CSSMutableStyleDeclaration> style)
{
    m_style = style;
    m_style->setParent(parent());
}

/* static */
float WebKitCSSKeyframeRule::keyStringToFloat(const String& s)
{
    float key = 0;
    
    // for now the syntax MUST be 'xxx%' or 'from' or 'to', where xxx is a legal floating point number
    if (s == "from")
        key = 0;
    else if (s == "to")
        key = 100;
    else {
        if (s.endsWith("%")) {
            float k = s.substring(0, s.length() - 1).toFloat();
            if (k >= 0 && k <= 100)
                key = k;
        }
    }
    
    return key;
}

} // namespace WebCore
