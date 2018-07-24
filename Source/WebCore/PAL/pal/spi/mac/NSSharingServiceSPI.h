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

#if USE(APPLE_INTERNAL_SDK)

#import <AppKit/NSSharingService_Private.h>

#else

#import <AppKit/NSSharingService.h>

typedef NS_ENUM(NSInteger, NSSharingServiceType) {
    NSSharingServiceTypeShare = 0,
    NSSharingServiceTypeViewer = 1,
    NSSharingServiceTypeEditor = 2
} NS_ENUM_AVAILABLE_MAC(10_10);

typedef NS_OPTIONS(NSUInteger, NSSharingServiceMask) {
    NSSharingServiceMaskShare = (1 << NSSharingServiceTypeShare),
    NSSharingServiceMaskViewer = (1 << NSSharingServiceTypeViewer),
    NSSharingServiceMaskEditor = (1 << NSSharingServiceTypeEditor),

    NSSharingServiceMaskAllTypes = 0xFFFF
} NS_ENUM_AVAILABLE_MAC(10_10);

@interface NSSharingService ()
+ (NSArray *)sharingServicesForItems:(NSArray *)items mask:(NSSharingServiceMask)maskForFiltering;
+ (void)getSharingServicesForItems:(NSArray *)items mask:(NSSharingServiceMask)maskForFiltering completion:(void(^)(NSArray *))completion;
@property (readonly) NSSharingServiceType type;
@property (readwrite, copy) NSString *name;
@end

#endif
