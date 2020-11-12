/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "AVAssetTrackUtilities.h"

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#import "FourCC.h"
#import "SystemBattery.h"
#import "VideoToolboxSoftLink.h"
#import <AVFoundation/AVAssetTrack.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

static Vector<FourCC> contentTypesToCodecs(const Vector<ContentType>& contentTypes)
{
    Vector<FourCC> codecs;
    for (auto& contentType : contentTypes) {
        auto codecStrings = contentType.codecs();
        for (auto& codecString : codecStrings) {
            // https://tools.ietf.org/html/rfc6381
            // Within a 'codecs' parameter value, "." is reserved as a hierarchy delimiter
            auto firstPeriod = codecString.find('.');
            if (firstPeriod != notFound)
                codecString.truncate(firstPeriod);

            auto codecIdentifier = FourCC::fromString(codecString.left(4));
            if (codecIdentifier)
                codecs.append(codecIdentifier.value());
        }
    }
    return codecs;
}

bool codecsMeetHardwareDecodeRequirements(const Vector<FourCC>& codecs, const Vector<ContentType>& contentTypesRequiringHardwareDecode)
{
    static bool hasBattery = systemHasBattery();

    // If the system is exclusively wall-powered, do not require hardware support.
    if (!hasBattery)
        return true;

    // If we can't determine whether a codec has hardware support or not, default to true.
    if (!canLoad_VideoToolbox_VTIsHardwareDecodeSupported())
        return true;

    if (contentTypesRequiringHardwareDecode.isEmpty())
        return true;

    Vector<FourCC> hardwareCodecs = contentTypesToCodecs(contentTypesRequiringHardwareDecode);

    for (auto& codec : codecs) {
        if (hardwareCodecs.contains(codec) && !VTIsHardwareDecodeSupported(codec.value))
            return false;
    }
    return true;
}

bool contentTypeMeetsHardwareDecodeRequirements(const ContentType& contentType, const Vector<ContentType>& contentTypesRequiringHardwareDecode)
{
    Vector<FourCC> codecs = contentTypesToCodecs({ contentType });
    return codecsMeetHardwareDecodeRequirements(codecs, contentTypesRequiringHardwareDecode);
}

bool assetTrackMeetsHardwareDecodeRequirements(AVAssetTrack *track, const Vector<ContentType>& contentTypesRequiringHardwareDecode)
{
    Vector<FourCC> codecs;
    for (NSUInteger i = 0, count = track.formatDescriptions.count; i < count; ++i) {
        CMFormatDescriptionRef description = (__bridge CMFormatDescriptionRef)track.formatDescriptions[i];
        if (PAL::CMFormatDescriptionGetMediaType(description) == kCMMediaType_Video)
            codecs.append(FourCC(PAL::CMFormatDescriptionGetMediaSubType(description)));
    }
    return codecsMeetHardwareDecodeRequirements(codecs, contentTypesRequiringHardwareDecode);
}

}

#endif
