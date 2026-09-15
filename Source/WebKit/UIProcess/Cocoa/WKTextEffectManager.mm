/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "WKTextEffectManager.h"

#if ENABLE(WRITING_TOOLS_TEXT_EFFECTS)

#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/NativeImage.h>
#import <WebCore/TextAnimationTypes.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/WritingDirection.h>
#import <wtf/BlockPtr.h>
#import <wtf/WeakObjCPtr.h>

// The grammar-presentation animation API is spelled slightly differently on AppKit and UIKit; alias the
// platform types so the coordinator plumbing below can be written once.
#if PLATFORM(MAC)
using CocoaWritingToolsCoordinator = NSWritingToolsCoordinator;
using CocoaWritingToolsCoordinatorContext = NSWritingToolsCoordinatorContext;
using CocoaWritingToolsCoordinatorContextScope = NSWritingToolsCoordinatorContextScope;
using CocoaWritingToolsCoordinatorTextReplacementReason = NSWritingToolsCoordinatorTextReplacementReason;
using CocoaWritingToolsCoordinatorAnimationParameters = NSWritingToolsCoordinatorAnimationParameters;
using CocoaWritingToolsCoordinatorTextAnimation = NSWritingToolsCoordinatorTextAnimation;
using CocoaWritingToolsCoordinatorTextDecoration = NSWritingToolsCoordinatorTextDecoration;
using CocoaBezierPath = NSBezierPath;
using CocoaTextPreview = NSArray<NSTextPreview *>;
static constexpr auto cocoaWritingToolsCoordinatorTextAnimationIndicateGrammar = NSWritingToolsCoordinatorTextAnimationIndicateGrammar;
static constexpr auto cocoaWritingToolsCoordinatorTextDecorationNone = NSWritingToolsCoordinatorTextDecorationNone;
static constexpr auto cocoaWritingToolsCoordinatorTextDecorationGrammarUnderline = NSWritingToolsCoordinatorTextDecorationGrammarUnderline;
#else
using CocoaWritingToolsCoordinator = UIWritingToolsCoordinator;
using CocoaWritingToolsCoordinatorContext = UIWritingToolsCoordinatorContext;
using CocoaWritingToolsCoordinatorContextScope = UIWritingToolsCoordinatorContextScope;
using CocoaWritingToolsCoordinatorTextReplacementReason = UIWritingToolsCoordinatorTextReplacementReason;
using CocoaWritingToolsCoordinatorAnimationParameters = UIWritingToolsCoordinatorAnimationParameters;
using CocoaWritingToolsCoordinatorTextAnimation = UIWritingToolsCoordinatorTextAnimation;
using CocoaWritingToolsCoordinatorTextDecoration = UIWritingToolsCoordinatorTextDecoration;
using CocoaBezierPath = UIBezierPath;
using CocoaTextPreview = UITargetedPreview;
static constexpr auto cocoaWritingToolsCoordinatorTextAnimationIndicateGrammar = UIWritingToolsCoordinatorTextAnimationIndicateGrammar;
static constexpr auto cocoaWritingToolsCoordinatorTextDecorationNone = UIWritingToolsCoordinatorTextDecorationNone;
static constexpr auto cocoaWritingToolsCoordinatorTextDecorationGrammarUnderline = UIWritingToolsCoordinatorTextDecorationGrammarUnderline;
#endif

enum class UnderlyingTextVisibility : bool { Hidden, Visible };

static NSWritingDirection toNSWritingDirection(WebCore::WritingDirection editorWritingDirection)
{
    return editorWritingDirection == WebCore::WritingDirection::RightToLeft ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
}

// The indicator's rects are in root view coordinates, which is also the coordinate space of the coordinator's
// effect container view, so no conversion is needed.
template<typename Callback>
static void forEachTextPreviewImage(const RefPtr<WebCore::TextIndicator>& textIndicator, NOESCAPE Callback&& callback)
{
    if (!textIndicator)
        return;

    RefPtr snapshot = textIndicator->contentImage();
    if (!snapshot)
        return;

    RefPtr snapshotImage = snapshot->nativeImage();
    if (!snapshotImage)
        return;

    RetainPtr snapshotPlatformImage = snapshotImage->platformImage();
    if (!snapshotPlatformImage)
        return;

    CGRect boundingRectInRootViewCoordinates = textIndicator->textBoundingRectInRootViewCoordinates();

    for (auto textRectInSnapshotCoordinates : textIndicator->textRectsInBoundingRectCoordinates()) {
        CGRect presentationFrame = CGRectOffset(textRectInSnapshotCoordinates, boundingRectInRootViewCoordinates.origin.x, boundingRectInRootViewCoordinates.origin.y);
        textRectInSnapshotCoordinates.scale(textIndicator->contentImageScaleFactor());
        callback(adoptCF(CGImageCreateWithImageInRect(snapshotPlatformImage.get(), textRectInSnapshotCoordinates)).get(), presentationFrame);
    }
}

#if PLATFORM(MAC)

static RetainPtr<CocoaTextPreview> textPreviewFromIndicator(const RefPtr<WebCore::TextIndicator>& textIndicator, NSView *)
{
    RetainPtr previews = adoptNS([[NSMutableArray alloc] init]);
    forEachTextPreviewImage(textIndicator, [&](CGImageRef image, CGRect presentationFrame) {
        [previews addObject:adoptNS([[NSTextPreview alloc] initWithSnapshotImage:image presentationFrame:presentationFrame]).get()];
    });

    if (![previews count])
        return nil;

    return previews;
}

#else

static RetainPtr<CocoaTextPreview> textPreviewFromIndicator(const RefPtr<WebCore::TextIndicator>& textIndicator, UIView *containerView)
{
    if (!containerView)
        return nil;

    RetainPtr previewView = adoptNS([[UIView alloc] init]);
    CGRect boundingFrame = CGRectNull;

    forEachTextPreviewImage(textIndicator, [&](CGImageRef image, CGRect presentationFrame) {
        RetainPtr imageView = adoptNS([[UIImageView alloc] initWithImage:adoptNS([[UIImage alloc] initWithCGImage:image]).get()]);
        [imageView setFrame:presentationFrame];
        [previewView addSubview:imageView.get()];
        boundingFrame = CGRectUnion(boundingFrame, presentationFrame);
    });

    if (CGRectIsNull(boundingFrame))
        return nil;

    // A targeted preview positions a single view by its center, so the image views have to be laid out relative
    // to the preview view's own bounds rather than to the container.
    [previewView setFrame:boundingFrame];
    for (UIView *imageView in [previewView subviews])
        [imageView setFrame:CGRectOffset([imageView frame], -boundingFrame.origin.x, -boundingFrame.origin.y)];

    RetainPtr parameters = adoptNS([[UIPreviewParameters alloc] init]);
    RetainPtr target = adoptNS([[UIPreviewTarget alloc] initWithContainer:containerView center:CGPointMake(CGRectGetMidX(boundingFrame), CGRectGetMidY(boundingFrame))]);
    return adoptNS([[UITargetedPreview alloc] initWithView:previewView.get() parameters:parameters.get() target:target.get()]);
}

#endif

@interface WKTextEffectManager () <
#if PLATFORM(MAC)
    NSWritingToolsCoordinatorDelegate
#else
    UIWritingToolsCoordinatorDelegate
#endif
>
@end

@implementation WKTextEffectManager {
    WeakObjCPtr<WKWebView> _webView;
    RetainPtr<CocoaWritingToolsCoordinator> _writingToolsCoordinator;
    RetainPtr<NSMutableDictionary<NSUUID *, CocoaWritingToolsCoordinatorContext *>> _effectIDToContext;
    RetainPtr<NSMutableDictionary<NSUUID *, NSUUID *>> _effectIDToAnimationID;
    RetainPtr<NSMutableDictionary<NSUUID *, NSUUID *>> _contextIDToEffectID;
}

- (instancetype)initWithWebView:(WKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    _writingToolsCoordinator = adoptNS([[CocoaWritingToolsCoordinator alloc] initWithDelegate:self]);
    _effectIDToContext = adoptNS([[NSMutableDictionary alloc] init]);
    _effectIDToAnimationID = adoptNS([[NSMutableDictionary alloc] init]);
    _contextIDToEffectID = adoptNS([[NSMutableDictionary alloc] init]);

#if PLATFORM(MAC)
    [webView setWritingToolsCoordinator:_writingToolsCoordinator.get()];
#else
    RetainPtr contentView = [self _effectContainerView];
    [_writingToolsCoordinator setEffectContainerView:contentView.get()];
    [contentView addInteraction:_writingToolsCoordinator.get()];
#endif

    return self;
}

- (void)dealloc
{
#if PLATFORM(MAC)
    RetainPtr webView = _webView.get();
    if ([webView writingToolsCoordinator] == _writingToolsCoordinator.get())
        [webView setWritingToolsCoordinator:nil];
#else
    [[self _effectContainerView] removeInteraction:_writingToolsCoordinator.get()];
#endif

    [super dealloc];
}

#if PLATFORM(IOS_FAMILY)
- (UIView *)_effectContainerView
{
    RetainPtr webView = _webView.get();
    return webView ? webView->_contentView.get() : nil;
}
#endif

- (void)addTextEffectForID:(NSUUID *)uuid withData:(const WebCore::TextEffectData&)data
{
    // The coordinator identifies an animation by (context, range). The grammar-indicate animation does not consult
    // the context's text -- that only happens for NSWritingDirectionNatural, which is never passed here -- so an
    // empty context is enough; its auto-assigned identifier is what maps back to the effect UUID.
    NSRange range = NSMakeRange(0, 0);
    RetainPtr attributedString = adoptNS([[NSAttributedString alloc] initWithString:@""]);
    RetainPtr context = adoptNS([[CocoaWritingToolsCoordinatorContext alloc] initWithAttributedString:attributedString.get() range:range]);

    // The prepare and preview delegate callbacks can be invoked from within startTextAnimation, so the context has
    // to be resolvable before the animation starts.
    [_effectIDToContext setObject:context.get() forKey:uuid];
    [_contextIDToEffectID setObject:uuid forKey:[context identifier]];

    RetainPtr animationID = [_writingToolsCoordinator startTextAnimation:cocoaWritingToolsCoordinatorTextAnimationIndicateGrammar forRange:range inContext:context.get() writingDirection:toNSWritingDirection(data.writingDirection)];
    if (!animationID) {
        [self forgetEffectForID:uuid];
        return;
    }

    [_effectIDToAnimationID setObject:animationID.get() forKey:uuid];
}

- (void)removeTextEffectForID:(NSUUID *)uuid
{
    if (RetainPtr animationID = [_effectIDToAnimationID objectForKey:uuid])
        [_writingToolsCoordinator cancelTextAnimationsWithIdentifiers:@[animationID.get()]];

    [self forgetEffectForID:uuid];
}

- (void)removeAllTextEffects
{
    [_writingToolsCoordinator stopWritingTools];
    [_effectIDToContext removeAllObjects];
    [_effectIDToAnimationID removeAllObjects];
    [_contextIDToEffectID removeAllObjects];
}

- (void)forgetEffectForID:(NSUUID *)uuid
{
    if (RetainPtr context = [_effectIDToContext objectForKey:uuid])
        [_contextIDToEffectID removeObjectForKey:[context identifier]];
    [_effectIDToAnimationID removeObjectForKey:uuid];
    [_effectIDToContext removeObjectForKey:uuid];
}

- (NSUUID *)effectIDForContext:(CocoaWritingToolsCoordinatorContext *)context
{
    return [_contextIDToEffectID objectForKey:[context identifier]];
}

- (void)setUnderlyingTextVisibility:(UnderlyingTextVisibility)visibility forEffectID:(NSUUID *)uuid completion:(void(^)(void))completionHandler
{
    RetainPtr webView = _webView.get();
    if (!webView)
        return completionHandler();

    auto textEffectID = WTF::UUID::fromNSUUID(uuid);
    if (!textEffectID)
        return completionHandler();

    [webView _page]->updateUnderlyingTextVisibilityForTextEffectID(*textEffectID, visibility == UnderlyingTextVisibility::Visible, [completionHandler = makeBlockPtr(completionHandler)] {
        if (completionHandler)
            completionHandler();
    });
}

- (void)previewForContext:(CocoaWritingToolsCoordinatorContext *)context textDecoration:(CocoaWritingToolsCoordinatorTextDecoration)textDecoration completion:(void(^)(CocoaTextPreview *))completion
{
    RetainPtr webView = _webView.get();
    NSUUID *effectID = [self effectIDForContext:context];
    if (!webView || !effectID)
        return completion(nil);

    auto textEffectID = WTF::UUID::fromNSUUID(effectID);
    if (!textEffectID)
        return completion(nil);

    auto previewFromIndicator = [protectedSelf = retainPtr(self), completion = makeBlockPtr(completion)](RefPtr<WebCore::TextIndicator>&& textIndicator) mutable {
#if PLATFORM(MAC)
        RetainPtr containerView = protectedSelf->_webView.get();
#else
        RetainPtr containerView = [protectedSelf _effectContainerView];
#endif
        completion(textPreviewFromIndicator(textIndicator, containerView.get()).get());
    };

    if (textDecoration == cocoaWritingToolsCoordinatorTextDecorationGrammarUnderline)
        [webView _page]->decorationIndicatorForTextEffectID(*textEffectID, WTF::move(previewFromIndicator));
    else
        [webView _page]->textIndicatorForTextEffectID(*textEffectID, WTF::move(previewFromIndicator));
}

#pragma mark Writing Tools coordinator delegate

- (void)writingToolsCoordinator:(CocoaWritingToolsCoordinator *)writingToolsCoordinator prepareForTextAnimation:(CocoaWritingToolsCoordinatorTextAnimation)textAnimation forRange:(NSRange)range inContext:(CocoaWritingToolsCoordinatorContext *)context completion:(void(^)(void))completion
{
    NSUUID *effectID = [self effectIDForContext:context];
    if (!effectID)
        return completion();

    [self setUnderlyingTextVisibility:UnderlyingTextVisibility::Hidden forEffectID:effectID completion:completion];
}

- (void)writingToolsCoordinator:(CocoaWritingToolsCoordinator *)writingToolsCoordinator finishTextAnimation:(CocoaWritingToolsCoordinatorTextAnimation)textAnimation forRange:(NSRange)range inContext:(CocoaWritingToolsCoordinatorContext *)context completion:(void(^)(void))completion
{
    NSUUID *effectID = [self effectIDForContext:context];
    if (!effectID)
        return completion();

    [self setUnderlyingTextVisibility:UnderlyingTextVisibility::Visible forEffectID:effectID completion:completion];
    [self forgetEffectForID:effectID];
}

- (void)writingToolsCoordinator:(CocoaWritingToolsCoordinator *)writingToolsCoordinator requestsPreviewForTextAnimation:(CocoaWritingToolsCoordinatorTextAnimation)textAnimation ofRange:(NSRange)range inContext:(CocoaWritingToolsCoordinatorContext *)context completion:(void(^)(CocoaTextPreview *))completion
{
    [self previewForContext:context textDecoration:cocoaWritingToolsCoordinatorTextDecorationNone completion:completion];
}

- (void)writingToolsCoordinator:(CocoaWritingToolsCoordinator *)writingToolsCoordinator requestsPreviewForTextAnimation:(CocoaWritingToolsCoordinatorTextAnimation)textAnimation ofRange:(NSRange)range inContext:(CocoaWritingToolsCoordinatorContext *)context textDecoration:(CocoaWritingToolsCoordinatorTextDecoration)textDecoration completion:(void(^)(CocoaTextPreview *))completion
{
    [self previewForContext:context textDecoration:textDecoration completion:completion];
}

// The grammar-indicate animation does not drive any of the base coordinator flows (contexts, replacement,
// selection, decorations), so the remaining required delegate methods are inert.

- (void)writingToolsCoordinator:(CocoaWritingToolsCoordinator *)writingToolsCoordinator requestsContextsForScope:(CocoaWritingToolsCoordinatorContextScope)scope completion:(void(^)(NSArray<CocoaWritingToolsCoordinatorContext *> *))completion
{
    completion(@[]);
}

- (void)writingToolsCoordinator:(CocoaWritingToolsCoordinator *)writingToolsCoordinator replaceRange:(NSRange)range inContext:(CocoaWritingToolsCoordinatorContext *)context proposedText:(NSAttributedString *)replacementText reason:(CocoaWritingToolsCoordinatorTextReplacementReason)reason animationParameters:(CocoaWritingToolsCoordinatorAnimationParameters *)animationParameters completion:(void(^)(NSAttributedString *))completion
{
    completion(nil);
}

- (void)writingToolsCoordinator:(CocoaWritingToolsCoordinator *)writingToolsCoordinator selectRanges:(NSArray<NSValue *> *)ranges inContext:(CocoaWritingToolsCoordinatorContext *)context completion:(void(^)(void))completion
{
    completion();
}

- (void)writingToolsCoordinator:(CocoaWritingToolsCoordinator *)writingToolsCoordinator requestsBoundingBezierPathsForRange:(NSRange)range inContext:(CocoaWritingToolsCoordinatorContext *)context completion:(void(^)(NSArray<CocoaBezierPath *> *))completion
{
    completion(@[]);
}

- (void)writingToolsCoordinator:(CocoaWritingToolsCoordinator *)writingToolsCoordinator requestsUnderlinePathsForRange:(NSRange)range inContext:(CocoaWritingToolsCoordinatorContext *)context completion:(void(^)(NSArray<CocoaBezierPath *> *))completion
{
    completion(@[]);
}

#if PLATFORM(MAC)

- (void)writingToolsCoordinator:(NSWritingToolsCoordinator *)writingToolsCoordinator requestsPreviewForRect:(NSRect)rect inContext:(NSWritingToolsCoordinatorContext *)context completion:(void(^)(NSTextPreview *))completion
{
    completion(nil);
}

#endif

@end

#endif // ENABLE(WRITING_TOOLS_TEXT_EFFECTS)
