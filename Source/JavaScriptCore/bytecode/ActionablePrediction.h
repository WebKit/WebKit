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

#ifndef ActionablePrediction_h
#define ActionablePrediction_h

#include "JSArray.h"
#include "JSValue.h"
#include "PredictedType.h"
#include <wtf/PackedIntVector.h>

namespace JSC {

// An actionable prediction is one that is enforced on stores to local variables
// and arguments, and when passing arguments to functions. These must be recorded
// in CodeBlock.

enum ActionablePrediction {
    NoActionablePrediction,
    EnforceInt32Prediction,
    EnforceArrayPrediction,
    EnforceBooleanPrediction
};

inline ActionablePrediction actionablePredictionFromPredictedType(PredictedType prediction)
{
    if (isInt32Prediction(prediction))
        return EnforceInt32Prediction;
    
    if (isArrayPrediction(prediction))
        return EnforceArrayPrediction;
    
    if (isBooleanPrediction(prediction))
        return EnforceBooleanPrediction;
    
    return NoActionablePrediction;
}

inline bool valueObeysPrediction(JSGlobalData* globalData, JSValue value, ActionablePrediction prediction)
{
    switch (prediction) {
    case EnforceInt32Prediction:
        return value.isInt32();

    case EnforceArrayPrediction:
        return isJSArray(globalData, value);

    case EnforceBooleanPrediction:
        return value.isBoolean();
        
    default:
        ASSERT(prediction == NoActionablePrediction);
        return true;
    }
}

inline const char* actionablePredictionToString(ActionablePrediction prediction)
{
    switch (prediction) {
    case EnforceInt32Prediction:
        return "EnforceInt32";
        
    case EnforceArrayPrediction:
        return "EnforceArray";
        
    case EnforceBooleanPrediction:
        return "EnforceBoolean";
        
    default:
        ASSERT(prediction == NoActionablePrediction);
        return "NoActionablePrediction";
    }
}

typedef PackedIntVector<ActionablePrediction, 2> ActionablePredictionVector;

class ActionablePredictions {
public:
    ActionablePredictions() { }

    ActionablePredictions(unsigned numArguments, unsigned numVariables)
    {
        m_arguments.ensureSize(numArguments);
        m_variables.ensureSize(numVariables);
        m_arguments.clearAll();
        m_variables.clearAll();
    }
    
    void setArgument(unsigned argument, ActionablePrediction value)
    {
        m_arguments.set(argument, value);
    }
    
    ActionablePrediction argument(unsigned argument)
    {
        if (argument >= m_arguments.size())
            return NoActionablePrediction;
        return m_arguments.get(argument);
    }
    
    void setVariable(unsigned variable, ActionablePrediction value)
    {
        m_variables.set(variable, value);
    }
    
    ActionablePrediction variable(unsigned variable)
    {
        if (variable >= m_variables.size())
            return NoActionablePrediction;
        return m_variables.get(variable);
    }
    
    unsigned argumentUpperBound() const { return m_arguments.size(); }
    unsigned variableUpperBound() const { return m_variables.size(); }
    
    void pack()
    {
        packVector(m_arguments);
        packVector(m_variables);
    }
    
private:
    static void packVector(ActionablePredictionVector& vector)
    {
        unsigned lastSetIndex = 0;
        for (unsigned i = 0; i < vector.size(); ++i) {
            if (vector.get(i) != NoActionablePrediction)
                lastSetIndex = i;
        }
        vector.resize(lastSetIndex + 1);
    }
    
    ActionablePredictionVector m_arguments;
    ActionablePredictionVector m_variables;
};

} // namespace JSC

#endif // ActionablePrediction_h
