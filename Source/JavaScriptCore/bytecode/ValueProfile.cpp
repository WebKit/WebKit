/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ValueProfile.h"

namespace JSC {

#if ENABLE(VALUE_PROFILER)
void ValueProfile::computeStatistics(const ClassInfo* classInfo, Statistics& statistics)
{
    statistics.cells++;
    
    if (classInfo == &JSFinalObject::s_info) {
        statistics.finalObjects++;
        statistics.objects++;
        return;
    }
    
    if (classInfo == &JSArray::s_info) {
        statistics.arrays++;
        statistics.objects++;
        return;
    }
    
    if (classInfo == &JSString::s_info) {
        statistics.strings++;
        return;
    }
    
    if (classInfo->isSubClassOf(&JSObject::s_info))
        statistics.objects++;
}

void ValueProfile::computeStatistics(Statistics& statistics) const
{
    for (unsigned i = 0; i < numberOfBuckets; ++i) {
        if (!m_buckets[i]) {
            WeakBucket weakBucket = m_weakBuckets[i];
            if (!!weakBucket) {
                statistics.samples++;
                computeStatistics(weakBucket.getClassInfo(), statistics);
            }
            
            continue;
        }
        
        statistics.samples++;
        
        JSValue value = JSValue::decode(m_buckets[i]);
        if (value.isInt32())
            statistics.int32s++;
        else if (value.isDouble())
            statistics.doubles++;
        else if (value.isCell())
            computeStatistics(value.asCell()->structure()->classInfo(), statistics);
        else if (value.isBoolean())
            statistics.booleans++;
    }
}

PredictedType ValueProfile::computeUpdatedPrediction()
{
    ValueProfile::Statistics statistics;
    computeStatistics(statistics);
    
    PredictedType prediction;
    
    if (!statistics.samples)
        prediction = PredictNone;
    else if (statistics.int32s == statistics.samples)
        prediction = StrongPredictionTag | PredictInt32;
    else if (statistics.doubles == statistics.samples)
        prediction = StrongPredictionTag | PredictDouble;
    else if (statistics.int32s + statistics.doubles == statistics.samples)
        prediction = StrongPredictionTag | PredictNumber;
    else if (statistics.arrays == statistics.samples)
        prediction = StrongPredictionTag | PredictArray;
    else if (statistics.finalObjects == statistics.samples)
        prediction = StrongPredictionTag | PredictFinalObject;
    else if (statistics.strings == statistics.samples)
        prediction = StrongPredictionTag | PredictString;
    else if (statistics.objects == statistics.samples)
        prediction = StrongPredictionTag | PredictObjectOther;
    else if (statistics.cells == statistics.samples)
        prediction = StrongPredictionTag | PredictCellOther;
    else if (statistics.booleans == statistics.samples)
        prediction = StrongPredictionTag | PredictBoolean;
    else
        prediction = StrongPredictionTag | PredictOther;

    m_numberOfSamplesInPrediction += statistics.samples;
    mergePrediction(m_prediction, prediction);
    for (unsigned i = 0; i < numberOfBuckets; ++i) {
        m_buckets[i] = JSValue::encode(JSValue());
        m_weakBuckets[i] = WeakBucket();
    }
    return m_prediction;
}
#endif // ENABLE(VALUE_PROFILER)

} // namespace JSC
