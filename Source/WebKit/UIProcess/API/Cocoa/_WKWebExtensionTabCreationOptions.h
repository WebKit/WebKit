/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import <WebKit/WKFoundation.h>

#import <Foundation/Foundation.h>

@protocol _WKWebExtensionTab;
@protocol _WKWebExtensionWindow;

NS_ASSUME_NONNULL_BEGIN

/*!
 @abstract A `_WKWebExtensionWindowCreationOptions` object encapsulates new window creation options for an extension.
 @discussion This class holds the various options that influence the behavior and initial state of a newly created window.
 The app retains the discretion to disregard any or all of these options, or even opt not to create a new window.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKWebExtensionTabCreationOptions : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*!
 @abstract Indicates the window where the new tab should be opened.
 @discussion If this property is set to `nil`, no window was specified.
 */
@property (nonatomic, nullable, readonly, strong) id <_WKWebExtensionWindow> desiredWindow;

/*! @abstract Indicates the position where the new tab should be opened within the window. */
@property (nonatomic, readonly) NSUInteger desiredIndex;

/*!
 @abstract Indicates the parent tab with which the new tab should be related.
 @discussion If this property is set to `nil`, no parent tab was specified.
 */
@property (nonatomic, nullable, readonly, strong) id <_WKWebExtensionTab> desiredParentTab;

/*!
 @abstract Indicates the initial URL for the new tab.
 @discussion If this property is set to `nil`, the app's default "start page" should appear in the new tab.
 */
@property (nonatomic, nullable, readonly, copy) NSURL *desiredURL;

/*!
 @abstract Indicates whether the new tab should be selected.
 @discussion If this property is set to `YES`, and if `shouldExtendSelection` is also `YES`, the new tab
 should be added to the current selection without altering the frontmost tab. If `shouldExtendSelection` is `NO`,
 the previous selection should be cleared and the new tab should be the only one selected. If `shouldSelect` is `NO`,
 this tab should not be part of the selection regardless of the `shouldExtendSelection` value.
 @seealso shouldExtendSelection
 */
@property (nonatomic, readonly) BOOL shouldSelect;

/*!
 @abstract Indicates whether the new tab's selection should extend the current selection.
 @discussion If this property is set to `YES`, and if `shouldSelect` is also `YES`, the new tab should be added
 to the current selection without altering the frontmost tab. If this property is set to `NO`, or if `shouldSelect` is `NO`,
 the current selection state of other tabs should not be affected by the creation of the new tab.
 @seealso shouldSelect
 */
@property (nonatomic, readonly) BOOL shouldExtendSelection;

/*! @abstract Indicates whether the new tab should be pinned. */
@property (nonatomic, readonly) BOOL shouldPin;

/*! @abstract Indicates whether the new tab should be muted. */
@property (nonatomic, readonly) BOOL shouldMute;

/*! @abstract Indicates whether the new tab should be in reader mode. */
@property (nonatomic, readonly) BOOL shouldShowReaderMode;

@end

NS_ASSUME_NONNULL_END
