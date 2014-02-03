/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#import "WebNSStringDrawing.h"

#if PLATFORM(IOS)

#import "EmojiFallbackFontSelector.h"
#import <CoreFoundation/CFPriv.h>
#import <CoreGraphics/CGColor.h>
#import <float.h>
#import <sys/types.h>
#import <unicode/ubrk.h>
#import <unicode/uchar.h>
#import <unicode/utf.h>
#import <WebCore/BidiResolver.h>
#import <WebCore/break_lines.h>
#import <WebCore/Font.h>
#import <WebCore/FontCache.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/StringTruncator.h>
#import <WebCore/TextBreakIterator.h>
#import <WebCore/TextRun.h>
#import <WebCore/WKGraphics.h>
#import <WebCore/WebCoreSystemInterface.h>
#import <wtf/text/WTFString.h>
#import <wtf/unicode/CharacterNames.h>

using namespace WebCore;
using namespace std;

static BOOL ascentRoundingEnabled = NO;

static BOOL wordRoundingEnabled;

static BOOL wordRoundingAllowed = YES;

static inline bool linkedOnOrAfterIPhoneOS3()
{
    static bool s_linkedOnOrAfterIOS3 = iosExecutableWasLinkedOnOrAfterVersion(wkIOSSystemVersion_3_0);
    return s_linkedOnOrAfterIOS3;
}

static inline bool shouldDisableWordRounding()
{
    // Note that even when rounding hacks are not disabled at this level, they will only be applied
    // if +[WebView _setAllowsRoundingHacks:YES] is called. Thus, in iOS 5 and later, rounding hacks are never
    // applied (see WebKitInitialize() in WebUIKitSupport.mm).

    if (!wordRoundingAllowed)
        return true;

    if (linkedOnOrAfterIPhoneOS3())
        return false;

    return !wordRoundingEnabled;
}

static inline int boundedTextBreakFollowing(TextBreakIterator* it, int offset, int length)
{
    int result = textBreakFollowing(it, offset);
    return result == TextBreakDone ? length : result;
}

static inline Font rendererForFont( GSFontRef font )
{
    GSFontTraitMask traits = GSFontGetSynthesizedTraits(font);
    float size = GSFontGetSize(font);

    static EmojiFallbackFontSelector* fontSelector = EmojiFallbackFontSelector::create().leakRef();

    FontPlatformData platformData(font, size, true, traits & GSBoldFontMask, traits & GSItalicFontMask);
    Font renderer(platformData, PassRefPtr<FontSelector>(fontSelector));
    
    return renderer;
}

static String applyEllipsisStyle(const String& srcString, WebEllipsisStyle style, float width, const Font& font, StringTruncator::EnableRoundingHacksOrNot enableRoundingHacks, float* resultWidth, bool insertEllipsis = true, float customTruncationElementWidth = 0, bool alwaysTruncate = false)
{
    // If it is WebEllipsisStyleClip, WebEllipsisStyleWordWrap, or WebEllipsisStyleCharacterWrap,
    // and we don't have any confine on the width, then it is the same as WebEllipsisStyleNone.
    if (style >= WebEllipsisStyleClip && width >= FLT_MAX)
        style = WebEllipsisStyleNone;

    float truncatedWidth = width;
    String truncatedString(srcString);

    switch (style) {
    case WebEllipsisStyleNone:
        truncatedWidth = StringTruncator::width(srcString, font, enableRoundingHacks);
        break;
    case WebEllipsisStyleHead:
        truncatedString = StringTruncator::leftTruncate(srcString, width, font, enableRoundingHacks, truncatedWidth, insertEllipsis, customTruncationElementWidth);
        break;
    case WebEllipsisStyleTail:
        truncatedString = StringTruncator::rightTruncate(srcString, width, font, enableRoundingHacks, truncatedWidth, insertEllipsis, customTruncationElementWidth);
        break;
    case WebEllipsisStyleCenter:
        truncatedString = StringTruncator::centerTruncate(srcString, width, font, enableRoundingHacks, truncatedWidth, insertEllipsis, customTruncationElementWidth);
        break;

    // Character wrap is the same as clipping for a single line, since we can't clip mid-character.
    case WebEllipsisStyleCharacterWrap:
    case WebEllipsisStyleClip:
        // If we were given a specific width to draw in, we shouldn't draw larger than that.
        truncatedString = StringTruncator::rightClipToCharacter(srcString, width, font, enableRoundingHacks, truncatedWidth, insertEllipsis, customTruncationElementWidth);
        break;
    case WebEllipsisStyleWordWrap:
        // If we were given a specific width to draw in, we shouldn't draw larger than that.
        truncatedString = StringTruncator::rightClipToWord(srcString, width, font, enableRoundingHacks, truncatedWidth, insertEllipsis, customTruncationElementWidth, alwaysTruncate);
        break;
    }

    if (resultWidth)
        *resultWidth = truncatedWidth;
    
    return truncatedString;
}

static float drawAtPoint(const UChar* str, const int runLength, const FloatPoint& point, const Font& renderer, GraphicsContext* context, bool drawUnderLine = false, BidiStatus* status = 0, int length = -1)
{
    TextRun textRun(str, runLength);
    if (shouldDisableWordRounding())
        textRun.disableRoundingHacks();

    float width = 0;
    if (!status)
        width = renderer.drawText(context, textRun, point);
    else
        width = context->drawBidiText(renderer, textRun, point, WebCore::Font::DoNotPaintIfFontNotReady, status, length);

    if (drawUnderLine)
        context->drawLineForText(point, width, false);
    return width;
}

static bool needsBidiLayout(const UChar *characters, unsigned length, UCharDirection& baseDirection, bool oneParagraph = false)
{
    bool foundFirstStrong = false;
    bool result = false;
    baseDirection = U_LEFT_TO_RIGHT;
    for (unsigned i = 0; i < length;) {
        UChar32 c;
        U16_NEXT(characters, i, length, c);
        switch (UCharDirection(c)) {
            case U_RIGHT_TO_LEFT:
            case U_RIGHT_TO_LEFT_ARABIC:
                if (!foundFirstStrong) {
                    foundFirstStrong = true;
                    baseDirection = U_RIGHT_TO_LEFT;
                }
                FALLTHROUGH; // To the rest of strongly directional cases.
            case U_LEFT_TO_RIGHT_EMBEDDING:
            case U_LEFT_TO_RIGHT_OVERRIDE:
            case U_RIGHT_TO_LEFT_EMBEDDING:
            case U_RIGHT_TO_LEFT_OVERRIDE:
            case U_POP_DIRECTIONAL_FORMAT:
                result = true;
                if (foundFirstStrong)
                    return result;
                break;
            case U_LEFT_TO_RIGHT:
                foundFirstStrong = true;
                if (result)
                    return result;
                break;
            case U_BLOCK_SEPARATOR:
                if (oneParagraph)
                    return result;
                break;
            default:
                break;
        }
    }
    return result;
}

@implementation NSString (WebStringDrawing)

+ (void)_web_setWordRoundingEnabled:(BOOL)flag
{
    if (!linkedOnOrAfterIPhoneOS3())
        wordRoundingEnabled = flag;
}

+ (BOOL)_web_wordRoundingEnabled
{
    return wordRoundingEnabled;
}

+ (void)_web_setWordRoundingAllowed:(BOOL)flag
{
    wordRoundingAllowed = flag;
}

+ (BOOL)_web_wordRoundingAllowed
{
    return wordRoundingAllowed;
}

+ (void)_web_setAscentRoundingEnabled:(BOOL)flag
{
    ascentRoundingEnabled = flag;
}

+ (BOOL)_web_ascentRoundingEnabled
{
    return ascentRoundingEnabled;
}

#pragma mark Internal primitives used for string drawing/sizing

// These methods should not be called from outside of this file

- (CGSize)__web_drawAtPoint:(CGPoint)point
                   forWidth:(CGFloat)width
                   withFont:(GSFontRef)font
                   ellipsis:(WebEllipsisStyle)ellipsisStyle
              letterSpacing:(float)letterSpacing
               includeEmoji:(BOOL)includeEmoji
                measureOnly:(BOOL)measureOnly
          renderedStringOut:(NSString **)renderedStringOut
              drawUnderline:(BOOL)drawUnderline
{
    if (width < 0) {
#ifndef NDEBUG
        fprintf(stderr, "%s: width must be greater than zero.\n", __FUNCTION__);
#endif
        return CGSizeZero;
    }
    
    if (font == NULL) {
#ifndef NDEBUG
        fprintf(stderr, "%s: font must be non-null.\n", __FUNCTION__);
#endif
        return CGSizeZero;
    }
    
    if ([self isEqualToString:@""]) {
        return CGSizeZero;
    }

    FontCachePurgePreventer fontCachePurgePreventer;

    Font renderer = rendererForFont(font);
    if (letterSpacing != 0.0)
        renderer.setLetterSpacing(letterSpacing);

    String fullString(self);
    UCharDirection base = U_LEFT_TO_RIGHT;
    bool stringNeedsBidi = needsBidiLayout(fullString.deprecatedCharacters(), fullString.length(), base);
    float stringWidth;
    String s = (width >= FLT_MAX && !measureOnly) ? fullString : applyEllipsisStyle(fullString, ellipsisStyle, width, renderer, shouldDisableWordRounding() ? StringTruncator::DisableRoundingHacks : StringTruncator::EnableRoundingHacks, &stringWidth);
    
    if (!measureOnly) {
        CGContextRef cgContext= WKGetCurrentGraphicsContext();
        GraphicsContext context(static_cast<PlatformGraphicsContext*>(cgContext), false);
        context.setEmojiDrawingEnabled(includeEmoji);

        if (stringNeedsBidi) {
            BidiStatus status(base, base, base, BidiContext::create((base == U_LEFT_TO_RIGHT) ? 0 : 1, base, false));
            // FIXME: For proper bidi rendering, we need to pass the whole string, rather than the truncated string.
            // Otherwise, weak/neutral characters on either side of the ellipsis may pick up the wrong direction
            // if there are strong characters ellided.
            stringWidth = drawAtPoint(s.deprecatedCharacters(), s.length(), point, renderer, &context, drawUnderline, &status);
        } else {
            stringWidth = drawAtPoint(s.deprecatedCharacters(), s.length(), point, renderer, &context, drawUnderline);
        }
    }
    
    if (renderedStringOut)
        *renderedStringOut = (NSString *)s;

    return CGSizeMake(stringWidth, GSFontGetLineSpacing(font));
}

- (CGSize)__web_drawAtPoint:(CGPoint)point
                   forWidth:(CGFloat)width
                   withFont:(GSFontRef)font
                   ellipsis:(WebEllipsisStyle)ellipsisStyle
              letterSpacing:(float)letterSpacing
               includeEmoji:(BOOL)includeEmoji
                measureOnly:(BOOL)measureOnly
          renderedStringOut:(NSString **)renderedStringOut
{
    return [self __web_drawAtPoint:point
                          forWidth:width
                          withFont:font
                          ellipsis:ellipsisStyle
                     letterSpacing:letterSpacing
                      includeEmoji:includeEmoji
                       measureOnly:measureOnly
                 renderedStringOut:renderedStringOut
                     drawUnderline:NO];
}

- (CGSize)__web_drawAtPoint:(CGPoint)point
                   forWidth:(CGFloat)width
                   withFont:(GSFontRef)font
                   ellipsis:(WebEllipsisStyle)ellipsisStyle
              letterSpacing:(float)letterSpacing
               includeEmoji:(BOOL)includeEmoji
                measureOnly:(BOOL)measureOnly
{
    return [self __web_drawAtPoint:point
                          forWidth:width
                          withFont:font
                          ellipsis:ellipsisStyle
                     letterSpacing:letterSpacing
                      includeEmoji:includeEmoji
                       measureOnly:measureOnly
                 renderedStringOut:nil];
}

- (CGSize)__web_drawInRect:(CGRect)rect
                  withFont:(GSFontRef)font
                  ellipsis:(WebEllipsisStyle)ellipsisStyle
                 alignment:(WebTextAlignment)alignment
             letterSpacing:(CGFloat)letterSpacing
               lineSpacing:(CGFloat)lineSpacing
              includeEmoji:(BOOL)includeEmoji
            truncationRect:(CGRect *)truncationRect
               measureOnly:(BOOL)measureOnly
         renderedStringOut:(NSString **)renderedStringOut
             drawUnderline:(BOOL)drawUnderline
{    
    if (rect.size.width < 0) {
#ifndef NDEBUG
        fprintf(stderr, "%s: width must be greater than zero.\n", __FUNCTION__);
#endif
        return CGSizeZero;
    }
    
    if (font == NULL) {
#ifndef NDEBUG
        fprintf(stderr, "%s: font must be non-null.\n", __FUNCTION__);
#endif
        return CGSizeZero;
    }    

    String renderedString = (renderedStringOut ? String("") : String());

    FontCachePurgePreventer fontCachePurgePreventer;

    Font renderer = rendererForFont(font);
    renderer.setLetterSpacing(letterSpacing);
    String drawString = String(self);
    if (drawString.contains('\r')) {
        drawString.replace("\r\n", "\n");
        drawString.replace('\r', '\n');
    }
    int length = drawString.length();
    
    CGPoint textPoint;
    CGPoint cursor = rect.origin;
    BOOL hasCustomLinespacing = YES;
    if (lineSpacing == 0.0) {
        lineSpacing = GSFontGetLineSpacing(font);
        hasCustomLinespacing = NO;
    }
    float ascent = GSFontGetAscent(font);
    if (ascentRoundingEnabled) {
        // This opt-in ascent rounding mode matches standard WebKit text rendering and is used by clients that want
        // to match, like a placeholder label inside a textfield that needs to line up exactly with text in the 
        // WebView that will eventually replace it.
        ascent = ceilf(ascent);
    } else {
        // For all other drawing, we round to the nearest 1/128 to avoid inaccurate results in the computations below, 
        // due to limited floating-point precision.  We round to the nearest 1/128 because a) it is a power of two, 
        // and you can exactly multiply integers by a power of 2, and b) it is small enough to not effect rendering 
        // at any resolution that we will conceivably use.  See <rdar://problem/8102100>.
        ascent = ceilf(ascent * 128) / 128.0f;
    }
    float maxLineWidth = 0;
    cursor.y += ascent;

    const UniChar *buffer = drawString.deprecatedCharacters();
    const UniChar *startOfLine = buffer;

    BOOL lastLine = NO;
    BOOL finishedLastLine = NO;
    UCharDirection base;
    BOOL paragraphNeedsBidi = needsBidiLayout(buffer, length, base, true);
    BidiStatus status;
    if (paragraphNeedsBidi)
        status = BidiStatus(base, base, base, BidiContext::create((base == U_LEFT_TO_RIGHT) ? 0 : 1, base, false));

    int lengthRemaining = length;
    
    while (lengthRemaining > 0 && !finishedLastLine) {
        if ((cursor.y - rect.origin.y) + lineSpacing > rect.size.height)
            lastLine = YES;
        
        BOOL reachedEndOfLine = NO;
        BOOL foundNewline = NO;

        UniChar *pos = const_cast<UniChar*>(startOfLine);
        float lineWidth = 0.0f; 
        float trailingSpaceWidth = 0.0f; // Width of trailing space(s) on line, if any
        int trailingSpaceCount = 0; // Count of trailing space(s) on line, if any
        
        BOOL lastLineEllipsed = lastLine && (ellipsisStyle >= WebEllipsisStyleHead && ellipsisStyle <= WebEllipsisStyleCenter);
        BOOL lastLineClipped = lastLine && (ellipsisStyle >= WebEllipsisStyleClip);
        int skippedWhitespace = 0;
        
        while (!(lastLineEllipsed || lastLineClipped) && lengthRemaining > 0 && !reachedEndOfLine) {
            int breakPos = 0;
            
            if (ellipsisStyle != WebEllipsisStyleCharacterWrap) {
                //FIXME: <rdar://problem/12457790> investigate perf impact after merging TOT r129662.
                LazyLineBreakIterator breakIterator(String(pos, lengthRemaining));
                breakPos = nextBreakablePosition(breakIterator, 0);
            }

            // FIXME: This code currently works because there are no characters with these break or whitespace
            // properties outside plane 0. If that ever changes, surrogate pair handling will be needed everywhere
            // below where the width or a break or space is assumed to be 1.
            BOOL foundSpace = NO;

            if (breakPos < lengthRemaining) {
                ULineBreak breakProperty = (ULineBreak) u_getIntPropertyValue(pos[breakPos], UCHAR_LINE_BREAK);
                switch (breakProperty) {
                    case U_LB_SPACE:
                    case U_LB_ZWSPACE:
                        foundSpace = YES;
                        break;
                    case U_LB_LINE_FEED:
                    case U_LB_NEXT_LINE:
                    case U_LB_MANDATORY_BREAK:
                    case U_LB_CARRIAGE_RETURN:
                        foundNewline = YES;
                        break;
                    default:
                        if (ellipsisStyle == WebEllipsisStyleCharacterWrap) {
                            // We can break on characters. We should do something smarter, like split the length of the entire line 
                            // Don't, however, break in the middle of a character.
                            NonSharedCharacterBreakIterator it(StringView(pos, lengthRemaining));
                            breakPos = boundedTextBreakFollowing(it, 0, lengthRemaining);
                        }                        
                        break;
                }                
            }

            // NSLog(@"apparently the break is %x", pos[breakPos]);

            // ICU reports the break to us as the position just before the character it gives us (between two characters).
            // measure everything up to the break
            TextRun textRun(pos, breakPos);
            if (shouldDisableWordRounding())
                textRun.disableRoundingHacks();
            float wordWidth = renderer.width(textRun);

            // NSLog(@"measuring at pos = %d len = %d, size = %f", pos - startOfLine, breakPos, wordWidth);
            if (lineWidth + wordWidth <= rect.size.width) {               
                // advance to the break character.
                pos += breakPos;
                lengthRemaining = length - (pos - buffer);
                lineWidth += wordWidth;
                
                // Reset trailing spaces if there was a real word.
                if (breakPos > 0) {
                    trailingSpaceWidth = 0.0f;
                    trailingSpaceCount = 0;
                }

                reachedEndOfLine = foundNewline;
                // don't ask the renderer to draw the line break character itself - skip ahead.
                if (foundNewline) {
                    pos++;
                    lengthRemaining--;
                    skippedWhitespace++;
                } else if (foundSpace) {
                    if (lengthRemaining > 0) {
                        // measure space break width
                        TextRun textRun(pos, 1);
                        if (shouldDisableWordRounding())
                            textRun.disableRoundingHacks();

                        float spaceWidth = renderer.width(textRun);

                        if (lineWidth + spaceWidth < rect.size.width) {
                            // Space fits on line, add it...
                            lineWidth += spaceWidth;
                            trailingSpaceWidth += spaceWidth;
                            trailingSpaceCount++;
                            pos++;
                            lengthRemaining--;
                        } else {
                            // Space does not fit on this line - it is legal then to skip through all consecutive spaces
                            reachedEndOfLine = YES;
                            do { // Line breaking code reported we found a space, so always skip at least one.
                                lengthRemaining--;
                                pos++;
                                skippedWhitespace++;
                                if (pos[-1] == '\n') { // Stop the loop just after a newline character, as spaces after that should render
                                    foundNewline = YES; // Record the fact that we found a new line
                                    break;
                                }
                            } while (lengthRemaining > 0 && u_isWhitespace(pos[0])); // continue to skip end-of-line whitespace
                        }
                    }                    
                } else if (wordWidth == 0 && breakPos == 0 && lengthRemaining > 0) {
                    // Should not happen, but be safe and make sure we always either add width to the line or advance by a character.
                    // If not, skip ahead.
                    // FIXME: Should advance using character iterator and look for paragraph separator, but don't bother for now
                    // since this is a "can't happen" path.
                    pos++;
                    lengthRemaining--;
                }
            } else {
                reachedEndOfLine = YES;
                foundNewline = NO;      // We are not consuming any paragraph break, so don't react to it yet.
                if (lineWidth == 0 && lengthRemaining > 0) {
                    // We have nothing on the line so far but reached the end of the line?
                    // This must be a long word that doesn't fit inside the entire width
                    // Fit it on a line one character at a time and break when no more characters fit
                    // Force at least one character to avoid the edge case where a single glyph doesn't fit within width
                    NonSharedCharacterBreakIterator it(StringView(pos, lengthRemaining));
                    int offset = 0;
                    int nextCharBreak = boundedTextBreakFollowing(it, offset, lengthRemaining);
                    TextRun textRun(pos, nextCharBreak);
                    if (shouldDisableWordRounding())
                        textRun.disableRoundingHacks();
                    lineWidth += renderer.width(textRun);
                    float newLineWidth = lineWidth;
                    do {
                        lineWidth = newLineWidth;
                        offset = nextCharBreak;
                        if (lengthRemaining > offset) {
                            nextCharBreak = boundedTextBreakFollowing(it, offset, lengthRemaining);
                            TextRun textRun(pos + offset, nextCharBreak - offset);
                            if (shouldDisableWordRounding())
                                textRun.disableRoundingHacks();
                            newLineWidth += renderer.width(textRun);
                        }
                    } while (newLineWidth <= rect.size.width && lengthRemaining > offset);
                    // Update pos and lengthRemaining. Due to the semantics of boundedTextBreakFollowing, this will
                    // never exceed the bounds.
                    ASSERT(lengthRemaining >= offset);
                    pos += offset;
                    lengthRemaining -= offset;
                }
            }
        }
        
        if (lastLineEllipsed || lastLineClipped) {
            int i;
            for (i = 0; i < lengthRemaining && pos[i] != '\n'; i++) {
                /* Empty body - we just want to know if there's a newline between here and the end of the string */
            }
            
            bool droppingLines = (i != lengthRemaining);
            
            String lastLine = String(pos, i);
            if (lastLineEllipsed && i < lengthRemaining && !(pos[i] == '\n' && i == lengthRemaining - 1)) {
                // linebreak on last line with extra content that won't be rendered - insert an ellipse (5133005);
                // the exception is if we're at the last character and its a newline - next line won't render/size so don't ellipsise
                // This is OK for bidi since characters after a newline will not influence direction preceding it.
                const UChar ellipsisCharacter = 0x2026;
                lastLine.append(ellipsisCharacter);
            }
            // The last line is either everything that's left or everything until the next newline character
            if (!measureOnly) {
                bool insertEllipsis = (truncationRect == NULL);
                bool forceTruncation = (droppingLines && truncationRect);
                float customTruncationElementWidth = truncationRect ? truncationRect->size.width : 0;                
                
                textPoint = cursor;
                String ellipsisResult;
                float lastLineWidth;
                if (alignment == WebTextAlignmentCenter) {
                    ellipsisResult = applyEllipsisStyle(lastLine, ellipsisStyle, rect.size.width, renderer, shouldDisableWordRounding() ? StringTruncator::DisableRoundingHacks : StringTruncator::EnableRoundingHacks, &lastLineWidth, insertEllipsis, customTruncationElementWidth);
                    if (lastLineWidth != 0) // special case for single glyphs wider  than the rect to which we are rendering... applyEllipsisStyle can return 0 in this case.
                        textPoint.x += (rect.size.width - lastLineWidth) / 2.0f;
                } else if (alignment == WebTextAlignmentRight) {
                    ellipsisResult = applyEllipsisStyle(lastLine, ellipsisStyle, rect.size.width, renderer, shouldDisableWordRounding() ? StringTruncator::DisableRoundingHacks : StringTruncator::EnableRoundingHacks, &lastLineWidth, insertEllipsis, customTruncationElementWidth);
                    if (lastLineWidth != 0)
                        textPoint.x += rect.size.width - lastLineWidth;
                } else {
                    ellipsisResult = applyEllipsisStyle(lastLine, ellipsisStyle, rect.size.width, renderer, shouldDisableWordRounding() ? StringTruncator::DisableRoundingHacks : StringTruncator::EnableRoundingHacks, &lastLineWidth, insertEllipsis, customTruncationElementWidth, forceTruncation);
                    if (truncationRect && (ellipsisResult != lastLine || droppingLines)
                        && lastLineWidth && base == U_RIGHT_TO_LEFT)
                        textPoint.x += truncationRect->size.width;
                }
                CGContextRef cgContext = WKGetCurrentGraphicsContext();
                GraphicsContext context(static_cast<PlatformGraphicsContext*>(cgContext), false);
                context.setEmojiDrawingEnabled(includeEmoji);
                // FIXME: For proper bidi rendering, we need to pass the whole string, rather than the truncated string.
                // Otherwise, weak/neutral characters on either side of the ellipsis may pick up the wrong direction
                // if there are strong characters ellided.
                lineWidth = drawAtPoint(ellipsisResult.deprecatedCharacters(), ellipsisResult.length(), textPoint, renderer, &context, drawUnderline, paragraphNeedsBidi ? &status : 0);
                if (!renderedString.isNull())
                    renderedString.append(ellipsisResult.deprecatedCharacters(), ellipsisResult.length());
                
                if (truncationRect) {
                    if (ellipsisResult == lastLine && !droppingLines) {
                        *truncationRect = CGRectNull;
                    } else {
                        truncationRect->origin = textPoint;
                        truncationRect->origin.x += (base == U_RIGHT_TO_LEFT) ? (-truncationRect->size.width) : lineWidth;
                        truncationRect->origin.y -= ascent;
                        truncationRect->size.height = ascent - GSFontGetDescent(font);
                    }
                }
            } else {
                applyEllipsisStyle(lastLine, ellipsisStyle, rect.size.width, renderer, shouldDisableWordRounding() ? StringTruncator::DisableRoundingHacks : StringTruncator::EnableRoundingHacks, &lineWidth);
            }
        } else  {
            if (!measureOnly) {
                textPoint = cursor;
                if (alignment == WebTextAlignmentCenter && lineWidth <= rect.size.width) {
                    textPoint.x += (rect.size.width - (lineWidth - trailingSpaceWidth)) / 2.0f;
                } else if (alignment == WebTextAlignmentRight && lineWidth <= rect.size.width) {
                    textPoint.x += rect.size.width - (lineWidth - trailingSpaceWidth);
                }
                CGContextRef cgContext = WKGetCurrentGraphicsContext();
                GraphicsContext context(static_cast<PlatformGraphicsContext*>(cgContext), false);
                context.setEmojiDrawingEnabled(includeEmoji);
                if (paragraphNeedsBidi) {
                    drawAtPoint(startOfLine, pos - startOfLine + lengthRemaining, textPoint, renderer, &context, drawUnderline, &status, pos - startOfLine - trailingSpaceCount - skippedWhitespace);
                    if (!renderedString.isNull())
                        renderedString.append(startOfLine, pos - startOfLine + lengthRemaining);
                } else {
                    drawAtPoint(startOfLine, pos - startOfLine - trailingSpaceCount - skippedWhitespace, textPoint, renderer, &context, drawUnderline);
                    if (!renderedString.isNull())
                        renderedString.append(startOfLine, pos - startOfLine);
                }
            }
            startOfLine = pos;
        }
        if (lastLine)
            finishedLastLine = YES;
        else if (foundNewline) {
            // Redetermine whether the succeeding paragraph needs bidi layout, and if so, determine the base direction
            paragraphNeedsBidi = needsBidiLayout(startOfLine, lengthRemaining, base, true);
            if (paragraphNeedsBidi)
                status = BidiStatus(base, base, base, BidiContext::create((base == U_LEFT_TO_RIGHT) ? 0 : 1, base, false));
        }
        maxLineWidth = max(maxLineWidth, lineWidth);
        cursor.y += lineSpacing;
    }
    
    CGSize drawnSize;
    drawnSize.width = ceilf(maxLineWidth);
    if (!hasCustomLinespacing) {
        if (measureOnly)
            drawnSize.height = ceilf(cursor.y - rect.origin.y - ascent);
        else
            drawnSize.height = min<CGFloat>(ceilf(cursor.y - rect.origin.y - ascent), rect.size.height);
    } else {
        // for custom linespacing, the formula above does not hold true.
        float descent = GSFontGetDescent(font);
        if (measureOnly)
            drawnSize.height = ceilf(cursor.y - rect.origin.y - lineSpacing - descent);
        else
            drawnSize.height = min<CGFloat>(ceilf(cursor.y - rect.origin.y - lineSpacing - descent), rect.size.height);
    }
    
    if (renderedStringOut)
        *renderedStringOut = (NSString *)renderedString;

    return drawnSize;        
}

- (CGSize)__web_drawInRect:(CGRect)rect
              withFont:(GSFontRef)font
              ellipsis:(WebEllipsisStyle)ellipsisStyle
             alignment:(WebTextAlignment)alignment
         letterSpacing:(CGFloat)letterSpacing
           lineSpacing:(CGFloat)lineSpacing
          includeEmoji:(BOOL)includeEmoji
        truncationRect:(CGRect *)truncationRect
           measureOnly:(BOOL)measureOnly
     renderedStringOut:(NSString **)renderedStringOut
{
    return [self __web_drawInRect:rect
                         withFont:font
                         ellipsis:ellipsisStyle
                        alignment:alignment
                    letterSpacing:letterSpacing
                      lineSpacing:lineSpacing includeEmoji:includeEmoji
                   truncationRect:truncationRect
                      measureOnly:measureOnly
                renderedStringOut:renderedStringOut
                    drawUnderline:NO];
}

- (CGSize)__web_drawInRect:(CGRect)rect
                  withFont:(GSFontRef)font
                  ellipsis:(WebEllipsisStyle)ellipsisStyle
                 alignment:(WebTextAlignment)alignment
             letterSpacing:(CGFloat)letterSpacing
               lineSpacing:(CGFloat)lineSpacing
              includeEmoji:(BOOL)includeEmoji
            truncationRect:(CGRect *)truncationRect
               measureOnly:(BOOL)measureOnly
{
    return [self __web_drawInRect:rect
                         withFont:font
                         ellipsis:ellipsisStyle
                        alignment:alignment
                    letterSpacing:letterSpacing
                      lineSpacing:lineSpacing includeEmoji:includeEmoji
                   truncationRect:truncationRect
                      measureOnly:measureOnly
                renderedStringOut:nil];
}


#pragma mark Public methods

- (CGSize)_web_drawAtPoint:(CGPoint)point withFont:(GSFontRef)font
{
    return [self _web_drawAtPoint:point forWidth:FLT_MAX withFont:font ellipsis:WebEllipsisStyleNone letterSpacing:0.0f includeEmoji:YES];
}

- (CGSize)_web_sizeWithFont:(GSFontRef)font
{
    return [self _web_sizeWithFont:font forWidth:FLT_MAX ellipsis:WebEllipsisStyleNone letterSpacing:0.0f];
}

- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle
{
    return [self _web_sizeWithFont:font forWidth:width ellipsis:ellipsisStyle letterSpacing:0.0f];
}

- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing
{
    return [self _web_sizeWithFont:font forWidth:width ellipsis:ellipsisStyle letterSpacing:letterSpacing resultRange:NULL];
}

- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing resultRange:(NSRange *)resultRangeOut
{
    if (width < 0) {
#ifndef NDEBUG
        fprintf(stderr, "%s: width must be greater than zero.\n", __FUNCTION__);
#endif
        return CGSizeZero;
    }
    
    if (font == NULL) {
#ifndef NDEBUG
        fprintf(stderr, "%s: font must be non-null.\n", __FUNCTION__);
#endif
        return CGSizeZero;
    }
    
    if ([self isEqualToString:@""]) {
        return CGSizeZero;
    }

    float stringWidth;
    String s = String(self);
    
    FontCachePurgePreventer fontCachePurgePreventer;

    Font renderer = rendererForFont(font);
    if (letterSpacing != 0.0)
        renderer.setLetterSpacing(letterSpacing);
    
    if (resultRangeOut) {
        // Don't insert the ellipsis.
        String truncated = applyEllipsisStyle(s, ellipsisStyle, width, renderer, shouldDisableWordRounding() ? StringTruncator::DisableRoundingHacks : StringTruncator::EnableRoundingHacks, &stringWidth, false);
        NSRange resultRange = NSMakeRange(0, truncated.length());
        *resultRangeOut = resultRange;
    } else {
        applyEllipsisStyle(s, ellipsisStyle, width, renderer, shouldDisableWordRounding() ? StringTruncator::DisableRoundingHacks : StringTruncator::EnableRoundingHacks, &stringWidth);
    }
    
    // Ensure integral sizes. Ceil to avoid possible clipping.
    CGSize size = CGSizeMake (stringWidth, GSFontGetLineSpacing(font));
    size.width = ceilf(size.width);
    size.height = ceilf(size.height);
    return size;
}

- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle
{
    return [self _web_drawAtPoint:point forWidth:width withFont:font ellipsis:ellipsisStyle letterSpacing:0.0f includeEmoji:YES];
}

- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing
{
    return [self _web_drawAtPoint:(CGPoint)point forWidth:width withFont:font ellipsis:ellipsisStyle letterSpacing:letterSpacing includeEmoji:YES];
}

- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing includeEmoji:(BOOL)includeEmoji
{
    return [self __web_drawAtPoint:point forWidth:width withFont:font ellipsis:ellipsisStyle letterSpacing:letterSpacing includeEmoji:includeEmoji measureOnly:NO];
}

- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment lineSpacing:(int)lineSpacing includeEmoji:(BOOL)includeEmoji truncationRect:(CGRect *)truncationRect measureOnly:(BOOL)measureOnly
{
    return [self __web_drawInRect:rect withFont:font ellipsis:ellipsisStyle alignment:alignment letterSpacing:(CGFloat)0.0 lineSpacing:(CGFloat)lineSpacing includeEmoji:includeEmoji truncationRect:truncationRect measureOnly:measureOnly];
}

- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment lineSpacing:(int)lineSpacing includeEmoji:(BOOL)includeEmoji truncationRect:(CGRect *)truncationRect
{
    return [self _web_drawInRect:rect withFont:font ellipsis:ellipsisStyle alignment:alignment lineSpacing:lineSpacing includeEmoji:includeEmoji truncationRect:truncationRect measureOnly:NO];
}

- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment lineSpacing:(int)lineSpacing
{
    return [self _web_drawInRect:rect withFont:font ellipsis:ellipsisStyle alignment:alignment lineSpacing:lineSpacing includeEmoji:YES truncationRect:nil measureOnly:NO];
}

- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment
{
    return [self _web_drawInRect:rect withFont:font ellipsis:ellipsisStyle alignment:alignment lineSpacing:0 includeEmoji:YES truncationRect:nil measureOnly:NO];
}

- (CGSize)_web_sizeInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle lineSpacing:(int)lineSpacing
{
    return [self _web_drawInRect:rect withFont:font ellipsis:ellipsisStyle alignment:WebTextAlignmentLeft lineSpacing:lineSpacing includeEmoji:YES truncationRect:nil measureOnly:YES];
}

- (CGSize)_web_sizeInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle
{
    return [self _web_drawInRect:rect withFont:font ellipsis:ellipsisStyle alignment:WebTextAlignmentLeft lineSpacing:0 includeEmoji:YES truncationRect:nil measureOnly:YES];
}

- (NSString *)_web_stringForWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing includeEmoji:(BOOL)includeEmoji
{
    UNUSED_PARAM(includeEmoji);

    if (width <= 0) {
#ifndef NDEBUG
        fprintf(stderr, "%s: width must be greater than zero.\n", __FUNCTION__);
#endif
        return @"";
    }
    
    if (font == NULL) {
#ifndef NDEBUG
        fprintf(stderr, "%s: font must be non-null.\n", __FUNCTION__);
#endif
        return self;
    }    

    FontCachePurgePreventer fontCachePurgePreventer;

    Font renderer = rendererForFont(font);
    if (letterSpacing != 0.0)
        renderer.setLetterSpacing(letterSpacing);

    return applyEllipsisStyle(self, ellipsisStyle, width, renderer, shouldDisableWordRounding() ? StringTruncator::DisableRoundingHacks : StringTruncator::EnableRoundingHacks, 0);
}

- (CGSize)_web_sizeForWidth:(CGFloat)width withAttributes:(id <WebTextRenderingAttributes>)attributes
{
    // FIXME: We're not computing the correct truncation rect here
    attributes.truncationRect = CGRectZero;
    return [self __web_drawAtPoint:CGPointZero
                          forWidth:width
                          withFont:attributes.font
                          ellipsis:attributes.ellipsisStyle
                     letterSpacing:attributes.letterSpacing
                      includeEmoji:attributes.includeEmoji
                       measureOnly:YES
                 renderedStringOut:attributes.renderString];
}

- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(CGFloat)width withAttributes:(id <WebTextRenderingAttributes>)attributes
{
    // FIXME: We're not computing the correct truncation rect here
    attributes.truncationRect = CGRectZero;
    return [self __web_drawAtPoint:point
                          forWidth:width
                          withFont:attributes.font
                          ellipsis:attributes.ellipsisStyle
                     letterSpacing:attributes.letterSpacing
                      includeEmoji:attributes.includeEmoji
                       measureOnly:NO
                 renderedStringOut:attributes.renderString
                     drawUnderline:attributes.drawUnderline];
}

- (CGSize)_web_sizeInRect:(CGRect)rect withAttributes:(id <WebTextRenderingAttributes>)attributes
{
    // FIXME: We're not computing the correct truncation rect here
    CGSize size = [self __web_drawInRect:rect
                                withFont:attributes.font
                                ellipsis:attributes.ellipsisStyle
                               alignment:attributes.alignment
                           letterSpacing:attributes.letterSpacing
                             lineSpacing:attributes.lineSpacing
                            includeEmoji:attributes.includeEmoji
                          truncationRect:NULL
                             measureOnly:YES
                       renderedStringOut:attributes.renderString];
    return size;
}

- (CGSize)_web_drawInRect:(CGRect)rect withAttributes:(id <WebTextRenderingAttributes>)attributes
{
    // FIXME: We're not computing the correct truncation rect here
    CGSize size = [self __web_drawInRect:rect
                                withFont:attributes.font
                                ellipsis:attributes.ellipsisStyle
                               alignment:attributes.alignment
                           letterSpacing:attributes.letterSpacing
                             lineSpacing:attributes.lineSpacing
                            includeEmoji:attributes.includeEmoji
                          truncationRect:NULL
                             measureOnly:NO
                       renderedStringOut:attributes.renderString
                           drawUnderline:attributes.drawUnderline];
    return size;
}

@end

#endif // PLATFORM(IOS)
