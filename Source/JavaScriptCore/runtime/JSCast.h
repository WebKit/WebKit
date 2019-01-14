/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
    macro(JSFixedArray, JSType::JSFixedArrayType, JSType::JSFixedArrayType) \
    macro(JSObject, FirstObjectType, LastObjectType) \
    macro(JSFinalObject, JSType::FinalObjectType, JSType::FinalObjectType) \
    macro(JSFunction, JSType::JSFunctionType, JSType::JSFunctionType) \
    macro(InternalFunction, JSType::InternalFunctionType, JSType::InternalFunctionType) \
    macro(JSArray, JSType::ArrayType, JSType::DerivedArrayType) \
    macro(JSArrayBuffer, JSType::ArrayBufferType, JSType::ArrayBufferType) \
    macro(JSArrayBufferView, FirstTypedArrayType, LastTypedArrayType) \
    macro(JSSet, JSType::JSSetType, JSType::JSSetType) \
    macro(JSMap, JSType::JSMapType, JSType::JSMapType) \
    macro(JSWeakSet, JSType::JSWeakSetType, JSType::JSWeakSetType) \
    macro(JSWeakMap, JSType::JSWeakMapType, JSType::JSWeakMapType) \
    macro(NumberObject, JSType::NumberObjectType, JSType::NumberObjectType) \
    macro(ProxyObject, JSType::ProxyObjectType, JSType::ProxyObjectType) \
    macro(RegExpObject, JSType::RegExpObjectType, JSType::RegExpObjectType) \
    macro(WebAssemblyToJSCallee, JSType::WebAssemblyToJSCalleeType, JSType::WebAssemblyToJSCalleeType) \
    macro(DirectArguments, JSType::DirectArgumentsType, JSType::DirectArgumentsType) \
    macro(ScopedArguments, JSType::ScopedArgumentsType, JSType::ScopedArgumentsType) \
    macro(ClonedArguments, JSType::ClonedArgumentsType, JSType::ClonedArgumentsType) \
    macro(JSGlobalObject, JSType::GlobalObjectType, JSType::GlobalObjectType) \
    macro(JSGlobalLexicalEnvironment, JSType::GlobalLexicalEnvironmentType, JSType::GlobalLexicalEnvironmentType) \
    macro(JSSegmentedVariableObject, JSType::GlobalObjectType, JSType::GlobalLexicalEnvironmentType) \
    macro(JSModuleEnvironment, JSType::ModuleEnvironmentType, JSType::ModuleEnvironmentType) \
    macro(JSLexicalEnvironment, JSType::LexicalEnvironmentType, JSType::ModuleEnvironmentType) \
    macro(JSSymbolTableObject, JSType::GlobalObjectType, JSType::ModuleEnvironmentType) \
    macro(JSScope, JSType::GlobalObjectType, JSType::WithScopeType) \


// Forward declare the classes because they may not already exist.
#define FORWARD_DECLARE_OVERLOAD_CLASS(className, jsType, op) class className;
FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD(FORWARD_DECLARE_OVERLOAD_CLASS)
#undef FORWARD_DECLARE_OVERLOAD_CLASS

namespace JSCastingHelpers {

template<bool isFinal>
struct FinalTypeDispatcher {
    template<typename Target, typename From>
    static inline bool inheritsGeneric(VM& vm, From* from)
    {
        static_assert(!std::is_same<JSObject*, Target*>::value, "This ensures our overloads work");
        static_assert(std::is_base_of<JSCell, Target>::value && std::is_base_of<JSCell, typename std::remove_pointer<From>::type>::value, "JS casting expects that the types you are casting to/from are subclasses of JSCell");
        // Do not use inherits<Target>(vm) since inherits<T> depends on this function.
        return from->JSCell::inherits(vm, Target::info());
    }
};

template<>
struct FinalTypeDispatcher</* isFinal */ true> {
    template<typename Target, typename From>
    static inline bool inheritsGeneric(VM& vm, From* from)
    {
        static_assert(!std::is_same<JSObject*, Target*>::value, "This ensures our overloads work");
        static_assert(std::is_base_of<JSCell, Target>::value && std::is_base_of<JSCell, typename std::remove_pointer<From>::type>::value, "JS casting expects that the types you are casting to/from are subclasses of JSCell");
        static_assert(std::is_final<Target>::value, "Target is a final type");
        bool canCast = from->JSCell::classInfo(vm) == Target::info();
        // Do not use inherits<Target>(vm) since inherits<T> depends on this function.
        ASSERT_UNUSED(vm, canCast == from->JSCell::inherits(vm, Target::info()));
        return canCast;
    }
};

template<typename Target, typename From>
inline bool inheritsJSTypeImpl(VM& vm, From* from, JSType firstType, JSType lastType)
{
    static_assert(std::is_base_of<JSCell, Target>::value && std::is_base_of<JSCell, typename std::remove_pointer<From>::type>::value, "JS casting expects that the types you are casting to/from are subclasses of JSCell");
    bool canCast = firstType <= from->type() && from->type() <= lastType;
    // Do not use inherits<Target>(vm) since inherits<T> depends on this function.
    ASSERT_UNUSED(vm, canCast == from->JSCell::inherits(vm, Target::info()));
    return canCast;
}

// C++ has bad syntax so we need to use this struct because C++ doesn't have a
// way to say that we are overloading just the first type in a template list...
template<typename Target>
struct InheritsTraits {
    template<typename From>
    static inline bool inherits(VM& vm, From* from) { return FinalTypeDispatcher<std::is_final<Target>::value>::template inheritsGeneric<Target>(vm, from); }
};

#define DEFINE_TRAITS_FOR_JS_TYPE_OVERLOAD(className, firstJSType, lastJSType) \
    template<> \
    struct InheritsTraits<className> { \
        template<typename From> \
        static inline bool inherits(VM& vm, From* from) { return inheritsJSTypeImpl<className, From>(vm, from, static_cast<JSType>(firstJSType), static_cast<JSType>(lastJSType)); } \
    }; \

FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD(DEFINE_TRAITS_FOR_JS_TYPE_OVERLOAD)

#undef DEFINE_TRAITS_FOR_JS_TYPE_OVERLOAD


template<typename Target, typename From>
bool inherits(VM& vm, From* from)
{
    using Dispatcher = InheritsTraits<Target>;
    return Dispatcher::template inherits(vm, from);
}

} // namespace JSCastingHelpers

template<typename To, typename From>
To jsDynamicCast(VM& vm, From* from)
{
    using Dispatcher = JSCastingHelpers::InheritsTraits<typename std::remove_cv<typename std::remove_pointer<To>::type>::type>;
    if (LIKELY(Dispatcher::template inherits(vm, from)))
        return static_cast<To>(from);
    return nullptr;
}

template<typename To>
To jsDynamicCast(VM& vm, JSValue from)
{
    if (UNLIKELY(!from.isCell()))
        return nullptr;
    return jsDynamicCast<To>(vm, from.asCell());
}

template<typename To, typename From>
To jsSecureCast(VM& vm, From from)
{
    auto* result = jsDynamicCast<To>(vm, from);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(result);
    return result;
}

}
