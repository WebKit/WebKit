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

#import "SoftLinking.h"
#import <objc/runtime.h>

typedef struct __DDScanner DDScanner, *DDScannerRef;
typedef struct __DDScanQuery *DDScanQueryRef;
typedef struct __DDResult *DDResultRef;

typedef enum {
    DDScannerTypeStandard = 0,
} DDScannerType;

enum {
    DDScannerOptionStopAtFirstMatch = 1,
};
typedef CFIndex DDScannerOptions;

enum {
    DDScannerCopyResultsOptionsNone = 0,
    DDScannerCopyResultsOptionsNoOverlap = 1 << 0,
};
typedef CFIndex DDScannerCopyResultsOptions;

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectors)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectorsCore)

extern "C" {

SOFT_LINK(DataDetectorsCore, DDScannerCreate, DDScannerRef, (DDScannerType type, DDScannerOptions options, CFErrorRef* errorRef), (type, options, errorRef))
SOFT_LINK(DataDetectorsCore, DDScanQueryCreateFromString, DDScanQueryRef, (CFAllocatorRef allocator, CFStringRef string, CFRange range), (allocator, string, range))
SOFT_LINK(DataDetectorsCore, DDScannerScanQuery, DDScanQueryRef, (DDScannerRef scanner, DDScanQueryRef query), (scanner, query))
SOFT_LINK(DataDetectorsCore, DDScannerCopyResultsWithOptions, CFArrayRef, (DDScannerRef scanner, DDScannerCopyResultsOptions options), (scanner, options))
SOFT_LINK(DataDetectorsCore, DDResultGetRange, CFRange, (DDResultRef result), (result))

}

SOFT_LINK_CLASS(DataDetectors, DDActionContext)
SOFT_LINK_CLASS(DataDetectors, DDActionsManager)

@interface DDActionContext : NSObject <NSCopying, NSSecureCoding>

@property NSRect highlightFrame;
@property (retain) NSArray *allResults;
@property (retain) __attribute__((NSObject)) DDResultRef mainResult;
@property (copy) void (^completionHandler)(void);
@property (assign) BOOL forActionMenuContent;

- (DDActionContext *)contextForView:(NSView *)view altMode:(BOOL)altMode interactionStartedHandler:(void (^)(void))interactionStartedHandler interactionChangedHandler:(void (^)(void))interactionChangedHandler interactionStoppedHandler:(void (^)(void))interactionStoppedHandler;

@end

@interface DDActionsManager : NSObject

+ (DDActionsManager *)sharedManager;
- (NSArray *)menuItemsForResult:(DDResultRef)result actionContext:(DDActionContext *)context;
- (NSArray *)menuItemsForTargetURL:(NSString *)targetURL actionContext:(DDActionContext *)context;
- (void)requestBubbleClosureUnanchorOnFailure:(BOOL)unanchorOnFailure;

+ (BOOL)shouldUseActionsWithContext:(DDActionContext *)context;
+ (void)didUseActions;


@end
