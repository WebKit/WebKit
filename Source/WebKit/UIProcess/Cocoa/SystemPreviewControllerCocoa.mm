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
#import "SystemPreviewController.h"

#if PLATFORM(IOS) && USE(QUICK_LOOK)

#import "APIUIClient.h"
#import "WebPageProxy.h"

#import <QuickLook/QuickLook.h>
#import <UIKit/UIViewController.h>
#import <wtf/SoftLinking.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/SystemPreviewTypes.cpp>
#endif

SOFT_LINK_FRAMEWORK(QuickLook)
SOFT_LINK_CLASS(QuickLook, QLPreviewController);

@interface _WKPreviewControllerDataSource : NSObject <QLPreviewControllerDataSource> {
    WebCore::URL _url;
};
@end

@implementation _WKPreviewControllerDataSource

- (id)initWithURL:(const WebCore::URL&)url
{
    if (!(self = [super init]))
        return nil;

    _url = url;
    return self;
}

- (NSInteger)numberOfPreviewItemsInPreviewController:(QLPreviewController *)controller
{
    return 1;
}

- (id<QLPreviewItem>)previewController:(QLPreviewController *)controller previewItemAtIndex:(NSInteger)index
{
    return (NSURL*)self->_url;
}

@end

namespace WebKit {

bool SystemPreviewController::canPreview(const String& mimeType) const
{
#if USE(APPLE_INTERNAL_SDK)
    return canShowSystemPreviewForMIMEType(mimeType);
#else
    UNUSED_PARAM(mimeType);
    return false;
#endif
}

void SystemPreviewController::showPreview(const WebCore::URL& url)
{
    // FIXME: Make sure you can't show a preview if we're already previewing.

    UIViewController *presentingViewController = m_webPageProxy.uiClient().presentingViewController();

    if (!presentingViewController)
        return;

    RetainPtr<QLPreviewController> qlPreviewController = adoptNS([allocQLPreviewControllerInstance() init]);
    RetainPtr<_WKPreviewControllerDataSource> dataSource = adoptNS([[_WKPreviewControllerDataSource alloc] initWithURL:url]);

    [qlPreviewController setDataSource:dataSource.get()];

    [presentingViewController presentViewController:qlPreviewController.get() animated:YES completion:nullptr];
}

}

#endif
