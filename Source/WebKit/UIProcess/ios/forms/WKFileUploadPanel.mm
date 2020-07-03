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
#import "UIKitSPI.h"
#import "UserInterfaceIdiom.h"
#import "WKContentViewInteraction.h"
#import "WKData.h"
#import "WKStringCF.h"
#import "WKURLCF.h"
#import "WebIconUtilities.h"
#import "WebOpenPanelResultListenerProxy.h"
#import "WebPageProxy.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIDocumentPickerViewController.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/UTIUtilities.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/StringView.h>

using namespace WebKit;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

static inline UIImagePickerControllerCameraDevice cameraDeviceForMediaCaptureType(WebCore::MediaCaptureType mediaCaptureType)
{
    return mediaCaptureType == WebCore::MediaCaptureTypeUser ? UIImagePickerControllerCameraDeviceFront : UIImagePickerControllerCameraDeviceRear;
}

static bool arrayContainsUTIThatConformsTo(NSArray<NSString *> *typeIdentifiers, CFStringRef conformToUTI)
{
    for (NSString *uti in typeIdentifiers) {
        if (UTTypeConformsTo((__bridge CFStringRef)uti, conformToUTI))
            return true;
    }
    return false;
}

#pragma mark - _WKFileUploadItem

@interface _WKFileUploadItem : NSObject
- (instancetype)initWithFileURL:(NSURL *)fileURL;
@property (nonatomic, readonly, getter=isVideo) BOOL video;
@property (nonatomic, readonly) NSURL *fileURL;
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
    return iconForImageFile(self.fileURL);
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
    return iconForVideoFile(self.fileURL);
}

@end


#pragma mark - WKFileUploadPanel


@interface WKFileUploadPanel () <UIPopoverControllerDelegate, UINavigationControllerDelegate, UIImagePickerControllerDelegate, UIDocumentPickerDelegate
#if USE(UICONTEXTMENU)
, UIContextMenuInteractionDelegate
#endif
>
@end

@implementation WKFileUploadPanel {
    WeakObjCPtr<WKContentView> _view;
    RefPtr<WebKit::WebOpenPanelResultListenerProxy> _listener;
    RetainPtr<NSArray> _mimeTypes;
    CGPoint _interactionPoint;
    BOOL _allowMultipleFiles;
    BOOL _usingCamera;
    RetainPtr<UIImagePickerController> _imagePicker;
    RetainPtr<UIViewController> _presentationViewController; // iPhone always. iPad for Fullscreen Camera.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<UIPopoverController> _presentationPopover; // iPad for action sheet and Photo Library.
    BOOL _isPresentingSubMenu;
    ALLOW_DEPRECATED_DECLARATIONS_END
#if USE(UICONTEXTMENU)
    RetainPtr<UIContextMenuInteraction> _documentContextMenuInteraction;
#endif
    RetainPtr<UIDocumentPickerViewController> _documentPickerController;
    WebCore::MediaCaptureType _mediaCaptureType;
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
    [_imagePicker setDelegate:nil];
    [_presentationPopover setDelegate:nil];
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

    NSData *jpeg = UIImageJPEGRepresentation(iconImage, 1.0);
    RefPtr<API::Data> iconImageDataRef = adoptRef(toImpl(WKDataCreate(reinterpret_cast<const unsigned char*>([jpeg bytes]), [jpeg length])));

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
        String mimeType = WebCore::MIMETypeRegistry::mimeTypeForExtension(extension->stringView().substring(1).toString());
        if (!mimeType.isEmpty())
            [mimeTypes addObject:mimeType];
    }

    _mimeTypes = adoptNS([mimeTypes copy]);

    _mediaCaptureType = WebCore::MediaCaptureTypeNone;
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
        [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypeCamera];

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
    
    [_presentationPopover setDelegate:nil];
    _presentationPopover = nil;
    _presentationViewController = nil;

    [self _cancel];
}

- (void)_dismissDisplayAnimated:(BOOL)animated
{
    if (_presentationPopover) {
        [_presentationPopover dismissPopoverAnimated:animated];
        [_presentationPopover setDelegate:nil];
        _presentationPopover = nil;
    }

    if (_presentationViewController) {
        UIViewController *currentPresentedViewController = [_presentationViewController presentedViewController];
        if (currentPresentedViewController == self || currentPresentedViewController == _imagePicker.get()) {
            [currentPresentedViewController dismissViewControllerAnimated:animated completion:^{
                _presentationViewController = nil;
            }];
        }
    }
}

- (NSArray<NSString *> *)currentAvailableActionTitles
{
    NSMutableArray<NSString *> *actionTitles = [NSMutableArray array];

    NSArray *mediaTypes = UTIsForMIMETypes(_mimeTypes.get()).allObjects;
    BOOL allowsImageMediaType = !mediaTypes.count || arrayContainsUTIThatConformsTo(mediaTypes, kUTTypeImage);
    BOOL allowsVideoMediaType = !mediaTypes.count || arrayContainsUTIThatConformsTo(mediaTypes, kUTTypeMovie);
    if (allowsImageMediaType || allowsVideoMediaType) {
        [actionTitles addObject:@"Photo Library"];
        if (allowsImageMediaType && allowsVideoMediaType)
            [actionTitles addObject:@"Take Photo or Video"];
        else if (allowsVideoMediaType)
            [actionTitles addObject:@"Take Video"];
        else
            [actionTitles addObject:@"Take Photo"];
    }
    [actionTitles addObject:@"Browse"];
    return actionTitles;
}

#pragma mark - Media Types

static NSSet<NSString *> *UTIsForMIMETypes(NSArray *mimeTypes)
{
    NSMutableSet *mediaTypes = [NSMutableSet set];
    for (NSString *mimeType in mimeTypes) {
        if ([mimeType caseInsensitiveCompare:@"image/*"] == NSOrderedSame)
            [mediaTypes addObject:(__bridge NSString *)kUTTypeImage];
        else if ([mimeType caseInsensitiveCompare:@"video/*"] == NSOrderedSame)
            [mediaTypes addObject:(__bridge NSString *)kUTTypeMovie];
        else if ([mimeType caseInsensitiveCompare:@"audio/*"] == NSOrderedSame)
            // UIImagePickerController doesn't allow audio-only recording, so show the video
            // recorder for "audio/*".
            [mediaTypes addObject:(__bridge NSString *)kUTTypeMovie];
        else {
            auto uti = WebCore::UTIFromMIMEType(mimeType);
            if (!uti.isEmpty())
                [mediaTypes addObject:(__bridge NSString *)uti];
        }
    }
    return mediaTypes;
}

- (NSArray<NSString *> *)_mediaTypesForPickerSourceType:(UIImagePickerControllerSourceType)sourceType
{
    NSArray<NSString *> *availableMediaTypes = [UIImagePickerController availableMediaTypesForSourceType:sourceType];
    NSSet<NSString *> *acceptedMediaTypes = UTIsForMIMETypes(_mimeTypes.get());
    if (acceptedMediaTypes.count) {
        NSMutableArray<NSString *> *mediaTypes = [NSMutableArray array];
        for (NSString *availableMediaType in availableMediaTypes) {
            if ([acceptedMediaTypes containsObject:availableMediaType])
                [mediaTypes addObject:availableMediaType];
            else {
                for (NSString *acceptedMediaType in acceptedMediaTypes) {
                    if (UTTypeConformsTo((__bridge CFStringRef)acceptedMediaType, (__bridge CFStringRef)availableMediaType)) {
                        [mediaTypes addObject:availableMediaType];
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
    return availableMediaTypes;
}

#pragma mark - Source selection menu

- (NSString *)_browseFilesButtonLabel
{
    return WEB_UI_STRING_KEY("Browse", "Browse (file upload action sheet)", "File Upload alert sheet button string for choosing an existing file from the file brower");
}

- (NSString *)_photoLibraryButtonLabel
{
    return WEB_UI_STRING_KEY("Photo Library", "Photo Library (file upload action sheet)", "File Upload alert sheet button string for choosing an existing media item from the Photo Library");
}

- (NSString *)_cameraButtonLabelAllowingPhoto:(BOOL)allowPhoto allowingVideo:(BOOL)allowVideo
{
    ASSERT(allowPhoto || allowVideo);
    if (allowPhoto && allowVideo)
        return WEB_UI_STRING_KEY("Take Photo or Video", "Take Photo or Video (file upload action sheet)", "File Upload alert sheet camera button string for taking photos or videos");

    if (allowVideo)
        return WEB_UI_STRING_KEY("Take Video", "Take Video (file upload action sheet)", "File Upload alert sheet camera button string for taking only videos");

    return WEB_UI_STRING_KEY("Take Photo", "Take Photo (file upload action sheet)", "File Upload alert sheet camera button string for taking only photos");
}

#if USE(UICONTEXTMENU)

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForHighlightingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
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
        NSArray *mediaTypes = UTIsForMIMETypes(_mimeTypes.get()).allObjects;

        BOOL allowsImageMediaType = !mediaTypes.count || arrayContainsUTIThatConformsTo(mediaTypes, kUTTypeImage);
        BOOL allowsVideoMediaType = !mediaTypes.count || arrayContainsUTIThatConformsTo(mediaTypes, kUTTypeMovie);
        auto strongSelf = weakSelf.get();
        
        if (!strongSelf)
            return nil;
        
        self->_isPresentingSubMenu = NO;
        UIAction *browseAction = [UIAction actionWithTitle:[strongSelf _browseFilesButtonLabel] image:[UIImage systemImageNamed:@"ellipsis"] identifier:@"browse" handler:^(__kindof UIAction *action) {
            self->_isPresentingSubMenu = YES;
            [self showFilePickerMenu];
        }];

        UIAction *photoAction = [UIAction actionWithTitle:[strongSelf _photoLibraryButtonLabel] image:[UIImage systemImageNamed:@"rectangle.on.rectangle"] identifier:@"photo" handler:^(__kindof UIAction *action) {
            self->_isPresentingSubMenu = YES;
            [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypePhotoLibrary];
        }];

        if ([UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
            NSString *cameraString = [strongSelf _cameraButtonLabelAllowingPhoto:allowsImageMediaType allowingVideo:allowsVideoMediaType];
            UIAction *cameraAction = [UIAction actionWithTitle:cameraString image:[UIImage systemImageNamed:@"camera.fill"] identifier:@"camera" handler:^(__kindof UIAction *action) {
                _usingCamera = YES;
                self->_isPresentingSubMenu = YES;
                [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypeCamera];
            }];
            actions = @[photoAction, cameraAction, browseAction];
        } else
            actions = @[photoAction, browseAction];

        return [UIMenu menuWithTitle:@"" children:actions];
    };
    
    return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:actionMenuProvider];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id<UIContextMenuInteractionAnimating>)animator
{
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
        [_view _removeContextMenuViewIfPossible];
    }
}

- (void)ensureContextMenuInteraction
{
    if (!_documentContextMenuInteraction) {
        _documentContextMenuInteraction = adoptNS([[UIContextMenuInteraction alloc] initWithDelegate:self]);
        [_view addInteraction:_documentContextMenuInteraction.get()];
        self->_isPresentingSubMenu = NO;
    }
}

#endif

- (void)showFilePickerMenu
{
    NSArray *mediaTypes = UTIsForMIMETypes(_mimeTypes.get()).allObjects;
    NSArray *documentTypes = mediaTypes.count ? mediaTypes : @[(__bridge NSString *)kUTTypeItem];
    
    _documentPickerController = adoptNS([[UIDocumentPickerViewController alloc] initWithDocumentTypes:documentTypes inMode:UIDocumentPickerModeImport]);
    [_documentPickerController setAllowsMultipleSelection:_allowMultipleFiles];
    [_documentPickerController setDelegate:self];
    if ([_delegate respondsToSelector:@selector(fileUploadPanelDestinationIsManaged:)])
        [_documentPickerController _setIsContentManaged:[_delegate fileUploadPanelDestinationIsManaged:self]];
    [self _presentFullscreenViewController:_documentPickerController.get() animated:YES];
}

- (void)showDocumentPickerMenu
{
    // FIXME 49961589: Support picking media with UIImagePickerController
#if HAVE(UICONTEXTMENU_LOCATION)
    NSArray *mediaTypes = UTIsForMIMETypes(_mimeTypes.get()).allObjects;
    BOOL allowsImageMediaType = !mediaTypes.count || arrayContainsUTIThatConformsTo(mediaTypes, kUTTypeImage);
    BOOL allowsVideoMediaType = !mediaTypes.count || arrayContainsUTIThatConformsTo(mediaTypes, kUTTypeMovie);
    BOOL shouldPresentDocumentMenuViewController = allowsImageMediaType || allowsVideoMediaType;
    if (shouldPresentDocumentMenuViewController) {
        [self ensureContextMenuInteraction];
        [_documentContextMenuInteraction _presentMenuAtLocation:_interactionPoint];
    } else // Image and Video types are not accepted so bypass the menu and open the file picker directly.
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
            _mediaCaptureType = WebCore::MediaCaptureTypeEnvironment;
        
        if (![UIImagePickerController isCameraDeviceAvailable:UIImagePickerControllerCameraDeviceRear])
            _mediaCaptureType = WebCore::MediaCaptureTypeUser;
        
        return;
    }
    
    _mediaCaptureType = WebCore::MediaCaptureTypeNone;
}

- (BOOL)_shouldMediaCaptureOpenMediaDevice
{
    if (_mediaCaptureType == WebCore::MediaCaptureTypeNone || ![UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera])
        return NO;
    
    return YES;
}

- (void)_showPhotoPickerWithSourceType:(UIImagePickerControllerSourceType)sourceType
{
    ASSERT([UIImagePickerController isSourceTypeAvailable:sourceType]);

    _imagePicker = adoptNS([[UIImagePickerController alloc] init]);
    [_imagePicker setSourceType:sourceType];
    [_imagePicker setMediaTypes:[self _mediaTypesForPickerSourceType:sourceType]];
    [self _configureImagePicker:_imagePicker.get()];

    // Use a popover on the iPad if the source type is not the camera.
    // The camera will use a fullscreen, modal view controller.
    BOOL usePopover = currentUserInterfaceIdiomIsPad() && sourceType != UIImagePickerControllerSourceTypeCamera;
    if (usePopover)
        [self _presentPopoverWithContentViewController:_imagePicker.get() animated:YES];
    else
        [self _presentFullscreenViewController:_imagePicker.get() animated:YES];
}

- (void)_configureImagePicker:(UIImagePickerController *)imagePicker
{
    [imagePicker setDelegate:self];
    [imagePicker setAllowsEditing:NO];
    [imagePicker setModalPresentationStyle:UIModalPresentationFullScreen];
    [imagePicker _setAllowsMultipleSelection:_allowMultipleFiles];

    if (_mediaCaptureType != WebCore::MediaCaptureTypeNone)
        [imagePicker setCameraDevice:cameraDeviceForMediaCaptureType(_mediaCaptureType)];
}

#pragma mark - Presenting View Controllers

- (void)_presentMenuOptionForCurrentInterfaceIdiom:(UIViewController *)viewController
{
    if (currentUserInterfaceIdiomIsPad())
        [self _presentPopoverWithContentViewController:viewController animated:YES];
    else
        [self _presentFullscreenViewController:viewController animated:YES];
}

- (void)_presentPopoverWithContentViewController:(UIViewController *)contentViewController animated:(BOOL)animated
{
    [self _dismissDisplayAnimated:animated];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    _presentationPopover = adoptNS([[UIPopoverController alloc] initWithContentViewController:contentViewController]);
    ALLOW_DEPRECATED_DECLARATIONS_END
    [_presentationPopover setDelegate:self];
    [_presentationPopover presentPopoverFromRect:CGRectIntegral(CGRectMake(_interactionPoint.x, _interactionPoint.y, 1, 1)) inView:_view.getAutoreleased() permittedArrowDirections:UIPopoverArrowDirectionAny animated:animated];
}

- (void)_presentFullscreenViewController:(UIViewController *)viewController animated:(BOOL)animated
{
    [self _dismissDisplayAnimated:animated];

    _presentationViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:_view.getAutoreleased()];
    [_presentationViewController presentViewController:viewController animated:animated completion:nil];
}

#pragma mark - UIPopoverControllerDelegate

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
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

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
    ASSERT(urls.count);
    [self _dismissDisplayAnimated:YES];
    [self _chooseFiles:urls displayString:displayStringForDocumentsAtURLs(urls) iconImage:iconForFile(urls[0]).get()];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)documentPicker
{
    [self _dismissDisplayAnimated:YES];
    [self _cancel];
}

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

    [self _dismissDisplayAnimated:YES];

    [self _processMediaInfoDictionaries:@[info]
        successBlock:^(NSArray *processedResults, NSString *displayString) {
            ASSERT([processedResults count] == 1);
            _WKFileUploadItem *result = [processedResults objectAtIndex:0];
            dispatch_async(dispatch_get_main_queue(), ^{
                [self _chooseFiles:@[result.fileURL] displayString:displayString iconImage:result.displayImage.get()];
            });
        }
        failureBlock:^{
            dispatch_async(dispatch_get_main_queue(), ^{
                [self _cancel];
            });
        }
    ];
}

- (void)imagePickerController:(UIImagePickerController *)imagePicker didFinishPickingMultipleMediaWithInfo:(NSArray *)infos
{
    [self _dismissDisplayAnimated:YES];

    [self _processMediaInfoDictionaries:infos
        successBlock:^(NSArray *processedResults, NSString *displayString) {
            RetainPtr<UIImage> iconImage = nil;
            NSMutableArray *fileURLs = [NSMutableArray array];
            for (_WKFileUploadItem *result in processedResults) {
                NSURL *fileURL = result.fileURL;
                if (!fileURL)
                    continue;
                [fileURLs addObject:result.fileURL];
                if (!iconImage)
                    iconImage = result.displayImage;
            }

            dispatch_async(dispatch_get_main_queue(), ^{
                [self _chooseFiles:fileURLs displayString:displayString iconImage:iconImage.get()];
            });
        }
        failureBlock:^{
            dispatch_async(dispatch_get_main_queue(), ^{
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

- (void)_processMediaInfoDictionaries:(NSArray *)infos successBlock:(void (^)(NSArray *processedResults, NSString *displayString))successBlock failureBlock:(void (^)(void))failureBlock
{
    [self _processMediaInfoDictionaries:infos atIndex:0 processedResults:[NSMutableArray array] processedImageCount:0 processedVideoCount:0 successBlock:successBlock failureBlock:failureBlock];
}

- (void)_processMediaInfoDictionaries:(NSArray *)infos atIndex:(NSUInteger)index processedResults:(NSMutableArray *)processedResults processedImageCount:(NSUInteger)processedImageCount processedVideoCount:(NSUInteger)processedVideoCount successBlock:(void (^)(NSArray *processedResults, NSString *displayString))successBlock failureBlock:(void (^)(void))failureBlock
{
    NSUInteger count = [infos count];
    if (index == count) {
        NSString *displayString = (processedImageCount || processedVideoCount) ? [NSString localizedStringWithFormat:WEB_UI_NSSTRING(@"%lu photo(s) and %lu video(s)", "label next to file upload control; parameters are the number of photos and the number of videos"), (unsigned long)processedImageCount, (unsigned long)processedVideoCount] : nil;
        successBlock(processedResults, displayString);
        return;
    }

    NSDictionary *info = [infos objectAtIndex:index];
    ASSERT(index < count);
    index++;

    auto uploadItemSuccessBlock = ^(_WKFileUploadItem *uploadItem) {
        NSUInteger newProcessedVideoCount = processedVideoCount + (uploadItem.isVideo ? 1 : 0);
        NSUInteger newProcessedImageCount = processedImageCount + (uploadItem.isVideo ? 0 : 1);
        [processedResults addObject:uploadItem];
        [self _processMediaInfoDictionaries:infos atIndex:index processedResults:processedResults processedImageCount:newProcessedImageCount processedVideoCount:newProcessedVideoCount successBlock:successBlock failureBlock:failureBlock];
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
    NSString *mediaType = [info objectForKey:UIImagePickerControllerMediaType];

    // For videos from the existing library or camera, the media URL will give us a file path.
    if (UTTypeConformsTo((CFStringRef)mediaType, kUTTypeMovie)) {
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

    if (!UTTypeConformsTo((CFStringRef)mediaType, kUTTypeImage)) {
        LOG_ERROR("WKFileUploadPanel: Unexpected media type. Expected image or video, got: %@", mediaType);
        ASSERT_NOT_REACHED();
        failureBlock();
        return;
    }

#if PLATFORM(IOS_FAMILY)
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
#endif

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
