/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebInspectorClient.h"

#if PLATFORM(IOS)

#import "WebFrameInternal.h"
#import "WebInspector.h"
#import "WebNSNotificationCenterExtras.h"
#import "WebNodeHighlighter.h"
#import "WebViewInternal.h"
#import <WebCore/InspectorController.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/Settings.h>
#import <WebCore/WebCoreThread.h>
#import <wtf/PassOwnPtr.h>

#if ENABLE(REMOTE_INSPECTOR)
#import "WebInspectorClientRegistry.h"
#import "WebInspectorRemoteChannel.h"
#endif

using namespace WebCore;

WebInspectorClient::WebInspectorClient(WebView *webView)
    : m_webView(webView)
    , m_highlighter(AdoptNS, [[WebNodeHighlighter alloc] initWithInspectedWebView:webView])
    , m_frontendPage(0)
    , m_frontendClient(0)
#if ENABLE(REMOTE_INSPECTOR)
    , m_remoteChannel(0)
    , m_pageId(0)
#endif
{
#if ENABLE(REMOTE_INSPECTOR)
    [[WebInspectorClientRegistry sharedRegistry] registerClient:this];
#endif
}

void WebInspectorClient::inspectorDestroyed()
{
#if ENABLE(REMOTE_INSPECTOR)
    [[WebInspectorClientRegistry sharedRegistry] unregisterClient:this];
    teardownRemoteConnection(true);
#endif

    delete this;
}

InspectorFrontendChannel* WebInspectorClient::openInspectorFrontend(InspectorController*)
{
    // iOS does not have a local inspector, this should not be reached.
    ASSERT_NOT_REACHED();
    return nullptr;
}

void WebInspectorClient::bringFrontendToFront()
{
    // iOS does not have a local inspector, nothing to do here.
}

void WebInspectorClient::closeInspectorFrontend()
{
    // iOS does not have a local inspector, nothing to do here.
}

void WebInspectorClient::didResizeMainFrame(Frame*)
{
    // iOS does not have a local inspector, nothing to do here.
}

void WebInspectorClient::highlight()
{
    [m_highlighter.get() highlight];
}

void WebInspectorClient::hideHighlight()
{
    [m_highlighter.get() hideHighlight];
}

void WebInspectorClient::didSetSearchingForNode(bool enabled)
{
    WebInspector *inspector = [m_webView inspector];

    if (enabled)
        [[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:WebInspectorDidStartSearchingForNode object:inspector];
    else
        [[NSNotificationCenter defaultCenter] postNotificationOnMainThreadWithName:WebInspectorDidStopSearchingForNode object:inspector];
}

#pragma mark -
#pragma mark Remote Web Inspector Implementation

#if ENABLE(REMOTE_INSPECTOR)
bool WebInspectorClient::sendMessageToFrontend(const String& message)
{
    if (m_remoteChannel) {
        [m_remoteChannel sendMessageToFrontend:message];
        return true;
    }

    // iOS does not have a local inspector. There should be no way to reach this.
    ASSERT_NOT_REACHED();
    notImplemented();

    return doDispatchMessageOnFrontendPage(m_frontendPage, message);
}

void WebInspectorClient::sendMessageToBackend(const String& message)
{
    ASSERT(m_remoteChannel);

    Page* page = core(m_webView);
    page->inspectorController()->dispatchMessageFromFrontend(message);
}

bool WebInspectorClient::setupRemoteConnection(WebInspectorRemoteChannel *remoteChannel)
{
    // There is already a local session, do not allow a remote session.
    if (hasLocalSession())
        return false;

    // There is already a remote session, do not allow a new remote session.
    if (m_remoteChannel)
        return false;

    m_remoteChannel = remoteChannel;

    Page* page = core(m_webView);
    page->inspectorController()->connectFrontend(this);

    // Force developer extras to be enabled in WebCore when a remote connection starts.
    if (page->settings())
        page->settings()->setDeveloperExtrasEnabled(true);

    return true;
}

void WebInspectorClient::teardownRemoteConnection(bool fromLocalSide)
{
    ASSERT(WebThreadIsLockedOrDisabled());
    if (!m_remoteChannel)
        return;

    if (fromLocalSide)
        [m_remoteChannel closeFromLocalSide];

    Page* page = core(m_webView);
    if (page) {
        page->inspectorController()->disconnectFrontend();

        // Restore developer extras setting in WebCore.
        if (page && page->settings())
            page->settings()->setDeveloperExtrasEnabled([[m_webView preferences] developerExtrasEnabled]);
    }

    if (fromLocalSide)
        [m_remoteChannel release];

    m_remoteChannel = 0;
}

bool WebInspectorClient::hasLocalSession() const
{
    return m_frontendPage != 0;
}

bool WebInspectorClient::canBeRemotelyInspected() const
{
    return [m_webView canBeRemotelyInspected];
}

WebView *WebInspectorClient::inspectedWebView()
{
    return m_webView;
}
#endif


#pragma mark -
#pragma mark WebInspectorFrontendClient Implementation

WebInspectorFrontendClient::WebInspectorFrontendClient(WebView* inspectedWebView, WebInspectorWindowController* windowController, InspectorController* inspectorController, Page* frontendPage, WTF::PassOwnPtr<Settings> settings)
    : InspectorFrontendClientLocal(inspectorController,  frontendPage, settings)
{
    // iOS does not have a local inspector, this should not be reached.
    notImplemented();
}

void WebInspectorFrontendClient::attachAvailabilityChanged(bool) { }
void WebInspectorFrontendClient::frontendLoaded() { }
String WebInspectorFrontendClient::localizedStringsURL() { return String(); }
void WebInspectorFrontendClient::bringToFront() { }
void WebInspectorFrontendClient::closeWindow() { }
void WebInspectorFrontendClient::disconnectFromBackend() { }
void WebInspectorFrontendClient::attachWindow(DockSide) { }
void WebInspectorFrontendClient::detachWindow() { }
void WebInspectorFrontendClient::setAttachedWindowHeight(unsigned) { }
void WebInspectorFrontendClient::setAttachedWindowWidth(unsigned) { }
void WebInspectorFrontendClient::setToolbarHeight(unsigned) { }
void WebInspectorFrontendClient::inspectedURLChanged(const String&) { }
void WebInspectorFrontendClient::updateWindowTitle() const { }
void WebInspectorFrontendClient::save(const String&, const String&, bool) { }
void WebInspectorFrontendClient::append(const String&, const String&) { }

#endif // PLATFORM(IOS)
