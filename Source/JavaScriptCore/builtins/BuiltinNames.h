/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include "BuiltinUtils.h"
#include "BytecodeIntrinsicRegistry.h"
#include "CommonIdentifiers.h"
#include "JSCBuiltins.h"

namespace JSC {

#define DECLARE_BUILTIN_NAMES_IN_JSC(name) const JSC::Identifier m_##name;
#define DECLARE_BUILTIN_SYMBOLS_IN_JSC(name) const JSC::Identifier m_##name##Symbol; const JSC::Identifier m_##name##SymbolPrivateIdentifier;
#define DECLARE_BUILTIN_SYMBOL_ACCESSOR(name) \
    const JSC::Identifier& name##Symbol() const { return m_##name##Symbol; }
#define DECLARE_BUILTIN_IDENTIFIER_ACCESSOR_IN_JSC(name) \
    const JSC::Identifier& name##PublicName() const { return m_##name; } \
    JSC::Identifier name##PrivateName() const { return JSC::Identifier::fromUid(*bitwise_cast<SymbolImpl*>(&JSC::Symbols::name##PrivateName)); }


#define JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(macro) \
    JSC_COMMON_BYTECODE_INTRINSIC_FUNCTIONS_EACH_NAME(macro) \
    JSC_COMMON_BYTECODE_INTRINSIC_CONSTANTS_EACH_NAME(macro) \
    macro(add) \
    macro(applyFunction) \
    macro(arraySpeciesCreate) \
    macro(assert) \
    macro(callFunction) \
    macro(charCodeAt) \
    macro(executor) \
    macro(isView) \
    macro(iteratedObject) \
    macro(iteratedString) \
    macro(promise) \
    macro(promiseOrCapability) \
    macro(Object) \
    macro(Number) \
    macro(Array) \
    macro(ArrayBuffer) \
    macro(RegExp) \
    macro(min) \
    macro(trunc) \
    macro(create) \
    macro(defineProperty) \
    macro(defaultPromiseThen) \
    macro(Set) \
    macro(throwTypeErrorFunction) \
    macro(typedArrayLength) \
    macro(typedArrayContentType) \
    macro(typedArraySort) \
    macro(typedArrayGetOriginalConstructor) \
    macro(typedArraySubarrayCreate) \
    macro(BuiltinLog) \
    macro(BuiltinDescribe) \
    macro(homeObject) \
    macro(enqueueJob) \
    macro(hostPromiseRejectionTracker) \
    macro(onFulfilled) \
    macro(onRejected) \
    macro(push) \
    macro(repeatCharacter) \
    macro(starDefault) \
    macro(starNamespace) \
    macro(keys) \
    macro(values) \
    macro(get) \
    macro(set) \
    macro(shift) \
    macro(Int8Array) \
    macro(Int16Array) \
    macro(Int32Array) \
    macro(Uint8Array) \
    macro(Uint8ClampedArray) \
    macro(Uint16Array) \
    macro(Uint32Array) \
    macro(Float32Array) \
    macro(Float64Array) \
    macro(BigInt64Array) \
    macro(BigUint64Array) \
    macro(exec) \
    macro(generator) \
    macro(generatorNext) \
    macro(generatorState) \
    macro(generatorFrame) \
    macro(generatorValue) \
    macro(generatorThis) \
    macro(generatorResumeMode) \
    macro(syncIterator) \
    macro(nextMethod) \
    macro(asyncGeneratorQueueItemNext) \
    macro(dateTimeFormat) \
    macro(this) \
    macro(thisTimeValue) \
    macro(newTargetLocal) \
    macro(derivedConstructor) \
    macro(isTypedArrayView) \
    macro(isSharedTypedArrayView) \
    macro(isDetached) \
    macro(typedArrayDefaultComparator) \
    macro(isBoundFunction) \
    macro(hasInstanceBoundFunction) \
    macro(instanceOf) \
    macro(isArraySlow) \
    macro(sameValue) \
    macro(concatMemcpy) \
    macro(appendMemcpy) \
    macro(regExpCreate) \
    macro(isRegExp) \
    macro(replaceUsingRegExp) \
    macro(replaceUsingStringSearch) \
    macro(replaceAllUsingStringSearch) \
    macro(makeTypeError) \
    macro(AggregateError) \
    macro(mapBucketHead) \
    macro(mapBucketNext) \
    macro(mapBucketKey) \
    macro(mapBucketValue) \
    macro(setBucketHead) \
    macro(setBucketNext) \
    macro(setBucketKey) \
    macro(setPrototypeDirect) \
    macro(regExpBuiltinExec) \
    macro(regExpMatchFast) \
    macro(regExpProtoFlagsGetter) \
    macro(regExpProtoGlobalGetter) \
    macro(regExpProtoIgnoreCaseGetter) \
    macro(regExpProtoMultilineGetter) \
    macro(regExpProtoSourceGetter) \
    macro(regExpProtoStickyGetter) \
    macro(regExpProtoUnicodeGetter) \
    macro(regExpPrototypeSymbolMatch) \
    macro(regExpPrototypeSymbolReplace) \
    macro(regExpSearchFast) \
    macro(regExpSplitFast) \
    macro(regExpTestFast) \
    macro(regExpStringIteratorRegExp) \
    macro(regExpStringIteratorString) \
    macro(regExpStringIteratorGlobal) \
    macro(regExpStringIteratorUnicode) \
    macro(regExpStringIteratorDone) \
    macro(stringIncludesInternal) \
    macro(stringIndexOfInternal) \
    macro(stringSplitFast) \
    macro(stringSubstringInternal) \
    macro(makeBoundFunction) \
    macro(hasOwnLengthProperty) \
    macro(importModule) \
    macro(copyDataProperties) \
    macro(meta) \
    macro(webAssemblyCompileStreamingInternal) \
    macro(webAssemblyInstantiateStreamingInternal) \
    macro(instanceFieldInitializer) \
    macro(privateBrand) \
    macro(privateClassBrand) \
    macro(hasOwnPropertyFunction) \
    macro(createPrivateSymbol) \
    macro(entries)


namespace Symbols {
#define DECLARE_BUILTIN_STATIC_SYMBOLS(name) extern JS_EXPORT_PRIVATE SymbolImpl::StaticSymbolImpl name##Symbol;
JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(DECLARE_BUILTIN_STATIC_SYMBOLS)
#undef DECLARE_BUILTIN_STATIC_SYMBOLS

#define DECLARE_BUILTIN_PRIVATE_NAMES(name) extern JS_EXPORT_PRIVATE SymbolImpl::StaticSymbolImpl name##PrivateName;
JSC_FOREACH_BUILTIN_FUNCTION_NAME(DECLARE_BUILTIN_PRIVATE_NAMES)
JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_PRIVATE_NAMES)
#undef DECLARE_BUILTIN_PRIVATE_NAMES

extern JS_EXPORT_PRIVATE SymbolImpl::StaticSymbolImpl dollarVMPrivateName;
extern JS_EXPORT_PRIVATE SymbolImpl::StaticSymbolImpl polyProtoPrivateName;
}

class BuiltinNames {
    WTF_MAKE_NONCOPYABLE(BuiltinNames); WTF_MAKE_FAST_ALLOCATED;
    
public:
    BuiltinNames(VM&, CommonIdentifiers*);

    PrivateSymbolImpl* lookUpPrivateName(const Identifier&) const;
    PrivateSymbolImpl* lookUpPrivateName(const String&) const;
    PrivateSymbolImpl* lookUpPrivateName(const LChar*, unsigned length) const;
    PrivateSymbolImpl* lookUpPrivateName(const UChar*, unsigned length) const;

    SymbolImpl* lookUpWellKnownSymbol(const Identifier&) const;
    SymbolImpl* lookUpWellKnownSymbol(const String&) const;
    SymbolImpl* lookUpWellKnownSymbol(const LChar*, unsigned length) const;
    SymbolImpl* lookUpWellKnownSymbol(const UChar*, unsigned length) const;
    
    void appendExternalName(const Identifier& publicName, const Identifier& privateName);

    JSC_FOREACH_BUILTIN_FUNCTION_NAME(DECLARE_BUILTIN_IDENTIFIER_ACCESSOR_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_IDENTIFIER_ACCESSOR_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(DECLARE_BUILTIN_SYMBOL_ACCESSOR)
    const JSC::Identifier& dollarVMPublicName() const { return m_dollarVMName; }
    const JSC::Identifier& dollarVMPrivateName() const { return m_dollarVMPrivateName; }
    const JSC::Identifier& polyProtoName() const { return m_polyProtoPrivateName; }
    const JSC::Identifier& intlLegacyConstructedSymbol() const { return m_intlLegacyConstructedSymbol; }

private:
    void checkPublicToPrivateMapConsistency(UniquedStringImpl* privateName);

    Identifier m_emptyIdentifier;
    JSC_FOREACH_BUILTIN_FUNCTION_NAME(DECLARE_BUILTIN_NAMES_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_NAMES_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(DECLARE_BUILTIN_SYMBOLS_IN_JSC)
    const JSC::Identifier m_intlLegacyConstructedSymbol;
    const JSC::Identifier m_dollarVMName;
    const JSC::Identifier m_dollarVMPrivateName;
    const JSC::Identifier m_polyProtoPrivateName;
    using PrivateNameSet = HashSet<String>;
    using WellKnownSymbolMap = HashMap<String, SymbolImpl*>;
    PrivateNameSet m_privateNameSet;
    WellKnownSymbolMap m_wellKnownSymbolsMap;
};

inline PrivateSymbolImpl* BuiltinNames::lookUpPrivateName(const Identifier& ident) const
{
    return lookUpPrivateName(ident.impl());
}

inline SymbolImpl* BuiltinNames::lookUpWellKnownSymbol(const Identifier& ident) const
{
    return lookUpWellKnownSymbol(ident.impl());
}

inline void BuiltinNames::checkPublicToPrivateMapConsistency(UniquedStringImpl* privateName)
{
#if ASSERT_ENABLED
    for (const auto& key : m_privateNameSet)
        ASSERT(String(privateName) != key);
    ASSERT(privateName->isSymbol());
    ASSERT(static_cast<SymbolImpl*>(privateName)->isPrivate());
#else
    UNUSED_PARAM(privateName);
#endif
}

inline void BuiltinNames::appendExternalName(const Identifier& publicName, const Identifier& privateName)
{
    ASSERT_UNUSED(publicName, String(publicName.impl()) == String(privateName.impl()));
    checkPublicToPrivateMapConsistency(privateName.impl());
    m_privateNameSet.add(privateName.impl());
}

} // namespace JSC
