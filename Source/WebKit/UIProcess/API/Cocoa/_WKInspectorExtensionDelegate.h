/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import <WebKit/WKFoundation.h>

#if TARGET_OS_OSX

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class _WKInspectorExtension;

@protocol _WKInspectorExtensionDelegate <NSObject>
@optional

/**
 * @abstract Called when a tab associated with this extension has been shown.
 * @param extension The extension that created the shown tab.
 * @param tabIdentifier Identifier for the tab that was shown.
 */
- (void)inspectorExtension:(_WKInspectorExtension *)extension didShowTabWithIdentifier:(NSString *)tabIdentifier;

/**
 * @abstract Called when a tab associated with this extension has been hidden.
 * @param extension The extension that created the hidden tab.
 * @param tabIdentifier Identifier for the tab that was hidden.
 */
- (void)inspectorExtension:(_WKInspectorExtension *)extension didHideTabWithIdentifier:(NSString *)tabIdentifier;

@end

NS_ASSUME_NONNULL_END

#endif // TARGET_OS_OSX
