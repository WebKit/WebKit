/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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
    macro(arrayIteratorNextIndex) \
    macro(arrayIterationKind) \
    macro(arrayIteratorNext) \
    macro(arrayIteratorIsDone) \
    macro(arrayIteratorKind) \
    macro(assert) \
    macro(charCodeAt) \
    macro(executor) \
    macro(isView) \
    macro(iteratedObject) \
    macro(iteratedString) \
    macro(stringIteratorNextIndex) \
    macro(promise) \
    macro(Object) \
    macro(Number) \
    macro(Array) \
    macro(ArrayBuffer) \
    macro(RegExp) \
    macro(Promise) \
    macro(Reflect) \
    macro(InternalPromise) \
    macro(trunc) \
    macro(create) \
    macro(defineProperty) \
    macro(getPrototypeOf) \
    macro(getOwnPropertyDescriptor) \
    macro(getOwnPropertyNames) \
    macro(ownKeys) \
    macro(Set) \
    macro(typedArrayLength) \
    macro(typedArraySort) \
    macro(typedArrayGetOriginalConstructor) \
    macro(typedArraySubarrayCreate) \
    macro(BuiltinLog) \
    macro(BuiltinDescribe) \
    macro(homeObject) \
    macro(templateRegistryKey) \
    macro(enqueueJob) \
    macro(hostPromiseRejectionTracker) \
    macro(promiseIsHandled) \
    macro(promiseState) \
    macro(promiseReactions) \
    macro(promiseResult) \
    macro(onFulfilled) \
    macro(onRejected) \
    macro(push) \
    macro(repeatCharacter) \
    macro(capabilities) \
    macro(starDefault) \
    macro(InspectorInstrumentation) \
    macro(get) \
    macro(set) \
    macro(shift) \
    macro(allocateTypedArray) \
    macro(Int8Array) \
    macro(Int16Array) \
    macro(Int32Array) \
    macro(Uint8Array) \
    macro(Uint8ClampedArray) \
    macro(Uint16Array) \
    macro(Uint32Array) \
    macro(Float32Array) \
    macro(Float64Array) \
    macro(exec) \
    macro(generator) \
    macro(generatorNext) \
    macro(generatorState) \
    macro(generatorFrame) \
    macro(generatorValue) \
    macro(generatorThis) \
    macro(syncIterator) \
    macro(nextMethod) \
    macro(asyncGeneratorState) \
    macro(asyncGeneratorSuspendReason) \
    macro(asyncGeneratorQueue) \
    macro(asyncGeneratorQueueFirst) \
    macro(asyncGeneratorQueueLast) \
    macro(asyncGeneratorQueueItemNext) \
    macro(asyncGeneratorQueueItemPrevious) \
    macro(generatorResumeMode) \
    macro(dateTimeFormat) \
    macro(intlSubstituteValue) \
    macro(thisTimeValue) \
    macro(newTargetLocal) \
    macro(derivedConstructor) \
    macro(isTypedArrayView) \
    macro(isBoundFunction) \
    macro(hasInstanceBoundFunction) \
    macro(instanceOf) \
    macro(isArraySlow) \
    macro(isArrayConstructor) \
    macro(isConstructor) \
    macro(concatMemcpy) \
    macro(appendMemcpy) \
    macro(regExpCreate) \
    macro(replaceUsingRegExp) \
    macro(replaceUsingStringSearch) \
    macro(makeTypeError) \
    macro(mapBucket) \
    macro(mapBucketHead) \
    macro(mapBucketNext) \
    macro(mapBucketKey) \
    macro(mapBucketValue) \
    macro(mapIteratorKind) \
    macro(setBucket) \
    macro(setBucketHead) \
    macro(setBucketNext) \
    macro(setBucketKey) \
    macro(setIteratorKind) \
    macro(regExpBuiltinExec) \
    macro(regExpMatchFast) \
    macro(regExpProtoFlagsGetter) \
    macro(regExpProtoGlobalGetter) \
    macro(regExpProtoIgnoreCaseGetter) \
    macro(regExpProtoMultilineGetter) \
    macro(regExpProtoSourceGetter) \
    macro(regExpProtoStickyGetter) \
    macro(regExpProtoUnicodeGetter) \
    macro(regExpPrototypeSymbolReplace) \
    macro(regExpSearchFast) \
    macro(regExpSplitFast) \
    macro(regExpTestFast) \
    macro(stringIncludesInternal) \
    macro(stringSplitFast) \
    macro(stringSubstrInternal) \
    macro(makeBoundFunction) \
    macro(hasOwnLengthProperty) \
    macro(importModule) \
    macro(propertyIsEnumerable) \
    macro(meta) \
    macro(webAssemblyCompileStreamingInternal) \
    macro(webAssemblyInstantiateStreamingInternal) \

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
    BuiltinNames(VM*, CommonIdentifiers*);

    SymbolImpl* lookUpPrivateName(const Identifier&) const;
    Identifier getPublicName(VM&, SymbolImpl*) const;
    
    void appendExternalName(const Identifier& publicName, const Identifier& privateName);

    JSC_FOREACH_BUILTIN_FUNCTION_NAME(DECLARE_BUILTIN_IDENTIFIER_ACCESSOR_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_IDENTIFIER_ACCESSOR_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(DECLARE_BUILTIN_SYMBOL_ACCESSOR)
    const JSC::Identifier& dollarVMPublicName() const { return m_dollarVMName; }
    const JSC::Identifier& dollarVMPrivateName() const { return m_dollarVMPrivateName; }
    const JSC::Identifier& polyProtoName() const { return m_polyProtoPrivateName; }

private:
    void checkPublicToPrivateMapConsistency(UniquedStringImpl* publicName, UniquedStringImpl* privateName);

    Identifier m_emptyIdentifier;
    JSC_FOREACH_BUILTIN_FUNCTION_NAME(DECLARE_BUILTIN_NAMES_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_NAMES_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(DECLARE_BUILTIN_SYMBOLS_IN_JSC)
    const JSC::Identifier m_dollarVMName;
    const JSC::Identifier m_dollarVMPrivateName;
    const JSC::Identifier m_polyProtoPrivateName;
    typedef HashMap<RefPtr<UniquedStringImpl>, SymbolImpl*, IdentifierRepHash> BuiltinNamesMap;
    BuiltinNamesMap m_publicToPrivateMap;
};

inline SymbolImpl* BuiltinNames::lookUpPrivateName(const Identifier& ident) const
{
    auto iter = m_publicToPrivateMap.find(ident.impl());
    if (iter != m_publicToPrivateMap.end())
        return iter->value;
    return nullptr;
}

inline Identifier BuiltinNames::getPublicName(VM& vm, SymbolImpl* symbol) const
{
    if (symbol->isPrivate())
        return Identifier::fromString(&vm, symbol);
    // We have special handling for well-known symbols.
    ASSERT(symbol->startsWith("Symbol."));
    return Identifier::fromString(&vm, makeString(String(symbol->substring(strlen("Symbol."))), "Symbol"));
}

inline void BuiltinNames::checkPublicToPrivateMapConsistency(UniquedStringImpl* publicName, UniquedStringImpl* privateName)
{
#ifndef NDEBUG
    for (const auto& key : m_publicToPrivateMap.keys())
        ASSERT(String(publicName) != *key);

    ASSERT(privateName->isSymbol());
    SymbolImpl* symbol = static_cast<SymbolImpl*>(privateName);
    if (symbol->isPrivate()) {
        // This guarantees that we can get public symbols from private symbols by using content of private symbols.
        ASSERT(String(symbol) == *publicName);
    } else {
        // We have a hack in m_publicToPrivateMap: adding non-private Symbol with readable name to use it
        // in builtin code. The example is @iteratorSymbol => Symbol.iterator mapping. To allow the reverse
        // transformation, we ensure that non-private symbol mapping has xxxSymbol => Symbol.xxx.
        ASSERT(makeString(String(symbol), "Symbol") == makeString("Symbol.", String(publicName)));
    }
#else
    UNUSED_PARAM(publicName);
    UNUSED_PARAM(privateName);
#endif
}

inline void BuiltinNames::appendExternalName(const Identifier& publicName, const Identifier& privateName)
{
    checkPublicToPrivateMapConsistency(publicName.impl(), privateName.impl());
    m_publicToPrivateMap.add(publicName.impl(), static_cast<SymbolImpl*>(privateName.impl()));
}

} // namespace JSC
