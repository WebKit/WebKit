/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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

#import "GraphicsContextCG.h"
#import "GraphicsContextPlatformPrivateCG.h"
#import "IntRect.h"
#if USE(APPKIT)
#import <AppKit/AppKit.h>
#endif
#import <wtf/StdLibExtras.h>

#if PLATFORM(IOS)
#import "Color.h"
#import "WKGraphics.h"
#endif

#if !PLATFORM(IOS)
#import "LocalCurrentGraphicsContext.h"
#endif
#import "WebCoreSystemInterface.h"

@class NSColor;

// FIXME: More of this should use CoreGraphics instead of AppKit.
// FIXME: More of this should move into GraphicsContextCG.cpp.

namespace WebCore {

// NSColor, NSBezierPath, and NSGraphicsContext
// calls in this file are all exception-safe, so we don't block
// exceptions for those.

#if !PLATFORM(IOS)
static void drawFocusRingToContext(CGContextRef context, CGPathRef focusRingPath)
{
    CGContextBeginPath(context);
    CGContextAddPath(context, focusRingPath);
    wkDrawFocusRing(context, nullptr, 0);
}

static bool drawFocusRingToContextAtTime(CGContextRef context, CGPathRef focusRingPath, double timeOffset)
{
    UNUSED_PARAM(timeOffset);
    CGContextBeginPath(context);
    CGContextAddPath(context, focusRingPath);
    return wkDrawFocusRingAtTime(context, std::numeric_limits<double>::max());
}
#endif // !PLATFORM(IOS)

void GraphicsContext::drawFocusRing(const Path& path, float /* width */, float /* offset */, const Color&)
{
#if PLATFORM(MAC)
    if (paintingDisabled() || path.isNull())
        return;

    drawFocusRingToContext(platformContext(), path.platformPath());
#else
    UNUSED_PARAM(path);
#endif
}

#if PLATFORM(MAC)
void GraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, float offset, double timeOffset, bool& needsRedraw)
{
    if (paintingDisabled())
        return;

    RetainPtr<CGMutablePathRef> focusRingPath = adoptCF(CGPathCreateMutable());
    for (auto& rect : rects)
        CGPathAddRect(focusRingPath.get(), 0, CGRectInset(rect, -offset, -offset));

    needsRedraw = drawFocusRingToContextAtTime(platformContext(), focusRingPath.get(), timeOffset);
}
#endif

void GraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, float, float offset, const Color&)
{
#if !PLATFORM(IOS)
    if (paintingDisabled())
        return;

    RetainPtr<CGMutablePathRef> focusRingPath = adoptCF(CGPathCreateMutable());
    for (auto& rect : rects)
        CGPathAddRect(focusRingPath.get(), 0, CGRectInset(rect, -offset, -offset));

    drawFocusRingToContext(platformContext(), focusRingPath.get());
#else
    UNUSED_PARAM(rects);
    UNUSED_PARAM(offset);
#endif
}

#if !PLATFORM(IOS)
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
#else
static RetainPtr<CGPatternRef> createDotPattern(bool& usingDot, const char* resourceName)
{
    RetainPtr<CGImageRef> image = adoptCF(WKGraphicsCreateImageFromBundleWithName(resourceName));
    ASSERT(image); // if image is not available, we want to know
    usingDot = true;
    return adoptCF(WKCreatePatternFromCGImage(image.get()));
}
#endif // !PLATFORM(IOS)

static NSImage *spellingImage = nullptr;
static NSImage *grammarImage = nullptr;
static NSImage *correctionImage = nullptr;

void GraphicsContext::updateDocumentMarkerResources()
{
    [spellingImage release];
    spellingImage = nullptr;
    [grammarImage release];
    grammarImage = nullptr;
    [correctionImage release];
    correctionImage = nullptr;
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
        case DocumentMarkerSpellingLineStyle:
        {
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
        case DocumentMarkerGrammarLineStyle:
        {
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
        case DocumentMarkerDictationAlternativesLineStyle:
        {
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
        case TextCheckingDictationPhraseWithAlternativesLineStyle:
        {
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
    CGContextSaveGState(context);

#if PLATFORM(IOS)
    WKSetPattern(context, dotPattern, YES, YES);
#endif

    setPatternPhaseInUserSpace(context, offsetPoint);

    CGRect destinationRect = CGRectMake(offsetPoint.x(), offsetPoint.y(), width, patternHeight);
#if !PLATFORM(IOS)
    if (image) {
        // FIXME: Rather than getting the NSImage and then picking the CGImage from it, we should do what iOS does and
        // just load the CGImage in the first place.
        NSRect dotRect = NSMakeRect(offsetPoint.x(), offsetPoint.y(), patternWidth, patternHeight);
        CGImageRef cgImage = [image CGImageForProposedRect:&dotRect context:[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:YES] hints:nullptr];
        CGContextClipToRect(context, destinationRect);
        CGContextDrawTiledImage(context, NSRectToCGRect(dotRect), cgImage);
    } else {
        CGContextSetFillColorWithColor(context, [fallbackColor CGColor]);
        CGContextFillRect(context, destinationRect);
    }
#else
    WKRectFillUsingOperation(context, destinationRect, kCGCompositeSover);
#endif
    
    CGContextRestoreGState(context);
}

CGColorSpaceRef linearRGBColorSpaceRef()
{
    static CGColorSpaceRef linearSRGBSpace = nullptr;

    if (linearSRGBSpace)
        return linearSRGBSpace;

    RetainPtr<NSString> iccProfilePath = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] pathForResource:@"linearSRGB" ofType:@"icc"];
    RetainPtr<NSData> iccProfileData = adoptNS([[NSData alloc] initWithContentsOfFile:iccProfilePath.get()]);

    if (iccProfileData)
        linearSRGBSpace = CGColorSpaceCreateWithICCProfile((CFDataRef)iccProfileData.get());

    // If we fail to load the linearized sRGB ICC profile, fall back to sRGB.
    if (!linearSRGBSpace)
        return sRGBColorSpaceRef();

    return linearSRGBSpace;
}

}
