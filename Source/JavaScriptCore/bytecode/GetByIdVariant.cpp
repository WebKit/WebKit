/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#include "GetByIdVariant.h"

#include "CacheableIdentifierInlines.h"
#include "CallLinkStatus.h"
#include "JSCInlines.h"
#include <wtf/ListDump.h>

namespace JSC {

GetByIdVariant::GetByIdVariant(
    CacheableIdentifier identifier,
    const StructureSet& structureSet, PropertyOffset offset,
    const ObjectPropertyConditionSet& conditionSet,
    std::unique_ptr<CallLinkStatus> callLinkStatus,
    JSFunction* intrinsicFunction,
    FunctionPtr<OperationPtrTag> customAccessorGetter,
    std::unique_ptr<DOMAttributeAnnotation> domAttribute)
    : m_structureSet(structureSet)
    , m_conditionSet(conditionSet)
    , m_offset(offset)
    , m_callLinkStatus(WTFMove(callLinkStatus))
    , m_intrinsicFunction(intrinsicFunction)
    , m_customAccessorGetter(customAccessorGetter)
    , m_domAttribute(WTFMove(domAttribute))
    , m_identifier(WTFMove(identifier))
{
    if (!structureSet.size()) {
        ASSERT(offset == invalidOffset);
        ASSERT(conditionSet.isEmpty());
    }
    if (intrinsicFunction)
        ASSERT(intrinsic() != NoIntrinsic);
}
                     
GetByIdVariant::~GetByIdVariant() { }

GetByIdVariant::GetByIdVariant(const GetByIdVariant& other)
    : GetByIdVariant(other.m_identifier)
{
    *this = other;
}

GetByIdVariant& GetByIdVariant::operator=(const GetByIdVariant& other)
{
    m_identifier = other.m_identifier;
    m_structureSet = other.m_structureSet;
    m_conditionSet = other.m_conditionSet;
    m_offset = other.m_offset;
    m_intrinsicFunction = other.m_intrinsicFunction;
    m_customAccessorGetter = other.m_customAccessorGetter;
    if (other.m_domAttribute)
        m_domAttribute = WTF::makeUnique<DOMAttributeAnnotation>(*other.m_domAttribute);
    else
        m_domAttribute = nullptr;
    if (other.m_callLinkStatus)
        m_callLinkStatus = makeUnique<CallLinkStatus>(*other.m_callLinkStatus);
    else
        m_callLinkStatus = nullptr;
    return *this;
}

inline bool GetByIdVariant::canMergeIntrinsicStructures(const GetByIdVariant& other) const
{
    if (m_intrinsicFunction != other.m_intrinsicFunction)
        return false;
    switch (intrinsic()) {
    case TypedArrayByteLengthIntrinsic: {
        // We can merge these sets as long as the element size of the two sets is the same.
        TypedArrayType thisType = (*m_structureSet.begin())->classInfo()->typedArrayStorageType;
        TypedArrayType otherType = (*other.m_structureSet.begin())->classInfo()->typedArrayStorageType;

        ASSERT(isTypedView(thisType) && isTypedView(otherType));

        return logElementSize(thisType) == logElementSize(otherType);
    }

    default:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool GetByIdVariant::attemptToMerge(const GetByIdVariant& other)
{
    if (!!m_identifier != !!other.m_identifier)
        return false;

    if (m_identifier && (m_identifier != other.m_identifier))
        return false;

    if (m_offset != other.m_offset)
        return false;
    
    if (m_callLinkStatus || other.m_callLinkStatus) {
        if (!(m_callLinkStatus && other.m_callLinkStatus))
            return false;
    }

    if (!canMergeIntrinsicStructures(other))
        return false;

    if (m_customAccessorGetter != other.m_customAccessorGetter)
        return false;

    if (m_domAttribute || other.m_domAttribute) {
        if (!(m_domAttribute && other.m_domAttribute))
            return false;
        if (*m_domAttribute != *other.m_domAttribute)
            return false;
    }

    if (m_conditionSet.isEmpty() != other.m_conditionSet.isEmpty())
        return false;
    
    ObjectPropertyConditionSet mergedConditionSet;
    if (!m_conditionSet.isEmpty()) {
        mergedConditionSet = m_conditionSet.mergedWith(other.m_conditionSet);
        if (!mergedConditionSet.isValid())
            return false;
        // If this is a hit variant, one slot base should exist. If this is not a hit variant, the slot base is not necessary.
        if (!isPropertyUnset() && !mergedConditionSet.hasOneSlotBaseCondition())
            return false;
    }
    m_conditionSet = mergedConditionSet;
    
    m_structureSet.merge(other.m_structureSet);
    
    if (m_callLinkStatus)
        m_callLinkStatus->merge(*other.m_callLinkStatus);

    return true;
}

void GetByIdVariant::visitAggregate(SlotVisitor& visitor)
{
    m_identifier.visitAggregate(visitor);
}

void GetByIdVariant::markIfCheap(SlotVisitor& visitor)
{
    m_structureSet.markIfCheap(visitor);
}

bool GetByIdVariant::finalize(VM& vm)
{
    if (!m_structureSet.isStillAlive(vm))
        return false;
    if (!m_conditionSet.areStillLive(vm))
        return false;
    if (m_callLinkStatus && !m_callLinkStatus->finalize(vm))
        return false;
    if (m_intrinsicFunction && !vm.heap.isMarked(m_intrinsicFunction))
        return false;
    return true;
}

void GetByIdVariant::dump(PrintStream& out) const
{
    dumpInContext(out, 0);
}

void GetByIdVariant::dumpInContext(PrintStream& out, DumpContext* context) const
{
    out.print("<");
    out.print("id='", m_identifier, "', ");
    if (!isSet()) {
        out.print("empty>");
        return;
    }
    out.print(inContext(structureSet(), context), ", ", inContext(m_conditionSet, context));
    out.print(", offset = ", offset());
    if (m_callLinkStatus)
        out.print(", call = ", *m_callLinkStatus);
    if (m_intrinsicFunction)
        out.print(", intrinsic = ", *m_intrinsicFunction);
    if (m_customAccessorGetter)
        out.print(", customaccessorgetter = ", RawPointer(m_customAccessorGetter.executableAddress()));
    if (m_domAttribute) {
        out.print(", domclass = ", RawPointer(m_domAttribute->classInfo));
        if (m_domAttribute->domJIT)
            out.print(", domjit = ", RawPointer(m_domAttribute->domJIT));
    }
    out.print(">");
}

} // namespace JSC

