/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "FrameIdentifier.h"
#include "FrameTree.h"
#include "PageIdentifier.h"
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMWindow;
class FrameView;
class FrameLoadRequest;
class HTMLFrameOwnerElement;
class NavigationScheduler;
class Page;
class Settings;
class WeakPtrImplWithEventTargetData;
class WindowProxy;

class Frame : public ThreadSafeRefCounted<Frame, WTF::DestructionThread::Main>, public CanMakeWeakPtr<Frame> {
public:
    virtual ~Frame();

    enum class FrameType : bool { Local, Remote };
    FrameType frameType() const { return m_frameType; }

    WindowProxy& windowProxy() { return m_windowProxy; }
    const WindowProxy& windowProxy() const { return m_windowProxy; }
    DOMWindow* window() const { return virtualWindow(); }
    FrameTree& tree() const { return m_treeNode; }
    FrameIdentifier frameID() const { return m_frameID; }
    inline Page* page() const; // Defined in Page.h.
    WEBCORE_EXPORT std::optional<PageIdentifier> pageID() const;
    Settings& settings() const { return m_settings.get(); }
    Frame& mainFrame() const { return m_mainFrame; }
    bool isMainFrame() const { return this == &m_mainFrame; }
    WEBCORE_EXPORT bool isRootFrame() const;

    WEBCORE_EXPORT void detachFromPage();
    inline HTMLFrameOwnerElement* ownerElement() const; // Defined in HTMLFrameOwnerElement.h.
    WEBCORE_EXPORT void disconnectOwnerElement();
    NavigationScheduler& navigationScheduler() const { return m_navigationScheduler.get(); }
    WEBCORE_EXPORT void takeWindowProxyFrom(Frame&);

    virtual void frameDetached() = 0;
    virtual bool preventsParentFromBeingComplete() const = 0;
    virtual void changeLocation(FrameLoadRequest&&) = 0;
    virtual void broadcastFrameRemovalToOtherProcesses() = 0;

    virtual FrameView* virtualView() const = 0;

protected:
    Frame(Page&, FrameIdentifier, FrameType, HTMLFrameOwnerElement*, Frame* parent);
    void resetWindowProxy();

private:
    virtual DOMWindow* virtualWindow() const = 0;

    WeakPtr<Page> m_page;
    const FrameIdentifier m_frameID;
    mutable FrameTree m_treeNode;
    Ref<WindowProxy> m_windowProxy;
    WeakPtr<HTMLFrameOwnerElement, WeakPtrImplWithEventTargetData> m_ownerElement;
    Frame& m_mainFrame;
    const Ref<Settings> m_settings;
    FrameType m_frameType;
    mutable UniqueRef<NavigationScheduler> m_navigationScheduler;
};

} // namespace WebCore
