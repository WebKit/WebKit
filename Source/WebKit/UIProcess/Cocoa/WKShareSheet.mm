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
#import <WebKit/WKShareSheet.h>

#if HAVE(SHARE_SHEET_UI)

#import "PickerDismissalReason.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/NSURLUtilities.h>
#import <WebCore/ShareData.h>
#import <pal/spi/mac/QuarantineSPI.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/Scope.h>
#import <wtf/SoftLinking.h>
#import <wtf/UUID.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import "UIKitUtilities.h"
#import "WKContentViewInteraction.h"
#else
#import "AppKitSPI.h"
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#endif

#import <pal/cocoa/LinkPresentationSoftLink.h>

static NSString *typeIdentifierForFileURL(NSURL *url)
{
    NSString *typeIdentifier = nil;
    [url getPromisedItemResourceValue:&typeIdentifier forKey:NSURLTypeIdentifierKey error:nil];

    if (typeIdentifier)
        return typeIdentifier;

    if (RetainPtr pathExtension = [url pathExtension]) {
        if (RetainPtr type = [UTType typeWithFilenameExtension:pathExtension.get()])
            return type.get().identifier;
    }

    return UTTypeData.identifier;
}

static RetainPtr<LPLinkMetadata> placeholderMetadataWithURLAndTitle(NSURL *url, NSString *title)
{
    RetainPtr metadata = adoptNS([PAL::allocLPLinkMetadataInstance() init]);
    [metadata setOriginalURL:url];
    [metadata setURL:url];
    [metadata setTitle:title];
    [metadata _setIncomplete:YES];
    return metadata;
}

#if PLATFORM(MAC)

static uint64_t sizeForFileURL(NSURL *url)
{
    NSNumber *size = 0;
    [url getResourceValue:&size forKey:NSURLFileSizeKey error:nil];

    if (size)
        return [size unsignedLongLongValue];

    return 0;
}

static RetainPtr<NSString> nameForFileURLWithTypeIdentifier(NSURL *url, NSString *typeIdentifier)
{
    BOOL isFolder = [typeIdentifier isEqualToString:UTTypeFolder.identifier];

    RetainPtr<NSDictionary<NSURLResourceKey, id>> itemResources = [url promisedItemResourceValuesForKeys:@[ NSURLHasHiddenExtensionKey, NSURLLocalizedNameKey ] error:nil];
    if (RetainPtr localizedName = checked_objc_cast<NSString>([itemResources objectForKey:NSURLLocalizedNameKey])) {
        if (!isFolder) {
            if (RetainPtr hasHiddenExtension = checked_objc_cast<NSNumber>([itemResources objectForKey:NSURLHasHiddenExtensionKey])) {
                if (![hasHiddenExtension boolValue])
                    localizedName = localizedName.get().stringByDeletingPathExtension;
            }
        }

        return localizedName;
    }

    return [[NSFileManager defaultManager] displayNameAtPath:retainPtr(url.path).get()];
}

static RetainPtr<LPLinkMetadata> placeholderMetadataWithFileURL(NSURL *url)
{
    RetainPtr metadata = adoptNS([PAL::allocLPLinkMetadataInstance() init]);
    [metadata setOriginalURL:url];
    [metadata setURL:url];

    RetainPtr typeIdentifier = typeIdentifierForFileURL(url);

    RetainPtr fileMetadata = adoptNS([PAL::allocLPFileMetadataInstance() init]);
    [fileMetadata setType:typeIdentifier.get()];
    [fileMetadata setName:nameForFileURLWithTypeIdentifier(url, typeIdentifier.get()).get()];
    [fileMetadata setSize:sizeForFileURL(url)];
    [metadata setSpecialization:fileMetadata.get()];

    return metadata;
}

#endif

#if PLATFORM(IOS_FAMILY)

@interface WKShareSheetFileItemProvider : UIActivityItemProvider
- (instancetype)initWithURL:(NSURL *)url;
@end

@implementation WKShareSheetFileItemProvider {
    RetainPtr<NSURL> _url;
    RetainPtr<LPLinkMetadata> _headerMetadata;
}

- (instancetype)initWithURL:(NSURL *)url
{
    RELEASE_ASSERT(isMainRunLoop());

    if (!(self = [super initWithPlaceholderItem:[NSData data]]))
        return nil;

    _url = url;

    auto provider = adoptNS([PAL::allocLPMetadataProviderInstance() init]);
    [provider setShouldFetchSubresources:NO];

    _headerMetadata = [provider _startFetchingMetadataForURL:url completionHandler:^(NSError *) { }];

    return self;
}

- (id)item
{
    return _url.get();
}

- (NSString *)activityViewController:(UIActivityViewController *)activityViewController dataTypeIdentifierForActivityType:(UIActivityType)activityType
{
    return typeIdentifierForFileURL(_url.get());
}

- (LPLinkMetadata *)activityViewControllerLinkMetadata:(UIActivityViewController *)activityViewController
{
    return _headerMetadata.get();
}

@end

@interface WKShareSheetURLItemProvider : UIActivityItemProvider
- (instancetype)initWithURL:(NSURL *)url title:(NSString *)title;
@end

@implementation WKShareSheetURLItemProvider {
    RetainPtr<NSURL> _url;
    RetainPtr<LPLinkMetadata> _metadata;
}

- (instancetype)initWithURL:(NSURL *)url title:(NSString *)title
{
    RELEASE_ASSERT(isMainRunLoop());

    if (!(self = [super initWithPlaceholderItem:url]))
        return nil;

    _metadata = placeholderMetadataWithURLAndTitle(url, title);

    _url = url;

    return self;
}

- (id)item
{
    return _url.get();
}

- (LPLinkMetadata *)activityViewControllerLinkMetadata:(UIActivityViewController *)activityViewController
{
    return _metadata.get();
}

@end

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
@interface WKShareSheet () <NSSharingServiceDelegate, NSSharingServicePickerDelegate>
@end
#else
@interface WKShareSheet () <UIAdaptivePresentationControllerDelegate>
@end
#endif

@implementation WKShareSheet {
    RetainPtr<NSURL> _temporaryFileShareDirectory;
    WeakObjCPtr<WKWebView> _webView;
    WeakObjCPtr<id <WKShareSheetDelegate> > _delegate;
    WTF::CompletionHandler<void(bool)> _completionHandler;

#if PLATFORM(MAC)
    RetainPtr<NSSharingServicePicker> _sharingServicePicker;
#else
    RetainPtr<UIActivityViewController> _shareSheetViewController;
    RetainPtr<UIViewController> _presentationViewController;
#endif

    BOOL _didShareSuccessfully;
}

- (id<WKShareSheetDelegate>)delegate
{
    return _delegate.getAutoreleased();
}

- (void)setDelegate:(id<WKShareSheetDelegate>)delegate
{
    _delegate = delegate;
}

- (instancetype)initWithView:(WKWebView *)view
{
    if (!(self = [super init]))
        return nil;

    _webView = view;

    return self;
}

static void appendFilesAsShareableURLs(RetainPtr<NSMutableArray>&& shareDataArray, const Vector<WebCore::RawFile>& files, NSURL* temporaryDirectory, bool usePlaceholderFiles, CompletionHandler<void(RetainPtr<NSMutableArray>&&)>&& completionHandler)
{
    struct FileWriteTask {
        String fileName;
        RetainPtr<NSData> fileData;
    };
    auto fileWriteTasks = files.map([](auto& file) {
        return FileWriteTask { file.fileName.isolatedCopy(), file.fileData->createNSData() };
    });

    auto queue = WorkQueue::create("com.apple.WebKit.WKShareSheet.ShareableFileWriter"_s);
    queue->dispatch([shareDataArray = WTFMove(shareDataArray), fileWriteTasks = WTFMove(fileWriteTasks), temporaryDirectory = retainPtr(temporaryDirectory), usePlaceholderFiles, completionHandler = WTFMove(completionHandler)]() mutable {
        RetainPtr fileURLs = adoptNS([[NSMutableArray alloc] initWithCapacity:fileWriteTasks.size()]);
        for (auto& fileWriteTask : fileWriteTasks) {
            RetainPtr fileURL = [WKShareSheet writeFileToShareableURL:WebCore::ResourceResponseBase::sanitizeSuggestedFilename(fileWriteTask.fileName).createNSString().get() data:fileWriteTask.fileData.get() temporaryDirectory:temporaryDirectory.get()];
            if (!fileURL)
                continue;

            [fileURLs addObject:fileURL.get()];
        }

        RunLoop::mainSingleton().dispatch([completionHandler = WTFMove(completionHandler), shareDataArray = WTFMove(shareDataArray), fileURLs = WTFMove(fileURLs), usePlaceholderFiles] mutable {
            if (usePlaceholderFiles) {
                RetainPtr placeholderShareDataArray = adoptNS([[NSMutableArray alloc] initWithCapacity:[fileURLs count]]);
                for (NSURL *fileURL in fileURLs.get()) {
#if PLATFORM(IOS_FAMILY)
                    RetainPtr item = adoptNS([[WKShareSheetFileItemProvider alloc] initWithURL:fileURL]);
#else
                    RetainPtr item = adoptNS([[NSPreviewRepresentingActivityItem alloc] initWithItem:fileURL linkMetadata:placeholderMetadataWithFileURL(fileURL).get()]);
#endif

                    if (!item)
                        continue;

                    [placeholderShareDataArray addObject:item.get()];
                }

                [shareDataArray addObjectsFromArray:placeholderShareDataArray.get()];
            } else
                [shareDataArray addObjectsFromArray:fileURLs.get()];

            completionHandler(WTFMove(shareDataArray));
        });
    });
}

- (void)presentWithParameters:(const WebCore::ShareDataWithParsedURL &)data inRect:(std::optional<WebCore::FloatRect>)rect completionHandler:(WTF::CompletionHandler<void(bool)>&&)completionHandler
{
    auto shareDataArray = adoptNS([[NSMutableArray alloc] init]);
    
    if (!data.shareData.text.isEmpty())
        [shareDataArray addObject:data.shareData.text.createNSString().get()];
    
    if (data.url) {
        RetainPtr url = data.url.value().createNSURL();
        RetainPtr<NSString> title;
        if (!data.shareData.title.isEmpty())
            title = data.shareData.title.createNSString();

        if (data.originator == WebCore::ShareDataOriginator::Web) {
#if PLATFORM(IOS_FAMILY)
            auto item = adoptNS([[WKShareSheetURLItemProvider alloc] initWithURL:url.get() title:title.get()]);
#else
            auto item = adoptNS([[NSPreviewRepresentingActivityItem alloc] initWithItem:url.get() linkMetadata:placeholderMetadataWithURLAndTitle(url.get(), title.get()).get()]);
#endif
            if (item)
                [shareDataArray addObject:item.get()];
        } else {
#if HAVE(NSURL_TITLE)
            [url _web_setTitle:title.get()];
#endif
            [shareDataArray addObject:url.get()];
        }
    }
    
    if (!data.shareData.title.isEmpty() && ![shareDataArray count])
        [shareDataArray addObject:data.shareData.title.createNSString().get()];

    _completionHandler = WTFMove(completionHandler);

    if (auto resolution = [_webView.get() _resolutionForShareSheetImmediateCompletionForTesting]) {
        _didShareSuccessfully = *resolution;
        [self dismiss];
        return;
    }
    
    if (!data.files.isEmpty()) {
        bool usePlaceholderFiles = data.originator == WebCore::ShareDataOriginator::Web;

        _temporaryFileShareDirectory = [WKShareSheet createTemporarySharingDirectory];
        appendFilesAsShareableURLs(WTFMove(shareDataArray), data.files, _temporaryFileShareDirectory.get(), usePlaceholderFiles, [retainedSelf = retainPtr(self), rect = WTFMove(rect)](RetainPtr<NSMutableArray>&& shareDataArray) mutable {
            if (!shareDataArray) {
                [retainedSelf dismiss];
                return;
            }
            [retainedSelf presentWithShareDataArray:shareDataArray.get() inRect:rect];
        });
        return;
    }
    
    [self presentWithShareDataArray:shareDataArray.get() inRect:rect];
}

- (void)presentWithShareDataArray:(NSArray *)sharingItems inRect:(std::optional<WebCore::FloatRect>)rect
{
    RetainPtr webView = _webView.get();

#if PLATFORM(MAC)
    _sharingServicePicker = adoptNS([[NSSharingServicePicker alloc] initWithItems:sharingItems]);
    _sharingServicePicker.get().delegate = self;
    
    // WKShareSheet can be released under NSSharingServicePicker delegate callbacks.
    RetainPtr<WKShareSheet> protector(self);
    NSRect presentationRect;

    if (rect)
        presentationRect = *rect;
    else {
        NSPoint location = [NSEvent mouseLocation];
        NSRect mouseLocationRect = NSMakeRect(location.x, location.y, 1.0, 1.0);
        NSRect mouseLocationInWindow = [retainPtr(webView.get().window) convertRectFromScreen:mouseLocationRect];
        presentationRect = [webView convertRect:mouseLocationInWindow fromView:nil];
    }

    [_sharingServicePicker.get() showPopoverRelativeToRect:presentationRect ofView:webView.get() preferredEdge:NSMinYEdge completion:^(NSSharingService *sharingService) { }];
#else
    _shareSheetViewController = adoptNS([[UIActivityViewController alloc] initWithActivityItems:sharingItems applicationActivities:nil]);

#if HAVE(UIACTIVITYTYPE_SHAREPLAY)
    [_shareSheetViewController setExcludedActivityTypes:@[ UIActivityTypeSharePlay ]];
#endif

    [_shareSheetViewController setCompletionWithItemsHandler:^(NSString *, BOOL completed, NSArray *, NSError *) {
        _didShareSuccessfully |= completed;

        // Make sure that we're actually not presented anymore (-completionWithItemsHandler can be called multiple times
        // before the share sheet is actually dismissed), and if so, clean up.
        if (![_shareSheetViewController presentingViewController] || [_shareSheetViewController isBeingDismissed])
            [self dismiss];
    }];

#if PLATFORM(VISION)
    if (webView.get().traitCollection.userInterfaceIdiom == UIUserInterfaceIdiomVision) {
        [_shareSheetViewController setAllowsCustomPresentationStyle:YES];
        [_shareSheetViewController setModalPresentationStyle:UIModalPresentationFormSheet];
    } else
#endif // PLATFORM(VISION)
    {
        UIPopoverPresentationController *popoverController = [_shareSheetViewController popoverPresentationController];
        popoverController.sourceView = webView.get();
        popoverController.sourceRect = rect.value_or(webView.get().bounds);
        if (!rect)
            popoverController.permittedArrowDirections = 0;
    }

    RetainPtr delegate = _delegate.get();
    if ([delegate respondsToSelector:@selector(shareSheet:willShowActivityItems:)])
        [delegate shareSheet:self willShowActivityItems:sharingItems];

    _presentationViewController = webView.get()._wk_viewControllerForFullScreenPresentation;
    [_presentationViewController presentViewController:_shareSheetViewController.get() animated:YES completion:nil];
#endif
}

#if PLATFORM(MAC)
- (void)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker didChooseSharingService:(NSSharingService *)service
{
    if (!service)
        [self dismiss];
}

- (id <NSSharingServiceDelegate>)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker delegateForSharingService:(NSSharingService *)sharingService
{
    return self;
}

- (NSWindow *)sharingService:(NSSharingService *)sharingService sourceWindowForShareItems:(NSArray *)items sharingContentScope:(NSSharingContentScope *)sharingContentScope
{
    return [_webView.get() window];
}

- (void)sharingService:(NSSharingService *)sharingService didFailToShareItems:(NSArray *)items error:(NSError *)error
{
    [self dismiss];
}

- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items
{
    _didShareSuccessfully = YES;
    [self dismiss];
}
#endif

- (void)dismiss
{
    auto completionHandler = WTFMove(_completionHandler);
    if (completionHandler)
        completionHandler(_didShareSuccessfully);
    
    if (_didShareSuccessfully) {
        // <rdar://problem/63030288>: didShareItems callback for NSSharingServiceDelegate currently is called
        // before the temporary files are copied, so we can't delete them here. UIActivityViewController doesn't
        // have this problem, so we can delete immediately for iOS.
#if PLATFORM(IOS_FAMILY)
        [[NSFileManager defaultManager] removeItemAtURL:_temporaryFileShareDirectory.get() error:nil];
#endif
    } else
        [[NSFileManager defaultManager] removeItemAtURL:_temporaryFileShareDirectory.get() error:nil];

    _temporaryFileShareDirectory = nullptr;

    auto dispatchDidDismiss = ^{
        RetainPtr delegate = _delegate.get();
        if ([delegate respondsToSelector:@selector(shareSheetDidDismiss:)])
            [delegate shareSheetDidDismiss:self];
    };

#if PLATFORM(MAC)
    [_sharingServicePicker hide];
    dispatchDidDismiss();
#else
    if (!_presentationViewController)
        return;

    UIViewController *currentPresentedViewController = [_presentationViewController presentedViewController];
    if (currentPresentedViewController != _shareSheetViewController) {
        dispatchDidDismiss();
        return;
    }

    [currentPresentedViewController dismissViewControllerAnimated:YES completion:^{
        dispatchDidDismiss();
        _presentationViewController = nil;
    }];
#endif
}

- (BOOL)dismissIfNeededWithReason:(WebKit::PickerDismissalReason)reason
{
#if PLATFORM(IOS_FAMILY)
    if (reason == WebKit::PickerDismissalReason::ViewRemoved) {
        if ([_shareSheetViewController _wk_isInFullscreenPresentation])
            return NO;
    }
#endif

    if (reason == WebKit::PickerDismissalReason::ProcessExited || reason == WebKit::PickerDismissalReason::ViewRemoved)
        [self setDelegate:nil];

    [self dismiss];
    return YES;
}

#if PLATFORM(MAC)
+ (BOOL)setQuarantineInformationForFilePath:(NSURL *)fileURL
{
    RetainPtr quarantineProperties = @{
        bridge_cast(kLSQuarantineTypeKey): bridge_cast(kLSQuarantineTypeWebDownload),
        bridge_cast(kLSQuarantineAgentBundleIdentifierKey): applicationBundleIdentifier().createNSString().get()
    };

    if (![fileURL setResourceValue:quarantineProperties.get() forKey:NSURLQuarantinePropertiesKey error:nil])
        return NO;

    // Whether the file was downloaded by sandboxed WebProcess or not, LSSetItemAttribute resets the flags to 0 (advisory QTN_FLAG_DOWNLOAD,
    // which can be then removed by WebProcess). Replace the flags with sandbox quarantine ones, which cannot be removed by sandboxed processes.
    return [WKShareSheet applyQuarantineSandboxAndDownloadFlagsToFileAtPath:fileURL];
}

+ (BOOL)applyQuarantineSandboxAndDownloadFlagsToFileAtPath:(NSURL *)fileURL
{
    qtn_file_t fq = qtn_file_alloc();
    auto scopeExit = WTF::makeScopeExit([&] {
        qtn_file_free(fq);
    });
    
    int quarantineError = qtn_file_init_with_path(fq, fileURL.fileSystemRepresentation);
    if (quarantineError)
        return NO;

    quarantineError = qtn_file_set_flags(fq, QTN_FLAG_SANDBOX | QTN_FLAG_DOWNLOAD);
    if (quarantineError)
        return NO;

    quarantineError = qtn_file_apply_to_path(fq, fileURL.fileSystemRepresentation);
    
    return YES;
}
#endif

+ (NSURL *)createTemporarySharingDirectory
{
    RetainPtr temporaryDirectory = FileSystem::createTemporaryDirectory(@"WKFileShare");
    
    if (![temporaryDirectory length])
        return nil;

    return [NSURL fileURLWithPath:temporaryDirectory.get() isDirectory:YES];
}

+ (NSURL *)createRandomSharingDirectoryForFile:(NSURL *)temporaryDirectory
{
    RetainPtr randomDirectory = createVersion4UUIDString().createNSString();
    if (![randomDirectory length] || !temporaryDirectory)
        return nil;
    RetainPtr dataPath = [temporaryDirectory URLByAppendingPathComponent:randomDirectory.get() isDirectory:YES];
    
    if (![[NSFileManager defaultManager] createDirectoryAtURL:dataPath.get() withIntermediateDirectories:NO attributes:nil error:nil])
        return nil;
    return dataPath.autorelease();
}

+ (NSURL *)writeFileToShareableURL:(NSString *)fileName data:(NSData *)fileData temporaryDirectory:(NSURL *)temporaryDirectory
{
    ASSERT(!RunLoop::isMain());
    if (!temporaryDirectory || ![fileName length] || !fileData)
        return nil;

    RetainPtr temporaryDirectoryForFile = [WKShareSheet createRandomSharingDirectoryForFile:temporaryDirectory];
    if (!temporaryDirectoryForFile)
        return nil;

    RetainPtr fileURL = [temporaryDirectoryForFile URLByAppendingPathComponent:fileName isDirectory:NO];

    if (![fileData writeToURL:fileURL.get() options:NSDataWritingAtomic error:nil])
        return nil;
#if PLATFORM(MAC)
    if (![WKShareSheet setQuarantineInformationForFilePath:fileURL.get()])
        return nil;
#endif
    return fileURL.autorelease();
}

@end

#endif // HAVE(SHARE_SHEET_UI)
