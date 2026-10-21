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

#include "config.h"
#include "PrintColor.h"

#if USE(CG)

#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

std::optional<PrintColor> PrintColor::create(CGColorRef color)
{
    if (!color)
        return std::nullopt;

    RetainPtr colorSpace = CGColorGetColorSpace(color);
    if (CGColorSpaceGetType(colorSpace.get()) != kCGColorSpaceDeviceCMYK || CGColorGetNumberOfComponents(color) != 5)
        return std::nullopt;

    auto components = unsafeMakeSpan(CGColorGetComponents(color), 5);
    return PrintColor { DeviceCMYK { static_cast<float>(components[0]), static_cast<float>(components[1]), static_cast<float>(components[2]), static_cast<float>(components[3]) }, static_cast<float>(CGColorGetAlpha(color)) };
}

RetainPtr<CGColorRef> PrintColor::createCGColor() const
{
    return WTF::switchOn(color,
        [&](const DeviceCMYK& cmyk) {
            static NeverDestroyed<RetainPtr<CGColorSpaceRef>> deviceCMYKColorSpace = adoptCF(CGColorSpaceCreateDeviceCMYK());
            std::array<CGFloat, 5> components { cmyk.cyan, cmyk.magenta, cmyk.yellow, cmyk.black, alpha };
            return adoptCF(CGColorCreate(deviceCMYKColorSpace->get(), components.data()));
        }
    );
}

Color PrintColor::equivalentColor() const
{
    return Color::createAndPreserveColorSpace(createCGColor().get());
}

} // namespace WebCore

#endif // USE(CG)
