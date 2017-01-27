/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <runtime/JSCJSValueInlines.h>
#include <runtime/JSCellInlines.h>
#include <type_traits>

namespace WebCore {

template<typename To, typename From>
using JSDynamicCastResult = typename std::conditional<std::is_const<From>::value, const To, To>::type*;

class JSNode;
template<typename From>
JSDynamicCastResult<JSNode, From> jsNodeCast(From* value);
class JSElement;
template<typename From>
JSDynamicCastResult<JSElement, From> jsElementCast(From* value);
class JSDocument;
template<typename From>
JSDynamicCastResult<JSDocument, From> jsDocumentCast(From* value);
class JSEvent;
template<typename From>
JSDynamicCastResult<JSEvent, From> jsEventCast(From* value);

template<typename Select>
struct JSDynamicCastTrait {
    template<typename To, typename From>
    ALWAYS_INLINE static To cast(JSC::VM& vm, From* from)
    {
        return JSC::jsDynamicCast<To>(vm, from);
    }
};

template<>
struct JSDynamicCastTrait<JSNode> {
    template<typename To, typename From>
    ALWAYS_INLINE static To cast(JSC::VM&, From* from)
    {
        return jsNodeCast(from);
    }
};

template<>
struct JSDynamicCastTrait<JSElement> {
    template<typename To, typename From>
    ALWAYS_INLINE static To cast(JSC::VM&, From* from)
    {
        return jsElementCast(from);
    }
};

template<>
struct JSDynamicCastTrait<JSDocument> {
    template<typename To, typename From>
    ALWAYS_INLINE static To cast(JSC::VM&, From* from)
    {
        return jsDocumentCast(from);
    }
};

template<>
struct JSDynamicCastTrait<JSEvent> {
    template<typename To, typename From>
    ALWAYS_INLINE static To cast(JSC::VM&, From* from)
    {
        return jsEventCast(from);
    }
};

template<typename To, typename From>
ALWAYS_INLINE To jsDynamicDowncast(JSC::VM& vm, From* from)
{
    typedef JSDynamicCastTrait<typename std::remove_cv<typename std::remove_pointer<To>::type>::type> Dispatcher;
    return Dispatcher::template cast<To>(vm, from);
}

template<typename To>
ALWAYS_INLINE To jsDynamicDowncast(JSC::VM& vm, JSC::JSValue from)
{
    if (UNLIKELY(!from.isCell()))
        return nullptr;
    return jsDynamicDowncast<To>(vm, from.asCell());
}

}
