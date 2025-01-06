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

#if PLATFORM(IOS_FAMILY)

#if USE(APPLE_INTERNAL_SDK)

#import <FrontBoardServices/FBSDisplay.h>
#import <FrontBoardServices/FBSOpenApplicationService.h>

#else

@interface FBSDisplayConfiguration : NSObject
@property (nonatomic, copy, readonly) NSString *name;
@end

extern NSString *const FBSActivateForEventOptionTypeBackgroundContentFetching;
extern NSString *const FBSOpenApplicationOptionKeyActions;
extern NSString *const FBSOpenApplicationOptionKeyActivateForEvent;
extern NSString *const FBSOpenApplicationOptionKeyActivateSuspended;
extern NSString *const FBSOpenApplicationOptionKeyPayloadOptions;
extern NSString *const FBSOpenApplicationOptionKeyPayloadURL;

@interface FBSOpenApplicationOptions : NSObject <NSCopying>
+ (instancetype)optionsWithDictionary:(NSDictionary *)dictionary;
@end

@class BSProcessHandle;
typedef void(^FBSOpenApplicationCompletion)(BSProcessHandle *process, NSError *error);

@interface FBSOpenApplicationService : NSObject
- (void)openApplication:(NSString *)bundleID withOptions:(FBSOpenApplicationOptions *)options completion:(FBSOpenApplicationCompletion)completion;
@end

#endif // USE(APPLE_INTERNAL_SDK)

// Forward declare this for all SDKs to get the extern C linkage
WTF_EXTERN_C_BEGIN
extern FBSOpenApplicationService *SBSCreateOpenApplicationService(void);
WTF_EXTERN_C_END

#endif // PLATFORM(IOS_FAMILY)
