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

#if PLATFORM(VISION) && ENABLE(CONNECTED_VOLUMETRIC_SCENE)

#import <WebCore/FloatSize.h>
#import <WebCore/LayerHostingContextIdentifier.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

// Owns one volumetric UIWindowScene hosting an element's model content; one per presented element.
@interface WKPortalVolumetricSceneController : NSObject

// closeHandler runs only for a user-initiated close.
- (instancetype)initWithCloseHandler:(void (^)(void))closeHandler;

- (void)presentWithCompletion:(void (^)(BOOL success))completion;
- (void)dismissWithCompletion:(nullable void (^)(void))completion;

// Returns the volume's extent in meters, or zero if the scene has not laid out yet.
- (WebCore::FloatSize)hostContentWithContext:(WebCore::LayerHostingContextIdentifier)contentContext pid:(int)pid;

- (void)setVolumeSizeChangedHandler:(nullable void (^)(WebCore::FloatSize))handler;

- (void)updateLayoutForVolumeSize;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // PLATFORM(VISION) && ENABLE(CONNECTED_VOLUMETRIC_SCENE)
