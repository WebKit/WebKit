/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "PlatformVideoColorSpace.h"

#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF::TextStream& operator<<(WTF::TextStream& ts, PlatformVideoMatrixCoefficients coefficients)
{
    switch (coefficients) {
    case PlatformVideoMatrixCoefficients::Rgb:
        ts << "rgb"_s;
        break;
    case PlatformVideoMatrixCoefficients::Bt709:
        ts << "bt709"_s;
        break;
    case PlatformVideoMatrixCoefficients::Bt470bg:
        ts << "bt470bg"_s;
        break;
    case PlatformVideoMatrixCoefficients::Smpte170m:
        ts << "smpte170m"_s;
        break;
    case PlatformVideoMatrixCoefficients::Smpte240m:
        ts << "smpte240m"_s;
        break;
    case PlatformVideoMatrixCoefficients::Fcc:
        ts << "fcc"_s;
        break;
    case PlatformVideoMatrixCoefficients::YCgCo:
        ts << "y-cg-co"_s;
        break;
    case PlatformVideoMatrixCoefficients::Bt2020NonconstantLuminance:
        ts << "bt2020-nonconstant-luminance"_s;
        break;
    case PlatformVideoMatrixCoefficients::Bt2020ConstantLuminance:
        ts << "bt2020-constant-luminance"_s;
        break;
    case PlatformVideoMatrixCoefficients::Unspecified:
        ts << "unspecified"_s;
        break;
    }
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, PlatformVideoColorPrimaries primaries)
{
    switch (primaries) {
    case PlatformVideoColorPrimaries::Bt709:
        ts << "bt709"_s;
        break;
    case PlatformVideoColorPrimaries::Bt470bg:
        ts << "bt470bg"_s;
        break;
    case PlatformVideoColorPrimaries::Smpte170m:
        ts << "smpte170m"_s;
        break;
    case PlatformVideoColorPrimaries::Bt470m:
        ts << "bt470m"_s;
        break;
    case PlatformVideoColorPrimaries::Smpte240m:
        ts << "smpte240m"_s;
        break;
    case PlatformVideoColorPrimaries::Film:
        ts << "film"_s;
        break;
    case PlatformVideoColorPrimaries::Bt2020:
        ts << "bt2020"_s;
        break;
    case PlatformVideoColorPrimaries::SmpteSt4281:
        ts << "smpte-st4281"_s;
        break;
    case PlatformVideoColorPrimaries::SmpteRp431:
        ts << "smpte-rp431"_s;
        break;
    case PlatformVideoColorPrimaries::SmpteEg432:
        ts << "smpte-eg432"_s;
        break;
    case PlatformVideoColorPrimaries::JedecP22Phosphors:
        ts << "jedec-p22-phosphors"_s;
        break;
    case PlatformVideoColorPrimaries::Unspecified:
        ts << "unspecified"_s;
        break;
    }
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, PlatformVideoTransferCharacteristics characteristics)
{
    switch (characteristics) {
    case PlatformVideoTransferCharacteristics::Bt709:
        ts << "bt709"_s;
        break;
    case PlatformVideoTransferCharacteristics::Smpte170m:
        ts << "smpte170m"_s;
        break;
    case PlatformVideoTransferCharacteristics::Iec6196621:
        ts << "iec6196621"_s;
        break;
    case PlatformVideoTransferCharacteristics::Gamma22curve:
        ts << "gamma22curve"_s;
        break;
    case PlatformVideoTransferCharacteristics::Gamma28curve:
        ts << "gamma28curve"_s;
        break;
    case PlatformVideoTransferCharacteristics::Smpte240m:
        ts << "smpte240m"_s;
        break;
    case PlatformVideoTransferCharacteristics::Linear:
        ts << "linear"_s;
        break;
    case PlatformVideoTransferCharacteristics::Log:
        ts << "log"_s;
        break;
    case PlatformVideoTransferCharacteristics::LogSqrt:
        ts << "log-sqrt"_s;
        break;
    case PlatformVideoTransferCharacteristics::Iec6196624:
        ts << "iec6196624"_s;
        break;
    case PlatformVideoTransferCharacteristics::Bt1361ExtendedColourGamut:
        ts << "bt1361-extended-colour-gamut"_s;
        break;
    case PlatformVideoTransferCharacteristics::Bt2020_10bit:
        ts << "bt2020_10bit"_s;
        break;
    case PlatformVideoTransferCharacteristics::Bt2020_12bit:
        ts << "bt2020_12bit"_s;
        break;
    case PlatformVideoTransferCharacteristics::SmpteSt2084:
        ts << "smpte-st2084"_s;
        break;
    case PlatformVideoTransferCharacteristics::SmpteSt4281:
        ts << "smpte-st4281"_s;
        break;
    case PlatformVideoTransferCharacteristics::AribStdB67Hlg:
        ts << "arib-std-b67-hlg"_s;
        break;
    case PlatformVideoTransferCharacteristics::Unspecified:
        ts << "unspecified"_s;
        break;
    }
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, PlatformVideoColorSpace colorSpace)
{
    ts.dumpProperty("primaries"_s, colorSpace.primaries);
    ts.dumpProperty("transfer"_s, colorSpace.transfer);
    ts.dumpProperty("matrix"_s, colorSpace.matrix);
    if (colorSpace.fullRange)
        ts.dumpProperty("full-range"_s, *colorSpace.fullRange);
    return ts;
}

}
