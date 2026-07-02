/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Yusuke Suzuki <yusukesuzuki@slowstart.org>.
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

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/WasmFormat.h>
#include <JavaScriptCore/WasmTypeDefinition.h>

namespace JSC { namespace Wasm {

inline TypeInformation& TypeInformation::singleton()
{
    static TypeInformation* theOne;
    static std::once_flag typeInformationFlag;

    std::call_once(typeInformationFlag, [] () {
        theOne = new TypeInformation;
    });
    return *theOne;
}

inline RefPtr<const RTT> TypeInformation::tryGetRTT(TypeIndex typeIndex)
{
    if (typeIndexIsType(typeIndex) || typeIndex == invalidTypeIndex)
        return nullptr;
    return std::bit_cast<const RTT*>(typeIndex);
}

// Provider-based typeDefinitionForStruct. Builds the RTTStructPayload's
// FixedVectors directly from the provider, skipping the intermediate
// Vector<FieldType> the Vector-based overload would require. Accumulates
// flags (hasRefFieldTypes, hasRecursiveReference) and field offsets in
// place. External RTT refs are anchored inline in each entry's rttAnchor.
template<typename FieldProvider>
Ref<const RTT> TypeInformation::typeDefinitionForStructFromProvider(StructFieldCount fieldCount, FieldProvider&& provider)
{
    bool hasRefFieldTypes = false;
    bool hasRecursiveReference = false;
    unsigned currentOffset = 0;

    auto entries = FixedVector<StructFieldEntry>::createWithSizeFromGenerator(fieldCount, [&](size_t i) -> StructFieldEntry {
        FieldType f = provider(i);
        hasRefFieldTypes |= isRefType(f.type);
        hasRecursiveReference |= isRefWithRecursiveReference(f.type);
        currentOffset = WTF::roundUpToMultipleOf(typeAlignmentInBytes(f.type), currentOffset);
        unsigned offset = currentOffset;
        currentOffset += typeSizeInBytes(f.type);
        RefPtr<const RTT> anchor;
        if (f.type.is<Type>())
            anchor = extractExternalRTT(f.type.as<Type>());
        return StructFieldEntry { f, offset, WTF::move(anchor) };
    });
    size_t instancePayloadSize = WTF::roundUpToMultipleOf<sizeof(uint64_t)>(currentOffset);

    RTTStructPayload payload {
        WTF::move(entries),
        instancePayloadSize,
        hasRefFieldTypes,
        hasRecursiveReference,
    };
    auto rtt = RTT::tryCreateStruct(/* isFinalType */ true, WTF::move(payload));
    RELEASE_ASSERT(rtt);
    return rtt.releaseNonNull();
}

// Provider-based typeDefinitionForFunction. Signature layout: returns first,
// then arguments. Builds the FixedVector<TypeSlot> in place from two
// providers. External RTT refs are anchored inline in each slot.
template<typename ReturnProvider, typename ArgProvider>
Ref<const RTT> TypeInformation::typeDefinitionForFunctionFromProviders(FunctionArgCount retCount, ReturnProvider&& returnsProvider, FunctionArgCount argCount, ArgProvider&& argsProvider)
{
    bool hasRecursiveReference = false;
    bool argumentsOrResultsIncludeI64 = false;
    bool argumentsOrResultsIncludeV128 = false;
    bool argumentsOrResultsIncludeExnref = false;

    auto signature = FixedVector<TypeSlot>::createWithSizeFromGenerator(static_cast<size_t>(retCount) + static_cast<size_t>(argCount), [&](size_t i) -> TypeSlot {
        Type t = (i < retCount) ? returnsProvider(i) : argsProvider(i - retCount);
        hasRecursiveReference |= isRefWithRecursiveReference(t);
        argumentsOrResultsIncludeI64 |= t.isI64();
        argumentsOrResultsIncludeV128 |= t.isV128();
        argumentsOrResultsIncludeExnref |= isExnref(t);
        return TypeSlot { t, extractExternalRTT(t) };
    });

    RTTFunctionPayload payload {
        argCount,
        retCount,
        WTF::move(signature),
        argumentsOrResultsIncludeI64,
        argumentsOrResultsIncludeV128,
        argumentsOrResultsIncludeExnref,
        hasRecursiveReference,
    };
    auto rtt = RTT::tryCreateFunction(/* isFinalType */ true, WTF::move(payload));
    RELEASE_ASSERT(rtt);
    return rtt.releaseNonNull();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
