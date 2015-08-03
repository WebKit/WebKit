/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef DFGAdaptiveInferredPropertyValueWatchpoint_h
#define DFGAdaptiveInferredPropertyValueWatchpoint_h

#if ENABLE(DFG_JIT)

#include "ObjectPropertyCondition.h"
#include "Watchpoint.h"
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

namespace JSC { namespace DFG {

class AdaptiveInferredPropertyValueWatchpoint {
    WTF_MAKE_NONCOPYABLE(AdaptiveInferredPropertyValueWatchpoint);
    WTF_MAKE_FAST_ALLOCATED;

public:
    AdaptiveInferredPropertyValueWatchpoint(const ObjectPropertyCondition&, CodeBlock*);
    
    const ObjectPropertyCondition& key() const { return m_key; }
    
    void install();

private:
    class StructureWatchpoint : public Watchpoint {
    public:
        StructureWatchpoint() { }
    protected:
        virtual void fireInternal(const FireDetail&) override;
    };
    class PropertyWatchpoint : public Watchpoint {
    public:
        PropertyWatchpoint() { }
    protected:
        virtual void fireInternal(const FireDetail&) override;
    };
    
    void fire(const FireDetail&);
    
    ObjectPropertyCondition m_key;
    CodeBlock* m_codeBlock;
    StructureWatchpoint m_structureWatchpoint;
    PropertyWatchpoint m_propertyWatchpoint;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGAdaptiveInferredPropertyValueWatchpoint_h

