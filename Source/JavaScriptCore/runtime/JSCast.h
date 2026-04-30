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

#include <JavaScriptCore/JSCell.h>
#include <wtf/TypeCasts.h>

namespace JSC {

// The first and last JSType are inclusive
struct JSTypeRange {
    bool contains(JSType type) const { return first <= type && type <= last; }

    JSType first;
    JSType last;
};

// Specific type overloads.

template<typename>
class JSGenericTypedArrayView;
template<typename>
class JSGenericResizableOrGrowableSharedTypedArrayView;
struct Int8Adaptor;
struct Int16Adaptor;
struct Int32Adaptor;
struct Uint8Adaptor;
struct Uint16Adaptor;
struct Uint32Adaptor;
struct Uint8ClampedAdaptor;
struct Float16Adaptor;
struct Float32Adaptor;
struct Float64Adaptor;
struct BigInt64Adaptor;
struct BigUint64Adaptor;

using JSInt8Array = JSGenericTypedArrayView<Int8Adaptor>;
using JSInt16Array = JSGenericTypedArrayView<Int16Adaptor>;
using JSInt32Array = JSGenericTypedArrayView<Int32Adaptor>;
using JSUint8Array = JSGenericTypedArrayView<Uint8Adaptor>;
using JSUint8ClampedArray = JSGenericTypedArrayView<Uint8ClampedAdaptor>;
using JSUint16Array = JSGenericTypedArrayView<Uint16Adaptor>;
using JSUint32Array = JSGenericTypedArrayView<Uint32Adaptor>;
using JSFloat16Array = JSGenericTypedArrayView<Float16Adaptor>;
using JSFloat32Array = JSGenericTypedArrayView<Float32Adaptor>;
using JSFloat64Array = JSGenericTypedArrayView<Float64Adaptor>;
using JSBigInt64Array = JSGenericTypedArrayView<BigInt64Adaptor>;
using JSBigUint64Array = JSGenericTypedArrayView<BigUint64Adaptor>;

using JSResizableOrGrowableSharedInt8Array = JSGenericResizableOrGrowableSharedTypedArrayView<Int8Adaptor>;
using JSResizableOrGrowableSharedInt16Array = JSGenericResizableOrGrowableSharedTypedArrayView<Int16Adaptor>;
using JSResizableOrGrowableSharedInt32Array = JSGenericResizableOrGrowableSharedTypedArrayView<Int32Adaptor>;
using JSResizableOrGrowableSharedUint8Array = JSGenericResizableOrGrowableSharedTypedArrayView<Uint8Adaptor>;
using JSResizableOrGrowableSharedUint8ClampedArray = JSGenericResizableOrGrowableSharedTypedArrayView<Uint8ClampedAdaptor>;
using JSResizableOrGrowableSharedUint16Array = JSGenericResizableOrGrowableSharedTypedArrayView<Uint16Adaptor>;
using JSResizableOrGrowableSharedUint32Array = JSGenericResizableOrGrowableSharedTypedArrayView<Uint32Adaptor>;
using JSResizableOrGrowableSharedFloat16Array = JSGenericResizableOrGrowableSharedTypedArrayView<Float16Adaptor>;
using JSResizableOrGrowableSharedFloat32Array = JSGenericResizableOrGrowableSharedTypedArrayView<Float32Adaptor>;
using JSResizableOrGrowableSharedFloat64Array = JSGenericResizableOrGrowableSharedTypedArrayView<Float64Adaptor>;
using JSResizableOrGrowableSharedBigInt64Array = JSGenericResizableOrGrowableSharedTypedArrayView<BigInt64Adaptor>;
using JSResizableOrGrowableSharedBigUint64Array = JSGenericResizableOrGrowableSharedTypedArrayView<BigUint64Adaptor>;

#define FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD_NON_FORWARD_DECLARED(macro) \
    /* TypedArrays are typedef, thus, we cannot use `class` forward declaration */ \
    macro(JSInt8Array, JSType::Int8ArrayType, JSType::Int8ArrayType) \
    macro(JSUint8Array, JSType::Uint8ArrayType, JSType::Uint8ArrayType) \
    macro(JSUint8ClampedArray, JSType::Uint8ClampedArrayType, JSType::Uint8ClampedArrayType) \
    macro(JSInt16Array, JSType::Int16ArrayType, JSType::Int16ArrayType) \
    macro(JSUint16Array, JSType::Uint16ArrayType, JSType::Uint16ArrayType) \
    macro(JSInt32Array, JSType::Int32ArrayType, JSType::Int32ArrayType) \
    macro(JSUint32Array, JSType::Uint32ArrayType, JSType::Uint32ArrayType) \
    macro(JSFloat16Array, JSType::Float16ArrayType, JSType::Float16ArrayType) \
    macro(JSFloat32Array, JSType::Float32ArrayType, JSType::Float32ArrayType) \
    macro(JSFloat64Array, JSType::Float64ArrayType, JSType::Float64ArrayType) \
    macro(JSBigInt64Array, JSType::BigInt64ArrayType, JSType::BigInt64ArrayType) \
    macro(JSBigUint64Array, JSType::BigUint64ArrayType, JSType::BigUint64ArrayType) \

#define FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD_FORWARD_DECLARED(macro) \
    macro(JSCellButterfly, JSType::JSCellButterflyType, JSType::JSCellButterflyType) \
    macro(JSStringIterator, JSType::JSStringIteratorType, JSType::JSStringIteratorType) \
    macro(Structure, JSType::StructureType, JSType::StructureType) \
    macro(JSString, JSType::StringType, JSType::StringType) \
    macro(JSBigInt, JSType::HeapBigIntType, JSType::HeapBigIntType) \
    macro(Symbol, JSType::SymbolType, JSType::SymbolType) \
    macro(GetterSetter, JSType::GetterSetterType, JSType::GetterSetterType) \
    macro(CustomGetterSetter, JSType::CustomGetterSetterType, JSType::CustomGetterSetterType) \
    macro(NativeExecutable, JSType::NativeExecutableType, JSType::NativeExecutableType) \
    macro(CodeBlock, JSType::CodeBlockType, JSType::CodeBlockType) \
    macro(JSObject, FirstObjectType, LastObjectType) \
    macro(JSFinalObject, JSType::FinalObjectType, JSType::FinalObjectType) \
    macro(JSFunction, JSType::JSFunctionType, JSType::JSFunctionType) \
    macro(InternalFunction, JSType::InternalFunctionType, JSType::NullSetterFunctionType) \
    macro(NullSetterFunction, JSType::NullSetterFunctionType, JSType::NullSetterFunctionType) \
    macro(JSArray, JSType::ArrayType, JSType::DerivedArrayType) \
    macro(JSArrayIterator, JSType::JSArrayIteratorType, JSType::JSArrayIteratorType) \
    macro(JSArrayBuffer, JSType::ArrayBufferType, JSType::ArrayBufferType) \
    macro(JSArrayBufferView, FirstTypedArrayType, LastTypedArrayType) \
    macro(JSIterator, JSType::JSIteratorType, JSType::JSIteratorType) \
    macro(JSPromise, JSType::JSPromiseType, JSType::JSPromiseType) \
    macro(JSPromiseCombinatorsContext, JSType::JSPromiseCombinatorsContextType, JSType::JSPromiseCombinatorsContextType) \
    macro(JSPromiseCombinatorsGlobalContext, JSType::JSPromiseCombinatorsGlobalContextType, JSType::JSPromiseCombinatorsGlobalContextType) \
    macro(JSPromiseReaction, JSType::JSSlimPromiseReactionType, JSType::JSFullPromiseReactionType) \
    macro(JSSlimPromiseReaction, JSType::JSSlimPromiseReactionType, JSType::JSSlimPromiseReactionType) \
    macro(JSFullPromiseReaction, JSType::JSFullPromiseReactionType, JSType::JSFullPromiseReactionType) \
    macro(JSGlobalProxy, JSType::GlobalProxyType, JSType::GlobalProxyType) \
    macro(JSSet, JSType::JSSetType, JSType::JSSetType) \
    macro(JSMap, JSType::JSMapType, JSType::JSMapType) \
    macro(JSWeakSet, JSType::JSWeakSetType, JSType::JSWeakSetType) \
    macro(JSWeakMap, JSType::JSWeakMapType, JSType::JSWeakMapType) \
    macro(NumberObject, JSType::NumberObjectType, JSType::NumberObjectType) \
    macro(ProxyObject, JSType::ProxyObjectType, JSType::ProxyObjectType) \
    macro(RegExpObject, JSType::RegExpObjectType, JSType::RegExpObjectType) \
    macro(JSWebAssemblyModule, JSType::WebAssemblyModuleType, JSType::WebAssemblyModuleType) \
    macro(JSWebAssemblyInstance, JSType::WebAssemblyInstanceType, JSType::WebAssemblyInstanceType) \
    macro(DirectArguments, JSType::DirectArgumentsType, JSType::DirectArgumentsType) \
    macro(FunctionExecutable, JSType::FunctionExecutableType, JSType::FunctionExecutableType) \
    macro(ScopedArguments, JSType::ScopedArgumentsType, JSType::ScopedArgumentsType) \
    macro(ClonedArguments, JSType::ClonedArgumentsType, JSType::ClonedArgumentsType) \
    macro(JSGlobalObject, JSType::GlobalObjectType, JSType::GlobalObjectType) \
    macro(JSGlobalLexicalEnvironment, JSType::GlobalLexicalEnvironmentType, JSType::GlobalLexicalEnvironmentType) \
    macro(JSSegmentedVariableObject, JSType::GlobalObjectType, JSType::GlobalLexicalEnvironmentType) \
    macro(JSModuleEnvironment, JSType::ModuleEnvironmentType, JSType::ModuleEnvironmentType) \
    macro(JSLexicalEnvironment, JSType::LexicalEnvironmentType, JSType::ModuleEnvironmentType) \
    macro(JSSymbolTableObject, JSType::GlobalObjectType, JSType::ModuleEnvironmentType) \
    macro(JSScope, FirstScopeType, LastScopeType) \
    macro(StringObject, JSType::StringObjectType, JSType::DerivedStringObjectType) \
    macro(ShadowRealmObject, JSType::ShadowRealmType, JSType::ShadowRealmType) \
    macro(JSDataView, JSType::DataViewType, JSType::DataViewType) \
    macro(JSGenerator, JSType::JSGeneratorType, JSType::JSGeneratorType) \
    macro(JSAsyncGenerator, JSType::JSAsyncGeneratorType, JSType::JSAsyncGeneratorType) \
    macro(WebAssemblyGCObjectBase, JSType::WebAssemblyGCObjectType, JSType::WebAssemblyGCObjectType) \

#define FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD(macro) \
    FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD_FORWARD_DECLARED(macro) \
    FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD_NON_FORWARD_DECLARED(macro) \

// Forward declare the classes because they may not already exist.
#define FORWARD_DECLARE_OVERLOAD_CLASS(className, jsType, op) class className;
FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD_FORWARD_DECLARED(FORWARD_DECLARE_OVERLOAD_CLASS)
#undef FORWARD_DECLARE_OVERLOAD_CLASS

namespace JSCastingHelpers {

template<bool isFinal>
struct FinalTypeDispatcher {
    template<typename To, typename From>
    static inline bool inheritsGeneric(From* from)
    {
        static_assert(!std::same_as<JSObject*, To*>, "This ensures our overloads work");
        static_assert(std::derived_from<To, JSCell> && std::derived_from<std::remove_pointer_t<From>, JSCell>, "JS casting expects that the types you are casting to/from are subclasses of JSCell");
        // Do not use inherits<To>() since inherits<T> depends on this function.
        return from->JSCell::inherits(To::info());
    }
};

template<>
struct FinalTypeDispatcher</* isFinal */ true> {
    template<typename To, typename From>
    static inline bool inheritsGeneric(From* from)
    {
        static_assert(!std::same_as<JSObject*, To*>, "This ensures our overloads work");
        static_assert(std::derived_from<To, JSCell> && std::derived_from<std::remove_pointer_t<From>, JSCell>, "JS casting expects that the types you are casting to/from are subclasses of JSCell");
        static_assert(std::is_final<To>::value, "To is a final type");
        bool canCast = from->JSCell::classInfo() == To::info();
        // Do not use inherits<To>() since inherits<T> depends on this function.
        ASSERT(canCast == from->JSCell::inheritsSlow(To::info()));
        return canCast;
    }
};

template<typename To, typename From>
inline bool inheritsJSTypeImpl(From* from, JSTypeRange range)
{
    static_assert(std::derived_from<To, JSCell> && std::derived_from<std::remove_pointer_t<From>, JSCell>, "JS casting expects that the types you are casting to/from are subclasses of JSCell");
    bool canCast = range.contains(from->type());
    // Do not use inherits<To>() since inherits<T> depends on this function.
    ASSERT(canCast == from->JSCell::inheritsSlow(To::info()));
    return canCast;
}

// C++ has bad syntax so we need to use this struct because C++ doesn't have a
// way to say that we are overloading just the first type in a template list...
template<typename To>
struct InheritsTraits {
    static constexpr std::optional<JSTypeRange> typeRange { std::nullopt };
    template<typename From>
    static inline bool inherits(From* from) { return FinalTypeDispatcher<std::is_final<To>::value>::template inheritsGeneric<To>(from); }
};

#define DEFINE_TRAITS_FOR_JS_TYPE_OVERLOAD(className, firstJSType, lastJSType) \
    template<> \
    struct InheritsTraits<className> { \
        static constexpr std::optional<JSTypeRange> typeRange { { static_cast<JSType>(firstJSType), static_cast<JSType>(lastJSType) } }; \
        template<typename From> \
        static inline bool inherits(From* from) { return inheritsJSTypeImpl<className, From>(from, *typeRange); } \
    }; \

FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD(DEFINE_TRAITS_FOR_JS_TYPE_OVERLOAD)

#undef DEFINE_TRAITS_FOR_JS_TYPE_OVERLOAD


template<typename To, typename From>
bool inherits(From* from)
{
    using Dispatcher = InheritsTraits<To>;
    return Dispatcher::template inherits<>(from);
}

} // namespace JSCastingHelpers

} // namespace JSC

// Concept that identifies JSCell subclasses without requiring complete types.
// Uses T::info() as a marker instead of std::derived_from (which is UB with incomplete types).
template<typename T>
concept IsJSCellType = requires { T::info(); };

// TypeCastTraits specializations for JSCell subclasses.
// This allows using is<>(), dynamicDowncast<>(), downcast<>(), and uncheckedDowncast<>() with JS types.

// Per-type specializations preserve the optimized JSType range checking.
#define JSC_SPECIALIZE_TYPE_CAST_TRAITS(className, firstJSType, lastJSType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::className) \
    static bool isType(const JSC::JSCell& cell) \
    { \
        return JSC::JSCastingHelpers::InheritsTraits<JSC::className>::inherits(&cell); \
    } \
SPECIALIZE_TYPE_TRAITS_END()

FOR_EACH_JS_DYNAMIC_CAST_JS_TYPE_OVERLOAD(JSC_SPECIALIZE_TYPE_CAST_TRAITS)
#undef JSC_SPECIALIZE_TYPE_CAST_TRAITS

// Generic fallback for JSCell subclasses not in the overload list (e.g. WebCore binding types).
// Uses IsJSCellType concept which is SFINAE-friendly with incomplete types.
// The From type is allowed to be JSCell itself (which lacks info() so doesn't satisfy
// IsJSCellType) since JSCell* is the common base pointer returned by JSValue::asCell().
namespace WTF {
template<typename To, typename From>
    requires (IsJSCellType<std::remove_const_t<To>> && (IsJSCellType<std::remove_const_t<From>> || std::is_same_v<std::remove_const_t<From>, JSC::JSCell>))
struct TypeCastTraits<To, From, false> {
    static bool isOfType(From& source)
    {
        return JSC::JSCastingHelpers::InheritsTraits<std::remove_const_t<To>>::inherits(&source);
    }
};

// JSValue overloads for is, dynamicDowncast, downcast, and uncheckedDowncast.
// Uses explicit JSC::JSValue& parameter type which is more specialized than the
// deduced From& in WTF's overloads, so these win in partial ordering.
// These use JSCastingHelpers directly rather than delegating to the WTF generic
// versions, because JSCell doesn't satisfy IsJSCellType (it has no info()) so
// TypeCastTraits<To, JSCell> would hit the default static_assert.

template<typename To>
    requires IsJSCellType<To>
inline bool is(JSC::JSValue& value)
{
    return value.isCell() && JSC::JSCastingHelpers::InheritsTraits<To>::inherits(value.asCell());
}

template<typename To>
    requires IsJSCellType<To>
inline bool is(const JSC::JSValue& value)
{
    return value.isCell() && JSC::JSCastingHelpers::InheritsTraits<To>::inherits(value.asCell());
}

template<typename To>
    requires IsJSCellType<To>
inline To* dynamicDowncast(JSC::JSValue& value)
{
    if (!value.isCell()) [[unlikely]]
        return nullptr;
    JSC::JSCell* cell = value.asCell();
    if (JSC::JSCastingHelpers::InheritsTraits<To>::inherits(cell)) [[likely]]
        SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<To*>(cell);
    return nullptr;
}

template<typename To>
    requires IsJSCellType<To>
inline To* dynamicDowncast(const JSC::JSValue& value)
{
    if (!value.isCell()) [[unlikely]]
        return nullptr;
    JSC::JSCell* cell = value.asCell();
    if (JSC::JSCastingHelpers::InheritsTraits<To>::inherits(cell)) [[likely]]
        SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<To*>(cell);
    return nullptr;
}

template<typename To>
    requires IsJSCellType<To>
inline To* downcast(JSC::JSValue& value)
{
    RELEASE_ASSERT(value.isCell());
    JSC::JSCell* cell = value.asCell();
    RELEASE_ASSERT(JSC::JSCastingHelpers::InheritsTraits<To>::inherits(cell));
    SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<To*>(cell);
}

template<typename To>
    requires IsJSCellType<To>
inline To* downcast(const JSC::JSValue& value)
{
    RELEASE_ASSERT(value.isCell());
    JSC::JSCell* cell = value.asCell();
    RELEASE_ASSERT(JSC::JSCastingHelpers::InheritsTraits<To>::inherits(cell));
    SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<To*>(cell);
}

template<typename To>
    requires IsJSCellType<To>
inline To* uncheckedDowncast(JSC::JSValue& value)
{
    JSC::JSCell* cell = value.asCell();
#if (ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)) && CPU(X86_64)
    ASSERT_WITH_SECURITY_IMPLICATION(value.isCell());
    if (!JSC::JSCastingHelpers::InheritsTraits<To>::inherits(cell)) [[unlikely]]
        JSC::reportZappedCellAndCrash(cell);
#else
    ASSERT_WITH_SECURITY_IMPLICATION(value.isCell() && JSC::JSCastingHelpers::InheritsTraits<To>::inherits(cell));
#endif
    SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<To*>(cell);
}

template<typename To>
    requires IsJSCellType<To>
inline To* uncheckedDowncast(const JSC::JSValue& value)
{
    JSC::JSCell* cell = value.asCell();
#if (ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)) && CPU(X86_64)
    ASSERT_WITH_SECURITY_IMPLICATION(value.isCell());
    if (!JSC::JSCastingHelpers::InheritsTraits<To>::inherits(cell)) [[unlikely]]
        JSC::reportZappedCellAndCrash(cell);
#else
    ASSERT_WITH_SECURITY_IMPLICATION(value.isCell() && JSC::JSCastingHelpers::InheritsTraits<To>::inherits(cell));
#endif
    SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<To*>(cell);
}

} // namespace WTF

// Re-export the JSValue overloads so unqualified lookup finds them.
// The using declarations in TypeCasts.h only see overloads declared before that point.
using WTF::is;
using WTF::dynamicDowncast;
using WTF::downcast;
using WTF::uncheckedDowncast;
