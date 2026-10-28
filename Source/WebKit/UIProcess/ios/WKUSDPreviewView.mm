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
    RetainPtr<UIStackView> _placeholderView;
    RetainPtr<UIImageView> _placeholderImageView;
    RetainPtr<UILabel> _placeholderLabel;
    RetainPtr<NSLayoutConstraint> _placeholderLeadingConstraint;
    RetainPtr<NSLayoutConstraint> _placeholderTrailingConstraint;
    CGFloat _placeholderLayoutDimension;
    WeakObjCPtr<WKWebView> _webView;
    BOOL _confirmationAlertPending;
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

    if ([self _presentConfirmationAlertWithCompletionHandler:completionHandler])
        return;

    RELEASE_LOG(Process, "WKUSDPreviewView %p: no presenting view controller, deferring confirmation alert", self);
    _confirmationAlertPending = YES;
    [self _showPlaceholderView];
    completionHandler();
}

- (BOOL)_presentConfirmationAlertWithCompletionHandler:(void (^)(void))completionHandler
{
    RefPtr page = _webView.get()->_page;
    RetainPtr presentingViewController = page ? page->uiClient().presentingViewController() : nil;
    if (!presentingViewController)
        return NO;

    _confirmationAlertPending = NO;
    [self _removePlaceholderView];

    RetainPtr alert = WebKit::createUIAlertController(protect(WEB_UI_NSSTRING(@"View 3D Object?", "View 3D Object?")), protect(WEB_UI_NSSTRING(@"You can see a preview of this object before viewing in 3D.", "You can see a preview of this object before viewing in 3D.")));

    RetainPtr allowAction = [UIAlertAction actionWithTitle:protect(WEB_UI_NSSTRING_KEY(@"View 3D Object", @"View 3D Object (QuickLook Preview)", "Allow displaying QuickLook Preview of 3D object")).get() style:UIAlertActionStyleDefault handler:[weakSelf = WeakObjCPtr<WKUSDPreviewView>(self), completionHandler = makeBlockPtr(completionHandler)](UIAlertAction *) mutable {
        [weakSelf.get() _showThumbnailView];
        if (completionHandler)
            completionHandler();
    }];

    RetainPtr doNotAllowAction = [UIAlertAction actionWithTitle:protect(WEB_UI_NSSTRING_KEY(@"Cancel", @"Cancel (QuickLook Preview)", "Cancel displaying QuickLook Preview of 3D object")).get() style:UIAlertActionStyleCancel handler:[completionHandler = makeBlockPtr(completionHandler)](UIAlertAction *) {
        if (completionHandler)
            completionHandler();
    }];

    [alert addAction:doNotAllowAction.get()];
    [alert addAction:allowAction.get()];

    [presentingViewController.get() presentViewController:alert.get() animated:YES completion:nil];
    return YES;
}

- (void)_presentPendingConfirmationAlertIfPossible
{
    if (!_confirmationAlertPending || !self.window || self.isBackground)
        return;

    if ([self _presentConfirmationAlertWithCompletionHandler:nil])
        RELEASE_LOG(Process, "WKUSDPreviewView %p: presented deferred confirmation alert", self);
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    [self _presentPendingConfirmationAlertIfPossible];
}

- (void)_applicationWillEnterForeground
{
    [super _applicationWillEnterForeground];
    [self _presentPendingConfirmationAlertIfPossible];
}

- (void)_showPlaceholderView
{
    if (_placeholderView)
        return;

    UIColor *foregroundColor = [UIColor colorWithWhite:1 alpha:0.6];

    _placeholderImageView = adoptNS([[UIImageView alloc] init]);
    [_placeholderImageView setTintColor:foregroundColor];
    [_placeholderImageView setContentMode:UIViewContentModeScaleAspectFit];

    _placeholderLabel = adoptNS([[UILabel alloc] init]);
    [_placeholderLabel setText:_suggestedFilename.get()];
    [_placeholderLabel setTextColor:foregroundColor];
    [_placeholderLabel setTextAlignment:NSTextAlignmentCenter];
    [_placeholderLabel setNumberOfLines:2];
    [_placeholderLabel setLineBreakMode:NSLineBreakByTruncatingMiddle];

    _placeholderView = adoptNS([[UIStackView alloc] initWithArrangedSubviews:@[ _placeholderImageView.get(), _placeholderLabel.get() ]]);
    [_placeholderView setAxis:UILayoutConstraintAxisVertical];
    [_placeholderView setAlignment:UIStackViewAlignmentCenter];
    [_placeholderView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:_placeholderView.get()];

    UILayoutGuide *safeArea = self.safeAreaLayoutGuide;
    _placeholderLeadingConstraint = [[_placeholderView leadingAnchor] constraintGreaterThanOrEqualToAnchor:safeArea.leadingAnchor];
    _placeholderTrailingConstraint = [safeArea.trailingAnchor constraintGreaterThanOrEqualToAnchor:[_placeholderView trailingAnchor]];
    [NSLayoutConstraint activateConstraints:@[
        [[_placeholderView centerXAnchor] constraintEqualToAnchor:safeArea.centerXAnchor],
        [[_placeholderView centerYAnchor] constraintEqualToAnchor:safeArea.centerYAnchor],
        _placeholderLeadingConstraint.get(),
        _placeholderTrailingConstraint.get(),
    ]];

    _placeholderLayoutDimension = 0;
    [self _updatePlaceholderSizes];
}

- (void)_updatePlaceholderSizes
{
    if (!_placeholderView)
        return;

    CGSize size = self.bounds.size;
    CGFloat dimension = std::min(size.width, size.height);
    if (dimension <= 0 || dimension == _placeholderLayoutDimension)
        return;
    _placeholderLayoutDimension = dimension;

    CGFloat symbolSize = dimension * 0.3;
    RetainPtr symbolConfiguration = [UIImageSymbolConfiguration configurationWithPointSize:symbolSize weight:UIImageSymbolWeightLight];
    [_placeholderImageView setImage:[UIImage systemImageNamed:@"arkit" withConfiguration:symbolConfiguration.get()]];
    [_placeholderLabel setFont:[UIFont systemFontOfSize:symbolSize / 5]];
    [_placeholderView setSpacing:symbolSize / 5];

    CGFloat margin = symbolSize / 3;
    [_placeholderLeadingConstraint setConstant:margin];
    [_placeholderTrailingConstraint setConstant:margin];
}

- (void)_removePlaceholderView
{
    [_placeholderView removeFromSuperview];
    _placeholderView = nil;
    _placeholderImageView = nil;
    _placeholderLabel = nil;
    _placeholderLeadingConstraint = nil;
    _placeholderTrailingConstraint = nil;
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    [self _updatePlaceholderSizes];
}

- (void)_showThumbnailView
{
    RetainPtr contentType = getUTIForUSDMIMEType(_mimeType.get());

    _item = adoptNS([PAL::allocQLItemInstance() initWithDataProvider:self contentType:contentType.get() previewTitle:_suggestedFilename.get()]);
    [_item setUseLoadingTimeout:NO];

    _thumbnailView = adoptNS([allocASVThumbnailViewInstance() init]);
    [_thumbnailView setDelegate:self];
    [self setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];

    [self setAutoresizesSubviews:YES];
    [self setClipsToBounds:YES];
    [self addSubview:_thumbnailView.get()];
    [self _layoutThumbnailView];

    UIScreen *screen = self.window.windowScene.screen;
    CGRect screenBounds = screen ? screen.bounds : self.bounds;
    CGFloat maxDimension = CGFloatMin(screenBounds.size.width, screenBounds.size.height);
    [_thumbnailView setMaxThumbnailSize:CGSizeMake(maxDimension, maxDimension)];

    [_thumbnailView setThumbnailItem:_item.get()];
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
