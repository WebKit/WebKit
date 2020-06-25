/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#import "config.h"
#import "RenderThemeMac.h"

#if PLATFORM(MAC)

#import "BitmapImage.h"
#import "CSSValueKeywords.h"
#import "CSSValueList.h"
#import "Color.h"
#import "ColorMac.h"
#import "Document.h"
#import "Element.h"
#import "FileList.h"
#import "FloatRoundedRect.h"
#import "FocusController.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "FrameView.h"
#import "GeometryUtilities.h"
#import "GraphicsContext.h"
#import "HTMLAttachmentElement.h"
#import "HTMLInputElement.h"
#import "HTMLMediaElement.h"
#import "HTMLNames.h"
#import "HTMLPlugInImageElement.h"
#import "Icon.h"
#import "Image.h"
#import "ImageBuffer.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "LocalizedStrings.h"
#import "Page.h"
#import "PaintInfo.h"
#import "PathUtilities.h"
#import "RenderAttachment.h"
#import "RenderLayer.h"
#import "RenderMedia.h"
#import "RenderProgress.h"
#import "RenderSlider.h"
#import "RenderSnapshottedPlugIn.h"
#import "RenderView.h"
#import "RuntimeEnabledFeatures.h"
#import "SharedBuffer.h"
#import "StringTruncator.h"
#import "ThemeMac.h"
#import "TimeRanges.h"
#import "UTIUtilities.h"
#import "UserAgentScripts.h"
#import "UserAgentStyleSheets.h"
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>
#import <math.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/NSColorSPI.h>
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <pal/spi/mac/NSCellSPI.h>
#import <pal/spi/mac/NSImageSPI.h>
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#import <wtf/MathExtras.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/StringBuilder.h>

#if ENABLE(METER_ELEMENT)
#import "RenderMeter.h"
#import "HTMLMeterElement.h"
#endif

#if ENABLE(SERVICE_CONTROLS)

// FIXME: This should go into an SPI.h file in the spi directory.
#if USE(APPLE_INTERNAL_SDK)
#import <AppKit/AppKitDefines_Private.h>
#import <AppKit/NSServicesRolloverButtonCell.h>
#else
#define APPKIT_PRIVATE_CLASS
@interface NSServicesRolloverButtonCell : NSButtonCell
@end
#endif

// FIXME: This should go into an SPI.h file in the spi directory.
@interface NSServicesRolloverButtonCell ()
+ (NSServicesRolloverButtonCell *)serviceRolloverButtonCellForStyle:(NSSharingServicePickerStyle)style;
- (NSRect)rectForBounds:(NSRect)bounds preferredEdge:(NSRectEdge)preferredEdge;
@end

#endif // ENABLE(SERVICE_CONTROLS)

// FIXME: This should go into an SPI.h file in the spi directory.
@interface NSTextFieldCell ()
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus;
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus maskOnly:(BOOL)maskOnly;
@end

// FIXME: This should go into an SPI.h file in the spi directory.
@interface NSSearchFieldCell ()
@property (getter=isCenteredLook) BOOL centeredLook;
@end

static const Seconds progressAnimationFrameRate = 33_ms; // 30 fps
static const double progressAnimationNumFrames = 256;

@interface WebCoreRenderThemeNotificationObserver : NSObject
@end

@implementation WebCoreRenderThemeNotificationObserver

- (id)init
{
    self = [super init];
    if (!self)
        return nil;

    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(systemColorsDidChange:) name:NSSystemColorsDidChangeNotification object:nil];

    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
        selector:@selector(systemColorsDidChange:) name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:nil];

    return self;
}

- (void)systemColorsDidChange:(NSNotification *)notification
{
    UNUSED_PARAM(notification);
    WebCore::RenderTheme::singleton().platformColorsDidChange();
}

@end

@interface WebCoreTextFieldCell : NSTextFieldCell
@end

@implementation WebCoreTextFieldCell

- (CFDictionaryRef)_adjustedCoreUIDrawOptionsForDrawingBordersOnly:(CFDictionaryRef)defaultOptions
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    // Dark mode controls don't have borders, just a semi-transparent background of shadows.
    // In the dark mode case we can't disable borders, or we will not paint anything for the control.
    NSAppearanceName appearance = [self.controlView.effectiveAppearance bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    if ([appearance isEqualToString:NSAppearanceNameDarkAqua])
        return defaultOptions;
#endif

    // FIXME: This is a workaround for <rdar://problem/11385461>. When that bug is resolved, we should remove this code,
    // as well as the internal method overrides below.
    CFMutableDictionaryRef coreUIDrawOptions = CFDictionaryCreateMutableCopy(NULL, 0, defaultOptions);
    CFDictionarySetValue(coreUIDrawOptions, CFSTR("borders only"), kCFBooleanTrue);
    CFAutorelease(coreUIDrawOptions);
    return coreUIDrawOptions;
}

- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus
{
    return [self _adjustedCoreUIDrawOptionsForDrawingBordersOnly:[super _coreUIDrawOptionsWithFrame:cellFrame inView:controlView includeFocus:includeFocus]];
}

- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus maskOnly:(BOOL)maskOnly
{
    return [self _adjustedCoreUIDrawOptionsForDrawingBordersOnly:[super _coreUIDrawOptionsWithFrame:cellFrame inView:controlView includeFocus:includeFocus maskOnly:maskOnly]];
}

@end

@interface WebCoreRenderThemeBundle : NSObject
@end

@implementation WebCoreRenderThemeBundle
@end

#if ENABLE(DATALIST_ELEMENT)

static const CGFloat listButtonWidth = 16.0f;
static const CGFloat listButtonCornerRadius = 5.0f;

@interface WebListButtonCell : NSCell
@end

@implementation WebListButtonCell
- (void)drawWithFrame:(NSRect)cellFrame inView:(__unused NSView *)controlView
{
    CGFloat listButtonCornerRadius = 5.0f;
    NSPoint topLeft = NSMakePoint(NSMinX(cellFrame), NSMinY(cellFrame));
    NSPoint topRight = NSMakePoint(NSMaxX(cellFrame), NSMinY(cellFrame));
    NSPoint bottomRight = NSMakePoint(NSMaxX(cellFrame), NSMaxY(cellFrame));
    NSPoint bottomLeft = NSMakePoint(NSMinX(cellFrame), NSMaxY(cellFrame));

    NSBezierPath *path = [NSBezierPath bezierPath];
    [path moveToPoint:topLeft];

    [path lineToPoint:NSMakePoint(topRight.x - listButtonCornerRadius, topRight.y)];
    [path curveToPoint:NSMakePoint(topRight.x, topRight.y + listButtonCornerRadius) controlPoint1:topRight controlPoint2:topRight];

    [path lineToPoint:NSMakePoint(bottomRight.x, bottomRight.y - listButtonCornerRadius)];
    [path curveToPoint:NSMakePoint(bottomRight.x - listButtonCornerRadius, bottomRight.y) controlPoint1:bottomRight controlPoint2:bottomRight];

    [path lineToPoint:bottomLeft];
    [path lineToPoint:topLeft];

    if ([self userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft) {
        NSAffineTransform *transform = [NSAffineTransform transform];
        [transform translateXBy:NSMidX(cellFrame) yBy:NSMidY(cellFrame)];
        [transform rotateByDegrees:180];
        [transform translateXBy:-1 * NSMidX(cellFrame) yBy:-1 * NSMidY(cellFrame)];
        [path transformUsingAffineTransform:transform];
    }

    // FIXME: Obtain the gradient colors from CoreUI or AppKit
    RetainPtr<NSGradient> gradient;
#if HAVE(OS_DARK_MODE_SUPPORT)
    NSUserAccentColor accentColor = NSColorGetUserAccentColor();
    if (accentColor == NSUserAccentColorRed)
        gradient = adoptNS([[NSGradient alloc] initWithStartingColor:[NSColor colorWithRed:(212.0 / 255) green:(122.0 / 255) blue:(117.0 / 255) alpha:1.0] endingColor:[NSColor colorWithRed:(189.0 / 255) green:(34.0 / 255) blue:(23.0 / 255) alpha:1.0]]);
    else if (accentColor == NSUserAccentColorOrange)
        gradient = adoptNS([[NSGradient alloc] initWithStartingColor:[NSColor colorWithRed:(242.0 / 255) green:(185.0 / 255) blue:(113.0 / 255) alpha:1.0] endingColor:[NSColor colorWithRed:(242.0 / 255) green:(145.0 / 255) blue:(17.0 / 255) alpha:1.0]]);
    else if (accentColor == NSUserAccentColorYellow)
        gradient = adoptNS([[NSGradient alloc] initWithStartingColor:[NSColor colorWithRed:(241.0 / 255) green:(212.0 / 255) blue:(119.0 / 255) alpha:1.0] endingColor:[NSColor colorWithRed:(239.0 / 255) green:(193.0 / 255) blue:(27.0 / 255) alpha:1.0]]);
    else if (accentColor == NSUserAccentColorGreen)
        gradient = adoptNS([[NSGradient alloc] initWithStartingColor:[NSColor colorWithRed:(132.0 / 255) green:(186.0 / 255) blue:(120.0 / 255) alpha:1.0] endingColor:[NSColor colorWithRed:(46.0 / 255) green:(145.0 / 255) blue:(30.0 / 255) alpha:1.0]]);
    else if (accentColor == NSUserAccentColorPurple)
        gradient = adoptNS([[NSGradient alloc] initWithStartingColor:[NSColor colorWithRed:(178.0 / 255) green:(128.0 / 255) blue:(175.0 / 255) alpha:1.0] endingColor:[NSColor colorWithRed:(130.0 / 255) green:(43.0 / 255) blue:(123.0 / 255) alpha:1.0]]);
    else if (accentColor == NSUserAccentColorPink)
        gradient = adoptNS([[NSGradient alloc] initWithStartingColor:[NSColor colorWithRed:(225.0 / 255) green:(126.0 / 255) blue:(165.0 / 255) alpha:1.0] endingColor:[NSColor colorWithRed:(211.0 / 255) green:(42.0 / 255) blue:(105.0 / 255) alpha:1.0]]);
    else if (accentColor == NSUserAccentColorNoColor)
        gradient = adoptNS([[NSGradient alloc] initWithStartingColor:[NSColor colorWithRed:(177.0 / 255) green:(177.0 / 255) blue:(182.0 / 255) alpha:1.0] endingColor:[NSColor colorWithRed:(145.0 / 255) green:(145.0 / 255) blue:(150.0 / 255) alpha:1.0]]);
    else
#endif
        gradient = adoptNS([[NSGradient alloc] initWithStartingColor:[NSColor colorWithRed:(114.0 / 255) green:(164.0 / 255) blue:(243.0 / 255) alpha:1.0] endingColor:[NSColor colorWithRed:(45.0 / 255) green:(117.0 / 255) blue:(246.0 / 255) alpha:1.0]]);

    [gradient drawInBezierPath:path angle:90];
    if ([self isHighlighted]) {
        NSColor *overlay = [NSColor colorWithWhite:0 alpha:0.1];
        [overlay setFill];
        [path fill];
    }
}
@end

#endif // ENABLE(DATALIST_ELEMENT)

namespace WebCore {

using namespace HTMLNames;

enum {
    topMargin,
    rightMargin,
    bottomMargin,
    leftMargin
};

enum {
    topPadding,
    rightPadding,
    bottomPadding,
    leftPadding
};

RenderTheme& RenderTheme::singleton()
{
    static NeverDestroyed<RenderThemeMac> theme;
    return theme;
}

CFStringRef RenderThemeMac::contentSizeCategory() const
{
    return kCTFontContentSizeCategoryL;
}

RenderThemeMac::RenderThemeMac()
    : m_notificationObserver(adoptNS([[WebCoreRenderThemeNotificationObserver alloc] init]))
{
}

NSView *RenderThemeMac::documentViewFor(const RenderObject& o) const
{
    LocalDefaultSystemAppearance localAppearance(o.useDarkAppearance());
    ControlStates states(extractControlStatesForRenderer(o));
    return ThemeMac::ensuredView(&o.view().frameView(), states);
}

String RenderThemeMac::mediaControlsStyleSheet()
{
    if (m_legacyMediaControlsStyleSheet.isEmpty())
        m_legacyMediaControlsStyleSheet = [NSString stringWithContentsOfFile:[[NSBundle bundleForClass:[WebCoreRenderThemeBundle class]] pathForResource:@"mediaControlsApple" ofType:@"css"] encoding:NSUTF8StringEncoding error:nil];
    return m_legacyMediaControlsStyleSheet;
}

String RenderThemeMac::modernMediaControlsStyleSheet()
{
    if (RuntimeEnabledFeatures::sharedFeatures().modernMediaControlsEnabled()) {
        if (m_mediaControlsStyleSheet.isEmpty())
            m_mediaControlsStyleSheet = [NSString stringWithContentsOfFile:[[NSBundle bundleForClass:[WebCoreRenderThemeBundle class]] pathForResource:@"modern-media-controls" ofType:@"css" inDirectory:@"modern-media-controls"] encoding:NSUTF8StringEncoding error:nil];
        return m_mediaControlsStyleSheet;
    }
    return emptyString();
}

void RenderThemeMac::purgeCaches()
{
    m_legacyMediaControlsScript.clearImplIfNotShared();
    m_mediaControlsScript.clearImplIfNotShared();
    m_legacyMediaControlsStyleSheet.clearImplIfNotShared();
    m_mediaControlsStyleSheet.clearImplIfNotShared();

    RenderTheme::purgeCaches();
}

String RenderThemeMac::mediaControlsScript()
{
    if (RuntimeEnabledFeatures::sharedFeatures().modernMediaControlsEnabled()) {
        if (m_mediaControlsScript.isEmpty()) {
            NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
            NSString *localizedStrings = [NSString stringWithContentsOfFile:[bundle pathForResource:@"modern-media-controls-localized-strings" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil];
            NSString *script = [NSString stringWithContentsOfFile:[bundle pathForResource:@"modern-media-controls" ofType:@"js" inDirectory:@"modern-media-controls"] encoding:NSUTF8StringEncoding error:nil];
            m_mediaControlsScript = makeString(String { localizedStrings }, String { script });
        }
        return m_mediaControlsScript;
    }

    if (m_legacyMediaControlsScript.isEmpty()) {
        NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
        NSString *localizedStrings = [NSString stringWithContentsOfFile:[bundle pathForResource:@"mediaControlsLocalizedStrings" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil];
        NSString *script = [NSString stringWithContentsOfFile:[bundle pathForResource:@"mediaControlsApple" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil];
        m_legacyMediaControlsScript = makeString(String { localizedStrings }, String { script });
    }
    return m_legacyMediaControlsScript;
}

String RenderThemeMac::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().modernMediaControlsEnabled())
        return emptyString();

    NSString *directory = @"modern-media-controls/images";
    NSBundle *bundle = [NSBundle bundleForClass:[WebCoreRenderThemeBundle class]];
    return [[NSData dataWithContentsOfFile:[bundle pathForResource:iconName ofType:iconType inDirectory:directory]] base64EncodedStringWithOptions:0];
}

#if ENABLE(SERVICE_CONTROLS)

String RenderThemeMac::imageControlsStyleSheet() const
{
    return String(imageControlsMacUserAgentStyleSheet, sizeof(imageControlsMacUserAgentStyleSheet));
}

#endif

Color RenderThemeMac::platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    return colorFromNSColor([NSColor selectedTextBackgroundColor]);
}

Color RenderThemeMac::platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    return colorFromNSColor([NSColor unemphasizedSelectedTextBackgroundColor]);
#else
    UNUSED_PARAM(options);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return colorFromNSColor([NSColor secondarySelectedControlColor]);
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif
}

Color RenderThemeMac::transformSelectionBackgroundColor(const Color& color, OptionSet<StyleColor::Options> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    if (localAppearance.usingDarkAppearance()) {
        // Use an alpha value that is similar to results from blendWithWhite() on light colors.
        static const float darkAppearanceAlpha = 0.8;
        return !color.isOpaque() ? color : color.colorWithAlpha(darkAppearanceAlpha);
    }

    return RenderTheme::transformSelectionBackgroundColor(color, options);
}

bool RenderThemeMac::supportsSelectionForegroundColors(OptionSet<StyleColor::Options> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    return localAppearance.usingDarkAppearance();
}

Color RenderThemeMac::platformActiveSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    if (localAppearance.usingDarkAppearance())
        return colorFromNSColor([NSColor selectedTextColor]);
    return { };
}

Color RenderThemeMac::platformInactiveSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    if (localAppearance.usingDarkAppearance())
        return colorFromNSColor([NSColor unemphasizedSelectedTextColor]);
    return { };
#else
    UNUSED_PARAM(options);
    return { };
#endif
}

Color RenderThemeMac::platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    return colorFromNSColor([NSColor selectedContentBackgroundColor]);
#else
    UNUSED_PARAM(options);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return colorFromNSColor([NSColor alternateSelectedControlColor]);
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif
}

Color RenderThemeMac::platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    return colorFromNSColor([NSColor unemphasizedSelectedContentBackgroundColor]);
#else
    UNUSED_PARAM(options);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return colorFromNSColor([NSColor secondarySelectedControlColor]);
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif
}

Color RenderThemeMac::platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    return colorFromNSColor([NSColor alternateSelectedControlTextColor]);
}

Color RenderThemeMac::platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    return colorFromNSColor([NSColor unemphasizedSelectedTextColor]);
#else
    UNUSED_PARAM(options);
    return colorFromNSColor([NSColor selectedControlTextColor]);
#endif
}

Color RenderThemeMac::platformFocusRingColor(OptionSet<StyleColor::Options> options) const
{
    if (usesTestModeFocusRingColor())
        return oldAquaFocusRingColor();
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    // The color is expected to be opaque, since CoreGraphics will apply opacity when drawing (because opacity is normally animated).
    return colorFromNSColor([NSColor keyboardFocusIndicatorColor]).opaqueColor();
}

Color RenderThemeMac::platformTextSearchHighlightColor(OptionSet<StyleColor::Options> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColor::Options::UseDarkAppearance));
    return colorFromNSColor([NSColor findHighlightColor]);
}

static SimpleColor menuBackgroundColor()
{
    RetainPtr<NSBitmapImageRep> offscreenRep = adoptNS([[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil pixelsWide:1 pixelsHigh:1
        bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:4 bitsPerPixel:32]);

    CGContextRef bitmapContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:offscreenRep.get()].CGContext;
    const CGRect rect = CGRectMake(0, 0, 1, 1);

    HIThemeMenuDrawInfo drawInfo;
    drawInfo.version =  0;
    drawInfo.menuType = kThemeMenuTypePopUp;

    HIThemeDrawMenuBackground(&rect, &drawInfo, bitmapContext, kHIThemeOrientationInverted);

    NSUInteger pixel[4];
    [offscreenRep getPixel:pixel atX:0 y:0];

    return makeSimpleColor(pixel[0], pixel[1], pixel[2], pixel[3]);
}

Color RenderThemeMac::systemColor(CSSValueID cssValueID, OptionSet<StyleColor::Options> options) const
{
    const bool useSystemAppearance = options.contains(StyleColor::Options::UseSystemAppearance);
    const bool useDarkAppearance = options.contains(StyleColor::Options::UseDarkAppearance);
    const bool forVisitedLink = options.contains(StyleColor::Options::ForVisitedLink);

    auto& cache = colorCache(options);

    if (useSystemAppearance) {
        // Special handling for links and other system colors when the system appearance is desired.
        auto systemAppearanceColor = [useDarkAppearance] (Color& color, SEL selector) -> Color {
            if (!color.isValid()) {
                LocalDefaultSystemAppearance localAppearance(useDarkAppearance);
                auto systemColor = wtfObjCMsgSend<NSColor *>([NSColor class], selector);
                color = semanticColorFromNSColor(systemColor);
            }

            return color;
        };

        switch (cssValueID) {
        // Web views that want system appearance get the system version of link colors, which differ from the HTML spec.
        case CSSValueWebkitLink:
            if (forVisitedLink)
                return systemAppearanceColor(cache.systemVisitedLinkColor, @selector(systemPurpleColor));
            return systemAppearanceColor(cache.systemLinkColor, @selector(linkColor));

        case CSSValueLinktext:
            return systemAppearanceColor(cache.systemLinkColor, @selector(linkColor));
        case CSSValueVisitedtext:
            return systemAppearanceColor(cache.systemVisitedLinkColor, @selector(systemPurpleColor));
        case CSSValueWebkitActivelink:
        case CSSValueActivetext:
            // FIXME: Use a semantic system color for this, instead of systemRedColor. <rdar://problem/39256684>
            return systemAppearanceColor(cache.systemActiveLinkColor, @selector(systemRedColor));

        // The following colors would expose user appearance preferences to the web, and could be used for fingerprinting.
        // These should only be available when the web view is wanting the system appearance.
        case CSSValueWebkitFocusRingColor:
        case CSSValueActiveborder:
            return focusRingColor(options);

        case CSSValueAppleSystemControlAccent:
#if HAVE(OS_DARK_MODE_SUPPORT)
            return systemAppearanceColor(cache.systemControlAccentColor, @selector(controlAccentColor));
#else
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            return systemAppearanceColor(cache.systemControlAccentColor, @selector(alternateSelectedControlColor));
            ALLOW_DEPRECATED_DECLARATIONS_END
#endif

        case CSSValueAppleSystemSelectedContentBackground:
            return activeListBoxSelectionBackgroundColor(options);

        case CSSValueAppleSystemSelectedTextBackground:
        case CSSValueHighlight:
            return activeSelectionBackgroundColor(options);

        default:
            // Handle other system colors below, that don't need special system appearance handling.
            break;
        }
    } else if (forVisitedLink && cssValueID == CSSValueWebkitLink) {
        // The system color cache below can't handle visited links. The only color value
        // that cares about visited links is CSSValueWebkitLink, so handle it here.
        return RenderTheme::systemColor(cssValueID, options);
    }

    ASSERT(!forVisitedLink);

    return cache.systemStyleColors.ensure(cssValueID, [this, cssValueID, options, useDarkAppearance] () -> Color {
        LocalDefaultSystemAppearance localAppearance(useDarkAppearance);

        auto selectCocoaColor = [cssValueID] () -> SEL {
            switch (cssValueID) {
            case CSSValueActivecaption:
                return @selector(windowFrameTextColor);
            case CSSValueAppworkspace:
                return @selector(headerColor);
            case CSSValueButtonhighlight:
                return @selector(controlHighlightColor);
            case CSSValueButtonshadow:
                return @selector(controlShadowColor);
            case CSSValueButtontext:
                return @selector(controlTextColor);
            case CSSValueCanvas:
                return @selector(textBackgroundColor);
            case CSSValueCanvastext:
                return @selector(textColor);
            case CSSValueCaptiontext:
                return @selector(textColor);
            case CSSValueField:
                return @selector(controlColor);
            case CSSValueFieldtext:
                return @selector(controlTextColor);
            case CSSValueGraytext:
                return @selector(disabledControlTextColor);
            case CSSValueHighlighttext:
                return @selector(selectedTextColor);
            case CSSValueInactiveborder:
                return @selector(controlBackgroundColor);
            case CSSValueInactivecaption:
                return @selector(controlBackgroundColor);
            case CSSValueInactivecaptiontext:
                return @selector(textColor);
            case CSSValueInfotext:
                return @selector(textColor);
            case CSSValueMenutext:
                return @selector(selectedMenuItemTextColor);
            case CSSValueScrollbar:
                return @selector(scrollBarColor);
            case CSSValueText:
                return @selector(textColor);
            case CSSValueThreeddarkshadow:
                return @selector(controlDarkShadowColor);
            case CSSValueThreedshadow:
                return @selector(shadowColor);
            case CSSValueThreedhighlight:
                return @selector(highlightColor);
            case CSSValueThreedlightshadow:
                return @selector(controlLightHighlightColor);
            case CSSValueWindow:
                return @selector(windowBackgroundColor);
            case CSSValueWindowframe:
                return @selector(windowFrameColor);
            case CSSValueWindowtext:
                return @selector(windowFrameTextColor);
            case CSSValueAppleSystemHeaderText:
                return @selector(headerTextColor);
            case CSSValueAppleSystemBackground:
            case CSSValueAppleSystemSecondaryBackground:
            case CSSValueAppleSystemTertiaryBackground:
            case CSSValueAppleSystemGroupedBackground:
            case CSSValueAppleSystemSecondaryGroupedBackground:
            case CSSValueAppleSystemTertiaryGroupedBackground:
            case CSSValueAppleSystemTextBackground:
                return @selector(textBackgroundColor);
            case CSSValueAppleSystemControlBackground:
#if HAVE(OS_DARK_MODE_SUPPORT)
            case CSSValueWebkitControlBackground:
#endif
                return @selector(controlBackgroundColor);
            case CSSValueAppleSystemAlternateSelectedText:
                return @selector(alternateSelectedControlTextColor);
            case CSSValueAppleSystemUnemphasizedSelectedContentBackground:
#if HAVE(OS_DARK_MODE_SUPPORT)
                return @selector(unemphasizedSelectedContentBackgroundColor);
#else
                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                return @selector(secondarySelectedControlColor);
                ALLOW_DEPRECATED_DECLARATIONS_END
#endif
            case CSSValueAppleSystemSelectedText:
                return @selector(selectedTextColor);
            case CSSValueAppleSystemUnemphasizedSelectedText:
#if HAVE(OS_DARK_MODE_SUPPORT)
                return @selector(unemphasizedSelectedTextColor);
#else
                return @selector(textColor);
#endif
            case CSSValueAppleSystemUnemphasizedSelectedTextBackground:
#if HAVE(OS_DARK_MODE_SUPPORT)
                return @selector(unemphasizedSelectedTextBackgroundColor);
#else
                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                return @selector(secondarySelectedControlColor);
                ALLOW_DEPRECATED_DECLARATIONS_END
#endif
            case CSSValueAppleSystemPlaceholderText:
                return @selector(placeholderTextColor);
            case CSSValueAppleSystemFindHighlightBackground:
                return @selector(findHighlightColor);
            case CSSValueAppleSystemContainerBorder:
#if HAVE(OS_DARK_MODE_SUPPORT)
                return @selector(containerBorderColor);
#else
                // Handled below.
                return nullptr;
#endif
            case CSSValueAppleSystemLabel:
                return @selector(labelColor);
            case CSSValueAppleSystemSecondaryLabel:
                return @selector(secondaryLabelColor);
            case CSSValueAppleSystemTertiaryLabel:
                return @selector(tertiaryLabelColor);
            case CSSValueAppleSystemQuaternaryLabel:
                return @selector(quaternaryLabelColor);
            case CSSValueAppleSystemGrid:
                return @selector(gridColor);
            case CSSValueAppleSystemSeparator:
#if HAVE(OS_DARK_MODE_SUPPORT)
                return @selector(separatorColor);
#else
                return @selector(gridColor);
#endif
            case CSSValueAppleWirelessPlaybackTargetActive:
            case CSSValueAppleSystemBlue:
                return @selector(systemBlueColor);
            case CSSValueAppleSystemBrown:
                return @selector(systemBrownColor);
            case CSSValueAppleSystemGray:
                return @selector(systemGrayColor);
            case CSSValueAppleSystemGreen:
                return @selector(systemGreenColor);
            case CSSValueAppleSystemOrange:
                return @selector(systemOrangeColor);
            case CSSValueAppleSystemPink:
                return @selector(systemPinkColor);
            case CSSValueAppleSystemPurple:
                return @selector(systemPurpleColor);
            case CSSValueAppleSystemRed:
                return @selector(systemRedColor);
            case CSSValueAppleSystemYellow:
                return @selector(systemYellowColor);
            default:
                return nullptr;
            }
        };

        if (auto selector = selectCocoaColor()) {
            if (auto color = wtfObjCMsgSend<NSColor *>([NSColor class], selector))
                return semanticColorFromNSColor(color);
        }

        switch (cssValueID) {
        case CSSValueActivebuttontext:
            // No corresponding NSColor for this so we use a hard coded value.
            return Color::white;

        case CSSValueButtonface:
        case CSSValueThreedface:
            // We selected this value instead of [NSColor controlColor] to avoid website incompatibilities.
            // We may want to consider changing to [NSColor controlColor] some day.
            return SimpleColor { 0xFFC0C0C0 };

        case CSSValueInfobackground:
            // No corresponding NSColor for this so we use a hard coded value.
            return SimpleColor { 0xFFFBFCC5 };

        case CSSValueMenu:
            return menuBackgroundColor();

        case CSSValueWebkitFocusRingColor:
        case CSSValueActiveborder:
            // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
            if (localAppearance.usingDarkAppearance())
                return { SimpleColor { 0xFF1AA9FF }, Color::Semantic };
            return { SimpleColor { 0xFF0067F4 }, Color::Semantic };

        case CSSValueAppleSystemControlAccent:
            // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
            // Same color in light and dark appearances.
            return { SimpleColor { 0xFF007AFF }, Color::Semantic };

        case CSSValueAppleSystemSelectedContentBackground:
            // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
            if (localAppearance.usingDarkAppearance())
                return { SimpleColor { 0xFF0058D0 }, Color::Semantic };
            return { SimpleColor { 0xFF0063E1 }, Color::Semantic };

        case CSSValueHighlight:
        case CSSValueAppleSystemSelectedTextBackground:
            // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
            if (localAppearance.usingDarkAppearance())
                return { SimpleColor { 0xCC3F638B }, Color::Semantic };
            return { SimpleColor { 0x9980BCFE }, Color::Semantic };

#if !HAVE(OS_DARK_MODE_SUPPORT)
        case CSSValueAppleSystemContainerBorder:
            return SimpleColor { 0xFFC5C5C5 };
#endif

        case CSSValueAppleSystemEvenAlternatingContentBackground: {
#if HAVE(OS_DARK_MODE_SUPPORT)
            NSArray<NSColor *> *alternateColors = [NSColor alternatingContentBackgroundColors];
#else
            NSArray<NSColor *> *alternateColors = [NSColor controlAlternatingRowBackgroundColors];
#endif
            ASSERT(alternateColors.count >= 2);
            return semanticColorFromNSColor(alternateColors[0]);
        }

        case CSSValueAppleSystemOddAlternatingContentBackground: {
#if HAVE(OS_DARK_MODE_SUPPORT)
            NSArray<NSColor *> *alternateColors = [NSColor alternatingContentBackgroundColors];
#else
            NSArray<NSColor *> *alternateColors = [NSColor controlAlternatingRowBackgroundColors];
#endif
            ASSERT(alternateColors.count >= 2);
            return semanticColorFromNSColor(alternateColors[1]);
        }

        case CSSValueBackground:
            // Use platform-independent value returned by base class.
            FALLTHROUGH;

        default:
            return RenderTheme::systemColor(cssValueID, options);
        }
    }).iterator->value;
}

bool RenderThemeMac::usesTestModeFocusRingColor() const
{
    return WebCore::usesTestModeFocusRingColor();
}

bool RenderThemeMac::isControlStyled(const RenderStyle& style, const RenderStyle& userAgentStyle) const
{
    auto appearance = style.appearance();
    if (appearance == TextFieldPart || appearance == TextAreaPart || appearance == SearchFieldPart || appearance == ListboxPart)
        return style.border() != userAgentStyle.border();

    // FIXME: This is horrible, but there is not much else that can be done.  Menu lists cannot draw properly when
    // scaled.  They can't really draw properly when transformed either.  We can't detect the transform case at style
    // adjustment time so that will just have to stay broken.  We can however detect that we're zooming.  If zooming
    // is in effect we treat it like the control is styled.
    if (appearance == MenulistPart && style.effectiveZoom() != 1.0f)
        return true;

    return RenderTheme::isControlStyled(style, userAgentStyle);
}

static FloatRect inflateRect(const FloatRect& rect, const IntSize& size, const int* margins, float zoomLevel)
{
    // Only do the inflation if the available width/height are too small. Otherwise try to
    // fit the glow/check space into the available box's width/height.
    int widthDelta = rect.width() - (size.width() + margins[leftMargin] * zoomLevel + margins[rightMargin] * zoomLevel);
    int heightDelta = rect.height() - (size.height() + margins[topMargin] * zoomLevel + margins[bottomMargin] * zoomLevel);
    FloatRect result(rect);
    if (widthDelta < 0) {
        result.setX(result.x() - margins[leftMargin] * zoomLevel);
        result.setWidth(result.width() - widthDelta);
    }
    if (heightDelta < 0) {
        result.setY(result.y() - margins[topMargin] * zoomLevel);
        result.setHeight(result.height() - heightDelta);
    }
    return result;
}

void RenderThemeMac::adjustRepaintRect(const RenderObject& renderer, FloatRect& rect)
{
    ControlPart part = renderer.style().appearance();

#if USE(NEW_THEME)
    switch (part) {
        case CheckboxPart:
        case RadioPart:
        case PushButtonPart:
        case SquareButtonPart:
#if ENABLE(INPUT_TYPE_COLOR)
        case ColorWellPart:
#endif
        case DefaultButtonPart:
        case ButtonPart:
        case InnerSpinButtonPart:
            return RenderTheme::adjustRepaintRect(renderer, rect);
        default:
            break;
    }
#endif

    float zoomLevel = renderer.style().effectiveZoom();

    if (part == MenulistPart) {
        setPopupButtonCellState(renderer, IntSize(rect.size()));
        IntSize size = popupButtonSizes()[[popupButton() controlSize]];
        size.setHeight(size.height() * zoomLevel);
        size.setWidth(rect.width());
        rect = inflateRect(rect, size, popupButtonMargins(), zoomLevel);
    }
}

static FloatPoint convertToPaintingPosition(const RenderBox& inputRenderer, const RenderBox& customButtonRenderer, const FloatPoint& customButtonLocalPosition,
    const IntPoint& paintOffset)
{
    IntPoint offsetFromInputRenderer = roundedIntPoint(customButtonRenderer.localToContainerPoint(customButtonRenderer.contentBoxRect().location(), &inputRenderer));
    FloatPoint paintingPosition = customButtonLocalPosition;
    paintingPosition.moveBy(-offsetFromInputRenderer);
    paintingPosition.moveBy(paintOffset);
    return paintingPosition;
}

void RenderThemeMac::updateCheckedState(NSCell* cell, const RenderObject& o)
{
    bool oldIndeterminate = [cell state] == NSControlStateValueMixed;
    bool indeterminate = isIndeterminate(o);
    bool checked = isChecked(o);

    if (oldIndeterminate != indeterminate) {
        [cell setState:indeterminate ? NSControlStateValueMixed : (checked ? NSControlStateValueOn : NSControlStateValueOff)];
        return;
    }

    bool oldChecked = [cell state] == NSControlStateValueOn;
    if (checked != oldChecked)
        [cell setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
}

void RenderThemeMac::updateEnabledState(NSCell* cell, const RenderObject& o)
{
    bool oldEnabled = [cell isEnabled];
    bool enabled = isEnabled(o);
    if (enabled != oldEnabled)
        [cell setEnabled:enabled];
}

void RenderThemeMac::updateFocusedState(NSCell* cell, const RenderObject& o)
{
    bool oldFocused = [cell showsFirstResponder];
    bool focused = isFocused(o) && o.style().outlineStyleIsAuto() == OutlineIsAuto::On;
    if (focused != oldFocused)
        [cell setShowsFirstResponder:focused];
}

void RenderThemeMac::updatePressedState(NSCell* cell, const RenderObject& o)
{
    bool oldPressed = [cell isHighlighted];
    bool pressed = is<Element>(o.node()) && downcast<Element>(*o.node()).active();
    if (pressed != oldPressed)
        [cell setHighlighted:pressed];
}

bool RenderThemeMac::controlSupportsTints(const RenderObject& o) const
{
    // An alternate way to implement this would be to get the appropriate cell object
    // and call the private _needRedrawOnWindowChangedKeyState method. An advantage of
    // that would be that we would match AppKit behavior more closely, but a disadvantage
    // would be that we would rely on an AppKit SPI method.

    if (!isEnabled(o))
        return false;

    // Checkboxes only have tint when checked.
    if (o.style().appearance() == CheckboxPart)
        return isChecked(o);

    // For now assume other controls have tint if enabled.
    return true;
}

NSControlSize RenderThemeMac::controlSizeForFont(const RenderStyle& style) const
{
    int fontSize = style.computedFontPixelSize();
#if HAVE(LARGE_CONTROL_SIZE)
    if (fontSize >= 21 && ThemeMac::supportsLargeFormControls())
        return NSControlSizeLarge;
#endif
    if (fontSize >= 16)
        return NSControlSizeRegular;
    if (fontSize >= 11)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

NSControlSize RenderThemeMac::controlSizeForCell(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel) const
{
#if HAVE(LARGE_CONTROL_SIZE)
    if (ThemeMac::supportsLargeFormControls()
        && minSize.width() >= static_cast<int>(sizes[NSControlSizeLarge].width() * zoomLevel)
        && minSize.height() >= static_cast<int>(sizes[NSControlSizeLarge].height() * zoomLevel))
        return NSControlSizeLarge;
#endif

    if (minSize.width() >= static_cast<int>(sizes[NSControlSizeRegular].width() * zoomLevel)
        && minSize.height() >= static_cast<int>(sizes[NSControlSizeRegular].height() * zoomLevel))
        return NSControlSizeRegular;

    if (minSize.width() >= static_cast<int>(sizes[NSControlSizeSmall].width() * zoomLevel)
        && minSize.height() >= static_cast<int>(sizes[NSControlSizeSmall].height() * zoomLevel))
        return NSControlSizeSmall;

    return NSControlSizeMini;
}

void RenderThemeMac::setControlSize(NSCell* cell, const IntSize* sizes, const IntSize& minSize, float zoomLevel)
{
    NSControlSize size = controlSizeForCell(cell, sizes, minSize, zoomLevel);
    if (size != [cell controlSize]) // Only update if we have to, since AppKit does work even if the size is the same.
        [cell setControlSize:size];
}

IntSize RenderThemeMac::sizeForFont(const RenderStyle& style, const IntSize* sizes) const
{
    if (style.effectiveZoom() != 1.0f) {
        IntSize result = sizes[controlSizeForFont(style)];
        return IntSize(result.width() * style.effectiveZoom(), result.height() * style.effectiveZoom());
    }
    return sizes[controlSizeForFont(style)];
}

IntSize RenderThemeMac::sizeForSystemFont(const RenderStyle& style, const IntSize* sizes) const
{
    if (style.effectiveZoom() != 1.0f) {
        IntSize result = sizes[controlSizeForSystemFont(style)];
        return IntSize(result.width() * style.effectiveZoom(), result.height() * style.effectiveZoom());
    }
    return sizes[controlSizeForSystemFont(style)];
}

void RenderThemeMac::setSizeFromFont(RenderStyle& style, const IntSize* sizes) const
{
    // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
    IntSize size = sizeForFont(style, sizes);
    if (style.width().isIntrinsicOrAuto() && size.width() > 0)
        style.setWidth(Length(size.width(), Fixed));
    if (style.height().isAuto() && size.height() > 0)
        style.setHeight(Length(size.height(), Fixed));
}

void RenderThemeMac::setFontFromControlSize(RenderStyle& style, NSControlSize controlSize) const
{
    FontCascadeDescription fontDescription;
    fontDescription.setIsAbsoluteSize(true);

    NSFont* font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSize]];
    fontDescription.setOneFamily(AtomString("-apple-system", AtomString::ConstructFromLiteral));
    fontDescription.setComputedSize([font pointSize] * style.effectiveZoom());
    fontDescription.setSpecifiedSize([font pointSize] * style.effectiveZoom());

    // Reset line height
    style.setLineHeight(RenderStyle::initialLineHeight());

    if (style.setFontDescription(WTFMove(fontDescription)))
        style.fontCascade().update(0);
}

NSControlSize RenderThemeMac::controlSizeForSystemFont(const RenderStyle& style) const
{
    int fontSize = style.computedFontPixelSize();
#if HAVE(LARGE_CONTROL_SIZE)
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSControlSizeLarge] && ThemeMac::supportsLargeFormControls())
        return NSControlSizeLarge;
#endif
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSControlSizeRegular])
        return NSControlSizeRegular;
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSControlSizeSmall])
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

#if ENABLE(DATALIST_ELEMENT)

void RenderThemeMac::paintListButtonForInput(const RenderObject& o, GraphicsContext& context, const FloatRect& r)
{
    // We can't paint an NSComboBoxCell since they are not height-resizable.
    const auto& input = downcast<HTMLInputElement>(*(o.generatingNode()));
    NSCell *listButton = this->listButton();

    NSRect listButtonFrame = NSMakeRect(r.maxX() - listButtonWidth, r.y(), listButtonWidth, r.height());
    if (!o.style().isLeftToRightDirection()) {
        listButtonFrame.origin.x = r.x();
        [listButton setUserInterfaceLayoutDirection:NSUserInterfaceLayoutDirectionRightToLeft];
    } else
        [listButton setUserInterfaceLayoutDirection:NSUserInterfaceLayoutDirectionLeftToRight];

    [listButton setHighlighted:input.isPresentingAttachedView()];
    if (!input.isPresentingAttachedView()) {
        ASSERT(input.dataListButtonElement());
        if (auto* buttonElement = input.dataListButtonElement())
            updatePressedState(listButton, *buttonElement->renderer());
    }

    [listButton drawWithFrame:listButtonFrame inView:documentViewFor(o)];
    [listButton setControlView:nil];

    RefPtr<Image> image;
    float imageScale = 1;
    if (o.document().deviceScaleFactor() >= 2) {
        image = Image::loadPlatformResource("ListButtonArrow@2x");
        imageScale = 2;
    } else
        image = Image::loadPlatformResource("ListButtonArrow");

    FloatRect imageRect(0, 0, image->width() / imageScale, image->height() / imageScale);
    imageRect.setX(NSMidX(listButtonFrame) - imageRect.width() / 2);
    imageRect.setY(NSMidY(listButtonFrame) - imageRect.height() / 2);

    context.drawImage(*image, imageRect);
}

void RenderThemeMac::adjustListButtonStyle(RenderStyle& style, const Element*) const
{
    // Add a margin to place the button at end of the input field.
    if (style.isLeftToRightDirection())
        style.setMarginRight(Length(-4, Fixed));
    else
        style.setMarginLeft(Length(-4, Fixed));
}

#endif

bool RenderThemeMac::paintTextField(const RenderObject& o, const PaintInfo& paintInfo, const FloatRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context());

    // <rdar://problem/22896977> We adjust the paint rect here to account for how AppKit draws the text
    // field cell slightly smaller than the rect we pass to drawWithFrame.
    FloatRect adjustedPaintRect(r);
    AffineTransform transform = paintInfo.context().getCTM();
    if (transform.xScale() > 1 || transform.yScale() > 1) {
        adjustedPaintRect.inflateX(1 / transform.xScale());
        adjustedPaintRect.inflateY(2 / transform.yScale());
        adjustedPaintRect.move(0, -1 / transform.yScale());
    }
    NSTextFieldCell *textField = this->textField();

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    [textField setEnabled:(isEnabled(o) && !isReadOnlyControl(o))];
    [textField drawWithFrame:NSRect(adjustedPaintRect) inView:documentViewFor(o)];

    [textField setControlView:nil];

#if ENABLE(DATALIST_ELEMENT)
    if (!is<HTMLInputElement>(o.generatingNode()))
        return false;

    const auto& input = downcast<HTMLInputElement>(*(o.generatingNode()));
    if (input.list())
        paintListButtonForInput(o, paintInfo.context(), adjustedPaintRect);
#endif

    return false;
}

void RenderThemeMac::adjustTextFieldStyle(RenderStyle&, const Element*) const
{
}

bool RenderThemeMac::paintTextArea(const RenderObject& o, const PaintInfo& paintInfo, const FloatRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context());
    _NSDrawCarbonThemeListBox(r, isEnabled(o) && !isReadOnlyControl(o), YES, YES);
    return false;
}

void RenderThemeMac::adjustTextAreaStyle(RenderStyle&, const Element*) const
{
}

const int* RenderThemeMac::popupButtonMargins() const
{
    static const int margins[4][4] =
    {
        { 0, 3, 1, 3 },
        { 0, 3, 2, 3 },
        { 0, 1, 0, 1 },
        { 0, 6, 2, 6 },
    };
    return margins[[popupButton() controlSize]];
}

const IntSize* RenderThemeMac::popupButtonSizes() const
{
    static const IntSize sizes[4] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15), IntSize(0, 24) };
    return sizes;
}

const int* RenderThemeMac::popupButtonPadding(NSControlSize size, bool isRTL) const
{
    static const int paddingLTR[4][4] =
    {
        { 2, 26, 3, 8 },
        { 2, 23, 3, 8 },
        { 2, 22, 3, 10 },
        { 2, 26, 3, 8 },
    };
    static const int paddingRTL[4][4] =
    {
        { 2, 8, 3, 26 },
        { 2, 8, 3, 23 },
        { 2, 8, 3, 22 },
        { 2, 8, 3, 26 },
    };
    return isRTL ? paddingRTL[size] : paddingLTR[size];
}

bool RenderThemeMac::paintMenuList(const RenderObject& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context());
    setPopupButtonCellState(renderer, IntSize(rect.size()));

    NSPopUpButtonCell* popupButton = this->popupButton();

    float zoomLevel = renderer.style().effectiveZoom();
    IntSize size = popupButtonSizes()[[popupButton controlSize]];
    size.setHeight(size.height() * zoomLevel);
    size.setWidth(rect.width());

    // Now inflate it to account for the shadow.
    FloatRect inflatedRect = rect;
    if (rect.width() >= minimumMenuListSize(renderer.style()))
        inflatedRect = inflateRect(rect, size, popupButtonMargins(), zoomLevel);

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    if (zoomLevel != 1.0f) {
        inflatedRect.setWidth(inflatedRect.width() / zoomLevel);
        inflatedRect.setHeight(inflatedRect.height() / zoomLevel);
        paintInfo.context().translate(inflatedRect.location());
        paintInfo.context().scale(zoomLevel);
        paintInfo.context().translate(-inflatedRect.location());
    }

    paintCellAndSetFocusedElementNeedsRepaintIfNecessary(popupButton, renderer, paintInfo, inflatedRect);
    [popupButton setControlView:nil];

    return false;
}

#if ENABLE(METER_ELEMENT)

IntSize RenderThemeMac::meterSizeForBounds(const RenderMeter& renderMeter, const IntRect& bounds) const
{
    if (NoControlPart == renderMeter.style().appearance())
        return bounds.size();

    NSLevelIndicatorCell* cell = levelIndicatorFor(renderMeter);
    // Makes enough room for cell's intrinsic size.
    NSSize cellSize = [cell cellSizeForBounds:IntRect(IntPoint(), bounds.size())];
    return IntSize(bounds.width() < cellSize.width ? cellSize.width : bounds.width(),
                   bounds.height() < cellSize.height ? cellSize.height : bounds.height());
}

bool RenderThemeMac::paintMeter(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!is<RenderMeter>(renderObject))
        return true;

    LocalCurrentGraphicsContext localContext(paintInfo.context());

    NSLevelIndicatorCell* cell = levelIndicatorFor(downcast<RenderMeter>(renderObject));
    GraphicsContextStateSaver stateSaver(paintInfo.context());

    [cell drawWithFrame:rect inView:documentViewFor(renderObject)];
    [cell setControlView:nil];
    return false;
}

bool RenderThemeMac::supportsMeter(ControlPart part) const
{
    switch (part) {
    case RelevancyLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
    case MeterPart:
    case ContinuousCapacityLevelIndicatorPart:
        return true;
    default:
        return false;
    }
}

NSLevelIndicatorStyle RenderThemeMac::levelIndicatorStyleFor(ControlPart part) const
{
    switch (part) {
    case RelevancyLevelIndicatorPart:
        return NSLevelIndicatorStyleRelevancy;
    case DiscreteCapacityLevelIndicatorPart:
        return NSLevelIndicatorStyleDiscreteCapacity;
    case RatingLevelIndicatorPart:
        return NSLevelIndicatorStyleRating;
    case MeterPart:
    case ContinuousCapacityLevelIndicatorPart:
    default:
        return NSLevelIndicatorStyleContinuousCapacity;
    }

}

NSLevelIndicatorCell* RenderThemeMac::levelIndicatorFor(const RenderMeter& renderMeter) const
{
    const RenderStyle& style = renderMeter.style();
    ASSERT(style.appearance() != NoControlPart);

    if (!m_levelIndicator)
        m_levelIndicator = adoptNS([[NSLevelIndicatorCell alloc] initWithLevelIndicatorStyle:NSLevelIndicatorStyleContinuousCapacity]);
    NSLevelIndicatorCell* cell = m_levelIndicator.get();

    HTMLMeterElement* element = renderMeter.meterElement();
    double value = element->value();

    // Because NSLevelIndicatorCell does not support optimum-in-the-middle type coloring,
    // we explicitly control the color instead giving low and high value to NSLevelIndicatorCell as is.
    switch (element->gaugeRegion()) {
    case HTMLMeterElement::GaugeRegionOptimum:
        // Make meter the green
        [cell setWarningValue:value + 1];
        [cell setCriticalValue:value + 2];
        break;
    case HTMLMeterElement::GaugeRegionSuboptimal:
        // Make the meter yellow
        [cell setWarningValue:value - 1];
        [cell setCriticalValue:value + 1];
        break;
    case HTMLMeterElement::GaugeRegionEvenLessGood:
        // Make the meter red
        [cell setWarningValue:value - 2];
        [cell setCriticalValue:value - 1];
        break;
    }

    [cell setLevelIndicatorStyle:levelIndicatorStyleFor(style.appearance())];
    [cell setUserInterfaceLayoutDirection:style.isLeftToRightDirection() ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft];
    [cell setMinValue:element->min()];
    [cell setMaxValue:element->max()];
    [cell setObjectValue:@(value)];

    return cell;
}

#endif

const IntSize* RenderThemeMac::progressBarSizes() const
{
    static const IntSize sizes[4] = { IntSize(0, 20), IntSize(0, 12), IntSize(0, 12), IntSize(0, 20) };
    return sizes;
}

const int* RenderThemeMac::progressBarMargins(NSControlSize controlSize) const
{
    static const int margins[4][4] =
    {
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
    };
    return margins[controlSize];
}

IntRect RenderThemeMac::progressBarRectForBounds(const RenderObject& renderObject, const IntRect& bounds) const
{
    // Workaround until <rdar://problem/15855086> is fixed.
    int maxDimension = static_cast<int>(std::numeric_limits<ushort>::max());
    IntRect progressBarBounds(bounds.x(), bounds.y(), std::min(bounds.width(), maxDimension), std::min(bounds.height(), maxDimension));
    if (NoControlPart == renderObject.style().appearance())
        return progressBarBounds;

    float zoomLevel = renderObject.style().effectiveZoom();
    NSControlSize controlSize = controlSizeForFont(renderObject.style());
    IntSize size = progressBarSizes()[controlSize];
    size.setHeight(size.height() * zoomLevel);
    size.setWidth(progressBarBounds.width());

    // Now inflate it to account for the shadow.
    IntRect inflatedRect = progressBarBounds;
    if (progressBarBounds.height() <= minimumProgressBarHeight(renderObject.style()))
        inflatedRect = IntRect(inflateRect(inflatedRect, size, progressBarMargins(controlSize), zoomLevel));

    return inflatedRect;
}

int RenderThemeMac::minimumProgressBarHeight(const RenderStyle& style) const
{
    return sizeForSystemFont(style, progressBarSizes()).height();
}

Seconds RenderThemeMac::animationRepeatIntervalForProgressBar(RenderProgress&) const
{
    return progressAnimationFrameRate;
}

Seconds RenderThemeMac::animationDurationForProgressBar(RenderProgress&) const
{
    return progressAnimationFrameRate * progressAnimationNumFrames;
}

void RenderThemeMac::adjustProgressBarStyle(RenderStyle&, const Element*) const
{
}

bool RenderThemeMac::paintProgressBar(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!is<RenderProgress>(renderObject))
        return true;

    IntRect inflatedRect = progressBarRectForBounds(renderObject, rect);
    NSControlSize controlSize = controlSizeForFont(renderObject.style());
    const auto& renderProgress = downcast<RenderProgress>(renderObject);
    float deviceScaleFactor = renderObject.document().deviceScaleFactor();
    bool isIndeterminate = renderProgress.position() < 0;
    auto animationFrame = lround(renderProgress.animationProgress() * nextafter(progressAnimationNumFrames, 0) * deviceScaleFactor);
    auto imageBuffer = ImageBuffer::createCompatibleBuffer(inflatedRect.size(), deviceScaleFactor, ColorSpace::SRGB, paintInfo.context());
    if (!imageBuffer)
        return true;

    ContextContainer cgContextContainer(imageBuffer->context());
    CGContextRef cgContext = cgContextContainer.context();

    auto coreUISizeForProgressBarSize = [](NSControlSize size) -> CFStringRef {
        switch (size) {
        case NSControlSizeMini:
        case NSControlSizeSmall:
            return kCUISizeSmall;
        case NSControlSizeRegular:
#if HAVE(LARGE_CONTROL_SIZE)
        case NSControlSizeLarge:
#endif
            return kCUISizeRegular;
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    };
    [[NSAppearance currentAppearance] _drawInRect:NSMakeRect(0, 0, inflatedRect.width(), inflatedRect.height()) context:cgContext options:@{
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)(isIndeterminate ? kCUIWidgetProgressIndeterminateBar : kCUIWidgetProgressBar),
        (__bridge NSString *)kCUIValueKey: @(isIndeterminate ? 1 : std::min(nextafter(1.0, -1), renderProgress.position())),
        (__bridge NSString *)kCUISizeKey: (__bridge NSString *)coreUISizeForProgressBarSize(controlSize),
        (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: (__bridge NSString *)kCUIUserInterfaceLayoutDirectionLeftToRight,
        (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
        (__bridge NSString *)kCUIPresentationStateKey: (__bridge NSString *)(isActive(renderObject) ? kCUIPresentationStateActiveKey : kCUIPresentationStateInactive),
        (__bridge NSString *)kCUIOrientationKey: (__bridge NSString *)kCUIOrientHorizontal,
        (__bridge NSString *)kCUIAnimationFrameKey: @(isIndeterminate ? animationFrame : 0)
    }];

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    if (!renderProgress.style().isLeftToRightDirection()) {
        paintInfo.context().translate(2 * inflatedRect.x() + inflatedRect.width(), 0);
        paintInfo.context().scale(FloatSize(-1, 1));
    }

    paintInfo.context().drawConsumingImageBuffer(WTFMove(imageBuffer), inflatedRect.location());
    return false;
}

const float baseFontSize = 11.0f;
const float baseArrowHeight = 4.0f;
const float baseArrowWidth = 5.0f;
const float baseSpaceBetweenArrows = 2.0f;
const int arrowPaddingBefore = 6;
const int arrowPaddingAfter = 6;
const int paddingBeforeSeparator = 4;
const int baseBorderRadius = 5;
const int styledPopupPaddingLeft = 8;
const int styledPopupPaddingTop = 1;
const int styledPopupPaddingBottom = 2;

static void TopGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static const float dark[4] = { 1.0f, 1.0f, 1.0f, 0.4f };
    static const float light[4] = { 1.0f, 1.0f, 1.0f, 0.15f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

static void BottomGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static const float dark[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
    static const float light[4] = { 1.0f, 1.0f, 1.0f, 0.3f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

static void MainGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static const float dark[4] = { 0.0f, 0.0f, 0.0f, 0.15f };
    static const float light[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

static void TrackGradientInterpolate(void*, const CGFloat* inData, CGFloat* outData)
{
    static const float dark[4] = { 0.0f, 0.0f, 0.0f, 0.678f };
    static const float light[4] = { 0.0f, 0.0f, 0.0f, 0.13f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

void RenderThemeMac::paintMenuListButtonGradients(const RenderObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    if (r.isEmpty())
        return;

    ContextContainer cgContextContainer(paintInfo.context());
    CGContextRef context = cgContextContainer.context();

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    FloatRoundedRect border = FloatRoundedRect(o.style().getRoundedBorderFor(r));
    int radius = border.radii().topLeft().width();

    CGColorSpaceRef cspace = sRGBColorSpaceRef();

    FloatRect topGradient(r.x(), r.y(), r.width(), r.height() / 2.0f);
    struct CGFunctionCallbacks topCallbacks = { 0, TopGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> topFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &topCallbacks));
    RetainPtr<CGShadingRef> topShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(topGradient.x(), topGradient.y()), CGPointMake(topGradient.x(), topGradient.maxY()), topFunction.get(), false, false));

    FloatRect bottomGradient(r.x() + radius, r.y() + r.height() / 2.0f, r.width() - 2.0f * radius, r.height() / 2.0f);
    struct CGFunctionCallbacks bottomCallbacks = { 0, BottomGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> bottomFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &bottomCallbacks));
    RetainPtr<CGShadingRef> bottomShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(bottomGradient.x(),  bottomGradient.y()), CGPointMake(bottomGradient.x(), bottomGradient.maxY()), bottomFunction.get(), false, false));

    struct CGFunctionCallbacks mainCallbacks = { 0, MainGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> mainFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &mainCallbacks));
    RetainPtr<CGShadingRef> mainShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(r.x(),  r.y()), CGPointMake(r.x(), r.maxY()), mainFunction.get(), false, false));

    RetainPtr<CGShadingRef> leftShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(r.x(),  r.y()), CGPointMake(r.x() + radius, r.y()), mainFunction.get(), false, false));

    RetainPtr<CGShadingRef> rightShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(r.maxX(),  r.y()), CGPointMake(r.maxX() - radius, r.y()), mainFunction.get(), false, false));

    {
        GraphicsContextStateSaver stateSaver(paintInfo.context());
        CGContextClipToRect(context, r);
        paintInfo.context().clipRoundedRect(border);
        context = cgContextContainer.context();
        CGContextDrawShading(context, mainShading.get());
    }

    {
        GraphicsContextStateSaver stateSaver(paintInfo.context());
        CGContextClipToRect(context, topGradient);
        paintInfo.context().clipRoundedRect(FloatRoundedRect(enclosingIntRect(topGradient), border.radii().topLeft(), border.radii().topRight(), IntSize(), IntSize()));
        context = cgContextContainer.context();
        CGContextDrawShading(context, topShading.get());
    }

    if (!bottomGradient.isEmpty()) {
        GraphicsContextStateSaver stateSaver(paintInfo.context());
        CGContextClipToRect(context, bottomGradient);
        paintInfo.context().clipRoundedRect(FloatRoundedRect(enclosingIntRect(bottomGradient), IntSize(), IntSize(), border.radii().bottomLeft(), border.radii().bottomRight()));
        context = cgContextContainer.context();
        CGContextDrawShading(context, bottomShading.get());
    }

    {
        GraphicsContextStateSaver stateSaver(paintInfo.context());
        CGContextClipToRect(context, r);
        paintInfo.context().clipRoundedRect(border);
        context = cgContextContainer.context();
        CGContextDrawShading(context, leftShading.get());
        CGContextDrawShading(context, rightShading.get());
    }
}

bool RenderThemeMac::paintMenuListButtonDecorations(const RenderBox& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
    bool isRTL = renderer.style().direction() == TextDirection::RTL;
    IntRect bounds = IntRect(rect.x() + renderer.style().borderLeftWidth(),
        rect.y() + renderer.style().borderTopWidth(),
        rect.width() - renderer.style().borderLeftWidth() - renderer.style().borderRightWidth(),
        rect.height() - renderer.style().borderTopWidth() - renderer.style().borderBottomWidth());
    // Draw the gradients to give the styled popup menu a button appearance
    paintMenuListButtonGradients(renderer, paintInfo, bounds);

    // Since we actually know the size of the control here, we restrict the font scale to make sure the arrows will fit vertically in the bounds
    float fontScale = std::min(renderer.style().computedFontPixelSize() / baseFontSize, bounds.height() / (baseArrowHeight * 2 + baseSpaceBetweenArrows));
    float centerY = bounds.y() + bounds.height() / 2.0f;
    float arrowHeight = baseArrowHeight * fontScale;
    float arrowWidth = baseArrowWidth * fontScale;
    float leftEdge;
    if (isRTL)
        leftEdge = bounds.x() + arrowPaddingAfter * renderer.style().effectiveZoom();
    else
        leftEdge = bounds.maxX() - arrowPaddingAfter * renderer.style().effectiveZoom() - arrowWidth;
    float spaceBetweenArrows = baseSpaceBetweenArrows * fontScale;

    if (bounds.width() < arrowWidth + arrowPaddingBefore * renderer.style().effectiveZoom())
        return false;

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    paintInfo.context().setFillColor(renderer.style().visitedDependentColor(CSSPropertyColor));
    paintInfo.context().setStrokeStyle(NoStroke);

    // Draw the top arrow
    Vector<FloatPoint> arrow1 = {
        { leftEdge, centerY - spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth, centerY - spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth / 2.0f, centerY - spaceBetweenArrows / 2.0f - arrowHeight }
    };
    paintInfo.context().fillPath(Path::polygonPathFromPoints(arrow1));

    // Draw the bottom arrow
    Vector<FloatPoint> arrow2 = {
        { leftEdge, centerY + spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth, centerY + spaceBetweenArrows / 2.0f },
        { leftEdge + arrowWidth / 2.0f, centerY + spaceBetweenArrows / 2.0f + arrowHeight }
    };
    paintInfo.context().fillPath(Path::polygonPathFromPoints(arrow2));

    constexpr auto leftSeparatorColor = makeSimpleColor(0, 0, 0, 40);
    constexpr auto rightSeparatorColor = makeSimpleColor(255, 255, 255, 40);

    // FIXME: Should the separator thickness and space be scaled up by fontScale?
    int separatorSpace = 2; // Deliberately ignores zoom since it looks nicer if it stays thin.
    int leftEdgeOfSeparator;
    if (isRTL)
        leftEdgeOfSeparator = static_cast<int>(roundf(leftEdge + arrowWidth + arrowPaddingBefore * renderer.style().effectiveZoom()));
    else
        leftEdgeOfSeparator = static_cast<int>(roundf(leftEdge - arrowPaddingBefore * renderer.style().effectiveZoom()));

    // Draw the separator to the left of the arrows
    paintInfo.context().setStrokeThickness(1); // Deliberately ignores zoom since it looks nicer if it stays thin.
    paintInfo.context().setStrokeStyle(SolidStroke);
    paintInfo.context().setStrokeColor(leftSeparatorColor);
    paintInfo.context().drawLine(IntPoint(leftEdgeOfSeparator, bounds.y()),
        IntPoint(leftEdgeOfSeparator, bounds.maxY()));

    paintInfo.context().setStrokeColor(rightSeparatorColor);
    paintInfo.context().drawLine(IntPoint(leftEdgeOfSeparator + separatorSpace, bounds.y()),
        IntPoint(leftEdgeOfSeparator + separatorSpace, bounds.maxY()));
    return false;
}

static const IntSize* menuListButtonSizes()
{
    static const IntSize sizes[4] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15), IntSize(0, 28) };
    return sizes;
}

void RenderThemeMac::adjustMenuListStyle(RenderStyle& style, const Element* e) const
{
    NSControlSize controlSize = controlSizeForFont(style);

    style.resetBorder();
    style.resetPadding();

    // Height is locked to auto.
    style.setHeight(Length(Auto));

    // White-space is locked to pre
    style.setWhiteSpace(WhiteSpace::Pre);

    // Set the foreground color to black or gray when we have the aqua look.
    Color c = Color::darkGray;
    if (e) {
        OptionSet<StyleColor::Options> options = e->document().styleColorOptions(&style);
        c = !e->isDisabledFormControl() ? systemColor(CSSValueButtontext, options) : systemColor(CSSValueGraytext, options);
    }
    style.setColor(c);

    // Set the button's vertical size.
    setSizeFromFont(style, menuListButtonSizes());

    // Our font is locked to the appropriate system font size for the control.  To clarify, we first use the CSS-specified font to figure out
    // a reasonable control size, but once that control size is determined, we throw that font away and use the appropriate
    // system font for the control size instead.
    setFontFromControlSize(style, controlSize);

    style.setBoxShadow(nullptr);
}

LengthBox RenderThemeMac::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.appearance() == MenulistPart) {
        const int* padding = popupButtonPadding(controlSizeForFont(style), style.direction() == TextDirection::RTL);
        return { static_cast<int>(padding[topPadding] * style.effectiveZoom()),
            static_cast<int>(padding[rightPadding] * style.effectiveZoom()),
            static_cast<int>(padding[bottomPadding] * style.effectiveZoom()),
            static_cast<int>(padding[leftPadding] * style.effectiveZoom()) };
    }

    if (style.appearance() == MenulistButtonPart) {
        float arrowWidth = baseArrowWidth * (style.computedFontPixelSize() / baseFontSize);
        float rightPadding = ceilf(arrowWidth + (arrowPaddingBefore + arrowPaddingAfter + paddingBeforeSeparator) * style.effectiveZoom());
        float leftPadding = styledPopupPaddingLeft * style.effectiveZoom();
        if (style.direction() == TextDirection::RTL)
            std::swap(rightPadding, leftPadding);
        return { static_cast<int>(styledPopupPaddingTop * style.effectiveZoom()),
            static_cast<int>(rightPadding),
            static_cast<int>(styledPopupPaddingBottom * style.effectiveZoom()),
            static_cast<int>(leftPadding) };
    }

    return { 0, 0, 0, 0 };
}

PopupMenuStyle::PopupMenuSize RenderThemeMac::popupMenuSize(const RenderStyle& style, IntRect& rect) const
{
    NSPopUpButtonCell* popupButton = this->popupButton();
    NSControlSize size = controlSizeForCell(popupButton, popupButtonSizes(), rect.size(), style.effectiveZoom());
    switch (size) {
    case NSControlSizeRegular:
        return PopupMenuStyle::PopupMenuSizeNormal;
    case NSControlSizeSmall:
        return PopupMenuStyle::PopupMenuSizeSmall;
    case NSControlSizeMini:
        return PopupMenuStyle::PopupMenuSizeMini;
#if HAVE(LARGE_CONTROL_SIZE)
    case NSControlSizeLarge:
        return ThemeMac::supportsLargeFormControls() ? PopupMenuStyle::PopupMenuSizeLarge : PopupMenuStyle::PopupMenuSizeNormal;
#endif
    default:
        return PopupMenuStyle::PopupMenuSizeNormal;
    }
}

void RenderThemeMac::adjustMenuListButtonStyle(RenderStyle& style, const Element*) const
{
    float fontScale = style.computedFontPixelSize() / baseFontSize;

    style.resetPadding();
    style.setBorderRadius(IntSize(int(baseBorderRadius + fontScale - 1), int(baseBorderRadius + fontScale - 1))); // FIXME: Round up?

    const int minHeight = 15;
    style.setMinHeight(Length(minHeight, Fixed));

    style.setLineHeight(RenderStyle::initialLineHeight());
}

void RenderThemeMac::setPopupButtonCellState(const RenderObject& o, const IntSize& buttonSize)
{
    NSPopUpButtonCell* popupButton = this->popupButton();

    // Set the control size based off the rectangle we're painting into.
    setControlSize(popupButton, popupButtonSizes(), buttonSize, o.style().effectiveZoom());

    popupButton.userInterfaceLayoutDirection = o.style().direction() == TextDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft;

    // Update the various states we respond to.
    updateCheckedState(popupButton, o);
    updateEnabledState(popupButton, o);
    updatePressedState(popupButton, o);
}

void RenderThemeMac::paintCellAndSetFocusedElementNeedsRepaintIfNecessary(NSCell* cell, const RenderObject& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
    LocalDefaultSystemAppearance localAppearance(renderer.useDarkAppearance());
    bool shouldDrawFocusRing = isFocused(renderer) && renderer.style().outlineStyleIsAuto() == OutlineIsAuto::On;
    bool shouldDrawCell = true;
    if (ThemeMac::drawCellOrFocusRingWithViewIntoContext(cell, paintInfo.context(), rect, documentViewFor(renderer), shouldDrawCell, shouldDrawFocusRing, renderer.page().deviceScaleFactor()))
        renderer.page().focusController().setFocusedElementNeedsRepaint();
}

const IntSize* RenderThemeMac::menuListSizes() const
{
    static const IntSize sizes[4] = { IntSize(9, 0), IntSize(5, 0), IntSize(0, 0), IntSize(13, 0) };
    return sizes;
}

int RenderThemeMac::minimumMenuListSize(const RenderStyle& style) const
{
    return sizeForSystemFont(style, menuListSizes()).width();
}

const int trackWidth = 5;
const int trackRadius = 2;

void RenderThemeMac::adjustSliderTrackStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(nullptr);
}

bool RenderThemeMac::paintSliderTrack(const RenderObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    IntRect bounds = r;
    float zoomLevel = o.style().effectiveZoom();
    float zoomedTrackWidth = trackWidth * zoomLevel;

    if (o.style().appearance() ==  SliderHorizontalPart || o.style().appearance() ==  MediaSliderPart) {
        bounds.setHeight(zoomedTrackWidth);
        bounds.setY(r.y() + r.height() / 2 - zoomedTrackWidth / 2);
    } else if (o.style().appearance() == SliderVerticalPart) {
        bounds.setWidth(zoomedTrackWidth);
        bounds.setX(r.x() + r.width() / 2 - zoomedTrackWidth / 2);
    }

    LocalCurrentGraphicsContext localContext(paintInfo.context());
    CGContextRef context = localContext.cgContext();
    CGColorSpaceRef cspace = sRGBColorSpaceRef();

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(o, paintInfo, r);
#endif

    GraphicsContextStateSaver stateSaver(paintInfo.context());
    CGContextClipToRect(context, bounds);

    struct CGFunctionCallbacks mainCallbacks = { 0, TrackGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> mainFunction = adoptCF(CGFunctionCreate(NULL, 1, NULL, 4, NULL, &mainCallbacks));
    RetainPtr<CGShadingRef> mainShading;
    if (o.style().appearance() == SliderVerticalPart)
        mainShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(bounds.x(),  bounds.maxY()), CGPointMake(bounds.maxX(), bounds.maxY()), mainFunction.get(), false, false));
    else
        mainShading = adoptCF(CGShadingCreateAxial(cspace, CGPointMake(bounds.x(),  bounds.y()), CGPointMake(bounds.x(), bounds.maxY()), mainFunction.get(), false, false));

    IntSize radius(trackRadius, trackRadius);
    paintInfo.context().clipRoundedRect(FloatRoundedRect(bounds, radius, radius, radius, radius));
    context = localContext.cgContext();
    CGContextDrawShading(context, mainShading.get());

    return false;
}

void RenderThemeMac::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(style, element);
    style.setBoxShadow(nullptr);
}

const float verticalSliderHeightPadding = 0.1f;

bool RenderThemeMac::paintSliderThumb(const RenderObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    NSSliderCell* sliderThumbCell = o.style().appearance() == SliderThumbVerticalPart
        ? sliderThumbVertical()
        : sliderThumbHorizontal();

    LocalDefaultSystemAppearance localAppearance(o.useDarkAppearance());

    LocalCurrentGraphicsContext localContext(paintInfo.context());

    // Update the various states we respond to.
    updateEnabledState(sliderThumbCell, o);
    auto focusDelegate = is<Element>(o.node()) ? downcast<Element>(*o.node()).focusDelegate() : nullptr;
    if (focusDelegate)
        updateFocusedState(sliderThumbCell, *focusDelegate->renderer());

    // Update the pressed state using the NSCell tracking methods, since that's how NSSliderCell keeps track of it.
    bool oldPressed;
    if (o.style().appearance() == SliderThumbVerticalPart)
        oldPressed = m_isSliderThumbVerticalPressed;
    else
        oldPressed = m_isSliderThumbHorizontalPressed;

    bool pressed = isPressed(o);

    if (o.style().appearance() == SliderThumbVerticalPart)
        m_isSliderThumbVerticalPressed = pressed;
    else
        m_isSliderThumbHorizontalPressed = pressed;

    NSView *view = documentViewFor(o);

    if (pressed != oldPressed) {
        if (pressed)
            [sliderThumbCell startTrackingAt:NSPoint() inView:view];
        else
            [sliderThumbCell stopTracking:NSPoint() at:NSPoint() inView:view mouseIsUp:YES];
    }

    FloatRect bounds = r;
    // Make the height of the vertical slider slightly larger so NSSliderCell will draw a vertical slider.
    if (o.style().appearance() == SliderThumbVerticalPart)
        bounds.setHeight(bounds.height() + verticalSliderHeightPadding * o.style().effectiveZoom());

    GraphicsContextStateSaver stateSaver(paintInfo.context());
    float zoomLevel = o.style().effectiveZoom();

    FloatRect unzoomedRect = bounds;
    if (zoomLevel != 1.0f) {
        unzoomedRect.setSize(unzoomedRect.size() / zoomLevel);
        paintInfo.context().translate(unzoomedRect.location());
        paintInfo.context().scale(zoomLevel);
        paintInfo.context().translate(-unzoomedRect.location());
    }

    bool shouldDrawCell = true;
    bool shouldDrawFocusRing = false;
    float deviceScaleFactor = o.page().deviceScaleFactor();
    ThemeMac::drawCellOrFocusRingWithViewIntoContext(sliderThumbCell, paintInfo.context(), unzoomedRect, view, shouldDrawCell, shouldDrawFocusRing, deviceScaleFactor);
    [sliderThumbCell setControlView:nil];

    return false;
}

bool RenderThemeMac::paintSearchField(const RenderObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context());
    NSSearchFieldCell* search = this->search();

    setSearchCellState(o, r);

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    float zoomLevel = o.style().effectiveZoom();

    FloatRect unzoomedRect = r;
    if (zoomLevel != 1.0f) {
        unzoomedRect.setSize(unzoomedRect.size() / zoomLevel);
        paintInfo.context().translate(unzoomedRect.location());
        paintInfo.context().scale(zoomLevel);
        paintInfo.context().translate(-unzoomedRect.location());
    }

    // Set the search button to nil before drawing.  Then reset it so we can draw it later.
    [search setSearchButtonCell:nil];

    paintCellAndSetFocusedElementNeedsRepaintIfNecessary(search, o, paintInfo, unzoomedRect);
    [search setControlView:nil];
    [search resetSearchButtonCell];

#if ENABLE(DATALIST_ELEMENT)
    if (!is<HTMLInputElement>(o.generatingNode()))
        return false;

    const auto& input = downcast<HTMLInputElement>(*(o.generatingNode()));
    if (input.list())
        paintListButtonForInput(o, paintInfo.context(), FloatRect(unzoomedRect.x(), unzoomedRect.y() + 1, unzoomedRect.width(), unzoomedRect.height() - 2));
#endif

    return false;
}

void RenderThemeMac::setSearchCellState(const RenderObject& o, const IntRect&)
{
    NSSearchFieldCell* search = this->search();

    [search setPlaceholderString:@""];
    [search setControlSize:controlSizeForFont(o.style())];

    // Update the various states we respond to.
    updateEnabledState(search, o);
    updateFocusedState(search, o);
}

const IntSize* RenderThemeMac::searchFieldSizes() const
{
    static const IntSize sizes[4] = { IntSize(0, 22), IntSize(0, 19), IntSize(0, 17), IntSize(0, 22) };
    return sizes;
}

void RenderThemeMac::setSearchFieldSize(RenderStyle& style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style, searchFieldSizes());
}

void RenderThemeMac::adjustSearchFieldStyle(RenderStyle& style, const Element*) const
{
    // Override border.
    style.resetBorder();
    const short borderWidth = 2 * style.effectiveZoom();
    style.setBorderLeftWidth(borderWidth);
    style.setBorderLeftStyle(BorderStyle::Inset);
    style.setBorderRightWidth(borderWidth);
    style.setBorderRightStyle(BorderStyle::Inset);
    style.setBorderBottomWidth(borderWidth);
    style.setBorderBottomStyle(BorderStyle::Inset);
    style.setBorderTopWidth(borderWidth);
    style.setBorderTopStyle(BorderStyle::Inset);

    // Override height.
    style.setHeight(Length(Auto));
    setSearchFieldSize(style);

    // Override padding size to match AppKit text positioning.
    const int padding = 1 * style.effectiveZoom();
    style.setPaddingLeft(Length(padding, Fixed));
    style.setPaddingRight(Length(padding, Fixed));
    style.setPaddingTop(Length(padding, Fixed));
    style.setPaddingBottom(Length(padding, Fixed));

    NSControlSize controlSize = controlSizeForFont(style);
    setFontFromControlSize(style, controlSize);

    style.setBoxShadow(nullptr);
}

bool RenderThemeMac::paintSearchFieldCancelButton(const RenderBox& box, const PaintInfo& paintInfo, const IntRect& r)
{
    auto adjustedCancelButtonRect = [this, &box] (const FloatRect& localBoundsForCancelButton) -> FloatRect
    {
        IntSize cancelButtonSizeBasedOnFontSize = sizeForSystemFont(box.style(), cancelButtonSizes());
        FloatSize diff = localBoundsForCancelButton.size() - FloatSize(cancelButtonSizeBasedOnFontSize);
        if (!diff.width() && !diff.height())
            return localBoundsForCancelButton;
        // Vertically centered and right aligned.
        FloatRect adjustedLocalBoundsForCancelButton = localBoundsForCancelButton;
        adjustedLocalBoundsForCancelButton.move(diff.width(), floorToDevicePixel(diff.height() / 2, box.document().deviceScaleFactor()));
        adjustedLocalBoundsForCancelButton.setSize(cancelButtonSizeBasedOnFontSize);
        return adjustedLocalBoundsForCancelButton;
    };

    if (!box.element())
        return false;
    Element* input = box.element()->shadowHost();
    if (!input)
        input = box.element();

    if (!is<RenderBox>(input->renderer()))
        return false;

    const RenderBox& inputBox = downcast<RenderBox>(*input->renderer());
    LocalCurrentGraphicsContext localContext(paintInfo.context());
    setSearchCellState(inputBox, r);

    NSSearchFieldCell* search = this->search();

    if (!input->isDisabledFormControl() && (is<HTMLTextFormControlElement>(*input) && !downcast<HTMLTextFormControlElement>(*input).isReadOnly()))
        updatePressedState([search cancelButtonCell], box);
    else if ([[search cancelButtonCell] isHighlighted])
        [[search cancelButtonCell] setHighlighted:NO];

    GraphicsContextStateSaver stateSaver(paintInfo.context());
    FloatRect localBounds = adjustedCancelButtonRect([search cancelButtonRectForBounds:NSRect(snappedIntRect(inputBox.contentBoxRect()))]);
    // Set the original horizontal position back (cancelButtonRectForBounds() moves it based on the system direction).
    localBounds.setX(inputBox.contentBoxRect().x() + box.x());
    FloatPoint paintingPos = convertToPaintingPosition(inputBox, box, localBounds.location(), r.location());

    FloatRect unzoomedRect(paintingPos, localBounds.size());
    auto zoomLevel = box.style().effectiveZoom();
    if (zoomLevel != 1.0f) {
        unzoomedRect.setSize(unzoomedRect.size() / zoomLevel);
        paintInfo.context().translate(unzoomedRect.location());
        paintInfo.context().scale(zoomLevel);
        paintInfo.context().translate(-unzoomedRect.location());
    }
    paintCellAndSetFocusedElementNeedsRepaintIfNecessary([search cancelButtonCell], box, paintInfo, unzoomedRect);
    [[search cancelButtonCell] setControlView:nil];
    return false;
}

const IntSize* RenderThemeMac::cancelButtonSizes() const
{
    static const IntSize sizes[4] = { IntSize(22, 22), IntSize(19, 19), IntSize(15, 15), IntSize(22, 22) };
    return sizes;
}

void RenderThemeMac::adjustSearchFieldCancelButtonStyle(RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, cancelButtonSizes());
    style.setWidth(Length(size.width(), Fixed));
    style.setHeight(Length(size.height(), Fixed));
    style.setBoxShadow(nullptr);
}

const int resultsArrowWidth = 5;
const IntSize* RenderThemeMac::resultsButtonSizes() const
{
    static const IntSize sizes[4] = { IntSize(19, 22), IntSize(17, 19), IntSize(17, 15), IntSize(19, 22) };
    return sizes;
}

const int emptyResultsOffset = 9;
void RenderThemeMac::adjustSearchFieldDecorationPartStyle(RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style.setWidth(Length(size.width() - emptyResultsOffset, Fixed));
    style.setHeight(Length(size.height(), Fixed));
    style.setBoxShadow(nullptr);
}

bool RenderThemeMac::paintSearchFieldDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&)
{
    return false;
}

void RenderThemeMac::adjustSearchFieldResultsDecorationPartStyle(RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style.setWidth(Length(size.width(), Fixed));
    style.setHeight(Length(size.height(), Fixed));
    style.setBoxShadow(nullptr);
}

bool RenderThemeMac::paintSearchFieldResultsDecorationPart(const RenderBox& box, const PaintInfo& paintInfo, const IntRect& r)
{
    if (!box.element())
        return false;
    Element* input = box.element()->shadowHost();
    if (!input)
        input = box.element();
    if (!is<RenderBox>(input->renderer()))
        return false;
    
    const RenderBox& inputBox = downcast<RenderBox>(*input->renderer());
    LocalCurrentGraphicsContext localContext(paintInfo.context());
    setSearchCellState(inputBox, r);

    NSSearchFieldCell* search = this->search();

    if ([search searchMenuTemplate] != nil)
        [search setSearchMenuTemplate:nil];

    FloatRect localBounds = [search searchButtonRectForBounds:NSRect(snappedIntRect(inputBox.borderBoxRect()))];
    FloatPoint paintingPos = convertToPaintingPosition(inputBox, box, localBounds.location(), r.location());
    localBounds.setLocation(paintingPos);

    [[search searchButtonCell] drawWithFrame:localBounds inView:documentViewFor(box)];
    [[search searchButtonCell] setControlView:nil];
    return false;
}

void RenderThemeMac::adjustSearchFieldResultsButtonStyle(RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style.setWidth(Length(size.width() + resultsArrowWidth, Fixed));
    style.setHeight(Length(size.height(), Fixed));
    style.setBoxShadow(nullptr);
}

bool RenderThemeMac::paintSearchFieldResultsButton(const RenderBox& box, const PaintInfo& paintInfo, const IntRect& r)
{
    auto adjustedResultButtonRect = [this, &box] (const FloatRect& localBounds) -> FloatRect
    {
        IntSize buttonSize = sizeForSystemFont(box.style(), resultsButtonSizes());
        buttonSize.expand(resultsArrowWidth, 0);
        FloatSize diff = localBounds.size() - FloatSize(buttonSize);
        if (!diff.isZero())
            return localBounds;
        // Vertically centered and left aligned.
        FloatRect adjustedLocalBounds = localBounds;
        adjustedLocalBounds.move(0, floorToDevicePixel(diff.height() / 2, box.document().deviceScaleFactor()));
        adjustedLocalBounds.setSize(buttonSize);
        return adjustedLocalBounds;
    };

    Element* input = box.element()->shadowHost();
    if (!input)
        input = box.element();
    if (!is<RenderBox>(input->renderer()))
        return false;
    
    const RenderBox& inputBox = downcast<RenderBox>(*input->renderer());
    LocalCurrentGraphicsContext localContext(paintInfo.context());
    setSearchCellState(inputBox, r);

    NSSearchFieldCell* search = this->search();

    if (![search searchMenuTemplate])
        [search setSearchMenuTemplate:searchMenuTemplate()];

    GraphicsContextStateSaver stateSaver(paintInfo.context());
    float zoomLevel = box.style().effectiveZoom();

    FloatRect localBounds = adjustedResultButtonRect([search searchButtonRectForBounds:NSRect(snappedIntRect(inputBox.contentBoxRect()))]);
    // Adjust position based on the content direction.
    float adjustedXPosition;
    if (box.style().direction() == TextDirection::RTL)
        adjustedXPosition = inputBox.contentBoxRect().maxX() - localBounds.size().width();
    else
        adjustedXPosition = inputBox.contentBoxRect().x();
    localBounds.setX(adjustedXPosition);
    FloatPoint paintingPos = convertToPaintingPosition(inputBox, box, localBounds.location(), r.location());
    
    FloatRect unzoomedRect(paintingPos, localBounds.size());
    if (zoomLevel != 1.0f) {
        unzoomedRect.setSize(unzoomedRect.size() / zoomLevel);
        paintInfo.context().translate(unzoomedRect.location());
        paintInfo.context().scale(zoomLevel);
        paintInfo.context().translate(-unzoomedRect.location());
    }

    paintCellAndSetFocusedElementNeedsRepaintIfNecessary([search searchButtonCell], box, paintInfo, unzoomedRect);
    [[search searchButtonCell] setControlView:nil];

    return false;
}

bool RenderThemeMac::paintSnapshottedPluginOverlay(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect&)
{
    if (paintInfo.phase != PaintPhase::BlockBackground)
        return true;

    if (!is<RenderBlock>(renderer))
        return true;

    const RenderBlock& renderBlock = downcast<RenderBlock>(renderer);

    LayoutUnit contentWidth = renderBlock.contentWidth();
    LayoutUnit contentHeight = renderBlock.contentHeight();
    if (!contentWidth || !contentHeight)
        return true;

    GraphicsContext& context = paintInfo.context();

    LayoutSize contentSize(contentWidth, contentHeight);
    LayoutPoint contentLocation = renderBlock.location();
    contentLocation.move(renderBlock.borderLeft() + renderBlock.paddingLeft(), renderBlock.borderTop() + renderBlock.paddingTop());

    LayoutRect rect(contentLocation, contentSize);
    IntRect alignedRect = snappedIntRect(rect);
    if (alignedRect.width() <= 0 || alignedRect.height() <= 0)
        return true;

    // We need to get the snapshot image from the plugin element, which should be available
    // from our node. Assuming this node is the plugin overlay element, we should get to the
    // plugin itself by asking for the shadow root parent, and then its parent.

    if (!is<HTMLElement>(*renderBlock.element()))
        return true;

    HTMLElement& plugInOverlay = downcast<HTMLElement>(*renderBlock.element());
    Element* parent = plugInOverlay.parentOrShadowHostElement();
    while (parent && !is<HTMLPlugInElement>(*parent))
        parent = parent->parentOrShadowHostElement();

    if (!parent)
        return true;

    HTMLPlugInElement& plugInElement = downcast<HTMLPlugInElement>(*parent);
    if (!is<HTMLPlugInImageElement>(plugInElement))
        return true;

    HTMLPlugInImageElement& plugInImageElement = downcast<HTMLPlugInImageElement>(plugInElement);

    Image* snapshot = plugInImageElement.snapshotImage();
    if (!snapshot)
        return true;

    RenderSnapshottedPlugIn& plugInRenderer = downcast<RenderSnapshottedPlugIn>(*plugInImageElement.renderer());
    FloatPoint snapshotAbsPos = plugInRenderer.localToAbsolute();
    snapshotAbsPos.move(plugInRenderer.borderLeft() + plugInRenderer.paddingLeft(), plugInRenderer.borderTop() + plugInRenderer.paddingTop());

    // We could draw the snapshot with that coordinates, but we need to make sure there
    // isn't a composited layer between us and the plugInRenderer.
    for (auto* renderBox = &downcast<RenderBox>(renderer); renderBox != &plugInRenderer; renderBox = renderBox->parentBox()) {
        if (renderBox->isComposited()) {
            snapshotAbsPos = -renderBox->location();
            break;
        }
    }

    LayoutSize pluginSize(plugInRenderer.contentWidth(), plugInRenderer.contentHeight());
    LayoutRect pluginRect(snapshotAbsPos, pluginSize);
    IntRect alignedPluginRect = snappedIntRect(pluginRect);

    if (alignedPluginRect.width() <= 0 || alignedPluginRect.height() <= 0)
        return true;

    context.drawImage(*snapshot, alignedPluginRect);
    return false;
}

#if ENABLE(DATALIST_ELEMENT)
IntSize RenderThemeMac::sliderTickSize() const
{
    return IntSize(1, 3);
}

int RenderThemeMac::sliderTickOffsetFromTrackCenter() const
{
    return -9;
}
#endif

const int sliderThumbWidth = 15;
const int sliderThumbHeight = 15;

void RenderThemeMac::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    float zoomLevel = style.effectiveZoom();
    if (style.appearance() == SliderThumbHorizontalPart || style.appearance() == SliderThumbVerticalPart) {
        style.setWidth(Length(static_cast<int>(sliderThumbWidth * zoomLevel), Fixed));
        style.setHeight(Length(static_cast<int>(sliderThumbHeight * zoomLevel), Fixed));
    }
}

NSPopUpButtonCell* RenderThemeMac::popupButton() const
{
    if (!m_popupButton) {
        m_popupButton = adoptNS([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
        [m_popupButton.get() setUsesItemFromMenu:NO];
        [m_popupButton.get() setFocusRingType:NSFocusRingTypeExterior];
        [m_popupButton setUserInterfaceLayoutDirection:NSUserInterfaceLayoutDirectionLeftToRight];
    }

    return m_popupButton.get();
}

NSSearchFieldCell* RenderThemeMac::search() const
{
    if (!m_search) {
        m_search = adoptNS([[NSSearchFieldCell alloc] initTextCell:@""]);
        [m_search.get() setBezelStyle:NSTextFieldRoundedBezel];
        [m_search.get() setBezeled:YES];
        [m_search.get() setEditable:YES];
        [m_search.get() setFocusRingType:NSFocusRingTypeExterior];
        [m_search.get() setCenteredLook:NO];
    }

    return m_search.get();
}

NSMenu* RenderThemeMac::searchMenuTemplate() const
{
    if (!m_searchMenuTemplate)
        m_searchMenuTemplate = adoptNS([[NSMenu alloc] initWithTitle:@""]);

    return m_searchMenuTemplate.get();
}

NSSliderCell* RenderThemeMac::sliderThumbHorizontal() const
{
    if (!m_sliderThumbHorizontal) {
        m_sliderThumbHorizontal = adoptNS([[NSSliderCell alloc] init]);
        [m_sliderThumbHorizontal.get() setSliderType:NSSliderTypeLinear];
        [m_sliderThumbHorizontal.get() setControlSize:NSControlSizeSmall];
        [m_sliderThumbHorizontal.get() setFocusRingType:NSFocusRingTypeExterior];
    }

    return m_sliderThumbHorizontal.get();
}

NSSliderCell* RenderThemeMac::sliderThumbVertical() const
{
    if (!m_sliderThumbVertical) {
        m_sliderThumbVertical = adoptNS([[NSSliderCell alloc] init]);
        [m_sliderThumbVertical.get() setSliderType:NSSliderTypeLinear];
        [m_sliderThumbVertical.get() setControlSize:NSControlSizeSmall];
        [m_sliderThumbVertical.get() setFocusRingType:NSFocusRingTypeExterior];
    }

    return m_sliderThumbVertical.get();
}

NSTextFieldCell* RenderThemeMac::textField() const
{
    if (!m_textField) {
        m_textField = adoptNS([[WebCoreTextFieldCell alloc] initTextCell:@""]);
        [m_textField.get() setBezeled:YES];
        [m_textField.get() setEditable:YES];
        [m_textField.get() setFocusRingType:NSFocusRingTypeExterior];
        // Post-Lion, WebCore can be in charge of paintinng the background thanks to
        // the workaround in place for <rdar://problem/11385461>, which is implemented
        // above as _coreUIDrawOptionsWithFrame.
        [m_textField.get() setDrawsBackground:NO];
    }

    return m_textField.get();
}

#if ENABLE(DATALIST_ELEMENT)
NSCell *RenderThemeMac::listButton() const
{
    if (!m_listButton)
        m_listButton = adoptNS([[WebListButtonCell alloc] init]);

    return m_listButton.get();
}
#endif

String RenderThemeMac::fileListNameForWidth(const FileList* fileList, const FontCascade& font, int width, bool multipleFilesAllowed) const
{
    if (width <= 0)
        return String();

    String strToTruncate;
    if (fileList->isEmpty())
        strToTruncate = fileListDefaultLabel(multipleFilesAllowed);
    else if (fileList->length() == 1)
        strToTruncate = [[NSFileManager defaultManager] displayNameAtPath:(fileList->item(0)->path())];
    else
        return StringTruncator::rightTruncate(multipleFileUploadText(fileList->length()), width, font);

    return StringTruncator::centerTruncate(strToTruncate, width, font);
}

#if ENABLE(SERVICE_CONTROLS)
NSServicesRolloverButtonCell* RenderThemeMac::servicesRolloverButtonCell() const
{
    if (!m_servicesRolloverButton) {
        m_servicesRolloverButton = [NSServicesRolloverButtonCell serviceRolloverButtonCellForStyle:NSSharingServicePickerStyleRollover];
        [m_servicesRolloverButton setBezelStyle:NSBezelStyleRoundedDisclosure];
        [m_servicesRolloverButton setButtonType:NSButtonTypePushOnPushOff];
        [m_servicesRolloverButton setImagePosition:NSImageOnly];
        [m_servicesRolloverButton setState:NO];
    }

    return m_servicesRolloverButton.get();
}

bool RenderThemeMac::paintImageControlsButton(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (paintInfo.phase != PaintPhase::BlockBackground)
        return true;

    NSServicesRolloverButtonCell *cell = servicesRolloverButtonCell();

    LocalCurrentGraphicsContext localContext(paintInfo.context());
    GraphicsContextStateSaver stateSaver(paintInfo.context());

    paintInfo.context().translate(rect.location());

    IntRect innerFrame(IntPoint(), rect.size());
    [cell drawWithFrame:innerFrame inView:documentViewFor(renderer)];
    [cell setControlView:nil];

    return true;
}

IntSize RenderThemeMac::imageControlsButtonSize(const RenderObject&) const
{
    return IntSize(servicesRolloverButtonCell().cellSize);
}

IntSize RenderThemeMac::imageControlsButtonPositionOffset() const
{
    // FIXME: Currently the offsets will always be the same no matter what image rect you try with.
    // This may not always be true in the future.
    static const int dummyDimension = 100;
    IntRect dummyImageRect(0, 0, dummyDimension, dummyDimension);
    NSRect bounds = [servicesRolloverButtonCell() rectForBounds:dummyImageRect preferredEdge:NSMinYEdge];

    return IntSize(dummyDimension - bounds.origin.x, bounds.origin.y);
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
const CGFloat attachmentIconSize = 48;
const CGFloat attachmentIconBackgroundPadding = 6;
const CGFloat attachmentIconBackgroundSize = attachmentIconSize + attachmentIconBackgroundPadding;
const CGFloat attachmentIconSelectionBorderThickness = 1;
const CGFloat attachmentIconBackgroundRadius = 3;
const CGFloat attachmentIconToTitleMargin = 2;

constexpr SimpleColor attachmentIconBackgroundColor = makeSimpleColor(0, 0, 0, 30);
constexpr SimpleColor attachmentIconBorderColor = makeSimpleColor(255, 255, 255, 125);

const CGFloat attachmentTitleFontSize = 12;
const CGFloat attachmentTitleBackgroundRadius = 3;
const CGFloat attachmentTitleBackgroundPadding = 3;
const CGFloat attachmentTitleMaximumWidth = 100 - (attachmentTitleBackgroundPadding * 2);
const CFIndex attachmentTitleMaximumLineCount = 2;

constexpr SimpleColor attachmentTitleInactiveBackgroundColor = makeSimpleColor(204, 204, 204, 255);
constexpr SimpleColor attachmentTitleInactiveTextColor = makeSimpleColor(100, 100, 100, 255);

const CGFloat attachmentSubtitleFontSize = 10;
const int attachmentSubtitleWidthIncrement = 10;
constexpr SimpleColor attachmentSubtitleTextColor = makeSimpleColor(82, 145, 214, 255);

const CGFloat attachmentProgressBarWidth = 30;
const CGFloat attachmentProgressBarHeight = 5;
const CGFloat attachmentProgressBarOffset = -9;
const CGFloat attachmentProgressBarBorderWidth = 1;
constexpr SimpleColor attachmentProgressBarBackgroundColor = makeSimpleColor(0, 0, 0, 89);
constexpr SimpleColor attachmentProgressBarFillColor = Color::white;
constexpr SimpleColor attachmentProgressBarBorderColor = makeSimpleColor(0, 0, 0, 128);

const CGFloat attachmentPlaceholderBorderRadius = 5;
constexpr SimpleColor attachmentPlaceholderBorderColor = makeSimpleColor(0, 0, 0, 56);
const CGFloat attachmentPlaceholderBorderWidth = 2;
const CGFloat attachmentPlaceholderBorderDashLength = 6;

const CGFloat attachmentMargin = 3;

enum class AttachmentLayoutStyle : uint8_t { NonSelected, Selected };

struct AttachmentLayout {
    explicit AttachmentLayout(const RenderAttachment&, AttachmentLayoutStyle);

    struct LabelLine {
        FloatRect backgroundRect;
        FloatPoint origin;
        RetainPtr<CTLineRef> line;
    };

    Vector<LabelLine> lines;

    FloatRect iconRect;
    FloatRect iconBackgroundRect;
    FloatRect attachmentRect;

    int baseline;
    AttachmentLayoutStyle style;

    RetainPtr<CTLineRef> subtitleLine;
    FloatRect subtitleTextRect;

private:
    void layOutTitle(const RenderAttachment&);
    void layOutSubtitle(const RenderAttachment&);

    void addTitleLine(CTLineRef, CGFloat& yOffset, Vector<CGPoint> origins, CFIndex lineIndex, const RenderAttachment&);
};

static Color titleTextColorForAttachment(const RenderAttachment& attachment, AttachmentLayoutStyle style)
{
    Color result = Color::black;
    
    if (style == AttachmentLayoutStyle::Selected) {
        if (attachment.frame().selection().isFocusedAndActive())
            result = colorFromNSColor([NSColor alternateSelectedControlTextColor]);
        else
            result = attachmentTitleInactiveTextColor;
    }

    return attachment.style().colorByApplyingColorFilter(result);
}

void AttachmentLayout::addTitleLine(CTLineRef line, CGFloat& yOffset, Vector<CGPoint> origins, CFIndex lineIndex, const RenderAttachment& attachment)
{
    CGRect lineBounds = CTLineGetBoundsWithOptions(line, 0);
    CGFloat trailingWhitespaceWidth = CTLineGetTrailingWhitespaceWidth(line);
    CGFloat lineWidthIgnoringTrailingWhitespace = lineBounds.size.width - trailingWhitespaceWidth;
    CGFloat lineHeight = CGCeiling(lineBounds.size.height);

    // Center the line relative to the icon.
    CGFloat xOffset = (attachmentIconBackgroundSize / 2) - (lineWidthIgnoringTrailingWhitespace / 2);

    if (lineIndex)
        yOffset += origins[lineIndex - 1].y - origins[lineIndex].y;

    LabelLine labelLine;
    labelLine.origin = FloatPoint(xOffset, yOffset + lineHeight - origins.last().y);
    labelLine.line = line;
    labelLine.backgroundRect = FloatRect(xOffset, yOffset, lineWidthIgnoringTrailingWhitespace, lineHeight);
    labelLine.backgroundRect.inflateX(attachmentTitleBackgroundPadding);
    labelLine.backgroundRect = encloseRectToDevicePixels(labelLine.backgroundRect, attachment.document().deviceScaleFactor());

    // If the text rects are close in size, the curved enclosing background won't
    // look right, so make them the same exact size.
    if (!lines.isEmpty()) {
        float previousBackgroundRectWidth = lines.last().backgroundRect.width();
        if (fabs(labelLine.backgroundRect.width() - previousBackgroundRectWidth) < attachmentTitleBackgroundRadius * 4) {
            float newBackgroundRectWidth = std::max(previousBackgroundRectWidth, labelLine.backgroundRect.width());
            labelLine.backgroundRect.inflateX((newBackgroundRectWidth - labelLine.backgroundRect.width()) / 2);
            lines.last().backgroundRect.inflateX((newBackgroundRectWidth - previousBackgroundRectWidth) / 2);
        }
    }

    lines.append(labelLine);
}

void AttachmentLayout::layOutTitle(const RenderAttachment& attachment)
{
    CFStringRef language = 0; // By not specifying a language we use the system language.
    RetainPtr<CTFontRef> font = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, attachmentTitleFontSize, language));
    baseline = CGRound(attachmentIconBackgroundSize + attachmentIconToTitleMargin + CTFontGetAscent(font.get()));

    String title = attachment.attachmentElement().attachmentTitleForDisplay();
    if (title.isEmpty())
        return;

    NSDictionary *textAttributes = @{
        (__bridge id)kCTFontAttributeName: (__bridge id)font.get(),
        (__bridge id)kCTForegroundColorAttributeName: (__bridge NSColor *)cachedCGColor(titleTextColorForAttachment(attachment, style))
    };
    RetainPtr<NSAttributedString> attributedTitle = adoptNS([[NSAttributedString alloc] initWithString:title attributes:textAttributes]);
    RetainPtr<CTFramesetterRef> titleFramesetter = adoptCF(CTFramesetterCreateWithAttributedString((CFAttributedStringRef)attributedTitle.get()));

    CFRange fitRange;
    CGSize titleTextSize = CTFramesetterSuggestFrameSizeWithConstraints(titleFramesetter.get(), CFRangeMake(0, 0), nullptr, CGSizeMake(attachmentTitleMaximumWidth, CGFLOAT_MAX), &fitRange);

    RetainPtr<CGPathRef> titlePath = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, titleTextSize.width, titleTextSize.height), nullptr));
    RetainPtr<CTFrameRef> titleFrame = adoptCF(CTFramesetterCreateFrame(titleFramesetter.get(), fitRange, titlePath.get(), nullptr));

    CFArrayRef ctLines = CTFrameGetLines(titleFrame.get());
    CFIndex lineCount = CFArrayGetCount(ctLines);
    if (!lineCount)
        return;

    Vector<CGPoint> origins(lineCount);
    CTFrameGetLineOrigins(titleFrame.get(), CFRangeMake(0, 0), origins.data());

    // Lay out and record the first (attachmentTitleMaximumLineCount - 1) lines.
    CFIndex lineIndex = 0;
    CGFloat yOffset = attachmentIconBackgroundSize + attachmentIconToTitleMargin;
    for (; lineIndex < std::min(attachmentTitleMaximumLineCount - 1, lineCount); ++lineIndex) {
        CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(ctLines, lineIndex);
        addTitleLine(line, yOffset, origins, lineIndex, attachment);
    }

    if (lineIndex == lineCount)
        return;

    // We had text that didn't fit in the first (attachmentTitleMaximumLineCount - 1) lines.
    // Combine it into one last line, and center-truncate it.
    CTLineRef firstRemainingLine = (CTLineRef)CFArrayGetValueAtIndex(ctLines, lineIndex);
    CFIndex remainingRangeStart = CTLineGetStringRange(firstRemainingLine).location;
    CFRange remainingRange = CFRangeMake(remainingRangeStart, [attributedTitle length] - remainingRangeStart);
    RetainPtr<CGPathRef> remainingPath = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, CGFLOAT_MAX, CGFLOAT_MAX), nullptr));
    RetainPtr<CTFrameRef> remainingFrame = adoptCF(CTFramesetterCreateFrame(titleFramesetter.get(), remainingRange, remainingPath.get(), nullptr));
    RetainPtr<NSAttributedString> ellipsisString = adoptNS([[NSAttributedString alloc] initWithString:@"\u2026" attributes:textAttributes]);
    RetainPtr<CTLineRef> ellipsisLine = adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)ellipsisString.get()));
    CTLineRef remainingLine = (CTLineRef)CFArrayGetValueAtIndex(CTFrameGetLines(remainingFrame.get()), 0);
    RetainPtr<CTLineRef> truncatedLine = adoptCF(CTLineCreateTruncatedLine(remainingLine, attachmentTitleMaximumWidth, kCTLineTruncationMiddle, ellipsisLine.get()));

    if (!truncatedLine)
        truncatedLine = remainingLine;

    addTitleLine(truncatedLine.get(), yOffset, origins, lineIndex, attachment);
}

void AttachmentLayout::layOutSubtitle(const RenderAttachment& attachment)
{
    auto& subtitleText = attachment.attachmentElement().attributeWithoutSynchronization(subtitleAttr);
    if (subtitleText.isEmpty())
        return;

    Color subtitleColor = attachment.style().colorByApplyingColorFilter(attachmentSubtitleTextColor);
    CFStringRef language = 0; // By not specifying a language we use the system language.
    RetainPtr<CTFontRef> font = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, attachmentSubtitleFontSize, language));
    NSDictionary *textAttributes = @{
        (__bridge id)kCTFontAttributeName: (__bridge id)font.get(),
        (__bridge id)kCTForegroundColorAttributeName: (__bridge NSColor *)cachedCGColor(subtitleColor)
    };
    RetainPtr<NSAttributedString> attributedSubtitleText = adoptNS([[NSAttributedString alloc] initWithString:subtitleText attributes:textAttributes]);
    subtitleLine = adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)attributedSubtitleText.get()));

    CGRect lineBounds = CTLineGetBoundsWithOptions(subtitleLine.get(), 0);

    // Center the line relative to the icon.
    CGFloat xOffset = (attachmentIconBackgroundSize / 2) - (lineBounds.size.width / 2);
    CGFloat yOffset = 0;

    if (!lines.isEmpty())
        yOffset = lines.last().backgroundRect.maxY();
    else
        yOffset = attachmentIconBackgroundSize + attachmentIconToTitleMargin;

    LabelLine labelLine;
    subtitleTextRect = FloatRect(xOffset, yOffset, lineBounds.size.width, lineBounds.size.height);
}

AttachmentLayout::AttachmentLayout(const RenderAttachment& attachment, AttachmentLayoutStyle layoutStyle)
    : style(layoutStyle)
{
    layOutTitle(attachment);
    layOutSubtitle(attachment);

    iconBackgroundRect = FloatRect(0, 0, attachmentIconBackgroundSize, attachmentIconBackgroundSize);

    iconRect = iconBackgroundRect;
    iconRect.setSize(FloatSize(attachmentIconSize, attachmentIconSize));
    iconRect.move(attachmentIconBackgroundPadding / 2, attachmentIconBackgroundPadding / 2);

    attachmentRect = iconBackgroundRect;
    for (const auto& line : lines)
        attachmentRect.unite(line.backgroundRect);
    if (!subtitleTextRect.isEmpty()) {
        FloatRect roundedSubtitleTextRect = subtitleTextRect;
        roundedSubtitleTextRect.inflateX(attachmentSubtitleWidthIncrement - clampToInteger(ceilf(subtitleTextRect.width())) % attachmentSubtitleWidthIncrement);
        attachmentRect.unite(roundedSubtitleTextRect);
    }
    attachmentRect.inflate(attachmentMargin);
    attachmentRect = encloseRectToDevicePixels(attachmentRect, attachment.document().deviceScaleFactor());
}

LayoutSize RenderThemeMac::attachmentIntrinsicSize(const RenderAttachment& attachment) const
{
    AttachmentLayout layout(attachment, AttachmentLayoutStyle::NonSelected);
    return LayoutSize(layout.attachmentRect.size());
}

int RenderThemeMac::attachmentBaseline(const RenderAttachment& attachment) const
{
    AttachmentLayout layout(attachment, AttachmentLayoutStyle::NonSelected);
    return layout.baseline;
}

static void paintAttachmentIconBackground(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout)
{
    if (layout.style == AttachmentLayoutStyle::NonSelected)
        return;

    // FIXME: Finder has a discontinuous behavior here when you have a background color other than white,
    // where it switches into 'bordered mode' and the border pops in on top of the background.
    bool paintBorder = true;

    FloatRect backgroundRect = layout.iconBackgroundRect;
    if (paintBorder)
        backgroundRect.inflate(-attachmentIconSelectionBorderThickness);

    Color backgroundColor = attachment.style().colorByApplyingColorFilter(attachmentIconBackgroundColor);
    context.fillRoundedRect(FloatRoundedRect(backgroundRect, FloatRoundedRect::Radii(attachmentIconBackgroundRadius)), backgroundColor);

    if (paintBorder) {
        FloatRect borderRect = layout.iconBackgroundRect;
        borderRect.inflate(-attachmentIconSelectionBorderThickness / 2);

        FloatSize iconBackgroundRadiusSize(attachmentIconBackgroundRadius, attachmentIconBackgroundRadius);
        Path borderPath;
        borderPath.addRoundedRect(borderRect, iconBackgroundRadiusSize);

        Color borderColor = attachment.style().colorByApplyingColorFilter(attachmentIconBorderColor);
        context.setStrokeColor(borderColor);
        context.setStrokeThickness(attachmentIconSelectionBorderThickness);
        context.strokePath(borderPath);
    }
}

static RefPtr<Icon> iconForAttachment(const RenderAttachment& attachment)
{
    String attachmentType = attachment.attachmentElement().attachmentType();
    
    if (!attachmentType.isEmpty()) {
        if (equalIgnoringASCIICase(attachmentType, "multipart/x-folder") || equalIgnoringASCIICase(attachmentType, "application/vnd.apple.folder")) {
            if (auto icon = Icon::createIconForUTI(kUTTypeFolder))
                return icon;
        } else {
            String UTI;
            if (isDeclaredUTI(attachmentType))
                UTI = attachmentType;
            else
                UTI = UTIFromMIMEType(attachmentType);

            if (auto icon = Icon::createIconForUTI(UTI))
                return icon;
        }
    }

    if (File* file = attachment.attachmentElement().file()) {
        if (auto icon = Icon::createIconForFiles({ file->path() }))
            return icon;
    }

    NSString *fileExtension = [static_cast<NSString *>(attachment.attachmentElement().attachmentTitle()) pathExtension];
    if (fileExtension.length) {
        if (auto icon = Icon::createIconForFileExtension(fileExtension))
            return icon;
    }

    return Icon::createIconForUTI("public.data");
}

static void paintAttachmentIcon(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout)
{
    if (auto thumbnailIcon = attachment.attachmentElement().thumbnail()) {
        context.drawImage(*thumbnailIcon, layout.iconRect);
        return;
    }
    auto icon = iconForAttachment(attachment);
    if (!icon)
        return;
    icon->paint(context, layout.iconRect);
}

static std::pair<RefPtr<Image>, float> createAttachmentPlaceholderImage(float deviceScaleFactor, const AttachmentLayout& layout)
{
#if HAVE(ALTERNATE_ICONS)
    auto image = [NSImage _imageWithSystemSymbolName:@"arrow.down.circle"];
    auto imageSize = FloatSize([image size]);
    auto imageSizeScales = deviceScaleFactor * layout.iconRect.size() / imageSize;
    imageSize.scale(std::min(imageSizeScales.width(), imageSizeScales.height()));
    auto imageRect = NSMakeRect(0, 0, imageSize.width(), imageSize.height());
    auto cgImage = [image CGImageForProposedRect:&imageRect context:nil hints:@{
        NSImageHintSymbolFont : [NSFont systemFontOfSize:32],
        NSImageHintSymbolScale : @(NSImageSymbolScaleMedium)
    }];
    return { BitmapImage::create(cgImage.get()), deviceScaleFactor };
#else
    UNUSED_PARAM(layout);
    if (deviceScaleFactor >= 2)
        return { Image::loadPlatformResource("AttachmentPlaceholder@2x"), 2 };

    return { Image::loadPlatformResource("AttachmentPlaceholder"), 1 };
#endif
}

static void paintAttachmentIconPlaceholder(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout)
{
    auto [placeholderImage, imageScale] = createAttachmentPlaceholderImage(attachment.document().deviceScaleFactor(), layout);

    // Center the placeholder image where the icon would usually be.
    FloatRect placeholderRect(0, 0, placeholderImage->width() / imageScale, placeholderImage->height() / imageScale);
    placeholderRect.setX(layout.iconRect.x() + (layout.iconRect.width() - placeholderRect.width()) / 2);
    placeholderRect.setY(layout.iconRect.y() + (layout.iconRect.height() - placeholderRect.height()) / 2);

    context.drawImage(*placeholderImage, placeholderRect);
}

static void paintAttachmentTitleBackground(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout)
{
    if (layout.style == AttachmentLayoutStyle::NonSelected)
        return;

    if (layout.lines.isEmpty())
        return;

    Vector<FloatRect> backgroundRects;

    for (size_t i = 0; i < layout.lines.size(); ++i)
        backgroundRects.append(layout.lines[i].backgroundRect);

    Color backgroundColor;
    if (attachment.frame().selection().isFocusedAndActive()) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        backgroundColor = colorFromNSColor([NSColor alternateSelectedControlColor]);
        ALLOW_DEPRECATED_DECLARATIONS_END
    } else
        backgroundColor = attachmentTitleInactiveBackgroundColor;

    backgroundColor = attachment.style().colorByApplyingColorFilter(backgroundColor);
    context.setFillColor(backgroundColor);

    Path backgroundPath = PathUtilities::pathWithShrinkWrappedRects(backgroundRects, attachmentTitleBackgroundRadius);
    context.fillPath(backgroundPath);
}

static void paintAttachmentTitle(const RenderAttachment&, GraphicsContext& context, AttachmentLayout& layout)
{
    for (const auto& line : layout.lines) {
        GraphicsContextStateSaver saver(context);

        context.translate(toFloatSize(line.origin));
        context.scale(FloatSize(1, -1));

        CGContextSetTextPosition(context.platformContext(), 0, 0);
        CTLineDraw(line.line.get(), context.platformContext());
    }
}

static void paintAttachmentSubtitle(const RenderAttachment&, GraphicsContext& context, AttachmentLayout& layout)
{
    GraphicsContextStateSaver saver(context);

    context.translate(toFloatSize(layout.subtitleTextRect.minXMaxYCorner()));
    context.scale(FloatSize(1, -1));

    CGContextSetTextPosition(context.platformContext(), 0, 0);
    CTLineDraw(layout.subtitleLine.get(), context.platformContext());
}

static void paintAttachmentProgress(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout, float progress)
{
    GraphicsContextStateSaver saver(context);

    FloatRect progressBounds((attachmentIconBackgroundSize - attachmentProgressBarWidth) / 2, layout.iconBackgroundRect.maxY() + attachmentProgressBarOffset - attachmentProgressBarHeight, attachmentProgressBarWidth, attachmentProgressBarHeight);

    FloatRect borderRect = progressBounds;
    borderRect.inflate(-0.5);
    FloatRect backgroundRect = borderRect;
    backgroundRect.inflate(-attachmentProgressBarBorderWidth / 2);

    FloatRoundedRect backgroundRoundedRect(backgroundRect, FloatRoundedRect::Radii(backgroundRect.height() / 2));
    context.fillRoundedRect(backgroundRoundedRect, attachmentProgressBarBackgroundColor);

    {
        GraphicsContextStateSaver clipSaver(context);
        context.clipRoundedRect(backgroundRoundedRect);

        FloatRect progressRect = progressBounds;
        progressRect.setWidth(progressRect.width() * progress);
        progressRect = encloseRectToDevicePixels(progressRect, attachment.document().deviceScaleFactor());

        context.fillRect(progressRect, attachmentProgressBarFillColor);
    }

    Path borderPath;
    float borderRadius = borderRect.height() / 2;
    borderPath.addRoundedRect(borderRect, FloatSize(borderRadius, borderRadius));
    context.setStrokeColor(attachmentProgressBarBorderColor);
    context.setStrokeThickness(attachmentProgressBarBorderWidth);
    context.strokePath(borderPath);
}

static void paintAttachmentPlaceholderBorder(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout)
{
    Path borderPath;
    borderPath.addRoundedRect(layout.attachmentRect, FloatSize(attachmentPlaceholderBorderRadius, attachmentPlaceholderBorderRadius));

    Color placeholderBorderColor = attachment.style().colorByApplyingColorFilter(attachmentPlaceholderBorderColor);
    context.setStrokeColor(placeholderBorderColor);
    context.setStrokeThickness(attachmentPlaceholderBorderWidth);
    context.setStrokeStyle(DashedStroke);
    context.setLineDash({attachmentPlaceholderBorderDashLength}, 0);
    context.strokePath(borderPath);
}

bool RenderThemeMac::paintAttachment(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& paintRect)
{
    if (!is<RenderAttachment>(renderer))
        return false;

    const RenderAttachment& attachment = downcast<RenderAttachment>(renderer);

    auto layoutStyle = AttachmentLayoutStyle::NonSelected;
    if (attachment.selectionState() != RenderObject::HighlightState::None && paintInfo.phase != PaintPhase::Selection)
        layoutStyle = AttachmentLayoutStyle::Selected;

    AttachmentLayout layout(attachment, layoutStyle);

    auto& progressString = attachment.attachmentElement().attributeWithoutSynchronization(progressAttr);
    bool validProgress = false;
    float progress = 0;
    if (!progressString.isEmpty())
        progress = progressString.toFloat(&validProgress);

    GraphicsContext& context = paintInfo.context();
    LocalCurrentGraphicsContext localContext(context);
    GraphicsContextStateSaver saver(context);

    context.translate(toFloatSize(paintRect.location()));
    context.translate(floorSizeToDevicePixels({ LayoutUnit((paintRect.width() - attachmentIconBackgroundSize) / 2), 0 }, renderer.document().deviceScaleFactor()));

    bool usePlaceholder = validProgress && !progress;

    paintAttachmentIconBackground(attachment, context, layout);
    if (usePlaceholder)
        paintAttachmentIconPlaceholder(attachment, context, layout);
    else
        paintAttachmentIcon(attachment, context, layout);

    paintAttachmentTitleBackground(attachment, context, layout);
    paintAttachmentTitle(attachment, context, layout);
    paintAttachmentSubtitle(attachment, context, layout);

    if (validProgress && progress)
        paintAttachmentProgress(attachment, context, layout, progress);

    if (usePlaceholder)
        paintAttachmentPlaceholderBorder(attachment, context, layout);

    return true;
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

} // namespace WebCore

#endif // PLATFORM(MAC)
