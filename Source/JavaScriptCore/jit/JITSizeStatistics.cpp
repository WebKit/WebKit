/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "JITSizeStatistics.h"

#if ENABLE(JIT)

#include "CCallHelpers.h"
#include "LinkBuffer.h"
#include <wtf/BubbleSort.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(JITSizeStatistics);

JITSizeStatistics::Marker JITSizeStatistics::markStart(String identifier, CCallHelpers& jit)
{
    Marker marker;
    marker.identifier = identifier;
    marker.start = jit.labelIgnoringWatchpoints();
    return marker;
}

void JITSizeStatistics::markEnd(Marker marker, CCallHelpers& jit)
{
    CCallHelpers::Label end = jit.labelIgnoringWatchpoints();
    jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf<NoPtrTag>(end).untaggedPtr<char*>() - linkBuffer.locationOf<NoPtrTag>(marker.start).untaggedPtr<char*>();
        linkBuffer.addMainThreadFinalizationTask([=, this] {
            auto& entry = m_data.add(marker.identifier, Entry { }).iterator->value;
            ++entry.count;
            entry.totalBytes += size;
        });
    });
}

void JITSizeStatistics::dump(PrintStream& out) const
{
    Vector<std::pair<String, Entry>> entries;

    for (auto pair : m_data)
        entries.append(std::make_pair(pair.key, pair.value));

    std::sort(entries.begin(), entries.end(), [] (const auto& lhs, const auto& rhs) {
        return lhs.second.totalBytes > rhs.second.totalBytes;
    });

    out.println("JIT size statistics:");
    out.println("==============================================");

    for (auto& entry : entries) {
        size_t totalBytes = entry.second.totalBytes;
        size_t count = entry.second.count;
        double avg = static_cast<double>(totalBytes) / static_cast<double>(count);
        out.println(entry.first, " totalBytes: ", totalBytes, " count: ", count, " avg: ", avg);
    }
}

} // namespace JSC

#endif // ENABLE(JIT)
