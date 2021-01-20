/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#include "DFGHeapLocation.h"

#if ENABLE(DFG_JIT)

#include "JSCJSValueInlines.h"

namespace JSC { namespace DFG {

void HeapLocation::dump(PrintStream& out) const
{
    out.print(m_kind, ":", m_heap);
    
    if (!m_base)
        return;
    
    out.print("[", m_base);
    if (m_index)
        out.print(", ", m_index);
    out.print("]");
}

} } // namespace JSC::DFG

namespace WTF {

using namespace JSC::DFG;

void printInternal(PrintStream& out, LocationKind kind)
{
    switch (kind) {
    case InvalidLocationKind:
        out.print("InvalidLocationKind");
        return;
        
    case InvalidationPointLoc:
        out.print("InvalidationPointLoc");
        return;
        
    case TypeOfIsObjectLoc:
        out.print("TypeOfIsObjectLoc");
        return;

    case TypeOfIsFunctionLoc:
        out.print("TypeOfIsFunctionLoc");
        return;

    case IsCallableLoc:
        out.print("IsCallableLoc");
        return;
        
    case IsConstructorLoc:
        out.print("IsConstructorLoc");
        return;

    case GetterLoc:
        out.print("GetterLoc");
        return;
        
    case SetterLoc:
        out.print("SetterLoc");
        return;
        
    case StackLoc:
        out.print("StackLoc");
        return;
        
    case StackPayloadLoc:
        out.print("StackPayloadLoc");
        return;
        
    case ArrayLengthLoc:
        out.print("ArrayLengthLoc");
        return;

    case ArrayMaskLoc:
        out.print("ArrayMaskLoc");
        return;

    case VectorLengthLoc:
        out.print("VectorLengthLoc");
        return;
        
    case ButterflyLoc:
        out.print("ButterflyLoc");
        return;
        
    case CheckTypeInfoFlagsLoc:
        out.print("CheckTypeInfoFlagsLoc");
        return;

    case OverridesHasInstanceLoc:
        out.print("OverridesHasInstanceLoc");
        return;
        
    case ClosureVariableLoc:
        out.print("ClosureVariableLoc");
        return;
        
    case DirectArgumentsLoc:
        out.print("DirectArgumentsLoc");
        return;
        
    case GlobalVariableLoc:
        out.print("GlobalVariableLoc");
        return;
        
    case HasIndexedPropertyLoc:
        out.print("HasIndexedPorpertyLoc");
        return;

    case IndexedPropertyDoubleLoc:
        out.print("IndexedPropertyDoubleLoc");
        return;

    case IndexedPropertyDoubleSaneChainLoc:
        out.print("IndexedPropertyDoubleSaneChainLoc");
        return;

    case IndexedPropertyDoubleOutOfBoundsSaneChainLoc:
        out.print("IndexedPropertyDoubleOutOfBoundsSaneChainLoc");
        return;

    case IndexedPropertyDoubleOrOtherOutOfBoundsSaneChainLoc:
        out.print("IndexedPropertyDoubleOrOtherOutOfBoundsSaneChainLoc");
        return;

    case IndexedPropertyInt32Loc:
        out.print("IndexedPropertyInt32Loc");
        return;

    case IndexedPropertyInt32OutOfBoundsSaneChainLoc:
        out.print("IndexedPropertyInt32OutOfBoundsSaneChainLoc");
        return;

    case IndexedPropertyInt52Loc:
        out.print("IndexedPropertyInt52Loc");
        return;

    case IndexedPropertyJSLoc:
        out.print("IndexedPropertyJSLoc");
        return;

    case IndexedPropertyJSOutOfBoundsSaneChainLoc:
        out.print("IndexedPropertyJSOutOfBoundsSaneChainLoc");
        return;

    case IndexedPropertyStorageLoc:
        out.print("IndexedPropertyStorageLoc");
        return;
        
    case NamedPropertyLoc:
        out.print("NamedPropertyLoc");
        return;
        
    case TypedArrayByteOffsetLoc:
        out.print("TypedArrayByteOffsetLoc");
        return;

    case PrototypeLoc:
        out.print("PrototypeLoc");
        return;
        
    case StructureLoc:
        out.print("StructureLoc");
        return;

    case RegExpObjectLastIndexLoc:
        out.print("RegExpObjectLastIndexLoc");
        return;

    case DateFieldLoc:
        out.print("DateFieldLoc");
        return;

    case MapBucketLoc:
        out.print("MapBucketLoc");
        return;

    case MapBucketHeadLoc:
        out.print("MapBucketHeadLoc");
        return;

    case MapBucketKeyLoc:
        out.print("MapBucketKeyLoc");
        return;

    case MapBucketValueLoc:
        out.print("MapBucketValueLoc");
        return;

    case MapBucketNextLoc:
        out.print("MapBucketNextLoc");
        return;

    case WeakMapGetLoc:
        out.print("WeakMapGetLoc");
        return;

    case InternalFieldObjectLoc:
        out.print("InternalFieldObjectLoc");
        return;

    case DOMStateLoc:
        out.print("DOMStateLoc");
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(DFG_JIT)

