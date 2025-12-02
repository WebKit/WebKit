/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>

namespace WebKit {

class BrowsingContextGroup;
class WebPreferences;
class WebProcessProxy;
enum class InjectBrowsingContextIntoProcess : bool;
enum class LoadedWebArchive : bool;

// Note: This object should only be referenced by WebFrameProxy because its destructor is an
// important part of managing the lifetime of a frame and the process used by the frame.
class FrameProcess : public RefCountedAndCanMakeWeakPtr<FrameProcess> {
public:
    ~FrameProcess();

    const std::optional<WebCore::Site>& site() const { return m_site; }
    const WebProcessProxy& process() const { return m_process.get(); }
    WebProcessProxy& process() { return m_process.get(); }
    bool isSharedProcess() const { return !m_site; }
    const WebCore::Site& sharedProcessMainFrameSite() const { ASSERT(!m_site); return m_mainFrameSite; }
    bool isArchiveProcess() const { return m_isArchiveProcess; }

private:
    friend class BrowsingContextGroup; // FrameProcess should not be created except by BrowsingContextGroup.
    static Ref<FrameProcess> create(WebProcessProxy& process, BrowsingContextGroup& group, const std::optional<WebCore::Site>& site, const WebCore::Site& mainFrameSite,
        const WebPreferences& preferences, LoadedWebArchive loadedWebArchive, InjectBrowsingContextIntoProcess injectBrowsingContextIntoProcess)
    {
        return adoptRef(*new FrameProcess(process, group, site, mainFrameSite, preferences, loadedWebArchive, injectBrowsingContextIntoProcess));
    }
    FrameProcess(WebProcessProxy&, BrowsingContextGroup&, const std::optional<WebCore::Site>&, const WebCore::Site& mainFrameSite, const WebPreferences&, LoadedWebArchive, InjectBrowsingContextIntoProcess);

    const Ref<WebProcessProxy> m_process;
    WeakPtr<BrowsingContextGroup> m_browsingContextGroup;
    const std::optional<WebCore::Site> m_site;
    const WebCore::Site m_mainFrameSite;
    bool m_isArchiveProcess;
};

}
