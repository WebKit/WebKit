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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Chained on top of WebCorePrefix.h's OBJCXX PCH; do not include that file
   here. Anchors are headers shared by >=75% of platform/graphics/avfoundation
   ObjC++ TUs and not already in the base PCH closure. */

#if defined(__cplusplus) && defined(__OBJC__)
#undef new
#undef delete

#include "FourCC.h"
#include "ImmersiveVideoMetadata.h"
#include "Logging.h"
#include "MediaPlayerEnums.h"
#include "PlatformVideoColorSpace.h"

#include <JavaScriptCore/ArrayBufferView.h>

#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/spi/cf/CoreMediaSPI.h>

#include <wtf/JSONValues.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/cocoa/TypeCastsCocoa.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#define new ("if you use new/delete make sure to include config.h at the top of the file"())
#define delete ("if you use new/delete make sure to include config.h at the top of the file"())
#endif
