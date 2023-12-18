/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "LazyValueProfile.h"

#include "JSCJSValueInlines.h"

namespace JSC {

void CompressedLazyValueProfileHolder::computeUpdatedPredictions(const ConcurrentJSLocker& locker, CodeBlock* codeBlock)
{
    if (!m_data)
        return;

    for (auto& profile : m_data->operandValueProfiles)
        profile.computeUpdatedPrediction(locker);

    for (auto& pair : m_data->speculationFailureValueProfileBuckets) {
        ValueProfile& profile = codeBlock->valueProfileForBytecodeIndex(pair.first);
        profile.computeUpdatedPredictionForExtraValue(locker, pair.second);
    }
}

void CompressedLazyValueProfileHolder::initializeData()
{
    ASSERT(!isCompilationThread());
    ASSERT(!m_data);
    auto data = makeUnique<LazyValueProfileHolder>();
    // Make sure the initialization of the holder happens before we expose the data to compiler threads.
    WTF::storeStoreFence();
    m_data = WTFMove(data);
}

LazyOperandValueProfile* CompressedLazyValueProfileHolder::addOperandValueProfile(const LazyOperandValueProfileKey& key)
{
    // This addition happens from mutator thread. Thus, we do not need to consider about concurrent additions from multiple threads.
    ASSERT(!isCompilationThread());

    if (!m_data)
        initializeData();

    for (auto& profile : m_data->operandValueProfiles) {
        if (profile.key() == key)
            return &profile;
    }

    m_data->operandValueProfiles.appendConcurrently(LazyOperandValueProfile(key));
    return &m_data->operandValueProfiles.last();
}

JSValue* CompressedLazyValueProfileHolder::addSpeculationFailureValueProfile(BytecodeIndex index)
{
    // This addition happens from mutator thread. Thus, we do not need to consider about concurrent additions from multiple threads.
    ASSERT(!isCompilationThread());

    if (!m_data)
        initializeData();

    for (auto& pair : m_data->speculationFailureValueProfileBuckets) {
        if (pair.first == index)
            return &pair.second;
    }

    m_data->speculationFailureValueProfileBuckets.appendConcurrently(std::make_pair(index, JSValue()));
    return &m_data->speculationFailureValueProfileBuckets.last().second;
}

HashMap<BytecodeIndex, JSValue*> CompressedLazyValueProfileHolder::speculationFailureValueProfileBucketsMap()
{
    HashMap<BytecodeIndex, JSValue*> result;
    if (m_data) {
        result.reserveInitialCapacity(m_data->speculationFailureValueProfileBuckets.size());
        for (auto& pair : m_data->speculationFailureValueProfileBuckets)
            result.add(pair.first, &pair.second);
    }

    return result;
}

void LazyOperandValueProfileParser::initialize(CompressedLazyValueProfileHolder& holder)
{
    ASSERT(m_map.isEmpty());

    if (!holder.m_data)
        return;

    for (auto& profile : holder.m_data->operandValueProfiles)
        m_map.add(profile.key(), &profile);
}

LazyOperandValueProfile* LazyOperandValueProfileParser::getIfPresent(const LazyOperandValueProfileKey& key) const
{
    auto iter = m_map.find(key);

    if (iter == m_map.end())
        return nullptr;

    return iter->value;
}

SpeculatedType LazyOperandValueProfileParser::prediction(const ConcurrentJSLocker& locker, const LazyOperandValueProfileKey& key) const
{
    LazyOperandValueProfile* profile = getIfPresent(key);
    if (!profile)
        return SpecNone;

    return profile->computeUpdatedPrediction(locker);
}

} // namespace JSC

