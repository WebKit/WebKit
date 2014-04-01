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
static void drawFocusRingToContext(CGContextRef context, CGPathRef focusRingPath, CGColorRef color, int radius)
{
    CGContextBeginPath(context);
    CGContextAddPath(context, focusRingPath);
    wkDrawFocusRing(context, color, radius);
}

void GraphicsContext::drawFocusRing(const Path& path, int width, int /*offset*/, const Color& color)
{
    // FIXME: Use 'offset' for something? http://webkit.org/b/49909

    if (paintingDisabled() || path.isNull())
        return;

    int radius = (width - 1) / 2;
    CGColorRef colorRef = color.isValid() ? cachedCGColor(color, ColorSpaceDeviceRGB) : 0;

    drawFocusRingToContext(platformContext(), path.platformPath(), colorRef, radius);
}
#endif // !PLATFORM(IOS)

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int width, int offset, const Color& color)
{
#if !PLATFORM(IOS)
    if (paintingDisabled())
        return;

    int radius = (width - 1) / 2;
    offset += radius;
    CGColorRef colorRef = color.isValid() ? cachedCGColor(color, ColorSpaceDeviceRGB) : 0;

    RetainPtr<CGMutablePathRef> focusRingPath = adoptCF(CGPathCreateMutable());
    unsigned rectCount = rects.size();
    for (unsigned i = 0; i < rectCount; i++)
        CGPathAddRect(focusRingPath.get(), 0, CGRectInset(rects[i], -offset, -offset));

    drawFocusRingToContext(platformContext(), focusRingPath.get(), colorRef, radius);
#else
    UNUSED_PARAM(rects);
    UNUSED_PARAM(width);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(color);
#endif
}


#if !PLATFORM(IOS)
static NSColor* makePatternColor(NSString* firstChoiceName, NSString* secondChoiceName, NSColor* defaultColor, bool& usingDot)
{
    // Eventually we should be able to get rid of the secondChoiceName. For the time being we need both to keep
    // this working on all platforms.
    NSImage *image = [NSImage imageNamed:firstChoiceName];
    if (!image)
        image = [NSImage imageNamed:secondChoiceName];
    ASSERT(image); // if image is not available, we want to know
    NSColor *color = (image ? [NSColor colorWithPatternImage:image] : nil);
    if (color)
        usingDot = true;
    else
        color = defaultColor;
    return color;
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

static NSColor *spellingPatternColor = nullptr;
static NSColor *grammarPatternColor = nullptr;
static NSColor *correctionPatternColor = nullptr;

void GraphicsContext::updateDocumentMarkerResources()
{
    spellingPatternColor = nullptr;
    grammarPatternColor = nullptr;
    correctionPatternColor = nullptr;
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
    NSColor *patternColor;
#else
    CGPatternRef dotPattern;
#endif
    switch (style) {
        case DocumentMarkerSpellingLineStyle:
        {
            // Constants for spelling pattern color.
            static bool usingDotForSpelling = false;
#if !PLATFORM(IOS)
            if (!spellingPatternColor)
                spellingPatternColor = [makePatternColor(@"NSSpellingDot", @"SpellingDot", [NSColor redColor], usingDotForSpelling) retain];
            usingDot = usingDotForSpelling;
            patternColor = spellingPatternColor;
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
            if (!grammarPatternColor)
                grammarPatternColor = [makePatternColor(@"NSGrammarDot", @"GrammarDot", [NSColor greenColor], usingDotForGrammar) retain];
            usingDot = usingDotForGrammar;
            patternColor = grammarPatternColor;
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
            if (!correctionPatternColor)
                correctionPatternColor = [makePatternColor(@"NSCorrectionDot", @"CorrectionDot", [NSColor blueColor], usingDotForSpelling) retain];
            usingDot = usingDotForSpelling;
            patternColor = correctionPatternColor;
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
    // FIXME: This code should not be using wkSetPatternPhaseInUserSpace, as this approach is wrong
    // for transforms.

    // Draw underline.
#if !PLATFORM(IOS)
    LocalCurrentGraphicsContext localContext(this);
    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    CGContextRef context = (CGContextRef)[currentContext graphicsPort];
#else
    CGContextRef context = platformContext();
#endif
    CGContextSaveGState(context);

#if !PLATFORM(IOS)
    [patternColor set];
#else
    WKSetPattern(context, dotPattern, YES, YES);
#endif

    wkSetPatternPhaseInUserSpace(context, offsetPoint);

#if !PLATFORM(IOS)
    NSRectFillUsingOperation(NSMakeRect(offsetPoint.x(), offsetPoint.y(), width, patternHeight), NSCompositeSourceOver);
#else
    WKRectFillUsingOperation(context, CGRectMake(offsetPoint.x(), offsetPoint.y(), width, patternHeight), kCGCompositeSover);
#endif
    
    CGContextRestoreGState(context);
}

#if !PLATFORM(IOS)
CGColorSpaceRef linearRGBColorSpaceRef()
{
    static CGColorSpaceRef linearSRGBSpace = 0;

    if (linearSRGBSpace)
        return linearSRGBSpace;

    RetainPtr<NSString> iccProfilePath = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] pathForResource:@"linearSRGB" ofType:@"icc"];
    RetainPtr<NSData> iccProfileData = adoptNS([[NSData alloc] initWithContentsOfFile:iccProfilePath.get()]);

    if (iccProfileData)
        linearSRGBSpace = CGColorSpaceCreateWithICCProfile((CFDataRef)iccProfileData.get());

    // If we fail to load the linearized sRGB ICC profile, fall back to DeviceRGB.
    if (!linearSRGBSpace)
        return deviceRGBColorSpaceRef();

    return linearSRGBSpace;
}
#endif

}
