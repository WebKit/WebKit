/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit/WKFoundation.h>

@protocol WKWebExtensionTab;
@protocol WKWebExtensionWindow;

WK_HEADER_AUDIT_BEGIN(nullability, sendability)

/*!
 @abstract A `WKWebExtensionTabCreationOptions` object encapsulates new tab creation options for an extension.
 @discussion This class holds the various options that influence the behavior and initial state of a newly created tab.
 The app retains the discretion to disregard any or all of these options, or even opt not to create a new tab.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
WK_SWIFT_UI_ACTOR NS_SWIFT_NAME(WKWebExtension.TabCreationOptions)
@interface WKWebExtensionTabCreationOptions : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*!
 @abstract Indicates the window where the new tab should be opened.
 @discussion If this property is set to `nil`, no window was specified.
 */
@property (nonatomic, nullable, readonly, strong) id <WKWebExtensionWindow> window;

/*! @abstract Indicates the position where the new tab should be opened within the window. */
@property (nonatomic, readonly) NSUInteger index;

/*!
 @abstract Indicates the parent tab with which the new tab should be related.
 @discussion If this property is set to `nil`, no parent tab was specified.
 */
@property (nonatomic, nullable, readonly, strong) id <WKWebExtensionTab> parentTab;

/*!
 @abstract Indicates the initial URL for the new tab.
 @discussion If this property is set to `nil`, the app's default "start page" should appear in the new tab.
 */
@property (nonatomic, nullable, readonly, copy) NSURL *url;

/*!
 @abstract Indicates whether the new tab should become the active tab.
 @discussion If this property is set to `YES`, the new tab should be made active in the window, ensuring it is
 the frontmost tab. Being active implies the tab is also selected. If this property is set to `NO`,  the new tab shouldn't
 affect the currently active tab.
 */
@property (nonatomic, readonly, getter=isActive) BOOL active;

/*!
 @abstract Indicates whether the new tab should be added to the current tab selection.
 @discussion If this property is set to `YES`, the new tab should be part of the current selection, but not necessarily
 become the active tab unless `shouldActivate` is also `YES`. If this property is set to `NO`, the new tab shouldn't
 be part of the current selection.
 */
@property (nonatomic, readonly, getter=isSelected) BOOL selected;

/*! @abstract Indicates whether the new tab should be pinned. */
@property (nonatomic, readonly, getter=isPinned) BOOL pinned;

/*! @abstract Indicates whether the new tab should be muted. */
@property (nonatomic, readonly, getter=isMuted) BOOL muted;

/*! @abstract Indicates whether the new tab should be in reader mode. */
@property (nonatomic, readonly, getter=isReaderModeShowing) BOOL readerModeShowing;

@end

WK_HEADER_AUDIT_END(nullability, sendability)
