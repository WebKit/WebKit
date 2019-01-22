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
#import "SystemPreviewController.h"

#if USE(SYSTEM_PREVIEW)

#import "APIUIClient.h"
#import "WebPageProxy.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <QuickLook/QuickLook.h>
#import <UIKit/UIViewController.h>
#import <WebCore/MIMETypeRegistry.h>
#import <pal/ios/QuickLookSoftLink.h>
#import <pal/spi/ios/QuickLookSPI.h>
#import <wtf/WeakObjCPtr.h>

@interface _WKPreviewControllerDataSource : NSObject <QLPreviewControllerDataSource> {
    RetainPtr<NSItemProvider> _itemProvider;
    RetainPtr<QLItem> _item;
};

@property (strong) NSItemProviderCompletionHandler completionHandler;
@property (copy) NSString *mimeType;

@end

@implementation _WKPreviewControllerDataSource

- (instancetype)initWithMIMEType:(NSString*)mimeType
{
    if (!(self = [super init]))
        return nil;

    _mimeType = [mimeType copy];

    return self;
}

- (void)dealloc
{
    [_completionHandler release];
    [_mimeType release];
    [super dealloc];
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
    // FIXME: We are launching the preview controller before getting a response from the resource, which
    // means we don't actually know the real MIME type yet.
    // FIXME: At the moment we only have one supported UTI, but if we start supporting more types,
    // then we'll need a table.
    static NSString *contentType = (__bridge NSString *) UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, CFSTR("usdz"), nil);

    _item = adoptNS([PAL::allocQLItemInstance() initWithPreviewItemProvider:_itemProvider.get() contentType:contentType previewTitle:@"Preview" fileSize:@(0)]);
    [_item setUseLoadingTimeout:NO];

    WeakObjCPtr<_WKPreviewControllerDataSource> weakSelf { self };
    [_itemProvider registerItemForTypeIdentifier:contentType loadHandler:[weakSelf = WTFMove(weakSelf)] (NSItemProviderCompletionHandler completionHandler, Class expectedValueClass, NSDictionary * options) {
        if (auto strongSelf = weakSelf.get())
            [strongSelf setCompletionHandler:completionHandler];
    }];
    return _item.get();
}

- (void)setProgress:(float)progress
{
    if (_item)
        [_item setPreviewItemProviderProgress:@(progress)];
}

- (void)finish:(URL)url
{
    if (self.completionHandler)
        self.completionHandler((NSURL*)url, nil);
}

- (void)failWithError:(NSError *)error
{
    if (self.completionHandler)
        self.completionHandler(nil, error);
}

@end

@interface _WKPreviewControllerDelegate : NSObject <QLPreviewControllerDelegate> {
    WebKit::SystemPreviewController* _previewController;
    WebCore::IntRect _linkRect;
};
@end

@implementation _WKPreviewControllerDelegate

- (id)initWithSystemPreviewController:(WebKit::SystemPreviewController*)previewController fromRect:(WebCore::IntRect)rect
{
    if (!(self = [super init]))
        return nil;

    _previewController = previewController;
    _linkRect = rect;
    return self;
}

- (void)previewControllerDidDismiss:(QLPreviewController *)controller
{
    if (_previewController)
        _previewController->cancel();
}

- (UIViewController *)presentingViewController
{
    if (!_previewController)
        return nil;

    return _previewController->page().uiClient().presentingViewController();
}

- (CGRect)previewController:(QLPreviewController *)controller frameForPreviewItem:(id <QLPreviewItem>)item inSourceView:(UIView * *)view
{
    UIViewController *presentingViewController = [self presentingViewController];

    if (!presentingViewController)
        return CGRectZero;

    *view = presentingViewController.view;

    if (_linkRect.isEmpty()) {
        CGRect frame;
        frame.size.width = presentingViewController.view.frame.size.width / 2.0;
        frame.size.height = presentingViewController.view.frame.size.height / 2.0;
        frame.origin.x = (presentingViewController.view.frame.size.width - frame.size.width) / 2.0;
        frame.origin.y = (presentingViewController.view.frame.size.height - frame.size.height) / 2.0;
        return frame;
    }

    return _previewController->page().syncRootViewToScreen(_linkRect);
}

- (UIImage *)previewController:(QLPreviewController *)controller transitionImageForPreviewItem:(id <QLPreviewItem>)item contentRect:(CGRect *)contentRect
{
    *contentRect = CGRectZero;

    UIViewController *presentingViewController = [self presentingViewController];
    if (presentingViewController) {
        if (_linkRect.isEmpty())
            *contentRect = {CGPointZero, {presentingViewController.view.frame.size.width / 2.0, presentingViewController.view.frame.size.height / 2.0}};
        else {
            WebCore::IntRect screenRect = _previewController->page().syncRootViewToScreen(_linkRect);
            *contentRect = { CGPointZero, { static_cast<CGFloat>(screenRect.width()), static_cast<CGFloat>(screenRect.height()) } };
        }
    }

    return [[UIImage new] autorelease];
}

@end

namespace WebKit {

void SystemPreviewController::start(const String& mimeType, const WebCore::IntRect& fromRect)
{
    ASSERT(!m_qlPreviewController);
    if (m_qlPreviewController)
        return;

    UIViewController *presentingViewController = m_webPageProxy.uiClient().presentingViewController();

    if (!presentingViewController)
        return;

    m_qlPreviewController = adoptNS([PAL::allocQLPreviewControllerInstance() init]);

    m_qlPreviewControllerDelegate = adoptNS([[_WKPreviewControllerDelegate alloc] initWithSystemPreviewController:this fromRect:fromRect]);
    [m_qlPreviewController setDelegate:m_qlPreviewControllerDelegate.get()];

    m_qlPreviewControllerDataSource = adoptNS([[_WKPreviewControllerDataSource alloc] initWithMIMEType:mimeType]);
    [m_qlPreviewController setDataSource:m_qlPreviewControllerDataSource.get()];

    [presentingViewController presentViewController:m_qlPreviewController.get() animated:YES completion:nullptr];
}

void SystemPreviewController::updateProgress(float progress)
{
    if (m_qlPreviewControllerDataSource)
        [m_qlPreviewControllerDataSource setProgress:progress];
}

void SystemPreviewController::finish(URL url)
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

void SystemPreviewController::fail(const WebCore::ResourceError& error)
{
    if (m_qlPreviewControllerDataSource)
        [m_qlPreviewControllerDataSource failWithError:error.nsError()];
}

}

#endif
