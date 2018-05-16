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
#import "SystemPreviewController.h"

#if USE(SYSTEM_PREVIEW)

#import "APIUIClient.h"
#import "WebPageProxy.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <QuickLook/QuickLook.h>
#import <UIKit/UIViewController.h>
#import <pal/spi/ios/QuickLookSPI.h>
#import <wtf/SoftLinking.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/SystemPreviewTypes.cpp>
#endif

SOFT_LINK_FRAMEWORK(QuickLook)
SOFT_LINK_CLASS(QuickLook, QLPreviewController);
SOFT_LINK_CLASS(QuickLook, QLItem);

@interface _WKPreviewControllerDataSource : NSObject <QLPreviewControllerDataSource> {
    RetainPtr<NSItemProvider> _itemProvider;
    RetainPtr<QLItem> _item;
};

@property (strong) NSItemProviderCompletionHandler completionHandler;
@property (retain) NSString *mimeType;

@end

@implementation _WKPreviewControllerDataSource

- (id)initWithMIMEType:(NSString*)mimeType
{
    if (!(self = [super init]))
        return nil;

    self.mimeType = mimeType;

    return self;
}

- (NSInteger)numberOfPreviewItemsInPreviewController:(QLPreviewController *)controller
{
    return 1;
}

- (id<QLPreviewItem>)previewController:(QLPreviewController *)controller previewItemAtIndex:(NSInteger)index
{
    if (_item)
        return _item.get();

    _itemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSString *contentType = @"public.content";
#if USE(APPLE_INTERNAL_SDK)
    contentType = WebKit::getUTIForMIMEType(self.mimeType);
#endif
    _item = adoptNS([allocQLItemInstance() initWithPreviewItemProvider:_itemProvider.get() contentType:contentType previewTitle:@"Preview" fileSize:@(0)]);
    [_item setUseLoadingTimeout:NO];

    [_itemProvider registerItemForTypeIdentifier:contentType loadHandler:^(NSItemProviderCompletionHandler completionHandler, Class expectedValueClass, NSDictionary * options) {
        // This will get called once the download completes.
        self.completionHandler = completionHandler;
    }];
    return _item.get();
}

- (void)setProgress:(float)progress
{
    if (_item)
        [_item setPreviewItemProviderProgress:@(progress)];
}

- (void)finish:(WebCore::URL)url
{
    if (self.completionHandler)
        self.completionHandler((NSURL*)url, nil);
}

@end

@interface _WKPreviewControllerDelegate : NSObject <QLPreviewControllerDelegate> {
    WebKit::SystemPreviewController* _previewController;
};
@end

@implementation _WKPreviewControllerDelegate

- (id)initWithSystemPreviewController:(WebKit::SystemPreviewController*)previewController
{
    if (!(self = [super init]))
        return nil;

    _previewController = previewController;
    return self;
}

- (void)previewControllerDidDismiss:(QLPreviewController *)controller
{
    if (_previewController)
        _previewController->cancel();
}

- (CGRect)previewController:(QLPreviewController *)controller frameForPreviewItem:(id <QLPreviewItem>)item inSourceView:(UIView * *)view
{
    if (!_previewController)
        return CGRectZero;

    UIViewController *presentingViewController = _previewController->page().uiClient().presentingViewController();

    if (!presentingViewController)
        return CGRectZero;

    *view = presentingViewController.view;
    CGRect frame = presentingViewController.view.frame;
    // Create a smaller rectangle centered in the frame.
    CGFloat halfWidth = frame.size.width / 2;
    CGFloat halfHeight = frame.size.height / 2;
    frame = CGRectMake(CGRectGetMidX(frame) - halfWidth / 2, CGRectGetMidY(frame) - halfHeight / 2, halfWidth, halfHeight);
    return frame;
}

- (UIImage *)previewController:(QLPreviewController *)controller transitionImageForPreviewItem:(id <QLPreviewItem>)item contentRect:(CGRect *)contentRect
{
    return nil;
}

@end

namespace WebKit {

void SystemPreviewController::start(const String& mimeType)
{
    // FIXME: Make sure you can't show a preview if we're already previewing.

    UIViewController *presentingViewController = m_webPageProxy.uiClient().presentingViewController();

    if (!presentingViewController)
        return;

    if (!m_qlPreviewController) {
        m_qlPreviewController = adoptNS([allocQLPreviewControllerInstance() init]);

        m_qlPreviewControllerDelegate = adoptNS([[_WKPreviewControllerDelegate alloc] initWithSystemPreviewController:this]);
        [m_qlPreviewController setDelegate:m_qlPreviewControllerDelegate.get()];

        m_qlPreviewControllerDataSource = adoptNS([[_WKPreviewControllerDataSource alloc] initWithMIMEType:mimeType]);
        [m_qlPreviewController setDataSource:m_qlPreviewControllerDataSource.get()];

    }

    [presentingViewController presentViewController:m_qlPreviewController.get() animated:YES completion:nullptr];
}

void SystemPreviewController::updateProgress(float progress)
{
    if (m_qlPreviewControllerDataSource)
        [m_qlPreviewControllerDataSource setProgress:progress];
}

void SystemPreviewController::finish(WebCore::URL url)
{
    if (m_qlPreviewControllerDataSource)
        [m_qlPreviewControllerDataSource finish:url];
}

void SystemPreviewController::cancel()
{
    if (m_qlPreviewController)
        [m_qlPreviewController.get() dismissViewControllerAnimated:YES completion:nullptr];

    m_qlPreviewControllerDelegate = nullptr;
    m_qlPreviewControllerDataSource = nullptr;
    m_qlPreviewController = nullptr;
}

}

#endif
