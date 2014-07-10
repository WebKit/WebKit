/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "APIArray.h"
#import "APIData.h"
#import "APIString.h"
#import "WKContentViewInteraction.h"
#import "WKData.h"
#import "WKStringCF.h"
#import "WKURLCF.h"
#import "WebOpenPanelParameters.h"
#import "WebOpenPanelResultListenerProxy.h"
#import "WebPageProxy.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIImagePickerController_Private.h>
#import <UIKit/UIImage_Private.h>
#import <UIKit/UIViewController_Private.h>
#import <UIKit/UIWindow_Private.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SoftLinking.h>
#import <WebKit/WebNSFileManagerExtras.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

SOFT_LINK_FRAMEWORK(AVFoundation);
SOFT_LINK_CLASS(AVFoundation, AVAssetImageGenerator);
SOFT_LINK_CLASS(AVFoundation, AVURLAsset);
#define AVAssetImageGenerator_class getAVAssetImageGeneratorClass()
#define AVURLAsset_class getAVURLAssetClass()

SOFT_LINK_FRAMEWORK(CoreMedia);
SOFT_LINK_CONSTANT(CoreMedia, kCMTimeZero, CMTime);
#define kCMTimeZero getkCMTimeZero()


#pragma mark - _WKFileUploadItem

static CGRect squareCropRectForSize(CGSize size)
{
    CGFloat smallerSide = MIN(size.width, size.height);
    CGRect cropRect = CGRectMake(0, 0, smallerSide, smallerSide);

    if (size.width < size.height)
        cropRect.origin.y = rintf((size.height - smallerSide) / 2);
    else
        cropRect.origin.x = rintf((size.width - smallerSide) / 2);

    return cropRect;
}

static UIImage *squareImage(UIImage *image)
{
    if (!image)
        return nil;

    CGImageRef imageRef = [image CGImage];
    CGSize imageSize = CGSizeMake(CGImageGetWidth(imageRef), CGImageGetHeight(imageRef));
    if (imageSize.width == imageSize.height)
        return image;

    CGRect squareCropRect = squareCropRectForSize(imageSize);
    CGImageRef squareImageRef = CGImageCreateWithImageInRect(imageRef, squareCropRect);
    UIImage *squareImage = [[UIImage alloc] initWithCGImage:squareImageRef imageOrientation:[image imageOrientation]];
    CGImageRelease(squareImageRef);
    return [squareImage autorelease];
}

static UIImage *thumbnailSizedImageForImage(UIImage *image)
{
    UIImage *squaredImage = squareImage(image);
    if (!squaredImage)
        return nil;

    CGRect destRect = CGRectMake(0, 0, 100, 100);
    UIGraphicsBeginImageContext(destRect.size);
    CGContextSetInterpolationQuality(UIGraphicsGetCurrentContext(), kCGInterpolationHigh);
    [squaredImage drawInRect:destRect];
    UIImage *resultImage = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return resultImage;
}


@interface _WKFileUploadItem : NSObject
@property (nonatomic, readonly, getter=isVideo) BOOL video;
@property (nonatomic, readonly) NSURL *fileURL;
@property (nonatomic, readonly) UIImage *displayImage;
@end

@implementation _WKFileUploadItem

- (BOOL)isVideo
{
    ASSERT_NOT_REACHED();
    return NO;
}

- (NSURL *)fileURL
{
    ASSERT_NOT_REACHED();
    return nil;
}

- (UIImage *)displayImage
{
    ASSERT_NOT_REACHED();
    return nil;
}

@end


@interface _WKImageFileUploadItem : _WKFileUploadItem
- (instancetype)initWithFilePath:(NSString *)filePath originalImage:(UIImage *)originalImage;
@end

@implementation _WKImageFileUploadItem {
    RetainPtr<NSString> _filePath;
    RetainPtr<UIImage> _originalImage;
}

- (instancetype)initWithFilePath:(NSString *)filePath originalImage:(UIImage *)originalImage
{
    if (!(self = [super init]))
        return nil;
    _filePath = filePath;
    _originalImage = originalImage;
    return self;
}

- (BOOL)isVideo
{
    return NO;
}

- (NSURL *)fileURL
{
    return [NSURL fileURLWithPath:_filePath.get()];
}

- (UIImage *)displayImage
{
    return thumbnailSizedImageForImage(_originalImage.get());
}

@end


@interface _WKVideoFileUploadItem : _WKFileUploadItem
- (instancetype)initWithFilePath:(NSString *)filePath mediaURL:(NSURL *)mediaURL;
@end

@implementation _WKVideoFileUploadItem {
    RetainPtr<NSString> _filePath;
    RetainPtr<NSURL> _mediaURL;
}

- (instancetype)initWithFilePath:(NSString *)filePath mediaURL:(NSURL *)mediaURL
{
    if (!(self = [super init]))
        return nil;
    _filePath = filePath;
    _mediaURL = mediaURL;
    return self;
}

- (BOOL)isVideo
{
    return YES;
}

- (NSURL *)fileURL
{
    return [NSURL fileURLWithPath:_filePath.get()];
}

- (UIImage *)displayImage
{
    RetainPtr<AVURLAsset> asset = adoptNS([[AVURLAsset_class alloc] initWithURL:_mediaURL.get() options:nil]);
    RetainPtr<AVAssetImageGenerator> generator = adoptNS([[AVAssetImageGenerator_class alloc] initWithAsset:asset.get()]);
    [generator setAppliesPreferredTrackTransform:YES];

    NSError *error = nil;
    RetainPtr<CGImageRef> imageRef = adoptCF([generator copyCGImageAtTime:kCMTimeZero actualTime:nil error:&error]);
    if (error) {
        LOG_ERROR("_WKVideoFileUploadItem: Error creating image for video: %@", _mediaURL.get());
        return nil;
    }

    RetainPtr<UIImage> image = adoptNS([[UIImage alloc] initWithCGImage:imageRef.get()]);
    return thumbnailSizedImageForImage(image.get());
}

@end


#pragma mark - WKFileUploadPanel

@interface WKFileUploadPanel () <UIPopoverControllerDelegate, UINavigationControllerDelegate, UIImagePickerControllerDelegate>
@end

@implementation WKFileUploadPanel {
    WKContentView *_view;
    WebKit::WebOpenPanelResultListenerProxy* _listener;
    RetainPtr<NSArray> _mimeTypes;
    CGPoint _interactionPoint;
    BOOL _allowMultipleFiles;
    BOOL _usingCamera;
    RetainPtr<UIImagePickerController> _imagePicker;
    RetainPtr<UIAlertController> _actionSheetController;
    RetainPtr<UIViewController> _presentationViewController; // iPhone always. iPad for Fullscreen Camera.
    RetainPtr<UIPopoverController> _presentationPopover; // iPad for action sheet and Photo Library.
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

    Vector<RefPtr<API::Object>> urls;
    urls.reserveInitialCapacity(count);
    for (NSURL *fileURL in fileURLs)
        urls.uncheckedAppend(adoptRef(toImpl(WKURLCreateWithCFURL((CFURLRef)fileURL))));
    RefPtr<API::Array> fileURLsRef = API::Array::create(WTF::move(urls));

    NSData *jpeg = UIImageJPEGRepresentation(iconImage, 1.0);
    RefPtr<API::Data> iconImageDataRef = adoptRef(toImpl(WKDataCreate(reinterpret_cast<const unsigned char*>([jpeg bytes]), [jpeg length])));

    RefPtr<API::String> displayStringRef = adoptRef(toImpl(WKStringCreateWithCFString((CFStringRef)displayString)));

    _listener->chooseFiles(fileURLsRef.get(), displayStringRef.get(), iconImageDataRef.get());
    [self _dispatchDidDismiss];
}

#pragma mark - Present / Dismiss API

- (void)presentWithParameters:(WebKit::WebOpenPanelParameters*)parameters resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener
{
    ASSERT(!_listener);

    _listener = listener;
    _allowMultipleFiles = parameters->allowMultipleFiles();
    _interactionPoint = [_view lastInteractionLocation];

    RefPtr<API::Array> acceptMimeTypes = parameters->acceptMIMETypes();
    NSMutableArray *mimeTypes = [NSMutableArray arrayWithCapacity:acceptMimeTypes->size()];
    for (const auto& mimeType : acceptMimeTypes->elementsOfType<API::String>())
        [mimeTypes addObject:mimeType->string()];
    _mimeTypes = adoptNS([mimeTypes copy]);

    // If there is no camera or this is type=multiple, just show the image picker for the photo library.
    // Otherwise, show an action sheet for the user to choose between camera or library.
    if (_allowMultipleFiles || ![UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera])
        [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypePhotoLibrary];
    else
        [self _showMediaSourceSelectionSheet];
}

- (void)dismiss
{
    [self _dismissDisplayAnimated:NO];
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
        [_presentationViewController dismissViewControllerAnimated:animated completion:^{
            _presentationViewController = nil;
        }];
    }
}

#pragma mark - Action Sheet

static bool stringHasPrefixCaseInsensitive(NSString *str, NSString *prefix)
{
    NSRange range = [str rangeOfString:prefix options:(NSCaseInsensitiveSearch | NSAnchoredSearch)];
    return range.location != NSNotFound;
}

- (NSArray *)_mediaTypesForPickerSourceType:(UIImagePickerControllerSourceType)sourceType
{
    // The HTML5 spec mentions the literal "image/*" and "video/*" strings.
    // We support these and go a step further, if the MIME type starts with
    // "image/" or "video/" we adjust the picker's image or video filters.
    // So, "image/jpeg" would make the picker display all images types.
    NSMutableSet *mediaTypes = [NSMutableSet set];
    for (NSString *mimeType in _mimeTypes.get()) {
        if (stringHasPrefixCaseInsensitive(mimeType, @"image/"))
            [mediaTypes addObject:(NSString *)kUTTypeImage];
        else if (stringHasPrefixCaseInsensitive(mimeType, @"video/"))
            [mediaTypes addObject:(NSString *)kUTTypeMovie];
    }

    if ([mediaTypes count])
        return [mediaTypes allObjects];

    // Fallback to every supported media type if there is no filter.
    return [UIImagePickerController availableMediaTypesForSourceType:sourceType];
}

- (void)_showMediaSourceSelectionSheet
{
    NSString *existingString = WEB_UI_STRING_KEY("Photo Library", "Photo Library (file upload action sheet)", "File Upload alert sheet button string for choosing an existing media item from the Photo Library");
    NSString *cancelString = WEB_UI_STRING_KEY("Cancel", "Cancel (file upload action sheet)", "File Upload alert sheet button string to cancel");

    // Choose the appropriate string for the camera button.
    NSString *cameraString;
    NSArray *filteredMediaTypes = [self _mediaTypesForPickerSourceType:UIImagePickerControllerSourceTypeCamera];
    BOOL containsImageMediaType = [filteredMediaTypes containsObject:(NSString *)kUTTypeImage];
    BOOL containsVideoMediaType = [filteredMediaTypes containsObject:(NSString *)kUTTypeMovie];
    ASSERT(containsImageMediaType || containsVideoMediaType);
    if (containsImageMediaType && containsVideoMediaType)
        cameraString = WEB_UI_STRING_KEY("Take Photo or Video", "Take Photo or Video (file upload action sheet)", "File Upload alert sheet camera button string for taking photos or videos");
    else if (containsVideoMediaType)
        cameraString = WEB_UI_STRING_KEY("Take Video", "Take Video (file upload action sheet)", "File Upload alert sheet camera button string for taking only videos");
    else
        cameraString = WEB_UI_STRING_KEY("Take Photo", "Take Photo (file upload action sheet)", "File Upload alert sheet camera button string for taking only photos");

    _actionSheetController = [UIAlertController alertControllerWithTitle:nil message:nil preferredStyle:UIAlertControllerStyleActionSheet];

    UIAlertAction *cancelAction = [UIAlertAction actionWithTitle:cancelString style:UIAlertActionStyleCancel handler:^(UIAlertAction *){
        [self _cancel];
        // We handled cancel ourselves. Prevent the popover controller delegate from cancelling when the popover dismissed.
        [_presentationPopover setDelegate:nil];
    }];

    UIAlertAction *cameraAction = [UIAlertAction actionWithTitle:cameraString style:UIAlertActionStyleDefault handler:^(UIAlertAction *){
        _usingCamera = YES;
        [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypeCamera];
    }];

    UIAlertAction *photoLibraryAction = [UIAlertAction actionWithTitle:existingString style:UIAlertActionStyleDefault handler:^(UIAlertAction *){
        [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypePhotoLibrary];
    }];

    [_actionSheetController addAction:cancelAction];
    [_actionSheetController addAction:cameraAction];
    [_actionSheetController addAction:photoLibraryAction];

    if (UICurrentUserInterfaceIdiomIsPad())
        [self _presentPopoverWithContentViewController:_actionSheetController.get() animated:YES];
    else
        [self _presentFullscreenViewController:_actionSheetController.get() animated:YES];
}

#pragma mark - Image Picker

- (void)_showPhotoPickerWithSourceType:(UIImagePickerControllerSourceType)sourceType
{
    _imagePicker = adoptNS([[UIImagePickerController alloc] init]);
    [_imagePicker setDelegate:self];
    [_imagePicker setSourceType:sourceType];
    [_imagePicker setAllowsEditing:NO];
    [_imagePicker setModalPresentationStyle:UIModalPresentationFullScreen];
    [_imagePicker _setAllowsMultipleSelection:_allowMultipleFiles];
    [_imagePicker setMediaTypes:[self _mediaTypesForPickerSourceType:sourceType]];

    // Use a popover on the iPad if the source type is not the camera.
    // The camera will use a fullscreen, modal view controller.
    BOOL usePopover = UICurrentUserInterfaceIdiomIsPad() && sourceType != UIImagePickerControllerSourceTypeCamera;
    if (usePopover)
        [self _presentPopoverWithContentViewController:_imagePicker.get() animated:YES];
    else
        [self _presentFullscreenViewController:_imagePicker.get() animated:YES];
}

#pragma mark - Presenting View Controllers

- (void)_presentPopoverWithContentViewController:(UIViewController *)contentViewController animated:(BOOL)animated
{
    [self _dismissDisplayAnimated:animated];

    _presentationPopover = adoptNS([[UIPopoverController alloc] initWithContentViewController:contentViewController]);
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

- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
{
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

- (void)_processMediaInfoDictionaries:(NSArray *)infos successBlock:(void (^)(NSArray *processedResults, NSString *displayString))successBlock failureBlock:(void (^)())failureBlock
{
    [self _processMediaInfoDictionaries:infos atIndex:0 processedResults:[NSMutableArray array] processedImageCount:0 processedVideoCount:0 successBlock:successBlock failureBlock:failureBlock];
}

- (void)_processMediaInfoDictionaries:(NSArray *)infos atIndex:(NSUInteger)index processedResults:(NSMutableArray *)processedResults processedImageCount:(NSUInteger)processedImageCount processedVideoCount:(NSUInteger)processedVideoCount successBlock:(void (^)(NSArray *processedResults, NSString *displayString))successBlock failureBlock:(void (^)())failureBlock
{
    NSUInteger count = [infos count];
    if (index == count) {
        NSString *displayString = [self _displayStringForPhotos:processedImageCount videos:processedVideoCount];
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

- (void)_uploadItemFromMediaInfo:(NSDictionary *)info successBlock:(void (^)(_WKFileUploadItem *))successBlock failureBlock:(void (^)())failureBlock
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

        NSString *filePath = [mediaURL path];
        successBlock(adoptNS([[_WKVideoFileUploadItem alloc] initWithFilePath:filePath mediaURL:mediaURL]).get());
        return;
    }

    // For images, we create a temporary file path and use the original image.
    if (UTTypeConformsTo((CFStringRef)mediaType, kUTTypeImage)) {
        UIImage *originalImage = [info objectForKey:UIImagePickerControllerOriginalImage];
        if (!originalImage) {
            LOG_ERROR("WKFileUploadPanel: Expected image data but there was none");
            ASSERT_NOT_REACHED();
            failureBlock();
            return;
        }

        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            NSString * const kTemporaryDirectoryName = @"WKWebFileUpload";
            NSString * const kUploadImageName = @"image.jpg";

            // Build temporary file path.
            // FIXME: Should we get the ALAsset for the mediaURL and get the actual filename for the photo
            // instead of naming each of the individual uploads image.jpg? This won't work for photos
            // taken with the camera, but would work for photos picked from the library.
            NSFileManager *fileManager = [NSFileManager defaultManager];
            NSString *temporaryDirectory = [fileManager _webkit_createTemporaryDirectoryWithTemplatePrefix:kTemporaryDirectoryName];
            NSString *filePath = [temporaryDirectory stringByAppendingPathComponent:kUploadImageName];
            if (!filePath) {
                LOG_ERROR("WKFileUploadPanel: Failed to create temporary directory to save image");
                failureBlock();
                return;
            }

            // Compress to JPEG format.
            // FIXME: Different compression for different devices?
            // FIXME: Different compression for different UIImage sizes?
            // FIXME: Should EXIF data be maintained?
            const CGFloat compression = 0.8;
            NSData *jpeg = UIImageJPEGRepresentation(originalImage, compression);
            if (!jpeg) {
                LOG_ERROR("WKFileUploadPanel: Failed to create JPEG representation for image");
                failureBlock();
                return;
            }

            // Save the image to the temporary file.
            NSError *error = nil;
            [jpeg writeToFile:filePath options:NSDataWritingAtomic error:&error];
            if (error) {
                LOG_ERROR("WKFileUploadPanel: Error writing image data to temporary file: %@", error);
                failureBlock();
                return;
            }

            successBlock(adoptNS([[_WKImageFileUploadItem alloc] initWithFilePath:filePath originalImage:originalImage]).get());
        });
        return;
    }

    // Unknown media type.
    LOG_ERROR("WKFileUploadPanel: Unexpected media type. Expected image or video, got: %@", mediaType);
    failureBlock();
}

- (NSString *)_displayStringForPhotos:(NSUInteger)imageCount videos:(NSUInteger)videoCount
{
    if (!imageCount && !videoCount)
        return nil;

    NSString *title;
    NSString *countString;
    NSString *imageString;
    NSString *videoString;
    NSUInteger numberOfTypes = 2;

    RetainPtr<NSNumberFormatter> countFormatter = adoptNS([[NSNumberFormatter alloc] init]);
    [countFormatter setLocale:[NSLocale currentLocale]];
    [countFormatter setGeneratesDecimalNumbers:YES];
    [countFormatter setNumberStyle:NSNumberFormatterDecimalStyle];

    // Generate the individual counts for each type.
    switch (imageCount) {
    case 0:
        imageString = nil;
        --numberOfTypes;
        break;
    case 1:
        imageString = WEB_UI_STRING_KEY("1 Photo", "1 Photo (file upload on page label for one photo)", "File Upload single photo label");
        break;
    default:
        countString = [countFormatter stringFromNumber:@(imageCount)];
        imageString = [NSString stringWithFormat:WEB_UI_STRING_KEY("%@ Photos", "# Photos (file upload on page label for multiple photos)", "File Upload multiple photos label"), countString];
        break;
    }

    switch (videoCount) {
    case 0:
        videoString = nil;
        --numberOfTypes;
        break;
    case 1:
        videoString = WEB_UI_STRING_KEY("1 Video", "1 Video (file upload on page label for one video)", "File Upload single video label");
        break;
    default:
        countString = [countFormatter stringFromNumber:@(videoCount)];
        videoString = [NSString stringWithFormat:WEB_UI_STRING_KEY("%@ Videos", "# Videos (file upload on page label for multiple videos)", "File Upload multiple videos label"), countString];
        break;
    }

    // Combine into a single result string if needed.
    switch (numberOfTypes) {
    case 2:
        // FIXME: For localization we should build a complete string. We should have a localized string for each different combination.
        title = [NSString stringWithFormat:WEB_UI_STRING_KEY("%@ and %@", "# Photos and # Videos (file upload on page label for image and videos)", "File Upload images and videos label"), imageString, videoString];
        break;
    case 1:
        title = imageString ? imageString : videoString;
        break;
    default:
        ASSERT_NOT_REACHED();
        title = nil;
        break;
    }

    return [title lowercaseString];
}

@end

#endif // PLATFORM(IOS)
