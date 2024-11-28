/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#include "InlineCacheCompiler.h"

#if ENABLE(JIT)

#include "AccessCaseSnippetParams.h"
#include "BaselineJITCode.h"
#include "BinarySwitch.h"
#include "CCallHelpers.h"
#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "DOMJITCallDOMGetterSnippet.h"
#include "DOMJITGetterSetter.h"
#include "DirectArguments.h"
#include "FullCodeOrigin.h"
#include "GetterSetter.h"
#include "GetterSetterAccessCase.h"
#include "Heap.h"
#include "InstanceOfAccessCase.h"
#include "IntrinsicGetterAccessCase.h"
#include "JIT.h"
#include "JITOperations.h"
#include "JITThunks.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "JSTypedArrays.h"
#include "JSWebAssemblyInstance.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "MegamorphicCache.h"
#include "ModuleNamespaceAccessCase.h"
#include "ScopedArguments.h"
#include "ScratchRegisterAllocator.h"
#include "SharedJITStubSet.h"
#include "StructureInlines.h"
#include "StructureStubClearingWatchpoint.h"
#include "StructureStubInfo.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"
#include "WebAssemblyModuleRecord.h"
#include <wtf/CommaPrinter.h>
#include <wtf/ListDump.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

namespace InlineCacheCompilerInternal {
static constexpr bool verbose = false;
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PolymorphicAccess);

void AccessGenerationResult::dump(PrintStream& out) const
{
    out.print(m_kind);
    if (m_handler)
        out.print(":", *m_handler);
}

void InlineCacheHandler::dump(PrintStream& out) const
{
    if (m_callTarget)
        out.print(m_callTarget);
}

static TypedArrayType toTypedArrayType(AccessCase::AccessType accessType)
{
    switch (accessType) {
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayInt8In:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayInt8In:
        return TypeInt8;
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8In:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8In:
        return TypeUint8;
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayUint8ClampedIn:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedIn:
        return TypeUint8Clamped;
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayInt16In:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayInt16In:
        return TypeInt16;
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayUint16In:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayUint16In:
        return TypeUint16;
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayInt32In:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayInt32In:
        return TypeInt32;
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayUint32In:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayUint32In:
        return TypeUint32;
    case AccessCase::IndexedTypedArrayFloat16Load:
    case AccessCase::IndexedTypedArrayFloat16Store:
    case AccessCase::IndexedTypedArrayFloat16In:
    case AccessCase::IndexedResizableTypedArrayFloat16Load:
    case AccessCase::IndexedResizableTypedArrayFloat16Store:
    case AccessCase::IndexedResizableTypedArrayFloat16In:
        return TypeFloat16;
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat32In:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat32In:
        return TypeFloat32;
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedTypedArrayFloat64In:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayFloat64In:
        return TypeFloat64;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static bool forResizableTypedArray(AccessCase::AccessType accessType)
{
    switch (accessType) {
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat16Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat16Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8In:
    case AccessCase::IndexedResizableTypedArrayUint8In:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedIn:
    case AccessCase::IndexedResizableTypedArrayInt16In:
    case AccessCase::IndexedResizableTypedArrayUint16In:
    case AccessCase::IndexedResizableTypedArrayInt32In:
    case AccessCase::IndexedResizableTypedArrayUint32In:
    case AccessCase::IndexedResizableTypedArrayFloat16In:
    case AccessCase::IndexedResizableTypedArrayFloat32In:
    case AccessCase::IndexedResizableTypedArrayFloat64In:
        return true;
    default:
        return false;
    }
}

static bool needsScratchFPR(AccessCase::AccessType type)
{
    switch (type) {
    case AccessCase::Load:
    case AccessCase::LoadMegamorphic:
    case AccessCase::StoreMegamorphic:
    case AccessCase::InMegamorphic:
    case AccessCase::Transition:
    case AccessCase::Delete:
    case AccessCase::DeleteNonConfigurable:
    case AccessCase::DeleteMiss:
    case AccessCase::Replace:
    case AccessCase::Miss:
    case AccessCase::GetGetter:
    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::InHit:
    case AccessCase::InMiss:
    case AccessCase::CheckPrivateBrand:
    case AccessCase::SetPrivateBrand:
    case AccessCase::ArrayLength:
    case AccessCase::StringLength:
    case AccessCase::DirectArgumentsLength:
    case AccessCase::ScopedArgumentsLength:
    case AccessCase::ModuleNamespaceLoad:
    case AccessCase::ProxyObjectIn:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
    case AccessCase::InstanceOfMegamorphic:
    case AccessCase::IndexedProxyObjectIn:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::IndexedProxyObjectStore:
    case AccessCase::IndexedMegamorphicLoad:
    case AccessCase::IndexedMegamorphicStore:
    case AccessCase::IndexedMegamorphicIn:
    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedStringLoad:
    case AccessCase::IndexedNoIndexingMiss:
    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit:
    case AccessCase::IndexedScopedArgumentsInHit:
    case AccessCase::IndexedDirectArgumentsInHit:
    case AccessCase::IndexedTypedArrayInt8In:
    case AccessCase::IndexedTypedArrayUint8In:
    case AccessCase::IndexedTypedArrayUint8ClampedIn:
    case AccessCase::IndexedTypedArrayInt16In:
    case AccessCase::IndexedTypedArrayUint16In:
    case AccessCase::IndexedTypedArrayInt32In:
    case AccessCase::IndexedResizableTypedArrayInt8In:
    case AccessCase::IndexedResizableTypedArrayUint8In:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedIn:
    case AccessCase::IndexedResizableTypedArrayInt16In:
    case AccessCase::IndexedResizableTypedArrayUint16In:
    case AccessCase::IndexedResizableTypedArrayInt32In:
    case AccessCase::IndexedStringInHit:
    case AccessCase::IndexedNoIndexingInMiss:
    // Indexed TypedArray In does not need FPR since it does not load a value.
    case AccessCase::IndexedTypedArrayUint32In:
    case AccessCase::IndexedTypedArrayFloat16In:
    case AccessCase::IndexedTypedArrayFloat32In:
    case AccessCase::IndexedTypedArrayFloat64In:
    case AccessCase::IndexedResizableTypedArrayUint32In:
    case AccessCase::IndexedResizableTypedArrayFloat16In:
    case AccessCase::IndexedResizableTypedArrayFloat32In:
    case AccessCase::IndexedResizableTypedArrayFloat64In:
        return false;
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedTypedArrayFloat16Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat16Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat16Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat16Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedDoubleInHit:
    // Used by TypedArrayLength/TypedArrayByteOffset in the process of boxing their result as a double
    case AccessCase::IntrinsicGetter:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static bool forInBy(AccessCase::AccessType type)
{
    switch (type) {
    case AccessCase::Load:
    case AccessCase::LoadMegamorphic:
    case AccessCase::StoreMegamorphic:
    case AccessCase::Transition:
    case AccessCase::Delete:
    case AccessCase::DeleteNonConfigurable:
    case AccessCase::DeleteMiss:
    case AccessCase::Replace:
    case AccessCase::Miss:
    case AccessCase::GetGetter:
    case AccessCase::ArrayLength:
    case AccessCase::StringLength:
    case AccessCase::DirectArgumentsLength:
    case AccessCase::ScopedArgumentsLength:
    case AccessCase::CheckPrivateBrand:
    case AccessCase::SetPrivateBrand:
    case AccessCase::IndexedMegamorphicLoad:
    case AccessCase::IndexedMegamorphicStore:
    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayFloat16Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat16Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat16Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat16Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedStringLoad:
    case AccessCase::IndexedNoIndexingMiss:
    case AccessCase::InstanceOfMegamorphic:
    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::IndexedProxyObjectStore:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::IntrinsicGetter:
    case AccessCase::ModuleNamespaceLoad:
    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
        return false;
    case AccessCase::ProxyObjectIn:
    case AccessCase::InHit:
    case AccessCase::InMiss:
    case AccessCase::InMegamorphic:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedDoubleInHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit:
    case AccessCase::IndexedScopedArgumentsInHit:
    case AccessCase::IndexedDirectArgumentsInHit:
    case AccessCase::IndexedTypedArrayInt8In:
    case AccessCase::IndexedTypedArrayUint8In:
    case AccessCase::IndexedTypedArrayUint8ClampedIn:
    case AccessCase::IndexedTypedArrayInt16In:
    case AccessCase::IndexedTypedArrayUint16In:
    case AccessCase::IndexedTypedArrayInt32In:
    case AccessCase::IndexedTypedArrayUint32In:
    case AccessCase::IndexedTypedArrayFloat16In:
    case AccessCase::IndexedTypedArrayFloat32In:
    case AccessCase::IndexedTypedArrayFloat64In:
    case AccessCase::IndexedResizableTypedArrayInt8In:
    case AccessCase::IndexedResizableTypedArrayUint8In:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedIn:
    case AccessCase::IndexedResizableTypedArrayInt16In:
    case AccessCase::IndexedResizableTypedArrayUint16In:
    case AccessCase::IndexedResizableTypedArrayInt32In:
    case AccessCase::IndexedResizableTypedArrayUint32In:
    case AccessCase::IndexedResizableTypedArrayFloat16In:
    case AccessCase::IndexedResizableTypedArrayFloat32In:
    case AccessCase::IndexedResizableTypedArrayFloat64In:
    case AccessCase::IndexedStringInHit:
    case AccessCase::IndexedNoIndexingInMiss:
    case AccessCase::IndexedProxyObjectIn:
    case AccessCase::IndexedMegamorphicIn:
        return true;
    }
    return false;
}

#if CPU(ADDRESS64)
static bool isStateless(AccessCase::AccessType type)
{
    switch (type) {
    case AccessCase::Load:
    case AccessCase::Transition:
    case AccessCase::Delete:
    case AccessCase::DeleteNonConfigurable:
    case AccessCase::DeleteMiss:
    case AccessCase::Replace:
    case AccessCase::Miss:
    case AccessCase::GetGetter:
    case AccessCase::CheckPrivateBrand:
    case AccessCase::SetPrivateBrand:
    case AccessCase::IndexedNoIndexingMiss:
    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::ProxyObjectIn:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::IntrinsicGetter:
    case AccessCase::ModuleNamespaceLoad:
    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
    case AccessCase::InHit:
    case AccessCase::InMiss:
    case AccessCase::IndexedNoIndexingInMiss:
        return false;

    case AccessCase::InMegamorphic:
    case AccessCase::LoadMegamorphic:
    case AccessCase::StoreMegamorphic:
    case AccessCase::ArrayLength:
    case AccessCase::StringLength:
    case AccessCase::DirectArgumentsLength:
    case AccessCase::ScopedArgumentsLength:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::IndexedMegamorphicLoad:
    case AccessCase::IndexedMegamorphicStore:
    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayFloat16Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat16Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat16Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat16Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedStringLoad:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedDoubleInHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit:
    case AccessCase::IndexedScopedArgumentsInHit:
    case AccessCase::IndexedDirectArgumentsInHit:
    case AccessCase::IndexedTypedArrayInt8In:
    case AccessCase::IndexedTypedArrayUint8In:
    case AccessCase::IndexedTypedArrayUint8ClampedIn:
    case AccessCase::IndexedTypedArrayInt16In:
    case AccessCase::IndexedTypedArrayUint16In:
    case AccessCase::IndexedTypedArrayInt32In:
    case AccessCase::IndexedTypedArrayUint32In:
    case AccessCase::IndexedTypedArrayFloat16In:
    case AccessCase::IndexedTypedArrayFloat32In:
    case AccessCase::IndexedTypedArrayFloat64In:
    case AccessCase::IndexedResizableTypedArrayInt8In:
    case AccessCase::IndexedResizableTypedArrayUint8In:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedIn:
    case AccessCase::IndexedResizableTypedArrayInt16In:
    case AccessCase::IndexedResizableTypedArrayUint16In:
    case AccessCase::IndexedResizableTypedArrayInt32In:
    case AccessCase::IndexedResizableTypedArrayUint32In:
    case AccessCase::IndexedResizableTypedArrayFloat16In:
    case AccessCase::IndexedResizableTypedArrayFloat32In:
    case AccessCase::IndexedResizableTypedArrayFloat64In:
    case AccessCase::IndexedStringInHit:
    case AccessCase::IndexedProxyObjectIn:
    case AccessCase::IndexedMegamorphicIn:
    case AccessCase::InstanceOfMegamorphic:
    case AccessCase::IndexedProxyObjectStore:
        return true;
    }

    return false;
}
#endif

static bool doesJSCalls(AccessCase::AccessType type)
{
    switch (type) {
    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::ProxyObjectIn:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::IndexedProxyObjectIn:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::IndexedProxyObjectStore:
        return true;

    case AccessCase::LoadMegamorphic:
    case AccessCase::StoreMegamorphic:
    case AccessCase::InMegamorphic:
    case AccessCase::Load:
    case AccessCase::Transition:
    case AccessCase::Delete:
    case AccessCase::DeleteNonConfigurable:
    case AccessCase::DeleteMiss:
    case AccessCase::Replace:
    case AccessCase::Miss:
    case AccessCase::GetGetter:
    case AccessCase::CheckPrivateBrand:
    case AccessCase::SetPrivateBrand:
    case AccessCase::IndexedNoIndexingMiss:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::IntrinsicGetter:
    case AccessCase::ModuleNamespaceLoad:
    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
    case AccessCase::InHit:
    case AccessCase::InMiss:
    case AccessCase::IndexedNoIndexingInMiss:
    case AccessCase::ArrayLength:
    case AccessCase::StringLength:
    case AccessCase::DirectArgumentsLength:
    case AccessCase::ScopedArgumentsLength:
    case AccessCase::IndexedMegamorphicLoad:
    case AccessCase::IndexedMegamorphicStore:
    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayFloat16Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat16Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat16Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat16Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedStringLoad:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedDoubleInHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit:
    case AccessCase::IndexedScopedArgumentsInHit:
    case AccessCase::IndexedDirectArgumentsInHit:
    case AccessCase::IndexedTypedArrayInt8In:
    case AccessCase::IndexedTypedArrayUint8In:
    case AccessCase::IndexedTypedArrayUint8ClampedIn:
    case AccessCase::IndexedTypedArrayInt16In:
    case AccessCase::IndexedTypedArrayUint16In:
    case AccessCase::IndexedTypedArrayInt32In:
    case AccessCase::IndexedTypedArrayUint32In:
    case AccessCase::IndexedTypedArrayFloat16In:
    case AccessCase::IndexedTypedArrayFloat32In:
    case AccessCase::IndexedTypedArrayFloat64In:
    case AccessCase::IndexedResizableTypedArrayInt8In:
    case AccessCase::IndexedResizableTypedArrayUint8In:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedIn:
    case AccessCase::IndexedResizableTypedArrayInt16In:
    case AccessCase::IndexedResizableTypedArrayUint16In:
    case AccessCase::IndexedResizableTypedArrayInt32In:
    case AccessCase::IndexedResizableTypedArrayUint32In:
    case AccessCase::IndexedResizableTypedArrayFloat16In:
    case AccessCase::IndexedResizableTypedArrayFloat32In:
    case AccessCase::IndexedResizableTypedArrayFloat64In:
    case AccessCase::IndexedStringInHit:
    case AccessCase::IndexedMegamorphicIn:
    case AccessCase::InstanceOfMegamorphic:
        return false;
    }

    return false;
}


static bool isMegamorphic(AccessCase::AccessType type)
{
    switch (type) {
    case AccessCase::LoadMegamorphic:
    case AccessCase::StoreMegamorphic:
    case AccessCase::InMegamorphic:
    case AccessCase::IndexedMegamorphicLoad:
    case AccessCase::IndexedMegamorphicStore:
    case AccessCase::IndexedMegamorphicIn:
    case AccessCase::InstanceOfMegamorphic:
        return true;

    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::ProxyObjectIn:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::IndexedProxyObjectIn:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::IndexedProxyObjectStore:
    case AccessCase::Load:
    case AccessCase::Transition:
    case AccessCase::Delete:
    case AccessCase::DeleteNonConfigurable:
    case AccessCase::DeleteMiss:
    case AccessCase::Replace:
    case AccessCase::Miss:
    case AccessCase::GetGetter:
    case AccessCase::CheckPrivateBrand:
    case AccessCase::SetPrivateBrand:
    case AccessCase::IndexedNoIndexingMiss:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::IntrinsicGetter:
    case AccessCase::ModuleNamespaceLoad:
    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
    case AccessCase::InHit:
    case AccessCase::InMiss:
    case AccessCase::IndexedNoIndexingInMiss:
    case AccessCase::ArrayLength:
    case AccessCase::StringLength:
    case AccessCase::DirectArgumentsLength:
    case AccessCase::ScopedArgumentsLength:
    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayFloat16Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat16Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat16Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat16Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedStringLoad:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedDoubleInHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit:
    case AccessCase::IndexedScopedArgumentsInHit:
    case AccessCase::IndexedDirectArgumentsInHit:
    case AccessCase::IndexedTypedArrayInt8In:
    case AccessCase::IndexedTypedArrayUint8In:
    case AccessCase::IndexedTypedArrayUint8ClampedIn:
    case AccessCase::IndexedTypedArrayInt16In:
    case AccessCase::IndexedTypedArrayUint16In:
    case AccessCase::IndexedTypedArrayInt32In:
    case AccessCase::IndexedTypedArrayUint32In:
    case AccessCase::IndexedTypedArrayFloat16In:
    case AccessCase::IndexedTypedArrayFloat32In:
    case AccessCase::IndexedTypedArrayFloat64In:
    case AccessCase::IndexedResizableTypedArrayInt8In:
    case AccessCase::IndexedResizableTypedArrayUint8In:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedIn:
    case AccessCase::IndexedResizableTypedArrayInt16In:
    case AccessCase::IndexedResizableTypedArrayUint16In:
    case AccessCase::IndexedResizableTypedArrayInt32In:
    case AccessCase::IndexedResizableTypedArrayUint32In:
    case AccessCase::IndexedResizableTypedArrayFloat16In:
    case AccessCase::IndexedResizableTypedArrayFloat32In:
    case AccessCase::IndexedResizableTypedArrayFloat64In:
    case AccessCase::IndexedStringInHit:
        return false;
    }

    return false;
}

bool canBeViaGlobalProxy(AccessCase::AccessType type)
{
    switch (type) {
    case AccessCase::Load:
    case AccessCase::Miss:
    case AccessCase::GetGetter:
    case AccessCase::Replace:
    case AccessCase::Getter:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::Setter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter:
        return true;
    case AccessCase::Transition:
    case AccessCase::Delete:
    case AccessCase::DeleteNonConfigurable:
    case AccessCase::DeleteMiss:
    case AccessCase::CheckPrivateBrand:
    case AccessCase::SetPrivateBrand:
    case AccessCase::IndexedNoIndexingMiss:
    case AccessCase::ProxyObjectIn:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::IndexedProxyObjectIn:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::IndexedProxyObjectStore:
    case AccessCase::IntrinsicGetter:
    case AccessCase::ModuleNamespaceLoad:
    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
    case AccessCase::InHit:
    case AccessCase::InMiss:
    case AccessCase::IndexedNoIndexingInMiss:
    case AccessCase::InMegamorphic:
    case AccessCase::LoadMegamorphic:
    case AccessCase::StoreMegamorphic:
    case AccessCase::ArrayLength:
    case AccessCase::StringLength:
    case AccessCase::DirectArgumentsLength:
    case AccessCase::ScopedArgumentsLength:
    case AccessCase::IndexedMegamorphicLoad:
    case AccessCase::IndexedMegamorphicStore:
    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayFloat16Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat16Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat16Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat16Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedStringLoad:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedDoubleInHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit:
    case AccessCase::IndexedScopedArgumentsInHit:
    case AccessCase::IndexedDirectArgumentsInHit:
    case AccessCase::IndexedTypedArrayInt8In:
    case AccessCase::IndexedTypedArrayUint8In:
    case AccessCase::IndexedTypedArrayUint8ClampedIn:
    case AccessCase::IndexedTypedArrayInt16In:
    case AccessCase::IndexedTypedArrayUint16In:
    case AccessCase::IndexedTypedArrayInt32In:
    case AccessCase::IndexedTypedArrayUint32In:
    case AccessCase::IndexedTypedArrayFloat16In:
    case AccessCase::IndexedTypedArrayFloat32In:
    case AccessCase::IndexedTypedArrayFloat64In:
    case AccessCase::IndexedResizableTypedArrayInt8In:
    case AccessCase::IndexedResizableTypedArrayUint8In:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedIn:
    case AccessCase::IndexedResizableTypedArrayInt16In:
    case AccessCase::IndexedResizableTypedArrayUint16In:
    case AccessCase::IndexedResizableTypedArrayInt32In:
    case AccessCase::IndexedResizableTypedArrayUint32In:
    case AccessCase::IndexedResizableTypedArrayFloat16In:
    case AccessCase::IndexedResizableTypedArrayFloat32In:
    case AccessCase::IndexedResizableTypedArrayFloat64In:
    case AccessCase::IndexedStringInHit:
    case AccessCase::IndexedMegamorphicIn:
    case AccessCase::InstanceOfMegamorphic:
        return false;
    }

    return false;
}

void InlineCacheCompiler::restoreScratch()
{
    m_allocator->restoreReusedRegistersByPopping(*m_jit, m_preservedReusedRegisterState);
}

inline bool InlineCacheCompiler::useHandlerIC() const
{
    return m_stubInfo.useHandlerIC();
}

void InlineCacheCompiler::succeed()
{
    restoreScratch();
    if (useHandlerIC()) {
        emitDataICEpilogue(*m_jit);
        m_jit->ret();
        return;
    }
    if (m_stubInfo.useDataIC) {
        m_jit->farJump(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfDoneLocation()), JSInternalPtrTag);
        return;
    }
    m_success.append(m_jit->jump());
}

const ScalarRegisterSet& InlineCacheCompiler::liveRegistersForCall()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();
    return m_liveRegistersForCall;
}

const ScalarRegisterSet& InlineCacheCompiler::liveRegistersToPreserveAtExceptionHandlingCallSite()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();
    return m_liveRegistersToPreserveAtExceptionHandlingCallSite;
}

static RegisterSetBuilder calleeSaveRegisters()
{
    return RegisterSetBuilder(RegisterSetBuilder::vmCalleeSaveRegisters())
        .filter(RegisterSetBuilder::calleeSaveRegisters())
        .merge(RegisterSetBuilder::reservedHardwareRegisters())
        .merge(RegisterSetBuilder::stackRegisters());
}

const ScalarRegisterSet& InlineCacheCompiler::calculateLiveRegistersForCallAndExceptionHandling()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling) {
        m_calculatedRegistersForCallAndExceptionHandling = true;

        m_liveRegistersToPreserveAtExceptionHandlingCallSite = m_jit->codeBlock()->jitCode()->liveRegistersToPreserveAtExceptionHandlingCallSite(m_jit->codeBlock(), m_stubInfo.callSiteIndex).buildScalarRegisterSet();
        m_needsToRestoreRegistersIfException = m_liveRegistersToPreserveAtExceptionHandlingCallSite.numberOfSetRegisters() > 0;
        if (m_needsToRestoreRegistersIfException) {
            RELEASE_ASSERT(JSC::JITCode::isOptimizingJIT(m_jit->codeBlock()->jitType()));
            ASSERT(!useHandlerIC());
        }

        auto liveRegistersForCall = RegisterSetBuilder(m_liveRegistersToPreserveAtExceptionHandlingCallSite.toRegisterSet(), m_allocator->usedRegisters());
        if (m_stubInfo.useDataIC)
            liveRegistersForCall.add(m_stubInfo.m_stubInfoGPR, IgnoreVectors);
        liveRegistersForCall.exclude(calleeSaveRegisters().buildAndValidate().includeWholeRegisterWidth());
        m_liveRegistersForCall = liveRegistersForCall.buildScalarRegisterSet();
    }
    return m_liveRegistersForCall;
}

auto InlineCacheCompiler::preserveLiveRegistersToStackForCall(const RegisterSet& extra) -> SpillState
{
    RegisterSetBuilder liveRegisters = liveRegistersForCall().toRegisterSet();
    liveRegisters.merge(extra);
    liveRegisters.filter(RegisterSetBuilder::allScalarRegisters());

    unsigned extraStackPadding = 0;
    unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(*m_jit, liveRegisters.buildAndValidate(), extraStackPadding);
    RELEASE_ASSERT(liveRegisters.buildAndValidate().numberOfSetRegisters() == liveRegisters.buildScalarRegisterSet().numberOfSetRegisters(),
        liveRegisters.buildAndValidate().numberOfSetRegisters(),
        liveRegisters.buildScalarRegisterSet().numberOfSetRegisters());
    RELEASE_ASSERT(liveRegisters.buildScalarRegisterSet().numberOfSetRegisters() || !numberOfStackBytesUsedForRegisterPreservation,
        liveRegisters.buildScalarRegisterSet().numberOfSetRegisters(),
        numberOfStackBytesUsedForRegisterPreservation);
    return SpillState {
        liveRegisters.buildScalarRegisterSet(),
        numberOfStackBytesUsedForRegisterPreservation
    };
}

auto InlineCacheCompiler::preserveLiveRegistersToStackForCallWithoutExceptions() -> SpillState
{
    RegisterSetBuilder liveRegisters = m_allocator->usedRegisters();
    if (m_stubInfo.useDataIC)
        liveRegisters.add(m_stubInfo.m_stubInfoGPR, IgnoreVectors);
    liveRegisters.exclude(calleeSaveRegisters().buildAndValidate().includeWholeRegisterWidth());
    liveRegisters.filter(RegisterSetBuilder::allScalarRegisters());

    constexpr unsigned extraStackPadding = 0;
    unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(*m_jit, liveRegisters.buildAndValidate(), extraStackPadding);
    RELEASE_ASSERT(liveRegisters.buildAndValidate().numberOfSetRegisters() == liveRegisters.buildScalarRegisterSet().numberOfSetRegisters(),
        liveRegisters.buildAndValidate().numberOfSetRegisters(),
        liveRegisters.buildScalarRegisterSet().numberOfSetRegisters());
    RELEASE_ASSERT(liveRegisters.buildScalarRegisterSet().numberOfSetRegisters() || !numberOfStackBytesUsedForRegisterPreservation,
        liveRegisters.buildScalarRegisterSet().numberOfSetRegisters(),
        numberOfStackBytesUsedForRegisterPreservation);
    return SpillState {
        liveRegisters.buildScalarRegisterSet(),
        numberOfStackBytesUsedForRegisterPreservation
    };
}

void InlineCacheCompiler::restoreLiveRegistersFromStackForCallWithThrownException(const SpillState& spillState)
{
    // Even if we're a getter, we don't want to ignore the result value like we normally do
    // because the getter threw, and therefore, didn't return a value that means anything.
    // Instead, we want to restore that register to what it was upon entering the getter
    // inline cache. The subtlety here is if the base and the result are the same register,
    // and the getter threw, we want OSR exit to see the original base value, not the result
    // of the getter call.
    RegisterSetBuilder dontRestore = spillState.spilledRegisters.toRegisterSet().includeWholeRegisterWidth();
    // As an optimization here, we only need to restore what is live for exception handling.
    // We can construct the dontRestore set to accomplish this goal by having it contain only
    // what is live for call but not live for exception handling. By ignoring things that are
    // only live at the call but not the exception handler, we will only restore things live
    // at the exception handler.
    dontRestore.exclude(liveRegistersToPreserveAtExceptionHandlingCallSite().toRegisterSet().includeWholeRegisterWidth());
    restoreLiveRegistersFromStackForCall(spillState, dontRestore.buildAndValidate());
}

void InlineCacheCompiler::restoreLiveRegistersFromStackForCall(const SpillState& spillState, const RegisterSet& dontRestore)
{
    unsigned extraStackPadding = 0;
    ScratchRegisterAllocator::restoreRegistersFromStackForCall(*m_jit, spillState.spilledRegisters.toRegisterSet(), dontRestore, spillState.numberOfStackBytesUsedForRegisterPreservation, extraStackPadding);
}

CallSiteIndex InlineCacheCompiler::callSiteIndexForExceptionHandlingOrOriginal()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();

    if (!m_calculatedCallSiteIndex) {
        m_calculatedCallSiteIndex = true;

        if (m_needsToRestoreRegistersIfException)
            m_callSiteIndex = m_jit->codeBlock()->newExceptionHandlingCallSiteIndex(m_stubInfo.callSiteIndex);
        else
            m_callSiteIndex = originalCallSiteIndex();
    }

    return m_callSiteIndex;
}

DisposableCallSiteIndex InlineCacheCompiler::callSiteIndexForExceptionHandling()
{
    RELEASE_ASSERT(m_calculatedRegistersForCallAndExceptionHandling);
    RELEASE_ASSERT(m_needsToRestoreRegistersIfException);
    RELEASE_ASSERT(m_calculatedCallSiteIndex);
    return DisposableCallSiteIndex::fromCallSiteIndex(m_callSiteIndex);
}

const HandlerInfo& InlineCacheCompiler::originalExceptionHandler()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();

    RELEASE_ASSERT(m_needsToRestoreRegistersIfException);
    HandlerInfo* exceptionHandler = m_jit->codeBlock()->handlerForIndex(m_stubInfo.callSiteIndex.bits());
    RELEASE_ASSERT(exceptionHandler);
    return *exceptionHandler;
}

CallSiteIndex InlineCacheCompiler::originalCallSiteIndex() const { return m_stubInfo.callSiteIndex; }

void InlineCacheCompiler::emitExplicitExceptionHandler()
{
    restoreScratch();
    m_jit->pushToSave(GPRInfo::regT0);
    m_jit->loadPtr(&m_vm.topEntryFrame, GPRInfo::regT0);
    m_jit->copyCalleeSavesToEntryFrameCalleeSavesBuffer(GPRInfo::regT0);
    m_jit->popToRestore(GPRInfo::regT0);

    if (needsToRestoreRegistersIfException()) {
        // To the JIT that produces the original exception handling
        // call site, they will expect the OSR exit to be arrived
        // at from genericUnwind. Therefore we must model what genericUnwind
        // does here. I.e, set callFrameForCatch and copy callee saves.

        m_jit->storePtr(GPRInfo::callFrameRegister, m_vm.addressOfCallFrameForCatch());
        // We don't need to insert a new exception handler in the table
        // because we're doing a manual exception check here. i.e, we'll
        // never arrive here from genericUnwind().
        HandlerInfo originalHandler = originalExceptionHandler();
        m_jit->jumpThunk(originalHandler.nativeCode);
    } else
        m_jit->jumpThunk(CodeLocationLabel(m_vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()));
}

ScratchRegisterAllocator InlineCacheCompiler::makeDefaultScratchAllocator(GPRReg extraToLock)
{
    ScratchRegisterAllocator allocator(m_stubInfo.usedRegisters.toRegisterSet());
    allocator.lock(m_stubInfo.baseRegs());
    allocator.lock(m_stubInfo.valueRegs());
    allocator.lock(m_stubInfo.m_extraGPR);
    allocator.lock(m_stubInfo.m_extra2GPR);
#if USE(JSVALUE32_64)
    allocator.lock(m_stubInfo.m_extraTagGPR);
    allocator.lock(m_stubInfo.m_extra2TagGPR);
#endif
    allocator.lock(m_stubInfo.m_stubInfoGPR);
    allocator.lock(m_stubInfo.m_arrayProfileGPR);
    allocator.lock(extraToLock);

    if (useHandlerIC())
        allocator.lock(GPRInfo::handlerGPR);

    return allocator;
}

#if CPU(X86_64)
static constexpr size_t prologueSizeInBytesDataIC = 1;
#elif CPU(ARM64E)
static constexpr size_t prologueSizeInBytesDataIC = 8;
#elif CPU(ARM64)
static constexpr size_t prologueSizeInBytesDataIC = 4;
#elif CPU(ARM_THUMB2)
static constexpr size_t prologueSizeInBytesDataIC = 6;
#elif CPU(RISCV64)
static constexpr size_t prologueSizeInBytesDataIC = 12;
#else
#error "unsupported architecture"
#endif

void InlineCacheCompiler::emitDataICPrologue(CCallHelpers& jit)
{
    // Important difference from the normal emitPrologue is that DataIC handler does not change callFrameRegister.
    // callFrameRegister is an original one of the caller JS function. This removes necessity of complicated handling
    // of exception unwinding, and it allows operations to access to CallFrame* via callFrameRegister.
#if ASSERT_ENABLED
    size_t startOffset = jit.debugOffset();
#endif

#if CPU(X86_64)
    static_assert(!maxFrameExtentForSlowPathCall);
    jit.push(CCallHelpers::framePointerRegister);
#elif CPU(ARM64)
    static_assert(!maxFrameExtentForSlowPathCall);
    jit.tagReturnAddress();
    jit.pushPair(CCallHelpers::framePointerRegister, CCallHelpers::linkRegister);
#elif CPU(ARM_THUMB2)
    static_assert(maxFrameExtentForSlowPathCall);
    jit.pushPair(CCallHelpers::framePointerRegister, CCallHelpers::linkRegister);
    jit.subPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
#elif CPU(RISCV64)
    static_assert(!maxFrameExtentForSlowPathCall);
    jit.pushPair(CCallHelpers::framePointerRegister, CCallHelpers::linkRegister);
#else
#error "unsupported architecture"
#endif

#if ASSERT_ENABLED
    ASSERT(prologueSizeInBytesDataIC == (jit.debugOffset() - startOffset));
#endif
}

void InlineCacheCompiler::emitDataICEpilogue(CCallHelpers& jit)
{
    if constexpr (!!maxFrameExtentForSlowPathCall)
        jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
    jit.emitFunctionEpilogueWithEmptyFrame();
}

CCallHelpers::Jump InlineCacheCompiler::emitDataICCheckStructure(CCallHelpers& jit, GPRReg baseGPR, GPRReg scratchGPR)
{
    JIT_COMMENT(jit, "check structure");
    jit.load32(CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()), scratchGPR);
    return jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfStructureID()));
}

CCallHelpers::JumpList InlineCacheCompiler::emitDataICCheckUid(CCallHelpers& jit, bool isSymbol, JSValueRegs propertyJSR, GPRReg scratchGPR)
{
    JIT_COMMENT(jit, "check uid");
    CCallHelpers::JumpList fallThrough;

    fallThrough.append(jit.branchIfNotCell(propertyJSR));
    if (isSymbol) {
        fallThrough.append(jit.branchIfNotSymbol(propertyJSR.payloadGPR()));
        jit.loadPtr(CCallHelpers::Address(propertyJSR.payloadGPR(), Symbol::offsetOfSymbolImpl()), scratchGPR);
    } else {
        fallThrough.append(jit.branchIfNotString(propertyJSR.payloadGPR()));
        jit.loadPtr(CCallHelpers::Address(propertyJSR.payloadGPR(), JSString::offsetOfValue()), scratchGPR);
        fallThrough.append(jit.branchIfRopeStringImpl(scratchGPR));
    }
    fallThrough.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfUid())));

    return fallThrough;
}

void InlineCacheCompiler::emitDataICJumpNextHandler(CCallHelpers& jit)
{
    jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfNext()), GPRInfo::handlerGPR);
    jit.farJump(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfJumpTarget()), JITStubRoutinePtrTag);
}

static MacroAssemblerCodeRef<JITThunkPtrTag> getByIdSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByIdOptimize);

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.setupArguments<SlowOperation>(baseJSR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 1>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "get_by_id_slow"_s, "DataIC get_by_id_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> getByIdWithThisSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByIdWithThisOptimize);

    using BaselineJITRegisters::GetByIdWithThis::baseJSR;
    using BaselineJITRegisters::GetByIdWithThis::thisJSR;
    using BaselineJITRegisters::GetByIdWithThis::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.setupArguments<SlowOperation>(baseJSR, thisJSR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 2>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "get_by_id_with_this_slow"_s, "DataIC get_by_id_with_this_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> getByValSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByValOptimize);

    using BaselineJITRegisters::GetByVal::baseJSR;
    using BaselineJITRegisters::GetByVal::propertyJSR;
    using BaselineJITRegisters::GetByVal::stubInfoGPR;
    using BaselineJITRegisters::GetByVal::profileGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.setupArguments<SlowOperation>(baseJSR, propertyJSR, stubInfoGPR, profileGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 2>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "get_by_val_slow"_s, "DataIC get_by_val_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> getPrivateNameSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetPrivateNameOptimize);

    using BaselineJITRegisters::PrivateBrand::baseJSR;
    using BaselineJITRegisters::PrivateBrand::propertyJSR;
    using BaselineJITRegisters::PrivateBrand::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.setupArguments<SlowOperation>(baseJSR, propertyJSR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 2>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "get_private_name_slow"_s, "DataIC get_private_name_slow");
}

#if USE(JSVALUE64)
static MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithThisSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByValWithThisOptimize);

    using BaselineJITRegisters::GetByValWithThis::baseJSR;
    using BaselineJITRegisters::GetByValWithThis::propertyJSR;
    using BaselineJITRegisters::GetByValWithThis::thisJSR;
    using BaselineJITRegisters::GetByValWithThis::stubInfoGPR;
    using BaselineJITRegisters::GetByValWithThis::profileGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.setupArguments<SlowOperation>(baseJSR, propertyJSR, thisJSR, stubInfoGPR, profileGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 3>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "get_by_val_with_this_slow"_s, "DataIC get_by_val_with_this_slow");
}
#endif

static MacroAssemblerCodeRef<JITThunkPtrTag> putByIdSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationPutByIdStrictOptimize);

    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.setupArguments<SlowOperation>(valueJSR, baseJSR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 2>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "put_by_id_slow"_s, "DataIC put_by_id_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> putByValSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperatoin = decltype(operationPutByValStrictOptimize);

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::profileGPR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.setupArguments<SlowOperatoin>(baseJSR, propertyJSR, valueJSR, stubInfoGPR, profileGPR);
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
#if CPU(ARM_THUMB2)
    // ARMv7 clobbers metadataTable register. Thus we need to restore them back here.
    JIT::emitMaterializeMetadataAndConstantPoolRegisters(jit);
#endif

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "put_by_val_slow"_s, "DataIC put_by_val_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> instanceOfSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationInstanceOfOptimize);

    using BaselineJITRegisters::Instanceof::valueJSR;
    using BaselineJITRegisters::Instanceof::protoJSR;
    using BaselineJITRegisters::Instanceof::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.setupArguments<SlowOperation>(valueJSR, protoJSR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 2>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "instanceof_slow"_s, "DataIC instanceof_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> delByIdSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationDeleteByIdStrictOptimize);

    using BaselineJITRegisters::DelById::baseJSR;
    using BaselineJITRegisters::DelById::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.setupArguments<SlowOperation>(baseJSR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 1>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "del_by_id_slow"_s, "DataIC del_by_id_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> delByValSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationDeleteByValStrictOptimize);

    using BaselineJITRegisters::DelByVal::baseJSR;
    using BaselineJITRegisters::DelByVal::propertyJSR;
    using BaselineJITRegisters::DelByVal::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.setupArguments<SlowOperation>(baseJSR, propertyJSR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 2>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "del_by_val_slow"_s, "DataIC del_by_val_slow");
}

MacroAssemblerCodeRef<JITThunkPtrTag> InlineCacheCompiler::generateSlowPathCode(VM& vm, AccessType type)
{
    switch (type) {
    case AccessType::GetById:
    case AccessType::TryGetById:
    case AccessType::GetByIdDirect:
    case AccessType::InById:
    case AccessType::GetPrivateNameById: {
        using ArgumentTypes = FunctionTraits<decltype(operationGetByIdOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationTryGetByIdOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationGetByIdDirectOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationInByIdOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationGetPrivateNameByIdOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(getByIdSlowPathCodeGenerator);
    }

    case AccessType::GetByIdWithThis:
        return vm.getCTIStub(getByIdWithThisSlowPathCodeGenerator);

    case AccessType::GetByVal:
    case AccessType::InByVal: {
        using ArgumentTypes = FunctionTraits<decltype(operationGetByValOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationInByValOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(getByValSlowPathCodeGenerator);
    }

    case AccessType::GetByValWithThis: {
#if USE(JSVALUE64)
        return vm.getCTIStub(getByValWithThisSlowPathCodeGenerator);
#else
        RELEASE_ASSERT_NOT_REACHED();
        return { };
#endif
    }

    case AccessType::GetPrivateName:
    case AccessType::HasPrivateBrand:
    case AccessType::HasPrivateName:
    case AccessType::CheckPrivateBrand:
    case AccessType::SetPrivateBrand: {
        using ArgumentTypes = FunctionTraits<decltype(operationGetPrivateNameOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationHasPrivateBrandOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationHasPrivateNameOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationCheckPrivateBrandOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationSetPrivateBrandOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(getPrivateNameSlowPathCodeGenerator);
    }

    case AccessType::PutByIdStrict:
    case AccessType::PutByIdSloppy:
    case AccessType::PutByIdDirectStrict:
    case AccessType::PutByIdDirectSloppy:
    case AccessType::DefinePrivateNameById:
    case AccessType::SetPrivateNameById: {
        using ArgumentTypes = FunctionTraits<decltype(operationPutByIdStrictOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByIdSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByIdDirectStrictOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByIdDirectSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByIdDefinePrivateFieldStrictOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByIdSetPrivateFieldStrictOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(putByIdSlowPathCodeGenerator);
    }

    case AccessType::PutByValStrict:
    case AccessType::PutByValSloppy:
    case AccessType::PutByValDirectStrict:
    case AccessType::PutByValDirectSloppy:
    case AccessType::DefinePrivateNameByVal:
    case AccessType::SetPrivateNameByVal: {
        using ArgumentTypes = FunctionTraits<decltype(operationPutByValStrictOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByValSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationDirectPutByValSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationDirectPutByValStrictOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByValDefinePrivateFieldOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByValSetPrivateFieldOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(putByValSlowPathCodeGenerator);
    }

    case AccessType::InstanceOf:
        return vm.getCTIStub(instanceOfSlowPathCodeGenerator);

    case AccessType::DeleteByIdStrict:
    case AccessType::DeleteByIdSloppy: {
        using ArgumentTypes = FunctionTraits<decltype(operationDeleteByIdStrictOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationDeleteByIdSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(delByIdSlowPathCodeGenerator);
    }

    case AccessType::DeleteByValStrict:
    case AccessType::DeleteByValSloppy: {
        using ArgumentTypes = FunctionTraits<decltype(operationDeleteByValStrictOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationDeleteByValSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(delByValSlowPathCodeGenerator);
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

InlineCacheHandler::InlineCacheHandler(Ref<InlineCacheHandler>&& previous, Ref<PolymorphicAccessJITStubRoutine>&& stubRoutine, std::unique_ptr<StructureStubInfoClearingWatchpoint>&& watchpoint, unsigned callLinkInfoCount, CacheType cacheType)
    : Base(callLinkInfoCount)
    , m_callTarget(stubRoutine->code().code().template retagged<JITStubRoutinePtrTag>())
    , m_jumpTarget(CodePtr<NoPtrTag> { m_callTarget.retagged<NoPtrTag>().dataLocation<uint8_t*>() + prologueSizeInBytesDataIC }.template retagged<JITStubRoutinePtrTag>())
    , m_cacheType(cacheType)
    , m_next(WTFMove(previous))
    , m_stubRoutine(WTFMove(stubRoutine))
    , m_watchpoint(WTFMove(watchpoint))
{
    disableThreadingChecks();
}

Ref<InlineCacheHandler> InlineCacheHandler::create(Ref<InlineCacheHandler>&& previous, CodeBlock* codeBlock, StructureStubInfo& stubInfo, Ref<PolymorphicAccessJITStubRoutine>&& stubRoutine, std::unique_ptr<StructureStubInfoClearingWatchpoint>&& watchpoint, unsigned callLinkInfoCount)
{
    auto result = adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(callLinkInfoCount))) InlineCacheHandler(WTFMove(previous), WTFMove(stubRoutine), WTFMove(watchpoint), callLinkInfoCount, CacheType::Unset));
    VM& vm = codeBlock->vm();
    for (auto& callLinkInfo : result->span())
        callLinkInfo.initialize(vm, codeBlock, CallLinkInfo::CallType::Call, stubInfo.codeOrigin);
    result->m_uid = stubInfo.identifier().uid();
    return result;
}

Ref<InlineCacheHandler> InlineCacheHandler::createPreCompiled(Ref<InlineCacheHandler>&& previous, CodeBlock* codeBlock, StructureStubInfo& stubInfo, Ref<PolymorphicAccessJITStubRoutine>&& stubRoutine, std::unique_ptr<StructureStubInfoClearingWatchpoint>&& watchpoint, AccessCase& accessCase, CacheType cacheType)
{
    unsigned callLinkInfoCount = JSC::doesJSCalls(accessCase.m_type) ? 1 : 0;
    auto result = adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(callLinkInfoCount))) InlineCacheHandler(WTFMove(previous), WTFMove(stubRoutine), WTFMove(watchpoint), callLinkInfoCount, cacheType));
    VM& vm = codeBlock->vm();
    for (auto& callLinkInfo : result->span())
        callLinkInfo.initialize(vm, codeBlock, CallLinkInfo::CallType::Call, stubInfo.codeOrigin);

    result->m_structureID = accessCase.structureID();
    result->m_offset = accessCase.offset();
    result->m_uid = stubInfo.identifier().uid();
    if (!result->m_uid)
        result->m_uid = accessCase.uid();
    switch (accessCase.m_type) {
    case AccessCase::Load:
    case AccessCase::GetGetter:
    case AccessCase::Getter:
    case AccessCase::Setter: {
        result->u.s1.m_holder = nullptr;
        if (auto* holder = accessCase.tryGetAlternateBase())
            result->u.s1.m_holder = holder;
        break;
    }
    case AccessCase::ProxyObjectLoad: {
        result->u.s1.m_holder = accessCase.identifier().cell();
        break;
    }
    case AccessCase::Delete:
    case AccessCase::SetPrivateBrand: {
        result->u.s2.m_newStructureID = accessCase.newStructureID();
        break;
    }
    case AccessCase::Transition: {
        result->u.s2.m_newStructureID = accessCase.newStructureID();
        result->u.s2.m_newSize = accessCase.newStructure()->outOfLineCapacity() * sizeof(JSValue);
        result->u.s2.m_oldSize = accessCase.structure()->outOfLineCapacity() * sizeof(JSValue);
        break;
    }
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomValueSetter: {
        result->u.s1.m_holder = nullptr;
        Structure* currStructure = accessCase.structure();
        if (auto* holder = accessCase.tryGetAlternateBase()) {
            currStructure = holder->structure();
            result->u.s1.m_holder = holder;
        }
        result->u.s1.m_globalObject = currStructure->globalObject();
        result->u.s1.m_customAccessor = accessCase.as<GetterSetterAccessCase>().customAccessor().taggedPtr();
        break;
    }
    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss: {
        result->u.s1.m_holder = accessCase.as<InstanceOfAccessCase>().prototype();
        break;
    }
    case AccessCase::ModuleNamespaceLoad: {
        auto& derived = accessCase.as<ModuleNamespaceAccessCase>();
        result->u.s3.m_moduleNamespaceObject = derived.moduleNamespaceObject();
        result->u.s3.m_moduleVariableSlot = &derived.moduleEnvironment()->variableAt(derived.scopeOffset());
        break;
    }
    case AccessCase::CheckPrivateBrand: {
        break;
    }
    default:
        break;
    }

    return result;
}

Ref<InlineCacheHandler> InlineCacheHandler::createNonHandlerSlowPath(CodePtr<JITStubRoutinePtrTag> slowPath)
{
    auto result = adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(0))) InlineCacheHandler);
    result->m_callTarget = slowPath;
    result->m_jumpTarget = slowPath;
    return result;
}

Ref<InlineCacheHandler> InlineCacheHandler::createSlowPath(VM& vm, AccessType accessType)
{
    auto result = adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(0))) InlineCacheHandler);
    auto codeRef = InlineCacheCompiler::generateSlowPathCode(vm, accessType);
    result->m_callTarget = codeRef.code().template retagged<JITStubRoutinePtrTag>();
    result->m_jumpTarget = CodePtr<NoPtrTag> { codeRef.retaggedCode<NoPtrTag>().dataLocation<uint8_t*>() + prologueSizeInBytesDataIC }.template retagged<JITStubRoutinePtrTag>();
    return result;
}

Ref<InlineCacheHandler> InlineCacheCompiler::generateSlowPathHandler(VM& vm, AccessType accessType)
{
    ASSERT(!isCompilationThread());
    if (auto handler = vm.m_sharedJITStubs->getSlowPathHandler(accessType))
        return handler.releaseNonNull();
    auto handler = InlineCacheHandler::createSlowPath(vm, accessType);
    vm.m_sharedJITStubs->setSlowPathHandler(accessType, handler);
    return handler;
}

template<typename Visitor>
void InlineCacheHandler::propagateTransitions(Visitor& visitor) const
{
    if (m_accessCase)
        m_accessCase->propagateTransitions(visitor);
}

template void InlineCacheHandler::propagateTransitions(AbstractSlotVisitor&) const;
template void InlineCacheHandler::propagateTransitions(SlotVisitor&) const;

template<typename Visitor>
void InlineCacheHandler::visitAggregateImpl(Visitor& visitor)
{
    if (m_accessCase)
        m_accessCase->visitAggregate(visitor);
}
DEFINE_VISIT_AGGREGATE(InlineCacheHandler);

void InlineCacheCompiler::generateWithGuard(unsigned index, AccessCase& accessCase, CCallHelpers::JumpList& fallThrough)
{
    SuperSamplerScope superSamplerScope(false);

    accessCase.checkConsistency(m_stubInfo);

    CCallHelpers& jit = *m_jit;
    JIT_COMMENT(jit, "Begin generateWithGuard");
    VM& vm = m_vm;
    JSValueRegs valueRegs = m_stubInfo.valueRegs();
    GPRReg baseGPR = m_stubInfo.m_baseGPR;
    GPRReg scratchGPR = m_scratchGPR;

    if (accessCase.requiresIdentifierNameMatch() && !hasConstantIdentifier(m_stubInfo.accessType)) {
        RELEASE_ASSERT(accessCase.m_identifier);
        GPRReg propertyGPR = m_stubInfo.propertyGPR();
        // non-rope string check done inside polymorphic access.

        if (accessCase.uid()->isSymbol())
            jit.loadPtr(MacroAssembler::Address(propertyGPR, Symbol::offsetOfSymbolImpl()), scratchGPR);
        else
            jit.loadPtr(MacroAssembler::Address(propertyGPR, JSString::offsetOfValue()), scratchGPR);
        fallThrough.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImmPtr(accessCase.uid())));
    }

    auto emitDefaultGuard = [&] () {
        if (accessCase.polyProtoAccessChain()) {
            ASSERT(!accessCase.viaGlobalProxy());
            GPRReg baseForAccessGPR = m_scratchGPR;
            jit.move(baseGPR, baseForAccessGPR);
            accessCase.polyProtoAccessChain()->forEach(vm, accessCase.structure(), [&](Structure* structure, bool atEnd) {
                fallThrough.append(
                    jit.branchStructure(
                        CCallHelpers::NotEqual,
                        CCallHelpers::Address(baseForAccessGPR, JSCell::structureIDOffset()),
                        structure));
                if (atEnd) {
                    if ((accessCase.m_type == AccessCase::Miss || accessCase.m_type == AccessCase::InMiss || accessCase.m_type == AccessCase::Transition) && structure->hasPolyProto()) {
                        // For a Miss/InMiss/Transition, we must ensure we're at the end when the last item is poly proto.
                        // Transitions must do this because they need to verify there isn't a setter in the chain.
                        // Miss/InMiss need to do this to ensure there isn't a new item at the end of the chain that
                        // has the property.
#if USE(JSVALUE64)
                        jit.load64(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset)), baseForAccessGPR);
                        fallThrough.append(jit.branch64(CCallHelpers::NotEqual, baseForAccessGPR, CCallHelpers::TrustedImm64(JSValue::ValueNull)));
#else
                        jit.load32(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset) + PayloadOffset), baseForAccessGPR);
                        fallThrough.append(jit.branchTestPtr(CCallHelpers::NonZero, baseForAccessGPR));
#endif
                    }
                } else {
                    if (structure->hasMonoProto()) {
                        if (!useHandlerIC() || structure->isObject()) {
                            JSValue prototype = structure->prototypeForLookup(m_globalObject);
                            RELEASE_ASSERT(prototype.isObject());
                            jit.move(CCallHelpers::TrustedImmPtr(asObject(prototype)), baseForAccessGPR);
                        } else {
                            ASSERT(useHandlerIC());
                            jit.loadPtr(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfGlobalObject()), baseForAccessGPR);
                            switch (structure->typeInfo().type()) {
                            case StringType:
                                jit.loadPtr(CCallHelpers::Address(baseForAccessGPR, JSGlobalObject::offsetOfStringPrototype()), baseForAccessGPR);
                                break;
                            case HeapBigIntType:
                                jit.loadPtr(CCallHelpers::Address(baseForAccessGPR, JSGlobalObject::offsetOfBigIntPrototype()), baseForAccessGPR);
                                break;
                            case SymbolType:
                                jit.loadPtr(CCallHelpers::Address(baseForAccessGPR, JSGlobalObject::offsetOfSymbolPrototype()), baseForAccessGPR);
                                break;
                            default:
                                RELEASE_ASSERT_NOT_REACHED();
                                break;
                            }
                        }
                    } else {
                        RELEASE_ASSERT(structure->isObject()); // Primitives must have a stored prototype. We use prototypeForLookup for them.
#if USE(JSVALUE64)
                        jit.load64(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset)), baseForAccessGPR);
                        fallThrough.append(jit.branch64(CCallHelpers::Equal, baseForAccessGPR, CCallHelpers::TrustedImm64(JSValue::ValueNull)));
#else
                        jit.load32(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset) + PayloadOffset), baseForAccessGPR);
                        fallThrough.append(jit.branchTestPtr(CCallHelpers::Zero, baseForAccessGPR));
#endif
                    }
                }
            });
            return;
        }

        if (accessCase.viaGlobalProxy()) {
            ASSERT(canBeViaGlobalProxy(accessCase.m_type));
            fallThrough.append(
                jit.branchIfNotType(baseGPR, GlobalProxyType));

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), scratchGPR);

            fallThrough.append(
                jit.branchStructure(
                    CCallHelpers::NotEqual,
                    CCallHelpers::Address(scratchGPR, JSCell::structureIDOffset()),
                    accessCase.structure()));
            return;
        }

        fallThrough.append(
            jit.branchStructure(
                CCallHelpers::NotEqual,
                CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()),
                accessCase.structure()));
    };

    switch (accessCase.m_type) {
    case AccessCase::ArrayLength: {
        ASSERT(!accessCase.viaGlobalProxy());
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), scratchGPR);
        fallThrough.append(
            jit.branchTest32(
                CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(IsArray)));
        fallThrough.append(
            jit.branchTest32(
                CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(IndexingShapeMask)));
        break;
    }

    case AccessCase::StringLength: {
        ASSERT(!accessCase.viaGlobalProxy());
        fallThrough.append(
            jit.branchIfNotString(baseGPR));
        break;
    }

    case AccessCase::DirectArgumentsLength: {
        ASSERT(!accessCase.viaGlobalProxy());
        fallThrough.append(
            jit.branchIfNotType(baseGPR, DirectArgumentsType));

        fallThrough.append(
            jit.branchTestPtr(
                CCallHelpers::NonZero,
                CCallHelpers::Address(baseGPR, DirectArguments::offsetOfMappedArguments())));
        jit.load32(
            CCallHelpers::Address(baseGPR, DirectArguments::offsetOfLength()),
            valueRegs.payloadGPR());
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        succeed();
        return;
    }

    case AccessCase::ScopedArgumentsLength: {
        ASSERT(!accessCase.viaGlobalProxy());
        fallThrough.append(
            jit.branchIfNotType(baseGPR, ScopedArgumentsType));

        fallThrough.append(
            jit.branchTest8(
                CCallHelpers::NonZero,
                CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfOverrodeThings())));
        jit.load32(
            CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfTotalLength()),
            valueRegs.payloadGPR());
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        succeed();
        return;
    }

    case AccessCase::ModuleNamespaceLoad: {
        ASSERT(!accessCase.viaGlobalProxy());
        emitModuleNamespaceLoad(accessCase.as<ModuleNamespaceAccessCase>(), fallThrough);
        return;
    }

    case AccessCase::ProxyObjectIn:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::IndexedProxyObjectIn:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::IndexedProxyObjectStore: {
        ASSERT(!accessCase.viaGlobalProxy());
        emitProxyObjectAccess(index, accessCase, fallThrough);
        return;
    }

    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedScopedArgumentsInHit: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = m_stubInfo.propertyGPR();

        jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
        fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(ScopedArgumentsType)));

        auto allocator = makeDefaultScratchAllocator(scratchGPR);

        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList failAndIgnore;

        failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfTotalLength())));

        jit.loadPtr(CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfTable()), scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, ScopedArgumentsTable::offsetOfLength()), scratch2GPR);
        auto overflowCase = jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratch2GPR);

        jit.loadPtr(CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfScope()), scratch2GPR);
        jit.loadPtr(CCallHelpers::Address(scratchGPR, ScopedArgumentsTable::offsetOfArguments()), scratchGPR);
        jit.zeroExtend32ToWord(propertyGPR, scratch3GPR);
        jit.load32(CCallHelpers::BaseIndex(scratchGPR, scratch3GPR, CCallHelpers::TimesFour), scratchGPR);
        failAndIgnore.append(jit.branch32(CCallHelpers::Equal, scratchGPR, CCallHelpers::TrustedImm32(ScopeOffset::invalidOffset)));
        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else
            jit.loadValue(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesEight, JSLexicalEnvironment::offsetOfVariables()), valueRegs);
        auto done = jit.jump();

        overflowCase.link(&jit);
        jit.sub32(propertyGPR, scratch2GPR);
        jit.neg32(scratch2GPR);
        jit.loadPtr(CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfStorage()), scratch3GPR);
#if USE(JSVALUE64)
        jit.loadValue(CCallHelpers::BaseIndex(scratch3GPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratchGPR));
        failAndIgnore.append(jit.branchIfEmpty(scratchGPR));
        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else
            jit.move(scratchGPR, valueRegs.payloadGPR());
#else
        jit.loadValue(CCallHelpers::BaseIndex(scratch3GPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratch2GPR, scratchGPR));
        failAndIgnore.append(jit.branchIfEmpty(scratch2GPR));
        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else {
            jit.move(scratchGPR, valueRegs.payloadGPR());
            jit.move(scratch2GPR, valueRegs.tagGPR());
        }
#endif

        done.link(&jit);

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndIgnore.append(jit.jump());
        } else
            m_failAndIgnore.append(failAndIgnore);

        return;
    }

    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsInHit: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = m_stubInfo.propertyGPR();
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
        fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(DirectArgumentsType)));

        jit.load32(CCallHelpers::Address(baseGPR, DirectArguments::offsetOfLength()), scratchGPR);
        m_failAndRepatch.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratchGPR));
        m_failAndRepatch.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::Address(baseGPR, DirectArguments::offsetOfMappedArguments())));
        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else {
            jit.zeroExtend32ToWord(propertyGPR, scratchGPR);
            jit.loadValue(CCallHelpers::BaseIndex(baseGPR, scratchGPR, CCallHelpers::TimesEight, DirectArguments::storageOffset()), valueRegs);
        }
        succeed();
        return;
    }

    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayFloat16Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat16Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedTypedArrayInt8In:
    case AccessCase::IndexedTypedArrayUint8In:
    case AccessCase::IndexedTypedArrayUint8ClampedIn:
    case AccessCase::IndexedTypedArrayInt16In:
    case AccessCase::IndexedTypedArrayUint16In:
    case AccessCase::IndexedTypedArrayInt32In:
    case AccessCase::IndexedTypedArrayUint32In:
    case AccessCase::IndexedTypedArrayFloat16In:
    case AccessCase::IndexedTypedArrayFloat32In:
    case AccessCase::IndexedTypedArrayFloat64In:
    case AccessCase::IndexedResizableTypedArrayInt8In:
    case AccessCase::IndexedResizableTypedArrayUint8In:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedIn:
    case AccessCase::IndexedResizableTypedArrayInt16In:
    case AccessCase::IndexedResizableTypedArrayUint16In:
    case AccessCase::IndexedResizableTypedArrayInt32In:
    case AccessCase::IndexedResizableTypedArrayUint32In:
    case AccessCase::IndexedResizableTypedArrayFloat16In:
    case AccessCase::IndexedResizableTypedArrayFloat32In:
    case AccessCase::IndexedResizableTypedArrayFloat64In: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.

        TypedArrayType type = toTypedArrayType(accessCase.m_type);
        bool isResizableOrGrowableShared = forResizableTypedArray(accessCase.m_type);

        GPRReg propertyGPR = m_stubInfo.propertyGPR();

        fallThrough.append(jit.branch8(CCallHelpers::NotEqual, CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), CCallHelpers::TrustedImm32(typeForTypedArrayType(type))));
        if (!isResizableOrGrowableShared)
            fallThrough.append(jit.branchTest8(CCallHelpers::NonZero, CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfMode()), CCallHelpers::TrustedImm32(isResizableOrGrowableSharedMode)));

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList isOutOfBounds;
        if (isResizableOrGrowableShared) {
            jit.loadTypedArrayLength(baseGPR, scratch2GPR, scratchGPR, scratch2GPR, type);
            jit.signExtend32ToPtr(propertyGPR, scratchGPR);
#if USE(LARGE_TYPED_ARRAYS)
            // The length is a size_t, so either 32 or 64 bits depending on the platform.
            isOutOfBounds.append(jit.branch64(CCallHelpers::AboveOrEqual, scratchGPR, scratch2GPR));
#else
            isOutOfBounds.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratch2GPR));
#endif
        } else {
            CCallHelpers::Address addressOfLength = CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength());
            jit.signExtend32ToPtr(propertyGPR, scratchGPR);
#if USE(LARGE_TYPED_ARRAYS)
            // The length is a size_t, so either 32 or 64 bits depending on the platform.
            isOutOfBounds.append(jit.branch64(CCallHelpers::AboveOrEqual, scratchGPR, addressOfLength));
#else
            isOutOfBounds.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, addressOfLength));
#endif
        }

        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else {
#if USE(LARGE_TYPED_ARRAYS)
            jit.load64(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength()), scratchGPR);
#else
            jit.load32(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength()), scratchGPR);
#endif
            jit.loadPtr(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfVector()), scratch2GPR);
            jit.cageConditionally(Gigacage::Primitive, scratch2GPR, scratchGPR, scratchGPR);
            jit.signExtend32ToPtr(propertyGPR, scratchGPR);
            if (isInt(type)) {
                switch (elementSize(type)) {
                case 1:
                    if (JSC::isSigned(type))
                        jit.load8SignedExtendTo32(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne), valueRegs.payloadGPR());
                    else
                        jit.load8(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne), valueRegs.payloadGPR());
                    break;
                case 2:
                    if (JSC::isSigned(type))
                        jit.load16SignedExtendTo32(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo), valueRegs.payloadGPR());
                    else
                        jit.load16(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo), valueRegs.payloadGPR());
                    break;
                case 4:
                    jit.load32(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour), valueRegs.payloadGPR());
                    break;
                default:
                    CRASH();
                }

                CCallHelpers::Jump done;
                if (type == TypeUint32) {
                    RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
                    auto canBeInt = jit.branch32(CCallHelpers::GreaterThanOrEqual, valueRegs.payloadGPR(), CCallHelpers::TrustedImm32(0));

                    jit.convertInt32ToDouble(valueRegs.payloadGPR(), m_scratchFPR);
                    jit.addDouble(CCallHelpers::AbsoluteAddress(&CCallHelpers::twoToThe32), m_scratchFPR);
                    jit.boxDouble(m_scratchFPR, valueRegs);
                    done = jit.jump();
                    canBeInt.link(&jit);
                }

                jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
                if (done.isSet())
                    done.link(&jit);
            } else {
                ASSERT(isFloat(type));
                RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
                switch (elementSize(type)) {
                case 2:
                    jit.loadFloat16(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo), m_scratchFPR);
                    jit.convertFloat16ToDouble(m_scratchFPR, m_scratchFPR);
                    break;
                case 4:
                    jit.loadFloat(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour), m_scratchFPR);
                    jit.convertFloatToDouble(m_scratchFPR, m_scratchFPR);
                    break;
                case 8: {
                    jit.loadDouble(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesEight), m_scratchFPR);
                    break;
                }
                default:
                    CRASH();
                }

                jit.purifyNaN(m_scratchFPR);
                jit.boxDouble(m_scratchFPR, valueRegs);
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        isOutOfBounds.link(&jit);
        if (m_stubInfo.m_arrayProfileGPR != InvalidGPRReg)
            jit.or32(CCallHelpers::TrustedImm32(static_cast<uint32_t>(ArrayProfileFlag::OutOfBounds)), CCallHelpers::Address(m_stubInfo.m_arrayProfileGPR, ArrayProfile::offsetOfArrayProfileFlags()));
        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(false), valueRegs);
        else
            jit.moveTrustedValue(jsUndefined(), valueRegs);
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();
        return;
    }

    case AccessCase::IndexedStringLoad:
    case AccessCase::IndexedStringInHit: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = m_stubInfo.propertyGPR();

        fallThrough.append(jit.branchIfNotString(baseGPR));

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        CCallHelpers::JumpList failAndIgnore;

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        jit.loadPtr(CCallHelpers::Address(baseGPR, JSString::offsetOfValue()), scratch2GPR);
        failAndIgnore.append(jit.branchIfRopeStringImpl(scratch2GPR));
        jit.load32(CCallHelpers::Address(scratch2GPR, StringImpl::lengthMemoryOffset()), scratchGPR);

        failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratchGPR));

        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else {
            jit.load32(CCallHelpers::Address(scratch2GPR, StringImpl::flagsOffset()), scratchGPR);
            jit.loadPtr(CCallHelpers::Address(scratch2GPR, StringImpl::dataOffset()), scratch2GPR);
            auto is16Bit = jit.branchTest32(CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(StringImpl::flagIs8Bit()));
            jit.zeroExtend32ToWord(propertyGPR, scratchGPR);
            jit.load8(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne, 0), scratch2GPR);
            auto is8BitLoadDone = jit.jump();
            is16Bit.link(&jit);
            jit.zeroExtend32ToWord(propertyGPR, scratchGPR);
            jit.load16(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo, 0), scratch2GPR);
            is8BitLoadDone.link(&jit);

            failAndIgnore.append(jit.branch32(CCallHelpers::Above, scratch2GPR, CCallHelpers::TrustedImm32(maxSingleCharacterString)));
            jit.move(CCallHelpers::TrustedImmPtr(vm.smallStrings.singleCharacterStrings()), scratchGPR);
            jit.loadPtr(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::ScalePtr, 0), valueRegs.payloadGPR());
            jit.boxCell(valueRegs.payloadGPR(), valueRegs);
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndIgnore.append(jit.jump());
        } else
            m_failAndIgnore.append(failAndIgnore);

        return;
    }

    case AccessCase::IndexedNoIndexingMiss:
    case AccessCase::IndexedNoIndexingInMiss: {
        emitDefaultGuard();
        GPRReg propertyGPR = m_stubInfo.propertyGPR();
        m_failAndIgnore.append(jit.branch32(CCallHelpers::LessThan, propertyGPR, CCallHelpers::TrustedImm32(0)));
        break;
    }

    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedDoubleInHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = m_stubInfo.propertyGPR();

        // int32 check done in polymorphic access.
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), scratchGPR);
        jit.and32(CCallHelpers::TrustedImm32(IndexingShapeMask), scratchGPR);

        auto allocator = makeDefaultScratchAllocator(scratchGPR);

        GPRReg scratch2GPR = allocator.allocateScratchGPR();
#if USE(JSVALUE32_64)
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
#endif
        ScratchRegisterAllocator::PreservedState preservedState;

        CCallHelpers::JumpList failAndIgnore;
        auto preserveReusedRegisters = [&] {
            preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
        };

        if (accessCase.m_type == AccessCase::IndexedArrayStorageLoad || accessCase.m_type == AccessCase::IndexedArrayStorageInHit) {
            jit.add32(CCallHelpers::TrustedImm32(-ArrayStorageShape), scratchGPR, scratchGPR);
            fallThrough.append(jit.branch32(CCallHelpers::Above, scratchGPR, CCallHelpers::TrustedImm32(SlowPutArrayStorageShape - ArrayStorageShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, ArrayStorage::vectorLengthOffset())));

            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
#if USE(JSVALUE64)
            jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()), JSValueRegs(scratchGPR));
            failAndIgnore.append(jit.branchIfEmpty(scratchGPR));
            if (forInBy(accessCase.m_type))
                jit.moveTrustedValue(jsBoolean(true), valueRegs);
            else
                jit.move(scratchGPR, valueRegs.payloadGPR());
#else
            jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()), JSValueRegs(scratch3GPR, scratchGPR));
            failAndIgnore.append(jit.branchIfEmpty(scratch3GPR));
            if (forInBy(accessCase.m_type))
                jit.moveTrustedValue(jsBoolean(true), valueRegs);
            else {
                jit.move(scratchGPR, valueRegs.payloadGPR());
                jit.move(scratch3GPR, valueRegs.tagGPR());
            }
#endif
        } else {
            IndexingType expectedShape;
            switch (accessCase.m_type) {
            case AccessCase::IndexedInt32Load:
            case AccessCase::IndexedInt32InHit:
                expectedShape = Int32Shape;
                break;
            case AccessCase::IndexedDoubleLoad:
            case AccessCase::IndexedDoubleInHit:
                ASSERT(Options::allowDoubleShape());
                expectedShape = DoubleShape;
                break;
            case AccessCase::IndexedContiguousLoad:
            case AccessCase::IndexedContiguousInHit:
                expectedShape = ContiguousShape;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(expectedShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfPublicLength())));
            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
            if (accessCase.m_type == AccessCase::IndexedDoubleLoad || accessCase.m_type == AccessCase::IndexedDoubleInHit) {
                RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
                jit.loadDouble(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), m_scratchFPR);
                failAndIgnore.append(jit.branchIfNaN(m_scratchFPR));
                if (forInBy(accessCase.m_type))
                    jit.moveTrustedValue(jsBoolean(true), valueRegs);
                else
                    jit.boxDouble(m_scratchFPR, valueRegs);
            } else {
#if USE(JSVALUE64)
                jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratchGPR));
                failAndIgnore.append(jit.branchIfEmpty(scratchGPR));
                if (forInBy(accessCase.m_type))
                    jit.moveTrustedValue(jsBoolean(true), valueRegs);
                else
                    jit.move(scratchGPR, valueRegs.payloadGPR());
#else
                jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratch3GPR, scratchGPR));
                failAndIgnore.append(jit.branchIfEmpty(scratch3GPR));
                if (forInBy(accessCase.m_type))
                    jit.moveTrustedValue(jsBoolean(true), valueRegs);
                else {
                    jit.move(scratchGPR, valueRegs.payloadGPR());
                    jit.move(scratch3GPR, valueRegs.tagGPR());
                }
#endif
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters() && !failAndIgnore.empty()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndIgnore.append(jit.jump());
        } else
            m_failAndIgnore.append(failAndIgnore);
        return;
    }

    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore: {
        ASSERT(!accessCase.viaGlobalProxy());
        GPRReg propertyGPR = m_stubInfo.propertyGPR();

        // int32 check done in polymorphic access.
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), scratchGPR);
        fallThrough.append(jit.branchTest32(CCallHelpers::NonZero, scratchGPR, CCallHelpers::TrustedImm32(CopyOnWrite)));
        jit.and32(CCallHelpers::TrustedImm32(IndexingShapeMask), scratchGPR);

        CCallHelpers::JumpList isOutOfBounds;

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        ScratchRegisterAllocator::PreservedState preservedState;

        CCallHelpers::JumpList failAndIgnore;
        CCallHelpers::JumpList failAndRepatch;
        auto preserveReusedRegisters = [&] {
            preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
        };

        CCallHelpers::Label storeResult;
        if (accessCase.m_type == AccessCase::IndexedArrayStorageStore) {
            fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(ArrayStorageShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, ArrayStorage::vectorLengthOffset())));

            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);

#if USE(JSVALUE64)
            isOutOfBounds.append(jit.branchTest64(CCallHelpers::Zero, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset())));
#else
            isOutOfBounds.append(jit.branch32(CCallHelpers::Equal, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset() + JSValue::offsetOfTag()), CCallHelpers::TrustedImm32(JSValue::EmptyValueTag)));
#endif

            storeResult = jit.label();
            jit.storeValue(valueRegs, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()));
        } else {
            IndexingType expectedShape;
            switch (accessCase.m_type) {
            case AccessCase::IndexedInt32Store:
                expectedShape = Int32Shape;
                break;
            case AccessCase::IndexedDoubleStore:
                ASSERT(Options::allowDoubleShape());
                expectedShape = DoubleShape;
                break;
            case AccessCase::IndexedContiguousStore:
                expectedShape = ContiguousShape;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(expectedShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            isOutOfBounds.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfPublicLength())));
            storeResult = jit.label();
            switch (accessCase.m_type) {
            case AccessCase::IndexedDoubleStore: {
                RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
                auto notInt = jit.branchIfNotInt32(valueRegs);
                jit.convertInt32ToDouble(valueRegs.payloadGPR(), m_scratchFPR);
                auto ready = jit.jump();
                notInt.link(&jit);
#if USE(JSVALUE64)
                jit.unboxDoubleWithoutAssertions(valueRegs.payloadGPR(), scratch2GPR, m_scratchFPR);
#else
                jit.unboxDouble(valueRegs, m_scratchFPR);
#endif
                failAndRepatch.append(jit.branchIfNaN(m_scratchFPR));
                ready.link(&jit);

                jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
                jit.storeDouble(m_scratchFPR, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight));
                break;
            }
            case AccessCase::IndexedInt32Store:
                jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
                failAndRepatch.append(jit.branchIfNotInt32(valueRegs));
                jit.storeValue(valueRegs, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight));
                break;
            case AccessCase::IndexedContiguousStore:
                jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
                jit.storeValue(valueRegs, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight));
                // WriteBarrier must be emitted in the embedder side.
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (accessCase.m_type == AccessCase::IndexedArrayStorageStore) {
            isOutOfBounds.link(&jit);
            if (m_stubInfo.m_arrayProfileGPR != InvalidGPRReg)
                jit.or32(CCallHelpers::TrustedImm32(static_cast<uint32_t>(ArrayProfileFlag::MayStoreHole)), CCallHelpers::Address(m_stubInfo.m_arrayProfileGPR, ArrayProfile::offsetOfArrayProfileFlags()));
            jit.add32(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(scratchGPR, ArrayStorage::numValuesInVectorOffset()));
            jit.branch32(CCallHelpers::Below, scratch2GPR, CCallHelpers::Address(scratchGPR, ArrayStorage::lengthOffset())).linkTo(storeResult, &jit);

            jit.add32(CCallHelpers::TrustedImm32(1), scratch2GPR);
            jit.store32(scratch2GPR, CCallHelpers::Address(scratchGPR, ArrayStorage::lengthOffset()));
            jit.sub32(CCallHelpers::TrustedImm32(1), scratch2GPR);
            jit.jump().linkTo(storeResult, &jit);
        } else {
            isOutOfBounds.link(&jit);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfVectorLength())));
            if (m_stubInfo.m_arrayProfileGPR != InvalidGPRReg)
                jit.or32(CCallHelpers::TrustedImm32(static_cast<uint32_t>(ArrayProfileFlag::MayStoreHole)), CCallHelpers::Address(m_stubInfo.m_arrayProfileGPR, ArrayProfile::offsetOfArrayProfileFlags()));
            jit.add32(CCallHelpers::TrustedImm32(1), propertyGPR, scratch2GPR);
            jit.store32(scratch2GPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfPublicLength()));
            jit.jump().linkTo(storeResult, &jit);

        }

        if (allocator.didReuseRegisters()) {
            if (!failAndIgnore.empty()) {
                failAndIgnore.link(&jit);
                allocator.restoreReusedRegistersByPopping(jit, preservedState);
                m_failAndIgnore.append(jit.jump());
            }
            if (!failAndRepatch.empty()) {
                failAndRepatch.link(&jit);
                allocator.restoreReusedRegistersByPopping(jit, preservedState);
                m_failAndRepatch.append(jit.jump());
            }
        } else {
            m_failAndIgnore.append(failAndIgnore);
            m_failAndRepatch.append(failAndRepatch);
        }
        return;
    }

    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat16Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat16Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.

        TypedArrayType type = toTypedArrayType(accessCase.m_type);
        bool isResizableOrGrowableShared = forResizableTypedArray(accessCase.m_type);

        GPRReg propertyGPR = m_stubInfo.propertyGPR();

        fallThrough.append(jit.branch8(CCallHelpers::NotEqual, CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), CCallHelpers::TrustedImm32(typeForTypedArrayType(type))));
        if (!isResizableOrGrowableShared)
            fallThrough.append(jit.branchTest8(CCallHelpers::NonZero, CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfMode()), CCallHelpers::TrustedImm32(isResizableOrGrowableSharedMode)));

        if (isInt(type))
            m_failAndRepatch.append(jit.branchIfNotInt32(valueRegs));
        else {
            ASSERT(isFloat(type));
            RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
            auto doubleCase = jit.branchIfNotInt32(valueRegs);
            jit.convertInt32ToDouble(valueRegs.payloadGPR(), m_scratchFPR);
            auto ready = jit.jump();
            doubleCase.link(&jit);
#if USE(JSVALUE64)
            m_failAndRepatch.append(jit.branchIfNotNumber(valueRegs.payloadGPR()));
            jit.unboxDoubleWithoutAssertions(valueRegs.payloadGPR(), scratchGPR, m_scratchFPR);
#else
            m_failAndRepatch.append(jit.branch32(CCallHelpers::Above, valueRegs.tagGPR(), CCallHelpers::TrustedImm32(JSValue::LowestTag)));
            jit.unboxDouble(valueRegs, m_scratchFPR);
#endif
            ready.link(&jit);
        }

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList outOfBoundsAfterSave;
        if (isResizableOrGrowableShared) {
            jit.loadTypedArrayLength(baseGPR, scratch2GPR, scratchGPR, scratch2GPR, type);
            jit.signExtend32ToPtr(propertyGPR, scratchGPR);
#if USE(LARGE_TYPED_ARRAYS)
            // The length is a size_t, so either 32 or 64 bits depending on the platform.
            outOfBoundsAfterSave.append(jit.branch64(CCallHelpers::AboveOrEqual, scratchGPR, scratch2GPR));
#else
            outOfBoundsAfterSave.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratch2GPR));
#endif
        } else {
            CCallHelpers::Address addressOfLength = CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength());
            jit.signExtend32ToPtr(propertyGPR, scratchGPR);
#if USE(LARGE_TYPED_ARRAYS)
            // The length is a UCPURegister, so either 32 or 64 bits depending on the platform.
            outOfBoundsAfterSave.append(jit.branch64(CCallHelpers::AboveOrEqual, scratchGPR, addressOfLength));
#else
            outOfBoundsAfterSave.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, addressOfLength));
#endif
        }

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength()), scratchGPR);
#else
        jit.load32(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength()), scratchGPR);
#endif
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfVector()), scratch2GPR);
        jit.cageConditionally(Gigacage::Primitive, scratch2GPR, scratchGPR, scratchGPR);
        jit.signExtend32ToPtr(propertyGPR, scratchGPR);
        if (isInt(type)) {
            if (isClamped(type)) {
                ASSERT(elementSize(type) == 1);
                ASSERT(!JSC::isSigned(type));
                jit.getEffectiveAddress(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne), scratch2GPR);
                jit.move(valueRegs.payloadGPR(), scratchGPR);
                auto inBounds = jit.branch32(CCallHelpers::BelowOrEqual, scratchGPR, CCallHelpers::TrustedImm32(0xff));
                auto tooBig = jit.branch32(CCallHelpers::GreaterThan, scratchGPR, CCallHelpers::TrustedImm32(0xff));
                jit.xor32(scratchGPR, scratchGPR);
                auto clamped = jit.jump();
                tooBig.link(&jit);
                jit.move(CCallHelpers::TrustedImm32(0xff), scratchGPR);
                clamped.link(&jit);
                inBounds.link(&jit);
                jit.store8(scratchGPR, CCallHelpers::Address(scratch2GPR));
            } else {
                switch (elementSize(type)) {
                case 1:
                    jit.store8(valueRegs.payloadGPR(), CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne));
                    break;
                case 2:
                    jit.store16(valueRegs.payloadGPR(), CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo));
                    break;
                case 4:
                    jit.store32(valueRegs.payloadGPR(), CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour));
                    break;
                default:
                    CRASH();
                }
            }
        } else {
            ASSERT(isFloat(type));
            RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
            switch (elementSize(type)) {
            case 2:
                jit.convertDoubleToFloat16(m_scratchFPR, m_scratchFPR);
                jit.storeFloat16(m_scratchFPR, CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo));
                break;
            case 4:
                jit.convertDoubleToFloat(m_scratchFPR, m_scratchFPR);
                jit.storeFloat(m_scratchFPR, CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour));
                break;
            case 8: {
                jit.storeDouble(m_scratchFPR, CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesEight));
                break;
            }
            default:
                CRASH();
            }
        }

        switch (m_stubInfo.accessType) {
        case AccessType::PutByValDirectStrict:
        case AccessType::PutByValDirectSloppy: {
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            succeed();

            outOfBoundsAfterSave.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
            break;
        }
        default:
            outOfBoundsAfterSave.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            succeed();
            break;
        }
        return;
    }

    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
        emitDefaultGuard();

        fallThrough.append(
            jit.branchPtr(
                CCallHelpers::NotEqual, m_stubInfo.prototypeGPR(),
                CCallHelpers::TrustedImmPtr(accessCase.as<InstanceOfAccessCase>().prototype())));
        break;

    case AccessCase::InstanceOfMegamorphic: {
        // Legend: value = `base instanceof prototypeGPR`.
        ASSERT(!accessCase.viaGlobalProxy());
        GPRReg prototypeGPR = m_stubInfo.prototypeGPR();

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
#if USE(JSVALUE64)
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        JSValueRegs scratchRegs(scratch2GPR);
#else
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
        JSValueRegs scratchRegs(scratch2GPR, scratch3GPR);
#endif

        if (!m_stubInfo.prototypeIsKnownObject)
            m_failAndIgnore.append(jit.branchIfNotObject(prototypeGPR));

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
        CCallHelpers::JumpList failAndIgnore;

        jit.move(baseGPR, scratchGPR);

        CCallHelpers::Label loop(&jit);
        jit.emitLoadPrototype(vm, scratchGPR, scratchRegs, failAndIgnore);
        CCallHelpers::Jump isInstance = jit.branchPtr(CCallHelpers::Equal, scratchRegs.payloadGPR(), prototypeGPR);
        jit.move(scratchRegs.payloadGPR(), scratchGPR);
#if USE(JSVALUE64)
        jit.branchIfCell(JSValueRegs(scratchGPR)).linkTo(loop, &jit);
#else
        jit.branchTestPtr(CCallHelpers::NonZero, scratchGPR).linkTo(loop, &jit);
#endif

        jit.boxBooleanPayload(false, valueRegs.payloadGPR());
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        isInstance.link(&jit);
        jit.boxBooleanPayload(true, valueRegs.payloadGPR());
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndIgnore.append(jit.jump());
        } else
            m_failAndIgnore.append(failAndIgnore);
        return;
    }

    case AccessCase::CheckPrivateBrand: {
        emitDefaultGuard();
        succeed();
        return;
    }

    case AccessCase::IndexedMegamorphicLoad: {
#if USE(JSVALUE64)
        ASSERT(!accessCase.viaGlobalProxy());

        CCallHelpers::JumpList slowCases;

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
        GPRReg scratch4GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList notString;
        GPRReg propertyGPR = m_stubInfo.propertyGPR();
        if (!m_stubInfo.propertyIsString) {
            slowCases.append(jit.branchIfNotCell(propertyGPR));
            slowCases.append(jit.branchIfNotString(propertyGPR));
        }

        jit.loadPtr(CCallHelpers::Address(propertyGPR, JSString::offsetOfValue()), scratch4GPR);
        slowCases.append(jit.branchIfRopeStringImpl(scratch4GPR));
        slowCases.append(jit.branchTest32(CCallHelpers::Zero, CCallHelpers::Address(scratch4GPR, StringImpl::flagsOffset()), CCallHelpers::TrustedImm32(StringImpl::flagIsAtom())));

        slowCases.append(jit.loadMegamorphicProperty(vm, baseGPR, scratch4GPR, nullptr, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR));

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            slowCases.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        } else
            m_failAndRepatch.append(slowCases);
#endif
        return;
    }

    case AccessCase::LoadMegamorphic: {
#if USE(JSVALUE64)
        ASSERT(!accessCase.viaGlobalProxy());
        CCallHelpers::JumpList failAndRepatch;
        auto* uid = accessCase.m_identifier.uid();

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
        GPRReg scratch4GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList slowCases;
        if (useHandlerIC()) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfUid()), scratch4GPR);
            slowCases.append(jit.loadMegamorphicProperty(vm, baseGPR, scratch4GPR, nullptr, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR));
        } else
            slowCases.append(jit.loadMegamorphicProperty(vm, baseGPR, InvalidGPRReg, uid, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR));

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            slowCases.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        } else
            m_failAndRepatch.append(slowCases);
#endif
        return;
    }

    case AccessCase::StoreMegamorphic: {
#if USE(JSVALUE64)
        ASSERT(!accessCase.viaGlobalProxy());
        CCallHelpers::JumpList failAndRepatch;
        auto* uid = accessCase.m_identifier.uid();

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
        GPRReg scratch4GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);

        CCallHelpers::JumpList slow;
        CCallHelpers::JumpList reallocating;

        if (useHandlerIC()) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfUid()), scratch4GPR);
            std::tie(slow, reallocating) = jit.storeMegamorphicProperty(vm, baseGPR, scratch4GPR, nullptr, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR);
        } else
            std::tie(slow, reallocating) = jit.storeMegamorphicProperty(vm, baseGPR, InvalidGPRReg, uid, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR);

        CCallHelpers::Label doneLabel = jit.label();
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            slow.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        } else
            m_failAndRepatch.append(slow);

        reallocating.link(&jit);
        {
            // Handle the case where we are allocating out-of-line using an operation.
            InlineCacheCompiler::SpillState spillState = preserveLiveRegistersToStackForCall();
            jit.makeSpaceOnStackForCCall();
            jit.setupArguments<decltype(operationPutByMegamorphicReallocating)>(CCallHelpers::TrustedImmPtr(&vm), baseGPR, valueRegs.payloadGPR(), scratch3GPR);
            jit.prepareCallOperation(vm);
            jit.callOperation<OperationPtrTag>(operationPutByMegamorphicReallocating);
            jit.reclaimSpaceOnStackForCCall();
            restoreLiveRegistersFromStackForCall(spillState, { });
            jit.jump().linkTo(doneLabel, &jit);
        }
#endif
        return;
    }

    case AccessCase::InMegamorphic: {
#if USE(JSVALUE64)
        ASSERT(!accessCase.viaGlobalProxy());
        CCallHelpers::JumpList failAndRepatch;
        auto* uid = accessCase.m_identifier.uid();

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
        GPRReg scratch4GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList slowCases;
        if (useHandlerIC()) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfUid()), scratch4GPR);
            slowCases.append(jit.hasMegamorphicProperty(vm, baseGPR, scratch4GPR, nullptr, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR));
        } else
            slowCases.append(jit.hasMegamorphicProperty(vm, baseGPR, InvalidGPRReg, uid, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR));

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            slowCases.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        } else
            m_failAndRepatch.append(slowCases);
#endif
        return;
    }

    case AccessCase::IndexedMegamorphicIn: {
#if USE(JSVALUE64)
        ASSERT(!accessCase.viaGlobalProxy());

        CCallHelpers::JumpList slowCases;

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
        GPRReg scratch4GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList notString;
        GPRReg propertyGPR = m_stubInfo.propertyGPR();
        if (!m_stubInfo.propertyIsString) {
            slowCases.append(jit.branchIfNotCell(propertyGPR));
            slowCases.append(jit.branchIfNotString(propertyGPR));
        }

        jit.loadPtr(CCallHelpers::Address(propertyGPR, JSString::offsetOfValue()), scratch4GPR);
        slowCases.append(jit.branchIfRopeStringImpl(scratch4GPR));
        slowCases.append(jit.branchTest32(CCallHelpers::Zero, CCallHelpers::Address(scratch4GPR, StringImpl::flagsOffset()), CCallHelpers::TrustedImm32(StringImpl::flagIsAtom())));

        slowCases.append(jit.hasMegamorphicProperty(vm, baseGPR, scratch4GPR, nullptr, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR));

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            slowCases.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        } else
            m_failAndRepatch.append(slowCases);
#endif
        return;
    }

    case AccessCase::IndexedMegamorphicStore: {
#if USE(JSVALUE64)
        ASSERT(!accessCase.viaGlobalProxy());

        CCallHelpers::JumpList slowCases;

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
        GPRReg scratch4GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);

        CCallHelpers::JumpList notString;
        GPRReg propertyGPR = m_stubInfo.propertyGPR();
        if (!m_stubInfo.propertyIsString) {
            slowCases.append(jit.branchIfNotCell(propertyGPR));
            slowCases.append(jit.branchIfNotString(propertyGPR));
        }

        jit.loadPtr(CCallHelpers::Address(propertyGPR, JSString::offsetOfValue()), scratch4GPR);
        slowCases.append(jit.branchIfRopeStringImpl(scratch4GPR));
        slowCases.append(jit.branchTest32(CCallHelpers::Zero, CCallHelpers::Address(scratch4GPR, StringImpl::flagsOffset()), CCallHelpers::TrustedImm32(StringImpl::flagIsAtom())));

        auto [slow, reallocating] = jit.storeMegamorphicProperty(vm, baseGPR, scratch4GPR, nullptr, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR);
        slowCases.append(WTFMove(slow));

        CCallHelpers::Label doneLabel = jit.label();
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            slowCases.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        } else
            m_failAndRepatch.append(slowCases);

        reallocating.link(&jit);
        {
            // Handle the case where we are allocating out-of-line using an operation.
            InlineCacheCompiler::SpillState spillState = preserveLiveRegistersToStackForCall();
            jit.makeSpaceOnStackForCCall();
            jit.setupArguments<decltype(operationPutByMegamorphicReallocating)>(CCallHelpers::TrustedImmPtr(&vm), baseGPR, valueRegs.payloadGPR(), scratch3GPR);
            jit.prepareCallOperation(vm);
            jit.callOperation<OperationPtrTag>(operationPutByMegamorphicReallocating);
            jit.reclaimSpaceOnStackForCCall();
            restoreLiveRegistersFromStackForCall(spillState, { });
            jit.jump().linkTo(doneLabel, &jit);
        }
#endif
        return;
    }

    default:
        emitDefaultGuard();
        break;
    }

    generateWithConditionChecks(index, accessCase);
}

void InlineCacheCompiler::generateWithoutGuard(unsigned index, AccessCase& accessCase)
{
    RELEASE_ASSERT(hasConstantIdentifier(m_stubInfo.accessType));
    accessCase.checkConsistency(m_stubInfo);
    generateWithConditionChecks(index, accessCase);
}

static void collectConditions(AccessCase& accessCase, Vector<ObjectPropertyCondition, 64>& watchedConditions, Vector<ObjectPropertyCondition, 64>& checkingConditions)
{
    for (const ObjectPropertyCondition& condition : accessCase.conditionSet()) {
        RELEASE_ASSERT(!accessCase.polyProtoAccessChain());

        if (condition.isWatchableAssumingImpurePropertyWatchpoint(PropertyCondition::WatchabilityEffort::EnsureWatchability, Concurrency::MainThread)) {
            watchedConditions.append(condition);
            continue;
        }

        // For now, we only allow equivalence when it's watchable.
        RELEASE_ASSERT(condition.condition().kind() != PropertyCondition::Equivalence);

        if (!condition.structureEnsuresValidityAssumingImpurePropertyWatchpoint(Concurrency::MainThread)) {
            // The reason why this cannot happen is that we require that PolymorphicAccess calls
            // AccessCase::generate() only after it has verified that
            // AccessCase::couldStillSucceed() returned true.

            dataLog("This condition is no longer met: ", condition, "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        checkingConditions.append(condition);
    }
}

void InlineCacheCompiler::generateWithConditionChecks(unsigned index, AccessCase& accessCase)
{
    Vector<ObjectPropertyCondition, 64> checkingConditions;
    collectConditions(accessCase, m_conditions, checkingConditions);

    CCallHelpers& jit = *m_jit;
    GPRReg scratchGPR = m_scratchGPR;
    for (auto& condition : checkingConditions) {
        // We will emit code that has a weak reference that isn't otherwise listed anywhere.
        Structure* structure = condition.object()->structure();
        m_weakStructures.append(structure->id());

        jit.move(CCallHelpers::TrustedImmPtr(condition.object()), scratchGPR);
        m_failAndRepatch.append(jit.branchStructure(CCallHelpers::NotEqual, CCallHelpers::Address(scratchGPR, JSCell::structureIDOffset()), structure));
    }

    generateAccessCase(index, accessCase);
}

void InlineCacheCompiler::generateAccessCase(unsigned index, AccessCase& accessCase)
{
    SuperSamplerScope superSamplerScope(false);
    dataLogLnIf(InlineCacheCompilerInternal::verbose, "\n\nGenerating code for: ", accessCase);

    CCallHelpers& jit = *m_jit;
    VM& vm = m_vm;
    ECMAMode ecmaMode = m_ecmaMode;
    JSValueRegs valueRegs = m_stubInfo.valueRegs();
    GPRReg baseGPR = m_stubInfo.m_baseGPR;
    GPRReg thisGPR = m_stubInfo.thisValueIsInExtraGPR() ? m_stubInfo.thisGPR() : baseGPR;
    GPRReg scratchGPR = m_scratchGPR;

    switch (accessCase.m_type) {
    case AccessCase::InHit:
    case AccessCase::InMiss:
        jit.boxBoolean(accessCase.m_type == AccessCase::InHit, valueRegs);
        succeed();
        return;

    case AccessCase::Miss:
        jit.moveTrustedValue(jsUndefined(), valueRegs);
        succeed();
        return;

    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
        jit.boxBooleanPayload(accessCase.m_type == AccessCase::InstanceOfHit, valueRegs.payloadGPR());
        succeed();
        return;

    case AccessCase::Load:
    case AccessCase::GetGetter: {
        Structure* currStructure = accessCase.structure();
        if (auto* object = accessCase.tryGetAlternateBase())
            currStructure = object->structure();

        if (isValidOffset(accessCase.m_offset))
            currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

        GPRReg propertyOwnerGPR = baseGPR;
        if (accessCase.polyProtoAccessChain()) {
            // This isn't pretty, but we know we got here via generateWithGuard,
            // and it left the baseForAccess inside scratchGPR. We could re-derive the base,
            // but it'd require emitting the same code to load the base twice.
            propertyOwnerGPR = scratchGPR;
        } else if (auto* object = accessCase.tryGetAlternateBase()) {
            jit.move(CCallHelpers::TrustedImmPtr(object), scratchGPR);
            propertyOwnerGPR = scratchGPR;
        } else if (accessCase.viaGlobalProxy()) {
            // We only need this when loading an inline or out of line property. For customs accessors,
            // we can invoke with a receiver value that is a JSGlobalProxy. For custom values, we unbox to the
            // JSGlobalProxy's target. For getters/setters, we'll also invoke them with the JSGlobalProxy as |this|,
            // but we need to load the actual GetterSetter cell from the JSGlobalProxy's target.
            ASSERT(canBeViaGlobalProxy(accessCase.m_type));
            jit.loadPtr(CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), scratchGPR);
            propertyOwnerGPR = scratchGPR;
        }

        GPRReg storageGPR = propertyOwnerGPR;
        if (!isInlineOffset(accessCase.m_offset)) {
            jit.loadPtr(CCallHelpers::Address(propertyOwnerGPR, JSObject::butterflyOffset()), scratchGPR);
            storageGPR = scratchGPR;
        }

        jit.loadValue(CCallHelpers::Address(storageGPR, offsetRelativeToBase(accessCase.m_offset)), valueRegs);
        succeed();
        return;
    }

    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter: {
        Structure* currStructure = accessCase.structure();
        if (auto* object = accessCase.tryGetAlternateBase())
            currStructure = object->structure();

        if (isValidOffset(accessCase.m_offset))
            currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

        GPRReg baseForCustom = baseGPR != thisGPR ? thisGPR : baseGPR; // Check if it is a super access;
        if (accessCase.m_type == AccessCase::CustomValueGetter || accessCase.m_type == AccessCase::CustomValueSetter) {
            baseForCustom = baseGPR;
            if (accessCase.polyProtoAccessChain()) {
                // This isn't pretty, but we know we got here via generateWithGuard,
                // and it left the baseForAccess inside scratchGPR. We could re-derive the base,
                // but it'd require emitting the same code to load the base twice.
                baseForCustom = scratchGPR;
            } else if (auto* object = accessCase.tryGetAlternateBase()) {
                jit.move(CCallHelpers::TrustedImmPtr(object), scratchGPR);
                baseForCustom = scratchGPR;
            } else if (accessCase.viaGlobalProxy()) {
                ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                jit.loadPtr(CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), scratchGPR);
                baseForCustom = scratchGPR;
            }
        }

        if (accessCase.m_type == AccessCase::CustomAccessorGetter && accessCase.as<GetterSetterAccessCase>().domAttribute()) {
            auto& access = accessCase.as<GetterSetterAccessCase>();
            // We do not need to emit CheckDOM operation since structure check ensures
            // that the structure of the given base value is accessCase.structure()! So all we should
            // do is performing the CheckDOM thingy in IC compiling time here.
            if (!accessCase.structure()->classInfoForCells()->isSubClassOf(access.domAttribute()->classInfo)) {
                m_failAndIgnore.append(jit.jump());
                return;
            }

            if (Options::useDOMJIT() && access.domAttribute()->domJIT) {
                emitDOMJITGetter(accessCase.structure()->globalObject(), access.domAttribute()->domJIT, baseGPR);
                return;
            }
        }

        // This also does the necessary calculations of whether or not we're an
        // exception handling call site.
        InlineCacheCompiler::SpillState spillState = preserveLiveRegistersToStackForCall();

        if (m_stubInfo.useDataIC) {
            callSiteIndexForExceptionHandlingOrOriginal();
            jit.transfer32(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
        } else
            jit.store32(CCallHelpers::TrustedImm32(callSiteIndexForExceptionHandlingOrOriginal().bits()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

        // We do not need to keep globalObject alive since
        // 1. if it is CustomValue, the owner CodeBlock (even if JSGlobalObject* is one of CodeBlock that is inlined and held by DFG CodeBlock) must keep it alive.
        // 2. if it is CustomAccessor, structure should hold it.
        JSGlobalObject* globalObject = currStructure->globalObject();

        // Need to make room for the C call so any of our stack spillage isn't overwritten. It's
        // hard to track if someone did spillage or not, so we just assume that we always need
        // to make some space here.
        jit.makeSpaceOnStackForCCall();

        jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);

        // getter: EncodedJSValue (*GetValueFunc)(JSGlobalObject*, EncodedJSValue thisValue, PropertyName);
        // setter: bool (*PutValueFunc)(JSGlobalObject*, EncodedJSValue thisObject, EncodedJSValue value, PropertyName);
        // Custom values are passed the slotBase (the property holder), custom accessors are passed the thisValue (receiver).
        // We do not need to keep globalObject alive since the owner CodeBlock (even if JSGlobalObject* is one of CodeBlock that is inlined and held by DFG CodeBlock)
        // must keep it alive.
        auto customAccessor = accessCase.as<GetterSetterAccessCase>().m_customAccessor;
        bool isGetter = accessCase.m_type == AccessCase::CustomValueGetter || accessCase.m_type == AccessCase::CustomAccessorGetter;
        if (isGetter) {
            RELEASE_ASSERT(accessCase.m_identifier);
            if (Options::useJITCage()) {
                jit.setupArguments<GetValueFuncWithPtr>(
                    CCallHelpers::TrustedImmPtr(globalObject),
                    CCallHelpers::CellValue(baseForCustom),
                    CCallHelpers::TrustedImmPtr(accessCase.uid()),
                    CCallHelpers::TrustedImmPtr(customAccessor.taggedPtr()));
                jit.callOperation<OperationPtrTag>(vmEntryCustomGetter);
            } else {
                jit.setupArguments<GetValueFunc>(
                    CCallHelpers::TrustedImmPtr(globalObject),
                    CCallHelpers::CellValue(baseForCustom),
                    CCallHelpers::TrustedImmPtr(accessCase.uid()));
                jit.callOperation<CustomAccessorPtrTag>(customAccessor);
            }
            jit.setupResults(valueRegs);
        } else {
            if (Options::useJITCage()) {
                jit.setupArguments<PutValueFuncWithPtr>(
                    CCallHelpers::TrustedImmPtr(globalObject),
                    CCallHelpers::CellValue(baseForCustom),
                    valueRegs,
                    CCallHelpers::TrustedImmPtr(accessCase.uid()),
                    CCallHelpers::TrustedImmPtr(customAccessor.taggedPtr()));
                jit.callOperation<OperationPtrTag>(vmEntryCustomSetter);
            } else {
                jit.setupArguments<PutValueFunc>(
                    CCallHelpers::TrustedImmPtr(globalObject),
                    CCallHelpers::CellValue(baseForCustom),
                    valueRegs,
                    CCallHelpers::TrustedImmPtr(accessCase.uid()));
                jit.callOperation<CustomAccessorPtrTag>(customAccessor);
            }
        }

        jit.reclaimSpaceOnStackForCCall();

        CCallHelpers::Jump noException = jit.emitExceptionCheck(vm, CCallHelpers::InvertedExceptionCheck);

        restoreLiveRegistersFromStackForCallWithThrownException(spillState);
        emitExplicitExceptionHandler();

        noException.link(&jit);
        RegisterSet dontRestore;
        if (isGetter) {
            // This is the result value. We don't want to overwrite the result with what we stored to the stack.
            // We sometimes have to store it to the stack just in case we throw an exception and need the original value.
            dontRestore.add(valueRegs, IgnoreVectors);
        }
        restoreLiveRegistersFromStackForCall(spillState, dontRestore);
        succeed();
        return;
    }

    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::IntrinsicGetter: {
        Structure* currStructure = accessCase.structure();
        if (auto* object = accessCase.tryGetAlternateBase())
            currStructure = object->structure();

        if (isValidOffset(accessCase.m_offset))
            currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

        {
            GPRReg propertyOwnerGPR = baseGPR;
            if (accessCase.polyProtoAccessChain()) {
                // This isn't pretty, but we know we got here via generateWithGuard,
                // and it left the baseForAccess inside scratchGPR. We could re-derive the base,
                // but it'd require emitting the same code to load the base twice.
                propertyOwnerGPR = scratchGPR;
            } else if (auto* object = accessCase.tryGetAlternateBase()) {
                jit.move(CCallHelpers::TrustedImmPtr(object), scratchGPR);
                propertyOwnerGPR = scratchGPR;
            } else if (accessCase.viaGlobalProxy()) {
                ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                // We only need this when loading an inline or out of line property. For customs accessors,
                // we can invoke with a receiver value that is a JSGlobalProxy. For custom values, we unbox to the
                // JSGlobalProxy's target. For getters/setters, we'll also invoke them with the JSGlobalProxy as |this|,
                // but we need to load the actual GetterSetter cell from the JSGlobalProxy's target.
                jit.loadPtr(CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), scratchGPR);
                propertyOwnerGPR = scratchGPR;
            }

            GPRReg storageGPR = propertyOwnerGPR;
            if (!isInlineOffset(accessCase.m_offset)) {
                jit.loadPtr(CCallHelpers::Address(propertyOwnerGPR, JSObject::butterflyOffset()), scratchGPR);
                storageGPR = scratchGPR;
            }

#if USE(JSVALUE64)
            jit.load64(CCallHelpers::Address(storageGPR, offsetRelativeToBase(accessCase.m_offset)), scratchGPR);
#else
            jit.load32(CCallHelpers::Address(storageGPR, offsetRelativeToBase(accessCase.m_offset) + PayloadOffset), scratchGPR);
#endif
        }

        if (accessCase.m_type == AccessCase::IntrinsicGetter) {
            jit.loadPtr(CCallHelpers::Address(scratchGPR, GetterSetter::offsetOfGetter()), scratchGPR);
            m_failAndIgnore.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImmPtr(accessCase.as<IntrinsicGetterAccessCase>().intrinsicFunction())));
            emitIntrinsicGetter(accessCase.as<IntrinsicGetterAccessCase>());
            return;
        }

        bool isGetter = accessCase.m_type == AccessCase::Getter;

        // This also does the necessary calculations of whether or not we're an
        // exception handling call site.
        InlineCacheCompiler::SpillState spillState = preserveLiveRegistersToStackForCall();

        if (m_stubInfo.useDataIC) {
            callSiteIndexForExceptionHandlingOrOriginal();
            jit.transfer32(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
        } else
            jit.store32(CCallHelpers::TrustedImm32(callSiteIndexForExceptionHandlingOrOriginal().bits()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

        ASSERT(baseGPR != scratchGPR);

        // Create a JS call using a JS call inline cache. Assume that:
        //
        // - SP is aligned and represents the extent of the calling compiler's stack usage.
        //
        // - FP is set correctly (i.e. it points to the caller's call frame header).
        //
        // - SP - FP is an aligned difference.
        //
        // - Any byte between FP (exclusive) and SP (inclusive) could be live in the calling
        //   code.
        //
        // Therefore, we temporarily grow the stack for the purpose of the call and then
        // shrink it after.

        setSpillStateForJSCall(spillState);

        // There is a "this" argument.
        // ... and a value argument if we're calling a setter.
        unsigned numberOfParameters = isGetter ? 1 : 2;

        // Get the accessor; if there ain't one then the result is jsUndefined().
        // Note that GetterSetter always has cells for both. If it is not set (like, getter exits, but setter is not set), Null{Getter,Setter}Function is stored.
        std::optional<CCallHelpers::Jump> returnUndefined;
        if (isGetter) {
            jit.loadPtr(CCallHelpers::Address(scratchGPR, GetterSetter::offsetOfGetter()), scratchGPR);
            returnUndefined = jit.branchIfType(scratchGPR, NullSetterFunctionType);
        } else {
            jit.loadPtr(CCallHelpers::Address(scratchGPR, GetterSetter::offsetOfSetter()), scratchGPR);
            if (ecmaMode.isStrict()) {
                CCallHelpers::Jump shouldNotThrowError = jit.branchIfNotType(scratchGPR, NullSetterFunctionType);
                // We replace setter with this AccessCase's JSGlobalObject::nullSetterStrictFunction, which will throw an error with the right JSGlobalObject.
                if (useHandlerIC()) {
                    jit.loadPtr(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfGlobalObject()), scratchGPR);
                    jit.loadPtr(CCallHelpers::Address(scratchGPR, JSGlobalObject::offsetOfNullSetterStrictFunction()), scratchGPR);
                } else
                    jit.move(CCallHelpers::TrustedImmPtr(m_globalObject->nullSetterStrictFunction()), scratchGPR);
                shouldNotThrowError.link(&jit);
            }
        }

        unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
        ASSERT(!(numberOfRegsForCall % stackAlignmentRegisters()));
        unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);

        unsigned alignedNumberOfBytesForCall = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(numberOfBytesForCall);

        jit.subPtr(CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall), CCallHelpers::stackPointerRegister);

        CCallHelpers::Address calleeFrame = CCallHelpers::Address(CCallHelpers::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

        jit.store32(CCallHelpers::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + PayloadOffset));

        jit.storeCell(scratchGPR, calleeFrame.withOffset(CallFrameSlot::callee * sizeof(Register)));

        jit.storeCell(thisGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(0).offset() * sizeof(Register)));

        if (!isGetter)
            jit.storeValue(valueRegs, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(1).offset() * sizeof(Register)));

        if (useHandlerIC()) {
            ASSERT(scratchGPR != GPRInfo::handlerGPR);
            // handlerGPR can be the same to BaselineJITRegisters::Call::calleeJSR.
            if constexpr (GPRInfo::handlerGPR == BaselineJITRegisters::Call::calleeJSR.payloadGPR()) {
                jit.swap(scratchGPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
                jit.addPtr(CCallHelpers::TrustedImm32(InlineCacheHandler::offsetOfCallLinkInfos() + sizeof(DataOnlyCallLinkInfo) * index), scratchGPR, BaselineJITRegisters::Call::callLinkInfoGPR);
            } else {
                jit.move(scratchGPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
                jit.addPtr(CCallHelpers::TrustedImm32(InlineCacheHandler::offsetOfCallLinkInfos() + sizeof(DataOnlyCallLinkInfo) * index), GPRInfo::handlerGPR, BaselineJITRegisters::Call::callLinkInfoGPR);
            }
            CallLinkInfo::emitDataICFastPath(jit);
        } else {
            jit.move(scratchGPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
#if USE(JSVALUE32_64)
            // We *always* know that the getter/setter, if non-null, is a cell.
            jit.move(CCallHelpers::TrustedImm32(JSValue::CellTag), BaselineJITRegisters::Call::calleeJSR.tagGPR());
#endif
            m_callLinkInfos[index] = makeUnique<OptimizingCallLinkInfo>(m_stubInfo.codeOrigin, nullptr);
            auto* callLinkInfo = m_callLinkInfos[index].get();
            callLinkInfo->setUpCall(CallLinkInfo::Call);
            CallLinkInfo::emitFastPath(jit, callLinkInfo);
        }

        if (isGetter) {
            jit.setupResults(valueRegs);
            auto done = jit.jump();
            ASSERT(returnUndefined);
            returnUndefined.value().link(&jit);
            jit.moveTrustedValue(jsUndefined(), valueRegs);
            done.link(&jit);
        }

        if (m_stubInfo.useDataIC) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), scratchGPR);
            if (useHandlerIC())
                jit.addPtr(CCallHelpers::TrustedImm32(-(sizeof(CallerFrameAndPC) + maxFrameExtentForSlowPathCall + m_preservedReusedRegisterState.numberOfBytesPreserved + spillState.numberOfStackBytesUsedForRegisterPreservation)), scratchGPR);
            else
                jit.addPtr(CCallHelpers::TrustedImm32(-(m_preservedReusedRegisterState.numberOfBytesPreserved + spillState.numberOfStackBytesUsedForRegisterPreservation)), scratchGPR);
            jit.addPtr(scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
        } else {
            int stackPointerOffset = (jit.codeBlock()->stackPointerOffset() * sizeof(Register)) - m_preservedReusedRegisterState.numberOfBytesPreserved - spillState.numberOfStackBytesUsedForRegisterPreservation;
            jit.addPtr(CCallHelpers::TrustedImm32(stackPointerOffset), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
        }

        RegisterSet dontRestore;
        if (isGetter) {
            // This is the result value. We don't want to overwrite the result with what we stored to the stack.
            // We sometimes have to store it to the stack just in case we throw an exception and need the original value.
            dontRestore.add(valueRegs, IgnoreVectors);
        }
        restoreLiveRegistersFromStackForCall(spillState, dontRestore);
        succeed();
        return;
    }

    case AccessCase::Replace: {
        ASSERT(canBeViaGlobalProxy(accessCase.m_type));
        GPRReg base = baseGPR;
        if (accessCase.viaGlobalProxy()) {
            // This aint pretty, but the path that structure checks loads the real base into scratchGPR.
            base = scratchGPR;
        }

        if (isInlineOffset(accessCase.m_offset)) {
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    base,
                    JSObject::offsetOfInlineStorage() +
                    offsetInInlineStorage(accessCase.m_offset) * sizeof(JSValue)));
        } else {
            jit.loadPtr(CCallHelpers::Address(base, JSObject::butterflyOffset()), scratchGPR);
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    scratchGPR, offsetInButterfly(accessCase.m_offset) * sizeof(JSValue)));
        }

        if (accessCase.viaGlobalProxy()) {
            ASSERT(canBeViaGlobalProxy(accessCase.m_type));
            CCallHelpers::JumpList skipBarrier;
            skipBarrier.append(jit.branchIfNotCell(valueRegs));
            if (!isInlineOffset(accessCase.m_offset))
                jit.loadPtr(CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), scratchGPR);
            skipBarrier.append(jit.barrierBranch(vm, scratchGPR, scratchGPR));

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), scratchGPR);

            auto spillState = preserveLiveRegistersToStackForCallWithoutExceptions();

            jit.setupArguments<decltype(operationWriteBarrierSlowPath)>(CCallHelpers::TrustedImmPtr(&vm), scratchGPR);
            jit.prepareCallOperation(vm);
            jit.callOperation<OperationPtrTag>(operationWriteBarrierSlowPath);
            restoreLiveRegistersFromStackForCall(spillState);

            skipBarrier.link(&jit);
        }

        succeed();
        return;
    }

    case AccessCase::Transition: {
        ASSERT(!accessCase.viaGlobalProxy());
        // AccessCase::createTransition() should have returned null if this wasn't true.
        RELEASE_ASSERT(GPRInfo::numberOfRegisters >= 6 || !accessCase.structure()->outOfLineCapacity() || accessCase.structure()->outOfLineCapacity() == accessCase.newStructure()->outOfLineCapacity());

        // NOTE: This logic is duplicated in AccessCase::doesCalls(). It's important that doesCalls() knows
        // exactly when this would make calls.
        bool allocating = accessCase.newStructure()->outOfLineCapacity() != accessCase.structure()->outOfLineCapacity();
        bool reallocating = allocating && accessCase.structure()->outOfLineCapacity();
        bool allocatingInline = allocating && !accessCase.structure()->couldHaveIndexingHeader();

        auto allocator = makeDefaultScratchAllocator(scratchGPR);

        GPRReg scratchGPR2 = InvalidGPRReg;
        GPRReg scratchGPR3 = InvalidGPRReg;
        if (allocatingInline) {
            scratchGPR2 = allocator.allocateScratchGPR();
            scratchGPR3 = allocator.allocateScratchGPR();
        }

        ScratchRegisterAllocator::PreservedState preservedState =
            allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);

        CCallHelpers::JumpList slowPath;

        ASSERT(accessCase.structure()->transitionWatchpointSetHasBeenInvalidated());

        if (allocating) {
            size_t newSize = accessCase.newStructure()->outOfLineCapacity() * sizeof(JSValue);

            if (allocatingInline) {
                Allocator allocator = vm.auxiliarySpace().allocatorForNonInline(newSize, AllocatorForMode::AllocatorIfExists);

                jit.emitAllocate(scratchGPR, JITAllocator::constant(allocator), scratchGPR2, scratchGPR3, slowPath);
                jit.addPtr(CCallHelpers::TrustedImm32(newSize + sizeof(IndexingHeader)), scratchGPR);

                size_t oldSize = accessCase.structure()->outOfLineCapacity() * sizeof(JSValue);
                ASSERT(newSize > oldSize);

                if (reallocating) {
                    // Handle the case where we are reallocating (i.e. the old structure/butterfly
                    // already had out-of-line property storage).

                    jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR3);

                    // We have scratchGPR = new storage, scratchGPR3 = old storage,
                    // scratchGPR2 = available
                    for (size_t offset = 0; offset < oldSize; offset += sizeof(void*)) {
                        jit.loadPtr(
                            CCallHelpers::Address(
                                scratchGPR3,
                                -static_cast<ptrdiff_t>(
                                    offset + sizeof(JSValue) + sizeof(void*))),
                            scratchGPR2);
                        jit.storePtr(
                            scratchGPR2,
                            CCallHelpers::Address(
                                scratchGPR,
                                -static_cast<ptrdiff_t>(offset + sizeof(JSValue) + sizeof(void*))));
                    }
                }

                for (size_t offset = oldSize; offset < newSize; offset += sizeof(void*))
                    jit.storePtr(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::Address(scratchGPR, -static_cast<ptrdiff_t>(offset + sizeof(JSValue) + sizeof(void*))));
            } else {
                // Handle the case where we are allocating out-of-line using an operation.
                RegisterSet extraRegistersToPreserve;
                extraRegistersToPreserve.add(baseGPR, IgnoreVectors);
                extraRegistersToPreserve.add(valueRegs, IgnoreVectors);
                InlineCacheCompiler::SpillState spillState = preserveLiveRegistersToStackForCall(extraRegistersToPreserve);

                if (m_stubInfo.useDataIC) {
                    callSiteIndexForExceptionHandlingOrOriginal();
                    jit.transfer32(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
                } else
                    jit.store32(CCallHelpers::TrustedImm32(callSiteIndexForExceptionHandlingOrOriginal().bits()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

                jit.makeSpaceOnStackForCCall();

                if (!reallocating) {
                    jit.setupArguments<decltype(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity)>(CCallHelpers::TrustedImmPtr(&vm), baseGPR);
                    jit.prepareCallOperation(vm);
                    jit.callOperation<OperationPtrTag>(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity);
                } else {
                    // Handle the case where we are reallocating (i.e. the old structure/butterfly
                    // already had out-of-line property storage).
                    jit.setupArguments<decltype(operationReallocateButterflyToGrowPropertyStorage)>(CCallHelpers::TrustedImmPtr(&vm), baseGPR, CCallHelpers::TrustedImm32(newSize / sizeof(JSValue)));
                    jit.prepareCallOperation(vm);
                    jit.callOperation<OperationPtrTag>(operationReallocateButterflyToGrowPropertyStorage);
                }

                jit.reclaimSpaceOnStackForCCall();
                jit.move(GPRInfo::returnValueGPR, scratchGPR);

                CCallHelpers::Jump noException = jit.emitExceptionCheck(vm, CCallHelpers::InvertedExceptionCheck);

                restoreLiveRegistersFromStackForCallWithThrownException(spillState);
                emitExplicitExceptionHandler();

                noException.link(&jit);
                RegisterSet resultRegisterToExclude;
                resultRegisterToExclude.add(scratchGPR, IgnoreVectors);
                restoreLiveRegistersFromStackForCall(spillState, resultRegisterToExclude);
            }
        }

        if (isInlineOffset(accessCase.m_offset)) {
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    baseGPR,
                    JSObject::offsetOfInlineStorage() +
                    offsetInInlineStorage(accessCase.m_offset) * sizeof(JSValue)));
        } else {
            if (!allocating)
                jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(scratchGPR, offsetInButterfly(accessCase.m_offset) * sizeof(JSValue)));
        }

        if (allocatingInline) {
            // If we were to have any indexed properties, then we would need to update the indexing mask on the base object.
            RELEASE_ASSERT(!accessCase.newStructure()->couldHaveIndexingHeader());
            // We set the new butterfly and the structure last. Doing it this way ensures that
            // whatever we had done up to this point is forgotten if we choose to branch to slow
            // path.
            jit.nukeStructureAndStoreButterfly(vm, scratchGPR, baseGPR);
        }

        uint32_t structureBits = std::bit_cast<uint32_t>(accessCase.newStructure()->id());
        jit.store32(
            CCallHelpers::TrustedImm32(structureBits),
            CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()));

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        // We will have a slow path if we were allocating without the help of an operation.
        if (allocatingInline) {
            if (allocator.didReuseRegisters()) {
                slowPath.link(&jit);
                allocator.restoreReusedRegistersByPopping(jit, preservedState);
                m_failAndIgnore.append(jit.jump());
            } else
                m_failAndIgnore.append(slowPath);
        } else
            RELEASE_ASSERT(slowPath.empty());
        return;
    }

    case AccessCase::Delete: {
        ASSERT(accessCase.structure()->transitionWatchpointSetHasBeenInvalidated());
        ASSERT(accessCase.newStructure()->transitionKind() == TransitionKind::PropertyDeletion);
        ASSERT(baseGPR != scratchGPR);

        if (isInlineOffset(accessCase.m_offset))
            jit.storeTrustedValue(JSValue(), CCallHelpers::Address(baseGPR, JSObject::offsetOfInlineStorage() + offsetInInlineStorage(accessCase.m_offset) * sizeof(JSValue)));
        else {
            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            jit.storeTrustedValue(JSValue(), CCallHelpers::Address(scratchGPR, offsetInButterfly(accessCase.m_offset) * sizeof(JSValue)));
        }

        uint32_t structureBits = std::bit_cast<uint32_t>(accessCase.newStructure()->id());
        jit.store32(
            CCallHelpers::TrustedImm32(structureBits),
            CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()));

        jit.move(MacroAssembler::TrustedImm32(true), valueRegs.payloadGPR());

        succeed();
        return;
    }

    case AccessCase::SetPrivateBrand: {
        ASSERT(accessCase.structure()->transitionWatchpointSetHasBeenInvalidated());
        ASSERT(accessCase.newStructure()->transitionKind() == TransitionKind::SetBrand);

        uint32_t structureBits = std::bit_cast<uint32_t>(accessCase.newStructure()->id());
        jit.store32(
            CCallHelpers::TrustedImm32(structureBits),
            CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()));

        succeed();
        return;
    }

    case AccessCase::DeleteNonConfigurable: {
        jit.move(MacroAssembler::TrustedImm32(false), valueRegs.payloadGPR());
        succeed();
        return;
    }

    case AccessCase::DeleteMiss: {
        jit.move(MacroAssembler::TrustedImm32(true), valueRegs.payloadGPR());
        succeed();
        return;
    }

    case AccessCase::ArrayLength: {
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, ArrayStorage::lengthOffset()), scratchGPR);
        m_failAndIgnore.append(
            jit.branch32(CCallHelpers::LessThan, scratchGPR, CCallHelpers::TrustedImm32(0)));
        jit.boxInt32(scratchGPR, valueRegs);
        succeed();
        return;
    }

    case AccessCase::StringLength: {
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSString::offsetOfValue()), scratchGPR);
        auto isRope = jit.branchIfRopeStringImpl(scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, StringImpl::lengthMemoryOffset()), valueRegs.payloadGPR());
        auto done = jit.jump();

        isRope.link(&jit);
        jit.load32(CCallHelpers::Address(baseGPR, JSRopeString::offsetOfLength()), valueRegs.payloadGPR());

        done.link(&jit);
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        succeed();
        return;
    }

    case AccessCase::IndexedNoIndexingMiss:
        jit.moveTrustedValue(jsUndefined(), valueRegs);
        succeed();
        return;

    case AccessCase::IndexedNoIndexingInMiss:
        jit.moveTrustedValue(jsBoolean(false), valueRegs);
        succeed();
        return;

    case AccessCase::DirectArgumentsLength:
    case AccessCase::ScopedArgumentsLength:
    case AccessCase::ModuleNamespaceLoad:
    case AccessCase::ProxyObjectIn:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::InstanceOfMegamorphic:
    case AccessCase::LoadMegamorphic:
    case AccessCase::StoreMegamorphic:
    case AccessCase::InMegamorphic:
    case AccessCase::IndexedProxyObjectIn:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::IndexedProxyObjectStore:
    case AccessCase::IndexedMegamorphicIn:
    case AccessCase::IndexedMegamorphicLoad:
    case AccessCase::IndexedMegamorphicStore:
    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayFloat16Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat16Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedStringLoad:
    case AccessCase::CheckPrivateBrand:
    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat16Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat16Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedDoubleInHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit:
    case AccessCase::IndexedScopedArgumentsInHit:
    case AccessCase::IndexedDirectArgumentsInHit:
    case AccessCase::IndexedTypedArrayInt8In:
    case AccessCase::IndexedTypedArrayUint8In:
    case AccessCase::IndexedTypedArrayUint8ClampedIn:
    case AccessCase::IndexedTypedArrayInt16In:
    case AccessCase::IndexedTypedArrayUint16In:
    case AccessCase::IndexedTypedArrayInt32In:
    case AccessCase::IndexedTypedArrayUint32In:
    case AccessCase::IndexedTypedArrayFloat16In:
    case AccessCase::IndexedTypedArrayFloat32In:
    case AccessCase::IndexedTypedArrayFloat64In:
    case AccessCase::IndexedResizableTypedArrayInt8In:
    case AccessCase::IndexedResizableTypedArrayUint8In:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedIn:
    case AccessCase::IndexedResizableTypedArrayInt16In:
    case AccessCase::IndexedResizableTypedArrayUint16In:
    case AccessCase::IndexedResizableTypedArrayInt32In:
    case AccessCase::IndexedResizableTypedArrayUint32In:
    case AccessCase::IndexedResizableTypedArrayFloat16In:
    case AccessCase::IndexedResizableTypedArrayFloat32In:
    case AccessCase::IndexedResizableTypedArrayFloat64In:
    case AccessCase::IndexedStringInHit:
        // These need to be handled by generateWithGuard(), since the guard is part of the
        // algorithm. We can be sure that nobody will call generate() directly for these since they
        // are not guarded by structure checks.
        RELEASE_ASSERT_NOT_REACHED();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void InlineCacheCompiler::emitDOMJITGetter(JSGlobalObject* globalObjectForDOMJIT, const DOMJIT::GetterSetter* domJIT, GPRReg baseForGetGPR)
{
    CCallHelpers& jit = *m_jit;
    JSValueRegs valueRegs = m_stubInfo.valueRegs();
    GPRReg baseGPR = m_stubInfo.m_baseGPR;
    GPRReg scratchGPR = m_scratchGPR;

    if (m_stubInfo.useDataIC) {
        callSiteIndexForExceptionHandlingOrOriginal();
        jit.transfer32(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
    } else
        jit.store32(CCallHelpers::TrustedImm32(callSiteIndexForExceptionHandlingOrOriginal().bits()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    // We construct the environment that can execute the DOMJIT::Snippet here.
    Ref<DOMJIT::CallDOMGetterSnippet> snippet = domJIT->compiler()();

    Vector<GPRReg> gpScratch;
    Vector<FPRReg> fpScratch;
    Vector<SnippetParams::Value> regs;

    auto allocator = makeDefaultScratchAllocator(scratchGPR);

    GPRReg paramBaseGPR = InvalidGPRReg;
    GPRReg paramGlobalObjectGPR = InvalidGPRReg;
    JSValueRegs paramValueRegs = valueRegs;
    GPRReg remainingScratchGPR = InvalidGPRReg;

    // valueRegs and baseForGetGPR may be the same. For example, in Baseline JIT, we pass the same regT0 for baseGPR and valueRegs.
    // In FTL, there is no constraint that the baseForGetGPR interferes with the result. To make implementation simple in
    // Snippet, Snippet assumes that result registers always early interfere with input registers, in this case,
    // baseForGetGPR. So we move baseForGetGPR to the other register if baseForGetGPR == valueRegs.
    if (baseForGetGPR != valueRegs.payloadGPR()) {
        paramBaseGPR = baseForGetGPR;
        if (!snippet->requireGlobalObject)
            remainingScratchGPR = scratchGPR;
        else
            paramGlobalObjectGPR = scratchGPR;
    } else {
        jit.move(valueRegs.payloadGPR(), scratchGPR);
        paramBaseGPR = scratchGPR;
        if (snippet->requireGlobalObject)
            paramGlobalObjectGPR = allocator.allocateScratchGPR();
    }

    regs.append(paramValueRegs);
    regs.append(paramBaseGPR);
    if (snippet->requireGlobalObject) {
        ASSERT(paramGlobalObjectGPR != InvalidGPRReg);
        if (!globalObjectForDOMJIT) {
            ASSERT(useHandlerIC());
            regs.append(SnippetParams::Value(paramGlobalObjectGPR));
        } else
            regs.append(SnippetParams::Value(paramGlobalObjectGPR, globalObjectForDOMJIT));
    }

    if (snippet->numGPScratchRegisters) {
        unsigned i = 0;
        if (remainingScratchGPR != InvalidGPRReg) {
            gpScratch.append(remainingScratchGPR);
            ++i;
        }
        for (; i < snippet->numGPScratchRegisters; ++i)
            gpScratch.append(allocator.allocateScratchGPR());
    }

    for (unsigned i = 0; i < snippet->numFPScratchRegisters; ++i)
        fpScratch.append(allocator.allocateScratchFPR());

    // Let's store the reused registers to the stack. After that, we can use allocated scratch registers.
    ScratchRegisterAllocator::PreservedState preservedState =
        allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);

    if (InlineCacheCompilerInternal::verbose) {
        dataLog("baseGPR = ", baseGPR, "\n");
        dataLog("valueRegs = ", valueRegs, "\n");
        dataLog("scratchGPR = ", scratchGPR, "\n");
        dataLog("paramBaseGPR = ", paramBaseGPR, "\n");
        if (paramGlobalObjectGPR != InvalidGPRReg)
            dataLog("paramGlobalObjectGPR = ", paramGlobalObjectGPR, "\n");
        dataLog("paramValueRegs = ", paramValueRegs, "\n");
        for (unsigned i = 0; i < snippet->numGPScratchRegisters; ++i)
            dataLog("gpScratch[", i, "] = ", gpScratch[i], "\n");
    }

    if (snippet->requireGlobalObject) {
        if (!globalObjectForDOMJIT) {
            ASSERT(useHandlerIC());
            jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfGlobalObject()), paramGlobalObjectGPR);
        } else
            jit.move(CCallHelpers::TrustedImmPtr(globalObjectForDOMJIT), paramGlobalObjectGPR);
    }

    // We just spill the registers used in Snippet here. For not spilled registers here explicitly,
    // they must be in the used register set passed by the callers (Baseline, DFG, and FTL) if they need to be kept.
    // Some registers can be locked, but not in the used register set. For example, the caller could make baseGPR
    // same to valueRegs, and not include it in the used registers since it will be changed.
    RegisterSetBuilder usedRegisters;
    for (auto& value : regs) {
        SnippetReg reg = value.reg();
        if (reg.isJSValueRegs())
            usedRegisters.add(reg.jsValueRegs(), IgnoreVectors);
        else if (reg.isGPR())
            usedRegisters.add(reg.gpr(), IgnoreVectors);
        else
            usedRegisters.add(reg.fpr(), IgnoreVectors);
    }
    for (GPRReg reg : gpScratch)
        usedRegisters.add(reg, IgnoreVectors);
    for (FPRReg reg : fpScratch)
        usedRegisters.add(reg, IgnoreVectors);
    if (m_stubInfo.useDataIC)
        usedRegisters.add(m_stubInfo.m_stubInfoGPR, IgnoreVectors);
    auto registersToSpillForCCall = RegisterSetBuilder::registersToSaveForCCall(usedRegisters);

    AccessCaseSnippetParams params(m_vm, WTFMove(regs), WTFMove(gpScratch), WTFMove(fpScratch));
    snippet->generator()->run(jit, params);
    allocator.restoreReusedRegistersByPopping(jit, preservedState);
    succeed();

    CCallHelpers::JumpList exceptions = params.emitSlowPathCalls(*this, registersToSpillForCCall, jit);
    if (!exceptions.empty()) {
        exceptions.link(&jit);
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        emitExplicitExceptionHandler();
    }
}

void InlineCacheCompiler::emitModuleNamespaceLoad(ModuleNamespaceAccessCase& accessCase, MacroAssembler::JumpList& fallThrough)
{
    CCallHelpers& jit = *m_jit;
    JSValueRegs valueRegs = m_stubInfo.valueRegs();
    GPRReg baseGPR = m_stubInfo.m_baseGPR;
    GPRReg scratchGPR = m_scratchGPR;

    fallThrough.append(
        jit.branchPtr(
            CCallHelpers::NotEqual,
            baseGPR,
            CCallHelpers::TrustedImmPtr(accessCase.moduleNamespaceObject())));

#if USE(JSVALUE64)
    jit.loadValue(&accessCase.moduleEnvironment()->variableAt(accessCase.scopeOffset()), JSValueRegs { scratchGPR });
    m_failAndIgnore.append(jit.branchIfEmpty(JSValueRegs { scratchGPR }));
    jit.moveValueRegs(JSValueRegs { scratchGPR }, valueRegs);
#else
    jit.load32(std::bit_cast<uint8_t*>(&accessCase.moduleEnvironment()->variableAt(accessCase.scopeOffset())) + TagOffset, scratchGPR);
    m_failAndIgnore.append(jit.branchIfEmpty(scratchGPR));
    jit.loadValue(&accessCase.moduleEnvironment()->variableAt(accessCase.scopeOffset()), valueRegs);
#endif
    succeed();
}

void InlineCacheCompiler::emitProxyObjectAccess(unsigned index, AccessCase& accessCase, MacroAssembler::JumpList& fallThrough)
{
    CCallHelpers& jit = *m_jit;
    ECMAMode ecmaMode = m_ecmaMode;
    JSValueRegs valueRegs = m_stubInfo.valueRegs();
    GPRReg baseGPR = m_stubInfo.m_baseGPR;
    GPRReg scratchGPR = m_scratchGPR;
    GPRReg thisGPR = m_stubInfo.thisValueIsInExtraGPR() ? m_stubInfo.thisGPR() : baseGPR;

    jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
    fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(ProxyObjectType)));

    InlineCacheCompiler::SpillState spillState = preserveLiveRegistersToStackForCall();

    if (m_stubInfo.useDataIC) {
        callSiteIndexForExceptionHandlingOrOriginal();
        jit.transfer32(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
    } else
        jit.store32(CCallHelpers::TrustedImm32(callSiteIndexForExceptionHandlingOrOriginal().bits()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    setSpillStateForJSCall(spillState);

    unsigned numberOfParameters;

    switch (accessCase.m_type) {
    case AccessCase::ProxyObjectIn:
    case AccessCase::IndexedProxyObjectIn:
        numberOfParameters = 2;
        break;
    case AccessCase::ProxyObjectLoad:
    case AccessCase::IndexedProxyObjectLoad:
        numberOfParameters = 3;
        break;
    case AccessCase::ProxyObjectStore:
    case AccessCase::IndexedProxyObjectStore:
        numberOfParameters = 4;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
    ASSERT(!(numberOfRegsForCall % stackAlignmentRegisters()));
    unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);

    unsigned alignedNumberOfBytesForCall = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(numberOfBytesForCall);

    jit.subPtr(
        CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall),
        CCallHelpers::stackPointerRegister);

    CCallHelpers::Address calleeFrame = CCallHelpers::Address(CCallHelpers::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

    jit.store32(CCallHelpers::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + PayloadOffset));

    jit.storeCell(baseGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(0).offset() * sizeof(Register)));

    if (!hasConstantIdentifier(m_stubInfo.accessType))
        jit.storeValue(m_stubInfo.propertyRegs(), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(1).offset() * sizeof(Register)));
    else
        jit.storeTrustedValue(accessCase.identifier().cell(), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(1).offset() * sizeof(Register)));

    switch (accessCase.m_type) {
    case AccessCase::ProxyObjectIn:
    case AccessCase::IndexedProxyObjectIn:
        break;
    case AccessCase::ProxyObjectLoad:
    case AccessCase::IndexedProxyObjectLoad:
        jit.storeCell(thisGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(2).offset() * sizeof(Register)));
        break;
    case AccessCase::ProxyObjectStore:
    case AccessCase::IndexedProxyObjectStore:
        jit.storeCell(thisGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(2).offset() * sizeof(Register)));
        jit.storeValue(valueRegs, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(3).offset() * sizeof(Register)));
        break;
    default:
        break;
    }

    switch (accessCase.m_type) {
    case AccessCase::ProxyObjectIn: {
        if (useHandlerIC()) {
            jit.loadPtr(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfGlobalObject()), scratchGPR);
            jit.loadPtr(CCallHelpers::Address(scratchGPR, JSGlobalObject::offsetOfPerformProxyObjectHasFunction()), scratchGPR);
        } else
            jit.move(CCallHelpers::TrustedImmPtr(m_globalObject->performProxyObjectHasFunction()), scratchGPR);
        break;
    }
    case AccessCase::IndexedProxyObjectIn: {
        if (useHandlerIC()) {
            jit.loadPtr(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfGlobalObject()), scratchGPR);
            jit.loadPtr(CCallHelpers::Address(scratchGPR, JSGlobalObject::offsetOfPerformProxyObjectHasByValFunction()), scratchGPR);
        } else
            jit.move(CCallHelpers::TrustedImmPtr(m_globalObject->performProxyObjectHasByValFunction()), scratchGPR);
        break;
    }
    case AccessCase::ProxyObjectLoad: {
        if (useHandlerIC()) {
            jit.loadPtr(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfGlobalObject()), scratchGPR);
            jit.loadPtr(CCallHelpers::Address(scratchGPR, JSGlobalObject::offsetOfPerformProxyObjectGetFunction()), scratchGPR);
        } else
            jit.move(CCallHelpers::TrustedImmPtr(m_globalObject->performProxyObjectGetFunction()), scratchGPR);
        break;
    }
    case AccessCase::IndexedProxyObjectLoad: {
        if (useHandlerIC()) {
            jit.loadPtr(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfGlobalObject()), scratchGPR);
            jit.loadPtr(CCallHelpers::Address(scratchGPR, JSGlobalObject::offsetOfPerformProxyObjectGetByValFunction()), scratchGPR);
        } else
            jit.move(CCallHelpers::TrustedImmPtr(m_globalObject->performProxyObjectGetByValFunction()), scratchGPR);
        break;
    }
    case AccessCase::ProxyObjectStore: {
        if (useHandlerIC()) {
            jit.loadPtr(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfGlobalObject()), scratchGPR);
            if (ecmaMode.isStrict())
                jit.loadPtr(CCallHelpers::Address(scratchGPR, JSGlobalObject::offsetOfPerformProxyObjectSetStrictFunction()), scratchGPR);
            else
                jit.loadPtr(CCallHelpers::Address(scratchGPR, JSGlobalObject::offsetOfPerformProxyObjectSetSloppyFunction()), scratchGPR);
        } else
            jit.move(CCallHelpers::TrustedImmPtr(ecmaMode.isStrict() ? m_globalObject->performProxyObjectSetStrictFunction() : m_globalObject->performProxyObjectSetSloppyFunction()), scratchGPR);
        break;
    }
    case AccessCase::IndexedProxyObjectStore: {
        if (useHandlerIC()) {
            jit.loadPtr(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfGlobalObject()), scratchGPR);
            if (ecmaMode.isStrict())
                jit.loadPtr(CCallHelpers::Address(scratchGPR, JSGlobalObject::offsetOfPerformProxyObjectSetByValStrictFunction()), scratchGPR);
            else
                jit.loadPtr(CCallHelpers::Address(scratchGPR, JSGlobalObject::offsetOfPerformProxyObjectSetByValSloppyFunction()), scratchGPR);
        } else
            jit.move(CCallHelpers::TrustedImmPtr(ecmaMode.isStrict() ? m_globalObject->performProxyObjectSetByValStrictFunction() : m_globalObject->performProxyObjectSetByValSloppyFunction()), scratchGPR);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    jit.storeCell(scratchGPR, calleeFrame.withOffset(CallFrameSlot::callee * sizeof(Register)));

    if (useHandlerIC()) {
        // handlerGPR can be the same to BaselineJITRegisters::Call::calleeJSR.
        if constexpr (GPRInfo::handlerGPR == BaselineJITRegisters::Call::calleeJSR.payloadGPR()) {
            jit.swap(scratchGPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
            jit.addPtr(CCallHelpers::TrustedImm32(InlineCacheHandler::offsetOfCallLinkInfos() + sizeof(DataOnlyCallLinkInfo) * index), scratchGPR, BaselineJITRegisters::Call::callLinkInfoGPR);
        } else {
            jit.move(scratchGPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
            jit.addPtr(CCallHelpers::TrustedImm32(InlineCacheHandler::offsetOfCallLinkInfos() + sizeof(DataOnlyCallLinkInfo) * index), GPRInfo::handlerGPR, BaselineJITRegisters::Call::callLinkInfoGPR);
        }

        CallLinkInfo::emitDataICFastPath(jit);
    } else {
        jit.move(scratchGPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
#if USE(JSVALUE32_64)
        // We *always* know that the proxy function, if non-null, is a cell.
        jit.move(CCallHelpers::TrustedImm32(JSValue::CellTag), BaselineJITRegisters::Call::calleeJSR.tagGPR());
#endif
#if CPU(ARM_THUMB2)
        // ARMv7 clobbers metadataTable register. Thus we need to restore them back here.
        JIT::emitMaterializeMetadataAndConstantPoolRegisters(jit);
#endif
        m_callLinkInfos[index] = makeUnique<OptimizingCallLinkInfo>(m_stubInfo.codeOrigin, nullptr);
        auto* callLinkInfo = m_callLinkInfos[index].get();
        callLinkInfo->setUpCall(CallLinkInfo::Call);
        CallLinkInfo::emitFastPath(jit, callLinkInfo);
    }

    if (accessCase.m_type != AccessCase::ProxyObjectStore && accessCase.m_type != AccessCase::IndexedProxyObjectStore)
        jit.setupResults(valueRegs);


    if (m_stubInfo.useDataIC) {
        jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), m_scratchGPR);
        if (useHandlerIC())
            jit.addPtr(CCallHelpers::TrustedImm32(-(sizeof(CallerFrameAndPC) + maxFrameExtentForSlowPathCall + m_preservedReusedRegisterState.numberOfBytesPreserved + spillState.numberOfStackBytesUsedForRegisterPreservation)), m_scratchGPR);
        else
            jit.addPtr(CCallHelpers::TrustedImm32(-(m_preservedReusedRegisterState.numberOfBytesPreserved + spillState.numberOfStackBytesUsedForRegisterPreservation)), m_scratchGPR);
        jit.addPtr(m_scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
    } else {
        int stackPointerOffset = (jit.codeBlock()->stackPointerOffset() * sizeof(Register)) - m_preservedReusedRegisterState.numberOfBytesPreserved - spillState.numberOfStackBytesUsedForRegisterPreservation;
        jit.addPtr(CCallHelpers::TrustedImm32(stackPointerOffset), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
    }

    RegisterSet dontRestore;
    if (accessCase.m_type != AccessCase::ProxyObjectStore && accessCase.m_type != AccessCase::IndexedProxyObjectStore) {
        // This is the result value. We don't want to overwrite the result with what we stored to the stack.
        // We sometimes have to store it to the stack just in case we throw an exception and need the original value.
        dontRestore.add(valueRegs, IgnoreVectors);
    }
    restoreLiveRegistersFromStackForCall(spillState, dontRestore);
    succeed();
}

bool InlineCacheCompiler::canEmitIntrinsicGetter(StructureStubInfo& stubInfo, JSFunction* getter, Structure* structure)
{
    // We aren't structure checking the this value, so we don't know:
    // - For type array loads, that it's a typed array.
    // - For __proto__ getter, that the incoming value is an object,
    //   and if it overrides getPrototype structure flags.
    // So for these cases, it's simpler to just call the getter directly.
    if (stubInfo.thisValueIsInExtraGPR())
        return false;

    switch (getter->intrinsic()) {
    case TypedArrayByteOffsetIntrinsic:
    case TypedArrayByteLengthIntrinsic:
    case TypedArrayLengthIntrinsic: {
        if (!isTypedView(structure->typeInfo().type()))
            return false;
#if USE(JSVALUE32_64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(structure->classInfoForCells()))
            return false;
#endif
        return true;
    }
    case UnderscoreProtoIntrinsic: {
        TypeInfo info = structure->typeInfo();
        return info.isObject() && !info.overridesGetPrototype();
    }
    case SpeciesGetterIntrinsic: {
#if USE(JSVALUE32_64)
        return false;
#else
        return !structure->classInfoForCells()->isSubClassOf(JSScope::info());
#endif
    }
    case WebAssemblyInstanceExportsIntrinsic:
        return structure->typeInfo().type() == WebAssemblyInstanceType;
    default:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void InlineCacheCompiler::emitIntrinsicGetter(IntrinsicGetterAccessCase& accessCase)
{
    CCallHelpers& jit = *m_jit;
    JSValueRegs valueRegs = m_stubInfo.valueRegs();
    GPRReg baseGPR = m_stubInfo.m_baseGPR;
    GPRReg valueGPR = valueRegs.payloadGPR();

    switch (accessCase.intrinsic()) {
    case TypedArrayLengthIntrinsic: {
#if USE(JSVALUE64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(accessCase.structure()->classInfoForCells())) {
            auto allocator = makeDefaultScratchAllocator(m_scratchGPR);
            GPRReg scratch2GPR = allocator.allocateScratchGPR();

            ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

            jit.loadTypedArrayLength(baseGPR, valueGPR, m_scratchGPR, scratch2GPR, typedArrayType(accessCase.structure()->typeInfo().type()));
#if USE(LARGE_TYPED_ARRAYS)
            jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
            jit.boxInt32(valueGPR, valueRegs);
#endif
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            succeed();
            return;
        }
#endif

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
        jit.load32(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        jit.boxInt32(valueGPR, valueRegs);
#endif
        succeed();
        return;
    }

    case TypedArrayByteLengthIntrinsic: {
        TypedArrayType type = typedArrayType(accessCase.structure()->typeInfo().type());

#if USE(JSVALUE64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(accessCase.structure()->classInfoForCells())) {
            auto allocator = makeDefaultScratchAllocator(m_scratchGPR);
            GPRReg scratch2GPR = allocator.allocateScratchGPR();

            ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

            jit.loadTypedArrayByteLength(baseGPR, valueGPR, m_scratchGPR, scratch2GPR, typedArrayType(accessCase.structure()->typeInfo().type()));
#if USE(LARGE_TYPED_ARRAYS)
            jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
            jit.boxInt32(valueGPR, valueRegs);
#endif
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            succeed();
            return;
        }
#endif

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        if (elementSize(type) > 1)
            jit.lshift64(CCallHelpers::TrustedImm32(logElementSize(type)), valueGPR);
        jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
        jit.load32(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        if (elementSize(type) > 1) {
            // We can use a bitshift here since on ADDRESS32 platforms TypedArrays cannot have byteLength that overflows an int32.
            jit.lshift32(CCallHelpers::TrustedImm32(logElementSize(type)), valueGPR);
        }
        jit.boxInt32(valueGPR, valueRegs);
#endif
        succeed();
        return;
    }

    case TypedArrayByteOffsetIntrinsic: {
#if USE(JSVALUE64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(accessCase.structure()->classInfoForCells())) {
            auto allocator = makeDefaultScratchAllocator(m_scratchGPR);
            GPRReg scratch2GPR = allocator.allocateScratchGPR();

            ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
            auto outOfBounds = jit.branchIfResizableOrGrowableSharedTypedArrayIsOutOfBounds(baseGPR, m_scratchGPR, scratch2GPR, typedArrayType(accessCase.structure()->typeInfo().type()));
#if USE(LARGE_TYPED_ARRAYS)
            jit.load64(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
#else
            jit.load32(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
#endif
            auto done = jit.jump();

            outOfBounds.link(&jit);
            jit.move(CCallHelpers::TrustedImm32(0), valueGPR);

            done.link(&jit);
#if USE(LARGE_TYPED_ARRAYS)
            jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
            jit.boxInt32(valueGPR, valueRegs);
#endif
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            succeed();
            return;
        }
#endif

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
        jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
        jit.load32(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
        jit.boxInt32(valueGPR, valueRegs);
#endif
        succeed();
        return;
    }

    case UnderscoreProtoIntrinsic: {
        if (accessCase.structure()->hasPolyProto())
            jit.loadValue(CCallHelpers::Address(baseGPR, offsetRelativeToBase(knownPolyProtoOffset)), valueRegs);
        else
            jit.moveValue(accessCase.structure()->storedPrototype(), valueRegs);
        succeed();
        return;
    }

    case SpeciesGetterIntrinsic: {
        jit.moveValueRegs(m_stubInfo.baseRegs(), valueRegs);
        succeed();
        return;
    }

    case WebAssemblyInstanceExportsIntrinsic: {
#if ENABLE(WEBASSEMBLY)
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSWebAssemblyInstance::offsetOfModuleRecord()), valueGPR);
        jit.loadPtr(CCallHelpers::Address(valueGPR, WebAssemblyModuleRecord::offsetOfExportsObject()), valueGPR);
        jit.boxCell(valueGPR, valueRegs);
        succeed();
#endif
        return;
    }

    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static inline bool canUseMegamorphicPutFastPath(Structure* structure)
{
    while (true) {
        if (structure->hasReadOnlyOrGetterSetterPropertiesExcludingProto() || structure->typeInfo().overridesGetPrototype() || structure->typeInfo().overridesPut() || structure->hasPolyProto())
            return false;
        JSValue prototype = structure->storedPrototype();
        if (prototype.isNull())
            return true;
        structure = asObject(prototype)->structure();
    }
}

static inline ASCIILiteral categoryName(AccessType type)
{
    switch (type) {
#define JSC_DEFINE_ACCESS_TYPE_CASE(name) \
    case AccessType::name: \
        return #name ""_s; \

        JSC_FOR_EACH_STRUCTURE_STUB_INFO_ACCESS_TYPE(JSC_DEFINE_ACCESS_TYPE_CASE)

#undef JSC_DEFINE_ACCESS_TYPE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

static Vector<WatchpointSet*, 3> collectAdditionalWatchpoints(VM& vm, AccessCase& accessCase)
{
    // It's fine to commit something that is already committed. That arises when we switch to using
    // newly allocated watchpoints. When it happens, it's not efficient - but we think that's OK
    // because most AccessCases have no extra watchpoints anyway.

    Vector<WatchpointSet*, 3> result;
    Structure* structure = accessCase.structure();

    if (accessCase.identifier()) {
        if ((structure && structure->needImpurePropertyWatchpoint())
            || accessCase.conditionSet().needImpurePropertyWatchpoint()
            || (accessCase.polyProtoAccessChain() && accessCase.polyProtoAccessChain()->needImpurePropertyWatchpoint(vm)))
            result.append(vm.ensureWatchpointSetForImpureProperty(accessCase.identifier().uid()));
    }

    if (WatchpointSet* set  = accessCase.additionalSet())
        result.append(set);

    if (structure
        && structure->hasRareData()
        && structure->rareData()->hasSharedPolyProtoWatchpoint()
        && structure->rareData()->sharedPolyProtoWatchpoint()->isStillValid()) {
        WatchpointSet* set = structure->rareData()->sharedPolyProtoWatchpoint()->inflate();
        result.append(set);
    }

    return result;
}

static std::variant<StructureTransitionStructureStubClearingWatchpoint, AdaptiveValueStructureStubClearingWatchpoint>& addWatchpoint(PolymorphicAccessJITStubRoutine::Watchpoints& watchpoints, const ObjectPropertyCondition& key, WatchpointSet& watchpointSet)
{
    if (!key || key.condition().kind() != PropertyCondition::Equivalence)
        return *watchpoints.add(std::in_place_type<StructureTransitionStructureStubClearingWatchpoint>, key, watchpointSet);
    ASSERT(key.condition().kind() == PropertyCondition::Equivalence);
    return *watchpoints.add(std::in_place_type<AdaptiveValueStructureStubClearingWatchpoint>, key, watchpointSet);
}

static void ensureReferenceAndInstallWatchpoint(VM& vm, PolymorphicAccessJITStubRoutine::Watchpoints& watchpoints, WatchpointSet& watchpointSet, const ObjectPropertyCondition& key)
{
    ASSERT(!!key);
    auto& watchpointVariant = addWatchpoint(watchpoints, key, watchpointSet);
    if (key.kind() == PropertyCondition::Equivalence) {
        auto& adaptiveWatchpoint = std::get<AdaptiveValueStructureStubClearingWatchpoint>(watchpointVariant);
        adaptiveWatchpoint.install(vm);
    } else {
        auto* structureTransitionWatchpoint = &std::get<StructureTransitionStructureStubClearingWatchpoint>(watchpointVariant);
        key.object()->structure()->addTransitionWatchpoint(structureTransitionWatchpoint);
    }
}

static void ensureReferenceAndAddWatchpoint(VM&, PolymorphicAccessJITStubRoutine::Watchpoints& watchpoints, WatchpointSet& watchpointSet, WatchpointSet& additionalWatchpointSet)
{
    auto* watchpoint = &std::get<StructureTransitionStructureStubClearingWatchpoint>(addWatchpoint(watchpoints, ObjectPropertyCondition(), watchpointSet));
    additionalWatchpointSet.add(watchpoint);
}

template<typename Container>
RefPtr<AccessCase> InlineCacheCompiler::tryFoldToMegamorphic(CodeBlock* codeBlock, const Container& cases)
{
    // Accidentally, it already includes megamorphic case. Then we just return it.
    for (auto accessCase : cases) {
        if (isMegamorphic(accessCase->m_type))
            return accessCase;
    }

    // If the resulting set of cases is so big that we would stop caching and this is InstanceOf,
    // then we want to generate the generic InstanceOf and then stop.
    if (cases.size() >= Options::maxAccessVariantListSize() || m_stubInfo.canBeMegamorphic) {
        switch (m_stubInfo.accessType) {
        case AccessType::InstanceOf:
            return AccessCase::create(vm(), codeBlock, AccessCase::InstanceOfMegamorphic, nullptr);

        case AccessType::GetById:
        case AccessType::GetByIdWithThis: {
            auto identifier = m_stubInfo.m_identifier;
            bool allAreSimpleLoadOrMiss = true;
            for (auto& accessCase : cases) {
                if (accessCase->type() != AccessCase::Load && accessCase->type() != AccessCase::Miss) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
                if (accessCase->usesPolyProto()) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
                if (accessCase->viaGlobalProxy()) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
            }

            // Currently, we do not apply megamorphic cache for "length" property since Array#length and String#length are too common.
            if (!canUseMegamorphicGetById(vm(), identifier.uid()))
                allAreSimpleLoadOrMiss = false;

#if USE(JSVALUE32_64)
            allAreSimpleLoadOrMiss = false;
#endif

            if (allAreSimpleLoadOrMiss)
                return AccessCase::create(vm(), codeBlock, AccessCase::LoadMegamorphic, useHandlerIC() ? nullptr : identifier);
            return nullptr;
        }
        case AccessType::GetByVal:
        case AccessType::GetByValWithThis: {
            bool allAreSimpleLoadOrMiss = true;
            for (auto& accessCase : cases) {
                if (accessCase->type() != AccessCase::Load && accessCase->type() != AccessCase::Miss) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
                if (accessCase->usesPolyProto()) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
                if (accessCase->viaGlobalProxy()) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
            }

#if USE(JSVALUE32_64)
            allAreSimpleLoadOrMiss = false;
#endif

            if (allAreSimpleLoadOrMiss)
                return AccessCase::create(vm(), codeBlock, AccessCase::IndexedMegamorphicLoad, nullptr);
            return nullptr;
        }
        case AccessType::PutByIdStrict:
        case AccessType::PutByIdSloppy: {
            auto identifier = m_stubInfo.m_identifier;
            bool allAreSimpleReplaceOrTransition = true;
            for (auto& accessCase : cases) {
                if (accessCase->type() != AccessCase::Replace && accessCase->type() != AccessCase::Transition) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (accessCase->usesPolyProto()) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (accessCase->viaGlobalProxy()) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (!canUseMegamorphicPutFastPath(accessCase->structure())) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
            }

            // Currently, we do not apply megamorphic cache for "length" property since Array#length and String#length are too common.
            if (!canUseMegamorphicPutById(vm(), identifier.uid()))
                allAreSimpleReplaceOrTransition = false;

#if USE(JSVALUE32_64)
            allAreSimpleReplaceOrTransition = false;
#endif

            if (allAreSimpleReplaceOrTransition)
                return AccessCase::create(vm(), codeBlock, AccessCase::StoreMegamorphic, useHandlerIC() ? nullptr : identifier);
            return nullptr;
        }
        case AccessType::PutByValStrict:
        case AccessType::PutByValSloppy: {
            bool allAreSimpleReplaceOrTransition = true;
            for (auto& accessCase : cases) {
                if (accessCase->type() != AccessCase::Replace && accessCase->type() != AccessCase::Transition) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (accessCase->usesPolyProto()) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (accessCase->viaGlobalProxy()) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (!canUseMegamorphicPutFastPath(accessCase->structure())) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
            }

#if USE(JSVALUE32_64)
            allAreSimpleReplaceOrTransition = false;
#endif

            if (allAreSimpleReplaceOrTransition)
                return AccessCase::create(vm(), codeBlock, AccessCase::IndexedMegamorphicStore, nullptr);
            return nullptr;
        }
        case AccessType::InById: {
            auto identifier = m_stubInfo.m_identifier;
            bool allAreSimpleHitOrMiss = true;
            for (auto& accessCase : cases) {
                if (accessCase->type() != AccessCase::InHit && accessCase->type() != AccessCase::InMiss) {
                    allAreSimpleHitOrMiss = false;
                    break;
                }
                if (accessCase->usesPolyProto()) {
                    allAreSimpleHitOrMiss = false;
                    break;
                }
                if (accessCase->viaGlobalProxy()) {
                    allAreSimpleHitOrMiss = false;
                    break;
                }
            }

            // Currently, we do not apply megamorphic cache for "length" property since Array#length and String#length are too common.
            if (!canUseMegamorphicInById(vm(), identifier.uid()))
                allAreSimpleHitOrMiss = false;

#if USE(JSVALUE32_64)
            allAreSimpleHitOrMiss = false;
#endif

            if (allAreSimpleHitOrMiss)
                return AccessCase::create(vm(), codeBlock, AccessCase::InMegamorphic, useHandlerIC() ? nullptr : identifier);
            return nullptr;
        }
        case AccessType::InByVal: {
            bool allAreSimpleHitOrMiss = true;
            for (auto& accessCase : cases) {
                if (accessCase->type() != AccessCase::InHit && accessCase->type() != AccessCase::InMiss) {
                    allAreSimpleHitOrMiss = false;
                    break;
                }
                if (accessCase->usesPolyProto()) {
                    allAreSimpleHitOrMiss = false;
                    break;
                }
                if (accessCase->viaGlobalProxy()) {
                    allAreSimpleHitOrMiss = false;
                    break;
                }
            }

#if USE(JSVALUE32_64)
            allAreSimpleHitOrMiss = false;
#endif

            if (allAreSimpleHitOrMiss)
                return AccessCase::create(vm(), codeBlock, AccessCase::IndexedMegamorphicIn, nullptr);
            return nullptr;
        }
        default:
            break;
        }
    }
    return nullptr;
}

AccessGenerationResult InlineCacheCompiler::compile(const GCSafeConcurrentJSLocker&, PolymorphicAccess& poly, CodeBlock* codeBlock)
{
    ASSERT(!useHandlerIC());
    SuperSamplerScope superSamplerScope(false);

    dataLogLnIf(InlineCacheCompilerInternal::verbose, "Regenerate with m_list: ", listDump(poly.m_list));

    // Regenerating is our opportunity to figure out what our list of cases should look like. We
    // do this here. The newly produced 'cases' list may be smaller than m_list. We don't edit
    // m_list in-place because we may still fail, in which case we want the PolymorphicAccess object
    // to be unmutated. For sure, we want it to hang onto any data structures that may be referenced
    // from the code of the current stub (aka previous).
    Vector<WatchpointSet*, 8> additionalWatchpointSets;
    Vector<Ref<AccessCase>, 16> cases;
    cases.reserveInitialCapacity(poly.m_list.size());
    unsigned srcIndex = 0;
    for (auto& someCase : poly.m_list) {
        [&] () {
            if (!someCase->couldStillSucceed())
                return;

            auto sets = collectAdditionalWatchpoints(vm(), someCase.get());
            for (auto* set : sets) {
                if (!set->isStillValid())
                    return;
            }

            // Figure out if this is replaced by any later case. Given two cases A and B where A
            // comes first in the case list, we know that A would have triggered first if we had
            // generated the cases in a cascade. That's why this loop asks B->canReplace(A) but not
            // A->canReplace(B). If A->canReplace(B) was true then A would never have requested
            // repatching in cases where Repatch.cpp would have then gone on to generate B. If that
            // did happen by some fluke, then we'd just miss the redundancy here, which wouldn't be
            // incorrect - just slow. However, if A's checks failed and Repatch.cpp concluded that
            // this new condition could be handled by B and B->canReplace(A), then this says that we
            // don't need A anymore.
            //
            // If we can generate a binary switch, then A->canReplace(B) == B->canReplace(A). So,
            // it doesn't matter that we only do the check in one direction.
            for (unsigned j = srcIndex + 1; j < poly.m_list.size(); ++j) {
                if (poly.m_list[j]->canReplace(someCase.get()))
                    return;
            }

            // If the case had been generated, then we have to keep the original in m_list in case we
            // fail to regenerate. That case may have data structures that are used by the code that it
            // had generated. If the case had not been generated, then we want to remove it from m_list.
            cases.append(someCase);

            additionalWatchpointSets.appendVector(WTFMove(sets));
        }();
        ++srcIndex;
    }

    // If the resulting set of cases is so big that we would stop caching and this is InstanceOf,
    // then we want to generate the generic InstanceOf and then stop.
    if (auto megamorphicCase = tryFoldToMegamorphic(codeBlock, cases.span())) {
        cases.shrink(0);
        additionalWatchpointSets.clear();
        cases.append(megamorphicCase.releaseNonNull());
    }

    dataLogLnIf(InlineCacheCompilerInternal::verbose, "Optimized cases: ", listDump(cases));

    auto finishCodeGeneration = [&](Ref<PolymorphicAccessJITStubRoutine>&& stub) {
        std::unique_ptr<StructureStubInfoClearingWatchpoint> watchpoint;
        if (!stub->watchpoints().isEmpty()) {
            watchpoint = makeUnique<StructureStubInfoClearingWatchpoint>(codeBlock, m_stubInfo);
            stub->watchpointSet().add(watchpoint.get());
        }

        auto cases = stub->cases().span();

        poly.m_list.shrink(0);
        poly.m_list.append(cases);

        unsigned callLinkInfoCount = 0;
        bool isMegamorphic = false;
        for (auto& accessCase : cases)
            isMegamorphic |= JSC::isMegamorphic(accessCase->type());

        auto handler = InlineCacheHandler::create(InlineCacheCompiler::generateSlowPathHandler(vm(), m_stubInfo.accessType), codeBlock, m_stubInfo, WTFMove(stub), WTFMove(watchpoint), callLinkInfoCount);
        dataLogLnIf(InlineCacheCompilerInternal::verbose, "Returning: ", handler->callTarget());

        AccessGenerationResult::Kind resultKind;
        if (isMegamorphic)
            resultKind = AccessGenerationResult::GeneratedMegamorphicCode;
        else if (poly.m_list.size() >= Options::maxAccessVariantListSize())
            resultKind = AccessGenerationResult::GeneratedFinalCode;
        else
            resultKind = AccessGenerationResult::GeneratedNewCode;

        return AccessGenerationResult(resultKind, WTFMove(handler));
    };

    // At this point we're convinced that 'cases' contains cases that we want to JIT now and we won't change that set anymore.

    auto allocator = makeDefaultScratchAllocator();
    m_allocator = &allocator;
    m_scratchGPR = allocator.allocateScratchGPR();

    std::optional<FPRReg> scratchFPR;
    bool doesCalls = false;
    bool doesJSCalls = false;
    bool needsInt32PropertyCheck = false;
    bool needsStringPropertyCheck = false;
    bool needsSymbolPropertyCheck = false;
    bool acceptValueProperty = false;
    bool allGuardedByStructureCheck = true;
    bool hasConstantIdentifier = JSC::hasConstantIdentifier(m_stubInfo.accessType);
    if (!hasConstantIdentifier)
        allGuardedByStructureCheck = false;
    FixedVector<Ref<AccessCase>> keys(WTFMove(cases));
    m_callLinkInfos.resize(keys.size());
    Vector<JSCell*> cellsToMark;
    for (auto& entry : keys) {
        if (!scratchFPR && needsScratchFPR(entry->m_type))
            scratchFPR = allocator.allocateScratchFPR();

        if (entry->doesCalls(vm())) {
            doesCalls = true;
            entry->collectDependentCells(vm(), cellsToMark);
        }
        doesJSCalls |= JSC::doesJSCalls(entry->type());

        if (!hasConstantIdentifier) {
            if (entry->requiresIdentifierNameMatch()) {
                if (entry->uid()->isSymbol())
                    needsSymbolPropertyCheck = true;
                else
                    needsStringPropertyCheck = true;
            } else if (entry->requiresInt32PropertyCheck())
                needsInt32PropertyCheck = true;
            else
                acceptValueProperty = true;
        } else
            allGuardedByStructureCheck &= entry->guardedByStructureCheckSkippingConstantIdentifierCheck();
    }
    m_scratchFPR = scratchFPR.value_or(InvalidFPRReg);

    CCallHelpers jit(codeBlock);
    m_jit = &jit;

    if (ASSERT_ENABLED) {
        if (m_stubInfo.useDataIC) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), jit.scratchRegister());
            jit.addPtr(jit.scratchRegister(), GPRInfo::callFrameRegister, jit.scratchRegister());
        } else
            jit.addPtr(CCallHelpers::TrustedImm32(codeBlock->stackPointerOffset() * sizeof(Register)), GPRInfo::callFrameRegister, jit.scratchRegister());
        auto ok = jit.branchPtr(CCallHelpers::Equal, CCallHelpers::stackPointerRegister, jit.scratchRegister());
        jit.breakpoint();
        ok.link(&jit);
    }

    m_preservedReusedRegisterState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

    if (keys.isEmpty()) {
        // This is super unlikely, but we make it legal anyway.
        m_failAndRepatch.append(jit.jump());
    } else if (!allGuardedByStructureCheck || keys.size() == 1) {
        // If there are any proxies in the list, we cannot just use a binary switch over the structure.
        // We need to resort to a cascade. A cascade also happens to be optimal if we only have just
        // one case.
        CCallHelpers::JumpList fallThrough;
        if (needsInt32PropertyCheck || needsStringPropertyCheck || needsSymbolPropertyCheck) {
            if (needsInt32PropertyCheck) {
                CCallHelpers::JumpList notInt32;

                if (!m_stubInfo.propertyIsInt32) {
#if USE(JSVALUE64)
                    notInt32.append(jit.branchIfNotInt32(m_stubInfo.propertyGPR()));
#else
                    notInt32.append(jit.branchIfNotInt32(m_stubInfo.propertyTagGPR()));
#endif
                }
                JIT_COMMENT(jit, "Cases start (needsInt32PropertyCheck)");
                for (unsigned i = keys.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.shrink(0);
                    if (keys[i]->requiresInt32PropertyCheck())
                        generateWithGuard(i, keys[i].get(), fallThrough);
                }

                if (needsStringPropertyCheck || needsSymbolPropertyCheck || acceptValueProperty) {
                    notInt32.link(&jit);
                    fallThrough.link(&jit);
                    fallThrough.shrink(0);
                } else
                    m_failAndRepatch.append(notInt32);
            }

            if (needsStringPropertyCheck) {
                CCallHelpers::JumpList notString;
                GPRReg propertyGPR = m_stubInfo.propertyGPR();
                if (!m_stubInfo.propertyIsString) {
#if USE(JSVALUE32_64)
                    GPRReg propertyTagGPR = m_stubInfo.propertyTagGPR();
                    notString.append(jit.branchIfNotCell(propertyTagGPR));
#else
                    notString.append(jit.branchIfNotCell(propertyGPR));
#endif
                    notString.append(jit.branchIfNotString(propertyGPR));
                }

                jit.loadPtr(MacroAssembler::Address(propertyGPR, JSString::offsetOfValue()), m_scratchGPR);

                m_failAndRepatch.append(jit.branchIfRopeStringImpl(m_scratchGPR));

                JIT_COMMENT(jit, "Cases start (needsStringPropertyCheck)");
                for (unsigned i = keys.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.shrink(0);
                    if (keys[i]->requiresIdentifierNameMatch() && !keys[i]->uid()->isSymbol())
                        generateWithGuard(i, keys[i].get(), fallThrough);
                }

                if (needsSymbolPropertyCheck || acceptValueProperty) {
                    notString.link(&jit);
                    fallThrough.link(&jit);
                    fallThrough.shrink(0);
                } else
                    m_failAndRepatch.append(notString);
            }

            if (needsSymbolPropertyCheck) {
                CCallHelpers::JumpList notSymbol;
                if (!m_stubInfo.propertyIsSymbol) {
                    GPRReg propertyGPR = m_stubInfo.propertyGPR();
#if USE(JSVALUE32_64)
                    GPRReg propertyTagGPR = m_stubInfo.propertyTagGPR();
                    notSymbol.append(jit.branchIfNotCell(propertyTagGPR));
#else
                    notSymbol.append(jit.branchIfNotCell(propertyGPR));
#endif
                    notSymbol.append(jit.branchIfNotSymbol(propertyGPR));
                }

                JIT_COMMENT(jit, "Cases start (needsSymbolPropertyCheck)");
                for (unsigned i = keys.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.shrink(0);
                    if (keys[i]->requiresIdentifierNameMatch() && keys[i]->uid()->isSymbol())
                        generateWithGuard(i, keys[i].get(), fallThrough);
                }

                if (acceptValueProperty) {
                    notSymbol.link(&jit);
                    fallThrough.link(&jit);
                    fallThrough.shrink(0);
                } else
                    m_failAndRepatch.append(notSymbol);
            }

            if (acceptValueProperty) {
                JIT_COMMENT(jit, "Cases start (remaining)");
                for (unsigned i = keys.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.shrink(0);
                    if (!keys[i]->requiresIdentifierNameMatch() && !keys[i]->requiresInt32PropertyCheck())
                        generateWithGuard(i, keys[i].get(), fallThrough);
                }
            }
        } else {
            // Cascade through the list, preferring newer entries.
            JIT_COMMENT(jit, "Cases start !(needsInt32PropertyCheck || needsStringPropertyCheck || needsSymbolPropertyCheck)");
            for (unsigned i = keys.size(); i--;) {
                fallThrough.link(&jit);
                fallThrough.shrink(0);
                generateWithGuard(i, keys[i].get(), fallThrough);
            }
        }

        m_failAndRepatch.append(fallThrough);

    } else {
        JIT_COMMENT(jit, "Cases start (allGuardedByStructureCheck)");
        jit.load32(
            CCallHelpers::Address(m_stubInfo.m_baseGPR, JSCell::structureIDOffset()),
            m_scratchGPR);

        Vector<int64_t, 16> caseValues(keys.size());
        for (unsigned i = 0; i < keys.size(); ++i)
            caseValues[i] = std::bit_cast<int32_t>(keys[i]->structure()->id());

        BinarySwitch binarySwitch(m_scratchGPR, caseValues.span(), BinarySwitch::Int32);
        while (binarySwitch.advance(jit))
            generateWithoutGuard(binarySwitch.caseIndex(), keys[binarySwitch.caseIndex()].get());
        m_failAndRepatch.append(binarySwitch.fallThrough());
    }

    if (!m_failAndIgnore.empty()) {
        m_failAndIgnore.link(&jit);
        JIT_COMMENT(jit, "failAndIgnore");

        // Make sure that the inline cache optimization code knows that we are taking slow path because
        // of something that isn't patchable. The slow path will decrement "countdown" and will only
        // patch things if the countdown reaches zero. We increment the slow path count here to ensure
        // that the slow path does not try to patch.
        if (m_stubInfo.useDataIC)
            jit.add8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCountdown()));
        else {
            jit.move(CCallHelpers::TrustedImmPtr(&m_stubInfo.countdown), m_scratchGPR);
            jit.add8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(m_scratchGPR));
        }
    }

    CCallHelpers::JumpList failure;
    if (allocator.didReuseRegisters()) {
        m_failAndRepatch.link(&jit);
        restoreScratch();
    } else
        failure = m_failAndRepatch;
    failure.append(jit.jump());

    CodeBlock* codeBlockThatOwnsExceptionHandlers = nullptr;
    DisposableCallSiteIndex callSiteIndexForExceptionHandling;
    if (needsToRestoreRegistersIfException() && doesJSCalls) {
        // Emit the exception handler.
        // Note that this code is only reachable when doing genericUnwind from a pure JS getter/setter .
        // Note also that this is not reachable from custom getter/setter. Custom getter/setters will have
        // their own exception handling logic that doesn't go through genericUnwind.
        MacroAssembler::Label makeshiftCatchHandler = jit.label();
        JIT_COMMENT(jit, "exception handler");

        InlineCacheCompiler::SpillState spillState = this->spillStateForJSCall();
        ASSERT(!spillState.isEmpty());
        jit.loadPtr(vm().addressOfCallFrameForCatch(), GPRInfo::callFrameRegister);
        if (m_stubInfo.useDataIC) {
            ASSERT(!JITCode::isBaselineCode(m_jitType));
            jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), m_scratchGPR);
            jit.addPtr(CCallHelpers::TrustedImm32(-(m_preservedReusedRegisterState.numberOfBytesPreserved + spillState.numberOfStackBytesUsedForRegisterPreservation)), m_scratchGPR);
            jit.addPtr(m_scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
        } else {
            int stackPointerOffset = (codeBlock->stackPointerOffset() * sizeof(Register)) - m_preservedReusedRegisterState.numberOfBytesPreserved - spillState.numberOfStackBytesUsedForRegisterPreservation;
            jit.addPtr(CCallHelpers::TrustedImm32(stackPointerOffset), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
        }

        restoreLiveRegistersFromStackForCallWithThrownException(spillState);
        restoreScratch();
        HandlerInfo oldHandler = originalExceptionHandler();
        jit.jumpThunk(oldHandler.nativeCode);
        DisposableCallSiteIndex newExceptionHandlingCallSite = this->callSiteIndexForExceptionHandling();
        jit.addLinkTask(
            [=] (LinkBuffer& linkBuffer) {
                HandlerInfo handlerToRegister = oldHandler;
                handlerToRegister.nativeCode = linkBuffer.locationOf<ExceptionHandlerPtrTag>(makeshiftCatchHandler);
                handlerToRegister.start = newExceptionHandlingCallSite.bits();
                handlerToRegister.end = newExceptionHandlingCallSite.bits() + 1;
                codeBlock->appendExceptionHandler(handlerToRegister);
            });

        // We set these to indicate to the stub to remove itself from the CodeBlock's
        // exception handler table when it is deallocated.
        codeBlockThatOwnsExceptionHandlers = codeBlock;
        ASSERT(JSC::JITCode::isOptimizingJIT(codeBlockThatOwnsExceptionHandlers->jitType()));
        callSiteIndexForExceptionHandling = this->callSiteIndexForExceptionHandling();
    }

    CodeLocationLabel<JSInternalPtrTag> successLabel = m_stubInfo.doneLocation;
    if (m_stubInfo.useDataIC) {
        JIT_COMMENT(jit, "failure far jump");
        failure.link(&jit);
        jit.farJump(CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfSlowPathStartLocation()), JITStubRoutinePtrTag);
    } else {
        m_success.linkThunk(successLabel, &jit);
        failure.linkThunk(m_stubInfo.slowPathStartLocation, &jit);
    }

    LinkBuffer linkBuffer(jit, codeBlock, LinkBuffer::Profile::InlineCache, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) {
        dataLogLnIf(InlineCacheCompilerInternal::verbose, "Did fail to allocate.");
        return AccessGenerationResult::GaveUp;
    }


    if (m_stubInfo.useDataIC)
        ASSERT(m_success.empty());

    dataLogLnIf(InlineCacheCompilerInternal::verbose, FullCodeOrigin(codeBlock, m_stubInfo.codeOrigin), ": Generating polymorphic access stub for ", listDump(keys));

    MacroAssemblerCodeRef<JITStubRoutinePtrTag> code = FINALIZE_CODE_FOR(codeBlock, linkBuffer, JITStubRoutinePtrTag, categoryName(m_stubInfo.accessType), "%s", toCString("Access stub for ", *codeBlock, " ", m_stubInfo.codeOrigin, "with start: ", m_stubInfo.startLocation, " with return point ", successLabel, ": ", listDump(keys)).data());

    CodeBlock* owner = codeBlock;
    FixedVector<StructureID> weakStructures(WTFMove(m_weakStructures));
    auto stub = createICJITStubRoutine(code, WTFMove(keys), WTFMove(weakStructures), vm(), owner, doesCalls, cellsToMark, WTFMove(m_callLinkInfos), codeBlockThatOwnsExceptionHandlers, callSiteIndexForExceptionHandling);

    {
        auto& watchpoints = stub->watchpoints();
        for (auto& condition : m_conditions)
            ensureReferenceAndInstallWatchpoint(vm(), watchpoints, stub->watchpointSet(), condition);

        // NOTE: We currently assume that this is relatively rare. It mainly arises for accesses to
        // properties on DOM nodes. For sure we cache many DOM node accesses, but even in
        // Real Pages (TM), we appear to spend most of our time caching accesses to properties on
        // vanilla objects or exotic objects from within JSC (like Arguments, those are super popular).
        // Those common kinds of JSC object accesses don't hit this case.
        for (WatchpointSet* set : additionalWatchpointSets)
            ensureReferenceAndAddWatchpoint(vm(), watchpoints, stub->watchpointSet(), *set);
    }

    return finishCodeGeneration(WTFMove(stub));
}

#if CPU(ADDRESS64)

template<bool ownProperty>
static void loadHandlerImpl(VM&, CCallHelpers& jit, JSValueRegs baseJSR, JSValueRegs resultJSR, GPRReg scratch1GPR, GPRReg scratch2GPR)
{
    jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfOffset()), scratch2GPR);
    if constexpr (ownProperty)
        jit.loadProperty(baseJSR.payloadGPR(), scratch2GPR, resultJSR);
    else {
        jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfHolder()), scratch1GPR);
        jit.loadProperty(scratch1GPR, scratch2GPR, resultJSR);
    }
}

// FIXME: We may need to implement it in offline asm eventually to share it with non JIT environment.
template<bool ownProperty>
static MacroAssemblerCodeRef<JITThunkPtrTag> getByIdLoadHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::scratch1GPR;
    using BaselineJITRegisters::GetById::scratch2GPR;
    using BaselineJITRegisters::GetById::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    loadHandlerImpl<ownProperty>(vm, jit, baseJSR, resultJSR, scratch1GPR, scratch2GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "GetById Load handler"_s, "GetById Load handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByIdLoadOwnPropertyHandler(VM& vm)
{
    constexpr bool ownProperty = true;
    return getByIdLoadHandlerImpl<ownProperty>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByIdLoadPrototypePropertyHandler(VM& vm)
{
    constexpr bool ownProperty = false;
    return getByIdLoadHandlerImpl<ownProperty>(vm);
}

// FIXME: We may need to implement it in offline asm eventually to share it with non JIT environment.
MacroAssemblerCodeRef<JITThunkPtrTag> getByIdMissHandler(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;
    using BaselineJITRegisters::GetById::scratch1GPR;
    using BaselineJITRegisters::GetById::scratch2GPR;
    using BaselineJITRegisters::GetById::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));

    jit.moveTrustedValue(jsUndefined(), resultJSR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "GetById Miss handler"_s, "GetById Miss handler");
}

template<bool isAccessor>
static void customGetterHandlerImpl(VM& vm, CCallHelpers& jit, JSValueRegs baseJSR, JSValueRegs resultJSR, GPRReg stubInfoGPR, GPRReg scratch1GPR, GPRReg scratch2GPR, GPRReg scratch3GPR)
{
    if constexpr (!isAccessor) {
        jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfHolder()), scratch3GPR);
        jit.moveConditionally64(CCallHelpers::Equal, scratch3GPR, CCallHelpers::TrustedImm32(0), baseJSR.payloadGPR(), scratch3GPR, baseJSR.payloadGPR());
    }

    jit.transfer32(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    // Need to make room for the C call so any of our stack spillage isn't overwritten. It's
    // hard to track if someone did spillage or not, so we just assume that we always need
    // to make some space here.
    jit.makeSpaceOnStackForCCall();
    jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);

    // EncodedJSValue (*GetValueFunc)(JSGlobalObject*, EncodedJSValue thisValue, PropertyName);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfGlobalObject()), scratch1GPR);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfUid()), scratch2GPR);
    if (Options::useJITCage()) {
        jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCustomAccessor()), scratch3GPR);
        jit.setupArguments<GetValueFuncWithPtr>(scratch1GPR, CCallHelpers::CellValue(baseJSR.payloadGPR()), scratch2GPR, scratch3GPR);
        jit.callOperation<OperationPtrTag>(vmEntryCustomGetter);
    } else {
        jit.setupArguments<GetValueFunc>(scratch1GPR, CCallHelpers::CellValue(baseJSR.payloadGPR()), scratch2GPR);
        jit.call(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCustomAccessor()), CustomAccessorPtrTag);
    }
    jit.setupResults(resultJSR);
    jit.reclaimSpaceOnStackForCCall();
    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);
}

// FIXME: We may need to implement it in offline asm eventually to share it with non JIT environment.
template<bool isAccessor>
static MacroAssemblerCodeRef<JITThunkPtrTag> getByIdCustomHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;
    using BaselineJITRegisters::GetById::scratch1GPR;
    using BaselineJITRegisters::GetById::scratch2GPR;
    using BaselineJITRegisters::GetById::scratch3GPR;
    using BaselineJITRegisters::GetById::scratch4GPR;
    using BaselineJITRegisters::GetById::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    customGetterHandlerImpl<isAccessor>(vm, jit, baseJSR, resultJSR, stubInfoGPR, scratch1GPR, scratch2GPR, scratch3GPR);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "GetById Custom handler"_s, "GetById Custom handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByIdCustomAccessorHandler(VM& vm)
{
    constexpr bool isAccessor = true;
    return getByIdCustomHandlerImpl<isAccessor>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByIdCustomValueHandler(VM& vm)
{
    constexpr bool isAccessor = false;
    return getByIdCustomHandlerImpl<isAccessor>(vm);
}

static void getterHandlerImpl(VM&, CCallHelpers& jit, JSValueRegs baseJSR, JSValueRegs resultJSR, GPRReg stubInfoGPR, GPRReg scratch1GPR, GPRReg scratch2GPR)
{
    jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfHolder()), scratch1GPR);
    jit.moveConditionally64(CCallHelpers::Equal, scratch1GPR, CCallHelpers::TrustedImm32(0), baseJSR.payloadGPR(), scratch1GPR, scratch1GPR);
    jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfOffset()), scratch2GPR);
    jit.loadProperty(scratch1GPR, scratch2GPR, JSValueRegs { scratch1GPR });

    jit.transfer32(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    // Create a JS call using a JS call inline cache. Assume that:
    //
    // - SP is aligned and represents the extent of the calling compiler's stack usage.
    //
    // - FP is set correctly (i.e. it points to the caller's call frame header).
    //
    // - SP - FP is an aligned difference.
    //
    // - Any byte between FP (exclusive) and SP (inclusive) could be live in the calling
    //   code.
    //
    // Therefore, we temporarily grow the stack for the purpose of the call and then
    // shrink it after.

    // There is a "this" argument.
    // ... and a value argument if we're calling a setter.
    constexpr unsigned numberOfParameters = 1;

    // Get the accessor; if there ain't one then the result is jsUndefined().
    // Note that GetterSetter always has cells for both. If it is not set (like, getter exits, but setter is not set), Null{Getter,Setter}Function is stored.
    jit.loadPtr(CCallHelpers::Address(scratch1GPR, GetterSetter::offsetOfGetter()), scratch1GPR);
    auto willInvokeGetter = jit.branchIfNotType(scratch1GPR, NullSetterFunctionType);

    jit.moveTrustedValue(jsUndefined(), resultJSR);
    auto done = jit.jump();
    willInvokeGetter.link(&jit);

    constexpr unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
    static_assert(!(numberOfRegsForCall % stackAlignmentRegisters()));
    constexpr unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);

    constexpr unsigned alignedNumberOfBytesForCall = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(numberOfBytesForCall);

    jit.subPtr(CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall), CCallHelpers::stackPointerRegister);
    CCallHelpers::Address calleeFrame = CCallHelpers::Address(CCallHelpers::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));
    jit.store32(CCallHelpers::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + PayloadOffset));
    jit.storeCell(scratch1GPR, calleeFrame.withOffset(CallFrameSlot::callee * sizeof(Register)));
    jit.storeCell(baseJSR.payloadGPR(), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(0).offset() * sizeof(Register)));

    // handlerGPR can be the same to BaselineJITRegisters::Call::calleeJSR.
    if constexpr (GPRInfo::handlerGPR == BaselineJITRegisters::Call::calleeJSR.payloadGPR()) {
        jit.swap(scratch1GPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
        jit.addPtr(CCallHelpers::TrustedImm32(InlineCacheHandler::offsetOfCallLinkInfos()), scratch1GPR, BaselineJITRegisters::Call::callLinkInfoGPR);
    } else {
        jit.move(scratch1GPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
        jit.addPtr(CCallHelpers::TrustedImm32(InlineCacheHandler::offsetOfCallLinkInfos()), GPRInfo::handlerGPR, BaselineJITRegisters::Call::callLinkInfoGPR);
    }
    CallLinkInfo::emitDataICFastPath(jit);
    jit.setupResults(resultJSR);

    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), scratch1GPR);
    jit.addPtr(CCallHelpers::TrustedImm32(-static_cast<int32_t>(sizeof(CallerFrameAndPC) + maxFrameExtentForSlowPathCall)), scratch1GPR);
    jit.addPtr(scratch1GPR, GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);

    done.link(&jit);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByIdGetterHandler(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;
    using BaselineJITRegisters::GetById::scratch1GPR;
    using BaselineJITRegisters::GetById::scratch2GPR;
    using BaselineJITRegisters::GetById::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));

    getterHandlerImpl(vm, jit, baseJSR, resultJSR, stubInfoGPR, scratch1GPR, scratch2GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "GetById Getter handler"_s, "GetById Getter handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByIdProxyObjectLoadHandler(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;
    using BaselineJITRegisters::GetById::scratch1GPR;
    using BaselineJITRegisters::GetById::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    jit.load8(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::typeInfoTypeOffset()), scratch1GPR);
    fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratch1GPR, CCallHelpers::TrustedImm32(ProxyObjectType)));

    jit.transfer32(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    // Create a JS call using a JS call inline cache. Assume that:
    //
    // - SP is aligned and represents the extent of the calling compiler's stack usage.
    //
    // - FP is set correctly (i.e. it points to the caller's call frame header).
    //
    // - SP - FP is an aligned difference.
    //
    // - Any byte between FP (exclusive) and SP (inclusive) could be live in the calling
    //   code.
    //
    // Therefore, we temporarily grow the stack for the purpose of the call and then
    // shrink it after.

    // There is a "this" argument.
    // ... and a value argument if we're calling a setter.
    constexpr unsigned numberOfParameters = 3;
    constexpr unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
    static_assert(!(numberOfRegsForCall % stackAlignmentRegisters()));
    constexpr unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);
    constexpr unsigned alignedNumberOfBytesForCall = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(numberOfBytesForCall);

    jit.subPtr(CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall), CCallHelpers::stackPointerRegister);
    CCallHelpers::Address calleeFrame = CCallHelpers::Address(CCallHelpers::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));
    jit.store32(CCallHelpers::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + PayloadOffset));
    jit.storeCell(baseJSR.payloadGPR(), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(0).offset() * sizeof(Register)));
    jit.transferPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfHolder()), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(1).offset() * sizeof(Register)));
    jit.storeCell(baseJSR.payloadGPR(), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(2).offset() * sizeof(Register)));
    jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfGlobalObject()), scratch1GPR);
    jit.loadPtr(CCallHelpers::Address(scratch1GPR, JSGlobalObject::offsetOfPerformProxyObjectGetFunction()), scratch1GPR);
    jit.storeCell(scratch1GPR, calleeFrame.withOffset(CallFrameSlot::callee * sizeof(Register)));

    // handlerGPR can be the same to BaselineJITRegisters::Call::calleeJSR.
    if constexpr (GPRInfo::handlerGPR == BaselineJITRegisters::Call::calleeJSR.payloadGPR()) {
        jit.swap(scratch1GPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
        jit.addPtr(CCallHelpers::TrustedImm32(InlineCacheHandler::offsetOfCallLinkInfos()), scratch1GPR, BaselineJITRegisters::Call::callLinkInfoGPR);
    } else {
        jit.move(scratch1GPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
        jit.addPtr(CCallHelpers::TrustedImm32(InlineCacheHandler::offsetOfCallLinkInfos()), GPRInfo::handlerGPR, BaselineJITRegisters::Call::callLinkInfoGPR);
    }
    CallLinkInfo::emitDataICFastPath(jit);
    jit.setupResults(resultJSR);

    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), scratch1GPR);
    jit.addPtr(CCallHelpers::TrustedImm32(-static_cast<int32_t>(sizeof(CallerFrameAndPC) + maxFrameExtentForSlowPathCall)), scratch1GPR);
    jit.addPtr(scratch1GPR, GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "GetById ProxyObjectLoad handler"_s, "GetById ProxyObjectLoad handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByIdModuleNamespaceLoadHandler(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;
    using BaselineJITRegisters::GetById::scratch1GPR;
    using BaselineJITRegisters::GetById::scratch2GPR;
    using BaselineJITRegisters::GetById::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;
    CCallHelpers::JumpList failAndIgnore;

    // We do not need to check structure. Just checking instance pointer.
    fallThrough.append(jit.branchPtr(CCallHelpers::NotEqual, baseJSR.payloadGPR(), CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfModuleNamespaceObject())));

    jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfModuleVariableSlot()), scratch1GPR);
    jit.loadValue(CCallHelpers::Address(scratch1GPR), JSValueRegs { scratch1GPR });
    failAndIgnore.append(jit.branchIfEmpty(JSValueRegs { scratch1GPR }));
    jit.moveValueRegs(JSValueRegs { scratch1GPR }, resultJSR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    failAndIgnore.link(&jit);
    jit.add8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfCountdown()));

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "GetById ModuleNamespaceLoad handler"_s, "GetById ModuleNamespaceLoad handler");
}

// FIXME: We may need to implement it in offline asm eventually to share it with non JIT environment.
MacroAssemblerCodeRef<JITThunkPtrTag> putByIdReplaceHandler(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::stubInfoGPR;
    using BaselineJITRegisters::PutById::scratch1GPR;
    using BaselineJITRegisters::PutById::scratch2GPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));

    jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfOffset()), scratch1GPR);
    jit.storeProperty(valueJSR, baseJSR.payloadGPR(), scratch1GPR, scratch2GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "PutById Replace handler"_s, "PutById Replace handler");
}

// FIXME: We may need to implement it in offline asm eventually to share it with non JIT environment.
template<bool allocating, bool reallocating>
static void transitionHandlerImpl(VM& vm, CCallHelpers& jit, CCallHelpers::JumpList& allocationFailure, JSValueRegs baseJSR, JSValueRegs valueJSR, GPRReg scratch1GPR, GPRReg scratch2GPR, GPRReg scratch3GPR, GPRReg scratch4GPR)
{
    if constexpr (!allocating) {
        JIT_COMMENT(jit, "storeProperty");
        jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfOffset()), scratch1GPR);
        jit.storeProperty(valueJSR, baseJSR.payloadGPR(), scratch1GPR, scratch2GPR);
        jit.transfer32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfNewStructureID()), CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()));
    } else {
        JIT_COMMENT(jit, "allocating");
        jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfNewSize()), scratch1GPR);
        jit.emitAllocateVariableSized(scratch2GPR, vm.auxiliarySpace(), scratch1GPR, scratch4GPR, scratch3GPR, allocationFailure);

        if constexpr (reallocating) {
            JIT_COMMENT(jit, "reallocating");
            // Handle the case where we are reallocating (i.e. the old structure/butterfly
            // already had out-of-line property storage).
            jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfOldSize()), scratch3GPR);
            jit.sub32(scratch1GPR, scratch3GPR, scratch1GPR);

            {
                auto empty = jit.branchTest32(CCallHelpers::Zero, scratch1GPR);
                auto loop = jit.label();
                jit.storeTrustedValue(JSValue(), CCallHelpers::Address(scratch2GPR));
                jit.addPtr(CCallHelpers::TrustedImm32(sizeof(JSValue)), scratch2GPR);
                jit.branchSub32(CCallHelpers::NonZero, CCallHelpers::TrustedImm32(sizeof(JSValue)), scratch1GPR).linkTo(loop, &jit);
                empty.link(&jit);
            }

            jit.loadPtr(CCallHelpers::Address(baseJSR.payloadGPR(), JSObject::butterflyOffset()), scratch1GPR);
            jit.subPtr(scratch1GPR, scratch3GPR, scratch1GPR);

            {
                auto empty = jit.branchTest32(CCallHelpers::Zero, scratch3GPR);
                auto loop = jit.label();
                jit.transferPtr(CCallHelpers::Address(scratch1GPR, -static_cast<ptrdiff_t>(sizeof(IndexingHeader))), CCallHelpers::Address(scratch2GPR));
                jit.addPtr(CCallHelpers::TrustedImm32(sizeof(JSValue)), scratch1GPR);
                jit.addPtr(CCallHelpers::TrustedImm32(sizeof(JSValue)), scratch2GPR);
                jit.branchSub32(CCallHelpers::NonZero, CCallHelpers::TrustedImm32(sizeof(JSValue)), scratch3GPR).linkTo(loop, &jit);
                empty.link(&jit);
            }
        } else {
            JIT_COMMENT(jit, "newlyAllocating");
            auto empty = jit.branchTest32(CCallHelpers::Zero, scratch1GPR);
            auto loop = jit.label();
            jit.storeTrustedValue(JSValue(), CCallHelpers::Address(scratch2GPR));
            jit.addPtr(CCallHelpers::TrustedImm32(sizeof(JSValue)), scratch2GPR);
            jit.branchSub32(CCallHelpers::NonZero, CCallHelpers::TrustedImm32(sizeof(JSValue)), scratch1GPR).linkTo(loop, &jit);
            empty.link(&jit);
        }
        jit.addPtr(CCallHelpers::TrustedImm32(sizeof(IndexingHeader)), scratch2GPR);

        JIT_COMMENT(jit, "updateButterfly");
        // We set the new butterfly and the structure last. Doing it this way ensures that
        // whatever we had done up to this point is forgotten if we choose to branch to slow
        // path.
        jit.nukeStructureAndStoreButterfly(vm, scratch2GPR, baseJSR.payloadGPR());
        jit.transfer32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfNewStructureID()), CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()));

        JIT_COMMENT(jit, "storeProperty");
        jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfOffset()), scratch1GPR);
        jit.storeProperty(valueJSR, baseJSR.payloadGPR(), scratch1GPR, scratch2GPR);
    }
}

// FIXME: We may need to implement it in offline asm eventually to share it with non JIT environment.
template<bool allocating, bool reallocating>
static MacroAssemblerCodeRef<JITThunkPtrTag> putByIdTransitionHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::stubInfoGPR;
    using BaselineJITRegisters::PutById::scratch1GPR;
    using BaselineJITRegisters::PutById::scratch2GPR;
    using BaselineJITRegisters::PutById::scratch3GPR;
    using BaselineJITRegisters::PutById::scratch4GPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;
    CCallHelpers::JumpList allocationFailure;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));

    transitionHandlerImpl<allocating, reallocating>(vm, jit, allocationFailure, baseJSR, valueJSR, scratch1GPR, scratch2GPR, scratch3GPR, scratch4GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    if (!allocationFailure.empty()) {
        ASSERT(allocating);
        allocationFailure.link(&jit);
        jit.makeSpaceOnStackForCCall();
        jit.setupArguments<decltype(operationReallocateButterflyAndTransition)>(CCallHelpers::TrustedImmPtr(&vm), baseJSR.payloadGPR(), GPRInfo::handlerGPR, valueJSR);
        jit.prepareCallOperation(vm);
        jit.callOperation<OperationPtrTag>(operationReallocateButterflyAndTransition);
        jit.reclaimSpaceOnStackForCCall();
        InlineCacheCompiler::emitDataICEpilogue(jit);
        jit.ret();
    }

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "PutById Transition handler"_s, "PutById Transition handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByIdTransitionNonAllocatingHandler(VM& vm)
{
    constexpr bool allocating = false;
    constexpr bool reallocating = false;
    return putByIdTransitionHandlerImpl<allocating, reallocating>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByIdTransitionNewlyAllocatingHandler(VM& vm)
{
    constexpr bool allocating = true;
    constexpr bool reallocating = false;
    return putByIdTransitionHandlerImpl<allocating, reallocating>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByIdTransitionReallocatingHandler(VM& vm)
{
    constexpr bool allocating = true;
    constexpr bool reallocating = true;
    return putByIdTransitionHandlerImpl<allocating, reallocating>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByIdTransitionReallocatingOutOfLineHandler(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::stubInfoGPR;
    using BaselineJITRegisters::PutById::scratch1GPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));

    jit.makeSpaceOnStackForCCall();
    jit.setupArguments<decltype(operationReallocateButterflyAndTransition)>(CCallHelpers::TrustedImmPtr(&vm), baseJSR.payloadGPR(), GPRInfo::handlerGPR, valueJSR);
    jit.prepareCallOperation(vm);
    jit.callOperation<OperationPtrTag>(operationReallocateButterflyAndTransition);
    jit.reclaimSpaceOnStackForCCall();
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "PutById Transition handler"_s, "PutById Transition handler");
}

template<bool isAccessor>
static void customSetterHandlerImpl(VM& vm, CCallHelpers& jit, JSValueRegs baseJSR, JSValueRegs valueJSR, GPRReg stubInfoGPR, GPRReg scratch1GPR, GPRReg scratch2GPR, GPRReg scratch3GPR)
{
    if constexpr (!isAccessor) {
        jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfHolder()), scratch3GPR);
        jit.moveConditionally64(CCallHelpers::Equal, scratch3GPR, CCallHelpers::TrustedImm32(0), baseJSR.payloadGPR(), scratch3GPR, baseJSR.payloadGPR());
    }

    jit.transfer32(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    // Need to make room for the C call so any of our stack spillage isn't overwritten. It's
    // hard to track if someone did spillage or not, so we just assume that we always need
    // to make some space here.
    jit.makeSpaceOnStackForCCall();
    jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);

    // bool (*PutValueFunc)(JSGlobalObject*, EncodedJSValue thisObject, EncodedJSValue value, PropertyName);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfGlobalObject()), scratch1GPR);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfUid()), scratch2GPR);
    if (Options::useJITCage()) {
        jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCustomAccessor()), scratch3GPR);
        jit.setupArguments<PutValueFuncWithPtr>(scratch1GPR, CCallHelpers::CellValue(baseJSR.payloadGPR()), valueJSR, scratch2GPR, scratch3GPR);
        jit.callOperation<OperationPtrTag>(vmEntryCustomSetter);
    } else {
        jit.setupArguments<PutValueFunc>(scratch1GPR, CCallHelpers::CellValue(baseJSR.payloadGPR()), valueJSR, scratch2GPR);
        jit.call(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCustomAccessor()), CustomAccessorPtrTag);
    }

    jit.reclaimSpaceOnStackForCCall();
    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);
}

// FIXME: We may need to implement it in offline asm eventually to share it with non JIT environment.
template<bool isAccessor>
static MacroAssemblerCodeRef<JITThunkPtrTag> putByIdCustomHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::stubInfoGPR;
    using BaselineJITRegisters::PutById::scratch1GPR;
    using BaselineJITRegisters::PutById::scratch2GPR;
    using BaselineJITRegisters::PutById::scratch3GPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    customSetterHandlerImpl<isAccessor>(vm, jit, baseJSR, valueJSR, stubInfoGPR, scratch1GPR, scratch2GPR, scratch3GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "PutById Custom handler"_s, "PutById Custom handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByIdCustomAccessorHandler(VM& vm)
{
    constexpr bool isAccessor = true;
    return putByIdCustomHandlerImpl<isAccessor>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByIdCustomValueHandler(VM& vm)
{
    constexpr bool isAccessor = false;
    return putByIdCustomHandlerImpl<isAccessor>(vm);
}

template<bool isStrict>
static void setterHandlerImpl(VM&, CCallHelpers& jit, JSValueRegs baseJSR, JSValueRegs valueJSR, GPRReg stubInfoGPR, GPRReg scratch1GPR, GPRReg scratch2GPR)
{
    jit.loadPtr(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfHolder()), scratch1GPR);
    jit.moveConditionally64(CCallHelpers::Equal, scratch1GPR, CCallHelpers::TrustedImm32(0), baseJSR.payloadGPR(), scratch1GPR, scratch1GPR);
    jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfOffset()), scratch2GPR);
    jit.loadProperty(scratch1GPR, scratch2GPR, JSValueRegs { scratch1GPR });

    jit.transfer32(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    // Create a JS call using a JS call inline cache. Assume that:
    //
    // - SP is aligned and represents the extent of the calling compiler's stack usage.
    //
    // - FP is set correctly (i.e. it points to the caller's call frame header).
    //
    // - SP - FP is an aligned difference.
    //
    // - Any byte between FP (exclusive) and SP (inclusive) could be live in the calling
    //   code.
    //
    // Therefore, we temporarily grow the stack for the purpose of the call and then
    // shrink it after.

    // There is a "this" argument.
    // ... and a value argument if we're calling a setter.
    constexpr unsigned numberOfParameters = 2;

    // Get the accessor; if there ain't one then the result is jsUndefined().
    // Note that GetterSetter always has cells for both. If it is not set (like, getter exits, but setter is not set), Null{Getter,Setter}Function is stored.
    jit.loadPtr(CCallHelpers::Address(scratch1GPR, GetterSetter::offsetOfSetter()), scratch1GPR);
    if constexpr (isStrict) {
        CCallHelpers::Jump shouldNotThrowError = jit.branchIfNotType(scratch1GPR, NullSetterFunctionType);
        // We replace setter with this AccessCase's JSGlobalObject::nullSetterStrictFunction, which will throw an error with the right JSGlobalObject.
        jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfGlobalObject()), scratch1GPR);
        jit.loadPtr(CCallHelpers::Address(scratch1GPR, JSGlobalObject::offsetOfNullSetterStrictFunction()), scratch1GPR);
        shouldNotThrowError.link(&jit);
    }

    constexpr unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
    static_assert(!(numberOfRegsForCall % stackAlignmentRegisters()));
    constexpr unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);

    constexpr unsigned alignedNumberOfBytesForCall = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(numberOfBytesForCall);

    jit.subPtr(CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall), CCallHelpers::stackPointerRegister);
    CCallHelpers::Address calleeFrame = CCallHelpers::Address(CCallHelpers::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));
    jit.store32(CCallHelpers::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + PayloadOffset));
    jit.storeCell(scratch1GPR, calleeFrame.withOffset(CallFrameSlot::callee * sizeof(Register)));
    jit.storeCell(baseJSR.payloadGPR(), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(0).offset() * sizeof(Register)));
    jit.storeValue(valueJSR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(1).offset() * sizeof(Register)));

    // handlerGPR can be the same to BaselineJITRegisters::Call::calleeJSR.
    if constexpr (GPRInfo::handlerGPR == BaselineJITRegisters::Call::calleeJSR.payloadGPR()) {
        jit.swap(scratch1GPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
        jit.addPtr(CCallHelpers::TrustedImm32(InlineCacheHandler::offsetOfCallLinkInfos()), scratch1GPR, BaselineJITRegisters::Call::callLinkInfoGPR);
    } else {
        jit.move(scratch1GPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
        jit.addPtr(CCallHelpers::TrustedImm32(InlineCacheHandler::offsetOfCallLinkInfos()), GPRInfo::handlerGPR, BaselineJITRegisters::Call::callLinkInfoGPR);
    }
    CallLinkInfo::emitDataICFastPath(jit);

    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), scratch1GPR);
    jit.addPtr(CCallHelpers::TrustedImm32(-static_cast<int32_t>(sizeof(CallerFrameAndPC) + maxFrameExtentForSlowPathCall)), scratch1GPR);
    jit.addPtr(scratch1GPR, GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
}

template<bool isStrict>
static MacroAssemblerCodeRef<JITThunkPtrTag> putByIdSetterHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::stubInfoGPR;
    using BaselineJITRegisters::PutById::scratch1GPR;
    using BaselineJITRegisters::PutById::scratch2GPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));

    setterHandlerImpl<isStrict>(vm, jit, baseJSR, valueJSR, stubInfoGPR, scratch1GPR, scratch2GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "PutById Setter handler"_s, "PutById Setter handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByIdStrictSetterHandler(VM& vm)
{
    constexpr bool isStrict = true;
    return putByIdSetterHandlerImpl<isStrict>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByIdSloppySetterHandler(VM& vm)
{
    constexpr bool isStrict = false;
    return putByIdSetterHandlerImpl<isStrict>(vm);
}

// FIXME: We may need to implement it in offline asm eventually to share it with non JIT environment.
template<bool hit>
static MacroAssemblerCodeRef<JITThunkPtrTag> inByIdInHandlerImpl(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::InById::baseJSR;
    using BaselineJITRegisters::InById::scratch1GPR;
    using BaselineJITRegisters::InById::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));

    jit.boxBoolean(hit, resultJSR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "InById handler"_s, "InById handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> inByIdHitHandler(VM& vm)
{
    constexpr bool hit = true;
    return inByIdInHandlerImpl<hit>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> inByIdMissHandler(VM& vm)
{
    constexpr bool hit = false;
    return inByIdInHandlerImpl<hit>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> deleteByIdDeleteHandler(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::DelById::baseJSR;
    using BaselineJITRegisters::DelById::scratch1GPR;
    using BaselineJITRegisters::DelById::scratch2GPR;
    using BaselineJITRegisters::DelById::scratch3GPR;
    using BaselineJITRegisters::DelById::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));

    jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfOffset()), scratch1GPR);
    jit.moveTrustedValue(JSValue(), JSValueRegs { scratch3GPR });
    jit.storeProperty(JSValueRegs { scratch3GPR }, baseJSR.payloadGPR(), scratch1GPR, scratch2GPR);
    jit.transfer32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfNewStructureID()), CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()));

    jit.move(MacroAssembler::TrustedImm32(true), resultJSR.payloadGPR());
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DeleteById handler"_s, "DeleteById handler");
}

template<bool returnValue>
static MacroAssemblerCodeRef<JITThunkPtrTag> deleteByIdIgnoreHandlerImpl(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::DelById::baseJSR;
    using BaselineJITRegisters::DelById::scratch1GPR;
    using BaselineJITRegisters::DelById::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));

    jit.move(MacroAssembler::TrustedImm32(returnValue), resultJSR.payloadGPR());
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DeleteById handler"_s, "DeleteById handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> deleteByIdDeleteNonConfigurableHandler(VM& vm)
{
    constexpr bool resultValue = false;
    return deleteByIdIgnoreHandlerImpl<resultValue>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> deleteByIdDeleteMissHandler(VM& vm)
{
    constexpr bool resultValue = true;
    return deleteByIdIgnoreHandlerImpl<resultValue>(vm);
}

// FIXME: We may need to implement it in offline asm eventually to share it with non JIT environment.
template<bool hit>
static MacroAssemblerCodeRef<JITThunkPtrTag> instanceOfHandlerImpl(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::Instanceof::valueJSR;
    using BaselineJITRegisters::Instanceof::protoJSR;
    using BaselineJITRegisters::Instanceof::resultJSR;
    using BaselineJITRegisters::Instanceof::scratch1GPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, valueJSR.payloadGPR(), scratch1GPR));

    fallThrough.append(jit.branchPtr(CCallHelpers::NotEqual, protoJSR.payloadGPR(), CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfHolder())));

    jit.boxBooleanPayload(hit, resultJSR.payloadGPR());
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "InstanceOf handler"_s, "InstanceOf handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> instanceOfHitHandler(VM& vm)
{
    constexpr bool hit = true;
    return instanceOfHandlerImpl<hit>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> instanceOfMissHandler(VM& vm)
{
    constexpr bool hit = false;
    return instanceOfHandlerImpl<hit>(vm);
}

template<bool ownProperty, bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> getByValLoadHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::GetByVal::baseJSR;
    using BaselineJITRegisters::GetByVal::propertyJSR;
    using BaselineJITRegisters::GetByVal::scratch1GPR;
    using BaselineJITRegisters::GetByVal::scratch2GPR;
    using BaselineJITRegisters::GetByVal::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    loadHandlerImpl<ownProperty>(vm, jit, baseJSR, resultJSR, scratch1GPR, scratch2GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "GetByVal Load handler"_s, "GetByVal Load handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringLoadOwnPropertyHandler(VM& vm)
{
    constexpr bool ownProperty = true;
    constexpr bool isSymbol = false;
    return getByValLoadHandlerImpl<ownProperty, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringLoadPrototypePropertyHandler(VM& vm)
{
    constexpr bool ownProperty = false;
    constexpr bool isSymbol = false;
    return getByValLoadHandlerImpl<ownProperty, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolLoadOwnPropertyHandler(VM& vm)
{
    constexpr bool ownProperty = true;
    constexpr bool isSymbol = true;
    return getByValLoadHandlerImpl<ownProperty, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolLoadPrototypePropertyHandler(VM& vm)
{
    constexpr bool ownProperty = false;
    constexpr bool isSymbol = true;
    return getByValLoadHandlerImpl<ownProperty, isSymbol>(vm);
}

template<bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> getByValMissHandlerImpl(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::GetByVal::baseJSR;
    using BaselineJITRegisters::GetByVal::propertyJSR;
    using BaselineJITRegisters::GetByVal::scratch1GPR;
    using BaselineJITRegisters::GetByVal::resultJSR;
    using BaselineJITRegisters::GetByVal::profileGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    jit.moveTrustedValue(jsUndefined(), resultJSR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "GetByVal Miss handler"_s, "GetByVal Miss handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringMissHandler(VM& vm)
{
    constexpr bool isSymbol = false;
    return getByValMissHandlerImpl<isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolMissHandler(VM& vm)
{
    constexpr bool isSymbol = true;
    return getByValMissHandlerImpl<isSymbol>(vm);
}

template<bool isAccessor, bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> getByValCustomHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::GetByVal::baseJSR;
    using BaselineJITRegisters::GetByVal::propertyJSR;
    using BaselineJITRegisters::GetByVal::stubInfoGPR;
    using BaselineJITRegisters::GetByVal::scratch1GPR;
    using BaselineJITRegisters::GetByVal::scratch2GPR;
    using BaselineJITRegisters::GetByVal::scratch3GPR;
    using BaselineJITRegisters::GetByVal::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    customGetterHandlerImpl<isAccessor>(vm, jit, baseJSR, resultJSR, stubInfoGPR, scratch1GPR, scratch2GPR, scratch3GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "GetByVal Custom handler"_s, "GetByVal Custom handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringCustomAccessorHandler(VM& vm)
{
    constexpr bool isAccessor = true;
    constexpr bool isSymbol = false;
    return getByValCustomHandlerImpl<isAccessor, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringCustomValueHandler(VM& vm)
{
    constexpr bool isAccessor = false;
    constexpr bool isSymbol = false;
    return getByValCustomHandlerImpl<isAccessor, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolCustomAccessorHandler(VM& vm)
{
    constexpr bool isAccessor = true;
    constexpr bool isSymbol = true;
    return getByValCustomHandlerImpl<isAccessor, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolCustomValueHandler(VM& vm)
{
    constexpr bool isAccessor = false;
    constexpr bool isSymbol = true;
    return getByValCustomHandlerImpl<isAccessor, isSymbol>(vm);
}

template<bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> getByValGetterHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::GetByVal::baseJSR;
    using BaselineJITRegisters::GetByVal::propertyJSR;
    using BaselineJITRegisters::GetByVal::stubInfoGPR;
    using BaselineJITRegisters::GetByVal::scratch1GPR;
    using BaselineJITRegisters::GetByVal::scratch2GPR;
    using BaselineJITRegisters::GetByVal::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    getterHandlerImpl(vm, jit, baseJSR, resultJSR, stubInfoGPR, scratch1GPR, scratch2GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "GetByVal Getter handler"_s, "GetByVal Getter handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringGetterHandler(VM& vm)
{
    constexpr bool isSymbol = false;
    return getByValGetterHandlerImpl<isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolGetterHandler(VM& vm)
{
    constexpr bool isSymbol = true;
    return getByValGetterHandlerImpl<isSymbol>(vm);
}

template<bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> putByValReplaceHandlerImpl(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    using BaselineJITRegisters::PutByVal::scratch1GPR;
    using BaselineJITRegisters::PutByVal::scratch2GPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfOffset()), scratch1GPR);
    jit.storeProperty(valueJSR, baseJSR.payloadGPR(), scratch1GPR, scratch2GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "PutByVal Replace handler"_s, "PutByVal Replace handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringReplaceHandler(VM& vm)
{
    constexpr bool isSymbol = false;
    return putByValReplaceHandlerImpl<isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolReplaceHandler(VM& vm)
{
    constexpr bool isSymbol = true;
    return putByValReplaceHandlerImpl<isSymbol>(vm);
}

template<bool allocating, bool reallocating, bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> putByValTransitionHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    using BaselineJITRegisters::PutByVal::scratch1GPR;
    using BaselineJITRegisters::PutByVal::scratch2GPR;
    using BaselineJITRegisters::PutByVal::profileGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;
    CCallHelpers::JumpList allocationFailure;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    // At this point, we will not go to slow path, so clobbering the other registers are fine.
    // We use propertyJSR and profileGPR for scratch register purpose.
    transitionHandlerImpl<allocating, reallocating>(vm, jit, allocationFailure, baseJSR, valueJSR, scratch1GPR, scratch2GPR, propertyJSR.payloadGPR(), profileGPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    if (!allocationFailure.empty()) {
        ASSERT(allocating);
        allocationFailure.link(&jit);
        jit.makeSpaceOnStackForCCall();
        jit.setupArguments<decltype(operationReallocateButterflyAndTransition)>(CCallHelpers::TrustedImmPtr(&vm), baseJSR.payloadGPR(), GPRInfo::handlerGPR, valueJSR);
        jit.prepareCallOperation(vm);
        jit.callOperation<OperationPtrTag>(operationReallocateButterflyAndTransition);
        jit.reclaimSpaceOnStackForCCall();
        InlineCacheCompiler::emitDataICEpilogue(jit);
        jit.ret();
    }

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "PutByVal Transition handler"_s, "PutByVal Transition handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringTransitionNonAllocatingHandler(VM& vm)
{
    constexpr bool allocating = false;
    constexpr bool reallocating = false;
    constexpr bool isSymbol = false;
    return putByValTransitionHandlerImpl<allocating, reallocating, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolTransitionNonAllocatingHandler(VM& vm)
{
    constexpr bool allocating = false;
    constexpr bool reallocating = false;
    constexpr bool isSymbol = true;
    return putByValTransitionHandlerImpl<allocating, reallocating, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringTransitionNewlyAllocatingHandler(VM& vm)
{
    constexpr bool allocating = true;
    constexpr bool reallocating = false;
    constexpr bool isSymbol = false;
    return putByValTransitionHandlerImpl<allocating, reallocating, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolTransitionNewlyAllocatingHandler(VM& vm)
{
    constexpr bool allocating = true;
    constexpr bool reallocating = false;
    constexpr bool isSymbol = true;
    return putByValTransitionHandlerImpl<allocating, reallocating, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringTransitionReallocatingHandler(VM& vm)
{
    constexpr bool allocating = true;
    constexpr bool reallocating = true;
    constexpr bool isSymbol = false;
    return putByValTransitionHandlerImpl<allocating, reallocating, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolTransitionReallocatingHandler(VM& vm)
{
    constexpr bool allocating = true;
    constexpr bool reallocating = true;
    constexpr bool isSymbol = true;
    return putByValTransitionHandlerImpl<allocating, reallocating, isSymbol>(vm);
}

template<bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> putByValTransitionOutOfLineHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    using BaselineJITRegisters::PutByVal::scratch1GPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    jit.makeSpaceOnStackForCCall();
    jit.setupArguments<decltype(operationReallocateButterflyAndTransition)>(CCallHelpers::TrustedImmPtr(&vm), baseJSR.payloadGPR(), GPRInfo::handlerGPR, valueJSR);
    jit.prepareCallOperation(vm);
    jit.callOperation<OperationPtrTag>(operationReallocateButterflyAndTransition);
    jit.reclaimSpaceOnStackForCCall();
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "PutByVal Transition handler"_s, "PutByVal Transition handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringTransitionReallocatingOutOfLineHandler(VM& vm)
{
    constexpr bool isSymbol = false;
    return putByValTransitionOutOfLineHandlerImpl<isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolTransitionReallocatingOutOfLineHandler(VM& vm)
{
    constexpr bool isSymbol = true;
    return putByValTransitionOutOfLineHandlerImpl<isSymbol>(vm);
}

template<bool isAccessor, bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> putByValCustomHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    using BaselineJITRegisters::PutByVal::scratch1GPR;
    using BaselineJITRegisters::PutByVal::scratch2GPR;
    using BaselineJITRegisters::PutByVal::profileGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    // At this point, we will not go to slow path, so clobbering the other registers are fine.
    // We use propertyJSR for scratch register purpose.
    customSetterHandlerImpl<isAccessor>(vm, jit, baseJSR, valueJSR, stubInfoGPR, scratch1GPR, scratch2GPR, propertyJSR.payloadGPR());
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "PutByVal Custom handler"_s, "PutByVal Custom handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringCustomAccessorHandler(VM& vm)
{
    constexpr bool isAccessor = true;
    constexpr bool isSymbol = false;
    return putByValCustomHandlerImpl<isAccessor, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringCustomValueHandler(VM& vm)
{
    constexpr bool isAccessor = false;
    constexpr bool isSymbol = false;
    return putByValCustomHandlerImpl<isAccessor, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolCustomAccessorHandler(VM& vm)
{
    constexpr bool isAccessor = true;
    constexpr bool isSymbol = true;
    return putByValCustomHandlerImpl<isAccessor, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolCustomValueHandler(VM& vm)
{
    constexpr bool isAccessor = false;
    constexpr bool isSymbol = true;
    return putByValCustomHandlerImpl<isAccessor, isSymbol>(vm);
}

template<bool isStrict, bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> putByValSetterHandlerImpl(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    using BaselineJITRegisters::PutByVal::scratch1GPR;
    using BaselineJITRegisters::PutByVal::scratch2GPR;
    using BaselineJITRegisters::PutByVal::profileGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    setterHandlerImpl<isStrict>(vm, jit, baseJSR, valueJSR, stubInfoGPR, scratch1GPR, scratch2GPR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "PutByVal Setter handler"_s, "PutByVal Setter handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringStrictSetterHandler(VM& vm)
{
    constexpr bool isStrict = true;
    constexpr bool isSymbol = false;
    return putByValSetterHandlerImpl<isStrict, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolStrictSetterHandler(VM& vm)
{
    constexpr bool isStrict = true;
    constexpr bool isSymbol = true;
    return putByValSetterHandlerImpl<isStrict, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringSloppySetterHandler(VM& vm)
{
    constexpr bool isStrict = false;
    constexpr bool isSymbol = false;
    return putByValSetterHandlerImpl<isStrict, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolSloppySetterHandler(VM& vm)
{
    constexpr bool isStrict = false;
    constexpr bool isSymbol = true;
    return putByValSetterHandlerImpl<isStrict, isSymbol>(vm);
}

template<bool hit, bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> inByValInHandlerImpl(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::InByVal::baseJSR;
    using BaselineJITRegisters::InByVal::propertyJSR;
    using BaselineJITRegisters::InByVal::scratch1GPR;
    using BaselineJITRegisters::InByVal::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    jit.boxBoolean(hit, resultJSR);
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "InByVal handler"_s, "InByVal handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> inByValWithStringHitHandler(VM& vm)
{
    constexpr bool hit = true;
    constexpr bool isSymbol = false;
    return inByValInHandlerImpl<hit, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> inByValWithStringMissHandler(VM& vm)
{
    constexpr bool hit = false;
    constexpr bool isSymbol = false;
    return inByValInHandlerImpl<hit, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> inByValWithSymbolHitHandler(VM& vm)
{
    constexpr bool hit = true;
    constexpr bool isSymbol = true;
    return inByValInHandlerImpl<hit, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> inByValWithSymbolMissHandler(VM& vm)
{
    constexpr bool hit = false;
    constexpr bool isSymbol = true;
    return inByValInHandlerImpl<hit, isSymbol>(vm);
}

template<bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValDeleteHandlerImpl(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::DelByVal::baseJSR;
    using BaselineJITRegisters::DelByVal::propertyJSR;
    using BaselineJITRegisters::DelByVal::scratch1GPR;
    using BaselineJITRegisters::DelByVal::scratch2GPR;
    using BaselineJITRegisters::DelByVal::scratch3GPR;
    using BaselineJITRegisters::DelByVal::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    jit.load32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfOffset()), scratch1GPR);
    jit.moveTrustedValue(JSValue(), JSValueRegs { scratch3GPR });
    jit.storeProperty(JSValueRegs { scratch3GPR }, baseJSR.payloadGPR(), scratch1GPR, scratch2GPR);
    jit.transfer32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfNewStructureID()), CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()));

    jit.move(MacroAssembler::TrustedImm32(true), resultJSR.payloadGPR());
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DeleteByVal handler"_s, "DeleteByVal handler");
}

template<bool returnValue, bool isSymbol>
static MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValIgnoreHandlerImpl(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::DelByVal::baseJSR;
    using BaselineJITRegisters::DelByVal::propertyJSR;
    using BaselineJITRegisters::DelByVal::scratch1GPR;
    using BaselineJITRegisters::DelByVal::resultJSR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    jit.move(MacroAssembler::TrustedImm32(returnValue), resultJSR.payloadGPR());
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DeleteByVal handler"_s, "DeleteByVal handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithStringDeleteHandler(VM& vm)
{
    constexpr bool isSymbol = false;
    return deleteByValDeleteHandlerImpl<isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithStringDeleteNonConfigurableHandler(VM& vm)
{
    constexpr bool isSymbol = false;
    constexpr bool resultValue = false;
    return deleteByValIgnoreHandlerImpl<resultValue, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithStringDeleteMissHandler(VM& vm)
{
    constexpr bool isSymbol = false;
    constexpr bool resultValue = true;
    return deleteByValIgnoreHandlerImpl<resultValue, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithSymbolDeleteHandler(VM& vm)
{
    constexpr bool isSymbol = true;
    return deleteByValDeleteHandlerImpl<isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithSymbolDeleteNonConfigurableHandler(VM& vm)
{
    constexpr bool isSymbol = true;
    constexpr bool resultValue = false;
    return deleteByValIgnoreHandlerImpl<resultValue, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithSymbolDeleteMissHandler(VM& vm)
{
    constexpr bool isSymbol = true;
    constexpr bool resultValue = true;
    return deleteByValIgnoreHandlerImpl<resultValue, isSymbol>(vm);
}

MacroAssemblerCodeRef<JITThunkPtrTag> checkPrivateBrandHandler(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PrivateBrand::baseJSR;
    using BaselineJITRegisters::PrivateBrand::propertyJSR;
    using BaselineJITRegisters::PrivateBrand::scratch1GPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    constexpr bool isSymbol = true;
    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "CheckPrivateBrand handler"_s, "CheckPrivateBrand handler");
}

MacroAssemblerCodeRef<JITThunkPtrTag> setPrivateBrandHandler(VM&)
{
    CCallHelpers jit;

    using BaselineJITRegisters::PrivateBrand::baseJSR;
    using BaselineJITRegisters::PrivateBrand::propertyJSR;
    using BaselineJITRegisters::PrivateBrand::scratch1GPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    constexpr bool isSymbol = true;
    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), scratch1GPR));
    fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol, propertyJSR, scratch1GPR));

    jit.transfer32(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfNewStructureID()), CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()));
    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    fallThrough.link(&jit);
    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "SetPrivateBrand handler"_s, "SetPrivateBrand handler");
}

AccessGenerationResult InlineCacheCompiler::compileHandler(const GCSafeConcurrentJSLocker&, Vector<AccessCase*, 16>&& poly, CodeBlock* codeBlock, AccessCase& accessCase)
{
    SuperSamplerScope superSamplerScope(false);

    if (!accessCase.couldStillSucceed())
        return AccessGenerationResult::MadeNoChanges;

    // Now add things to the new list. Note that at this point, we will still have old cases that
    // may be replaced by the new ones. That's fine. We will sort that out when we regenerate.
    auto sets = collectAdditionalWatchpoints(vm(), accessCase);
    for (auto* set : sets) {
        if (!set->isStillValid())
            return AccessGenerationResult::MadeNoChanges;
    }

    for (auto& alreadyListedCase : poly) {
        if (alreadyListedCase != &accessCase) {
            if (alreadyListedCase->canReplace(accessCase))
                return AccessGenerationResult::MadeNoChanges;
        }
    }
    poly.append(&accessCase);
    dataLogLnIf(InlineCacheCompilerInternal::verbose, "Generate with m_list: ", listDump(poly));

    Vector<WatchpointSet*, 8> additionalWatchpointSets;
    if (auto megamorphicCase = tryFoldToMegamorphic(codeBlock, poly.span()))
        return compileOneAccessCaseHandler(poly, codeBlock, *megamorphicCase, WTFMove(additionalWatchpointSets));

    additionalWatchpointSets.appendVector(WTFMove(sets));
    ASSERT(m_stubInfo.useDataIC);
    return compileOneAccessCaseHandler(poly, codeBlock, accessCase, WTFMove(additionalWatchpointSets));
}

AccessGenerationResult InlineCacheCompiler::compileOneAccessCaseHandler(const Vector<AccessCase*, 16>& poly, CodeBlock* codeBlock, AccessCase& accessCase, Vector<WatchpointSet*, 8>&& additionalWatchpointSets)
{
    ASSERT(useHandlerIC());

    VM& vm = this->vm();

    auto connectWatchpointSets = [&](PolymorphicAccessJITStubRoutine::Watchpoints& watchpoints, WatchpointSet& watchpointSet, Vector<ObjectPropertyCondition, 64>&& watchedConditions, Vector<WatchpointSet*, 8>&& additionalWatchpointSets) {
        for (auto& condition : watchedConditions)
            ensureReferenceAndInstallWatchpoint(vm, watchpoints, watchpointSet, condition);

        // NOTE: We currently assume that this is relatively rare. It mainly arises for accesses to
        // properties on DOM nodes. For sure we cache many DOM node accesses, but even in
        // Real Pages (TM), we appear to spend most of our time caching accesses to properties on
        // vanilla objects or exotic objects from within JSC (like Arguments, those are super popular).
        // Those common kinds of JSC object accesses don't hit this case.
        for (WatchpointSet* set : additionalWatchpointSets)
            ensureReferenceAndAddWatchpoint(vm, watchpoints, watchpointSet, *set);
    };

    auto finishPreCompiledCodeGeneration = [&](Ref<PolymorphicAccessJITStubRoutine>&& stub, CacheType cacheType = CacheType::Unset) {
        std::unique_ptr<StructureStubInfoClearingWatchpoint> watchpoint;
        if (!stub->watchpoints().isEmpty()) {
            watchpoint = makeUnique<StructureStubInfoClearingWatchpoint>(codeBlock, m_stubInfo);
            stub->watchpointSet().add(watchpoint.get());
        }

        auto handler = InlineCacheHandler::createPreCompiled(InlineCacheCompiler::generateSlowPathHandler(vm, m_stubInfo.accessType), codeBlock, m_stubInfo, WTFMove(stub), WTFMove(watchpoint), accessCase, cacheType);
        handler->setAccessCase(Ref { accessCase });
        dataLogLnIf(InlineCacheCompilerInternal::verbose, "Returning: ", handler->callTarget());

        AccessGenerationResult::Kind resultKind;
        if (isMegamorphic(accessCase.m_type))
            resultKind = AccessGenerationResult::GeneratedMegamorphicCode;
        else if (poly.size() >= Options::maxAccessVariantListSize())
            resultKind = AccessGenerationResult::GeneratedFinalCode;
        else
            resultKind = AccessGenerationResult::GeneratedNewCode;

        return AccessGenerationResult(resultKind, WTFMove(handler));
    };

    auto finishCodeGeneration = [&](Ref<PolymorphicAccessJITStubRoutine>&& stub, bool doesJSCalls) {
        std::unique_ptr<StructureStubInfoClearingWatchpoint> watchpoint;
        if (!stub->watchpoints().isEmpty()) {
            watchpoint = makeUnique<StructureStubInfoClearingWatchpoint>(codeBlock, m_stubInfo);
            stub->watchpointSet().add(watchpoint.get());
        }

        auto handler = InlineCacheHandler::create(InlineCacheCompiler::generateSlowPathHandler(vm, m_stubInfo.accessType), codeBlock, m_stubInfo, Ref { stub }, WTFMove(watchpoint), doesJSCalls ? 1 : 0);
        ASSERT(!stub->cases().isEmpty());
        handler->setAccessCase(Ref { stub->cases().first() });
        dataLogLnIf(InlineCacheCompilerInternal::verbose, "Returning: ", handler->callTarget());

        AccessGenerationResult::Kind resultKind;
        if (isMegamorphic(accessCase.m_type))
            resultKind = AccessGenerationResult::GeneratedMegamorphicCode;
        else if (poly.size() >= Options::maxAccessVariantListSize())
            resultKind = AccessGenerationResult::GeneratedFinalCode;
        else
            resultKind = AccessGenerationResult::GeneratedNewCode;

        return AccessGenerationResult(resultKind, WTFMove(handler));
    };

    // At this point we're convinced that 'cases' contains cases that we want to JIT now and we won't change that set anymore.

    std::optional<SharedJITStubSet::StatelessCacheKey> statelessType;
    if (isStateless(accessCase.m_type)) {
        statelessType = std::tuple { SharedJITStubSet::stubInfoKey(m_stubInfo), accessCase.m_type };
        if (auto stub = vm.m_sharedJITStubs->getStatelessStub(statelessType.value())) {
            dataLogLnIf(InlineCacheCompilerInternal::verbose, "Using ", m_stubInfo.accessType, " / ", accessCase.m_type);
            return finishPreCompiledCodeGeneration(stub.releaseNonNull());
        }
    } else {
        if (!accessCase.usesPolyProto()) {
            Vector<ObjectPropertyCondition, 64> watchedConditions;
            Vector<ObjectPropertyCondition, 64> checkingConditions;
            switch (m_stubInfo.accessType) {
            case AccessType::GetById:
            case AccessType::TryGetById:
            case AccessType::GetByIdDirect:
            case AccessType::GetPrivateNameById: {
                switch (accessCase.m_type) {
                case AccessCase::GetGetter:
                case AccessCase::Load: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            Structure* currStructure = accessCase.structure();
                            if (auto* object = accessCase.tryGetAlternateBase())
                                currStructure = object->structure();
                            if (isValidOffset(accessCase.m_offset))
                                currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

                            MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                            CacheType cacheType = CacheType::Unset;
                            if (!accessCase.tryGetAlternateBase()) {
                                cacheType = CacheType::GetByIdSelf;
                                code = vm.getCTIStub(CommonJITThunkID::GetByIdLoadOwnPropertyHandler).retagged<JITStubRoutinePtrTag>();
                            } else {
                                cacheType = CacheType::GetByIdPrototype;
                                code = vm.getCTIStub(CommonJITThunkID::GetByIdLoadPrototypePropertyHandler).retagged<JITStubRoutinePtrTag>();
                            }
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub), cacheType);
                        }
                    }
                    break;
                }
                case AccessCase::Miss: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            auto code = vm.getCTIStub(CommonJITThunkID::GetByIdMissHandler).retagged<JITStubRoutinePtrTag>();
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                case AccessCase::CustomAccessorGetter:
                case AccessCase::CustomValueGetter: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        auto& access = accessCase.as<GetterSetterAccessCase>();
                        if (accessCase.m_type == AccessCase::CustomAccessorGetter && access.domAttribute()) {
                            // We do not need to emit CheckDOM operation since structure check ensures
                            // that the structure of the given base value is accessCase.structure()! So all we should
                            // do is performing the CheckDOM thingy in IC compiling time here.
                            if (!accessCase.structure()->classInfoForCells()->isSubClassOf(access.domAttribute()->classInfo))
                                break;
                        }

                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            Structure* currStructure = accessCase.structure();
                            if (auto* object = accessCase.tryGetAlternateBase())
                                currStructure = object->structure();
                            if (isValidOffset(accessCase.m_offset))
                                currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

                            MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                            if (accessCase.m_type == AccessCase::CustomAccessorGetter) {
                                if (Options::useDOMJIT() && access.domAttribute() && access.domAttribute()->domJIT) {
                                    code = compileGetByDOMJITHandler(codeBlock, access.domAttribute()->domJIT, std::nullopt);
                                    if (!code)
                                        return AccessGenerationResult::GaveUp;
                                } else
                                    code = vm.getCTIStub(CommonJITThunkID::GetByIdCustomAccessorHandler).retagged<JITStubRoutinePtrTag>();
                            } else
                                code = vm.getCTIStub(CommonJITThunkID::GetByIdCustomValueHandler).retagged<JITStubRoutinePtrTag>();
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                case AccessCase::Getter: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            Structure* currStructure = accessCase.structure();
                            if (auto* object = accessCase.tryGetAlternateBase())
                                currStructure = object->structure();
                            if (isValidOffset(accessCase.m_offset))
                                currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

                            auto code = vm.getCTIStub(CommonJITThunkID::GetByIdGetterHandler).retagged<JITStubRoutinePtrTag>();
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                case AccessCase::ProxyObjectLoad: {
                    ASSERT(!accessCase.viaGlobalProxy());
                    ASSERT(accessCase.conditionSet().isEmpty());
                    auto code = vm.getCTIStub(CommonJITThunkID::GetByIdProxyObjectLoadHandler).retagged<JITStubRoutinePtrTag>();
                    auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                    connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), { }, WTFMove(additionalWatchpointSets));
                    return finishPreCompiledCodeGeneration(WTFMove(stub));
                }
                case AccessCase::IntrinsicGetter:
                    break;
                case AccessCase::ModuleNamespaceLoad: {
                    ASSERT(!accessCase.viaGlobalProxy());
                    ASSERT(accessCase.conditionSet().isEmpty());
                    auto code = vm.getCTIStub(CommonJITThunkID::GetByIdModuleNamespaceLoadHandler).retagged<JITStubRoutinePtrTag>();
                    auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                    connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), { }, WTFMove(additionalWatchpointSets));
                    return finishPreCompiledCodeGeneration(WTFMove(stub));
                }
                default:
                    break;
                }
                break;
            }

            case AccessType::PutByIdDirectStrict:
            case AccessType::PutByIdStrict:
            case AccessType::PutByIdSloppy:
            case AccessType::PutByIdDirectSloppy:
            case AccessType::DefinePrivateNameById:
            case AccessType::SetPrivateNameById: {
                bool isStrict = m_stubInfo.accessType == AccessType::PutByIdDirectStrict || m_stubInfo.accessType == AccessType::PutByIdStrict || m_stubInfo.accessType == AccessType::DefinePrivateNameById || m_stubInfo.accessType == AccessType::SetPrivateNameById;
                switch (accessCase.m_type) {
                case AccessCase::Replace: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    ASSERT(accessCase.conditionSet().isEmpty());
                    if (!accessCase.viaGlobalProxy()) {
                        auto code = vm.getCTIStub(CommonJITThunkID::PutByIdReplaceHandler).retagged<JITStubRoutinePtrTag>();
                        auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                        connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), { }, { });
                        return finishPreCompiledCodeGeneration(WTFMove(stub), CacheType::PutByIdReplace);
                    }
                    break;
                }
                case AccessCase::Transition: {
                    ASSERT(!accessCase.viaGlobalProxy());
                    bool allocating = accessCase.newStructure()->outOfLineCapacity() != accessCase.structure()->outOfLineCapacity();
                    bool reallocating = allocating && accessCase.structure()->outOfLineCapacity();
                    bool allocatingInline = allocating && !accessCase.structure()->couldHaveIndexingHeader();
                    collectConditions(accessCase, watchedConditions, checkingConditions);
                    if (checkingConditions.isEmpty()) {
                        MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                        if (!allocating)
                            code = vm.getCTIStub(CommonJITThunkID::PutByIdTransitionNonAllocatingHandler).retagged<JITStubRoutinePtrTag>();
                        else if (!allocatingInline)
                            code = vm.getCTIStub(CommonJITThunkID::PutByIdTransitionReallocatingOutOfLineHandler).retagged<JITStubRoutinePtrTag>();
                        else if (!reallocating)
                            code = vm.getCTIStub(CommonJITThunkID::PutByIdTransitionNewlyAllocatingHandler).retagged<JITStubRoutinePtrTag>();
                        else
                            code = vm.getCTIStub(CommonJITThunkID::PutByIdTransitionReallocatingHandler).retagged<JITStubRoutinePtrTag>();
                        auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                        connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                        return finishPreCompiledCodeGeneration(WTFMove(stub));
                    }
                    break;
                }
                case AccessCase::CustomAccessorSetter:
                case AccessCase::CustomValueSetter: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            Structure* currStructure = accessCase.structure();
                            if (auto* object = accessCase.tryGetAlternateBase())
                                currStructure = object->structure();
                            if (isValidOffset(accessCase.m_offset))
                                currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

                            MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                            if (accessCase.m_type == AccessCase::CustomAccessorSetter)
                                code = vm.getCTIStub(CommonJITThunkID::PutByIdCustomAccessorHandler).retagged<JITStubRoutinePtrTag>();
                            else
                                code = vm.getCTIStub(CommonJITThunkID::PutByIdCustomValueHandler).retagged<JITStubRoutinePtrTag>();
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                case AccessCase::Setter: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            Structure* currStructure = accessCase.structure();
                            if (auto* object = accessCase.tryGetAlternateBase())
                                currStructure = object->structure();
                            if (isValidOffset(accessCase.m_offset))
                                currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

                            MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                            if (isStrict)
                                code = vm.getCTIStub(CommonJITThunkID::PutByIdStrictSetterHandler).retagged<JITStubRoutinePtrTag>();
                            else
                                code = vm.getCTIStub(CommonJITThunkID::PutByIdSloppySetterHandler).retagged<JITStubRoutinePtrTag>();
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                default:
                    break;
                }
                break;
            }

            case AccessType::InById: {
                switch (accessCase.m_type) {
                case AccessCase::InHit:
                case AccessCase::InMiss: {
                    ASSERT(!accessCase.viaGlobalProxy());
                    collectConditions(accessCase, watchedConditions, checkingConditions);
                    if (checkingConditions.isEmpty()) {
                        bool isHit = accessCase.m_type == AccessCase::InHit;
                        auto code = vm.getCTIStub(isHit ? CommonJITThunkID::InByIdHitHandler : CommonJITThunkID::InByIdMissHandler).retagged<JITStubRoutinePtrTag>();
                        auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                        connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                        return finishPreCompiledCodeGeneration(WTFMove(stub), isHit ? CacheType::InByIdSelf : CacheType::Unset);
                    }
                    break;
                }
                default:
                    break;
                }
                break;
            }

            case AccessType::DeleteByIdStrict:
            case AccessType::DeleteByIdSloppy: {
                switch (accessCase.m_type) {
                case AccessCase::Delete:
                case AccessCase::DeleteNonConfigurable:
                case AccessCase::DeleteMiss: {
                    ASSERT(!accessCase.viaGlobalProxy());
                    ASSERT(accessCase.conditionSet().isEmpty());
                    CommonJITThunkID thunkID = CommonJITThunkID::DeleteByIdDeleteHandler;
                    switch (accessCase.m_type) {
                    case AccessCase::Delete:
                        thunkID = CommonJITThunkID::DeleteByIdDeleteHandler;
                        break;
                    case AccessCase::DeleteNonConfigurable:
                        thunkID = CommonJITThunkID::DeleteByIdDeleteNonConfigurableHandler;
                        break;
                    case AccessCase::DeleteMiss:
                        thunkID = CommonJITThunkID::DeleteByIdDeleteMissHandler;
                        break;
                    default:
                        break;
                    }
                    auto code = vm.getCTIStub(thunkID).retagged<JITStubRoutinePtrTag>();
                    auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                    connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), { }, WTFMove(additionalWatchpointSets));
                    return finishPreCompiledCodeGeneration(WTFMove(stub));
                }
                default:
                    break;
                }
                break;
            }

            case AccessType::InstanceOf: {
                switch (accessCase.m_type) {
                case AccessCase::InstanceOfHit:
                case AccessCase::InstanceOfMiss: {
                    ASSERT(!accessCase.viaGlobalProxy());
                    collectConditions(accessCase, watchedConditions, checkingConditions);
                    if (checkingConditions.isEmpty()) {
                        auto code = vm.getCTIStub(accessCase.m_type == AccessCase::InstanceOfHit ? CommonJITThunkID::InstanceOfHitHandler : CommonJITThunkID::InstanceOfMissHandler).retagged<JITStubRoutinePtrTag>();
                        auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                        connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                        return finishPreCompiledCodeGeneration(WTFMove(stub));
                    }
                    break;
                }
                default:
                    break;
                }
                break;
            }

            case AccessType::GetByVal:
            case AccessType::GetPrivateName: {
                switch (accessCase.m_type) {
                case AccessCase::GetGetter:
                case AccessCase::Load: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            Structure* currStructure = accessCase.structure();
                            if (auto* object = accessCase.tryGetAlternateBase())
                                currStructure = object->structure();
                            if (isValidOffset(accessCase.m_offset))
                                currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

                            MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                            if (!accessCase.tryGetAlternateBase()) {
                                if (accessCase.uid()->isSymbol())
                                    code = vm.getCTIStub(CommonJITThunkID::GetByValWithSymbolLoadOwnPropertyHandler).retagged<JITStubRoutinePtrTag>();
                                else
                                    code = vm.getCTIStub(CommonJITThunkID::GetByValWithStringLoadOwnPropertyHandler).retagged<JITStubRoutinePtrTag>();
                            } else {
                                if (accessCase.uid()->isSymbol())
                                    code = vm.getCTIStub(CommonJITThunkID::GetByValWithSymbolLoadPrototypePropertyHandler).retagged<JITStubRoutinePtrTag>();
                                else
                                    code = vm.getCTIStub(CommonJITThunkID::GetByValWithStringLoadPrototypePropertyHandler).retagged<JITStubRoutinePtrTag>();
                            }
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                case AccessCase::Miss: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                            if (accessCase.uid()->isSymbol())
                                code = vm.getCTIStub(CommonJITThunkID::GetByValWithSymbolMissHandler).retagged<JITStubRoutinePtrTag>();
                            else
                                code = vm.getCTIStub(CommonJITThunkID::GetByValWithStringMissHandler).retagged<JITStubRoutinePtrTag>();
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                case AccessCase::CustomAccessorGetter:
                case AccessCase::CustomValueGetter: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        auto& access = accessCase.as<GetterSetterAccessCase>();
                        if (accessCase.m_type == AccessCase::CustomAccessorGetter && access.domAttribute()) {
                            // We do not need to emit CheckDOM operation since structure check ensures
                            // that the structure of the given base value is accessCase.structure()! So all we should
                            // do is performing the CheckDOM thingy in IC compiling time here.
                            if (!accessCase.structure()->classInfoForCells()->isSubClassOf(access.domAttribute()->classInfo))
                                break;
                        }

                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            Structure* currStructure = accessCase.structure();
                            if (auto* object = accessCase.tryGetAlternateBase())
                                currStructure = object->structure();
                            if (isValidOffset(accessCase.m_offset))
                                currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

                            MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                            if (accessCase.m_type == AccessCase::CustomAccessorGetter) {
                                if (Options::useDOMJIT() && access.domAttribute() && access.domAttribute()->domJIT) {
                                    code = compileGetByDOMJITHandler(codeBlock, access.domAttribute()->domJIT, accessCase.uid()->isSymbol());
                                    if (!code)
                                        return AccessGenerationResult::GaveUp;
                                } else {
                                    if (accessCase.uid()->isSymbol())
                                        code = vm.getCTIStub(CommonJITThunkID::GetByValWithSymbolCustomAccessorHandler).retagged<JITStubRoutinePtrTag>();
                                    else
                                        code = vm.getCTIStub(CommonJITThunkID::GetByValWithStringCustomAccessorHandler).retagged<JITStubRoutinePtrTag>();
                                }
                            } else {
                                if (accessCase.uid()->isSymbol())
                                    code = vm.getCTIStub(CommonJITThunkID::GetByValWithSymbolCustomValueHandler).retagged<JITStubRoutinePtrTag>();
                                else
                                    code = vm.getCTIStub(CommonJITThunkID::GetByValWithStringCustomValueHandler).retagged<JITStubRoutinePtrTag>();
                            }
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                case AccessCase::Getter: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            Structure* currStructure = accessCase.structure();
                            if (auto* object = accessCase.tryGetAlternateBase())
                                currStructure = object->structure();
                            if (isValidOffset(accessCase.m_offset))
                                currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());
                            auto code = vm.getCTIStub(accessCase.uid()->isSymbol() ? CommonJITThunkID::GetByValWithSymbolGetterHandler : CommonJITThunkID::GetByValWithStringGetterHandler).retagged<JITStubRoutinePtrTag>();
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                default:
                    break;
                }
                break;
            }

            case AccessType::PutByValDirectStrict:
            case AccessType::PutByValStrict:
            case AccessType::PutByValSloppy:
            case AccessType::PutByValDirectSloppy:
            case AccessType::DefinePrivateNameByVal:
            case AccessType::SetPrivateNameByVal: {
                bool isStrict = m_stubInfo.accessType == AccessType::PutByValDirectStrict || m_stubInfo.accessType == AccessType::PutByValStrict || m_stubInfo.accessType == AccessType::DefinePrivateNameByVal || m_stubInfo.accessType == AccessType::SetPrivateNameByVal;
                switch (accessCase.m_type) {
                case AccessCase::Replace: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    ASSERT(accessCase.conditionSet().isEmpty());
                    if (!accessCase.viaGlobalProxy()) {
                        MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                        if (accessCase.uid()->isSymbol())
                            code = vm.getCTIStub(CommonJITThunkID::PutByValWithSymbolReplaceHandler).retagged<JITStubRoutinePtrTag>();
                        else
                            code = vm.getCTIStub(CommonJITThunkID::PutByValWithStringReplaceHandler).retagged<JITStubRoutinePtrTag>();
                        auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                        connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), { }, WTFMove(additionalWatchpointSets));
                        return finishPreCompiledCodeGeneration(WTFMove(stub));
                    }
                    break;
                }
                case AccessCase::Transition: {
                    ASSERT(!accessCase.viaGlobalProxy());
                    bool allocating = accessCase.newStructure()->outOfLineCapacity() != accessCase.structure()->outOfLineCapacity();
                    bool reallocating = allocating && accessCase.structure()->outOfLineCapacity();
                    bool allocatingInline = allocating && !accessCase.structure()->couldHaveIndexingHeader();
                    collectConditions(accessCase, watchedConditions, checkingConditions);
                    if (checkingConditions.isEmpty()) {
                        MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                        if (!allocating) {
                            if (accessCase.uid()->isSymbol())
                                code = vm.getCTIStub(CommonJITThunkID::PutByValWithSymbolTransitionNonAllocatingHandler).retagged<JITStubRoutinePtrTag>();
                            else
                                code = vm.getCTIStub(CommonJITThunkID::PutByValWithStringTransitionNonAllocatingHandler).retagged<JITStubRoutinePtrTag>();
                        } else if (!allocatingInline) {
                            if (accessCase.uid()->isSymbol())
                                code = vm.getCTIStub(CommonJITThunkID::PutByValWithSymbolTransitionReallocatingOutOfLineHandler).retagged<JITStubRoutinePtrTag>();
                            else
                                code = vm.getCTIStub(CommonJITThunkID::PutByValWithStringTransitionReallocatingOutOfLineHandler).retagged<JITStubRoutinePtrTag>();
                        } else if (!reallocating) {
                            if (accessCase.uid()->isSymbol())
                                code = vm.getCTIStub(CommonJITThunkID::PutByValWithSymbolTransitionNewlyAllocatingHandler).retagged<JITStubRoutinePtrTag>();
                            else
                                code = vm.getCTIStub(CommonJITThunkID::PutByValWithStringTransitionNewlyAllocatingHandler).retagged<JITStubRoutinePtrTag>();
                        } else {
                            if (accessCase.uid()->isSymbol())
                                code = vm.getCTIStub(CommonJITThunkID::PutByValWithSymbolTransitionReallocatingHandler).retagged<JITStubRoutinePtrTag>();
                            else
                                code = vm.getCTIStub(CommonJITThunkID::PutByValWithStringTransitionReallocatingHandler).retagged<JITStubRoutinePtrTag>();
                        }
                        auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                        connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                        return finishPreCompiledCodeGeneration(WTFMove(stub));
                    }
                    break;
                }
                case AccessCase::CustomAccessorSetter:
                case AccessCase::CustomValueSetter: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            Structure* currStructure = accessCase.structure();
                            if (auto* object = accessCase.tryGetAlternateBase())
                                currStructure = object->structure();
                            if (isValidOffset(accessCase.m_offset))
                                currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

                            MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                            if (accessCase.m_type == AccessCase::CustomAccessorSetter) {
                                if (accessCase.uid()->isSymbol())
                                    code = vm.getCTIStub(CommonJITThunkID::PutByValWithSymbolCustomAccessorHandler).retagged<JITStubRoutinePtrTag>();
                                else
                                    code = vm.getCTIStub(CommonJITThunkID::PutByValWithStringCustomAccessorHandler).retagged<JITStubRoutinePtrTag>();
                            } else {
                                if (accessCase.uid()->isSymbol())
                                    code = vm.getCTIStub(CommonJITThunkID::PutByValWithSymbolCustomValueHandler).retagged<JITStubRoutinePtrTag>();
                                else
                                    code = vm.getCTIStub(CommonJITThunkID::PutByValWithStringCustomValueHandler).retagged<JITStubRoutinePtrTag>();
                            }
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                case AccessCase::Setter: {
                    ASSERT(canBeViaGlobalProxy(accessCase.m_type));
                    if (!accessCase.viaGlobalProxy()) {
                        collectConditions(accessCase, watchedConditions, checkingConditions);
                        if (checkingConditions.isEmpty()) {
                            Structure* currStructure = accessCase.structure();
                            if (auto* object = accessCase.tryGetAlternateBase())
                                currStructure = object->structure();
                            if (isValidOffset(accessCase.m_offset))
                                currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

                            MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                            if (isStrict)
                                code = vm.getCTIStub(accessCase.uid()->isSymbol() ? CommonJITThunkID::PutByValWithSymbolStrictSetterHandler : CommonJITThunkID::PutByValWithStringStrictSetterHandler).retagged<JITStubRoutinePtrTag>();
                            else
                                code = vm.getCTIStub(accessCase.uid()->isSymbol() ? CommonJITThunkID::PutByValWithSymbolSloppySetterHandler : CommonJITThunkID::PutByValWithStringSloppySetterHandler).retagged<JITStubRoutinePtrTag>();
                            auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                            connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                            return finishPreCompiledCodeGeneration(WTFMove(stub));
                        }
                    }
                    break;
                }
                default:
                    break;
                }
                break;
            }

            case AccessType::InByVal:
            case AccessType::HasPrivateBrand:
            case AccessType::HasPrivateName: {
                switch (accessCase.m_type) {
                case AccessCase::InHit:
                case AccessCase::InMiss: {
                    ASSERT(!accessCase.viaGlobalProxy());
                    collectConditions(accessCase, watchedConditions, checkingConditions);
                    if (checkingConditions.isEmpty()) {
                        MacroAssemblerCodeRef<JITStubRoutinePtrTag> code;
                        if (accessCase.uid()->isSymbol())
                            code = vm.getCTIStub(accessCase.m_type == AccessCase::InHit ? CommonJITThunkID::InByValWithSymbolHitHandler : CommonJITThunkID::InByValWithSymbolMissHandler).retagged<JITStubRoutinePtrTag>();
                        else
                            code = vm.getCTIStub(accessCase.m_type == AccessCase::InHit ? CommonJITThunkID::InByValWithStringHitHandler : CommonJITThunkID::InByValWithStringMissHandler).retagged<JITStubRoutinePtrTag>();
                        auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                        connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(watchedConditions), WTFMove(additionalWatchpointSets));
                        return finishPreCompiledCodeGeneration(WTFMove(stub));
                    }
                    break;
                }
                default:
                    break;
                }
                break;
            }

            case AccessType::DeleteByValStrict:
            case AccessType::DeleteByValSloppy: {
                switch (accessCase.m_type) {
                case AccessCase::Delete:
                case AccessCase::DeleteNonConfigurable:
                case AccessCase::DeleteMiss: {
                    ASSERT(!accessCase.viaGlobalProxy());
                    ASSERT(accessCase.conditionSet().isEmpty());
                    CommonJITThunkID thunkID = CommonJITThunkID::DeleteByValWithStringDeleteHandler;
                    switch (accessCase.m_type) {
                    case AccessCase::Delete:
                        thunkID = accessCase.uid()->isSymbol() ? CommonJITThunkID::DeleteByValWithSymbolDeleteHandler : CommonJITThunkID::DeleteByValWithStringDeleteHandler;
                        break;
                    case AccessCase::DeleteNonConfigurable:
                        thunkID = accessCase.uid()->isSymbol() ? CommonJITThunkID::DeleteByValWithSymbolDeleteNonConfigurableHandler : CommonJITThunkID::DeleteByValWithStringDeleteNonConfigurableHandler;
                        break;
                    case AccessCase::DeleteMiss:
                        thunkID = accessCase.uid()->isSymbol() ? CommonJITThunkID::DeleteByValWithSymbolDeleteMissHandler : CommonJITThunkID::DeleteByValWithStringDeleteMissHandler;
                        break;
                    default:
                        break;
                    }
                    auto code = vm.getCTIStub(thunkID).retagged<JITStubRoutinePtrTag>();
                    auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                    connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), { }, WTFMove(additionalWatchpointSets));
                    return finishPreCompiledCodeGeneration(WTFMove(stub));
                }
                default:
                    break;
                }
                break;
            }

            case AccessType::CheckPrivateBrand:
            case AccessType::SetPrivateBrand: {
                switch (accessCase.m_type) {
                case AccessCase::CheckPrivateBrand:
                case AccessCase::SetPrivateBrand: {
                    ASSERT(!accessCase.viaGlobalProxy());
                    ASSERT(accessCase.conditionSet().isEmpty());
                    CommonJITThunkID thunkID = CommonJITThunkID::CheckPrivateBrandHandler;
                    switch (accessCase.m_type) {
                    case AccessCase::CheckPrivateBrand:
                        thunkID = CommonJITThunkID::CheckPrivateBrandHandler;
                        break;
                    case AccessCase::SetPrivateBrand:
                        thunkID = CommonJITThunkID::SetPrivateBrandHandler;
                        break;
                    default:
                        break;
                    }
                    auto code = vm.getCTIStub(thunkID).retagged<JITStubRoutinePtrTag>();
                    auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
                    connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), { }, WTFMove(additionalWatchpointSets));
                    return finishPreCompiledCodeGeneration(WTFMove(stub));
                }
                default:
                    break;
                }
                break;
            }

            default:
                break;
            }
        }

        SharedJITStubSet::Searcher searcher {
            SharedJITStubSet::stubInfoKey(m_stubInfo),
            Ref { accessCase }
        };
        if (auto stub = vm.m_sharedJITStubs->find(searcher)) {
            if (stub->isStillValid()) {
                dataLogLnIf(InlineCacheCompilerInternal::verbose, "Using ", m_stubInfo.accessType, " / ", listDump(stub->cases()));
                return finishCodeGeneration(stub.releaseNonNull(), JSC::doesJSCalls(accessCase.m_type));
            }
            vm.m_sharedJITStubs->remove(stub.get());
        }
    }

    auto allocator = makeDefaultScratchAllocator();
    m_allocator = &allocator;
    m_scratchGPR = allocator.allocateScratchGPR();
    if (needsScratchFPR(accessCase.m_type))
        m_scratchFPR = allocator.allocateScratchFPR();

    Vector<JSCell*> cellsToMark;
    bool doesCalls = accessCase.doesCalls(vm);
    if (doesCalls)
        accessCase.collectDependentCells(vm, cellsToMark);

    CCallHelpers jit(codeBlock);
    m_jit = &jit;

    emitDataICPrologue(*m_jit);

    m_preservedReusedRegisterState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

    // If there are any proxies in the list, we cannot just use a binary switch over the structure.
    // We need to resort to a cascade. A cascade also happens to be optimal if we only have just
    // one case.
    CCallHelpers::JumpList fallThrough;
    if (!JSC::hasConstantIdentifier(m_stubInfo.accessType)) {
        if (accessCase.requiresInt32PropertyCheck()) {
            CCallHelpers::JumpList notInt32;
            if (!m_stubInfo.propertyIsInt32) {
#if USE(JSVALUE64)
                notInt32.append(jit.branchIfNotInt32(m_stubInfo.propertyGPR()));
#else
                notInt32.append(jit.branchIfNotInt32(m_stubInfo.propertyTagGPR()));
#endif
            }
            m_failAndRepatch.append(notInt32);
        } else if (accessCase.requiresIdentifierNameMatch() && !accessCase.uid()->isSymbol()) {
            CCallHelpers::JumpList notString;
            GPRReg propertyGPR = m_stubInfo.propertyGPR();
            if (!m_stubInfo.propertyIsString) {
#if USE(JSVALUE32_64)
                GPRReg propertyTagGPR = m_stubInfo.propertyTagGPR();
                notString.append(jit.branchIfNotCell(propertyTagGPR));
#else
                notString.append(jit.branchIfNotCell(propertyGPR));
#endif
                notString.append(jit.branchIfNotString(propertyGPR));
            }
            jit.loadPtr(MacroAssembler::Address(propertyGPR, JSString::offsetOfValue()), m_scratchGPR);
            m_failAndRepatch.append(jit.branchIfRopeStringImpl(m_scratchGPR));
            m_failAndRepatch.append(notString);
        } else if (accessCase.requiresIdentifierNameMatch() && accessCase.uid()->isSymbol()) {
            CCallHelpers::JumpList notSymbol;
            if (!m_stubInfo.propertyIsSymbol) {
                GPRReg propertyGPR = m_stubInfo.propertyGPR();
#if USE(JSVALUE32_64)
                GPRReg propertyTagGPR = m_stubInfo.propertyTagGPR();
                notSymbol.append(jit.branchIfNotCell(propertyTagGPR));
#else
                notSymbol.append(jit.branchIfNotCell(propertyGPR));
#endif
                notSymbol.append(jit.branchIfNotSymbol(propertyGPR));
            }
            m_failAndRepatch.append(notSymbol);
        }
    }

    generateWithGuard(0, accessCase, fallThrough);
    m_failAndRepatch.append(fallThrough);

    if (!m_failAndIgnore.empty()) {
        m_failAndIgnore.link(&jit);
        JIT_COMMENT(jit, "failAndIgnore");

        // Make sure that the inline cache optimization code knows that we are taking slow path because
        // of something that isn't patchable. The slow path will decrement "countdown" and will only
        // patch things if the countdown reaches zero. We increment the slow path count here to ensure
        // that the slow path does not try to patch.
        jit.add8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCountdown()));
    }

    m_failAndRepatch.link(&jit);
    if (allocator.didReuseRegisters())
        restoreScratch();

    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer linkBuffer(jit, codeBlock, LinkBuffer::Profile::InlineCache, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) {
        dataLogLnIf(InlineCacheCompilerInternal::verbose, "Did fail to allocate.");
        return AccessGenerationResult::GaveUp;
    }

    ASSERT(m_success.empty());

    auto keys = FixedVector<Ref<AccessCase>>::createWithSizeFromGenerator(1,
        [&](unsigned) {
            return std::optional { Ref { accessCase } };
        });
    dataLogLnIf(InlineCacheCompilerInternal::verbose, FullCodeOrigin(codeBlock, m_stubInfo.codeOrigin), ": Generating polymorphic access stub for ", listDump(keys));

    MacroAssemblerCodeRef<JITStubRoutinePtrTag> code = FINALIZE_CODE_FOR(codeBlock, linkBuffer, JITStubRoutinePtrTag, categoryName(m_stubInfo.accessType), "%s", toCString("Access stub for ", *codeBlock, " ", m_stubInfo.codeOrigin, "with start: ", m_stubInfo.startLocation, ": ", listDump(keys)).data());

    if (statelessType) {
        auto stub = createPreCompiledICJITStubRoutine(WTFMove(code), vm);
        connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(m_conditions), WTFMove(additionalWatchpointSets));
        dataLogLnIf(InlineCacheCompilerInternal::verbose, "Installing ", m_stubInfo.accessType, " / ", accessCase.m_type);
        vm.m_sharedJITStubs->setStatelessStub(statelessType.value(), Ref { stub });
        return finishPreCompiledCodeGeneration(WTFMove(stub));
    }

    FixedVector<StructureID> weakStructures(WTFMove(m_weakStructures));
    auto stub = createICJITStubRoutine(WTFMove(code), WTFMove(keys), WTFMove(weakStructures), vm, nullptr, doesCalls, cellsToMark, { }, nullptr, { });
    connectWatchpointSets(stub->watchpoints(), stub->watchpointSet(), WTFMove(m_conditions), WTFMove(additionalWatchpointSets));

    dataLogLnIf(InlineCacheCompilerInternal::verbose, "Installing ", m_stubInfo.accessType, " / ", listDump(stub->cases()));
    vm.m_sharedJITStubs->add(SharedJITStubSet::Hash::Key(SharedJITStubSet::stubInfoKey(m_stubInfo), stub.ptr()));
    stub->addedToSharedJITStubSet();

    return finishCodeGeneration(WTFMove(stub), JSC::doesJSCalls(accessCase.m_type));
}

MacroAssemblerCodeRef<JITStubRoutinePtrTag> InlineCacheCompiler::compileGetByDOMJITHandler(CodeBlock* codeBlock, const DOMJIT::GetterSetter* domJIT, std::optional<bool> isSymbol)
{
    VM& vm = codeBlock->vm();
    ASSERT(useHandlerIC());

    static_assert(BaselineJITRegisters::GetById::baseJSR == BaselineJITRegisters::GetByVal::baseJSR);
    static_assert(BaselineJITRegisters::GetById::resultJSR == BaselineJITRegisters::GetByVal::resultJSR);
    using BaselineJITRegisters::GetById::baseJSR;

    auto cacheKey = std::tuple { SharedJITStubSet::stubInfoKey(m_stubInfo), domJIT };
    if (auto code = vm.m_sharedJITStubs->getDOMJITCode(cacheKey))
        return code;

    auto allocator = makeDefaultScratchAllocator();
    m_allocator = &allocator;
    m_scratchGPR = allocator.allocateScratchGPR();

    CCallHelpers jit(codeBlock);
    m_jit = &jit;

    InlineCacheCompiler::emitDataICPrologue(jit);

    CCallHelpers::JumpList fallThrough;

    m_preservedReusedRegisterState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

    fallThrough.append(InlineCacheCompiler::emitDataICCheckStructure(jit, baseJSR.payloadGPR(), m_scratchGPR));
    if (isSymbol)
        fallThrough.append(InlineCacheCompiler::emitDataICCheckUid(jit, isSymbol.value(), BaselineJITRegisters::GetByVal::propertyJSR, m_scratchGPR));

    emitDOMJITGetter(nullptr, domJIT, baseJSR.payloadGPR());

    m_failAndRepatch.append(fallThrough);
    if (!m_failAndIgnore.empty()) {
        m_failAndIgnore.link(&jit);
        JIT_COMMENT(jit, "failAndIgnore");

        // Make sure that the inline cache optimization code knows that we are taking slow path because
        // of something that isn't patchable. The slow path will decrement "countdown" and will only
        // patch things if the countdown reaches zero. We increment the slow path count here to ensure
        // that the slow path does not try to patch.
        jit.add8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(m_stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCountdown()));
    }

    m_failAndRepatch.link(&jit);
    if (allocator.didReuseRegisters())
        restoreScratch();

    InlineCacheCompiler::emitDataICJumpNextHandler(jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::InlineCache, JITCompilationCanFail);
    if (patchBuffer.didFailToAllocate()) {
        dataLogLnIf(InlineCacheCompilerInternal::verbose, "Did fail to allocate.");
        return { };
    }

    auto code = FINALIZE_THUNK(patchBuffer, JITStubRoutinePtrTag, "GetById DOMJIT handler"_s, "GetById DOMJIT handler");
    vm.m_sharedJITStubs->setDOMJITCode(cacheKey, code);
    return code;
}

#else
MacroAssemblerCodeRef<JITThunkPtrTag> getByIdLoadOwnPropertyHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByIdLoadPrototypePropertyHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByIdMissHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByIdCustomAccessorHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByIdCustomValueHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByIdGetterHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByIdProxyObjectLoadHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByIdModuleNamespaceLoadHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByIdReplaceHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByIdTransitionNonAllocatingHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByIdTransitionNewlyAllocatingHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByIdTransitionReallocatingHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByIdTransitionReallocatingOutOfLineHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByIdCustomAccessorHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByIdCustomValueHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByIdStrictSetterHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByIdSloppySetterHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> inByIdHitHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> inByIdMissHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> deleteByIdDeleteHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> deleteByIdDeleteNonConfigurableHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> deleteByIdDeleteMissHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> instanceOfHitHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> instanceOfMissHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringLoadOwnPropertyHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringLoadPrototypePropertyHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringMissHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringCustomAccessorHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringCustomValueHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithStringGetterHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolLoadOwnPropertyHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolLoadPrototypePropertyHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolMissHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolCustomAccessorHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolCustomValueHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithSymbolGetterHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringReplaceHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringTransitionNonAllocatingHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringTransitionNewlyAllocatingHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringTransitionReallocatingHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringTransitionReallocatingOutOfLineHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringCustomAccessorHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringCustomValueHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringStrictSetterHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithStringSloppySetterHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolReplaceHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolTransitionNonAllocatingHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolTransitionNewlyAllocatingHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolTransitionReallocatingHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolTransitionReallocatingOutOfLineHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolCustomAccessorHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolCustomValueHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolStrictSetterHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> putByValWithSymbolSloppySetterHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> inByValWithStringHitHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> inByValWithStringMissHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> inByValWithSymbolHitHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> inByValWithSymbolMissHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithStringDeleteHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithStringDeleteNonConfigurableHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithStringDeleteMissHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithSymbolDeleteHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithSymbolDeleteNonConfigurableHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> deleteByValWithSymbolDeleteMissHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> checkPrivateBrandHandler(VM&) { return { }; }
MacroAssemblerCodeRef<JITThunkPtrTag> setPrivateBrandHandler(VM&) { return { }; }
AccessGenerationResult InlineCacheCompiler::compileHandler(const GCSafeConcurrentJSLocker&, Vector<AccessCase*, 16>&&, CodeBlock*, AccessCase&) { return { }; }
#endif

PolymorphicAccess::PolymorphicAccess() = default;
PolymorphicAccess::~PolymorphicAccess() = default;

AccessGenerationResult PolymorphicAccess::addCases(const GCSafeConcurrentJSLocker&, VM& vm, CodeBlock*, StructureStubInfo& stubInfo, RefPtr<AccessCase>&& previousCase, Ref<AccessCase> accessCase)
{
    SuperSamplerScope superSamplerScope(false);

    // This method will add the casesToAdd to the list one at a time while preserving the
    // invariants:
    // - If a newly added case canReplace() any existing case, then the existing case is removed before
    //   the new case is added. Removal doesn't change order of the list. Any number of existing cases
    //   can be removed via the canReplace() rule.
    // - Cases in the list always appear in ascending order of time of addition. Therefore, if you
    //   cascade through the cases in reverse order, you will get the most recent cases first.
    // - If this method fails (returns null, doesn't add the cases), then both the previous case list
    //   and the previous stub are kept intact and the new cases are destroyed. It's OK to attempt to
    //   add more things after failure.

    // First ensure that the casesToAdd doesn't contain duplicates.

    ListType casesToAdd;
    if (previousCase) {
        auto previous = previousCase.releaseNonNull();
        if (previous->canReplace(accessCase.get()))
            casesToAdd.append(WTFMove(previous));
        else {
            casesToAdd.append(WTFMove(previous));
            casesToAdd.append(WTFMove(accessCase));
        }
    } else
        casesToAdd.append(WTFMove(accessCase));

    dataLogLnIf(InlineCacheCompilerInternal::verbose, "casesToAdd: ", listDump(casesToAdd));

    // If there aren't any cases to add, then fail on the grounds that there's no point to generating a
    // new stub that will be identical to the old one. Returning null should tell the caller to just
    // keep doing what they were doing before.
    if (casesToAdd.isEmpty())
        return AccessGenerationResult::MadeNoChanges;

    if (stubInfo.accessType != AccessType::InstanceOf) {
        bool shouldReset = false;
        AccessGenerationResult resetResult(AccessGenerationResult::ResetStubAndFireWatchpoints);
        auto considerPolyProtoReset = [&] (Structure* a, Structure* b) {
            if (Structure::shouldConvertToPolyProto(a, b)) {
                // For now, we only reset if this is our first time invalidating this watchpoint.
                // The reason we don't immediately fire this watchpoint is that we may be already
                // watching the poly proto watchpoint, which if fired, would destroy us. We let
                // the person handling the result to do a delayed fire.
                ASSERT(a->rareData()->sharedPolyProtoWatchpoint().get() == b->rareData()->sharedPolyProtoWatchpoint().get());
                if (a->rareData()->sharedPolyProtoWatchpoint()->isStillValid()) {
                    shouldReset = true;
                    resetResult.addWatchpointToFire(*a->rareData()->sharedPolyProtoWatchpoint(), StringFireDetail("Detected poly proto optimization opportunity."));
                }
            }
        };

        for (auto& caseToAdd : casesToAdd) {
            for (auto& existingCase : m_list) {
                Structure* a = caseToAdd->structure();
                Structure* b = existingCase->structure();
                considerPolyProtoReset(a, b);
            }
        }
        for (unsigned i = 0; i < casesToAdd.size(); ++i) {
            for (unsigned j = i + 1; j < casesToAdd.size(); ++j) {
                Structure* a = casesToAdd[i]->structure();
                Structure* b = casesToAdd[j]->structure();
                considerPolyProtoReset(a, b);
            }
        }

        if (shouldReset)
            return resetResult;
    }

    // Now add things to the new list. Note that at this point, we will still have old cases that
    // may be replaced by the new ones. That's fine. We will sort that out when we regenerate.
    for (auto& caseToAdd : casesToAdd) {
        collectAdditionalWatchpoints(vm, caseToAdd.get());
        m_list.append(WTFMove(caseToAdd));
    }

    dataLogLnIf(InlineCacheCompilerInternal::verbose, "After addCases: m_list: ", listDump(m_list));

    return AccessGenerationResult::Buffered;
}

bool PolymorphicAccess::visitWeak(VM& vm)
{
    for (unsigned i = 0; i < size(); ++i) {
        if (!at(i).visitWeak(vm))
            return false;
    }
    return true;
}

template<typename Visitor>
void PolymorphicAccess::propagateTransitions(Visitor& visitor) const
{
    for (unsigned i = 0; i < size(); ++i)
        at(i).propagateTransitions(visitor);
}

template void PolymorphicAccess::propagateTransitions(AbstractSlotVisitor&) const;
template void PolymorphicAccess::propagateTransitions(SlotVisitor&) const;

template<typename Visitor>
void PolymorphicAccess::visitAggregateImpl(Visitor& visitor)
{
    for (unsigned i = 0; i < size(); ++i)
        at(i).visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(PolymorphicAccess);

void PolymorphicAccess::dump(PrintStream& out) const
{
    out.print(RawPointer(this), ":["_s);
    CommaPrinter comma;
    for (auto& entry : m_list)
        out.print(comma, entry.get());
    out.print("]"_s);
}

void InlineCacheHandler::aboutToDie()
{
    if (m_stubRoutine)
        m_stubRoutine->aboutToDie();
    // A reference to InlineCacheHandler may keep it alive later than the CodeBlock that "owns" this
    // watchpoint but the watchpoint must not fire after the CodeBlock has finished destruction,
    // so clear the watchpoint eagerly.
    m_watchpoint.reset();
}

CallLinkInfo* InlineCacheHandler::callLinkInfoAt(const ConcurrentJSLocker& locker, unsigned index)
{
    if (index < Base::size())
        return &span()[index];
    if (!m_stubRoutine)
        return nullptr;
    return m_stubRoutine->callLinkInfoAt(locker, index);
}

bool InlineCacheHandler::visitWeak(VM& vm)
{
    for (auto& callLinkInfo : Base::span())
        callLinkInfo.visitWeak(vm);

    if (m_accessCase) {
        if (!m_accessCase->visitWeak(vm))
            return false;
    }

    if (m_stubRoutine) {
        m_stubRoutine->visitWeak(vm);
        for (StructureID weakReference : m_stubRoutine->weakStructures()) {
            Structure* structure = weakReference.decode();
            if (!vm.heap.isMarked(structure))
                return false;
        }
    }

    return true;
}

void InlineCacheHandler::addOwner(CodeBlock* codeBlock)
{
    if (!m_stubRoutine)
        return;
    m_stubRoutine->addOwner(codeBlock);
}

void InlineCacheHandler::removeOwner(CodeBlock* codeBlock)
{
    if (!m_stubRoutine)
        return;
    m_stubRoutine->removeOwner(codeBlock);
}

} // namespace JSC

namespace WTF {

using namespace JSC;

void printInternal(PrintStream& out, AccessGenerationResult::Kind kind)
{
    switch (kind) {
    case AccessGenerationResult::MadeNoChanges:
        out.print("MadeNoChanges");
        return;
    case AccessGenerationResult::GaveUp:
        out.print("GaveUp");
        return;
    case AccessGenerationResult::Buffered:
        out.print("Buffered");
        return;
    case AccessGenerationResult::GeneratedNewCode:
        out.print("GeneratedNewCode");
        return;
    case AccessGenerationResult::GeneratedFinalCode:
        out.print("GeneratedFinalCode");
        return;
    case AccessGenerationResult::GeneratedMegamorphicCode:
        out.print("GeneratedMegamorphicCode");
        return;
    case AccessGenerationResult::ResetStubAndFireWatchpoints:
        out.print("ResetStubAndFireWatchpoints");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, AccessCase::AccessType type)
{
    switch (type) {
#define JSC_DEFINE_ACCESS_TYPE_CASE(name) \
    case AccessCase::name: \
        out.print(#name); \
        return;

        JSC_FOR_EACH_ACCESS_TYPE(JSC_DEFINE_ACCESS_TYPE_CASE)

#undef JSC_DEFINE_ACCESS_TYPE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, AccessType type)
{
    out.print(JSC::categoryName(type));
}

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(JIT)
