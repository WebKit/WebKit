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
#import "WKUSDPreviewView.h"

#if USE(SYSTEM_PREVIEW)

#import "APIFindClient.h"
#import "APIUIClient.h"
#import "UIKitUtilities.h"
#import "WKWebViewIOS.h"
#import "WebPageProxy.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <WebCore/FloatRect.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/UTIUtilities.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/ios/SystemPreviewSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>

#import <pal/ios/QuickLookSoftLink.h>

SOFT_LINK_PRIVATE_FRAMEWORK(AssetViewer);
SOFT_LINK_CLASS(AssetViewer, ASVThumbnailView);

static RetainPtr<NSString> getUTIForUSDMIMEType(const String& mimeType)
{
    if (!WebCore::MIMETypeRegistry::isUSDMIMEType(mimeType))
        return nil;

    return WebCore::UTIFromMIMEType(mimeType).createNSString();
}

@interface WKUSDPreviewView () <ASVThumbnailViewDelegate>
@end

@implementation WKUSDPreviewView {
    RetainPtr<NSItemProvider> _itemProvider;
    RetainPtr<NSData> _data;
    RetainPtr<NSString> _suggestedFilename;
    RetainPtr<NSString> _mimeType;
    RetainPtr<QLItem> _item;
    RetainPtr<ASVThumbnailView> _thumbnailView;
    WeakObjCPtr<WKWebView> _webView;
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

- (void)web_setContentProviderData:(NSData *)data suggestedFilename:(NSString *)filename completionHandler:(void (^)(void))completionHandler
{
    _suggestedFilename = adoptNS([filename copy]);
    _data = adoptNS([data copy]);

    RetainPtr alert = WebKit::createUIAlertController(protect(WEB_UI_NSSTRING(@"View 3D Object?", "View 3D Object?")), protect(WEB_UI_NSSTRING(@"You can see a preview of this object before viewing in 3D.", "You can see a preview of this object before viewing in 3D.")));

    RetainPtr allowAction = [UIAlertAction actionWithTitle:protect(WEB_UI_NSSTRING_KEY(@"View 3D Object", @"View 3D Object (QuickLook Preview)", "Allow displaying QuickLook Preview of 3D object")).get() style:UIAlertActionStyleDefault handler:[weakSelf = WeakObjCPtr<WKUSDPreviewView>(self), completionHandler = makeBlockPtr(completionHandler)](UIAlertAction *) mutable {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf) {
            completionHandler();
            return;
        }

        RetainPtr contentType = getUTIForUSDMIMEType(strongSelf->_mimeType.get());

        strongSelf->_item = adoptNS([PAL::allocQLItemInstance() initWithDataProvider:strongSelf.get() contentType:contentType.get() previewTitle:strongSelf->_suggestedFilename.get()]);
        [strongSelf->_item setUseLoadingTimeout:NO];

        strongSelf->_thumbnailView = adoptNS([allocASVThumbnailViewInstance() init]);
        [strongSelf->_thumbnailView setDelegate:strongSelf.get()];
        [strongSelf setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];

        [strongSelf setAutoresizesSubviews:YES];
        [strongSelf setClipsToBounds:YES];
        [strongSelf addSubview:strongSelf->_thumbnailView.get()];
        [strongSelf _layoutThumbnailView];

        auto screenBounds = strongSelf.get().window.windowScene.screen.bounds;
        CGFloat maxDimension = CGFloatMin(screenBounds.size.width, screenBounds.size.height);
        [strongSelf->_thumbnailView setMaxThumbnailSize:CGSizeMake(maxDimension, maxDimension)];

        [strongSelf->_thumbnailView setThumbnailItem:strongSelf->_item.get()];

        completionHandler();
    }];

    RetainPtr doNotAllowAction = [UIAlertAction actionWithTitle:protect(WEB_UI_NSSTRING_KEY(@"Cancel", @"Cancel (QuickLook Preview)", "Cancel displaying QuickLook Preview of 3D object")).get() style:UIAlertActionStyleCancel handler:[completionHandler = makeBlockPtr(completionHandler)](UIAlertAction *) {
        completionHandler();
    }];

    [alert addAction:doNotAllowAction.get()];
    [alert addAction:allowAction.get()];

    RefPtr page = _webView.get()->_page;
    RetainPtr presentingViewController = page->uiClient().presentingViewController();
    [presentingViewController.get() presentViewController:alert.get() animated:YES completion:nil];
}

- (void)_layoutThumbnailView
{
    if (_thumbnailView) {
        RetainPtr webView = _webView.get();
        UIEdgeInsets safeAreaInsets = [webView _computedUnobscuredSafeAreaInset];
        UIEdgeInsets obscuredAreaInsets = [webView _computedObscuredInset];

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
    RefPtr<WebKit::WebPageProxy> page = _webView.get()->_page;

    // FIXME: When in element fullscreen, UIClient::presentingViewController() may not return the
    // WKFullScreenViewController even though that is the presenting view controller of the WKWebView.
    // We should call PageClientImpl::presentingViewController() instead.
    RetainPtr presentingViewController = page->uiClient().presentingViewController();

    [presentingViewController.get() presentViewController:previewController animated:YES completion:nil];
}

#pragma mark WKWebViewContentProvider protocol

- (UIView *)web_contentView
{
    return self;
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
    RefPtr<WebKit::WebPageProxy> page = _webView.get()->_page;
    page->findClient().didCountStringMatches(page.get(), string, 0);
}

- (void)web_findString:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount
{
    RefPtr<WebKit::WebPageProxy> page = _webView.get()->_page;
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
