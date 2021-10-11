/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#if USE(SYSTEM_PREVIEW) && !HAVE(UIKIT_WEBKIT_INTERNALS)

#import "APIFindClient.h"
#import "APIUIClient.h"
#import "WKWebViewIOS.h"
#import "WebPageProxy.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <WebCore/FloatRect.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/UTIUtilities.h>
#import <pal/ios/QuickLookSoftLink.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/ios/SystemPreviewSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

SOFT_LINK_PRIVATE_FRAMEWORK(AssetViewer);
SOFT_LINK_CLASS(AssetViewer, ASVThumbnailView);

static NSString *getUTIForSystemPreviewMIMEType(const String& mimeType)
{
    if (!WebCore::MIMETypeRegistry::isSystemPreviewMIMEType(mimeType))
        return nil;

    return WebCore::UTIFromMIMEType(mimeType);
}

@interface WKSystemPreviewView () <ASVThumbnailViewDelegate>
@end

@implementation WKSystemPreviewView {
    RetainPtr<NSItemProvider> _itemProvider;
    RetainPtr<NSData> _data;
    RetainPtr<NSString> _suggestedFilename;
    RetainPtr<NSString> _mimeType;
    RetainPtr<QLItem> _item;
    RetainPtr<ASVThumbnailView> _thumbnailView;
    WKWebView *_webView;
}

- (instancetype)web_initWithFrame:(CGRect)frame webView:(WKWebView *)webView mimeType:(NSString *)mimeType
{
    if (!(self = [super initWithFrame:frame webView:webView]))
        return nil;

    UIColor *backgroundColor = [UIColor colorWithRed:(38. / 255) green:(38. / 255) blue:(38. / 255) alpha:1];
    self.backgroundColor = backgroundColor;

    _webView = webView;
    _mimeType = mimeType;

    UIScrollView *scrollView = webView.scrollView;
    [scrollView setMinimumZoomScale:1];
    [scrollView setMaximumZoomScale:1];
    [scrollView setBackgroundColor:backgroundColor];

    return self;
}

- (void)web_setContentProviderData:(NSData *)data suggestedFilename:(NSString *)filename
{
    _suggestedFilename = adoptNS([filename copy]);
    _data = adoptNS([data copy]);

    NSString *contentType = getUTIForSystemPreviewMIMEType(_mimeType.get());

    _item = adoptNS([PAL::allocQLItemInstance() initWithDataProvider:self contentType:contentType previewTitle:_suggestedFilename.get()]);
    [_item setUseLoadingTimeout:NO];

    _thumbnailView = adoptNS([allocASVThumbnailViewInstance() init]);
    [_thumbnailView setDelegate:self];
    [self setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];

    [self setAutoresizesSubviews:YES];
    [self setClipsToBounds:YES];
    [self addSubview:_thumbnailView.get()];
    [self _layoutThumbnailView];

    auto screenBounds = UIScreen.mainScreen.bounds;
    CGFloat maxDimension = CGFloatMin(screenBounds.size.width, screenBounds.size.height);
    [_thumbnailView setMaxThumbnailSize:CGSizeMake(maxDimension, maxDimension)];

    [_thumbnailView setThumbnailItem:_item.get()];
}

- (void)_layoutThumbnailView
{
    if (_thumbnailView) {
        UIEdgeInsets safeAreaInsets = _webView._computedUnobscuredSafeAreaInset;
        UIEdgeInsets obscuredAreaInsets = _webView._computedObscuredInset;

        CGRect layoutFrame = UIEdgeInsetsInsetRect(self.frame, safeAreaInsets);

        layoutFrame.size.width -= obscuredAreaInsets.left + obscuredAreaInsets.right;
        layoutFrame.size.height -= obscuredAreaInsets.top + obscuredAreaInsets.bottom;

        [_thumbnailView setFrame:layoutFrame];
        [_thumbnailView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    }
}

#pragma mark ASVThumbnailViewDelegate

- (void)thumbnailView:(ASVThumbnailView *)thumbnailView wantsToPresentPreviewController:(QLPreviewController *)previewController forItem:(QLItem *)item
{
    RefPtr<WebKit::WebPageProxy> page = _webView->_page;
    UIViewController *presentingViewController = page->uiClient().presentingViewController();
    [presentingViewController presentViewController:previewController animated:YES completion:nil];
}

#pragma mark WKWebViewContentProvider protocol

- (UIView *)web_contentView
{
    return self;
}

+ (BOOL)web_requiresCustomSnapshotting
{
    return false;
}

- (void)web_setMinimumSize:(CGSize)size
{
}

- (void)web_setOverlaidAccessoryViewsInset:(CGSize)inset
{
}

- (void)web_computedContentInsetDidChange
{
    [self _layoutThumbnailView];
}

- (void)web_setFixedOverlayView:(UIView *)fixedOverlayView
{
}

- (void)web_didSameDocumentNavigation:(WKSameDocumentNavigationType)navigationType
{
}

- (BOOL)web_isBackground
{
    return self.isBackground;
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

#pragma mark QLPreviewItemDataProvider

- (NSData *)provideDataForItem:(QLItem *)item
{
    return _data.get();
}

@end

#endif
