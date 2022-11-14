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
#import <WebCore/UTIUtilities.h>
#import <pal/ios/QuickLookSoftLink.h>
#import <pal/spi/ios/QuickLookSPI.h>
#import <wtf/WeakObjCPtr.h>

#if HAVE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
#import "ARKitSoftLink.h"
#import <pal/spi/ios/SystemPreviewSPI.h>

SOFT_LINK_PRIVATE_FRAMEWORK(AssetViewer);
SOFT_LINK_CLASS(AssetViewer, ARQuickLookWebKitItem);

#if HAVE(UIKIT_WEBKIT_INTERNALS)
SOFT_LINK_CLASS(AssetViewer, ASVLaunchPreview);

@interface ASVLaunchPreview (Staging_101981518)
+ (void)beginPreviewApplicationWithURLs:(NSArray *)urls is3DContent:(BOOL)is3DContent completion:(void (^)(NSError *))handler;
+ (void)launchPreviewApplicationWithURLs:(NSArray *)urls completion:(void (^)(NSError *))handler;
@end
#endif

static NSString * const _WKARQLWebsiteURLParameterKey = @"ARQLWebsiteURLParameterKey";

@interface ARQuickLookWebKitItem ()
- (void)setAdditionalParameters:(NSDictionary *)parameters;
@end

@interface _WKPreviewControllerDataSource : NSObject <QLPreviewControllerDataSource, ARQuickLookWebKitItemDelegate> {
#else
@interface _WKPreviewControllerDataSource : NSObject <QLPreviewControllerDataSource> {
#endif
    RetainPtr<NSItemProvider> _itemProvider;
#if HAVE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
    RetainPtr<ARQuickLookWebKitItem> _item;
#else
    RetainPtr<QLItem> _item;
#endif
    URL _originatingPageURL;
    URL _downloadedURL;
    WebKit::SystemPreviewController* _previewController;
};

@property (strong) NSItemProviderCompletionHandler completionHandler;
@property (copy) NSString *mimeType;

@end

@implementation _WKPreviewControllerDataSource

- (instancetype)initWithSystemPreviewController:(WebKit::SystemPreviewController*)previewController MIMEType:(NSString*)mimeType originatingPageURL:(URL)url
{
    if (!(self = [super init]))
        return nil;

    _previewController = previewController;
    _originatingPageURL = url;
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
    NSString *contentType = WebCore::UTIFromMIMEType("model/vnd.usdz+zip"_s);

#if HAVE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
    auto previewItem = adoptNS([WebKit::allocARQuickLookPreviewItemInstance() initWithFileAtURL:_downloadedURL]);
    [previewItem setCanonicalWebPageURL:_originatingPageURL];

    _item = adoptNS([allocARQuickLookWebKitItemInstance() initWithPreviewItemProvider:_itemProvider.get() contentType:contentType previewTitle:@"Preview" fileSize:@(0) previewItem:previewItem.get()]);
    [_item setDelegate:self];

    if ([_item respondsToSelector:(@selector(setAdditionalParameters:))]) {
        NSURL *urlParameter = _originatingPageURL;
        [_item setAdditionalParameters:@{ _WKARQLWebsiteURLParameterKey: urlParameter }];
    }

#else
    _item = adoptNS([PAL::allocQLItemInstance() initWithPreviewItemProvider:_itemProvider.get() contentType:contentType previewTitle:@"Preview" fileSize:@(0)]);
#endif
    [_item setUseLoadingTimeout:NO];

    WeakObjCPtr<_WKPreviewControllerDataSource> weakSelf { self };
    [_itemProvider registerItemForTypeIdentifier:contentType loadHandler:[weakSelf = WTFMove(weakSelf)] (NSItemProviderCompletionHandler completionHandler, Class expectedValueClass, NSDictionary * options) {
        if (auto strongSelf = weakSelf.get()) {
            // If the download happened instantly, the call to finish might have come before this
            // loadHandler. In that case, call the completionHandler here.
            if (!strongSelf->_downloadedURL.isEmpty())
                completionHandler((NSURL *)strongSelf->_downloadedURL, nil);
            else
                [strongSelf setCompletionHandler:completionHandler];
        }
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
    _downloadedURL = url;

    if (self.completionHandler)
        self.completionHandler((NSURL *)url, nil);
}

- (void)failWithError:(NSError *)error
{
    if (self.completionHandler)
        self.completionHandler(nil, error);
}

#if HAVE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
- (void)previewItem:(ARQuickLookWebKitItem *)previewItem didReceiveMessage:(NSDictionary *)message
{
    if (!_previewController)
        return;

    if ([[message[@"callToAction"] stringValue] isEqualToString:@"buttonTapped"])
        _previewController->triggerSystemPreviewAction();
}
#endif

@end

@interface _WKPreviewControllerDelegate : NSObject <QLPreviewControllerDelegate> {
    WebKit::SystemPreviewController* _previewController;
    WebCore::IntRect _linkRect;
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

    if (!_previewController->previewInfo().previewRect.isEmpty())
        return _previewController->page().syncRootViewToScreen(_previewController->previewInfo().previewRect);

    CGRect frame;
    frame.size.width = (*view).frame.size.width / 2.0;
    frame.size.height = (*view).frame.size.height / 2.0;
    frame.origin.x = ((*view).frame.size.width - frame.size.width) / 2.0;
    frame.origin.y = ((*view).frame.size.height - frame.size.height) / 2.0;
    return frame;
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

    return adoptNS([UIImage new]).autorelease();
}

@end

namespace WebKit {

void SystemPreviewController::start(URL originatingPageURL, const String& mimeType, const WebCore::SystemPreviewInfo& systemPreviewInfo)
{
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    UNUSED_PARAM(originatingPageURL);
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(systemPreviewInfo);
#else
    ASSERT(!m_qlPreviewController);
    if (m_qlPreviewController)
        return;

    UIViewController *presentingViewController = m_webPageProxy.uiClient().presentingViewController();

    if (!presentingViewController)
        return;

    m_systemPreviewInfo = systemPreviewInfo;

    m_qlPreviewController = adoptNS([PAL::allocQLPreviewControllerInstance() init]);

    m_qlPreviewControllerDelegate = adoptNS([[_WKPreviewControllerDelegate alloc] initWithSystemPreviewController:this]);
    [m_qlPreviewController setDelegate:m_qlPreviewControllerDelegate.get()];

    m_qlPreviewControllerDataSource = adoptNS([[_WKPreviewControllerDataSource alloc] initWithSystemPreviewController:this MIMEType:mimeType originatingPageURL:originatingPageURL]);
    [m_qlPreviewController setDataSource:m_qlPreviewControllerDataSource.get()];

    [presentingViewController presentViewController:m_qlPreviewController.get() animated:YES completion:nullptr];
#endif
}

void SystemPreviewController::setDestinationURL(URL url)
{
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    NSURL *nsurl = (NSURL *)url;
    if ([getASVLaunchPreviewClass() respondsToSelector:@selector(beginPreviewApplicationWithURLs:is3DContent:completion:)])
        [getASVLaunchPreviewClass() beginPreviewApplicationWithURLs:@[nsurl] is3DContent:YES completion:^(NSError *error) { }];
#endif
    m_destinationURL = WTFMove(url);
}

void SystemPreviewController::updateProgress(float progress)
{
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    UNUSED_PARAM(progress);
#else
    if (m_qlPreviewControllerDataSource)
        [m_qlPreviewControllerDataSource setProgress:progress];
#endif
}

void SystemPreviewController::finish(URL url)
{
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    NSURL *nsurl = (NSURL *)url;
    if ([getASVLaunchPreviewClass() respondsToSelector:@selector(launchPreviewApplicationWithURLs:completion:)])
        [getASVLaunchPreviewClass() launchPreviewApplicationWithURLs:@[nsurl] completion:^(NSError *error) { }];
#else
    if (m_qlPreviewControllerDataSource)
        [m_qlPreviewControllerDataSource finish:url];
#endif
}

void SystemPreviewController::cancel()
{
#if !HAVE(UIKIT_WEBKIT_INTERNALS)
    if (m_qlPreviewController)
        [m_qlPreviewController.get() dismissViewControllerAnimated:YES completion:nullptr];

    m_qlPreviewControllerDelegate = nullptr;
    m_qlPreviewControllerDataSource = nullptr;
    m_qlPreviewController = nullptr;
#endif
}

void SystemPreviewController::fail(const WebCore::ResourceError& error)
{
#if !HAVE(UIKIT_WEBKIT_INTERNALS)
    if (m_qlPreviewControllerDataSource)
        [m_qlPreviewControllerDataSource failWithError:error.nsError()];
#endif
}

void SystemPreviewController::triggerSystemPreviewAction()
{
    page().systemPreviewActionTriggered(m_systemPreviewInfo, "_apple_ar_quicklook_button_tapped"_s);
}

void SystemPreviewController::triggerSystemPreviewActionWithTargetForTesting(uint64_t elementID, NSString* documentID, uint64_t pageID)
{
    auto uuid = UUID::parseVersion4(String(documentID));
    ASSERT(uuid);
    if (!uuid)
        return;

    m_systemPreviewInfo.isPreview = true;
    m_systemPreviewInfo.element.elementIdentifier = makeObjectIdentifier<WebCore::ElementIdentifierType>(elementID);
    m_systemPreviewInfo.element.documentIdentifier = { *uuid, m_webPageProxy.process().coreProcessIdentifier() };
    m_systemPreviewInfo.element.webPageIdentifier = makeObjectIdentifier<WebCore::PageIdentifierType>(pageID);
    triggerSystemPreviewAction();
}

}

#endif
