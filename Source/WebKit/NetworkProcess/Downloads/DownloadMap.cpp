/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "DownloadMap.h"

#if ENABLE(TAKE_UNBOUNDED_NETWORKING_ASSERTION)

#include "Download.h"
#include "Logging.h"

namespace WebKit {


Download* DownloadMap::get(DownloadID downloadID) const
{
    return m_downloads.get(downloadID);
}

bool DownloadMap::isEmpty() const
{
    return m_downloads.isEmpty();
}

uint64_t DownloadMap::size() const
{
    return m_downloads.size();
}

bool DownloadMap::contains(DownloadID downloadID) const
{
    return m_downloads.contains(downloadID);
}

DownloadMap::DownloadMapType::AddResult DownloadMap::add(DownloadID downloadID, std::unique_ptr<Download>&& download)
{
    RELEASE_LOG(Loading, "Adding download %" PRIu64 " to NetworkProcess DownloadMap", downloadID.toUInt64());

    auto result = m_downloads.add(downloadID, WTFMove(download));
    if (m_downloads.size() == 1) {
        ASSERT(!m_downloadAssertion);
        m_downloadAssertion = ProcessAssertion::create(getpid(), "WebKit downloads"_s, ProcessAssertionType::UnboundedNetworking);
        RELEASE_LOG(ProcessSuspension, "Took 'WebKit downloads' assertion in NetworkProcess");
    }

    return result;
}

bool DownloadMap::remove(DownloadID downloadID)
{
    RELEASE_LOG(Loading, "Removing download %" PRIu64 " from NetworkProcess DownloadMap", downloadID.toUInt64());

    auto result = m_downloads.remove(downloadID);
    if (m_downloads.isEmpty()) {
        ASSERT(m_downloadAssertion);
        m_downloadAssertion = nullptr;
        RELEASE_LOG(ProcessSuspension, "Dropped 'WebKit downloads' assertion in NetworkProcess");
    }
    
    return result;
}

auto DownloadMap::values() -> DownloadMapType::ValuesIteratorRange
{
    return m_downloads.values();
}

} // namespace WebKit

#endif // ENABLE(TAKE_UNBOUNDED_NETWORKING_ASSERTION)
