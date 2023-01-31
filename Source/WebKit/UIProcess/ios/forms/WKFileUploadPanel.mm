/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#import "WKFileUploadPanel.h"

#if PLATFORM(IOS_FAMILY)

#import "APIArray.h"
#import "APIData.h"
#import "APIOpenPanelParameters.h"
#import "APIString.h"
#import "PhotosUISPI.h"
#import "UIKitSPI.h"
#import "UserInterfaceIdiom.h"
#import "WKContentViewInteraction.h"
#import "WKData.h"
#import "WKStringCF.h"
#import "WKURLCF.h"
#import "WKWebViewInternal.h"
#import "WebIconUtilities.h"
#import "WebOpenPanelResultListenerProxy.h"
#import "WebPageProxy.h"
#import <UIKit/UIDocumentPickerViewController.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MIMETypeRegistry.h>
#import <wtf/MainThread.h>
#import <wtf/OptionSet.h>
#import <wtf/RetainPtr.h>
#import <wtf/SetForScope.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/StringView.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

#if HAVE(PX_ACTIVITY_PROGRESS_CONTROLLER)
SOFT_LINK_PRIVATE_FRAMEWORK(PhotosUICore)
SOFT_LINK_CLASS(PhotosUICore, PXActivityProgressController)
#else
SOFT_LINK_PRIVATE_FRAMEWORK(PhotosUIPrivate)
SOFT_LINK_CLASS(PhotosUIPrivate, PUActivityProgressController)
#endif

#if HAVE(PHOTOS_UI)
SOFT_LINK_FRAMEWORK(PhotosUI)
SOFT_LINK_CLASS(PhotosUI, PHPickerConfiguration)
SOFT_LINK_CLASS(PhotosUI, PHPickerViewController)
SOFT_LINK_CLASS(PhotosUI, PHPickerResult)
#endif

enum class WKFileUploadPanelImagePickerType : uint8_t {
    Image = 1 << 0,
    Video  = 1 << 1,
};

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

static inline UIImagePickerControllerCameraDevice cameraDeviceForMediaCaptureType(WebCore::MediaCaptureType mediaCaptureType)
{
    return mediaCaptureType == WebCore::MediaCaptureType::MediaCaptureTypeUser ? UIImagePickerControllerCameraDeviceFront : UIImagePickerControllerCameraDeviceRear;
}

static bool setContainsUTIThatConformsTo(NSSet<NSString *> *typeIdentifiers, UTType *conformToUTType)
{
    for (NSString *uti in typeIdentifiers) {
        UTType *type = [UTType typeWithIdentifier:uti];
        if ([type conformsToType:conformToUTType])
            return true;
    }
    return false;
}

#if HAVE(PHOTOS_UI)

static NSString * firstUTIThatConformsTo(NSArray<NSString *> *typeIdentifiers, UTType *conformToUTType)
{
    for (NSString *uti in typeIdentifiers) {
        UTType *type = [UTType typeWithIdentifier:uti];
        if ([type conformsToType:conformToUTType])
            return uti;
    }
    return nil;
}

#endif

#pragma mark - _WKFileUploadItem

@interface _WKFileUploadItem : NSObject
- (instancetype)initWithFileURL:(NSURL *)fileURL;
@property (nonatomic, readonly, getter=isVideo) BOOL video;
@property (nonatomic, readonly) RetainPtr<UIImage> displayImage;
@end

@implementation _WKFileUploadItem {
    RetainPtr<NSURL> _fileURL;
}

- (instancetype)initWithFileURL:(NSURL *)fileURL
{
    if (!(self = [super init]))
        return nil;

    ASSERT([fileURL isFileURL]);
    ASSERT([[NSFileManager defaultManager] fileExistsAtPath:fileURL.path]);
    _fileURL = fileURL;
    return self;
}

- (BOOL)isVideo
{
    ASSERT_NOT_REACHED();
    return NO;
}

- (NSURL *)fileURL
{
    return _fileURL.get();
}

- (void)setFileURL:(NSURL *)fileURL
{
    _fileURL = fileURL;
}

- (RetainPtr<UIImage>)displayImage
{
    ASSERT_NOT_REACHED();
    return nil;
}

@end


@interface _WKImageFileUploadItem : _WKFileUploadItem
@end

@implementation _WKImageFileUploadItem

- (BOOL)isVideo
{
    return NO;
}

- (RetainPtr<UIImage>)displayImage
{
    return WebKit::iconForImageFile(self.fileURL);
}

@end


@interface _WKVideoFileUploadItem : _WKFileUploadItem
@end

@implementation _WKVideoFileUploadItem

- (BOOL)isVideo
{
    return YES;
}

- (RetainPtr<UIImage>)displayImage
{
    return WebKit::iconForVideoFile(self.fileURL);
}

@end

#pragma mark - WKFileUploadMediaTranscoder

#if ENABLE(TRANSCODE_UIIMAGEPICKERCONTROLLER_VIDEO)

@interface WKFileUploadMediaTranscoder : NSObject

- (instancetype)initWithItems:(NSArray *)items videoCount:(NSUInteger)videoCount completionHandler:(WTF::Function<void(NSArray<_WKFileUploadItem *> *)>&&)completionHandler;

- (void)start;

@end

@implementation WKFileUploadMediaTranscoder {
    RetainPtr<NSTimer> _progressTimer;
#if HAVE(PX_ACTIVITY_PROGRESS_CONTROLLER)
    RetainPtr<PXActivityProgressController> _progressController;
#else
    RetainPtr<PUActivityProgressController> _progressController;
#endif
    RetainPtr<AVAssetExportSession> _exportSession;
    RetainPtr<NSArray<_WKFileUploadItem *>> _items;
    RetainPtr<NSString> _temporaryDirectoryPath;

    // Only called if the transcoding is not cancelled.
    WTF::Function<void(NSArray<_WKFileUploadItem *> *)> _completionHandler;

    NSUInteger _videoCount;
    NSUInteger _processedVideoCount;
}

- (instancetype)initWithItems:(NSArray<_WKFileUploadItem *> *)items videoCount:(NSUInteger)videoCount completionHandler:(WTF::Function<void(NSArray<_WKFileUploadItem *> *)>&&)completionHandler
{
    if (!(self = [super init]))
        return nil;

    _items = items;
    _processedVideoCount = 0;
    _videoCount = videoCount;

    _completionHandler = WTFMove(completionHandler);

    return self;
}

- (void)start
{
#if HAVE(PX_ACTIVITY_PROGRESS_CONTROLLER)
    _progressController = adoptNS([allocPXActivityProgressControllerInstance() init]);
#else
    _progressController = adoptNS([allocPUActivityProgressControllerInstance() init]);
#endif
    [_progressController setTitle:WEB_UI_STRING_KEY("Preparing…", "Preparing (file upload)", "Title for file upload progress view")];
    [_progressController showAnimated:YES allowDelay:YES];

    [_progressController setCancellationHandler:makeBlockPtr([weakSelf = WeakObjCPtr<WKFileUploadMediaTranscoder>(self)] {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        [strongSelf->_exportSession cancelExport];
        [strongSelf _dismissProgress];
    }).get()];

    _progressTimer = [NSTimer scheduledTimerWithTimeInterval:0.1f target:self selector:@selector(_updateProgress:) userInfo:nil repeats:YES];

    [self _processItemAtIndex:0];
}

- (void)_processItemAtIndex:(NSUInteger)index
{
    if ([_progressController isCancelled])
        return;

    if (index >= [_items count]) {
        [self _finishedProcessing];
        return;
    }

    _WKFileUploadItem *item = [_items objectAtIndex:index];

    while (!item.isVideo) {
        index++;

        if (index == [_items count]) {
            [self _finishedProcessing];
            return;
        }

        item = [_items objectAtIndex:index];
    }

    NSString *temporaryDirectory = [self _temporaryDirectoryCreateIfNecessary];
    if (!temporaryDirectory) {
        LOG_ERROR("WKFileUploadMediaTranscoder: Failed to make temporary directory");
        [self _finishedProcessing];
        return;
    }

    NSString *fileName = [item.fileURL.lastPathComponent.stringByDeletingPathExtension stringByAppendingPathExtension:UTTypeQuickTimeMovie.preferredFilenameExtension.uppercaseString];
    NSString *filePath = [temporaryDirectory stringByAppendingPathComponent:fileName];
    NSURL *outputURL = [NSURL fileURLWithPath:filePath isDirectory:NO];

    RetainPtr<AVURLAsset> asset = adoptNS([PAL::allocAVURLAssetInstance() initWithURL:item.fileURL options:nil]);
    _exportSession = adoptNS([PAL::allocAVAssetExportSessionInstance() initWithAsset:asset.get() presetName:AVAssetExportPresetHighestQuality]);
    [_exportSession setOutputURL:outputURL];
    [_exportSession setOutputFileType:AVFileTypeQuickTimeMovie];

    [_exportSession exportAsynchronouslyWithCompletionHandler:makeBlockPtr([weakSelf = WeakObjCPtr<WKFileUploadMediaTranscoder>(self), index] () mutable {
        ensureOnMainRunLoop([weakSelf = WTFMove(weakSelf), index] {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            AVAssetExportSessionStatus status = [strongSelf->_exportSession status];

            if (status == AVAssetExportSessionStatusCancelled)
                return;

            if (status == AVAssetExportSessionStatusCompleted) {
                _WKFileUploadItem *item = [strongSelf->_items objectAtIndex:index];
                [item setFileURL:[strongSelf->_exportSession outputURL]];
            }

            strongSelf->_exportSession = nil;

            strongSelf->_processedVideoCount++;
            [strongSelf _processItemAtIndex:index + 1];
        });
    }).get()];
}

- (void)_finishedProcessing
{
    [self _dismissProgress];

    if (auto completionHandler = std::exchange(_completionHandler, nullptr))
        completionHandler(_items.get());
}

- (void)_dismissProgress
{
    [_progressTimer invalidate];
    [_progressController hideAnimated:NO allowDelay:NO];
}

- (void)_updateProgress:(NSTimer *)timer
{
    auto currentSessionProgress = [_exportSession progress];
    [_progressController setFractionCompleted:(currentSessionProgress + _processedVideoCount) / _videoCount];
}

- (NSString *)_temporaryDirectoryCreateIfNecessary
{
    if (_temporaryDirectoryPath) {
        BOOL isDirectory = NO;
        BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:_temporaryDirectoryPath.get() isDirectory:&isDirectory];

        if (exists && isDirectory)
            return _temporaryDirectoryPath.get();
    }

    _temporaryDirectoryPath = FileSystem::createTemporaryDirectory(@"WKVideoUpload");
    return _temporaryDirectoryPath.get();
}

@end

#endif // ENABLE(TRANSCODE_UIIMAGEPICKERCONTROLLER_VIDEO)

#pragma mark - WKFileUploadPanel

@interface WKFileUploadPanel () <UINavigationControllerDelegate, UIImagePickerControllerDelegate, UIDocumentPickerDelegate, UIAdaptivePresentationControllerDelegate
#if USE(UICONTEXTMENU)
, UIContextMenuInteractionDelegate
#endif
#if HAVE(PHOTOS_UI)
, PHPickerViewControllerDelegate
#endif
>
@end

@implementation WKFileUploadPanel {
    WeakObjCPtr<WKContentView> _view;
    RefPtr<WebKit::WebOpenPanelResultListenerProxy> _listener;
    RetainPtr<NSSet<NSString *>> _acceptedUTIs;
    OptionSet<WKFileUploadPanelImagePickerType> _allowedImagePickerTypes;
    CGPoint _interactionPoint;
    BOOL _allowMultipleFiles;
    BOOL _usingCamera;
#if ENABLE(TRANSCODE_UIIMAGEPICKERCONTROLLER_VIDEO)
    RetainPtr<WKFileUploadMediaTranscoder> _mediaTranscoder;
#endif
    RetainPtr<UIImagePickerController> _cameraPicker;
#if HAVE(PHOTOS_UI)
    RetainPtr<PHPickerViewController> _photoPicker;
#endif
    RetainPtr<UIViewController> _presentationViewController;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    BOOL _isPresentingSubMenu;
    ALLOW_DEPRECATED_DECLARATIONS_END
#if USE(UICONTEXTMENU)
    BOOL _isRepositioningContextMenu;
    RetainPtr<UIContextMenuInteraction> _documentContextMenuInteraction;
#endif
    RetainPtr<UIDocumentPickerViewController> _documentPickerController;
    WebCore::MediaCaptureType _mediaCaptureType;
    Vector<RetainPtr<NSURL>> _temporaryUploadedFileURLs;
    RetainPtr<NSFileManager> _uploadFileManager;
    RetainPtr<NSFileCoordinator> _uploadFileCoordinator;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;
    _view = view;
    return self;
}

- (void)dealloc
{
    [_cameraPicker setDelegate:nil];
    [_documentPickerController setDelegate:nil];
#if USE(UICONTEXTMENU)
    [self removeContextMenuInteraction];
#endif
    [super dealloc];
}

- (void)_dispatchDidDismiss
{
    if ([_delegate respondsToSelector:@selector(fileUploadPanelDidDismiss:)])
        [_delegate fileUploadPanelDidDismiss:self];
}

#pragma mark - Panel Completion (one of these must be called)

- (void)_cancel
{
    if (_listener)
        _listener->cancel();

    [self _dispatchDidDismiss];
}

- (void)_chooseMediaItems:(NSArray<_WKFileUploadItem *> *)mediaItems
{
    RetainPtr<UIImage> iconImage = nil;
    NSMutableArray *fileURLs = [NSMutableArray array];
    NSUInteger videoCount = 0;

    for (_WKFileUploadItem *item in mediaItems) {
        [fileURLs addObject:item.fileURL];

        if (!iconImage)
            iconImage = item.displayImage;

        if (item.isVideo)
            videoCount++;
    }

    NSUInteger imageCount = mediaItems.count - videoCount;

    NSString *displayString;
    if (mediaItems.count == 1)
        displayString = mediaItems.firstObject.fileURL.lastPathComponent;
    else
        displayString = (imageCount || videoCount) ? [NSString localizedStringWithFormat:WEB_UI_NSSTRING(@"%lu photo(s) and %lu video(s)", "label next to file upload control; parameters are the number of photos and the number of videos"), (unsigned long)imageCount, (unsigned long)videoCount] : nil;

    [self _dismissDisplayAnimated:YES];
    [self _chooseFiles:fileURLs displayString:displayString iconImage:iconImage.get()];
}

- (void)_chooseFiles:(NSArray *)fileURLs displayString:(NSString *)displayString iconImage:(UIImage *)iconImage
{
    NSUInteger count = [fileURLs count];
    if (!count) {
        [self _cancel];
        return;
    }

    Vector<String> filenames;
    filenames.reserveInitialCapacity(count);
    for (NSURL *fileURL in fileURLs)
        filenames.uncheckedAppend(String::fromUTF8(fileURL.fileSystemRepresentation));

    NSData *png = UIImagePNGRepresentation(iconImage);
    RefPtr<API::Data> iconImageDataRef = adoptRef(WebKit::toImpl(WKDataCreate(reinterpret_cast<const unsigned char*>([png bytes]), [png length])));

    _listener->chooseFiles(filenames, displayString, iconImageDataRef.get());
    [self _dispatchDidDismiss];
}

#pragma mark - Present / Dismiss API

- (void)presentWithParameters:(API::OpenPanelParameters*)parameters resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener
{
    ASSERT(!_listener);

    _listener = listener;
    _allowMultipleFiles = parameters->allowMultipleFiles();
    _interactionPoint = [_view lastInteractionLocation];

    Ref<API::Array> acceptMimeTypes = parameters->acceptMIMETypes();
    NSMutableArray *mimeTypes = [NSMutableArray arrayWithCapacity:acceptMimeTypes->size()];
    for (auto mimeType : acceptMimeTypes->elementsOfType<API::String>())
        [mimeTypes addObject:mimeType->string()];

    Ref<API::Array> acceptFileExtensions = parameters->acceptFileExtensions();
    for (auto extension : acceptFileExtensions->elementsOfType<API::String>()) {
        String mimeType = WebCore::MIMETypeRegistry::mimeTypeForExtension(extension->stringView().substring(1));
        if (!mimeType.isEmpty())
            [mimeTypes addObject:mimeType];
    }

    _acceptedUTIs = UTIsForMIMETypes(mimeTypes);
    if (![_acceptedUTIs count])
        _allowedImagePickerTypes.add({ WKFileUploadPanelImagePickerType::Image, WKFileUploadPanelImagePickerType::Video });
    else {
        if (setContainsUTIThatConformsTo(_acceptedUTIs.get(), UTTypeImage))
            _allowedImagePickerTypes.add({ WKFileUploadPanelImagePickerType::Image });
        if (setContainsUTIThatConformsTo(_acceptedUTIs.get(), UTTypeMovie))
            _allowedImagePickerTypes.add({ WKFileUploadPanelImagePickerType::Video });
    }

    _mediaCaptureType = WebCore::MediaCaptureType::MediaCaptureTypeNone;
#if ENABLE(MEDIA_CAPTURE)
    _mediaCaptureType = parameters->mediaCaptureType();
#endif

    if (![self platformSupportsPickerViewController]) {
        [self _cancel];
        return;
    }

    if ([self _shouldMediaCaptureOpenMediaDevice]) {
        [self _adjustMediaCaptureType];

        _usingCamera = YES;
        [self _showCamera];

        return;
    }

    [self showDocumentPickerMenu];
}

- (void)dismiss
{
    // Dismiss any view controller that is being presented. This works for all types of view controllers, popovers, etc.
    // If there is any kind of view controller presented on this view, it will be removed.

    if (auto view = _view.get())
        [[UIViewController _viewControllerForFullScreenPresentationFromView:view.get()] dismissViewControllerAnimated:NO completion:nil];

    _presentationViewController = nil;

    [self _cancel];
}

- (void)_dismissDisplayAnimated:(BOOL)animated
{
    if (_presentationViewController) {
        UIViewController *currentPresentedViewController = [_presentationViewController presentedViewController];
#if HAVE(PHOTOS_UI)
        if (currentPresentedViewController == self || currentPresentedViewController == _cameraPicker.get() || currentPresentedViewController == _photoPicker.get()) {
#else
        if (currentPresentedViewController == self || currentPresentedViewController == _cameraPicker.get()) {
#endif
            [currentPresentedViewController dismissViewControllerAnimated:animated completion:^{
                _presentationViewController = nil;
            }];
        }
    }
}

- (NSArray<NSString *> *)currentAvailableActionTitles
{
    NSMutableArray<NSString *> *actionTitles = [NSMutableArray array];

    if (_allowedImagePickerTypes.containsAny({ WKFileUploadPanelImagePickerType::Image, WKFileUploadPanelImagePickerType::Video })) {
        [actionTitles addObject:@"Photo Library"];
        if (_allowedImagePickerTypes.containsAll({ WKFileUploadPanelImagePickerType::Image, WKFileUploadPanelImagePickerType::Video }))
            [actionTitles addObject:@"Take Photo or Video"];
        else if (_allowedImagePickerTypes.contains(WKFileUploadPanelImagePickerType::Video))
            [actionTitles addObject:@"Take Video"];
        else
            [actionTitles addObject:@"Take Photo"];
    }
    [actionTitles addObject:[self _chooseFilesButtonLabel]];
    return actionTitles;
}

- (NSArray<NSString *> *)acceptedTypeIdentifiers
{
    return [[_acceptedUTIs allObjects] sortedArrayUsingSelector:@selector(compare:)];
}

#pragma mark - Media Types

static NSSet<NSString *> *UTIsForMIMETypes(NSArray *mimeTypes)
{
    NSMutableSet *mediaTypes = [NSMutableSet set];
    for (NSString *mimeType in mimeTypes) {
        if ([mimeType isEqualToString:@"*/*"])
            return [NSSet set];

        if ([mimeType caseInsensitiveCompare:@"image/*"] == NSOrderedSame)
            [mediaTypes addObject:UTTypeImage.identifier];
        else if ([mimeType caseInsensitiveCompare:@"video/*"] == NSOrderedSame)
            [mediaTypes addObject:UTTypeMovie.identifier];
        else if ([mimeType caseInsensitiveCompare:@"audio/*"] == NSOrderedSame)
            // UIImagePickerController doesn't allow audio-only recording, so show the video
            // recorder for "audio/*".
            [mediaTypes addObject:UTTypeMovie.identifier];
        else {
            auto uti = [UTType typeWithMIMEType:mimeType];
            if (uti)
                [mediaTypes addObject:uti.identifier];
        }
    }
    return mediaTypes;
}

- (NSArray<NSString *> *)_mediaTypesForPickerSourceType:(UIImagePickerControllerSourceType)sourceType
{
    NSArray<NSString *> *availableMediaTypeUTIs = [UIImagePickerController availableMediaTypesForSourceType:sourceType];
    NSSet<NSString *> *acceptedMediaTypeUTIs = _acceptedUTIs.get();
    if (acceptedMediaTypeUTIs.count) {
        NSMutableArray<NSString *> *mediaTypes = [NSMutableArray array];
        for (NSString *availableMediaTypeUTI in availableMediaTypeUTIs) {
            if ([acceptedMediaTypeUTIs containsObject:availableMediaTypeUTI])
                [mediaTypes addObject:availableMediaTypeUTI];
            else {
                UTType *availableMediaType = [UTType typeWithIdentifier:availableMediaTypeUTI];
                for (NSString *acceptedMediaTypeUTI in acceptedMediaTypeUTIs) {
                    UTType *acceptedMediaType = [UTType typeWithIdentifier:acceptedMediaTypeUTI];
                    if ([acceptedMediaType conformsToType:availableMediaType]) {
                        [mediaTypes addObject:availableMediaTypeUTI];
                        break;
                    }
                }
            }
        }

        ASSERT(mediaTypes.count);
        if (mediaTypes.count)
            return mediaTypes;
    }

    // Fallback to every supported media type if there is no filter.
    return availableMediaTypeUTIs;
}

#pragma mark - Source selection menu

- (NSString *)_chooseFilesButtonLabel
{
    if (_allowMultipleFiles)
        return WebCore::fileButtonChooseMultipleFilesLabel();

    return WebCore::fileButtonChooseFileLabel();
}

- (NSString *)_photoLibraryButtonLabel
{
    return WEB_UI_STRING_KEY("Photo Library", "Photo Library (file upload action sheet)", "File Upload alert sheet button string for choosing an existing media item from the Photo Library");
}

- (NSString *)_cameraButtonLabel
{
    ASSERT(_allowedImagePickerTypes.containsAny({ WKFileUploadPanelImagePickerType::Image, WKFileUploadPanelImagePickerType::Video }));

    if (_allowedImagePickerTypes.containsAll({ WKFileUploadPanelImagePickerType::Image, WKFileUploadPanelImagePickerType::Video }))
        return WEB_UI_STRING_KEY("Take Photo or Video", "Take Photo or Video (file upload action sheet)", "File Upload alert sheet camera button string for taking photos or videos");

    if (_allowedImagePickerTypes.contains(WKFileUploadPanelImagePickerType::Video))
        return WEB_UI_STRING_KEY("Take Video", "Take Video (file upload action sheet)", "File Upload alert sheet camera button string for taking only videos");

    return WEB_UI_STRING_KEY("Take Photo", "Take Photo (file upload action sheet)", "File Upload alert sheet camera button string for taking only photos");
}

#if USE(UICONTEXTMENU)

#if HAVE(UI_CONTEXT_MENU_PREVIEW_ITEM_IDENTIFIER)
- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configuration:(UIContextMenuConfiguration *)configuration highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier
#else
- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForHighlightingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
#endif
{
    return [_view _createTargetedContextMenuHintPreviewIfPossible];
}

- (_UIContextMenuStyle *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction styleForMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    _UIContextMenuStyle *style = [_UIContextMenuStyle defaultStyle];
    style.preferredLayout = _UIContextMenuLayoutCompactMenu;
    return style;
}

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location {

    UIContextMenuActionProvider actionMenuProvider = [self, weakSelf = WeakObjCPtr<WKFileUploadPanel>(self)] (NSArray<UIMenuElement *> *) -> UIMenu * {
        NSArray *actions;

        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return nil;

        self->_isPresentingSubMenu = NO;
        UIAction *chooseAction = [UIAction actionWithTitle:[strongSelf _chooseFilesButtonLabel] image:[UIImage systemImageNamed:@"folder"] identifier:@"choose" handler:^(__kindof UIAction *action) {
            self->_isPresentingSubMenu = YES;
            [self showFilePickerMenu];
        }];

        UIAction *photoAction = [UIAction actionWithTitle:[strongSelf _photoLibraryButtonLabel] image:[UIImage systemImageNamed:@"photo.on.rectangle"] identifier:@"photo" handler:^(__kindof UIAction *action) {
            self->_isPresentingSubMenu = YES;
            [self _showPhotoPicker];
        }];

        if ([UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
            NSString *cameraString = [strongSelf _cameraButtonLabel];
            UIAction *cameraAction = [UIAction actionWithTitle:cameraString image:[UIImage systemImageNamed:@"camera"] identifier:@"camera" handler:^(__kindof UIAction *action) {
                _usingCamera = YES;
                self->_isPresentingSubMenu = YES;
                [self _showCamera];
            }];
            actions = @[photoAction, cameraAction, chooseAction];
        } else
            actions = @[photoAction, chooseAction];

        return [UIMenu menuWithTitle:@"" children:actions];
    };

    return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:actionMenuProvider];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id<UIContextMenuInteractionAnimating>)animator
{
    if (_isRepositioningContextMenu)
        return;

    [animator addCompletion:^{
        [self removeContextMenuInteraction];
        if (!self->_isPresentingSubMenu)
            [self _cancel];
    }];
}

- (void)removeContextMenuInteraction
{
    if (_documentContextMenuInteraction) {
        [_view removeInteraction:_documentContextMenuInteraction.get()];
        _documentContextMenuInteraction = nil;
        [_view _removeContextMenuHintContainerIfPossible];
    }
}

- (UIContextMenuInteraction *)ensureContextMenuInteraction
{
    if (!_documentContextMenuInteraction) {
        _documentContextMenuInteraction = adoptNS([[UIContextMenuInteraction alloc] initWithDelegate:self]);
        [_view addInteraction:_documentContextMenuInteraction.get()];
        self->_isPresentingSubMenu = NO;
    }
    return _documentContextMenuInteraction.get();
}

- (void)repositionContextMenuIfNeeded
{
    if (!_documentContextMenuInteraction)
        return;

    auto *webView = [_view webView];
    if (!webView)
        return;

    auto inputViewBoundsInWindow = webView->_inputViewBoundsInWindow;
    if (CGRectIsEmpty(inputViewBoundsInWindow))
        return;

    // The exact bounds of the context menu container itself isn't exposed through any UIKit API or SPI,
    // and would require traversing the view hierarchy in search of internal UIKit views. For now, just
    // reposition the context menu if its presentation location is covered by the input view.
    if (!CGRectContainsPoint(inputViewBoundsInWindow, [_view convertPoint:_interactionPoint toView:webView.window]))
        return;

    SetForScope repositioningContextMenuScope { _isRepositioningContextMenu, YES };
    [UIView performWithoutAnimation:^{
        [_documentContextMenuInteraction dismissMenu];
        [_view presentContextMenu:_documentContextMenuInteraction.get() atLocation:_interactionPoint];
    }];
}

#endif // USE(UICONTEXTMENU)

- (void)showFilePickerMenu
{
    NSArray *mediaTypes = [_acceptedUTIs allObjects];
    NSArray *documentTypes = mediaTypes.count ? mediaTypes : @[ UTTypeItem.identifier ];

    _uploadFileManager = adoptNS([[NSFileManager alloc] init]);
    _uploadFileCoordinator = adoptNS([[NSFileCoordinator alloc] init]);

    _documentPickerController = adoptNS([[UIDocumentPickerViewController alloc] initWithDocumentTypes:documentTypes inMode:UIDocumentPickerModeImport]);
    [_documentPickerController setAllowsMultipleSelection:_allowMultipleFiles];
    [_documentPickerController setDelegate:self];
    [_documentPickerController presentationController].delegate = self;
    if ([_delegate respondsToSelector:@selector(fileUploadPanelDestinationIsManaged:)])
        [_documentPickerController _setIsContentManaged:[_delegate fileUploadPanelDestinationIsManaged:self]];
    [self _presentFullscreenViewController:_documentPickerController.get() animated:YES];
}

- (void)showDocumentPickerMenu
{
    // FIXME 49961589: Support picking media with UIImagePickerController
#if HAVE(UICONTEXTMENU_LOCATION)
    if (_allowedImagePickerTypes.containsAny({ WKFileUploadPanelImagePickerType::Image, WKFileUploadPanelImagePickerType::Video }))
        [_view presentContextMenu:self.ensureContextMenuInteraction atLocation:_interactionPoint];
    else // Image and Video types are not accepted so bypass the menu and open the file picker directly.
#endif
        [self showFilePickerMenu];

    // Clear out the view controller we just presented. Don't save a reference to the UIDocumentPickerViewController as it is self dismissing.
    _presentationViewController = nil;
}

#pragma mark - Image Picker

- (void)_adjustMediaCaptureType
{
    if ([UIImagePickerController isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceFront] || [UIImagePickerController isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceRear]) {
        if (![UIImagePickerController isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceFront])
            _mediaCaptureType = WebCore::MediaCaptureType::MediaCaptureTypeEnvironment;

        if (![UIImagePickerController isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceRear])
            _mediaCaptureType = WebCore::MediaCaptureType::MediaCaptureTypeUser;

        return;
    }

    _mediaCaptureType = WebCore::MediaCaptureType::MediaCaptureTypeNone;
}

- (BOOL)_shouldMediaCaptureOpenMediaDevice
{
    if (_mediaCaptureType == WebCore::MediaCaptureType::MediaCaptureTypeNone || ![UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera])
        return NO;

    return YES;
}

- (void)_showCamera
{
    ASSERT([UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]);

    _cameraPicker = adoptNS([[UIImagePickerController alloc] init]);
    [_cameraPicker setSourceType:UIImagePickerControllerSourceTypeCamera];
    [_cameraPicker setMediaTypes:[self _mediaTypesForPickerSourceType:UIImagePickerControllerSourceTypeCamera]];
    [_cameraPicker setDelegate:self];
    [_cameraPicker presentationController].delegate = self;
    [_cameraPicker setAllowsEditing:NO];
    [_cameraPicker _setAllowsMultipleSelection:_allowMultipleFiles];
    [_cameraPicker _setRequiresPickingConfirmation:YES];
    [_cameraPicker _setShowsFileSizePicker:YES];

    if (_mediaCaptureType != WebCore::MediaCaptureType::MediaCaptureTypeNone)
        [_cameraPicker setCameraDevice:cameraDeviceForMediaCaptureType(_mediaCaptureType)];

    [self _presentFullscreenViewController:_cameraPicker.get() animated:YES];
}

- (void)_showPhotoPicker
{
#if HAVE(PHOTOS_UI)
    auto configuration = adoptNS([allocPHPickerConfigurationInstance() init]);
    [configuration setSelectionLimit:_allowMultipleFiles ? 0 : 1];
    [configuration setPreferredAssetRepresentationMode:PHPickerConfigurationAssetRepresentationModeCompatible];
    [configuration _setAllowsDownscaling:YES];

    _uploadFileManager = adoptNS([[NSFileManager alloc] init]);
    _uploadFileCoordinator = adoptNS([[NSFileCoordinator alloc] init]);

    _photoPicker = adoptNS([allocPHPickerViewControllerInstance() initWithConfiguration:configuration.get()]);
    [_photoPicker setDelegate:self];
    [_photoPicker presentationController].delegate = self;

    [self _presentFullscreenViewController:_photoPicker.get() animated:YES];
#endif // HAVE(PHOTOS_UI)
}

#pragma mark - Presenting View Controllers

- (void)_presentFullscreenViewController:(UIViewController *)viewController animated:(BOOL)animated
{
    [self _dismissDisplayAnimated:animated];

    _presentationViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:_view.getAutoreleased()];
    [_presentationViewController presentViewController:viewController animated:animated completion:nil];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:(UIPresentationController *)presentationController
{
    [self _cancel];
}

#pragma mark - UIDocumentPickerControllerDelegate implementation

static NSString *displayStringForDocumentsAtURLs(NSArray<NSURL *> *urls)
{
    auto urlsCount = urls.count;
    ASSERT(urlsCount);
    if (urlsCount == 1)
        return urls[0].lastPathComponent;

    return WebCore::multipleFileUploadText(urlsCount);
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urlsFromUIKit
{
    ASSERT(urlsFromUIKit.count);
    [self _dismissDisplayAnimated:YES];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr([retainedSelf = retainPtr(self), urlsFromUIKit = retainPtr(urlsFromUIKit)] () mutable {
        RetainPtr<NSMutableArray<NSURL *>> maybeMovedURLs = adoptNS([[NSMutableArray alloc] initWithCapacity:urlsFromUIKit.get().count]);
        for (NSURL *originalURL in urlsFromUIKit.get()) {
            auto [maybeMovedURL, temporaryURL] = [retainedSelf _copyToNewTemporaryDirectory:originalURL];

            if (maybeMovedURL)
                [maybeMovedURLs addObject:maybeMovedURL.get()];

            if (temporaryURL)
                retainedSelf->_temporaryUploadedFileURLs.append(temporaryURL);
        }

        [retainedSelf->_view _removeTemporaryDirectoriesWhenDeallocated:std::exchange(retainedSelf->_temporaryUploadedFileURLs, { })];
        RunLoop::main().dispatch([retainedSelf = WTFMove(retainedSelf), maybeMovedURLs = WTFMove(maybeMovedURLs)] {
            [retainedSelf _chooseFiles:maybeMovedURLs.get() displayString:displayStringForDocumentsAtURLs(maybeMovedURLs.get()) iconImage:WebKit::iconForFiles({ maybeMovedURLs.get()[0].absoluteString }).get()];
        });
    }).get());
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)documentPicker
{
    [self _dismissDisplayAnimated:YES];
    [self _cancel];
}

#pragma mark - PHPickerViewControllerDelegate implementation

#if HAVE(PHOTOS_UI)

- (void)picker:(PHPickerViewController *)picker didFinishPicking:(NSArray<PHPickerResult *> *)results
{
    [self _processPickerResults:results successBlock:^(NSArray<_WKFileUploadItem *> *items) {
        ensureOnMainRunLoop([self, strongSelf = retainPtr(self), items = retainPtr(items)] {
            [strongSelf->_view _removeTemporaryDirectoriesWhenDeallocated:std::exchange(strongSelf->_temporaryUploadedFileURLs, { })];
            [self _chooseMediaItems:items.get()];
        });
    } failureBlock:^{
        ensureOnMainRunLoop([self, strongSelf = retainPtr(self)] {
            [self _dismissDisplayAnimated:YES];
            [self _cancel];
        });
    }];
}

#endif // HAVE(PHOTOS_UI)

#pragma mark - UIImagePickerControllerDelegate implementation

- (BOOL)_willMultipleSelectionDelegateBeCalled
{
    // The multiple selection delegate will not be called when the UIImagePicker was not multiple selection.
    if (!_allowMultipleFiles)
        return NO;

    // The multiple selection delegate will not be called when we used the camera in the UIImagePicker.
    if (_usingCamera)
        return NO;

    return YES;
}

- (void)imagePickerController:(UIImagePickerController *)imagePicker didFinishPickingMediaWithInfo:(NSDictionary *)info
{
    // Sometimes both delegates get called, sometimes just one. Always let the
    // multiple selection delegate handle everything if it will get called.
    if ([self _willMultipleSelectionDelegateBeCalled])
        return;

    [self _processMediaInfoDictionaries:@[info]
        successBlock:^(NSArray<_WKFileUploadItem *> *items) {
            ASSERT([items count] == 1);
            ensureOnMainRunLoop([self, strongSelf = retainPtr(self), items = retainPtr(items)] {
                [self _chooseMediaItems:items.get()];
            });
        }
        failureBlock:^{
            ensureOnMainRunLoop([self, strongSelf = retainPtr(self)] {
                [self _dismissDisplayAnimated:YES];
                [self _cancel];
            });
        }
    ];
}

- (void)imagePickerController:(UIImagePickerController *)imagePicker didFinishPickingMultipleMediaWithInfo:(NSArray *)infos
{
    [self _processMediaInfoDictionaries:infos
        successBlock:^(NSArray<_WKFileUploadItem *> *items) {
#if ENABLE(TRANSCODE_UIIMAGEPICKERCONTROLLER_VIDEO)
            [self _uploadMediaItemsTranscodingVideo:items];
#else
            ensureOnMainRunLoop([self, strongSelf = retainPtr(self), items = retainPtr(items)] {
                [self _chooseMediaItems:items.get()];
            });
#endif
        }
        failureBlock:^{
            ensureOnMainRunLoop([self, strongSelf = retainPtr(self)] {
                [self _dismissDisplayAnimated:YES];
                [self _cancel];
            });
        }
    ];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController *)imagePicker
{
    [self _dismissDisplayAnimated:YES];
    [self _cancel];
}

#pragma mark - Process UIImagePicker results

- (void)_processMediaInfoDictionaries:(NSArray *)infos successBlock:(void (^)(NSArray<_WKFileUploadItem *> *processedResults))successBlock failureBlock:(void (^)(void))failureBlock
{
    [self _processMediaInfoDictionaries:infos atIndex:0 processedResults:[NSMutableArray array] successBlock:successBlock failureBlock:failureBlock];
}

#if HAVE(PHOTOS_UI)

- (void)_processPickerResults:(NSArray<PHPickerResult *> *)results successBlock:(void (^)(NSArray<_WKFileUploadItem *> *processedResults))successBlock failureBlock:(void (^)(void))failureBlock
{
    [self _processPickerResults:results atIndex:0 processedResults:[NSMutableArray array] successBlock:successBlock failureBlock:failureBlock];
}

- (void)_processPickerResults:(NSArray<PHPickerResult *> *)results atIndex:(NSUInteger)index processedResults:(NSMutableArray<_WKFileUploadItem *> *)processedResults successBlock:(void (^)(NSArray<_WKFileUploadItem *> *processedResults))successBlock failureBlock:(void (^)(void))failureBlock
{
    NSUInteger count = [results count];
    if (index == count) {
        successBlock(processedResults);
        return;
    }

    PHPickerResult *result = [results objectAtIndex:index];
    ASSERT(index < count);
    index++;

    auto uploadItemSuccessBlock = ^(_WKFileUploadItem *uploadItem) {
        [processedResults addObject:uploadItem];
        [self _processPickerResults:results atIndex:index processedResults:processedResults successBlock:successBlock failureBlock:failureBlock];
    };

    [self _uploadItemFromResult:result successBlock:uploadItemSuccessBlock failureBlock:failureBlock];
}

- (void)_uploadItemFromResult:(PHPickerResult *)result successBlock:(void (^)(_WKFileUploadItem *))successBlock failureBlock:(void (^)(void))failureBlock
{
    if (NSString *uti = firstUTIThatConformsTo(result.itemProvider.registeredTypeIdentifiers, UTTypeMovie)) {
        [result.itemProvider loadFileRepresentationForTypeIdentifier:uti completionHandler:^(NSURL *url, NSError *error) {
            if (error) {
                failureBlock();
                return;
            }

            if (![url isFileURL]) {
                LOG_ERROR("WKFileUploadPanel: Expected media URL to be a file path, it was not");
                ASSERT_NOT_REACHED();
                failureBlock();
                return;
            }

            auto [maybeMovedURL, temporaryURL] = [self _copyToNewTemporaryDirectory:url];
            self->_temporaryUploadedFileURLs.append(WTFMove(temporaryURL));

            successBlock(adoptNS([[_WKVideoFileUploadItem alloc] initWithFileURL:maybeMovedURL.get()]).get());
        }];

        return;
    }

    NSString *uti = firstUTIThatConformsTo(result.itemProvider.registeredTypeIdentifiers, UTTypeImage);

    if (!uti) {
        LOG_ERROR("WKFileUploadPanel: Unexpected media type. Expected image or video");
        ASSERT_NOT_REACHED();
        failureBlock();
        return;
    }

    [result.itemProvider loadFileRepresentationForTypeIdentifier:uti completionHandler:^(NSURL *url, NSError *error) {
        if (error) {
            failureBlock();
            return;
        }

        if (![url isFileURL]) {
            LOG_ERROR("WKFileUploadPanel: Expected media URL to be a file path, it was not");
            ASSERT_NOT_REACHED();
            failureBlock();
            return;
        }

        auto [maybeMovedURL, temporaryURL] = [self _copyToNewTemporaryDirectory:url];
        self->_temporaryUploadedFileURLs.append(WTFMove(temporaryURL));

        successBlock(adoptNS([[_WKImageFileUploadItem alloc] initWithFileURL:maybeMovedURL.get()]).get());
    }];
}

#endif // HAVE(PHOTOS_UI)

- (void)_processMediaInfoDictionaries:(NSArray *)infos atIndex:(NSUInteger)index processedResults:(NSMutableArray<_WKFileUploadItem *> *)processedResults successBlock:(void (^)(NSArray<_WKFileUploadItem *> *processedResults))successBlock failureBlock:(void (^)(void))failureBlock
{
    NSUInteger count = [infos count];
    if (index == count) {
        successBlock(processedResults);
        return;
    }

    NSDictionary *info = [infos objectAtIndex:index];
    ASSERT(index < count);
    index++;

    auto uploadItemSuccessBlock = ^(_WKFileUploadItem *uploadItem) {
        [processedResults addObject:uploadItem];
        [self _processMediaInfoDictionaries:infos atIndex:index processedResults:processedResults successBlock:successBlock failureBlock:failureBlock];
    };

    [self _uploadItemFromMediaInfo:info successBlock:uploadItemSuccessBlock failureBlock:failureBlock];
}

- (void)_uploadItemForImageData:(NSData *)imageData imageName:(NSString *)imageName successBlock:(void (^)(_WKFileUploadItem *))successBlock failureBlock:(void (^)(void))failureBlock
{
    ASSERT_ARG(imageData, imageData);
    ASSERT(!RunLoop::isMain());

    NSString * const kTemporaryDirectoryName = @"WKWebFileUpload";

    // Build temporary file path.
    NSString *temporaryDirectory = FileSystem::createTemporaryDirectory(kTemporaryDirectoryName);
    NSString *filePath = [temporaryDirectory stringByAppendingPathComponent:imageName];
    if (!filePath) {
        LOG_ERROR("WKFileUploadPanel: Failed to create temporary directory to save image");
        failureBlock();
        return;
    }

    // Save the image to the temporary file.
    NSError *error = nil;
    [imageData writeToFile:filePath options:NSDataWritingAtomic error:&error];
    if (error) {
        LOG_ERROR("WKFileUploadPanel: Error writing image data to temporary file: %@", error);
        failureBlock();
        return;
    }

    successBlock(adoptNS([[_WKImageFileUploadItem alloc] initWithFileURL:[NSURL fileURLWithPath:filePath isDirectory:NO]]).get());
}

- (void)_uploadItemForJPEGRepresentationOfImage:(UIImage *)image successBlock:(void (^)(_WKFileUploadItem *))successBlock failureBlock:(void (^)(void))failureBlock
{
    ASSERT_ARG(image, image);

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // FIXME: Different compression for different devices?
        // FIXME: Different compression for different UIImage sizes?
        // FIXME: Should EXIF data be maintained?
        const CGFloat compression = 0.8;
        NSData *jpeg = UIImageJPEGRepresentation(image, compression);
        if (!jpeg) {
            LOG_ERROR("WKFileUploadPanel: Failed to create JPEG representation for image");
            failureBlock();
            return;
        }

        // FIXME: Should we get the photo asset and get the actual filename for the photo instead of
        // naming each of the individual uploads image.jpg? This won't work for photos taken with
        // the camera, but would work for photos picked from the library.
        NSString * const kUploadImageName = @"image.jpg";
        [self _uploadItemForImageData:jpeg imageName:kUploadImageName successBlock:successBlock failureBlock:failureBlock];
    });
}

- (void)_uploadItemFromMediaInfo:(NSDictionary *)info successBlock:(void (^)(_WKFileUploadItem *))successBlock failureBlock:(void (^)(void))failureBlock
{
    NSString *mediaTypeUTI = [info objectForKey:UIImagePickerControllerMediaType];
    UTType *mediaType = [UTType typeWithIdentifier:mediaTypeUTI];

    // For videos from the existing library or camera, the media URL will give us a file path.
    if ([mediaType conformsToType:UTTypeMovie]) {
        NSURL *mediaURL = [info objectForKey:UIImagePickerControllerMediaURL];
        if (![mediaURL isFileURL]) {
            LOG_ERROR("WKFileUploadPanel: Expected media URL to be a file path, it was not");
            ASSERT_NOT_REACHED();
            failureBlock();
            return;
        }

        successBlock(adoptNS([[_WKVideoFileUploadItem alloc] initWithFileURL:mediaURL]).get());
        return;
    }

    if (![mediaType conformsToType:UTTypeImage]) {
        LOG_ERROR("WKFileUploadPanel: Unexpected media type. Expected image or video, got: %@", mediaType);
        ASSERT_NOT_REACHED();
        failureBlock();
        return;
    }

    if (NSURL *imageURL = info[UIImagePickerControllerImageURL]) {
        if (!imageURL.isFileURL) {
            LOG_ERROR("WKFileUploadPanel: Expected image URL to be a file path, it was not");
            ASSERT_NOT_REACHED();
            failureBlock();
            return;
        }

        successBlock(adoptNS([[_WKImageFileUploadItem alloc] initWithFileURL:imageURL]).get());
        return;
    }

    UIImage *originalImage = [info objectForKey:UIImagePickerControllerOriginalImage];
    if (!originalImage) {
        LOG_ERROR("WKFileUploadPanel: Expected image data but there was none");
        ASSERT_NOT_REACHED();
        failureBlock();
        return;
    }

    // Photos taken with the camera will not have an image URL. Fall back to a JPEG representation.
    [self _uploadItemForJPEGRepresentationOfImage:originalImage successBlock:successBlock failureBlock:failureBlock];
}

#if ENABLE(TRANSCODE_UIIMAGEPICKERCONTROLLER_VIDEO)

- (void)_uploadMediaItemsTranscodingVideo:(NSArray<_WKFileUploadItem *> *)items
{
    auto videoCount = [[items indexesOfObjectsPassingTest:^(_WKFileUploadItem *item, NSUInteger, BOOL*) {
        return item.isVideo;
    }] count];

    ensureOnMainRunLoop([self, strongSelf = retainPtr(self), items = retainPtr(items), videoCount] {
        if (!videoCount) {
            [self _chooseMediaItems:items.get()];
            return;
        }

        _mediaTranscoder = adoptNS([[WKFileUploadMediaTranscoder alloc] initWithItems:items.get() videoCount:videoCount completionHandler:[weakSelf = WeakObjCPtr<WKFileUploadPanel>(self)] (NSArray<_WKFileUploadItem *> *items) {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            [strongSelf _chooseMediaItems:items];
        }]);

        [_mediaTranscoder start];
    });
}

#endif

- (std::pair<RetainPtr<NSURL>, RetainPtr<NSURL>>)_copyToNewTemporaryDirectory:(NSURL *)originalURL
{
    NSError *error = nil;
    NSString *temporaryDirectory = FileSystem::createTemporaryDirectory(@"WKFileUploadPanel");
    if (!temporaryDirectory) {
        LOG_ERROR("WKFileUploadPanel: Failed to make temporary directory");
        return { originalURL, nil };
    }
    NSString *filePath = [temporaryDirectory stringByAppendingPathComponent:originalURL.lastPathComponent];
    auto destinationFileURL = adoptNS([[NSURL alloc] initFileURLWithPath:filePath isDirectory:NO]);

    __block std::pair<RetainPtr<NSURL>, RetainPtr<NSURL>> result;
    [_uploadFileCoordinator coordinateWritingItemAtURL:originalURL options:NSFileCoordinatorWritingForMoving error:&error byAccessor:^(NSURL *coordinatedOriginalURL) {
        NSError *error = nil;
        if (![_uploadFileManager moveItemAtURL:coordinatedOriginalURL toURL:destinationFileURL.get() error:&error] || error) {
            // If moving fails, keep the original URL and our 60 second time limit before it is deleted. We tried our best to extend it.
            result = { coordinatedOriginalURL, adoptNS([[NSURL alloc] initFileURLWithPath:temporaryDirectory isDirectory:YES]) };
        } else
            result = { destinationFileURL, adoptNS([[NSURL alloc] initFileURLWithPath:temporaryDirectory isDirectory:YES]) };
    }];
    if (error) {
        LOG_ERROR("WKFileUploadPanel: Failed to coordinate moving file with error %@", error);
        // If moving fails, keep the original URL and our 60 second time limit before it is deleted. We tried our best to extend it.
        return { originalURL, adoptNS([[NSURL alloc] initFileURLWithPath:temporaryDirectory isDirectory:YES]) };
    }

    return result;
}

- (BOOL)platformSupportsPickerViewController
{
#if PLATFORM(WATCHOS)
    return NO;
#else
    return YES;
#endif
}

@end

ALLOW_DEPRECATED_DECLARATIONS_END

#endif // PLATFORM(IOS_FAMILY)
