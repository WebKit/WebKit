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

SOFT_LINK_CLASS_FOR_HEADER_WITH_AVAILABILITY(PAL, SCWindow, API_AVAILABLE(macos(12.3)))
SOFT_LINK_CLASS_FOR_HEADER_WITH_AVAILABILITY(PAL, SCDisplay, API_AVAILABLE(macos(12.3)))
SOFT_LINK_CLASS_FOR_HEADER_WITH_AVAILABILITY(PAL, SCShareableContent, API_AVAILABLE(macos(12.3)))
SOFT_LINK_CLASS_FOR_HEADER_WITH_AVAILABILITY(PAL, SCContentFilter, API_AVAILABLE(macos(12.3)))
SOFT_LINK_CLASS_FOR_HEADER_WITH_AVAILABILITY(PAL, SCStreamConfiguration, API_AVAILABLE(macos(12.3)))
SOFT_LINK_CLASS_FOR_HEADER_WITH_AVAILABILITY(PAL, SCStream, API_AVAILABLE(macos(12.3)))
SOFT_LINK_CLASS_FOR_HEADER(PAL, SCContentSharingSession)

#if HAVE(SC_CONTENT_SHARING_PICKER)
SOFT_LINK_CLASS_FOR_HEADER(PAL, SCContentSharingPicker)
SOFT_LINK_CLASS_FOR_HEADER(PAL, SCContentSharingPickerConfiguration)
#endif

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, ScreenCaptureKit, SCStreamFrameInfoStatus, NSString *)
#define SCStreamFrameInfoStatus PAL::get_ScreenCaptureKit_SCStreamFrameInfoStatus()

#endif // HAVE(SCREEN_CAPTURE_KIT)
