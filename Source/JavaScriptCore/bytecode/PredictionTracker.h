/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef PredictionTracker_h
#define PredictionTracker_h

#include "PredictedType.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace JSC {

// helper function to distinguish vars & temporaries from arguments.
inline bool operandIsArgument(int operand) { return operand < 0; }

struct PredictionSlot {
public:
    PredictionSlot()
        : m_value(PredictNone)
    {
    }
    PredictedType m_value;
};

class PredictionTracker {
public:
    PredictionTracker()
    {
    }
    
    PredictionTracker(unsigned numArguments, unsigned numVariables)
        : m_arguments(numArguments)
        , m_variables(numVariables)
    {
    }
    
    void initializeSimilarTo(const PredictionTracker& other)
    {
        m_arguments.resize(other.numberOfArguments());
        m_variables.resize(other.numberOfVariables());
    }
    
    void copyLocalsFrom(const PredictionTracker& other)
    {
        m_arguments = other.m_arguments;
        m_variables = other.m_variables;
    }
    
    size_t numberOfArguments() const { return m_arguments.size(); }
    size_t numberOfVariables() const { return m_variables.size(); }
    
    unsigned argumentIndexForOperand(int operand) const { return operand + m_arguments.size() + RegisterFile::CallFrameHeaderSize; }

    bool predictArgument(unsigned argument, PredictedType prediction, PredictionSource source)
    {
        return mergePrediction(m_arguments[argument].m_value, makePrediction(prediction, source));
    }
    
    bool predict(int operand, PredictedType prediction, PredictionSource source)
    {
        if (operandIsArgument(operand))
            return predictArgument(argumentIndexForOperand(operand), prediction, source);
        if ((unsigned)operand >= m_variables.size()) {
            ASSERT(operand >= 0);
            m_variables.resize(operand + 1);
        }
        
        return mergePrediction(m_variables[operand].m_value, makePrediction(prediction, source));
    }
    
    bool predictGlobalVar(unsigned varNumber, PredictedType prediction, PredictionSource source)
    {
        HashMap<unsigned, PredictionSlot>::iterator iter = m_globalVars.find(varNumber + 1);
        if (iter == m_globalVars.end()) {
            PredictionSlot predictionSlot;
            bool result = mergePrediction(predictionSlot.m_value, makePrediction(prediction, source));
            m_globalVars.add(varNumber + 1, predictionSlot);
            return result;
        }
        return mergePrediction(iter->second.m_value, makePrediction(prediction, source));
    }
    
    PredictedType getArgumentPrediction(unsigned argument)
    {
        return m_arguments[argument].m_value;
    }

    PredictedType getPrediction(int operand)
    {
        if (operandIsArgument(operand))
            return getArgumentPrediction(argumentIndexForOperand(operand));
        if ((unsigned)operand < m_variables.size())
            return m_variables[operand].m_value;
        return PredictNone;
    }
    
    PredictedType getGlobalVarPrediction(unsigned varNumber)
    {
        HashMap<unsigned, PredictionSlot>::iterator iter = m_globalVars.find(varNumber + 1);
        if (iter == m_globalVars.end())
            return PredictNone;
        return iter->second.m_value;
    }
    
private:
    Vector<PredictionSlot, 16> m_arguments;
    Vector<PredictionSlot, 16> m_variables;
    HashMap<unsigned, PredictionSlot> m_globalVars;
};

} // namespace JSC

#endif // PredictionTracker_h

