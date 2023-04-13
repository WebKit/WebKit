/*
 * Copyright (C) 2008, 2010 Apple Inc. All Rights Reserved.
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
#include "CSSFunctionValue.h"

#include "CSSValueKeywords.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
    
CSSFunctionValue::CSSFunctionValue(CSSValueID name, CSSValueListBuilder arguments)
    : CSSValueContainingVector(FunctionClass, CommaSeparator, WTFMove(arguments))
    , m_name(name)
{
}

CSSFunctionValue::CSSFunctionValue(CSSValueID name)
    : CSSValueContainingVector(FunctionClass, CommaSeparator)
    , m_name(name)
{
}

CSSFunctionValue::CSSFunctionValue(CSSValueID name, Ref<CSSValue> argument)
    : CSSValueContainingVector(FunctionClass, CommaSeparator, WTFMove(argument))
    , m_name(name)
{
}

CSSFunctionValue::CSSFunctionValue(CSSValueID name, Ref<CSSValue> argument1, Ref<CSSValue> argument2)
    : CSSValueContainingVector(FunctionClass, CommaSeparator, WTFMove(argument1), WTFMove(argument2))
    , m_name(name)
{
}

CSSFunctionValue::CSSFunctionValue(CSSValueID name, Ref<CSSValue> argument1, Ref<CSSValue> argument2, Ref<CSSValue> argument3)
    : CSSValueContainingVector(FunctionClass, CommaSeparator, WTFMove(argument1), WTFMove(argument2), WTFMove(argument3))
    , m_name(name)
{
}

CSSFunctionValue::CSSFunctionValue(CSSValueID name, Ref<CSSValue> argument1, Ref<CSSValue> argument2, Ref<CSSValue> argument3, Ref<CSSValue> argument4)
    : CSSValueContainingVector(FunctionClass, CommaSeparator, WTFMove(argument1), WTFMove(argument2), WTFMove(argument3), WTFMove(argument4))
    , m_name(name)
{
}

Ref<CSSFunctionValue> CSSFunctionValue::create(CSSValueID name, CSSValueListBuilder arguments)
{
    return adoptRef(*new CSSFunctionValue(name, WTFMove(arguments)));
}

Ref<CSSFunctionValue> CSSFunctionValue::create(CSSValueID name)
{
    return adoptRef(*new CSSFunctionValue(name));
}

Ref<CSSFunctionValue> CSSFunctionValue::create(CSSValueID name, Ref<CSSValue> argument)
{
    return adoptRef(*new CSSFunctionValue(name, WTFMove(argument)));
}

Ref<CSSFunctionValue> CSSFunctionValue::create(CSSValueID name, Ref<CSSValue> argument1, Ref<CSSValue> argument2)
{
    return adoptRef(*new CSSFunctionValue(name, WTFMove(argument1), WTFMove(argument2)));
}

Ref<CSSFunctionValue> CSSFunctionValue::create(CSSValueID name, Ref<CSSValue> argument1, Ref<CSSValue> argument2, Ref<CSSValue> argument3)
{
    return adoptRef(*new CSSFunctionValue(name, WTFMove(argument1), WTFMove(argument2), WTFMove(argument3)));
}

Ref<CSSFunctionValue> CSSFunctionValue::create(CSSValueID name, Ref<CSSValue> argument1, Ref<CSSValue> argument2, Ref<CSSValue> argument3, Ref<CSSValue> argument4)
{
    return adoptRef(*new CSSFunctionValue(name, WTFMove(argument1), WTFMove(argument2), WTFMove(argument3), WTFMove(argument4)));
}

String CSSFunctionValue::customCSSText() const
{
    StringBuilder result;
    result.append(nameLiteral(m_name), '(');
    serializeItems(result);
    result.append(')');
    return result.toString();
}

}
