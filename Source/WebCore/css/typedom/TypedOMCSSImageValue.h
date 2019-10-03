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

#if ENABLE(CSS_TYPED_OM)

#include "CSSImageValue.h"
#include "TypedOMCSSStyleValue.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;

class TypedOMCSSImageValue final : public TypedOMCSSStyleValue {
    WTF_MAKE_ISO_ALLOCATED(TypedOMCSSImageValue);
public:
    static Ref<TypedOMCSSImageValue> create(CSSImageValue& cssValue, Document& document)
    {
        return adoptRef(*new TypedOMCSSImageValue(cssValue, document));
    }

    String toString() final { return m_cssValue->cssText(); }

    CachedImage* image() { return m_cssValue->cachedImage(); }
    Document* document() const;

private:
    TypedOMCSSImageValue(CSSImageValue& cssValue, Document& document);

    bool isImageValue() final { return true; }

    Ref<CSSImageValue> m_cssValue;
    WeakPtr<Document> m_document;
};

} // namespace WebCore

#endif
