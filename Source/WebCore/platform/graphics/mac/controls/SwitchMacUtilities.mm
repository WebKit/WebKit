/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "SwitchMacUtilities.h"

#if PLATFORM(MAC)

#import "ControlFactoryMac.h"
#import "FloatRoundedRect.h"
#import "GraphicsContext.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>

namespace WebCore::SwitchMacUtilities {

static IntSize cellSize(NSControlSize controlSize)
{
    static const std::array<IntSize, 4> sizes =
    {
        IntSize { 38, 22 },
        IntSize { 32, 18 },
        IntSize { 26, 15 },
        IntSize { 38, 22 }
    };
    return sizes[controlSize];
}

static IntOutsets cellOutsets(NSControlSize controlSize)
{
    static const IntOutsets margins[] =
    {
        // top right bottom left
        { 2, 2, 1, 2 },
        { 2, 2, 1, 2 },
        { 1, 1, 0, 1 },
        { 2, 2, 1, 2 },
    };
    return margins[controlSize];
}

static FloatRect rectForBounds(const FloatRect& bounds)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return bounds;
}

static NSString *coreUISizeForControlSize(const NSControlSize controlSize)
{
    if (controlSize == NSControlSizeMini)
        return (__bridge NSString *)kCUISizeMini;
    if (controlSize == NSControlSizeSmall)
        return (__bridge NSString *)kCUISizeSmall;
    return (__bridge NSString *)kCUISizeRegular;
}

static float easeInOut(const float progress)
{
    return -2.0f * pow(progress, 3.0f) + 3.0f * pow(progress, 2.0f);
}

} // namespace WebCore::SwitchMacUtilities

#endif // PLATFORM(MAC)
