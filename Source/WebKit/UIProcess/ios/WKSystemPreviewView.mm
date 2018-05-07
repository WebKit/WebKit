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

#import "config.h"
#import "WKSystemPreviewView.h"

#if USE(SYSTEM_PREVIEW)

#import "APIFindClient.h"
#import "APIUIClient.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <WebCore/FloatRect.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/Vector.h>

using namespace WebCore;
using namespace WebKit;

SOFT_LINK_FRAMEWORK(QuickLook)
SOFT_LINK_CLASS(QuickLook, QLItem);
SOFT_LINK_CLASS(QuickLook, QLPreviewController);

@implementation WKSystemPreviewView {
    RetainPtr<QLPreviewController> _qlPreviewController;
    RetainPtr<NSItemProvider> _itemProvider;
    RetainPtr<QLItem> _item;
    RetainPtr<NSData> _data;
    RetainPtr<NSString> _suggestedFilename;

    WKWebView *_webView;
}

- (instancetype)web_initWithFrame:(CGRect)frame webView:(WKWebView *)webView
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    UIColor *backgroundColor = [UIColor colorWithRed:(38. / 255) green:(38. / 255) blue:(38. / 255) alpha:1];
    self.backgroundColor = backgroundColor;

    _webView = webView;

    UIScrollView *scrollView = webView.scrollView;
    [scrollView setMinimumZoomScale:1];
    [scrollView setMaximumZoomScale:1];
    [scrollView setBackgroundColor:backgroundColor];

    return self;
}

- (void)web_setContentProviderData:(NSData *)data suggestedFilename:(NSString *)filename
{
    RefPtr<WebKit::WebPageProxy> page = _webView->_page;
    UIViewController *presentingViewController = page->uiClient().presentingViewController();

    if (!presentingViewController)
        return;

    _suggestedFilename = adoptNS([filename copy]);
    _data = adoptNS([data copy]);

    NSString *extension = [[_suggestedFilename.get() pathExtension] lowercaseString];
    RetainPtr<CFStringRef> contentType = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (__bridge CFStringRef)extension, nil));

    _item = adoptNS([allocQLItemInstance() initWithDataProvider:self contentType:(NSString *)contentType.get() previewTitle:_suggestedFilename.get()]);

    _qlPreviewController = adoptNS([allocQLPreviewControllerInstance() init]);
    [_qlPreviewController setDelegate:self];
    [_qlPreviewController setDataSource:self];

    [presentingViewController presentViewController:_qlPreviewController.get() animated:YES completion:nullptr];
}

- (void)web_setMinimumSize:(CGSize)size
{
}

- (void)web_setOverlaidAccessoryViewsInset:(CGSize)inset
{
}

- (void)web_computedContentInsetDidChange
{
}

- (void)web_setFixedOverlayView:(UIView *)fixedOverlayView
{
}

- (void)web_didSameDocumentNavigation:(WKSameDocumentNavigationType)navigationType
{
}

- (UIView *)web_contentView
{
    return self;
}

#pragma mark Find-in-Page

- (void)web_countStringMatches:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount
{
    RefPtr<WebKit::WebPageProxy> page = _webView->_page;
    page->findClient().didCountStringMatches(page.get(), string, 0);
}

- (void)web_findString:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount
{
    RefPtr<WebKit::WebPageProxy> page = _webView->_page;
    page->findClient().didFailToFindString(page.get(), string);
}

- (void)web_hideFindUI
{
}

#pragma mark QLPreviewControllerDataSource

- (NSInteger)numberOfPreviewItemsInPreviewController:(QLPreviewController *)controller
{
    return 1;
}

- (id<QLPreviewItem>)previewController:(QLPreviewController *)controller previewItemAtIndex:(NSInteger)index
{
    return static_cast<id<QLPreviewItem>>(_item.get());
}

#pragma mark QLPreviewItemDataProvider

- (NSData *)provideDataForItem:(QLItem *)item
{
    return _data.get();
}

#pragma mark QLPreviewControllerDelegate

- (void)previewControllerWillDismiss:(QLPreviewController *)controller
{
    RefPtr<WebKit::WebPageProxy> page = _webView->_page;
    page->goBack();
}

@end

#endif
