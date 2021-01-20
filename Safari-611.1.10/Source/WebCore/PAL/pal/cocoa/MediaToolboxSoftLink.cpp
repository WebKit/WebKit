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

#include "config.h"

#if USE(MEDIATOOLBOX)

#include <MediaToolbox/MediaToolbox.h>
#include <pal/spi/cocoa/MediaToolboxSPI.h>
#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, PAL_EXPORT)

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, FigPhotoDecompressionSetHardwareCutoff, void, (int format, size_t numPixelsCutoff), (format, numPixelsCutoff), PAL_EXPORT)

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTShouldPlayHDRVideo, Boolean, (CFArrayRef displayList), (displayList), PAL_EXPORT)
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTOverrideShouldPlayHDRVideo, void, (Boolean override, Boolean playHDRVideo), (override, playHDRVideo), PAL_EXPORT)
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MT_GetShouldPlayHDRVideoNotificationSingleton, CFTypeRef, (void), (), PAL_EXPORT)

SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, kMTSupportNotification_ShouldPlayHDRVideoChanged, CFStringRef, PAL_EXPORT)

#if HAVE(MT_PLUGIN_FORMAT_READER)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, kMTPluginFormatReaderProperty_Duration, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, kMTPluginTrackReaderProperty_Enabled, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, kMTPluginTrackReaderProperty_FormatDescriptionArray, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, kMTPluginTrackReaderProperty_NominalFrameRate, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, kMTPluginFormatReader_SupportsPlayableHorizonQueries, CFStringRef, PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTPluginByteSourceGetLength, int64_t, (MTPluginByteSourceRef byteSource), (byteSource), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTPluginByteSourceRead, OSStatus, (MTPluginByteSourceRef byteSource, size_t num, int64_t offset, void* dest, size_t* bytesReadOut), (byteSource, num, offset, dest, bytesReadOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTPluginFormatReaderGetClassID, CMBaseClassID, (), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTPluginFormatReaderGetTypeID, CFTypeID, (), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTPluginFormatReaderDisableSandboxing, OSStatus, (), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTRegisterPluginFormatReaderBundleDirectory, void, (CFURLRef directoryURL), (directoryURL), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTPluginSampleCursorGetClassID, CMBaseClassID, (), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTPluginSampleCursorGetTypeID, CFTypeID, (), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTPluginTrackReaderGetClassID, CMBaseClassID, (), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, MediaToolbox, MTPluginTrackReaderGetTypeID, CFTypeID, (), (), PAL_EXPORT)

#endif // HAVE(MT_PLUGIN_FORMAT_READER)

#endif // USE(MEDIATOOLBOX)
