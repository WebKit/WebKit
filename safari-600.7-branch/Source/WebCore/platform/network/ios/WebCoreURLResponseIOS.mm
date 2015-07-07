/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebCoreURLResponseIOS.h"
#import "UTIUtilities.h"
#import "WebCoreSystemInterface.h"

#import "QuickLook.h"
#import "SoftLinking.h"
#import <MobileCoreServices/MobileCoreServices.h>

SOFT_LINK_FRAMEWORK(MobileCoreServices)

SOFT_LINK(MobileCoreServices, UTTypeCreatePreferredIdentifierForTag, CFStringRef, (CFStringRef inTagClass, CFStringRef inTag, CFStringRef inConformingToUTI), (inTagClass, inTag, inConformingToUTI))

SOFT_LINK_CONSTANT(MobileCoreServices, kUTTagClassFilenameExtension, CFStringRef)

#define kUTTagClassFilenameExtension getkUTTagClassFilenameExtension()

namespace WebCore {

void adjustMIMETypeIfNecessary(CFURLResponseRef cfResponse)
{
    RetainPtr<CFStringRef> mimeType = wkGetCFURLResponseMIMEType(cfResponse);
    RetainPtr<CFStringRef> updatedMIMEType = mimeType;
    if (!updatedMIMEType)
        updatedMIMEType = defaultMIMEType().createCFString();

#if USE(QUICK_LOOK)
    // We must ensure that the MIME type is correct, so that QuickLook's web plugin is called when needed.
    // We filter the basic MIME types so that we don't do unnecessary work in standard browsing situations.
    if (shouldUseQuickLookForMIMEType((NSString *)updatedMIMEType.get())) {
        RetainPtr<CFStringRef> suggestedFilename = adoptCF(wkCopyCFURLResponseSuggestedFilename(cfResponse));
        RetainPtr<CFStringRef> quickLookMIMEType = adoptCF((CFStringRef)QLTypeCopyBestMimeTypeForFileNameAndMimeType((NSString *)suggestedFilename.get(), (NSString *)mimeType.get()));
        if (!quickLookMIMEType) {
            CFURLRef url = wkGetCFURLResponseURL(cfResponse);
            NSURL *nsURL = (NSURL *)url;
            if ([nsURL isFileURL]) {
                RetainPtr<CFStringRef> extension = adoptCF(CFURLCopyPathExtension(url));
                if (extension) {
                    RetainPtr<CFStringRef> uti = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, extension.get(), nullptr));
                    quickLookMIMEType = mimeTypeFromUTITree(uti.get());
                }
            }
        }

        if (quickLookMIMEType)
            updatedMIMEType = quickLookMIMEType;
    }
#endif // USE(QUICK_LOOK)
    if (!mimeType || CFStringCompare(mimeType.get(), updatedMIMEType.get(), kCFCompareCaseInsensitive) != kCFCompareEqualTo)
        wkSetCFURLResponseMIMEType(cfResponse, updatedMIMEType.get());
}

} // namespace WebCore
