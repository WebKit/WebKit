/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPinnedAnchorNameValue.h"

#if ENABLE(SPATIAL_PORTAL)

#include <wtf/Hasher.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

Ref<CSSPinnedAnchorNameValue> CSSPinnedAnchorNameValue::create(CSS::CustomIdent name, CSS::CustomIdent attachment)
{
    return adoptRef(*new CSSPinnedAnchorNameValue(WTF::move(name), WTF::move(attachment)));
}

CSSPinnedAnchorNameValue::CSSPinnedAnchorNameValue(CSS::CustomIdent&& name, CSS::CustomIdent&& attachment)
    : CSSValue(ClassType::PinnedAnchorName)
    , m_name(WTF::move(name))
    , m_attachment(WTF::move(attachment))
{
}

String CSSPinnedAnchorNameValue::customCSSText(const CSS::SerializationContext& context) const
{
    return makeString(CSS::serializationForCSS(context, m_name), '#', CSS::serializationForCSS(context, m_attachment));
}

bool CSSPinnedAnchorNameValue::equals(const CSSPinnedAnchorNameValue& other) const
{
    return m_name == other.m_name && m_attachment == other.m_attachment;
}

bool CSSPinnedAnchorNameValue::addDerivedHash(Hasher& hasher) const
{
    add(hasher, m_name);
    add(hasher, m_attachment);
    return true;
}

} // namespace WebCore

#endif // ENABLE(SPATIAL_PORTAL)
