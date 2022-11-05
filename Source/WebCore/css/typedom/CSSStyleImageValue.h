/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "CSSImageValue.h"
#include "CSSStyleValue.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class WeakPtrImplWithEventTargetData;

class CSSStyleImageValue final : public CSSStyleValue {
    WTF_MAKE_ISO_ALLOCATED(CSSStyleImageValue);
public:
    static Ref<CSSStyleImageValue> create(Ref<CSSImageValue>&& cssValue, Document* document)
    {
        return adoptRef(*new CSSStyleImageValue(WTFMove(cssValue), document));
    }

    void serialize(StringBuilder&, OptionSet<SerializationArguments>) const final;

    CachedImage* image() { return m_cssValue->cachedImage(); }
    Document* document() const;
    
    CSSStyleValueType getType() const final { return CSSStyleValueType::CSSStyleImageValue; }
    
    RefPtr<CSSValue> toCSSValue() const final;

private:
    CSSStyleImageValue(Ref<CSSImageValue>&&, Document*);

    Ref<CSSImageValue> m_cssValue;
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSStyleImageValue)
    static bool isType(const WebCore::CSSStyleValue& styleValue) { return styleValue.getType() == WebCore::CSSStyleValueType::CSSStyleImageValue; }
SPECIALIZE_TYPE_TRAITS_END()
