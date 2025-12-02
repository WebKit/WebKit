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

#include <WebCore/FrameIdentifier.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameTreeSyncData.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/CheckedRef.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMWindow;
class Event;
class FloatSize;
class FrameLoaderClient;
class FrameLoadRequest;
class FrameView;
class HTMLFrameOwnerElement;
class NavigationScheduler;
class Page;
class RenderWidget;
class Settings;
class WeakPtrImplWithEventTargetData;
class WindowProxy;

enum class AdjustViewSize : bool;

struct OwnerPermissionsPolicyData;

enum class AdvancedPrivacyProtections : uint16_t;
enum class AutoplayPolicy : uint8_t;
enum class ReferrerPolicy : uint8_t;
enum class SandboxFlag : uint16_t;
enum class ScrollbarMode : uint8_t;

using SandboxFlags = OptionSet<SandboxFlag>;

class Frame : public RefCountedAndCanMakeWeakPtr<Frame> {
public:
    virtual ~Frame();

    enum class AddToFrameTree : bool { No, Yes };
    enum class NotifyUIProcess : bool { No, Yes };
    enum class FrameType : bool { Local, Remote };
    FrameType frameType() const { return m_frameType; }

    WindowProxy& windowProxy() { return m_windowProxy; }
    const WindowProxy& windowProxy() const { return m_windowProxy; }
    Ref<WindowProxy> protectedWindowProxy() const;

    DOMWindow* window() const { return virtualWindow(); }
    RefPtr<DOMWindow> protectedWindow() const;
    FrameTree& tree() const { return m_treeNode; }
    FrameIdentifier frameID() const { return m_frameID; }
    inline Page* page() const; // Defined in DocumentPage.h.
    inline RefPtr<Page> protectedPage() const; // Defined in DocumentPage.h.
    inline std::optional<PageIdentifier> pageID() const; // Defined in DocumentPage.h.
    Settings& settings() const { return m_settings.get(); }
    Frame& mainFrame() { return *m_mainFrame; }
    const Frame& mainFrame() const { return *m_mainFrame; }
    Ref<Frame> protectedMainFrame() { return mainFrame(); }
    Ref<const Frame> protectedMainFrame() const { return mainFrame(); }
    bool isMainFrame() const { return this == m_mainFrame.get(); }
    WEBCORE_EXPORT void disownOpener();
    WEBCORE_EXPORT void updateOpener(Frame&, NotifyUIProcess = NotifyUIProcess::Yes);
    WEBCORE_EXPORT void setOpenerForWebKitLegacy(Frame*);
    const Frame* opener() const { return m_opener.get(); }
    Frame* opener() { return m_opener.get(); }
    bool hasOpenedFrames() const;
    WEBCORE_EXPORT void detachFromAllOpenedFrames();
    virtual bool isRootFrame() const = 0;
#if ASSERT_ENABLED
    WEBCORE_EXPORT static bool isRootFrameIdentifier(FrameIdentifier);
#endif

    WEBCORE_EXPORT void detachFromPage();

    WEBCORE_EXPORT void setOwnerElement(HTMLFrameOwnerElement*);
    inline HTMLFrameOwnerElement* ownerElement() const; // Defined in FrameInlines.h.
    inline RefPtr<HTMLFrameOwnerElement> protectedOwnerElement() const; // Defined in FrameInlines.h.

    WEBCORE_EXPORT void disconnectOwnerElement();
    NavigationScheduler& navigationScheduler() const { return m_navigationScheduler.get(); }
    Ref<NavigationScheduler> protectedNavigationScheduler() const;
    WEBCORE_EXPORT void takeWindowProxyAndOpenerFrom(Frame&);

    virtual void frameDetached() = 0;
    virtual bool preventsParentFromBeingComplete() const = 0;
    virtual void changeLocation(FrameLoadRequest&&) = 0;
    virtual void loadFrameRequest(FrameLoadRequest&&, Event*) = 0;
    virtual void didFinishLoadInAnotherProcess() = 0;

    virtual FrameView* virtualView() const = 0;
    WEBCORE_EXPORT RefPtr<FrameView> protectedVirtualView() const;
    virtual void disconnectView() = 0;
    virtual FrameLoaderClient& loaderClient() = 0;
    virtual void documentURLForConsoleLog(CompletionHandler<void(const URL&)>&&) = 0;

    virtual String customUserAgent() const = 0;
    virtual String customUserAgentAsSiteSpecificQuirks() const = 0;
    virtual String customNavigatorPlatform() const = 0;
    virtual OptionSet<AdvancedPrivacyProtections> advancedPrivacyProtections() const = 0;
    virtual AutoplayPolicy autoplayPolicy() const = 0;

    virtual void updateSandboxFlags(SandboxFlags, NotifyUIProcess);
    virtual void updateReferrerPolicy(ReferrerPolicy) { }

    WEBCORE_EXPORT RenderWidget* ownerRenderer() const; // Renderer for the element that contains this frame.

    WEBCORE_EXPORT void setOwnerPermissionsPolicy(OwnerPermissionsPolicyData&&);
    WEBCORE_EXPORT std::optional<OwnerPermissionsPolicyData> ownerPermissionsPolicy() const;

    virtual void updateScrollingMode() = 0;

    virtual void reportMixedContentViolation(bool blocked, const URL& target) const = 0;

    void stopForBackForwardCache();

    WEBCORE_EXPORT void updateFrameTreeSyncData(Ref<FrameTreeSyncData>&&);
    WEBCORE_EXPORT void updateFrameTreeSyncData(const FrameTreeSyncSerializationData&);

    virtual bool frameCanCreatePaymentSession() const;
    FrameTreeSyncData& frameTreeSyncData() const { return m_frameTreeSyncData.get(); }
    Ref<FrameTreeSyncData> protectedFrameTreeSyncData() const { return m_frameTreeSyncData.get(); }
    WEBCORE_EXPORT virtual RefPtr<SecurityOrigin> frameDocumentSecurityOrigin() const = 0;
    WEBCORE_EXPORT virtual String frameURLProtocol() const = 0;

    WEBCORE_EXPORT virtual void setPrinting(bool printing, FloatSize pageSize, FloatSize originalPageSize, float maximumShrinkRatio, AdjustViewSize, NotifyUIProcess = NotifyUIProcess::Yes);
    WEBCORE_EXPORT bool isPrinting() const;

protected:
    Frame(Page&, FrameIdentifier, FrameType, HTMLFrameOwnerElement*, Frame* parent, Frame* opener, Ref<FrameTreeSyncData>&&, AddToFrameTree = AddToFrameTree::Yes);
    void resetWindowProxy();

    virtual void frameWasDisconnectedFromOwner() const { }

private:
    virtual DOMWindow* virtualWindow() const = 0;
    virtual void reinitializeDocumentSecurityContext() = 0;

    WeakPtr<Page> m_page;
    const FrameIdentifier m_frameID;
    mutable FrameTree m_treeNode;
    Ref<WindowProxy> m_windowProxy;
    WeakPtr<HTMLFrameOwnerElement, WeakPtrImplWithEventTargetData> m_ownerElement;
    const WeakPtr<Frame> m_mainFrame;
    const Ref<Settings> m_settings;
    FrameType m_frameType;
    mutable UniqueRef<NavigationScheduler> m_navigationScheduler;
    WeakPtr<Frame> m_opener;
    WeakHashSet<Frame> m_openedFrames;
    std::unique_ptr<OwnerPermissionsPolicyData> m_ownerPermisssionsPolicyOverride;
    bool m_isPrinting { false };

    Ref<FrameTreeSyncData> m_frameTreeSyncData;
};

} // namespace WebCore
