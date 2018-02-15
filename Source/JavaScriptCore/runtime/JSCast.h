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

#include "JSCell.h"

namespace JSC {

template<typename To, typename From>
inline To jsCast(From* from)
{
    static_assert(std::is_base_of<JSCell, typename std::remove_pointer<To>::type>::value && std::is_base_of<JSCell, typename std::remove_pointer<From>::type>::value, "JS casting expects that the types you are casting to/from are subclasses of JSCell");
    ASSERT_WITH_SECURITY_IMPLICATION(!from || from->JSCell::inherits(*from->JSCell::vm(), std::remove_pointer<To>::type::info()));
    return static_cast<To>(from);
}

template<typename To>
inline To jsCast(JSValue from)
{
    static_assert(std::is_base_of<JSCell, typename std::remove_pointer<To>::type>::value, "JS casting expects that the types you are casting to is a subclass of JSCell");
    ASSERT_WITH_SECURITY_IMPLICATION(from.isCell() && from.asCell()->JSCell::inherits(*from.asCell()->vm(), std::remove_pointer<To>::type::info()));
    return static_cast<To>(from.asCell());
}

// Specific type overloads.
#define FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD(macro) \
    macro(JSObject, JSType::ObjectType, JSType::LastJSCObjectType) \
    macro(JSFunction, JSType::JSFunctionType, JSType::JSFunctionType) \
    macro(InternalFunction, JSType::InternalFunctionType, JSType::InternalFunctionType) \
    macro(JSArray, JSType::ArrayType, JSType::DerivedArrayType) \
    macro(JSArrayBufferView, FirstTypedArrayType, LastTypedArrayType) \


// Forward declare the classes because they may not already exist.
#define FORWARD_DECLARE_OVERLOAD_CLASS(className, jsType, op) class className;
FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD(FORWARD_DECLARE_OVERLOAD_CLASS)
#undef FORWARD_DECLARE_OVERLOAD_CLASS

namespace JSCastingHelpers {

template<typename To, typename From>
inline To jsDynamicCastGenericImpl(VM& vm, From* from)
{
    static_assert(!std::is_same<JSObject*, To*>::value, "This ensures our overloads work");
    static_assert(std::is_base_of<JSCell, typename std::remove_pointer<To>::type>::value && std::is_base_of<JSCell, typename std::remove_pointer<From>::type>::value, "JS casting expects that the types you are casting to/from are subclasses of JSCell");
    if (LIKELY(from->JSCell::inherits(vm, std::remove_pointer<To>::type::info())))
        return static_cast<To>(from);
    return nullptr;
}

template<typename To, typename From>
inline To jsDynamicCastJSTypeImpl(VM& vm, From* from, JSType firstType, JSType lastType)
{
    bool canCast = firstType <= from->type() && from->type() <= lastType;
    ASSERT_UNUSED(vm, canCast == jsDynamicCastGenericImpl<To>(vm, from));
    if (LIKELY(canCast))
        return static_cast<To>(from);
    return nullptr;
}

// C++ has bad syntax so we need to use this struct because C++ doesn't have a
// way to say that we are overloading just the first type in a template list...
template<typename to>
struct JSDynamicCastTraits {
    template<typename To, typename From>
    static inline To cast(VM& vm, From* from) { return jsDynamicCastGenericImpl<To>(vm, from); }
};

#define DEFINE_TRAITS_FOR_JS_TYPE_OVERLOAD(className, firstJSType, lastJSType) \
    template<> \
    struct JSDynamicCastTraits<className*> { \
        template<typename To, typename From> \
        static inline To cast(VM& vm, From* from) { return jsDynamicCastJSTypeImpl<To>(vm, from, firstJSType, lastJSType); } \
    }; \

FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD(DEFINE_TRAITS_FOR_JS_TYPE_OVERLOAD)

#undef DEFINE_TRAITS_FOR_JS_TYPE_OVERLOAD

} // namespace JSCastingHelpers

template<typename To, typename From>
To jsDynamicCast(VM& vm, From* from)
{
    typedef JSCastingHelpers::JSDynamicCastTraits<typename std::remove_cv<typename std::remove_pointer<To>::type>::type> Dispatcher;
    return Dispatcher::template cast<To>(vm, from);
}

template<typename To>
To jsDynamicCast(VM& vm, JSValue from)
{
    if (UNLIKELY(!from.isCell()))
        return nullptr;
    return jsDynamicCast<To>(vm, from.asCell());
}

}
