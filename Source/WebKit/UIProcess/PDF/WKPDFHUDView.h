/*
 * Copyright (C) 2020-2026 Apple Inc. All rights reserved.
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

#import <wtf/Platform.h>

#if ENABLE(PDF_HUD)

#import <AppKit/AppKit.h>

#if ENABLE(AX_PDF_SUPPORT)
#import <WebKitAdditions/WKPDFHUDViewAdditions.h>
#endif

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

typedef NS_ENUM(NSInteger, WKPDFHUDViewControlAction) {
    WKPDFHUDViewControlActionZoomIn,
    WKPDFHUDViewControlActionZoomOut,
    WKPDFHUDViewControlActionToggleAccessibilityDisplayMode,
    WKPDFHUDViewControlActionOpenInPreview,
    WKPDFHUDViewControlActionSavePDF,
};

typedef NS_ENUM(NSInteger, WKPDFHUDViewAccessibilityDisplayModeState) {
    WKPDFHUDViewAccessibilityDisplayModeStateUnavailable,
    WKPDFHUDViewAccessibilityDisplayModeStateInactive,
    WKPDFHUDViewAccessibilityDisplayModeStateActive,
};

NS_SWIFT_UI_ACTOR
@protocol WKPDFHUDView

@property (nonatomic, readonly) uint64_t frameIdentifier;

// FIXME: Ideally this initializer would be a requirement of this protocol, but Swift 6.3 has a bug
// when dealing with `required` initializers of internal protocol types.
// - (instancetype)initWithFrame:(NSRect)frame frameIdentifier:(uint64_t)frameIdentifier compositingBordersVisible:(BOOL)compositingBordersVisible actionHandler:(NS_SWIFT_UI_ACTOR void(^)(WKPDFHUDViewControlAction))actionHandler;

- (void)show;

- (void)setAccessibilityDisplayModeState:(WKPDFHUDViewAccessibilityDisplayModeState)state;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // ENABLE(PDF_HUD)
