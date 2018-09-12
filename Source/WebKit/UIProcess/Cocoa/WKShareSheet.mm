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

#if PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV) && WK_API_ENABLED

#import "WKContentViewInteraction.h"
#import "WebPageProxy.h"
#import <WebCore/ShareData.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS)
#import "UIKitSPI.h"
#else
#import "WKWebView.h"
#endif

using namespace WebKit;

@implementation WKShareSheet {
#if PLATFORM(MAC)
    WeakObjCPtr<WKWebView> _view;

    BOOL _isShowingSharingServicePicker;
    RetainPtr<NSSharingServicePicker> _sharingServicePickerForMenu;
    WTF::CompletionHandler<void(bool)> _completionHandler;
#else
    WeakObjCPtr<WKContentView> _view;
    
    RetainPtr<UIActivityViewController> _shareSheetViewController;
    RetainPtr<UIViewController> _presentationViewController;
    BOOL _shouldDismissWithAnimation;
#endif
}

#if PLATFORM(MAC)
- (instancetype)initWithView:(WKWebView *)view
#else
- (instancetype)initWithView:(WKContentView *)view
#endif
{
    if (!(self = [super init]))
        return nil;
    _view = view;
#if PLATFORM(IOS)
    _shouldDismissWithAnimation = YES;
#endif
    return self;
}

- (void)presentWithParameters:(const WebCore::ShareDataWithParsedURL &)data completionHandler:(WTF::CompletionHandler<void(bool)>&&)completionHandler
{
    auto shareDataArray = adoptNS([[NSMutableArray alloc] init]);
    
    if (!data.shareData.text.isEmpty())
        [shareDataArray addObject:(NSString *)data.shareData.text];
    
    if (!data.url.isNull()) {
        NSURL *url = (NSURL *)data.url;
#if PLATFORM(IOS)
        if (!data.shareData.title.isEmpty())
            url._title = data.shareData.title;
#endif
        [shareDataArray addObject:url];
    }
    
    if (!data.shareData.title.isEmpty() && ![shareDataArray count])
        [shareDataArray addObject:(NSString *)data.shareData.title];
    
#if PLATFORM(MAC)
    _sharingServicePickerForMenu = adoptNS([[NSSharingServicePicker alloc] initWithItems:shareDataArray.get()]);
    _sharingServicePickerForMenu.get().delegate = self;
    _completionHandler = WTFMove(completionHandler);
    
    NSPoint location = [NSEvent mouseLocation];
    NSRect mouseLocationRect = NSMakeRect(location.x, location.y, 1.0, 1.0);
    NSRect mouseLocationInWindow = [[_view window] convertRectFromScreen:mouseLocationRect];
    NSRect mouseLocationInView = [_view convertRect:mouseLocationInWindow fromView:nil];
    [_sharingServicePickerForMenu showRelativeToRect:mouseLocationInView ofView:_view.getAutoreleased() preferredEdge:NSMinYEdge];
#else
    auto shareSheetController = adoptNS([[UIActivityViewController alloc] initWithActivityItems:shareDataArray.get() applicationActivities:nil]);

    auto completionHandlerBlock = BlockPtr<void((NSString *, BOOL completed, NSArray *, NSError *))>::fromCallable([completionHandler = WTFMove(completionHandler), shareSheet = self](NSString *, BOOL completed, NSArray *, NSError *) mutable {
        completionHandler(completed);
        [shareSheet dismiss];
    });
    
    shareSheetController.get().completionWithItemsHandler = completionHandlerBlock.get();
    _shareSheetViewController = WTFMove(shareSheetController);
    [self _presentFullscreenViewController:_shareSheetViewController.get() animated:YES];
#endif
}

#if PLATFORM(MAC)
- (void)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker didChooseSharingService:(nullable NSSharingService *)service
{
    self->_completionHandler(!service);
    [self _dispatchDidDismiss];
}

#endif
- (void)_dispatchDidDismiss
{
    if ([_delegate respondsToSelector:@selector(shareSheetDidDismiss:)])
        [_delegate shareSheetDidDismiss:self];
}

- (void)_cancel
{
#if PLATFORM(IOS)
    [self _dispatchDidDismiss];
#endif
}

- (void)dismiss
{
#if PLATFORM(IOS)
    [[UIViewController _viewControllerForFullScreenPresentationFromView:_view.getAutoreleased()] dismissViewControllerAnimated:_shouldDismissWithAnimation completion:nil];
    _presentationViewController = nil;
    
    [self _cancel];
#endif
}

#if PLATFORM(IOS)

- (void)_dismissDisplayAnimated:(BOOL)animated
{
    if (_presentationViewController) {
        UIViewController *currentPresentedViewController = [_presentationViewController presentedViewController];
        if (currentPresentedViewController == _shareSheetViewController) {
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
#endif

- (void)invokeShareSheetWithResolution:(BOOL)resolved
{
#if PLATFORM(IOS)
    _shouldDismissWithAnimation = NO;
    _shareSheetViewController.get().completionWithItemsHandler(nil, resolved, nil, nil);
#else
    _completionHandler(resolved);
#endif
}

@end
#endif // !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
