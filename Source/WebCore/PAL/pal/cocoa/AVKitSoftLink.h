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

#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, AVKit)

#if HAVE(AVLEGIBLEMEDIAOPTIONSMENUCONTROLLER)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVLegibleMediaOptionsMenuController)
#endif

#if HAVE(PIP_CONTROLLER)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVPictureInPictureContentViewController)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVPictureInPictureControllerContentSource)
#endif

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVBackgroundView)
#endif // ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)

#if ENABLE(VIDEO) && PLATFORM(MAC)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVPlayerView)
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVInterfaceMediaSelectionOptionSource)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVInterfaceMetadata)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVInterfaceTimelineSegment)
#pragma clang diagnostic pop

SOFT_LINK_CLASS_FOR_HEADER(PAL, __AVPlayerLayerView)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVOutputDeviceMenuController)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVPictureInPictureController)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVPlayerController)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVPlayerViewController)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVPlayerViewControllerContentSource)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVRoutePickerView)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVTimeRange)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVTouchBarMediaSelectionOption)
SOFT_LINK_CLASS_FOR_HEADER(PAL, AVValueTiming)
