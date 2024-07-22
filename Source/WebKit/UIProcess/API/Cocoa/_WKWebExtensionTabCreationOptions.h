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
 @abstract A `_WKWebExtensionTabCreationOptions` object encapsulates new tab creation options for an extension.
 @discussion This class holds the various options that influence the behavior and initial state of a newly created tab.
 The app retains the discretion to disregard any or all of these options, or even opt not to create a new tab.
 */
WK_CLASS_AVAILABLE(macos(14.2), ios(17.2))
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
 @abstract Indicates whether the new tab should become the active tab.
 @discussion If this property is set to `YES`, the new tab should be made active in the window, ensuring it is
 the frontmost tab. Being active implies the tab is also selected. If this property is set to `NO`,  the new tab shouldn't
 affect the currently active tab.
 */
@property (nonatomic, readonly) BOOL shouldActivate;

/*!
 @abstract Indicates whether the new tab should be added to the current tab selection.
 @discussion If this property is set to `YES`, the new tab should be part of the current selection, but not necessarily
 become the active tab unless `shouldActivate` is also `YES`. If this property is set to `NO`, the new tab shouldn't
 be part of the current selection.
 */
@property (nonatomic, readonly) BOOL shouldSelect;

/*! @abstract Indicates whether the new tab should be pinned. */
@property (nonatomic, readonly) BOOL shouldPin;

/*! @abstract Indicates whether the new tab should be muted. */
@property (nonatomic, readonly) BOOL shouldMute;

/*! @abstract Indicates whether the new tab should be in reader mode. */
@property (nonatomic, readonly) BOOL shouldShowReaderMode;

@end

NS_ASSUME_NONNULL_END
