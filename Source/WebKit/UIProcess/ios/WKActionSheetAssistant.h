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
#import <WebCore/ElementContext.h>
#import <WebCore/HTMLMediaElementIdentifier.h>
#import <WebCore/MediaControlsContextMenuItem.h>
#import <pal/spi/ios/DataDetectorsUISPI.h>
#import <wtf/CompletionHandler.h>
#import <wtf/Forward.h>
#import <wtf/RetainPtr.h>

namespace WebCore {
class FloatRect;
}

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
- (std::optional<WebKit::InteractionInformationAtPosition>)positionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant;
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
- (void)actionSheetAssistantDidShowContextMenu:(WKActionSheetAssistant *)assistant;
- (void)actionSheetAssistantDidDismissContextMenu:(WKActionSheetAssistant *)assistant;
#endif
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant shareElementWithImage:(UIImage *)image rect:(CGRect)boundingRect;
#if ENABLE(IMAGE_ANALYSIS)
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeShowTextActionForElement:(_WKActivatedElementInfo *)element;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant showTextForImage:(UIImage *)image imageURL:(NSURL *)imageURL title:(NSString *)title imageBounds:(CGRect)imageBounds;
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeLookUpImageActionForElement:(_WKActivatedElementInfo *)element;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant lookUpImage:(UIImage *)image imageURL:(NSURL *)imageURL title:(NSString *)title imageBounds:(CGRect)imageBounds;
#endif
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
- (BOOL)actionSheetAssistantShouldIncludeCopySubjectAction:(WKActionSheetAssistant *)assistant;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant copySubject:(UIImage *)image sourceMIMEType:(NSString *)sourceMIMEType;
#endif
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
- (BOOL)_allowAnimationControls;
- (void)_actionSheetAssistant:(WKActionSheetAssistant *)assistant performAction:(WebKit::SheetAction)action onElements:(Vector<WebCore::ElementContext>&&)elements;
#endif
- (NSArray<UIMenuElement *> *)additionalMediaControlsContextMenuItemsForActionSheetAssistant:(WKActionSheetAssistant *)assistant;
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
- (void)captionStyleMenuWillOpenWithFrameInfo:(const WebKit::FrameInfoData&)frameInfo identifier:(WebCore::HTMLMediaElementIdentifier)identifier;
- (void)captionStyleMenuDidCloseWithFrameInfo:(const WebKit::FrameInfoData&)frameInfo identifier:(WebCore::HTMLMediaElementIdentifier)identifier;
#endif
@end

#if USE(UICONTEXTMENU)
@interface WKActionSheetAssistant : NSObject <WKActionSheetDelegate,
#if ENABLE(DATA_DETECTION)
DDDetectionControllerInteractionDelegate,
#endif
UIContextMenuInteractionDelegate>
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
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
- (void)showMediaControlsContextMenu:(WebCore::FloatRect&&)targetFrame items:(Vector<WebCore::MediaControlsContextMenuItem>&&)items frameInfo:(const WebKit::FrameInfoData&)frameInfo identifier:(WebCore::HTMLMediaElementIdentifier)identifier completionHandler:(CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&&)completionHandler;
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
#if ENABLE(VIDEO) && USE(UICONTEXTMENU)
- (void)showCaptionDisplaySettingsMenu:(WebCore::HTMLMediaElementIdentifier)identifier withOptions:(const WebCore::ResolvedCaptionDisplaySettingsOptions&)options completionHandler:(CompletionHandler<void(Expected<void, WebCore::ExceptionData>)>&&)completionHandler;
#endif
- (void)cleanupSheet;
- (void)updateSheetPosition;
- (RetainPtr<NSArray<_WKElementAction *>>)defaultActionsForLinkSheet:(_WKActivatedElementInfo *)elementInfo;
- (RetainPtr<NSArray<_WKElementAction *>>)defaultActionsForImageSheet:(_WKActivatedElementInfo *)elementInfo;
- (BOOL)isShowingSheet;
- (void)interactionDidStartWithPositionInformation:(const WebKit::InteractionInformationAtPosition&)information;
- (void)handleElementActionWithType:(_WKElementActionType)type element:(_WKActivatedElementInfo *)element needsInteraction:(BOOL)needsInteraction;
#if USE(UICONTEXTMENU)
- (NSMutableArray<UIMenuElement *> *)suggestedActionsForContextMenuWithPositionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation;
#endif
@end

@interface WKActionSheetAssistant (WKTesting)
- (NSArray *)currentlyAvailableActionTitles;
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
- (NSArray *)currentlyAvailableMediaControlsContextMenuItems;
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
@end

#endif // PLATFORM(IOS_FAMILY)
