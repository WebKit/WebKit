/*
 * Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "GraphicsContext.h"

#import "DisplayListRecorder.h"
#import "GraphicsContextCG.h"
#import "GraphicsContextPlatformPrivateCG.h"
#import "IOSurface.h"
#import "IntRect.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/FeatureFlagsSPI.h>
#import <pal/spi/mac/NSGraphicsSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/StdLibExtras.h>

#if USE(APPKIT)
#import <AppKit/AppKit.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "Color.h"
#import "WKGraphics.h"
#import <pal/ios/UIKitSoftLink.h>
#import <pal/spi/ios/UIKitSPI.h>
#endif

#if PLATFORM(MAC)
#import "LocalCurrentGraphicsContext.h"
#endif

@class NSColor;

// FIXME: More of this should use CoreGraphics instead of AppKit.
// FIXME: More of this should move into GraphicsContextCG.cpp.

namespace WebCore {

// NSColor, NSBezierPath, and NSGraphicsContext calls do not raise exceptions
// so we don't block exceptions.

static RetainPtr<CGColorRef> grammarColor(bool useDarkMode)
{
#if ENABLE(POST_EDITING_GRAMMAR_CHECKING)
    static bool useBlueForGrammar = false;
    static std::once_flag flag;
    std::call_once(flag, [] {
        useBlueForGrammar = os_feature_enabled(TextComposer, PostEditing) && os_feature_enabled(TextComposer, PostEditingUseBlueDots);
    });

    if (useBlueForGrammar)
        return cachedCGColor(useDarkMode ? SRGBA<uint8_t> { 40, 145, 255, 217 } : SRGBA<uint8_t> { 0, 122, 255, 191 });
#endif
    return cachedCGColor(useDarkMode ? SRGBA<uint8_t> { 50, 215, 75, 217 } : SRGBA<uint8_t> { 25, 175, 50, 191 });
}

void GraphicsContextCG::drawFocusRing(const Path& path, float, const Color& color)
{
    if (path.isNull())
        return;

    CGFocusRingStyle focusRingStyle;
#if USE(APPKIT)
    NSInitializeCGFocusRingStyleForTime(NSFocusRingOnly, &focusRingStyle, std::numeric_limits<double>::max());
#else
    focusRingStyle.version = 0;
    focusRingStyle.tint = kCGFocusRingTintBlue;
    focusRingStyle.ordering = kCGFocusRingOrderingNone;
    focusRingStyle.alpha = [PAL::getUIFocusRingStyleClass() maxAlpha];
    focusRingStyle.radius = [PAL::getUIFocusRingStyleClass() borderThickness];
    focusRingStyle.threshold = [PAL::getUIFocusRingStyleClass() alphaThreshold];
    focusRingStyle.bounds = CGRectZero;
#endif

    // We want to respect the CGContext clipping and also not overpaint any
    // existing focus ring. The way to do this is set accumulate to
    // -1. According to CoreGraphics, the reasoning for this behavior has been
    // lost in time.
    focusRingStyle.accumulate = -1;
    auto style = adoptCF(CGStyleCreateFocusRingWithColor(&focusRingStyle, cachedCGColor(color).get()));

    CGContextRef platformContext = this->platformContext();

    CGContextStateSaver stateSaver(platformContext);

    CGContextSetStyle(platformContext, style.get());
    CGContextBeginPath(platformContext);
    CGContextAddPath(platformContext, path.platformPath());

    CGContextFillPath(platformContext);
}

void GraphicsContextCG::drawFocusRing(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
    Path path;
    for (const auto& rect : rects) {
        auto r = rect;
        r.inflate(-outlineOffset);
        path.addRect(r);
    }
    drawFocusRing(path, outlineWidth, color);
}

static inline void setPatternPhaseInUserSpace(CGContextRef context, CGPoint phasePoint)
{
    CGAffineTransform userToBase = getUserToBaseCTM(context);
    CGPoint phase = CGPointApplyAffineTransform(phasePoint, userToBase);

    CGContextSetPatternPhase(context, CGSizeMake(phase.x, phase.y));
}

static RetainPtr<CGColorRef> colorForMarkerLineStyle(DocumentMarkerLineStyleMode style, bool useDarkMode)
{
    switch (style) {
    // Red
    case DocumentMarkerLineStyleMode::Spelling:
        return cachedCGColor(useDarkMode ? SRGBA<uint8_t> { 255, 140, 140, 217 } : SRGBA<uint8_t> { 255, 59, 48, 191 });
    // Blue
    case DocumentMarkerLineStyleMode::DictationAlternatives:
    case DocumentMarkerLineStyleMode::TextCheckingDictationPhraseWithAlternatives:
    case DocumentMarkerLineStyleMode::AutocorrectionReplacement:
        return cachedCGColor(useDarkMode ? SRGBA<uint8_t> { 40, 145, 255, 217 } : SRGBA<uint8_t> { 0, 122, 255, 191 });
    case DocumentMarkerLineStyleMode::Grammar:
        return grammarColor(useDarkMode);
    }
}

void GraphicsContextCG::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    // We want to find the number of full dots, so we're solving the equations:
    // dotDiameter = height
    // dotDiameter / dotGap = 13.247 / 9.457
    // numberOfGaps = numberOfDots - 1
    // dotDiameter * numberOfDots + dotGap * numberOfGaps = width

    auto width = rect.width();
    auto dotDiameter = rect.height();
    auto dotGap = dotDiameter * 9.457 / 13.247;
    auto numberOfDots = (width + dotGap) / (dotDiameter + dotGap);
    auto numberOfWholeDots = static_cast<unsigned>(numberOfDots);
    auto numberOfWholeGaps = numberOfWholeDots - 1;

    // Center the dots
    auto offset = (width - (dotDiameter * numberOfWholeDots + dotGap * numberOfWholeGaps)) / 2;

    auto circleColor = colorForMarkerLineStyle(style.mode, style.shouldUseDarkAppearance);

    CGContextRef platformContext = this->platformContext();
    CGContextStateSaver stateSaver { platformContext };
    CGContextSetFillColorWithColor(platformContext, circleColor.get());
    for (unsigned i = 0; i < numberOfWholeDots; ++i) {
        auto location = rect.location();
        location.move(offset + i * (dotDiameter + dotGap), 0);
        auto size = FloatSize(dotDiameter, dotDiameter);
        CGContextAddEllipseInRect(platformContext, FloatRect(location, size));
    }
    CGContextSetCompositeOperation(platformContext, kCGCompositeSover);
    CGContextFillPath(platformContext);
}

void GraphicsContextCG::convertToDestinationColorSpaceIfNeeded(RetainPtr<CGImageRef>& image)
{
#if HAVE(CORE_ANIMATION_FIX_FOR_RADAR_93560567)
    UNUSED_PARAM(image);
#else
    if (!CGColorSpaceUsesITUR_2100TF(CGImageGetColorSpace(image.get())))
        return;

    auto context = platformContext();

    auto destinationSurface = CGIOSurfaceContextGetSurface(context);
    if (!destinationSurface)
        return;

    auto destinationColorSpace = CGIOSurfaceContextGetColorSpace(context);
    if (!destinationColorSpace)
        return;

    auto surface = IOSurface::createFromSurface(destinationSurface, DestinationColorSpace(destinationColorSpace));

    auto width = CGImageGetWidth(image.get());
    auto height = CGImageGetHeight(image.get());

    auto bitmapContext = surface->createCompatibleBitmap(width, height);
    CGContextDrawImage(bitmapContext.get(), CGRectMake(0, 0, width, height), image.get());

    image = adoptCF(CGBitmapContextCreateImage(bitmapContext.get()));
#endif
}

} // namespace WebCore
