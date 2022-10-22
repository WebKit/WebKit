/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "CSSStyleValue.h"

#include "CSSParser.h"
#include "CSSPropertyParser.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnitValue.h"
#include "CSSValueList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringView.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSStyleValue);

ExceptionOr<Ref<CSSStyleValue>> CSSStyleValue::parse(const AtomString& property, const String& cssText)
{
    constexpr bool parseMultiple = false;
    auto parseResult = CSSStyleValueFactory::parseStyleValue(property, cssText, parseMultiple);
    if (parseResult.hasException())
        return parseResult.releaseException();
    
    auto returnValue = parseResult.releaseReturnValue();
    
    // Returned vector should not be empty. If parsing failed, an exception should be returned.
    if (returnValue.isEmpty())
        return Exception { SyntaxError, makeString(cssText, " cannot be parsed as a ", property) };

    return WTFMove(returnValue.at(0));
}

ExceptionOr<Vector<Ref<CSSStyleValue>>> CSSStyleValue::parseAll(const AtomString& property, const String& cssText)
{
    constexpr bool parseMultiple = true;
    return CSSStyleValueFactory::parseStyleValue(property, cssText, parseMultiple);
}

Ref<CSSStyleValue> CSSStyleValue::create(RefPtr<CSSValue>&& cssValue, String&& property)
{
    return adoptRef(*new CSSStyleValue(WTFMove(cssValue), WTFMove(property)));
}

Ref<CSSStyleValue> CSSStyleValue::create()
{
    return adoptRef(*new CSSStyleValue());
}

CSSStyleValue::CSSStyleValue(RefPtr<CSSValue>&& cssValue, String&& property)
    : m_customPropertyName(WTFMove(property))
    , m_propertyValue(WTFMove(cssValue))
{
}

String CSSStyleValue::toString() const
{
    StringBuilder builder;
    serialize(builder);
    return builder.toString();
}

void CSSStyleValue::serialize(StringBuilder& builder, OptionSet<SerializationArguments>) const
{
    if (m_propertyValue)
        builder.append(m_propertyValue->cssText());
}

} // namespace WebCore
