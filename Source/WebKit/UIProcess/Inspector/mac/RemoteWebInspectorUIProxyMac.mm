/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#import "RemoteWebInspectorUIProxy.h"

#if PLATFORM(MAC) && ENABLE(REMOTE_INSPECTOR)

#import "GlobalFindInPageState.h"
#import "RemoteWebInspectorUIMessages.h"
#import "RemoteWebInspectorUIProxyMessages.h"
#import <WebKit/WKFrameInfo.h>
#import "WKInspectorViewController.h"
#import <WebKit/WKNavigationAction.h>
#import <WebKit/WKNavigationDelegate.h>
#import "WKWebViewInternal.h"
#import "WebInspectorUIProxy.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "_WKInspectorConfigurationInternal.h"
#import <SecurityInterface/SFCertificatePanel.h>
#import <SecurityInterface/SFCertificateView.h>
#import <WebCore/CertificateInfo.h>
#import <WebCore/Color.h>
#import <wtf/BlockPtr.h>
#import <wtf/text/Base64.h>

@interface WKRemoteWebInspectorUIProxyObjCAdapter : NSObject <NSWindowDelegate, WKInspectorViewControllerDelegate> {
    WebKit::RemoteWebInspectorUIProxy* _inspectorProxy;
}
- (instancetype)initWithRemoteWebInspectorUIProxy:(WebKit::RemoteWebInspectorUIProxy*)inspectorProxy;
@end

@implementation WKRemoteWebInspectorUIProxyObjCAdapter

- (NSRect)window:(NSWindow *)window willPositionSheet:(NSWindow *)sheet usingRect:(NSRect)rect
{
    if (_inspectorProxy)
        return NSMakeRect(0, _inspectorProxy->sheetRect().height(), _inspectorProxy->sheetRect().width(), 0);
    return rect;
}

- (instancetype)initWithRemoteWebInspectorUIProxy:(WebKit::RemoteWebInspectorUIProxy*)inspectorProxy
{
    if (!(self = [super init]))
        return nil;

    _inspectorProxy = inspectorProxy;

    return self;
}

- (void)inspectorWKWebViewDidBecomeActive:(WKInspectorViewController *)inspectorViewController
{
    _inspectorProxy->didBecomeActive();
}

- (void)inspectorViewControllerInspectorDidCrash:(WKInspectorViewController *)inspectorViewController
{
    _inspectorProxy->closeFromCrash();
}

- (BOOL)inspectorViewControllerInspectorIsUnderTest:(WKInspectorViewController *)inspectorViewController
{
    return _inspectorProxy->isUnderTest();
}

@end

namespace WebKit {
using namespace WebCore;

WKWebView *RemoteWebInspectorUIProxy::webView() const
{
    return m_inspectorView.get().webView;
}

void RemoteWebInspectorUIProxy::didBecomeActive()
{
    m_inspectorPage->send(Messages::RemoteWebInspectorUI::UpdateFindString(WebKit::stringForFind()));
}

WebPageProxy* RemoteWebInspectorUIProxy::platformCreateFrontendPageAndWindow()
{
    m_objCAdapter = adoptNS([[WKRemoteWebInspectorUIProxyObjCAdapter alloc] initWithRemoteWebInspectorUIProxy:this]);

    Ref<API::InspectorConfiguration> configuration = m_client->configurationForRemoteInspector(*this);
    m_inspectorView = adoptNS([[WKInspectorViewController alloc] initWithConfiguration: WebKit::wrapper(configuration) inspectedPage:nullptr]);
    [m_inspectorView.get() setDelegate:m_objCAdapter.get()];

    m_window = WebInspectorUIProxy::createFrontendWindow(NSZeroRect, WebInspectorUIProxy::InspectionTargetType::Remote);
    [m_window setDelegate:m_objCAdapter.get()];
    [m_window setFrameAutosaveName:@"WKRemoteWebInspectorWindowFrame"];

    NSView *contentView = m_window.get().contentView;
    [webView() setFrame:contentView.bounds];
    [contentView addSubview:webView()];

    return webView()->_page.get();
}

void RemoteWebInspectorUIProxy::platformCloseFrontendPageAndWindow()
{
    if (m_window) {
        [m_window setDelegate:nil];
        [m_window close];
        m_window = nil;
    }

    if (m_inspectorView) {
        [m_inspectorView.get() setDelegate:nil];
        m_inspectorView = nil;
    }

    if (m_objCAdapter)
        m_objCAdapter = nil;
}

void RemoteWebInspectorUIProxy::platformResetState()
{
    [NSWindow removeFrameUsingName:[m_window frameAutosaveName]];
}

void RemoteWebInspectorUIProxy::platformBringToFront()
{
    [m_window makeKeyAndOrderFront:nil];
    [m_window makeFirstResponder:webView()];
}

void RemoteWebInspectorUIProxy::platformSave(Vector<InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs)
{
    RetainPtr<NSString> urlCommonPrefix;
    for (auto& item : saveDatas) {
        if (!urlCommonPrefix)
            urlCommonPrefix = item.url;
        else
            urlCommonPrefix = [urlCommonPrefix commonPrefixWithString:item.url options:0];
    }
    if ([urlCommonPrefix hasSuffix:@"."])
        urlCommonPrefix = [urlCommonPrefix substringToIndex:[urlCommonPrefix length] - 1];

    RetainPtr platformURL = m_suggestedToActualURLMap.get(urlCommonPrefix.get());
    if (!platformURL) {
        platformURL = [NSURL URLWithString:urlCommonPrefix.get()];
        // The user must confirm new filenames before we can save to them.
        forceSaveAs = true;
    }

    WebInspectorUIProxy::showSavePanel(m_window.get(), platformURL.get(), WTFMove(saveDatas), forceSaveAs, [urlCommonPrefix, protectedThis = Ref { *this }] (NSURL *actualURL) {
        protectedThis->m_suggestedToActualURLMap.set(urlCommonPrefix.get(), actualURL);
    });
}

void RemoteWebInspectorUIProxy::platformLoad(const String& path, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (auto contents = FileSystem::readEntireFile(path))
        completionHandler(String::adopt(WTFMove(*contents)));
    else
        completionHandler(nullString());
}

void RemoteWebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    auto sampler = adoptNS([[NSColorSampler alloc] init]);
    [sampler.get() showSamplerWithSelectionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSColor *selectedColor) mutable {
        if (!selectedColor) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(Color::createAndPreserveColorSpace(selectedColor.CGColor));
    }).get()];
}

void RemoteWebInspectorUIProxy::platformSetSheetRect(const FloatRect& rect)
{
    m_sheetRect = rect;
}

void RemoteWebInspectorUIProxy::platformSetForcedAppearance(InspectorFrontendClient::Appearance appearance)
{
    NSAppearance *platformAppearance;
    switch (appearance) {
    case InspectorFrontendClient::Appearance::System:
        platformAppearance = nil;
        break;

    case InspectorFrontendClient::Appearance::Light:
        platformAppearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
        break;

    case InspectorFrontendClient::Appearance::Dark:
        platformAppearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        break;
    }

    webView().appearance = platformAppearance;

    NSWindow *window = m_window.get();
    ASSERT(window);
    window.appearance = platformAppearance;
}

void RemoteWebInspectorUIProxy::platformStartWindowDrag()
{
    webView()->_page->startWindowDrag();
}

void RemoteWebInspectorUIProxy::platformOpenURLExternally(const String& url)
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:url]];
}

void RemoteWebInspectorUIProxy::platformRevealFileExternally(const String& path)
{
    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ [NSURL URLWithString:path] ]];
}

void RemoteWebInspectorUIProxy::platformShowCertificate(const CertificateInfo& certificateInfo)
{
    ASSERT(!certificateInfo.isEmpty());

    RetainPtr<SFCertificatePanel> certificatePanel = adoptNS([[SFCertificatePanel alloc] init]);

    ASSERT(m_window);
    [certificatePanel beginSheetForWindow:m_window.get() modalDelegate:nil didEndSelector:NULL contextInfo:nullptr trust:certificateInfo.trust().get() showGroup:YES];

    // This must be called after the trust panel has been displayed, because the certificateView doesn't exist beforehand.
    SFCertificateView *certificateView = [certificatePanel certificateView];
    [certificateView setDisplayTrust:YES];
    [certificateView setEditableTrust:NO];
    [certificateView setDisplayDetails:YES];
    [certificateView setDetailsDisclosed:YES];
}

} // namespace WebKit

#endif
