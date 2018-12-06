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

#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/ShareData.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import "WKContentViewInteraction.h"
#else
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#endif

#if PLATFORM(MAC)
@interface WKShareSheet () <NSSharingServiceDelegate, NSSharingServicePickerDelegate>
@end
#endif

@implementation WKShareSheet {
    WeakObjCPtr<WKWebView> _webView;
    WTF::CompletionHandler<void(bool)> _completionHandler;

#if PLATFORM(MAC)
    RetainPtr<NSSharingServicePicker> _sharingServicePicker;
#else
    RetainPtr<UIActivityViewController> _shareSheetViewController;
    RetainPtr<UIViewController> _presentationViewController;
#endif
}

- (instancetype)initWithView:(WKWebView *)view
{
    if (!(self = [super init]))
        return nil;

    _webView = view;

    return self;
}

- (void)presentWithParameters:(const WebCore::ShareDataWithParsedURL &)data completionHandler:(WTF::CompletionHandler<void(bool)>&&)completionHandler
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
        [self _didCompleteWithSuccess:*resolution];
        [self dismiss];
        return;
    }
    
    WKWebView *webView = _webView.getAutoreleased();
    
#if PLATFORM(MAC)
    _sharingServicePicker = adoptNS([[NSSharingServicePicker alloc] initWithItems:shareDataArray.get()]);
    _sharingServicePicker.get().delegate = self;
    
    NSPoint location = [NSEvent mouseLocation];
    NSRect mouseLocationRect = NSMakeRect(location.x, location.y, 1.0, 1.0);
    NSRect mouseLocationInWindow = [webView.window convertRectFromScreen:mouseLocationRect];
    NSRect mouseLocationInView = [webView convertRect:mouseLocationInWindow fromView:nil];
    [_sharingServicePicker showRelativeToRect:mouseLocationInView ofView:webView preferredEdge:NSMinYEdge];
#else
    _shareSheetViewController = adoptNS([[UIActivityViewController alloc] initWithActivityItems:shareDataArray.get() applicationActivities:nil]);
    [_shareSheetViewController setCompletionWithItemsHandler:^(NSString *, BOOL completed, NSArray *, NSError *) {
        [self _didCompleteWithSuccess:completed];
        [self dispatchDidDismiss];
    }];
    
    UIPopoverPresentationController *popoverController = [_shareSheetViewController popoverPresentationController];
    popoverController._centersPopoverIfSourceViewNotSet = YES;
    
    _presentationViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:webView];
    [_presentationViewController presentViewController:_shareSheetViewController.get() animated:YES completion:nil];
#endif
}

#if PLATFORM(MAC)
- (void)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker didChooseSharingService:(NSSharingService *)service
{
    if (service)
        return;

    [self _didCompleteWithSuccess:NO];
    [self dispatchDidDismiss];
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
    [self _didCompleteWithSuccess:NO];
    [self dispatchDidDismiss];
}

- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items
{
    [self _didCompleteWithSuccess:YES];
    [self dispatchDidDismiss];
}

#endif

- (void)_didCompleteWithSuccess:(BOOL)success
{
    auto completionHandler = WTFMove(_completionHandler);
    if (completionHandler)
        completionHandler(success);
}

- (void)dismiss
{
    // If programmatically dismissed without having already called the
    // completion handler, call it assuming failure.
    [self _didCompleteWithSuccess:NO];

#if PLATFORM(MAC)
    [_sharingServicePicker hide];
    [self dispatchDidDismiss];
#else
    if (!_presentationViewController)
        return;

    UIViewController *currentPresentedViewController = [_presentationViewController presentedViewController];
    if (currentPresentedViewController != _shareSheetViewController)
        return;

    [currentPresentedViewController dismissViewControllerAnimated:YES completion:^{
        [self dispatchDidDismiss];
        _presentationViewController = nil;
    }];
#endif
}

- (void)dispatchDidDismiss
{
    if ([_delegate respondsToSelector:@selector(shareSheetDidDismiss:)])
        [_delegate shareSheetDidDismiss:self];
}

@end

#endif // PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV) && WK_API_ENABLED
