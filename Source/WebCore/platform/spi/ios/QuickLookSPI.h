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

#import <Foundation/Foundation.h>

#if USE(APPLE_INTERNAL_SDK)

#import <QuickLook/QuickLookPrivate.h>

#else

@interface QLPreviewConverter : NSObject
@end

@interface QLPreviewConverter ()
- (NSURLRequest *)safeRequestForRequest:(NSURLRequest *)request;
- (id)initWithConnection:(NSURLConnection *)connection delegate:(id)delegate response:(NSURLResponse *)response options:(NSDictionary *)options;
- (id)initWithData:(NSData *)data name:(NSString *)name uti:(NSString *)uti options:(NSDictionary *)options;
- (void)appendDataArray:(NSArray *)dataArray;
- (void)finishConverting;
- (void)finishedAppendingData;
@property (readonly, nonatomic) NSString *previewFileName;
@property (readonly, nonatomic) NSString *previewUTI;
@property (readonly, nonatomic) NSURLRequest *previewRequest;
@property (readonly, nonatomic) NSURLResponse *previewResponse;
@end

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
#define kQLReturnPasswordProtected 1 << 2
#else
#define kQLReturnMask 0xaf00
#define kQLReturnPasswordProtected (kQLReturnMask | 20)
#endif

#endif

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
static_assert(kQLReturnPasswordProtected == 4, "kQLReturnPasswordProtected should equal 4.");
#else
static_assert(kQLReturnPasswordProtected == 44820, "kQLReturnPasswordProtected should equal 44820.");
#endif

WTF_EXTERN_C_BEGIN

NSSet *QLPreviewGetSupportedMIMETypes();
NSString *QLTypeCopyBestMimeTypeForFileNameAndMimeType(NSString *fileName, NSString *mimeType);
NSString *QLTypeCopyBestMimeTypeForURLAndMimeType(NSURL *, NSString *mimeType);
NSString *QLTypeCopyUTIForURLAndMimeType(NSURL *, NSString *mimeType);

WTF_EXTERN_C_END
