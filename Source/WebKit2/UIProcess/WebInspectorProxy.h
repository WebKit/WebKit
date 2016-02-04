/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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

#ifndef WebInspectorProxy_h
#define WebInspectorProxy_h

#include "APIObject.h"
#include "Attachment.h"
#include "MessageReceiver.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#include "WKGeometry.h"
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>

OBJC_CLASS NSURL;
OBJC_CLASS NSView;
OBJC_CLASS NSWindow;
OBJC_CLASS WKWebInspectorProxyObjCAdapter;
OBJC_CLASS WKWebInspectorWKWebView;
#endif

#if PLATFORM(GTK)
#include "WebInspectorClientGtk.h"
#endif

#if PLATFORM(EFL)
#include <Ecore_Evas.h>
#include <Evas.h>
#endif

namespace WebKit {

class WebFrameProxy;
class WebPageProxy;
class WebPreferences;
class WebProcessPool;

enum class AttachmentSide {
    Bottom,
    Right
};

class WebInspectorProxy : public API::ObjectImpl<API::Object::Type::Inspector>, public IPC::MessageReceiver {
public:
    static Ref<WebInspectorProxy> create(WebPageProxy* inspectedPage)
    {
        return adoptRef(*new WebInspectorProxy(inspectedPage));
    }

    ~WebInspectorProxy();

    void invalidate();

    // Public APIs
    WebPageProxy* inspectedPage() const { return m_inspectedPage; }

    bool isConnected() const { return !!m_inspectorPage; }
    bool isVisible() const { return m_isVisible; }
    bool isFront();

    void connect();

    void show();
    void hide();
    void close();

    void didRelaunchInspectorPageProcess();

#if PLATFORM(MAC) && WK_API_ENABLED
    void createInspectorWindow();
    void updateInspectorWindowTitle() const;
    void inspectedViewFrameDidChange(CGFloat = 0);
    void windowFrameDidChange();
    void windowFullScreenDidChange();
    NSWindow* inspectorWindow() const { return m_inspectorWindow.get(); }

    void setInspectorWindowFrame(WKRect&);
    WKRect inspectorWindowFrame();

    void closeTimerFired();

    void attachmentViewDidChange(NSView *oldView, NSView *newView);
#endif

#if PLATFORM(GTK)
    GtkWidget* inspectorView() const { return m_inspectorView; };
    void initializeInspectorClientGtk(const WKInspectorClientGtkBase*);
#endif

    void showConsole();
    void showResources();
    void showMainResourceForFrame(WebFrameProxy*);

    AttachmentSide attachmentSide() const { return m_attachmentSide; }
    bool isAttached() const { return m_isAttached; }
    void attachRight();
    void attachBottom();
    void attach(AttachmentSide = AttachmentSide::Bottom);
    void detach();

    void setAttachedWindowHeight(unsigned);
    void setAttachedWindowWidth(unsigned);

    void startWindowDrag();

    bool isProfilingPage() const { return m_isProfilingPage; }
    void togglePageProfiling();

    static bool isInspectorProcessPool(WebProcessPool&);
    static bool isInspectorPage(WebPageProxy&);

    // Provided by platform WebInspectorProxy implementations.
    static String inspectorPageURL();
    static String inspectorTestPageURL();
    static String inspectorBaseURL();

#if ENABLE(INSPECTOR_SERVER)
    void enableRemoteInspection();
    void remoteFrontendConnected();
    void remoteFrontendDisconnected();
    void dispatchMessageFromRemoteFrontend(const String& message);
    int remoteInspectionPageID() const { return m_remoteInspectionPageId; }
#endif

private:
    explicit WebInspectorProxy(WebPageProxy*);

    static WebProcessPool& inspectorProcessPool(unsigned inspectionLevel);

    void eagerlyCreateInspectorPage();

    // IPC::MessageReceiver
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    WebPageProxy* platformCreateInspectorPage();
    void platformOpen();
    void platformDidClose();
    void platformInvalidate();
    void platformBringToFront();
    void platformHide();
    bool platformIsFront();
    void platformAttachAvailabilityChanged(bool);
    void platformInspectedURLChanged(const String&);
    unsigned platformInspectedWindowHeight();
    unsigned platformInspectedWindowWidth();
    void platformAttach();
    void platformDetach();
    void platformSetAttachedWindowHeight(unsigned);
    void platformSetAttachedWindowWidth(unsigned);
    void platformStartWindowDrag();
    void platformSave(const String& filename, const String& content, bool base64Encoded, bool forceSaveAs);
    void platformAppend(const String& filename, const String& content);

#if PLATFORM(MAC) && WK_API_ENABLED
    bool platformCanAttach(bool webProcessCanAttach);
#else
    bool platformCanAttach(bool webProcessCanAttach) { return webProcessCanAttach; }
#endif

    // Called by WebInspectorProxy messages
    void createInspectorPage(IPC::Attachment, bool canAttach, bool underTest);
    void didClose();
    void bringToFront();
    void attachAvailabilityChanged(bool);
    void inspectedURLChanged(const String&);

    void save(const String& filename, const String& content, bool base64Encoded, bool forceSaveAs);
    void append(const String& filename, const String& content);

#if ENABLE(INSPECTOR_SERVER)
    void sendMessageToRemoteFrontend(const String& message);
#endif

    bool canAttach() const { return m_canAttach; }
    bool shouldOpenAttached();

    bool isUnderTest() const { return m_underTest; }

    void open();

    // The inspection level is used to give different preferences to each inspector
    // by setting a per-level page group identifier. Local storage settings in the frontend
    // also use the inspection level in the key prefix to disambiguate persistent view state.
    unsigned inspectionLevel() const;
    String inspectorPageGroupIdentifier() const;
    WebPreferences& inspectorPagePreferences() const;

#if PLATFORM(GTK) || PLATFORM(EFL)
    void createInspectorWindow();
#endif

#if PLATFORM(GTK)
    void updateInspectorWindowTitle() const;
#endif

    static const unsigned minimumWindowWidth;
    static const unsigned minimumWindowHeight;

    static const unsigned initialWindowWidth;
    static const unsigned initialWindowHeight;

    WebPageProxy* m_inspectedPage {nullptr};
    WebPageProxy* m_inspectorPage {nullptr};

    bool m_underTest {false};
    bool m_isVisible {false};
    bool m_isAttached {false};
    bool m_canAttach {false};
    bool m_isProfilingPage {false};
    bool m_showMessageSent {false};
    bool m_ignoreFirstBringToFront {false};

    IPC::Attachment m_connectionIdentifier;

    AttachmentSide m_attachmentSide {AttachmentSide::Bottom};

#if PLATFORM(MAC) && WK_API_ENABLED
    RetainPtr<WKWebInspectorWKWebView> m_inspectorView;
    RetainPtr<NSWindow> m_inspectorWindow;
    RetainPtr<WKWebInspectorProxyObjCAdapter> m_inspectorProxyObjCAdapter;
    HashMap<String, RetainPtr<NSURL>> m_suggestedToActualURLMap;
    RunLoop::Timer<WebInspectorProxy> m_closeTimer;
    String m_urlString;
#elif PLATFORM(GTK)
    WebInspectorClientGtk m_client;
    GtkWidget* m_inspectorView {nullptr};
    GtkWidget* m_inspectorWindow {nullptr};
    GtkWidget* m_headerBar {nullptr};
    String m_inspectedURLString;
#elif PLATFORM(EFL)
    Evas_Object* m_inspectorView {nullptr};
    Ecore_Evas* m_inspectorWindow {nullptr};
#endif
#if ENABLE(INSPECTOR_SERVER)
    int m_remoteInspectionPageId {0};
#endif
};

} // namespace WebKit

#endif // WebInspectorProxy_h
