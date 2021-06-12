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
#include "DestinationColorSpace.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>

#if USE(CG)
#include "ColorSpaceCG.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#endif

namespace WebCore {

#if USE(CG)
using KnownColorSpaceAccessor = CGColorSpaceRef();
template<KnownColorSpaceAccessor accessor> static const DestinationColorSpace& knownColorSpace()
{
    static LazyNeverDestroyed<DestinationColorSpace> colorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        colorSpace.construct(accessor());
    });
    return colorSpace.get();
}
#else
template<PlatformColorSpace::Name name> static const DestinationColorSpace& knownColorSpace()
{
    static NeverDestroyed<DestinationColorSpace> colorSpace { name };
    return colorSpace.get();
}
#endif

const DestinationColorSpace& DestinationColorSpace::SRGB()
{
#if USE(CG)
    return knownColorSpace<sRGBColorSpaceRef>();
#else
    return knownColorSpace<PlatformColorSpace::Name::SRGB>();
#endif
}

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
const DestinationColorSpace& DestinationColorSpace::LinearSRGB()
{
#if USE(CG)
    return knownColorSpace<linearSRGBColorSpaceRef>();
#else
    return knownColorSpace<PlatformColorSpace::Name::LinearSRGB>();
#endif
}
#endif

#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
const DestinationColorSpace& DestinationColorSpace::DisplayP3()
{
#if USE(CG)
    return knownColorSpace<displayP3ColorSpaceRef>();
#else
    return knownColorSpace<PlatformColorSpace::Name::DisplayP3>();
#endif
}
#endif

DestinationColorSpace::DestinationColorSpace(PlatformColorSpace platformColorSpace)
    : m_platformColorSpace { WTFMove(platformColorSpace) }
{
#if USE(CG)
    ASSERT(m_platformColorSpace);
#endif
}

bool operator==(const DestinationColorSpace& a, const DestinationColorSpace& b)
{
#if USE(CG)
    return CGColorSpaceEqualToColorSpace(a.platformColorSpace(), b.platformColorSpace());
#else
    return a.platformColorSpace() == b.platformColorSpace();
#endif
}

bool operator!=(const DestinationColorSpace& a, const DestinationColorSpace& b)
{
    return !(a == b);
}

TextStream& operator<<(TextStream& ts, const DestinationColorSpace& colorSpace)
{
    if (colorSpace == DestinationColorSpace::SRGB())
        ts << "sRGB";
#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
    else if (colorSpace == DestinationColorSpace::LinearSRGB())
        ts << "LinearSRGB";
#endif
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
    else if (colorSpace == DestinationColorSpace::DisplayP3())
        ts << "DisplayP3";
#endif
#if USE(CG)
    else if (auto description = adoptCF(CGColorSpaceCopyICCProfileDescription(colorSpace.platformColorSpace())))
        ts << String(description.get());
#endif

    return ts;
}

}
