/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "VisitingTimeout.h"
#include <wtf/FastMalloc.h>
#include <wtf/Function.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/CString.h>

namespace JSC {

class MarkingConstraintSet;
class SlotVisitor;

class MarkingConstraint {
    WTF_MAKE_NONCOPYABLE(MarkingConstraint);
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum Volatility {
        // FIXME: We could introduce a new kind of volatility called GreyedByResumption, which
        // would mean running all of the times that GreyedByExecution runs except as a root in a
        // full GC.
        // https://bugs.webkit.org/show_bug.cgi?id=166830
        
        // The constraint needs to be reevaluated anytime the mutator runs: so at GC start and
        // whenever the GC resuspends after a resumption. This is almost always something that
        // you'd call a "root" in a traditional GC.
        GreyedByExecution,
        
        // The constraint needs to be reevaluated any time any object is marked and anytime the
        // mutator resumes.
        GreyedByMarking
    };
    
    MarkingConstraint(
        CString abbreviatedName, CString name,
        ::Function<void(SlotVisitor&, const VisitingTimeout&)>,
        Volatility);
    
    MarkingConstraint(
        CString abbreviatedName, CString name,
        ::Function<void(SlotVisitor&, const VisitingTimeout&)>,
        ::Function<double(SlotVisitor&)>,
        Volatility);
    
    ~MarkingConstraint();
    
    unsigned index() const { return m_index; }
    
    const char* abbreviatedName() const { return m_abbreviatedName.data(); }
    const char* name() const { return m_name.data(); }
    
    void resetStats();
    
    size_t lastVisitCount() const { return m_lastVisitCount; }
    
    void execute(SlotVisitor&, bool& didVisitSomething, MonotonicTime timeout);
    
    double quickWorkEstimate(SlotVisitor& visitor)
    {
        if (!m_quickWorkEstimateFunction)
            return 0;
        return m_quickWorkEstimateFunction(visitor);
    }
    
    double workEstimate(SlotVisitor& visitor)
    {
        return lastVisitCount() + quickWorkEstimate(visitor);
    }
    
    Volatility volatility() const { return m_volatility; }
    
private:
    friend class MarkingConstraintSet; // So it can set m_index.
    
    unsigned m_index { UINT_MAX };
    CString m_abbreviatedName;
    CString m_name;
    ::Function<void(SlotVisitor&, const VisitingTimeout& timeout)> m_executeFunction;
    ::Function<double(SlotVisitor&)> m_quickWorkEstimateFunction;
    Volatility m_volatility;
    size_t m_lastVisitCount { 0 };
};

} // namespace JSC

