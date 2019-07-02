/*
 * Copyright (C) 2006-2008, 2010, 2015 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#if PLATFORM(IOS_FAMILY)

#import "WebFrameInternal.h"
#import "WebInspector.h"
#import "WebNodeHighlighter.h"
#import "WebViewInternal.h"
#import <WebCore/CertificateInfo.h>
#import <WebCore/FloatRect.h>
#import <WebCore/InspectorController.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/Settings.h>
#import <WebCore/WebCoreThread.h>

using namespace WebCore;

WebInspectorClient::WebInspectorClient(WebView* inspectedWebView)
    : m_inspectedWebView(inspectedWebView)
    , m_highlighter(adoptNS([[WebNodeHighlighter alloc] initWithInspectedWebView:inspectedWebView]))
{
}

void WebInspectorClient::inspectedPageDestroyed()
{
    delete this;
}

Inspector::FrontendChannel* WebInspectorClient::openLocalFrontend(InspectorController*)
{
    // iOS does not have a local inspector, this should not be reached.
    ASSERT_NOT_REACHED();
    return nullptr;
}

void WebInspectorClient::bringFrontendToFront()
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

void WebInspectorClient::showInspectorIndication()
{
    [m_inspectedWebView setShowingInspectorIndication:YES];
}

void WebInspectorClient::hideInspectorIndication()
{
    [m_inspectedWebView setShowingInspectorIndication:NO];
}

void WebInspectorClient::setShowPaintRects(bool)
{
    // FIXME: implement.
}

void WebInspectorClient::showPaintRect(const FloatRect&)
{
    // FIXME: need to do CALayer-based highlighting of paint rects.
}

void WebInspectorClient::didSetSearchingForNode(bool enabled)
{
    WebInspector *inspector = [m_inspectedWebView inspector];
    NSString *notificationName = enabled ? WebInspectorDidStartSearchingForNode : WebInspectorDidStopSearchingForNode;
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:notificationName object:inspector];
    });
}

#pragma mark -
#pragma mark WebInspectorFrontendClient Implementation

WebInspectorFrontendClient::WebInspectorFrontendClient(WebView* inspectedWebView, WebInspectorWindowController* frontendWindowController, InspectorController* inspectedPageController, Page* frontendPage, std::unique_ptr<Settings> settings)
    : InspectorFrontendClientLocal(inspectedPageController, frontendPage, WTFMove(settings))
{
    // iOS does not have a local inspector, this should not be reached.
    notImplemented();
}

void WebInspectorFrontendClient::attachAvailabilityChanged(bool) { }
void WebInspectorFrontendClient::frontendLoaded() { }
String WebInspectorFrontendClient::localizedStringsURL() { return String(); }
void WebInspectorFrontendClient::bringToFront() { }
void WebInspectorFrontendClient::closeWindow() { }
void WebInspectorFrontendClient::reopen() { }
void WebInspectorFrontendClient::resetState() { }
void WebInspectorFrontendClient::attachWindow(DockSide) { }
void WebInspectorFrontendClient::detachWindow() { }
void WebInspectorFrontendClient::setAttachedWindowHeight(unsigned) { }
void WebInspectorFrontendClient::setAttachedWindowWidth(unsigned) { }
void WebInspectorFrontendClient::setSheetRect(const FloatRect&) { }
void WebInspectorFrontendClient::startWindowDrag() { }
void WebInspectorFrontendClient::inspectedURLChanged(const String&) { }
void WebInspectorFrontendClient::showCertificate(const CertificateInfo&) { }
void WebInspectorFrontendClient::updateWindowTitle() const { }
void WebInspectorFrontendClient::save(const String&, const String&, bool, bool) { }
void WebInspectorFrontendClient::append(const String&, const String&) { }

#endif // PLATFORM(IOS_FAMILY)
