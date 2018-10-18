/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebKitNSStringExtras.h"

#import <WebCore/ColorMac.h>
#import <WebCore/FontCascade.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/TextRun.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <sys/param.h>
#import <unicode/uchar.h>

NSString *WebKitLocalCacheDefaultsKey = @"WebKitLocalCache";
NSString *WebKitResourceLoadStatisticsDirectoryDefaultsKey = @"WebKitResourceLoadStatisticsDirectory";

using namespace WebCore;

@implementation NSString (WebKitExtras)

#if PLATFORM(MAC)

static bool canUseFastRenderer(const UniChar* buffer, unsigned length)
{
    for (unsigned i = 0; i < length; i++) {
        if (buffer[i] > 0xFF) {
            auto direction = u_charDirection(buffer[i]);
            if (direction == U_RIGHT_TO_LEFT || (direction > U_OTHER_NEUTRAL && direction != U_DIR_NON_SPACING_MARK && direction != U_BOUNDARY_NEUTRAL))
                return false;
        }
    }
    return true;
}

- (void)_web_drawAtPoint:(NSPoint)point font:(NSFont *)font textColor:(NSColor *)textColor
{
    if (!font)
        return;

    unsigned length = [self length];
    Vector<UniChar, 2048> buffer(length);
    [self getCharacters:buffer.data()];

    if (canUseFastRenderer(buffer.data(), length)) {
        FontCascade webCoreFont(FontPlatformData((__bridge CTFontRef)font, [font pointSize]));
        TextRun run(StringView(buffer.data(), length));

        // The following is a half-assed attempt to match AppKit's rounding rules for drawAtPoint.
        // If you change it, be sure to test all the text drawn this way in Safari, including
        // the status bar, bookmarks bar, tab bar, and activity window.
        point.y = CGCeiling(point.y);

        NSGraphicsContext *nsContext = [NSGraphicsContext currentContext];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CGContextRef cgContext = static_cast<CGContextRef>([nsContext graphicsPort]);
        ALLOW_DEPRECATED_DECLARATIONS_END
        GraphicsContext graphicsContext { cgContext };

        // WebCore requires a flipped graphics context.
        bool flipped = [nsContext isFlipped];
        if (!flipped)
            CGContextScaleCTM(cgContext, 1, -1);

        graphicsContext.setFillColor(colorFromNSColor(textColor));
        webCoreFont.drawText(graphicsContext, run, FloatPoint(point.x, flipped ? point.y : -point.y));

        if (!flipped)
            CGContextScaleCTM(cgContext, 1, -1);
    } else {
        // The given point is on the baseline.
        if ([[NSView focusView] isFlipped])
            point.y -= [font ascender];
        else
            point.y += [font descender];

        [self drawAtPoint:point withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, textColor, NSForegroundColorAttributeName, nil]];
    }
}

- (float)_web_widthWithFont:(NSFont *)font
{
    unsigned length = [self length];
    Vector<UniChar, 2048> buffer(length);
    [self getCharacters:buffer.data()];

    if (canUseFastRenderer(buffer.data(), length)) {
        FontCascade webCoreFont(FontPlatformData((__bridge CTFontRef)font, [font pointSize]));
        TextRun run(StringView(buffer.data(), length));
        return webCoreFont.width(run);
    }

    return [self sizeWithAttributes:[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, nil]].width;
}

#endif // PLATFORM(MAC)

- (NSString *)_web_stringByAbbreviatingWithTildeInPath
{
    // Handles home directories that have symlinks in their paths as well as what stringByAbbreviatingWithTildeInPath handles.
    // This works around Radar bug 2774250.

    NSString *resolvedHomeDirectory = [NSHomeDirectory() stringByResolvingSymlinksInPath];
    NSString *path;

    if ([self hasPrefix:resolvedHomeDirectory]) {
        NSString *relativePath = [self substringFromIndex:[resolvedHomeDirectory length]];
        path = [NSHomeDirectory() stringByAppendingPathComponent:relativePath];
    } else {
        path = self;
    }

    return [path stringByAbbreviatingWithTildeInPath];
}

- (BOOL)_webkit_isCaseInsensitiveEqualToString:(NSString *)string
{
    return [self compare:string options:(NSCaseInsensitiveSearch | NSLiteralSearch)] == NSOrderedSame;
}

-(BOOL)_webkit_hasCaseInsensitivePrefix:(NSString *)prefix
{
    return [self rangeOfString:prefix options:(NSCaseInsensitiveSearch | NSAnchoredSearch)].location != NSNotFound;
}

-(BOOL)_webkit_hasCaseInsensitiveSuffix:(NSString *)suffix
{
    return [self rangeOfString:suffix options:(NSCaseInsensitiveSearch | NSBackwardsSearch | NSAnchoredSearch)].location != NSNotFound;
}

-(NSString *)_webkit_filenameByFixingIllegalCharacters
{
    return filenameByFixingIllegalCharacters(self);
}

-(NSString *)_webkit_stringByTrimmingWhitespace
{
    NSMutableString *trimmed = [[self mutableCopy] autorelease];
    CFStringTrimWhitespace((__bridge CFMutableStringRef)trimmed);
    return trimmed;
}

#if PLATFORM(MAC)

// FIXME: This is here only for binary compatibility with Safari 8 and earlier.
// Remove it once we don't have to support that any more.
-(NSString *)_webkit_fixedCarbonPOSIXPath
{
    return self;
}

#endif

+ (NSString *)_webkit_localCacheDirectoryWithBundleIdentifier:(NSString*)bundleIdentifier
{
    NSString *cacheDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebKitLocalCacheDefaultsKey];

    if (!cacheDirectory || ![cacheDirectory isKindOfClass:[NSString class]]) {
#if PLATFORM(IOS_FAMILY)
        cacheDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Caches"];
#endif
#if PLATFORM(MAC)
        char buffer[MAXPATHLEN];
        if (size_t length = confstr(_CS_DARWIN_USER_CACHE_DIR, buffer, MAXPATHLEN))
            cacheDirectory = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:buffer length:length - 1];
#endif
    }

    return [cacheDirectory stringByAppendingPathComponent:bundleIdentifier];
}

@end
