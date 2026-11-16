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

#if PLATFORM(IOS_FAMILY) && ENABLE(MEDIA_STREAM) && !PLATFORM(APPLETV)

#include "MediaPermissionPromptCustomization.h"
#include "WKCapturePreviewPrompting.h"
#include <UIKit/UIKit.h>
#include <WebCore/CaptureDevice.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Vector.h>

@interface WKCapturePreviewViewController : UIViewController <WKCapturePreviewPrompting>

- (instancetype)initWithAvailableSize:(CGSize)availableSize title:(NSString *)alertTitle informativeText:(NSString *)informativeText allowButtonTitle:(NSString *)allowButtonTitle denyButtonTitle:(NSString *)denyButtonTitle additionalActions:(Vector<WebKit::MediaPermissionPromptAdditionalAction>&&)additionalActions videoDevices:(Vector<WebCore::CaptureDevice>&&)videoDevices audioDevices:(Vector<WebCore::CaptureDevice>&&)audioDevices;

- (void)setDecisionHandler:(CompletionHandler<void(WebKit::MediaPermissionPromptResult&&)>&&)handler;
- (void)dismissWithoutAnswer;
- (void)dismissWhenPossible;

@end

#endif // PLATFORM(IOS_FAMILY) && ENABLE(MEDIA_STREAM) && !PLATFORM(APPLETV)
