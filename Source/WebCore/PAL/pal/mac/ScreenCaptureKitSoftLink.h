/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if HAVE(SCREEN_CAPTURE_KIT)

#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, ScreenCaptureKit);

SOFT_LINK_CLASS_FOR_HEADER(PAL, SCWindow)
SOFT_LINK_CLASS_FOR_HEADER(PAL, SCDisplay)
SOFT_LINK_CLASS_FOR_HEADER(PAL, SCShareableContent)
SOFT_LINK_CLASS_FOR_HEADER(PAL, SCContentFilter)
SOFT_LINK_CLASS_FOR_HEADER(PAL, SCStreamConfiguration)
SOFT_LINK_CLASS_FOR_HEADER(PAL, SCStream)
SOFT_LINK_CLASS_FOR_HEADER(PAL, SCContentSharingPicker)
SOFT_LINK_CLASS_FOR_HEADER(PAL, SCContentSharingPickerConfiguration)

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, ScreenCaptureKit, SCStreamFrameInfoStatus, NSString *)
#define SCStreamFrameInfoStatus PAL::get_ScreenCaptureKit_SCStreamFrameInfoStatusSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, ScreenCaptureKit, SCStreamFrameInfoScaleFactor, NSString *)
#define SCStreamFrameInfoScaleFactor PAL::get_ScreenCaptureKit_SCStreamFrameInfoScaleFactorSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, ScreenCaptureKit, SCStreamFrameInfoContentScale, NSString *)
#define SCStreamFrameInfoContentScale PAL::get_ScreenCaptureKit_SCStreamFrameInfoContentScaleSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, ScreenCaptureKit, SCStreamFrameInfoContentRect, NSString *)
#define SCStreamFrameInfoContentRect PAL::get_ScreenCaptureKit_SCStreamFrameInfoContentRectSingleton()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, ScreenCaptureKit, SCStreamFrameInfoPresenterOverlayContentRect, NSString *)
#define SCStreamFrameInfoPresenterOverlayContentRect PAL::get_ScreenCaptureKit_SCStreamFrameInfoPresenterOverlayContentRectSingleton()

#endif // HAVE(SCREEN_CAPTURE_KIT)
