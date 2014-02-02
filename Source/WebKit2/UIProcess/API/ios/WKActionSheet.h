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

#import <UIKit/UIActionSheet.h>
#import <UIKit/UIPopoverController.h>
#import <WebKit2/WKDeclarationSpecifiers.h>

@protocol WKActionSheetDelegate;
@class WKInteractionView;

@interface WKActionSheet : UIActionSheet

@property (nonatomic, assign) id <WKActionSheetDelegate> sheetDelegate;
@property (nonatomic) UIPopoverArrowDirection arrowDirections;
- (id)initWithView:(WKInteractionView *)view;
- (void)doneWithSheet;
- (BOOL)presentSheet;
- (BOOL)presentSheetFromRect:(CGRect)presentationRect;
- (void)updateSheetPosition;
@end


@protocol WKActionSheetDelegate<UIActionSheetDelegate>
@required
- (UIView *)hostViewForSheet;
- (CGRect)initialPresentationRectInHostViewForSheet;
- (CGRect)presentationRectInHostViewForSheet;
@end

// Elements to describe action sheet content.

WK_EXPORT @interface WKElementActionInfo : NSObject

- (WKElementActionInfo *)initWithInfo:(NSURL *)url location:(CGPoint)location title:(NSString *)title rect:(CGRect)rect;
@property (nonatomic, readonly) CGPoint interactionLocation;
@property (nonatomic, readonly) NSURL *url;
@property (nonatomic, readonly) NSString *title;
@property (nonatomic, readonly) CGRect boundingRect;
@end

typedef void (^WKElementActionHandler)(WKElementActionInfo *);
typedef void (^WKElementActionHandlerInternal)(WKInteractionView *, WKElementActionInfo *);

typedef enum {
    WKElementActionTypeCustom = 0,
    WKElementActionTypeOpen,
    WKElementActionTypeCopy,
    WKElementActionTypeSaveImage,
    WKElementActionTypeAddToReadingList,
} WKElementActionType;

WK_EXPORT @interface WKElementAction : NSObject
@property (nonatomic, readonly) WKElementActionType type;
@property (nonatomic, readonly) NSString* title;

+ (WKElementAction *)customElementActionWithTitle:(NSString *)title actionHandler:(WKElementActionHandler)handler;
+ (WKElementAction *)standardElementActionWithType:(WKElementActionType)type;
+ (WKElementAction *)standardElementActionWithType:(WKElementActionType)type customTitle:(NSString *)title;
- (void)runActionWithElementInfo:(WKElementActionInfo *)info view:(WKInteractionView *)view;

@end
