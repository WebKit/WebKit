/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#import <WebKit/WKWebView.h>

#if TARGET_OS_IPHONE

@interface WKWebView (WKTestingIOS)

@property (nonatomic, readonly) NSString *textContentTypeForTesting;
@property (nonatomic, readonly) NSString *selectFormPopoverTitle;
@property (nonatomic, readonly) NSString *formInputLabel;
@property (nonatomic, readonly) NSArray<NSValue *> *_uiTextSelectionRects;
@property (nonatomic, readonly) CGRect _inputViewBounds;
@property (nonatomic, readonly) NSString *_scrollingTreeAsText;
@property (nonatomic, readonly) NSNumber *_stableStateOverride;
@property (nonatomic, readonly) CGRect _dragCaretRect;

- (void)keyboardAccessoryBarNext;
- (void)keyboardAccessoryBarPrevious;
- (void)dismissFormAccessoryView;
- (void)_dismissFilePicker;
- (void)selectFormAccessoryPickerRow:(int)rowIndex;

- (void)setTimePickerValueToHour:(NSInteger)hour minute:(NSInteger)minute;

- (void)applyAutocorrection:(NSString *)newString toString:(NSString *)oldString withCompletionHandler:(void (^)(void))completionHandler;

- (void)_didShowContextMenu;
- (void)_didDismissContextMenu;
- (void)_doAfterResettingSingleTapGesture:(dispatch_block_t)action;

- (NSDictionary *)_propertiesOfLayerWithID:(unsigned long long)layerID;
- (void)_simulateLongPressActionAtLocation:(CGPoint)location;
- (void)_simulateTextEntered:(NSString *)text;

- (void)_doAfterReceivingEditDragSnapshotForTesting:(dispatch_block_t)action;

- (void)_triggerSystemPreviewActionOnElement:(uint64_t)elementID document:(uint64_t)documentID page:(uint64_t)pageID;

- (void)_setDeviceOrientationUserPermissionHandlerForTesting:(BOOL (^)(void))handler;

@end

#endif // TARGET_OS_IPHONE
