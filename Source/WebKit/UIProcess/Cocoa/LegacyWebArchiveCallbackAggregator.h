/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/FrameIdentifier.h>
#include <wtf/CompletionHandler.h>

namespace WebKit {

class LegacyWebArchiveCallbackAggregator final : public ThreadSafeRefCounted<LegacyWebArchiveCallbackAggregator, WTF::DestructionThread::MainRunLoop> {
public:
    static Ref<LegacyWebArchiveCallbackAggregator> create(WebCore::FrameIdentifier rootFrameIdentifier, HashMap<WebCore::FrameIdentifier, Ref<WebCore::LegacyWebArchive>>&& frameArchives, CompletionHandler<void(RefPtr<LegacyWebArchive>&&)>&& callback)
    {
        return adoptRef(*new LegacyWebArchiveCallbackAggregator(rootFrameIdentifier, WTFMove(frameArchives), WTFMove(callback)));
    }

    RefPtr<WebCore::LegacyWebArchive> completeFrameArchive(WebCore::FrameIdentifier identifier)
    {
        RefPtr archive = m_frameArchives.take(identifier);
        if (!archive)
            return archive;

        for (auto subframeIdentifier : archive->subframeIdentifiers()) {
            if (auto subframeArchive = completeFrameArchive(subframeIdentifier))
                archive->appendSubframeArchive(subframeArchive.releaseNonNull());
        }

        return archive;
    }

    ~LegacyWebArchiveCallbackAggregator()
    {
        RELEASE_ASSERT(m_callback);
        m_callback(completeFrameArchive(m_rootFrameIdentifier));
    }

    void addResult(HashMap<WebCore::FrameIdentifier, Ref<WebCore::LegacyWebArchive>>&& frameArchives)
    {
        for (auto&& [frameIdentifier, archive] : frameArchives)
            m_frameArchives.set(frameIdentifier, WTFMove(archive));
    }

private:
    LegacyWebArchiveCallbackAggregator(WebCore::FrameIdentifier rootFrameIdentifier, HashMap<WebCore::FrameIdentifier, Ref<WebCore::LegacyWebArchive>>&& frameArchives, CompletionHandler<void(RefPtr<LegacyWebArchive>&&)>&& callback)
        : m_rootFrameIdentifier(rootFrameIdentifier)
        , m_frameArchives(WTFMove(frameArchives))
        , m_callback(WTFMove(callback))
    {
    }

    WebCore::FrameIdentifier m_rootFrameIdentifier;
    HashMap<WebCore::FrameIdentifier, Ref<WebCore::LegacyWebArchive>> m_frameArchives;
    CompletionHandler<void(RefPtr<WebCore::LegacyWebArchive>&&)> m_callback;
};

} // namespace WebKit
