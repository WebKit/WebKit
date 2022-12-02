/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
    static ExceptionOr<Ref<CSSStyleValue>> reifyValue(Ref<CSSValue>, Document* = nullptr);
    static ExceptionOr<Vector<Ref<CSSStyleValue>>> parseStyleValue(const AtomString&, const String&, bool);
    static RefPtr<CSSStyleValue> constructStyleValueForShorthandSerialization(const String&);
    static ExceptionOr<Vector<Ref<CSSStyleValue>>> vectorFromStyleValuesOrStrings(const AtomString& property, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&& values);

protected:
    CSSStyleValueFactory() = delete;

private:
    static ExceptionOr<RefPtr<CSSValue>> extractCSSValue(const CSSPropertyID&, const String&);
    static ExceptionOr<RefPtr<CSSStyleValue>> extractShorthandCSSValues(const CSSPropertyID&, const String&);
    static ExceptionOr<Ref<CSSUnparsedValue>> extractCustomCSSValues(const String&);
};

} // namespace WebCore
