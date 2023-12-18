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
#import "WebProcessProxy.h"
#import "_WKDataTaskDelegate.h"
#import "_WKDataTaskInternal.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <QuickLook/QuickLook.h>
#import <UIKit/UIViewController.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/SecurityOriginData.h>
#import <WebCore/UTIUtilities.h>
#import <pal/ios/QuickLookSoftLink.h>
#import <pal/spi/cocoa/FoundationSPI.h>
#import <pal/spi/ios/QuickLookSPI.h>
#import <wtf/WeakObjCPtr.h>

#if HAVE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
#import "ARKitSoftLink.h"
#import <pal/spi/ios/SystemPreviewSPI.h>

SOFT_LINK_PRIVATE_FRAMEWORK(AssetViewer);
SOFT_LINK_CLASS(AssetViewer, ARQuickLookWebKitItem);

#if PLATFORM(VISION)
SOFT_LINK_CLASS(AssetViewer, ASVLaunchPreview);

@interface ASVLaunchPreview (Staging_101981518)
+ (void)beginPreviewApplicationWithURLs:(NSArray *)urls is3DContent:(BOOL)is3DContent websiteURL:(NSURL *)websiteURL completion:(void (^)(NSError *))handler;
+ (void)launchPreviewApplicationWithURLs:(NSArray *)urls completion:(void (^)(NSError *))handler;
+ (void)cancelPreviewApplicationWithURLs:(NSArray<NSURL *> *)urls error:(NSError *)error completion:(void (^)(NSError *))handler;
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
        _previewController->end();
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

@interface _WKSystemPreviewDataTaskDelegate : NSObject <_WKDataTaskDelegate> {
    WebKit::SystemPreviewController* _previewController;
    long long _expectedContentLength;
    RetainPtr<NSMutableData> _data;
    FileSystem::PlatformFileHandle _fileHandle;
    String _filePath;
};
@end

@implementation _WKSystemPreviewDataTaskDelegate

- (id)initWithSystemPreviewController:(WebKit::SystemPreviewController*)previewController
{
    if (!(self = [super init]))
        return nil;

    _previewController = previewController;
    _fileHandle = FileSystem::invalidPlatformFileHandle;
    return self;
}

- (BOOL)isValidMIMEType:(NSString *)MIMEType
{
    // We need to be liberal in which MIME types we accept, because some servers are not configured for USD.
    return WebCore::MIMETypeRegistry::isUSDMIMEType(MIMEType) || [MIMEType isEqualToString:@"application/octet-stream"];
}

- (BOOL)isValidFileExtension:(NSString *)extension
{
    return WebCore::MIMETypeRegistry::isUSDMIMEType(WebCore::MIMETypeRegistry::mimeTypeForExtension(String(extension)));
}

- (void)dataTask:(_WKDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response decisionHandler:(void (^)(_WKDataTaskResponsePolicy))decisionHandler
{
    if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
        NSHTTPURLResponse *HTTPResponse = (NSHTTPURLResponse *)response;
        if ([NSHTTPURLResponse isErrorStatusCode:HTTPResponse.statusCode]) {
            RELEASE_LOG(SystemPreview, "cancelling subresource load due to error status code: %ld", (long)HTTPResponse.statusCode);
            decisionHandler(_WKDataTaskResponsePolicyCancel);
            _previewController->loadFailed();
            return;
        }
    }

    if (![self isValidMIMEType:response.MIMEType] && ![self isValidFileExtension:response.URL.pathExtension]) {
        RELEASE_LOG(SystemPreview, "cancelling subresource load due to unhandled MIME type: \"%@\" extension: \"%@\"", response.MIMEType, response.URL.pathExtension);
        decisionHandler(_WKDataTaskResponsePolicyCancel);
        _previewController->loadFailed();
        return;
    }

    _expectedContentLength = response.expectedContentLength;
    if (_expectedContentLength == NSURLResponseUnknownLength)
        _expectedContentLength = 0;

    _data = adoptNS([[NSMutableData alloc] initWithCapacity:_expectedContentLength]);

    auto fileExtension = [response] {
        // Without this file extension for reality files, ARQL fails to open them.
        if ([response.MIMEType isEqualToString:@"model/vnd.reality"])
            return ".reality"_s;
        return ".usdz"_s;
    }();

    _filePath = FileSystem::openTemporaryFile("SystemPreview"_s, _fileHandle, fileExtension);
    ASSERT(FileSystem::isHandleValid(_fileHandle));

    _previewController->loadStarted(URL::fileURLWithFileSystemPath(_filePath));
    decisionHandler(_WKDataTaskResponsePolicyAllow);
}

- (void)dataTask:(_WKDataTask *)dataTask didReceiveData:(NSData *)data
{
    ASSERT(_data);
    [_data appendData:data];
    if (_expectedContentLength)
        _previewController->updateProgress((float)_data.get().length / _expectedContentLength);
}

- (void)dataTask:(_WKDataTask *)dataTask didCompleteWithError:(NSError *)error
{
    if (error) {
        FileSystem::closeFile(_fileHandle);
        _previewController->loadFailed();
        return;
    }

    [self completeLoad];
}

- (void)completeLoad
{
    ASSERT(_fileHandle != FileSystem::invalidPlatformFileHandle);
    size_t byteCount = FileSystem::writeToFile(_fileHandle, [_data bytes], [_data length]);
    FileSystem::closeFile(_fileHandle);

    if (byteCount != _data.get().length) {
        _previewController->loadFailed();
        return;
    }

    _previewController->loadCompleted(URL::fileURLWithFileSystemPath(_filePath));
}

@end

namespace WebKit {

void SystemPreviewController::begin(const URL& url, const WebCore::SecurityOriginData& topOrigin, const WebCore::SystemPreviewInfo& systemPreviewInfo, CompletionHandler<void()>&& completionHandler)
{
    if (m_state != State::Initial) {
        RELEASE_LOG(SystemPreview, "SystemPreview didn't start because an existing preview is in progress");
        return completionHandler();
    }

    ASSERT(!m_qlPreviewController);
    if (m_qlPreviewController)
        return completionHandler();

    UIViewController *presentingViewController = m_webPageProxy.uiClient().presentingViewController();

    if (!presentingViewController)
        return completionHandler();

    m_systemPreviewInfo = systemPreviewInfo;

    RELEASE_LOG(SystemPreview, "SystemPreview began on %lld", m_systemPreviewInfo.element.elementIdentifier.toUInt64());

    auto request = WebCore::ResourceRequest(url);
    WeakPtr weakThis { *this };
    m_webPageProxy.dataTaskWithRequest(WTFMove(request), topOrigin, [weakThis, completionHandler = WTFMove(completionHandler)] (Ref<API::DataTask>&& task) mutable {
        if (!weakThis)
            return completionHandler();

        auto protectedThis = weakThis.get();

        _WKDataTask *dataTask = wrapper(task);
        protectedThis->m_wkSystemPreviewDataTaskDelegate = adoptNS([[_WKSystemPreviewDataTaskDelegate alloc] initWithSystemPreviewController:protectedThis]);
        [dataTask setDelegate:protectedThis->m_wkSystemPreviewDataTaskDelegate.get()];
        protectedThis->takeActivityToken();
        completionHandler();
    });

    m_downloadURL = url;
    m_fragmentIdentifier = url.fragmentIdentifier().toString();

#if !PLATFORM(VISION)
    m_qlPreviewController = adoptNS([PAL::allocQLPreviewControllerInstance() init]);

    m_qlPreviewControllerDelegate = adoptNS([[_WKPreviewControllerDelegate alloc] initWithSystemPreviewController:this]);
    [m_qlPreviewController setDelegate:m_qlPreviewControllerDelegate.get()];

    m_qlPreviewControllerDataSource = adoptNS([[_WKPreviewControllerDataSource alloc] initWithSystemPreviewController:this MIMEType:@"model/vnd.usdz+zip" originatingPageURL:url]);
    [m_qlPreviewController setDataSource:m_qlPreviewControllerDataSource.get()];

    [presentingViewController presentViewController:m_qlPreviewController.get() animated:YES completion:nullptr];
#endif

    m_state = State::Initial;
}

void SystemPreviewController::loadStarted(const URL& localFileURL)
{
    RELEASE_LOG(SystemPreview, "SystemPreview load has started on %lld", m_systemPreviewInfo.element.elementIdentifier.toUInt64());
    m_localFileURL = localFileURL;

    // Take the local URL, but add on the fragment from the original request URL.
    if (!m_fragmentIdentifier.isEmpty())
        m_localFileURL.setFragmentIdentifier(m_fragmentIdentifier);

#if PLATFORM(VISION)
    if ([getASVLaunchPreviewClass() respondsToSelector:@selector(beginPreviewApplicationWithURLs:is3DContent:websiteURL:completion:)])
        [getASVLaunchPreviewClass() beginPreviewApplicationWithURLs:localFileURLs() is3DContent:YES websiteURL:m_downloadURL completion:^(NSError *error) { }];
#endif

    m_state = State::Loading;
}

void SystemPreviewController::loadCompleted(const URL& localFileURL)
{
    RELEASE_LOG(SystemPreview, "SystemPreview load has finished on %lld", m_systemPreviewInfo.element.elementIdentifier.toUInt64());

    ASSERT(equalIgnoringFragmentIdentifier(m_localFileURL, localFileURL));

#if PLATFORM(VISION)
    if ([getASVLaunchPreviewClass() respondsToSelector:@selector(launchPreviewApplicationWithURLs:completion:)])
        [getASVLaunchPreviewClass() launchPreviewApplicationWithURLs:localFileURLs() completion:^(NSError *error) { }];
    m_state = State::Initial;
#else
    if (m_qlPreviewControllerDataSource)
        [m_qlPreviewControllerDataSource finish:m_localFileURL];
    m_state = State::Viewing;
#endif
    releaseActivityTokenIfNecessary();

    if (m_testingCallback)
        m_testingCallback(true);
}

void SystemPreviewController::loadFailed()
{
    RELEASE_LOG(SystemPreview, "SystemPreview load has failed on %lld", m_systemPreviewInfo.element.elementIdentifier.toUInt64());

#if PLATFORM(VISION)
    if (m_state == State::Loading && [getASVLaunchPreviewClass() respondsToSelector:@selector(cancelPreviewApplicationWithURLs:error:completion:)])
        [getASVLaunchPreviewClass() cancelPreviewApplicationWithURLs:localFileURLs() error:nil completion:^(NSError *error) { }];
#else
    if (m_qlPreviewControllerDataSource)
        [m_qlPreviewControllerDataSource.get() failWithError:nil];

    if (m_qlPreviewController)
        [m_qlPreviewController.get() dismissViewControllerAnimated:YES completion:nullptr];

    m_qlPreviewControllerDelegate = nullptr;
    m_qlPreviewControllerDataSource = nullptr;
    m_qlPreviewController = nullptr;
    m_wkSystemPreviewDataTaskDelegate = nullptr;
#endif
    releaseActivityTokenIfNecessary();

    if (m_testingCallback)
        m_testingCallback(false);

    m_state = State::Initial;
}

void SystemPreviewController::end()
{
    RELEASE_LOG(SystemPreview, "SystemPreview ended on %lld", m_systemPreviewInfo.element.elementIdentifier.toUInt64());

#if !PLATFORM(VISION)
    m_qlPreviewControllerDelegate = nullptr;
    m_qlPreviewControllerDataSource = nullptr;
    m_qlPreviewController = nullptr;
    m_wkSystemPreviewDataTaskDelegate = nullptr;
#endif

    m_state = State::Initial;
}

void SystemPreviewController::updateProgress(float progress)
{
#if PLATFORM(VISION)
    UNUSED_PARAM(progress);
#else
    if (m_qlPreviewControllerDataSource)
        [m_qlPreviewControllerDataSource setProgress:progress];
#endif
}

void SystemPreviewController::takeActivityToken()
{
#if USE(RUNNINGBOARD)
    RELEASE_LOG(ProcessSuspension, "%p - UIProcess is taking a background assertion because it is downloading a system preview", this);
    ASSERT(!m_activity);
    m_activity = page().process().throttler().backgroundActivity("System preview download"_s).moveToUniquePtr();
#endif
}

void SystemPreviewController::releaseActivityTokenIfNecessary()
{
#if USE(RUNNINGBOARD)
    if (m_activity) {
        RELEASE_LOG(ProcessSuspension, "%p UIProcess is releasing a background assertion because a system preview download completed", this);
        m_activity = nullptr;
    }
#endif
}

void SystemPreviewController::setCompletionHandlerForLoadTesting(CompletionHandler<void(bool)>&& handler)
{
    m_testingCallback = WTFMove(handler);
}

void SystemPreviewController::triggerSystemPreviewAction()
{
    RELEASE_LOG(SystemPreview, "SystemPreview action was triggered on %lld", m_systemPreviewInfo.element.elementIdentifier.toUInt64());

    page().systemPreviewActionTriggered(m_systemPreviewInfo, "_apple_ar_quicklook_button_tapped"_s);
}

void SystemPreviewController::triggerSystemPreviewActionWithTargetForTesting(uint64_t elementID, NSString* documentID, uint64_t pageID)
{
    auto uuid = WTF::UUID::parseVersion4(String(documentID));
    ASSERT(uuid);
    if (!uuid)
        return;

    m_systemPreviewInfo.isPreview = true;
    m_systemPreviewInfo.element.elementIdentifier = ObjectIdentifier<WebCore::ElementIdentifierType>(elementID);
    m_systemPreviewInfo.element.documentIdentifier = { *uuid, m_webPageProxy.process().coreProcessIdentifier() };
    m_systemPreviewInfo.element.webPageIdentifier = ObjectIdentifier<WebCore::PageIdentifierType>(pageID);
    triggerSystemPreviewAction();
}

NSArray *SystemPreviewController::localFileURLs() const
{
    NSURL *nsurl = (NSURL *)m_localFileURL;
    return nsurl ? @[ nsurl ] : @[];
}

}

#endif
