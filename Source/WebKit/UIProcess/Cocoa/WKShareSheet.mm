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

#if PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/ShareData.h>
#import <pal/spi/mac/QuarantineSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/Scope.h>
#import <wtf/UUID.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WorkQueue.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import "WKContentViewInteraction.h"
#else
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#endif

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
    return _delegate.get().get();
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

static void appendFilesAsShareableURLs(RetainPtr<NSMutableArray>&& shareDataArray, const Vector<WebCore::RawFile>& files, NSURL* temporaryDirectory, CompletionHandler<void(RetainPtr<NSMutableArray>&&)>&& completionHandler)
{
    struct FileWriteTask {
        String fileName;
        RetainPtr<NSData> fileData;
    };
    auto fileWriteTasks = files.map([](auto& file) {
        return FileWriteTask { file.fileName.isolatedCopy(), file.fileData->createNSData() };
    });

    auto queue = WorkQueue::create("com.apple.WebKit.WKShareSheet.ShareableFileWriter");
    queue->dispatch([shareDataArray = WTFMove(shareDataArray), fileWriteTasks = WTFMove(fileWriteTasks), temporaryDirectory = retainPtr(temporaryDirectory), completionHandler = WTFMove(completionHandler)]() mutable {
        for (auto& fileWriteTask : fileWriteTasks) {
            NSURL *fileURL = [WKShareSheet writeFileToShareableURL:WebCore::ResourceResponseBase::sanitizeSuggestedFilename(fileWriteTask.fileName) data:fileWriteTask.fileData.get() temporaryDirectory:temporaryDirectory.get()];
            if (!fileURL) {
                shareDataArray = nullptr;
                break;
            }
            [shareDataArray addObject:fileURL];
        }
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), shareDataArray = WTFMove(shareDataArray)]() mutable {
            completionHandler(WTFMove(shareDataArray));
        });
    });
}

- (void)presentWithParameters:(const WebCore::ShareDataWithParsedURL &)data inRect:(std::optional<WebCore::FloatRect>)rect completionHandler:(WTF::CompletionHandler<void(bool)>&&)completionHandler
{
    auto shareDataArray = adoptNS([[NSMutableArray alloc] init]);
    
    if (!data.shareData.text.isEmpty())
        [shareDataArray addObject:(NSString *)data.shareData.text];
    
    if (data.url) {
        NSURL *url = (NSURL *)data.url.value();
#if PLATFORM(IOS_FAMILY)
        if (!data.shareData.title.isEmpty())
            url._title = data.shareData.title;
#endif
        [shareDataArray addObject:url];
    }
    
    if (!data.shareData.title.isEmpty() && ![shareDataArray count])
        [shareDataArray addObject:(NSString *)data.shareData.title];

    _completionHandler = WTFMove(completionHandler);

    if (auto resolution = [_webView _resolutionForShareSheetImmediateCompletionForTesting]) {
        _didShareSuccessfully = *resolution;
        [self dismiss];
        return;
    }
    
    if (!data.files.isEmpty()) {
        _temporaryFileShareDirectory = [WKShareSheet createTemporarySharingDirectory];
        appendFilesAsShareableURLs(WTFMove(shareDataArray), data.files, _temporaryFileShareDirectory.get(), [retainedSelf = retainPtr(self), rect = WTFMove(rect)](RetainPtr<NSMutableArray>&& shareDataArray) mutable {
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
    WKWebView *webView = _webView.getAutoreleased();

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
        NSRect mouseLocationInWindow = [webView.window convertRectFromScreen:mouseLocationRect];
        presentationRect = [webView convertRect:mouseLocationInWindow fromView:nil];
    }

#if HAVE(SHARING_SERVICE_PICKER_POPOVER_SPI)
    [_sharingServicePicker.get() showPopoverRelativeToRect:presentationRect ofView:webView preferredEdge:NSMinYEdge completion:^(NSSharingService *sharingService) { }];
#else
    [_sharingServicePicker showRelativeToRect:presentationRect ofView:webView preferredEdge:NSMinYEdge];
#endif
#else
    _shareSheetViewController = adoptNS([[UIActivityViewController alloc] initWithActivityItems:sharingItems applicationActivities:nil]);

#if HAVE(UIACTIVITYTYPE_SHAREPLAY)
    [_shareSheetViewController setExcludedActivityTypes:@[ UIActivityTypeSharePlay ]];
#endif

    [_shareSheetViewController setCompletionWithItemsHandler:^(NSString *, BOOL completed, NSArray *, NSError *) {
        _didShareSuccessfully |= completed;

        // Make sure that we're actually not presented anymore (-completionWithItemsHandler can be called multiple times
        // before the share sheet is actually dismissed), and if so, clean up.
        if (![_shareSheetViewController presentingViewController])
            [self dismiss];
    }];

    UIPopoverPresentationController *popoverController = [_shareSheetViewController popoverPresentationController];
    if (rect) {
        popoverController.sourceView = webView;
        popoverController.sourceRect = *rect;
    } else
        popoverController._centersPopoverIfSourceViewNotSet = YES;

    if ([_delegate respondsToSelector:@selector(shareSheet:willShowActivityItems:)])
        [_delegate shareSheet:self willShowActivityItems:sharingItems];

    _presentationViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:webView];
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
    return [_webView window];
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
        if ([_delegate respondsToSelector:@selector(shareSheetDidDismiss:)])
            [_delegate shareSheetDidDismiss:self];
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

#if PLATFORM(MAC)
+ (BOOL)setQuarantineInformationForFilePath:(NSURL *)fileURL
{
    auto quarantineProperties = @{
        (__bridge NSString *)kLSQuarantineTypeKey: (__bridge NSString *)kLSQuarantineTypeWebDownload,
        (__bridge NSString *)kLSQuarantineAgentBundleIdentifierKey: WebCore::applicationBundleIdentifier()
    };

    if (![fileURL setResourceValue:quarantineProperties forKey:NSURLQuarantinePropertiesKey error:nil])
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
    NSString *temporaryDirectory = FileSystem::createTemporaryDirectory(@"WKFileShare");
    
    if (![temporaryDirectory length])
        return nil;

    return [NSURL fileURLWithPath:temporaryDirectory isDirectory:YES];
}

+ (NSURL *)createRandomSharingDirectoryForFile:(NSURL *)temporaryDirectory
{
    NSString *randomDirectory = createVersion4UUIDString();
    if (![randomDirectory length] || !temporaryDirectory)
        return nil;
    NSURL *dataPath = [temporaryDirectory URLByAppendingPathComponent:randomDirectory isDirectory:YES];
    
    if (![[NSFileManager defaultManager] createDirectoryAtURL:dataPath withIntermediateDirectories:NO attributes:nil error:nil])
        return nil;
    return dataPath;
}

+ (NSURL *)writeFileToShareableURL:(NSString *)fileName data:(NSData *)fileData temporaryDirectory:(NSURL *)temporaryDirectory
{
    ASSERT(!RunLoop::isMain());
    if (!temporaryDirectory || ![fileName length] || !fileData)
        return nil;
    
    NSURL *temporaryDirectoryForFile = [WKShareSheet createRandomSharingDirectoryForFile:temporaryDirectory];
    if (!temporaryDirectoryForFile)
        return nil;
    
    NSURL *fileURL = [temporaryDirectoryForFile URLByAppendingPathComponent:fileName isDirectory:NO];

    if (![fileData writeToURL:fileURL options:NSDataWritingAtomic error:nil])
        return nil;
#if PLATFORM(MAC)
    if (![WKShareSheet setQuarantineInformationForFilePath:fileURL])
        return nil;
#endif
    return fileURL;
}

@end

#endif // PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
