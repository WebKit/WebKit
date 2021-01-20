/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StatisticsRequest.h"

#include "APIArray.h"
#include "APIDictionary.h"

namespace WebKit {

StatisticsRequest::StatisticsRequest(Ref<DictionaryCallback>&& callback)
    : m_callback(WTFMove(callback))
{
}

StatisticsRequest::~StatisticsRequest()
{
    if (m_callback)
        m_callback->invalidate();
}

uint64_t StatisticsRequest::addOutstandingRequest()
{
    static std::atomic<int64_t> uniqueRequestID;

    uint64_t requestID = ++uniqueRequestID;
    m_outstandingRequests.add(requestID);
    return requestID;
}

static void addToDictionaryFromHashMap(API::Dictionary* dictionary, const HashMap<String, uint64_t>& map)
{
    HashMap<String, uint64_t>::const_iterator end = map.end();
    for (HashMap<String, uint64_t>::const_iterator it = map.begin(); it != end; ++it)
        dictionary->set(it->key, RefPtr<API::UInt64>(API::UInt64::create(it->value)).get());
}

static Ref<API::Dictionary> createDictionaryFromHashMap(const HashMap<String, uint64_t>& map)
{
    Ref<API::Dictionary> result = API::Dictionary::create();
    addToDictionaryFromHashMap(result.ptr(), map);
    return result;
}

void StatisticsRequest::completedRequest(uint64_t requestID, const StatisticsData& data)
{
    ASSERT(m_outstandingRequests.contains(requestID));
    m_outstandingRequests.remove(requestID);

    if (!m_responseDictionary)
        m_responseDictionary = API::Dictionary::create();
    
    // FIXME (Multi-WebProcess) <rdar://problem/13200059>: This code overwrites any previous response data received.
    // When getting responses from multiple WebProcesses we need to combine items instead of clobbering them.

    addToDictionaryFromHashMap(m_responseDictionary.get(), data.statisticsNumbers);

    if (!data.javaScriptProtectedObjectTypeCounts.isEmpty())
        m_responseDictionary->set("JavaScriptProtectedObjectTypeCounts", createDictionaryFromHashMap(data.javaScriptProtectedObjectTypeCounts));
    if (!data.javaScriptObjectTypeCounts.isEmpty())
        m_responseDictionary->set("JavaScriptObjectTypeCounts", createDictionaryFromHashMap(data.javaScriptObjectTypeCounts));

    if (!data.webCoreCacheStatistics.isEmpty()) {
        Vector<RefPtr<API::Object>> cacheStatistics;
        cacheStatistics.reserveInitialCapacity(data.webCoreCacheStatistics.size());

        for (const auto& statistic : data.webCoreCacheStatistics)
            cacheStatistics.uncheckedAppend(createDictionaryFromHashMap(statistic));

        m_responseDictionary->set("WebCoreCacheStatistics", API::Array::create(WTFMove(cacheStatistics)));
    }

    if (m_outstandingRequests.isEmpty()) {
        m_callback->performCallbackWithReturnValue(m_responseDictionary.get());
        m_callback = nullptr;
    }
}

} // namespace WebKit
