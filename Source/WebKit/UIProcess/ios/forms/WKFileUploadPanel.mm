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
#import "WKContentViewInteraction.h"
#import "WKData.h"
#import "WKStringCF.h"
#import "WKURLCF.h"
#import "WebIconUtilities.h"
#import "WebOpenPanelResultListenerProxy.h"
#import "WebPageProxy.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

static inline UIImagePickerControllerCameraDevice cameraDeviceForMediaCaptureType(WebCore::MediaCaptureType mediaCaptureType)
{
    return mediaCaptureType == WebCore::MediaCaptureTypeUser ? UIImagePickerControllerCameraDeviceFront : UIImagePickerControllerCameraDeviceRear;
}

#pragma mark - Document picker icons

static inline UIImage *photoLibraryIcon()
{
    return _UIImageGetWebKitPhotoLibraryIcon();
}

static inline UIImage *cameraIcon()
{
    return _UIImageGetWebKitTakePhotoOrVideoIcon();
}

#pragma mark - _WKFileUploadItem

@interface _WKFileUploadItem : NSObject
- (instancetype)initWithFileURL:(NSURL *)fileURL;
@property (nonatomic, readonly, getter=isVideo) BOOL video;
@property (nonatomic, readonly) NSURL *fileURL;
@property (nonatomic, readonly) UIImage *displayImage;
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

- (UIImage *)displayImage
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

- (UIImage *)displayImage
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

- (UIImage *)displayImage
{
    return iconForVideoFile(self.fileURL);
}

@end


#pragma mark - WKFileUploadPanel

@interface WKFileUploadPanel () <UIPopoverControllerDelegate, UINavigationControllerDelegate, UIImagePickerControllerDelegate, UIDocumentPickerDelegate, UIDocumentMenuDelegate>
@end

@implementation WKFileUploadPanel {
    WKContentView *_view;
    RefPtr<WebKit::WebOpenPanelResultListenerProxy> _listener;
    RetainPtr<NSArray> _mimeTypes;
    CGPoint _interactionPoint;
    BOOL _allowMultipleFiles;
    BOOL _usingCamera;
    RetainPtr<UIImagePickerController> _imagePicker;
    RetainPtr<UIViewController> _presentationViewController; // iPhone always. iPad for Fullscreen Camera.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<UIPopoverController> _presentationPopover; // iPad for action sheet and Photo Library.
    ALLOW_DEPRECATED_DECLARATIONS_END
    RetainPtr<UIDocumentMenuViewController> _documentMenuController;
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
    [_documentMenuController setDelegate:nil];

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

    [self _showDocumentPickerMenu];
}

- (void)dismiss
{
    // Dismiss any view controller that is being presented. This works for all types of view controllers, popovers, etc.
    // If there is any kind of view controller presented on this view, it will be removed.
    
    [[UIViewController _viewControllerForFullScreenPresentationFromView:_view] dismissViewControllerAnimated:NO completion:nil];
    
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

#pragma mark - Media Types

static bool stringHasPrefixCaseInsensitive(NSString *str, NSString *prefix)
{
    NSRange range = [str rangeOfString:prefix options:(NSCaseInsensitiveSearch | NSAnchoredSearch)];
    return range.location != NSNotFound;
}

static NSArray *UTIsForMIMETypes(NSArray *mimeTypes)
{
    // The HTML5 spec mentions the literal "image/*" and "video/*" strings.
    // We support these and go a step further, if the MIME type starts with
    // "image/" or "video/" we adjust the picker's image or video filters.
    // So, "image/jpeg" would make the picker display all images types.
    NSMutableSet *mediaTypes = [NSMutableSet set];
    for (NSString *mimeType in mimeTypes) {
        // FIXME: We should support more MIME type -> UTI mappings. <http://webkit.org/b/142614>
        if (stringHasPrefixCaseInsensitive(mimeType, @"image/"))
            [mediaTypes addObject:(NSString *)kUTTypeImage];
        else if (stringHasPrefixCaseInsensitive(mimeType, @"video/"))
            [mediaTypes addObject:(NSString *)kUTTypeMovie];
    }

    return mediaTypes.allObjects;
}

- (NSArray *)_mediaTypesForPickerSourceType:(UIImagePickerControllerSourceType)sourceType
{
    NSArray *mediaTypes = UTIsForMIMETypes(_mimeTypes.get());
    if (mediaTypes.count)
        return mediaTypes;

    // Fallback to every supported media type if there is no filter.
    return [UIImagePickerController availableMediaTypesForSourceType:sourceType];
}

- (NSArray *)_documentPickerMenuMediaTypes
{
    NSArray *mediaTypes = UTIsForMIMETypes(_mimeTypes.get());
    if (mediaTypes.count)
        return mediaTypes;

    // Fallback to every supported media type if there is no filter.
    return @[@"public.item"];
}

#pragma mark - Source selection menu

- (NSString *)_photoLibraryButtonLabel
{
    return WEB_UI_STRING_KEY("Photo Library", "Photo Library (file upload action sheet)", "File Upload alert sheet button string for choosing an existing media item from the Photo Library");
}

- (NSString *)_cameraButtonLabel
{
    if (![UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera])
        return nil;

    // Choose the appropriate string for the camera button.
    NSArray *filteredMediaTypes = [self _mediaTypesForPickerSourceType:UIImagePickerControllerSourceTypeCamera];
    BOOL containsImageMediaType = [filteredMediaTypes containsObject:(NSString *)kUTTypeImage];
    BOOL containsVideoMediaType = [filteredMediaTypes containsObject:(NSString *)kUTTypeMovie];
    ASSERT(containsImageMediaType || containsVideoMediaType);
    if (containsImageMediaType && containsVideoMediaType)
        return WEB_UI_STRING_KEY("Take Photo or Video", "Take Photo or Video (file upload action sheet)", "File Upload alert sheet camera button string for taking photos or videos");

    if (containsVideoMediaType)
        return WEB_UI_STRING_KEY("Take Video", "Take Video (file upload action sheet)", "File Upload alert sheet camera button string for taking only videos");

    return WEB_UI_STRING_KEY("Take Photo", "Take Photo (file upload action sheet)", "File Upload alert sheet camera button string for taking only photos");
}

- (void)_showDocumentPickerMenu
{
    // FIXME: Support multiple file selection when implemented. <rdar://17177981>
    _documentMenuController = adoptNS([[UIDocumentMenuViewController alloc] _initIgnoringApplicationEntitlementForImportOfTypes:[self _documentPickerMenuMediaTypes]]);
    [_documentMenuController setDelegate:self];

    [_documentMenuController addOptionWithTitle:[self _photoLibraryButtonLabel] image:photoLibraryIcon() order:UIDocumentMenuOrderFirst handler:^{
        [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypePhotoLibrary];
    }];

    if ([UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
        if (NSString *cameraString = [self _cameraButtonLabel]) {
            [_documentMenuController addOptionWithTitle:cameraString image:cameraIcon() order:UIDocumentMenuOrderFirst handler:^{
                _usingCamera = YES;
                [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypeCamera];
            }];
        }
    }

    [self _presentMenuOptionForCurrentInterfaceIdiom:_documentMenuController.get()];
    // Clear out the view controller we just presented. Don't save a reference to the UIDocumentMenuViewController as it is self dismissing.
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
    [self _configureImagePicker:_imagePicker.get()];
    [_imagePicker setSourceType:sourceType];
    [_imagePicker setMediaTypes:[self _mediaTypesForPickerSourceType:sourceType]];
    
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
    [_presentationPopover presentPopoverFromRect:CGRectIntegral(CGRectMake(_interactionPoint.x, _interactionPoint.y, 1, 1)) inView:_view permittedArrowDirections:UIPopoverArrowDirectionAny animated:animated];
}

- (void)_presentFullscreenViewController:(UIViewController *)viewController animated:(BOOL)animated
{
    [self _dismissDisplayAnimated:animated];

    _presentationViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:_view];
    [_presentationViewController presentViewController:viewController animated:animated completion:nil];
}

#pragma mark - UIPopoverControllerDelegate

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
IGNORE_WARNINGS_END
{
    [self _cancel];
}

#pragma mark - UIDocumentMenuDelegate implementation

- (void)documentMenu:(UIDocumentMenuViewController *)documentMenu didPickDocumentPicker:(UIDocumentPickerViewController *)documentPicker
{
    documentPicker.delegate = self;
    [self _presentFullscreenViewController:documentPicker animated:YES];
}

- (void)documentMenuWasCancelled:(UIDocumentMenuViewController *)documentMenu
{
    [self _dismissDisplayAnimated:YES];
    [self _cancel];
}

#pragma mark - UIDocumentPickerControllerDelegate implementation

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)documentPicker:(UIDocumentPickerViewController *)documentPicker didPickDocumentAtURL:(NSURL *)url
IGNORE_WARNINGS_END
{
    [self _dismissDisplayAnimated:YES];
    [self _chooseFiles:@[url] displayString:url.lastPathComponent iconImage:iconForFile(url)];
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

    [self _processMediaInfoDictionaries:[NSArray arrayWithObject:info]
        successBlock:^(NSArray *processedResults, NSString *displayString) {
            ASSERT([processedResults count] == 1);
            _WKFileUploadItem *result = [processedResults objectAtIndex:0];
            dispatch_async(dispatch_get_main_queue(), ^{
                [self _chooseFiles:[NSArray arrayWithObject:result.fileURL] displayString:displayString iconImage:result.displayImage];
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
            UIImage *iconImage = nil;
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
                [self _chooseFiles:fileURLs displayString:displayString iconImage:iconImage];
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
    NSString *temporaryDirectory = WebCore::FileSystem::createTemporaryDirectory(kTemporaryDirectoryName);
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

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
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
