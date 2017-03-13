/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteWebInspectorProxy.h"

#if PLATFORM(MAC) && WK_API_ENABLED

#import "RemoteWebInspectorProxyMessages.h"
#import "RemoteWebInspectorUIMessages.h"
#import "WKFrameInfo.h"
#import "WKNavigationAction.h"
#import "WKNavigationDelegate.h"
#import "WKWebInspectorWKWebView.h"
#import "WKWebViewInternal.h"
#import "WebInspectorProxy.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import <wtf/text/Base64.h>

using namespace WebKit;

@interface WKRemoteWebInspectorProxyObjCAdapter : NSObject <WKNavigationDelegate> {
    RemoteWebInspectorProxy* _inspectorProxy;
}
- (instancetype)initWithRemoteWebInspectorProxy:(RemoteWebInspectorProxy*)inspectorProxy;
@end

@implementation WKRemoteWebInspectorProxyObjCAdapter

- (instancetype)initWithRemoteWebInspectorProxy:(RemoteWebInspectorProxy*)inspectorProxy
{
    if (!(self = [super init]))
        return nil;

    _inspectorProxy = inspectorProxy;

    return self;
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    _inspectorProxy->closeFromCrash();
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    // Allow non-main frames to navigate anywhere.
    if (!navigationAction.targetFrame.isMainFrame) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    // Allow loading of the main inspector file.
    if (WebInspectorProxy::isMainOrTestInspectorPage(navigationAction.request.URL)) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    // Prevent everything else.
    decisionHandler(WKNavigationActionPolicyCancel);
}

@end

namespace WebKit {

WebPageProxy* RemoteWebInspectorProxy::platformCreateFrontendPageAndWindow()
{
    NSRect initialFrame = NSMakeRect(0, 0, WebInspectorProxy::initialWindowWidth, WebInspectorProxy::initialWindowHeight);
    auto configuration = WebInspectorProxy::createFrontendConfiguration(nullptr, false);

    m_objCAdapter = adoptNS([[WKRemoteWebInspectorProxyObjCAdapter alloc] initWithRemoteWebInspectorProxy:this]);

    m_webView = adoptNS([[WKWebInspectorWKWebView alloc] initWithFrame:initialFrame configuration:configuration.get()]);
    [m_webView setNavigationDelegate:m_objCAdapter.get()];

    m_window = WebInspectorProxy::createFrontendWindow(NSZeroRect);

    NSView *contentView = [m_window contentView];
    [m_webView setFrame:[contentView bounds]];
    [m_webView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [contentView addSubview:m_webView.get()];

    return m_webView->_page.get();
}

void RemoteWebInspectorProxy::platformCloseFrontendPageAndWindow()
{
    if (m_window) {
        [m_window setDelegate:nil];
        [m_window close];
        m_window = nil;
    }

    if (m_webView) {
        WebPageProxy* inspectorPage = m_webView->_page.get();
        inspectorPage->close();
        [m_webView setNavigationDelegate:nil];
        m_webView = nil;
    }

    if (m_objCAdapter)
        m_objCAdapter = nil;
}


void RemoteWebInspectorProxy::platformBringToFront()
{
    [m_window makeKeyAndOrderFront:nil];
    [m_window makeFirstResponder:m_webView.get()];
}

void RemoteWebInspectorProxy::platformSave(const String& suggestedURL, const String& content, bool base64Encoded, bool forceSaveDialog)
{
    // FIXME: Share with WebInspectorProxyMac.

    ASSERT(!suggestedURL.isEmpty());
    
    NSURL *platformURL = m_suggestedToActualURLMap.get(suggestedURL).get();
    if (!platformURL) {
        platformURL = [NSURL URLWithString:suggestedURL];
        // The user must confirm new filenames before we can save to them.
        forceSaveDialog = true;
    }
    
    ASSERT(platformURL);
    if (!platformURL)
        return;

    // Necessary for the block below.
    String suggestedURLCopy = suggestedURL;
    String contentCopy = content;

    auto saveToURL = ^(NSURL *actualURL) {
        ASSERT(actualURL);

        m_suggestedToActualURLMap.set(suggestedURLCopy, actualURL);

        if (base64Encoded) {
            Vector<char> out;
            if (!base64Decode(contentCopy, out, Base64ValidatePadding))
                return;
            RetainPtr<NSData> dataContent = adoptNS([[NSData alloc] initWithBytes:out.data() length:out.size()]);
            [dataContent writeToURL:actualURL atomically:YES];
        } else
            [contentCopy writeToURL:actualURL atomically:YES encoding:NSUTF8StringEncoding error:NULL];

        WebPageProxy* inspectorPage = m_webView->_page.get();
        inspectorPage->process().send(Messages::RemoteWebInspectorUI::DidSave([actualURL absoluteString]), inspectorPage->pageID());
    };

    if (!forceSaveDialog) {
        saveToURL(platformURL);
        return;
    }

    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = platformURL.lastPathComponent;
    panel.directoryURL = [platformURL URLByDeletingLastPathComponent];

    auto completionHandler = ^(NSInteger result) {
        if (result == NSModalResponseCancel)
            return;
        ASSERT(result == NSModalResponseOK);
        saveToURL(panel.URL);
    };

    if (m_window)
        [panel beginSheetModalForWindow:m_window.get() completionHandler:completionHandler];
    else
        completionHandler([panel runModal]);
}

void RemoteWebInspectorProxy::platformAppend(const String& suggestedURL, const String& content)
{
    // FIXME: Share with WebInspectorProxyMac.

    ASSERT(!suggestedURL.isEmpty());
    
    RetainPtr<NSURL> actualURL = m_suggestedToActualURLMap.get(suggestedURL);
    // Do not append unless the user has already confirmed this filename in save().
    if (!actualURL)
        return;

    NSFileHandle *handle = [NSFileHandle fileHandleForWritingToURL:actualURL.get() error:NULL];
    [handle seekToEndOfFile];
    [handle writeData:[content dataUsingEncoding:NSUTF8StringEncoding]];
    [handle closeFile];

    WebPageProxy* inspectorPage = m_webView->_page.get();
    inspectorPage->process().send(Messages::RemoteWebInspectorUI::DidAppend([actualURL absoluteString]), inspectorPage->pageID());
}

void RemoteWebInspectorProxy::platformStartWindowDrag()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    m_webView->_page->startWindowDrag();
#endif
}

void RemoteWebInspectorProxy::platformOpenInNewTab(const String& url)
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:url]];
}

} // namespace WebKit

#endif
