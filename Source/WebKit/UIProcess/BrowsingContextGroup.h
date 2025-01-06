/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <WebCore/Site.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakListHashSet.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class FrameProcess;
class ProvisionalPageProxy;
class RemotePageProxy;
class WebPageProxy;
class WebPreferences;
class WebProcessProxy;

class BrowsingContextGroup : public RefCountedAndCanMakeWeakPtr<BrowsingContextGroup> {
public:
    static Ref<BrowsingContextGroup> create() { return adoptRef(*new BrowsingContextGroup()); }
    ~BrowsingContextGroup();

    Ref<FrameProcess> ensureProcessForSite(const WebCore::Site&, WebProcessProxy&, const WebPreferences&);
    Ref<FrameProcess> ensureProcessForConnection(IPC::Connection&, WebPageProxy&, const WebPreferences&);
    FrameProcess* processForSite(const WebCore::Site&);
    void addFrameProcess(FrameProcess&);
    void removeFrameProcess(FrameProcess&);
    void processDidTerminate(WebPageProxy&, WebProcessProxy&);

    void addPage(WebPageProxy&);
    void removePage(WebPageProxy&);
    void forEachRemotePage(const WebPageProxy&, Function<void(RemotePageProxy&)>&&);

    RemotePageProxy* remotePageInProcess(const WebPageProxy&, const WebProcessProxy&);

    RefPtr<RemotePageProxy> takeRemotePageInProcessForProvisionalPage(const WebPageProxy&, const WebProcessProxy&);
    void transitionPageToRemotePage(WebPageProxy&, const WebCore::Site& openerSite);
    void transitionProvisionalPageToRemotePage(ProvisionalPageProxy&, const WebCore::Site& provisionalNavigationFailureSite);

    bool hasRemotePages(const WebPageProxy&);

private:
    BrowsingContextGroup();

    HashMap<WebCore::Site, WeakPtr<FrameProcess>> m_processMap;
    WeakListHashSet<WebPageProxy> m_pages;
    WeakHashMap<WebPageProxy, HashSet<Ref<RemotePageProxy>>> m_remotePages;
};

}
