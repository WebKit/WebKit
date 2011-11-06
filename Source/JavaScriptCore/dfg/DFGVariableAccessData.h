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

#ifndef DFGVariableAccessData_h
#define DFGVariableAccessData_h

#include "PredictedType.h"
#include "VirtualRegister.h"
#include <wtf/Platform.h>
#include <wtf/UnionFind.h>

namespace JSC { namespace DFG {

class VariableAccessData : public UnionFind<VariableAccessData> {
public:
    VariableAccessData()
        : m_local(static_cast<VirtualRegister>(std::numeric_limits<int>::min()))
        , m_prediction(PredictNone)
    {
    }
    
    VariableAccessData(VirtualRegister local)
        : m_local(local)
        , m_prediction(PredictNone)
    {
    }
    
    VirtualRegister local()
    {
        ASSERT(m_local == find()->m_local);
        return m_local;
    }
    
    int operand()
    {
        return static_cast<int>(local());
    }
    
    bool predict(PredictedType prediction)
    {
        return mergePrediction(find()->m_prediction, prediction);
    }
    
    PredictedType prediction()
    {
        return find()->m_prediction;
    }
    
private:
    // This is slightly space-inefficient, since anything we're unified with
    // will have the same operand and should have the same prediction. But
    // putting them here simplifies the code, and we don't expect DFG space
    // usage for variable access nodes do be significant.

    VirtualRegister m_local;
    PredictedType m_prediction;
};

} } // namespace JSC::DFG

#endif // DFGVariableAccessData_h
