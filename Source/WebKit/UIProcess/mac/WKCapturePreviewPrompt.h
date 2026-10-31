/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) && ENABLE(MEDIA_STREAM)

#import "WKCapturePreviewPrompting.h"
#import <AppKit/AppKit.h>
#import <WebCore/CaptureDevice.h>
#import <wtf/Vector.h>

@interface WKCaptureDeviceChooser : NSObject <WKCapturePreviewPrompting>

- (instancetype)initWithVideoDevices:(Vector<WebCore::CaptureDevice>&&)videoDevices audioDevices:(Vector<WebCore::CaptureDevice>&&)audioDevices;
- (NSView *)view;
- (void)setAllowButtons:(NSArray<NSButton *> *)allowButtons;
- (void)beginPreview;

@end

@interface WKCapturePreviewPromptSheet : NSObject

- (instancetype)initWithMessageText:(NSString *)messageText informativeText:(NSString *)informativeText accessoryView:(NSView *)accessoryView;
- (NSButton *)addButtonWithTitle:(NSString *)title response:(NSModalResponse)response;
- (void)beginSheetModalForWindow:(NSWindow *)parentWindow didPresentHandler:(void (^)(void))didPresentHandler completionHandler:(void (^)(NSModalResponse))completionHandler;
- (void)dismissWithResponse:(NSModalResponse)response;

@end

#endif // PLATFORM(MAC) && ENABLE(MEDIA_STREAM)
