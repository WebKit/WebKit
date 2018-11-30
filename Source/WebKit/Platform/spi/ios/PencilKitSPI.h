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

#if HAVE(PENCILKIT)

#if USE(APPLE_INTERNAL_SDK)

#import <PencilKit/PencilKit.h>
#import <PencilKit/PKCanvasView_Private.h>
#import <PencilKit/PKDrawing_Private.h>
#import <PencilKit/PKImageRenderer.h>
#import <PencilKit/PKInlineInkPicker.h>
#import <PencilKit/PKInlineInkPicker_Private.h>
#import <PencilKit/PKStroke.h>

#else

#import <UIKit/UIKit.h>

@class PKCanvasView;
@class PKDrawing;
@class PKInk;
@class PKInlineInkPicker;

typedef NSString * PKInkIdentifier;

@protocol PKCanvasViewDelegate
@optional
- (void)drawingDidChange:(PKCanvasView *)canvasView;
@end

@protocol PKInlineInkPickerDelegate <NSObject>
@optional
- (void)inlineInkPicker:(PKInlineInkPicker *)inlineInkPicker didSelectTool:(PKInkIdentifier)identifer;
- (void)inlineInkPickerDidToggleRuler:(PKInlineInkPicker *)inlineInkPicker;
- (void)inlineInkPicker:(PKInlineInkPicker *)inlineInkPicker didSelectColor:(UIColor *)color;
- (UIViewController *)viewControllerForPopoverPresentationFromInlineInkPicker:(PKInlineInkPicker *)inlineInkPicker;
@end

@interface PKCanvasView : UIView

@property (nonatomic, getter=isFingerDrawingEnabled) BOOL fingerDrawingEnabled;
@property (nonatomic) BOOL rulerEnabled;
@property (nonatomic, copy) PKDrawing *drawing;
@property (nonatomic, strong) PKInk *ink;
@property (nonatomic, weak) NSObject<PKCanvasViewDelegate> *drawingDelegate;

@end

@interface PKInlineInkPicker : UIControl

@property (nonatomic, copy) PKInk *selectedInk;
- (void)setSelectedInk:(PKInk *)selectedInk animated:(BOOL)animated;
@property (nonatomic, weak) id<PKInlineInkPickerDelegate> delegate;

@end

@interface PKDrawing : NSObject

- (instancetype)initWithData:(NSData *)data error:(NSError **)error;
- (NSData *)serialize;

@end

@interface PKImageRenderer : NSObject

- (instancetype)initWithSize:(CGSize)size scale:(CGFloat)scale renderQueue:(dispatch_queue_t)renderQueue;
- (void)renderDrawing:(PKDrawing *)drawing completion:(void(^)(UIImage *image))completionBlock;

@end

#endif

#endif // HAVE(PENCILKIT)
