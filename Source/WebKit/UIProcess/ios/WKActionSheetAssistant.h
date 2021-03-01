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

#if PLATFORM(IOS_FAMILY)

#import "GestureTypes.h"
#import "WKActionSheet.h"
#import <UIKit/UIPopoverController.h>
#import <pal/spi/ios/DataDetectorsUISPI.h>
#import <wtf/RetainPtr.h>

namespace WebKit {
class WebPageProxy;
struct InteractionInformationAtPosition;
}

@class UIMenuElement;
@class UITargetedPreview;
@class WKActionSheetAssistant;
@class _WKActivatedElementInfo;
@class _WKElementAction;
@protocol WKActionSheetDelegate;
@protocol UIContextMenuInteractionDelegate;

typedef NS_ENUM(NSInteger, _WKElementActionType);

@protocol WKActionSheetAssistantDelegate <NSObject>
@required
- (Optional<WebKit::InteractionInformationAtPosition>)positionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant performAction:(WebKit::SheetAction)action;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant openElementAtLocation:(CGPoint)location;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant shareElementWithURL:(NSURL *)url rect:(CGRect)boundingRect;
#if HAVE(APP_LINKS)
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeAppLinkActionsForElement:(_WKActivatedElementInfo *)element;
#endif
- (RetainPtr<NSArray>)actionSheetAssistant:(WKActionSheetAssistant *)assistant decideActionsForElement:(_WKActivatedElementInfo *)element defaultActions:(RetainPtr<NSArray>)defaultActions;

@optional
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant showCustomSheetForElement:(_WKActivatedElementInfo *)element;
- (void)updatePositionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant;
- (CGRect)unoccludedWindowBoundsForActionSheetAssistant:(WKActionSheetAssistant *)assistant;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant willStartInteractionWithElement:(_WKActivatedElementInfo *)element;
- (void)actionSheetAssistantDidStopInteraction:(WKActionSheetAssistant *)assistant;
- (NSDictionary *)dataDetectionContextForActionSheetAssistant:(WKActionSheetAssistant *)assistant positionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation;
- (NSString *)selectedTextForActionSheetAssistant:(WKActionSheetAssistant *)assistant;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant getAlternateURLForImage:(UIImage *)image completion:(void (^)(NSURL *alternateURL, NSDictionary *userInfo))completion;
#if USE(UICONTEXTMENU)
- (UITargetedPreview *)createTargetedContextMenuHintForActionSheetAssistant:(WKActionSheetAssistant *)assistant;
- (void)removeContextMenuViewIfPossibleForActionSheetAssistant:(WKActionSheetAssistant *)assistant;
#endif
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant shareElementWithImage:(UIImage *)image rect:(CGRect)boundingRect;
#if ENABLE(IMAGE_EXTRACTION)
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeImageExtractionActionForElement:(_WKActivatedElementInfo *)element;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant handleImageExtraction:(UIImage *)image title:(NSString *)title;
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeRevealImageActionForElement:(_WKActivatedElementInfo *)element;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant handleRevealImage:(UIImage *)image title:(NSString *)title;
#endif

@end

#if ENABLE(DATA_DETECTION) && USE(UICONTEXTMENU)
@interface WKActionSheetAssistant : NSObject <WKActionSheetDelegate, DDDetectionControllerInteractionDelegate, UIContextMenuInteractionDelegate>
- (BOOL)hasContextMenuInteraction;
#else
@interface WKActionSheetAssistant : NSObject <WKActionSheetDelegate>
#endif
@property (nonatomic, weak) id <WKActionSheetAssistantDelegate> delegate;
@property (nonatomic) BOOL needsLinkIndicator;
- (id)initWithView:(UIView *)view;
- (void)showLinkSheet;
- (void)showImageSheet;
- (void)showDataDetectorsUIForPositionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation;
- (void)cleanupSheet;
- (void)updateSheetPosition;
- (RetainPtr<NSArray<_WKElementAction *>>)defaultActionsForLinkSheet:(_WKActivatedElementInfo *)elementInfo;
- (RetainPtr<NSArray<_WKElementAction *>>)defaultActionsForImageSheet:(_WKActivatedElementInfo *)elementInfo;
- (BOOL)isShowingSheet;
- (void)interactionDidStartWithPositionInformation:(const WebKit::InteractionInformationAtPosition&)information;
- (NSArray *)currentAvailableActionTitles;
- (void)handleElementActionWithType:(_WKElementActionType)type element:(_WKActivatedElementInfo *)element needsInteraction:(BOOL)needsInteraction;
#if USE(UICONTEXTMENU)
- (NSArray<UIMenuElement *> *)suggestedActionsForContextMenuWithPositionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation;
#endif
@end

#endif // PLATFORM(IOS_FAMILY)
