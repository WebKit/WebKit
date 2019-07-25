/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "WKImagePreviewViewController.h"

#if PLATFORM(IOS_FAMILY)

#import <UIKitSPI.h>
#import <WebCore/IntSize.h>
#import <_WKElementAction.h>

@implementation WKImagePreviewViewController {
    RetainPtr<CGImageRef> _image;
    RetainPtr<UIImageView> _imageView;
}

- (void)loadView
{
    [super loadView];
    self.view.backgroundColor = [UIColor whiteColor];
    [self.view addSubview:_imageView.get()];
}

- (id)initWithCGImage:(RetainPtr<CGImageRef>)image defaultActions:(RetainPtr<NSArray>)actions elementInfo:(RetainPtr<_WKActivatedElementInfo>)elementInfo
{
    self = [super initWithNibName:nil bundle:nil];
    if (!self)
        return nil;

    _image = image;

    _imageView = adoptNS([[UIImageView alloc] initWithFrame:CGRectZero]);
    _imageView.get().contentMode = UIViewContentModeScaleAspectFill;
    RetainPtr<UIImage> uiImage = adoptNS([[UIImage alloc] initWithCGImage:_image.get()]);
    [_imageView setImage:uiImage.get()];

    CGSize screenSize = [UIScreen mainScreen].bounds.size;
    CGSize imageSize = _scaleSizeWithinSize(CGSizeMake(CGImageGetWidth(_image.get()), CGImageGetHeight(_image.get())), screenSize);
    [_imageView setFrame:CGRectMake([_imageView frame].origin.x, [_imageView frame].origin.y, imageSize.width, imageSize.height)];
    [self setPreferredContentSize:imageSize];

    _imageActions = actions;
    _activatedElementInfo = elementInfo;

    return self;
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];

    [_imageView setFrame:self.view.bounds];
}

static CGSize _scaleSizeWithinSize(CGSize source, CGSize destination)
{
    CGSize size = destination;
    CGFloat sourceAspectRatio = (source.width / source.height);
    CGFloat destinationAspectRatio = (destination.width / destination.height);
    
    if (sourceAspectRatio > destinationAspectRatio) {
        size.width = destination.width;
        size.height = (source.height * (destination.width / source.width));
    } else if (sourceAspectRatio < destinationAspectRatio) {
        size.width = (source.width * (destination.height / source.height));
        size.height = destination.height;
    }
    
    return size;
}

#if HAVE(LINK_PREVIEW)
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray<UIPreviewAction *> *)previewActionItems
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    NSMutableArray<UIPreviewAction *> *previewActions = [NSMutableArray array];
    for (_WKElementAction *imageAction in _imageActions.get()) {
        UIPreviewAction *previewAction = [UIPreviewAction actionWithTitle:imageAction.title style:UIPreviewActionStyleDefault handler:^(UIPreviewAction *action, UIViewController *previewViewController) {
            [imageAction runActionWithElementInfo:_activatedElementInfo.get()];
        }];
        previewAction.image = [_WKElementAction imageForElementActionType:imageAction.type];

        [previewActions addObject:previewAction];
    }

    return previewActions;
}
#endif

@end

#endif
