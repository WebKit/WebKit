/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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

#include "CacheableIdentifier.h"
#include "ClassInfo.h"
#include "CodeLocation.h"
#include "IndexingType.h"
#include "JITStubRoutine.h"
#include "Structure.h"

namespace JSC {

class Symbol;

#if ENABLE(JIT)

class ArrayProfile;
class StructureStubInfo;

enum JITArrayMode : uint8_t {
    JITInt32,
    JITDouble,
    JITContiguous,
    JITArrayStorage,
    JITDirectArguments,
    JITScopedArguments,
    JITInt8Array,
    JITInt16Array,
    JITInt32Array,
    JITUint8Array,
    JITUint8ClampedArray,
    JITUint16Array,
    JITUint32Array,
    JITFloat32Array,
    JITFloat64Array,
    JITBigInt64Array,
    JITBigUint64Array,
};

inline bool isOptimizableIndexingType(IndexingType indexingType)
{
    switch (indexingType) {
    case ALL_INT32_INDEXING_TYPES:
    case ALL_DOUBLE_INDEXING_TYPES:
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ARRAY_WITH_ARRAY_STORAGE_INDEXING_TYPES:
        return true;
    default:
        return false;
    }
}

inline bool hasOptimizableIndexingForJSType(JSType type)
{
    switch (type) {
    case DirectArgumentsType:
    case ScopedArgumentsType:
        return true;
    default:
        return false;
    }
}

inline bool hasOptimizableIndexingForClassInfo(const ClassInfo* classInfo)
{
    return isTypedView(classInfo->typedArrayStorageType);
}

inline bool hasOptimizableIndexing(Structure* structure)
{
    return isOptimizableIndexingType(structure->indexingType())
        || hasOptimizableIndexingForJSType(structure->typeInfo().type())
        || hasOptimizableIndexingForClassInfo(structure->classInfo());
}

inline JITArrayMode jitArrayModeForIndexingType(IndexingType indexingType)
{
    switch (indexingType) {
    case ALL_INT32_INDEXING_TYPES:
        return JITInt32;
    case ALL_DOUBLE_INDEXING_TYPES:
        return JITDouble;
    case ALL_CONTIGUOUS_INDEXING_TYPES:
        return JITContiguous;
    case ARRAY_WITH_ARRAY_STORAGE_INDEXING_TYPES:
        return JITArrayStorage;
    default:
        CRASH();
        return JITContiguous;
    }
}

inline JITArrayMode jitArrayModeForJSType(JSType type)
{
    switch (type) {
    case DirectArgumentsType:
        return JITDirectArguments;
    case ScopedArgumentsType:
        return JITScopedArguments;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return JITContiguous;
    }
}

inline JITArrayMode jitArrayModeForClassInfo(const ClassInfo* classInfo)
{
    switch (classInfo->typedArrayStorageType) {
    case TypeInt8:
        return JITInt8Array;
    case TypeInt16:
        return JITInt16Array;
    case TypeInt32:
        return JITInt32Array;
    case TypeUint8:
        return JITUint8Array;
    case TypeUint8Clamped:
        return JITUint8ClampedArray;
    case TypeUint16:
        return JITUint16Array;
    case TypeUint32:
        return JITUint32Array;
    case TypeFloat32:
        return JITFloat32Array;
    case TypeFloat64:
        return JITFloat64Array;
    case TypeBigInt64:
        return JITBigInt64Array;
    case TypeBigUint64:
        return JITBigUint64Array;
    default:
        CRASH();
        return JITContiguous;
    }
}

inline bool jitArrayModePermitsPut(JITArrayMode mode)
{
    switch (mode) {
    case JITDirectArguments:
    case JITScopedArguments:
    // FIXME: Optimize BigInt64Array / BigUint64Array in IC
    // https://bugs.webkit.org/show_bug.cgi?id=221183
    case JITBigInt64Array:
    case JITBigUint64Array:
        // We could support put_by_val on these at some point, but it's just not that profitable
        // at the moment.
        return false;
    default:
        return true;
    }
}

inline bool jitArrayModePermitsPutDirect(JITArrayMode mode)
{
    // We don't allow typed array putDirect here since putDirect has
    // defineOwnProperty({configurable: true, writable:true, enumerable:true})
    // semantics. Typed array indexed properties are non-configurable by
    // default, so we can't simply store to a typed array for putDirect.
    //
    // We could model putDirect on ScopedArguments and DirectArguments, but we
    // haven't found any performance incentive to do it yet.
    switch (mode) {
    case JITInt32:
    case JITDouble:
    case JITContiguous:
    case JITArrayStorage:
        return true;
    default:
        return false;
    }
}

inline TypedArrayType typedArrayTypeForJITArrayMode(JITArrayMode mode)
{
    switch (mode) {
    case JITInt8Array:
        return TypeInt8;
    case JITInt16Array:
        return TypeInt16;
    case JITInt32Array:
        return TypeInt32;
    case JITUint8Array:
        return TypeUint8;
    case JITUint8ClampedArray:
        return TypeUint8Clamped;
    case JITUint16Array:
        return TypeUint16;
    case JITUint32Array:
        return TypeUint32;
    case JITFloat32Array:
        return TypeFloat32;
    case JITFloat64Array:
        return TypeFloat64;
    case JITBigInt64Array:
        return TypeBigInt64;
    case JITBigUint64Array:
        return TypeBigUint64;
    default:
        CRASH();
        return NotTypedArray;
    }
}

inline JITArrayMode jitArrayModeForStructure(Structure* structure)
{
    if (isOptimizableIndexingType(structure->indexingType()))
        return jitArrayModeForIndexingType(structure->indexingType());
    
    if (hasOptimizableIndexingForJSType(structure->typeInfo().type()))
        return jitArrayModeForJSType(structure->typeInfo().type());
    
    ASSERT(hasOptimizableIndexingForClassInfo(structure->classInfo()));
    return jitArrayModeForClassInfo(structure->classInfo());
}

struct ByValInfo {
    ByValInfo(BytecodeIndex bytecodeIndex)
        : bytecodeIndex(bytecodeIndex)
    {
    }

    void setUp(CodeLocationJump<JSInternalPtrTag> notIndexJump, CodeLocationJump<JSInternalPtrTag> badTypeJump, CodeLocationLabel<ExceptionHandlerPtrTag> exceptionHandler, JITArrayMode arrayMode, ArrayProfile* arrayProfile, CodeLocationLabel<JSInternalPtrTag> doneTarget, CodeLocationLabel<JSInternalPtrTag> badTypeNextHotPathTarget, CodeLocationLabel<JSInternalPtrTag> slowPathTarget)
    {
        this->notIndexJump = notIndexJump;
        this->badTypeJump = badTypeJump;
        this->exceptionHandler = exceptionHandler;
        this->doneTarget = doneTarget;
        this->badTypeNextHotPathTarget = badTypeNextHotPathTarget;
        this->slowPathTarget = slowPathTarget;
        this->arrayProfile = arrayProfile;
        this->slowPathCount = 0;
        this->stubInfo = nullptr;
        this->arrayMode = arrayMode;
        this->tookSlowPath = false;
        this->seen = false;
    }

    DECLARE_VISIT_AGGREGATE;

    CodeLocationJump<JSInternalPtrTag> notIndexJump;
    CodeLocationJump<JSInternalPtrTag> badTypeJump;
    CodeLocationLabel<ExceptionHandlerPtrTag> exceptionHandler;
    CodeLocationLabel<JSInternalPtrTag> doneTarget;
    CodeLocationLabel<JSInternalPtrTag> badTypeNextHotPathTarget;
    CodeLocationLabel<JSInternalPtrTag> slowPathTarget;
    ArrayProfile* arrayProfile;
    BytecodeIndex bytecodeIndex;
    unsigned slowPathCount;
    RefPtr<JITStubRoutine> stubRoutine;
    CacheableIdentifier cachedId; // Once we set cachedId, we must not change the value. JIT code relies on that configured cachedId is marked and retained by CodeBlock through ByValInfo.
    StructureStubInfo* stubInfo;
    JITArrayMode arrayMode; // The array mode that was baked into the inline JIT code.
    bool tookSlowPath : 1;
    bool seen : 1;
};

inline BytecodeIndex getByValInfoBytecodeIndex(ByValInfo* info)
{
    return info->bytecodeIndex;
}

#endif // ENABLE(JIT)

} // namespace JSC
