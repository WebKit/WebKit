/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#pragma once

#import <wtf/Platform.h>

#if ENABLE(DATA_DETECTION)

#import <pal/spi/cocoa/DataDetectorsCoreSPI.h>
#import <wtf/SoftLinking.h>

#if PLATFORM(MAC)

#if USE(APPLE_INTERNAL_SDK)

// Can't include DDAction.h because as of this writing it is a private header that includes a non-private header with an "" include.
#import <DataDetectors/DDActionContext.h>
#import <DataDetectors/DDActionsManager.h>
#import <DataDetectors/DDHighlightDrawing.h>

#else // !USE(APPLE_INTERNAL_SDK)

@interface DDActionContext : NSObject <NSCopying, NSSecureCoding>

@property NSRect highlightFrame;
@property (retain) NSArray *allResults;
@property (retain) __attribute__((NSObject)) DDResultRef mainResult;
@property (assign) BOOL altMode;
@property (assign) BOOL immediate;

@property (copy) NSArray *allowedActionUTIs;

- (DDActionContext *)contextForView:(NSView *)view altMode:(BOOL)altMode interactionStartedHandler:(void (^)(void))interactionStartedHandler interactionChangedHandler:(void (^)(void))interactionChangedHandler interactionStoppedHandler:(void (^)(void))interactionStoppedHandler;

@end

@interface DDActionsManager : NSObject

+ (DDActionsManager *)sharedManager;
- (NSArray *)menuItemsForResult:(DDResultRef)result actionContext:(DDActionContext *)context;
- (NSArray *)menuItemsForTargetURL:(NSString *)targetURL actionContext:(DDActionContext *)context;
- (void)requestBubbleClosureUnanchorOnFailure:(BOOL)unanchorOnFailure;

+ (BOOL)shouldUseActionsWithContext:(DDActionContext *)context;
+ (void)didUseActions;

- (BOOL)hasActionsForResult:(DDResultRef)result actionContext:(DDActionContext *)actionContext;

- (NSArray *)menuItemsForValue:(NSString *)value type:(CFStringRef)type service:(NSString *)service context:(DDActionContext *)context;

@end

enum {
    DDHighlightStyleBubbleNone = 0,
    DDHighlightStyleBubbleStandard = 1
};

enum {
    DDHighlightStyleIconNone = (0 << 16),
    DDHighlightStyleStandardIconArrow = (1 << 16)
};

enum {
    DDHighlightStyleButtonShowAlways = (1 << 24),
};

#endif // !USE(APPLE_INTERNAL_SDK)

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400

WTF_EXTERN_C_BEGIN
CFTypeID DDResultGetCFTypeID(void);
WTF_EXTERN_C_END

#endif

typedef struct __DDHighlight *DDHighlightRef;
typedef NSUInteger DDHighlightStyle;

@interface DDAction : NSObject

@property (readonly) NSString *actionUTI;

@end

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectors)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectorsCore)

SOFT_LINK_CLASS_OPTIONAL(DataDetectors, DDAction)
SOFT_LINK_CLASS_OPTIONAL(DataDetectors, DDActionContext)
SOFT_LINK_CLASS_OPTIONAL(DataDetectors, DDActionsManager)

SOFT_LINK_CONSTANT(DataDetectorsCore, DDBinderPhoneNumberKey, CFStringRef)

SOFT_LINK(DataDetectors, DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection, DDHighlightRef, (CFAllocatorRef allocator, CGRect* rects, CFIndex count, CGRect globalVisibleRect, DDHighlightStyle style, Boolean withArrow, NSWritingDirection writingDirection, Boolean endsWithEOL, Boolean flipped), (allocator, rects, count, globalVisibleRect, style, withArrow, writingDirection, endsWithEOL, flipped))
SOFT_LINK(DataDetectors, DDHighlightGetLayerWithContext, CGLayerRef, (DDHighlightRef highlight, CGContextRef context), (highlight, context))
SOFT_LINK(DataDetectors, DDHighlightGetBoundingRect, CGRect, (DDHighlightRef highlight), (highlight))
SOFT_LINK(DataDetectors, DDHighlightPointIsOnHighlight, Boolean, (DDHighlightRef highlight, CGPoint point, Boolean* onButton), (highlight, point, onButton))

#endif

#endif

