/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSCustomPropertyValue.h"
#include "CSSImageValue.h"
#include "CSSPropertyNames.h"
#include "CSSStyleValue.h"
#include "CSSValue.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

template<typename T> class ExceptionOr;
struct CSSParserContext;
class CSSUnparsedValue;
class Document;
class StylePropertyShorthand;

class CSSStyleValueFactory {
public:
    static ExceptionOr<Ref<CSSStyleValue>> reifyValue(Document&, const CSSValue&, AssociatedProperty&&);

    // https://drafts.css-houdini.org/css-typed-om-1/#parse-a-cssstylevalue
    static ExceptionOr<Vector<Ref<CSSStyleValue>>> parseStyleValue(Document&, const AtomString&, const String&, bool parseMultiple);
    static ExceptionOr<Vector<Ref<CSSStyleValue>>> parseStyleValueForCustomProperty(Document&, const AtomString&, const String&, bool parseMultiple);
    static ExceptionOr<Vector<Ref<CSSStyleValue>>> parseStyleValueForKnownProperty(Document&, CSSPropertyID, const String&, bool parseMultiple);

    static RefPtr<CSSStyleValue> constructStyleValueForShorthandSerialization(Document&, const String&, CSSPropertyID);

    static ExceptionOr<Vector<Ref<CSSStyleValue>>> vectorFromStyleValuesOrStringsForCustomProperty(Document&, const AtomString& property, FixedVector<Variant<Ref<CSSStyleValue>, String>>&&);
    static ExceptionOr<Vector<Ref<CSSStyleValue>>> vectorFromStyleValuesOrStringsForKnownProperty(Document&, CSSPropertyID, FixedVector<Variant<Ref<CSSStyleValue>, String>>&&);

    ~CSSStyleValueFactory();

protected:
    CSSStyleValueFactory() = delete;

private:
    static ExceptionOr<RefPtr<CSSValue>> extractCSSValue(Document&, const CSSPropertyID&, const String&);
    static ExceptionOr<RefPtr<CSSStyleValue>> extractShorthandCSSValues(Document&, const CSSPropertyID&, const String&);
    static ExceptionOr<Ref<CSSUnparsedValue>> extractCustomCSSValues(const String&);
};

} // namespace WebCore
