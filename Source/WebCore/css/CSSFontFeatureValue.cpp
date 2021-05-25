/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "CSSFontFeatureValue.h"

#include "CSSValueKeywords.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSFontFeatureValue::CSSFontFeatureValue(FontTag&& tag, int value)
    : CSSValue(FontFeatureClass)
    , m_tag(WTFMove(tag))
    , m_value(value)
{
}

String CSSFontFeatureValue::customCSSText() const
{
    StringBuilder builder;
    builder.append('"');
    for (char c : m_tag)
        builder.append(c);
    builder.append('"');
    // Omit the value if it's 1 as 1 is implied by default.
    if (m_value != 1)
        builder.append(' ', m_value);
    return builder.toString();
}

bool CSSFontFeatureValue::equals(const CSSFontFeatureValue& other) const
{
    return m_tag == other.m_tag && m_value == other.m_value;
}

}
