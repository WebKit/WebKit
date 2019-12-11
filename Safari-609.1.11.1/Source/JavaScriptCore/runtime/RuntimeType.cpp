/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
 * Copyright (C) Saam Barati <saambarati1@gmail.com>. All rights reserved.
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
#include "RuntimeType.h"

#include "JSCInlines.h"

namespace JSC {

RuntimeType runtimeTypeForValue(VM& vm, JSValue value)
{
    if (UNLIKELY(!value))
        return TypeNothing;

    if (value.isUndefined())
        return TypeUndefined;
    if (value.isNull())
        return TypeNull;
    if (value.isAnyInt())
        return TypeAnyInt;
    if (value.isNumber())
        return TypeNumber;
    if (value.isString())
        return TypeString;
    if (value.isBoolean())
        return TypeBoolean;
    if (value.isObject())
        return TypeObject;
    if (value.isFunction(vm))
        return TypeFunction;
    if (value.isSymbol())
        return TypeSymbol;
    if (value.isBigInt())
        return TypeBigInt;

    return TypeNothing;
}

String runtimeTypeAsString(RuntimeType type)
{
    if (type == TypeUndefined)
        return "Undefined"_s;
    if (type == TypeNull)
        return "Null"_s;
    if (type == TypeAnyInt)
        return "Integer"_s;
    if (type == TypeNumber)
        return "Number"_s;
    if (type == TypeString)
        return "String"_s;
    if (type == TypeObject)
        return "Object"_s;
    if (type == TypeBoolean)
        return "Boolean"_s;
    if (type == TypeFunction)
        return "Function"_s;
    if (type == TypeSymbol)
        return "Symbol"_s;
    if (type == TypeBigInt)
        return "BigInt"_s;
    if (type == TypeNothing)
        return "(Nothing)"_s;

    RELEASE_ASSERT_NOT_REACHED();
    return emptyString();
}

} // namespace JSC
