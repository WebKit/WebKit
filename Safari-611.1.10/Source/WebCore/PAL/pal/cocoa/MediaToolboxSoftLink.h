/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if USE(MEDIATOOLBOX)

#include <MediaToolbox/MediaToolbox.h>
#include <pal/spi/cocoa/MediaToolboxSPI.h>
#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, MediaToolbox)

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, FigPhotoDecompressionSetHardwareCutoff, void, (int, size_t numPixelsCutoff), (format, numPixelsCutoff))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, MTShouldPlayHDRVideo, Boolean, (CFArrayRef displayList), (displayList))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, MTOverrideShouldPlayHDRVideo, void, (Boolean override, Boolean playHDRVideo), (override, playHDRVideo))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, MT_GetShouldPlayHDRVideoNotificationSingleton, CFTypeRef, (void), ())

SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, kMTSupportNotification_ShouldPlayHDRVideoChanged, CFStringRef)

#if HAVE(MT_PLUGIN_FORMAT_READER)

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, MediaToolbox, kMTPluginFormatReaderProperty_Duration, CFStringRef)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, MediaToolbox, kMTPluginTrackReaderProperty_Enabled, CFStringRef)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, MediaToolbox, kMTPluginTrackReaderProperty_FormatDescriptionArray, CFStringRef)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, MediaToolbox, kMTPluginTrackReaderProperty_NominalFrameRate, CFStringRef)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, kMTPluginFormatReader_SupportsPlayableHorizonQueries, CFStringRef)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTPluginByteSourceGetLength, int64_t, (MTPluginByteSourceRef byteSource), (byteSource))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTPluginByteSourceRead, OSStatus, (MTPluginByteSourceRef byteSource, size_t num, int64_t offset, void* dest, size_t* bytesReadOut), (byteSource, num, offset, dest, bytesReadOut))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTPluginFormatReaderGetClassID, CMBaseClassID, (), ())
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTPluginFormatReaderGetTypeID, CFTypeID, (), ())
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTPluginFormatReaderDisableSandboxing, OSStatus, (), ())
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTRegisterPluginFormatReaderBundleDirectory, void, (CFURLRef directoryURL), (directoryURL))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTPluginSampleCursorGetClassID, CMBaseClassID, (), ())
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTPluginSampleCursorGetTypeID, CFTypeID, (), ())
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTPluginTrackReaderGetClassID, CMBaseClassID, (), ())
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTPluginTrackReaderGetTypeID, CFTypeID, (), ())

#endif // HAVE(MT_PLUGIN_FORMAT_READER)

#endif // USE(MEDIATOOLBOX)
