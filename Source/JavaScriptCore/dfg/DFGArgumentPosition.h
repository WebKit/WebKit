/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef DFGArgumentPosition_h
#define DFGArgumentPosition_h

#include "DFGDoubleFormatState.h"
#include "DFGVariableAccessData.h"
#include "SpeculatedType.h"

namespace JSC { namespace DFG {

class ArgumentPosition {
public:
    ArgumentPosition()
        : m_prediction(SpecNone)
        , m_doubleFormatState(EmptyDoubleFormatState)
        , m_shouldNeverUnbox(false)
    {
    }
    
    void addVariable(VariableAccessData* variable)
    {
        m_variables.append(variable);
    }
    
    bool mergeShouldNeverUnbox(bool shouldNeverUnbox)
    {
        bool newShouldNeverUnbox = m_shouldNeverUnbox | shouldNeverUnbox;
        if (newShouldNeverUnbox == m_shouldNeverUnbox)
            return false;
        m_shouldNeverUnbox = newShouldNeverUnbox;
        return true;
    }
    
    bool mergeArgumentAwareness()
    {
        bool changed = false;
        for (unsigned i = 0; i < m_variables.size(); ++i) {
            changed |= mergeSpeculation(m_prediction, m_variables[i]->argumentAwarePrediction());
            changed |= mergeDoubleFormatState(m_doubleFormatState, m_variables[i]->doubleFormatState());
            changed |= mergeShouldNeverUnbox(m_variables[i]->shouldNeverUnbox());
        }
        if (!changed)
            return false;
        changed = false;
        for (unsigned i = 0; i < m_variables.size(); ++i) {
            changed |= m_variables[i]->mergeArgumentAwarePrediction(m_prediction);
            changed |= m_variables[i]->mergeDoubleFormatState(m_doubleFormatState);
            changed |= m_variables[i]->mergeShouldNeverUnbox(m_shouldNeverUnbox);
        }
        return changed;
    }
    
    SpeculatedType prediction() const { return m_prediction; }
    DoubleFormatState doubleFormatState() const { return m_doubleFormatState; }
    bool shouldUseDoubleFormat() const
    {
        return doubleFormatState() == UsingDoubleFormat;
    }
    
private:
    SpeculatedType m_prediction;
    DoubleFormatState m_doubleFormatState;
    bool m_shouldNeverUnbox;
    
    Vector<VariableAccessData*, 2> m_variables;
};

} } // namespace JSC::DFG

#endif // DFGArgumentPosition_h

