/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "DownloadID.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(IOS_FAMILY)
#include <objc/objc.h>
#endif

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {

class DownloadProxy;
class NetworkProcessProxy;
class ProcessAssertion;
class WebPageProxy;
class WebProcessPool;
class WebsiteDataStore;
struct FrameInfoData;

class DownloadProxyMap : public CanMakeWeakPtr<DownloadProxyMap> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DownloadProxyMap);

public:
    explicit DownloadProxyMap(NetworkProcessProxy&);
    ~DownloadProxyMap();

    DownloadProxy& createDownloadProxy(WebsiteDataStore&, WebProcessPool&, const WebCore::ResourceRequest&, const FrameInfoData&, WebPageProxy* originatingPage);
    void downloadFinished(DownloadProxy&);

    bool isEmpty() const { return m_downloads.isEmpty(); }

    void invalidate();

    void applicationDidEnterBackground();
    void applicationWillEnterForeground();

private:
    void platformCreate();
    void platformDestroy();

    NetworkProcessProxy& m_process;
    HashMap<DownloadID, RefPtr<DownloadProxy>> m_downloads;

    bool m_shouldTakeAssertion { false };
    RefPtr<ProcessAssertion> m_downloadUIAssertion;
    RefPtr<ProcessAssertion> m_downloadNetworkingAssertion;

#if PLATFORM(IOS_FAMILY)
    RetainPtr<id> m_backgroundObserver;
    RetainPtr<id> m_foregroundObserver;
#endif
};

} // namespace WebKit
