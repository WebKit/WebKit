/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#import "config.h"

#import "ArgumentCodersCocoa.h"
#import <CoreText/CoreText.h>
#import <WebCore/AttributedString.h>
#import <WebCore/DataDetectorElementInfo.h>
#import <WebCore/DictionaryPopupInfo.h>
#import <WebCore/Font.h>
#import <WebCore/FontAttributes.h>
#import <WebCore/FontCustomPlatformData.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/SharedMemory.h>
#import <WebCore/TextRecognitionResult.h>
#import <pal/spi/cf/CoreTextSPI.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIFont.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <WebCore/MediaPlaybackTargetContext.h>
#import <objc/runtime.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebCoreArgumentCodersCocoaAdditions.mm>
#endif

#if USE(AVFOUNDATION)
#import <wtf/MachSendRight.h>
#endif

#if ENABLE(APPLE_PAY)
#import <pal/cocoa/PassKitSoftLink.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <pal/cocoa/AVFoundationSoftLink.h>
#endif

#if ENABLE(DATA_DETECTION)
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#endif

#if USE(AVFOUNDATION)
#import <WebCore/CoreVideoSoftLink.h>
#endif

#import <pal/cocoa/VisionKitCoreSoftLink.h>

namespace IPC {

} // namespace IPC
