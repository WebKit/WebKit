/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/TrackingRef.h>
#include <wtf/text/TextStream.h>

namespace WTF {

template <typename T>
class TrackableRefCounted {
public:
    void recordRefOperation(T& refCountedObject, TrackingReferenceOperation operation, HolderContainer holderContainer, String holderName)
    {
        m_records.append({ refCountedObject, operation, holderContainer, WTFMove(holderName), notFound });
    }

    void dumpRefOperationRecords(String listPrefix = ""_s, String rowPrefix = ""_s)
    {
        const size_t len = m_records.size();
        if (!len) {
            ALWAYS_LOG_WITH_STREAM(stream << listPrefix << (listPrefix.isEmpty() ? "" : " - ") << "no operations");
            return;
        }

        for (size_t indexToStartMatch = 0; indexToStartMatch < len - 1; ++indexToStartMatch) {
            auto& recordToMatch = m_records[indexToStartMatch];
            if (recordToMatch.matchingIndex != notFound)
                continue;
            if (recordToMatch.operation != TrackingReferenceOperation::ref)
                continue;
            for (size_t indexToLookForMatching = indexToStartMatch + 1; indexToLookForMatching < len; ++indexToLookForMatching) {
                auto& recordPossiblyMatching = m_records[indexToLookForMatching];
                if (recordPossiblyMatching.operation == TrackingReferenceOperation::deref && recordPossiblyMatching.holderContainer == recordToMatch.holderContainer && recordPossiblyMatching.matchingIndex == notFound && recordPossiblyMatching.holderName == recordToMatch.holderName) {
                    recordToMatch.matchingIndex = indexToLookForMatching;
                    recordPossiblyMatching.matchingIndex = indexToStartMatch;
                    break;
                }
            }
        }

        constexpr auto toString = [](TrackingReferenceOperation op) {
            switch (op) {
            case TrackingReferenceOperation::ref: return "ref"_s;
            case TrackingReferenceOperation::deref: return "deref"_s;
            default: return "?"_s;
            }
        };

        size_t alives = 0;
        for (const auto& record : m_records)
            if (record.matchingIndex == notFound)
                ++alives;

        ALWAYS_LOG_WITH_STREAM(stream << listPrefix << (listPrefix.isEmpty() ? "" : " - ") << len << " operation" << ((len > 1) ? "s: (" : ": (") << alives << " alive)");
        for (size_t i = 0; i < m_records.size(); ++i) {
            const auto& record = m_records[i];
            ALWAYS_LOG_WITH_STREAM(stream << rowPrefix << (listPrefix.isEmpty() ? "#" : " #") << (i + 1) << ": " << &record.refCountedObject << "." << toString(record.operation) << " by " << record.holderName << " @" << reinterpret_cast<const void*>(record.holderContainer.m_value); if (record.matchingIndex == notFound) stream << " alive"; else stream << " matching with #" << (record.matchingIndex + 1));
        }
    }

private:
    struct Record {
        T& refCountedObject;
        TrackingReferenceOperation operation;
        HolderContainer holderContainer;
        String holderName;
        size_t matchingIndex;
    };
    Vector<Record> m_records;
};

} // namespace WTF
