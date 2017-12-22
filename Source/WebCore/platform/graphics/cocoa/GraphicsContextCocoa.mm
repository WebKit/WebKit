/*
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
#import "IntRect.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSGraphicsSPI.h>
#import <wtf/StdLibExtras.h>

#if USE(APPKIT)
#import <AppKit/AppKit.h>
#endif

#if PLATFORM(IOS)
#import "Color.h"
#import "WKGraphics.h"
#import "WKGraphicsInternal.h"
#endif

#if !PLATFORM(IOS)
#import "LocalCurrentGraphicsContext.h"
#endif

@class NSColor;

// FIXME: More of this should use CoreGraphics instead of AppKit.
// FIXME: More of this should move into GraphicsContextCG.cpp.

namespace WebCore {

// NSColor, NSBezierPath, and NSGraphicsContext
// calls in this file are all exception-safe, so we don't block
// exceptions for those.

#if !PLATFORM(IOS)
CGColorRef GraphicsContext::focusRingColor()
{
    static CGColorRef color;
    if (!color) {
        CGFloat colorComponents[] = { 0.5, 0.75, 1.0, 1.0 };
        auto colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        color = CGColorCreate(colorSpace.get(), colorComponents);
    }

    return color;
}

static bool drawFocusRingAtTime(CGContextRef context, NSTimeInterval timeOffset)
{
    CGFocusRingStyle focusRingStyle;
    BOOL needsRepaint = NSInitializeCGFocusRingStyleForTime(NSFocusRingOnly, &focusRingStyle, timeOffset);

    // We want to respect the CGContext clipping and also not overpaint any
    // existing focus ring. The way to do this is set accumulate to
    // -1. According to CoreGraphics, the reasoning for this behavior has been
    // lost in time.
    focusRingStyle.accumulate = -1;
    auto style = adoptCF(CGStyleCreateFocusRingWithColor(&focusRingStyle, GraphicsContext::focusRingColor()));

    CGContextSaveGState(context);
    CGContextSetStyle(context, style.get());
    CGContextFillPath(context);
    CGContextRestoreGState(context);

    return needsRepaint;
}

static void drawFocusRing(CGContextRef context)
{
    drawFocusRingAtTime(context, std::numeric_limits<double>::max());
}

static void drawFocusRingToContext(CGContextRef context, CGPathRef focusRingPath)
{
    CGContextBeginPath(context);
    CGContextAddPath(context, focusRingPath);
    drawFocusRing(context);
}

static bool drawFocusRingToContextAtTime(CGContextRef context, CGPathRef focusRingPath, double timeOffset)
{
    UNUSED_PARAM(timeOffset);
    CGContextBeginPath(context);
    CGContextAddPath(context, focusRingPath);
    return drawFocusRingAtTime(context, std::numeric_limits<double>::max());
}
#endif // !PLATFORM(IOS)

void GraphicsContext::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
#if PLATFORM(MAC)
    if (paintingDisabled() || path.isNull())
        return;

    if (m_impl) {
        m_impl->drawFocusRing(path, width, offset, color);
        return;
    }

    drawFocusRingToContext(platformContext(), path.platformPath());
#else
    UNUSED_PARAM(path);
    UNUSED_PARAM(width);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(color);
#endif
}

#if PLATFORM(MAC)
void GraphicsContext::drawFocusRing(const Path& path, double timeOffset, bool& needsRedraw)
{
    if (paintingDisabled() || path.isNull())
        return;

    if (m_impl) // FIXME: implement animated focus ring drawing.
        return;

    needsRedraw = drawFocusRingToContextAtTime(platformContext(), path.platformPath(), timeOffset);
}

void GraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, double timeOffset, bool& needsRedraw)
{
    if (paintingDisabled())
        return;

    if (m_impl) // FIXME: implement animated focus ring drawing.
        return;

    RetainPtr<CGMutablePathRef> focusRingPath = adoptCF(CGPathCreateMutable());
    for (const auto& rect : rects)
        CGPathAddRect(focusRingPath.get(), 0, CGRect(rect));

    needsRedraw = drawFocusRingToContextAtTime(platformContext(), focusRingPath.get(), timeOffset);
}
#endif

void GraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
#if !PLATFORM(IOS)
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawFocusRing(rects, width, offset, color);
        return;
    }

    RetainPtr<CGMutablePathRef> focusRingPath = adoptCF(CGPathCreateMutable());
    for (auto& rect : rects)
        CGPathAddRect(focusRingPath.get(), 0, CGRectInset(rect, -offset, -offset));

    drawFocusRingToContext(platformContext(), focusRingPath.get());
#else
    UNUSED_PARAM(rects);
    UNUSED_PARAM(width);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(color);
#endif
}

#if PLATFORM(MAC)
static NSImage *findImage(NSString* firstChoiceName, NSString* secondChoiceName, bool& usingDot)
{
    // Eventually we should be able to get rid of the secondChoiceName. For the time being we need both to keep
    // this working on all platforms.
    NSImage *image = [NSImage imageNamed:firstChoiceName];
    if (!image)
        image = [NSImage imageNamed:secondChoiceName];
    ASSERT(image); // if image is not available, we want to know
    usingDot = image;
    return image;
}
static NSImage *spellingImage = nullptr;
static NSImage *grammarImage = nullptr;
static NSImage *correctionImage = nullptr;
#else
static RetainPtr<CGPatternRef> createDotPattern(bool& usingDot, const char* resourceName)
{
    RetainPtr<CGImageRef> image = adoptCF(WKGraphicsCreateImageFromBundleWithName(resourceName));
    ASSERT(image); // if image is not available, we want to know
    usingDot = true;
    return adoptCF(WKCreatePatternFromCGImage(image.get()));
}
#endif // PLATFORM(MAC)

void GraphicsContext::updateDocumentMarkerResources()
{
#if PLATFORM(MAC)
    [spellingImage release];
    spellingImage = nullptr;
    [grammarImage release];
    grammarImage = nullptr;
    [correctionImage release];
    correctionImage = nullptr;
#endif
}

static inline void setPatternPhaseInUserSpace(CGContextRef context, CGPoint phasePoint)
{
    CGAffineTransform userToBase = getUserToBaseCTM(context);
    CGPoint phase = CGPointApplyAffineTransform(phasePoint, userToBase);

    CGContextSetPatternPhase(context, CGSizeMake(phase.x, phase.y));
}

// WebKit on Mac is a standard platform component, so it must use the standard platform artwork for underline.
void GraphicsContext::drawLineForDocumentMarker(const FloatPoint& point, float width, DocumentMarkerLineStyle style)
{
    if (paintingDisabled())
        return;

    // These are the same for misspelling or bad grammar.
    int patternHeight = cMisspellingLineThickness;
    float patternWidth = cMisspellingLinePatternWidth;

    bool usingDot;
#if !PLATFORM(IOS)
    NSImage *image;
    NSColor *fallbackColor;
#else
    CGPatternRef dotPattern;
#endif
    switch (style) {
    case DocumentMarkerSpellingLineStyle: {
        // Constants for spelling pattern color.
        static bool usingDotForSpelling = false;
#if !PLATFORM(IOS)
        if (!spellingImage)
            spellingImage = [findImage(@"NSSpellingDot", @"SpellingDot", usingDotForSpelling) retain];
        image = spellingImage;
        fallbackColor = [NSColor redColor];
#else
        static CGPatternRef spellingPattern = createDotPattern(usingDotForSpelling, "SpellingDot").leakRef();
        dotPattern = spellingPattern;
#endif
        usingDot = usingDotForSpelling;
        break;
    }
    case DocumentMarkerGrammarLineStyle: {
#if !PLATFORM(IOS)
        // Constants for grammar pattern color.
        static bool usingDotForGrammar = false;
        if (!grammarImage)
            grammarImage = [findImage(@"NSGrammarDot", @"GrammarDot", usingDotForGrammar) retain];
        usingDot = grammarImage;
        image = grammarImage;
        fallbackColor = [NSColor greenColor];
        break;
#else
        ASSERT_NOT_REACHED();
        return;
#endif
    }
#if PLATFORM(MAC)
    // To support correction panel.
    case DocumentMarkerAutocorrectionReplacementLineStyle:
    case DocumentMarkerDictationAlternativesLineStyle: {
        // Constants for spelling pattern color.
        static bool usingDotForSpelling = false;
        if (!correctionImage)
            correctionImage = [findImage(@"NSCorrectionDot", @"CorrectionDot", usingDotForSpelling) retain];
        usingDot = usingDotForSpelling;
        image = correctionImage;
        fallbackColor = [NSColor blueColor];
        break;
    }
#endif
#if PLATFORM(IOS)
    case TextCheckingDictationPhraseWithAlternativesLineStyle: {
        static bool usingDotForDictationPhraseWithAlternatives = false;
        static CGPatternRef dictationPhraseWithAlternativesPattern = createDotPattern(usingDotForDictationPhraseWithAlternatives, "DictationPhraseWithAlternativesDot").leakRef();
        dotPattern = dictationPhraseWithAlternativesPattern;
        usingDot = usingDotForDictationPhraseWithAlternatives;
        break;
    }
#endif // PLATFORM(IOS)
    default:
#if PLATFORM(IOS)
        // FIXME: Should remove default case so we get compile-time errors.
        ASSERT_NOT_REACHED();
#endif // PLATFORM(IOS)
        return;
    }
    
    FloatPoint offsetPoint = point;

    // Make sure to draw only complete dots.
    if (usingDot) {
        // allow slightly more considering that the pattern ends with a transparent pixel
        float widthMod = fmodf(width, patternWidth);
        if (patternWidth - widthMod > cMisspellingLinePatternGapWidth) {
            float gapIncludeWidth = 0;
            if (width > patternWidth)
                gapIncludeWidth = cMisspellingLinePatternGapWidth;
            offsetPoint.move(floor((widthMod + gapIncludeWidth) / 2), 0);
            width -= widthMod;
        }
    }
    
    // FIXME: This code should not use NSGraphicsContext currentContext
    // In order to remove this requirement we will need to use CGPattern instead of NSColor
    // FIXME: This code should not be using setPatternPhaseInUserSpace, as this approach is wrong
    // for transforms.

    // Draw underline.
    CGContextRef context = platformContext();
    CGContextStateSaver stateSaver { context };

#if PLATFORM(IOS)
    WKSetPattern(context, dotPattern, YES, YES);
#endif

    setPatternPhaseInUserSpace(context, offsetPoint);

    CGRect destinationRect = CGRectMake(offsetPoint.x(), offsetPoint.y(), width, patternHeight);
#if !PLATFORM(IOS)
    if (image) {
        CGContextClipToRect(context, destinationRect);

        // We explicitly flip coordinates so as to ensure we paint the image right-side up. We do this because
        // -[NSImage CGImageForProposedRect:context:hints:] does not guarantee that the returned image will respect
        // any transforms applied to the context or any specified hints.
        CGContextTranslateCTM(context, 0, patternHeight);
        CGContextScaleCTM(context, 1, -1);

        NSRect dotRect = NSMakeRect(offsetPoint.x(), patternHeight - offsetPoint.y(), patternWidth, patternHeight); // Adjust y position as we flipped coordinates.

        // FIXME: Rather than getting the NSImage and then picking the CGImage from it, we should do what iOS does and
        // just load the CGImage in the first place.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CGImageRef cgImage = [image CGImageForProposedRect:&dotRect context:[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO] hints:nullptr];
#pragma clang diagnostic pop
        CGContextDrawTiledImage(context, NSRectToCGRect(dotRect), cgImage);
    } else {
        CGContextSetFillColorWithColor(context, [fallbackColor CGColor]);
        CGContextFillRect(context, destinationRect);
    }
#else
    WKRectFillUsingOperation(context, destinationRect, kCGCompositeSover);
#endif
}

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101200
CGColorSpaceRef linearRGBColorSpaceRef()
{
    static CGColorSpaceRef linearSRGBColorSpace;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        RetainPtr<NSString> iccProfilePath = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] pathForResource:@"linearSRGB" ofType:@"icc"];
        RetainPtr<NSData> iccProfileData = adoptNS([[NSData alloc] initWithContentsOfFile:iccProfilePath.get()]);
        if (iccProfileData)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            linearSRGBColorSpace = CGColorSpaceCreateWithICCProfile((CFDataRef)iccProfileData.get());
#pragma clang diagnostic pop

        // If we fail to load the linearized sRGB ICC profile, fall back to sRGB.
        if (!linearSRGBColorSpace)
            linearSRGBColorSpace = sRGBColorSpaceRef();
    });
    return linearSRGBColorSpace;
}
#endif

} // namespace WebCore
