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

#pragma once

#include "CacheableIdentifier.h"
#include "CallLinkStatus.h"
#include "ObjectPropertyConditionSet.h"
#include "PropertyOffset.h"
#include "StructureSet.h"
#include <wtf/Box.h>

namespace JSC {
namespace DOMJIT {
class GetterSetter;
}

class CallLinkStatus;
class GetByStatus;
struct DumpContext;

class GetByIdVariant {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GetByIdVariant(
        CacheableIdentifier,
        const StructureSet& structureSet = StructureSet(), PropertyOffset offset = invalidOffset,
        const ObjectPropertyConditionSet& = ObjectPropertyConditionSet(),
        std::unique_ptr<CallLinkStatus> = nullptr,
        JSFunction* = nullptr,
        FunctionPtr<OperationPtrTag> customAccessorGetter = nullptr,
        std::unique_ptr<DOMAttributeAnnotation> = nullptr);

    ~GetByIdVariant();
    
    GetByIdVariant(const GetByIdVariant&);
    GetByIdVariant& operator=(const GetByIdVariant&);
    
    bool isSet() const { return !!m_structureSet.size(); }
    explicit operator bool() const { return isSet(); }
    const StructureSet& structureSet() const { return m_structureSet; }
    StructureSet& structureSet() { return m_structureSet; }

    // A non-empty condition set means that this is a prototype load.
    const ObjectPropertyConditionSet& conditionSet() const { return m_conditionSet; }
    
    PropertyOffset offset() const { return m_offset; }
    CallLinkStatus* callLinkStatus() const { return m_callLinkStatus.get(); }
    JSFunction* intrinsicFunction() const { return m_intrinsicFunction; }
    Intrinsic intrinsic() const { return m_intrinsicFunction ? m_intrinsicFunction->intrinsic() : NoIntrinsic; }
    FunctionPtr<OperationPtrTag> customAccessorGetter() const { return m_customAccessorGetter; }
    DOMAttributeAnnotation* domAttribute() const { return m_domAttribute.get(); }

    bool isPropertyUnset() const { return offset() == invalidOffset; }

    bool attemptToMerge(const GetByIdVariant& other);
    
    void visitAggregate(SlotVisitor&);
    void markIfCheap(SlotVisitor&);
    bool finalize(VM&);
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;

    CacheableIdentifier identifier() const { return m_identifier; }

    bool overlaps(const GetByIdVariant& other)
    {
        if (!!m_identifier != !!other.m_identifier)
            return true;
        if (m_identifier) {
            if (m_identifier != other.m_identifier)
                return false;
        }
        return structureSet().overlaps(other.structureSet());
    }
    
private:
    friend class GetByStatus;

    bool canMergeIntrinsicStructures(const GetByIdVariant&) const;
    
    StructureSet m_structureSet;
    ObjectPropertyConditionSet m_conditionSet;
    PropertyOffset m_offset;
    std::unique_ptr<CallLinkStatus> m_callLinkStatus;
    JSFunction* m_intrinsicFunction;
    FunctionPtr<OperationPtrTag> m_customAccessorGetter;
    std::unique_ptr<DOMAttributeAnnotation> m_domAttribute;
    CacheableIdentifier m_identifier;
};

} // namespace JSC
