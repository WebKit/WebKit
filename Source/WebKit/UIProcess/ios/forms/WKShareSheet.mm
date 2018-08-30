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
#import "WKShareSheet.h"

#if PLATFORM(IOS) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
#import "UIKitSPI.h"
#import "WKContentViewInteraction.h"
#import "WebPageProxy.h"
#import <WebCore/ShareData.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

using namespace WebKit;

@implementation WKShareSheet {
    WeakObjCPtr<WKContentView> _view;

    RetainPtr<UIActivityViewController> _shareSheetViewController;
    RetainPtr<UIViewController> _presentationViewController;
    BOOL _shouldDismissWithAnimation;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;
    _view = view;
    _shouldDismissWithAnimation = YES;
    return self;
}

- (void)presentWithParameters:(const WebCore::ShareDataWithParsedURL &)data completionHandler:(WTF::CompletionHandler<void(bool)>&&)completionHandler
{
    auto shareDataArray = adoptNS([[NSMutableArray alloc] init]);
    
    if (!data.shareData.text.isEmpty())
        [shareDataArray addObject:(NSString *)data.shareData.text];
    
    if (!data.url.isNull()) {
        NSURL *url = (NSURL *)data.url;
        if (!data.shareData.title.isEmpty())
            url._title = data.shareData.title;
        [shareDataArray addObject:url];
    }
    
    if (!data.shareData.title.isEmpty() && ![shareDataArray count])
        [shareDataArray addObject:(NSString *)data.shareData.title];
    
    auto shareSheetController = adoptNS([[UIActivityViewController alloc] initWithActivityItems:shareDataArray.get() applicationActivities:nil]);

    auto completionHandlerBlock = BlockPtr<void((NSString *, BOOL completed, NSArray *, NSError *))>::fromCallable([completionHandler = WTFMove(completionHandler), shareSheet = self](NSString *, BOOL completed, NSArray *, NSError *) mutable {
        completionHandler(completed);
        [shareSheet dismiss];
    });
    
    shareSheetController.get().completionWithItemsHandler = completionHandlerBlock.get();
    _shareSheetViewController = WTFMove(shareSheetController);
    [self _presentFullscreenViewController:_shareSheetViewController.get() animated:YES];
}

- (void)_dispatchDidDismiss
{
    if ([_delegate respondsToSelector:@selector(shareSheetDidDismiss:)])
        [_delegate shareSheetDidDismiss:self];
}

- (void)_cancel
{
    [self _dispatchDidDismiss];
}

- (void)dismiss
{
    [[UIViewController _viewControllerForFullScreenPresentationFromView:_view.getAutoreleased()] dismissViewControllerAnimated:_shouldDismissWithAnimation completion:nil];
    _presentationViewController = nil;
    
    [self _cancel];
}

- (void)_dismissDisplayAnimated:(BOOL)animated
{
    if (_presentationViewController) {
        UIViewController *currentPresentedViewController = [_presentationViewController presentedViewController];
        if (currentPresentedViewController == self) {
            [currentPresentedViewController dismissViewControllerAnimated:animated completion:^{
                _presentationViewController = nil;
            }];
        }
    }
}

- (void)_presentFullscreenViewController:(UIViewController *)viewController animated:(BOOL)animated
{
    _presentationViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:_view.getAutoreleased()];
    [_presentationViewController presentViewController:viewController animated:animated completion:nil];
}

- (void)invokeShareSheetWithResolution:(BOOL)resolved
{
    _shouldDismissWithAnimation = NO;
    _shareSheetViewController.get().completionWithItemsHandler(nil, resolved, nil, nil);
}

@end
#endif // PLATFORM(IOS) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
