/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "WebCoreTextAttachment.h"

#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>

#import <pal/ios/UIKitSoftLink.h>

namespace WebCore {

static RetainPtr<CocoaImage>& webCoreTextAttachmentMissingPlatformImageIfExists()
{
    static NeverDestroyed<RetainPtr<CocoaImage>> missingImage;
    return missingImage.get();
}

CocoaImage *webCoreTextAttachmentMissingPlatformImage()
{
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        RetainPtr webCoreBundle = [NSBundle bundleWithIdentifier:@"com.apple.WebCore"];
#if PLATFORM(IOS_FAMILY)
        RetainPtr image = [PAL::getUIImageClassSingleton() imageNamed:@"missingImage" inBundle:webCoreBundle.get() compatibleWithTraitCollection:nil];
#else
        RetainPtr image = [webCoreBundle imageForResource:@"missingImage"];
#endif
        ASSERT_WITH_MESSAGE(!!image, "Unable to find missingImage.");
        webCoreTextAttachmentMissingPlatformImageIfExists() = WTFMove(image);
    });

    return webCoreTextAttachmentMissingPlatformImageIfExists().get();
}

bool isWebCoreTextAttachmentMissingPlatformImage(CocoaImage *image)
{
    return image && image == webCoreTextAttachmentMissingPlatformImageIfExists();
}

} // namespace WebCore
