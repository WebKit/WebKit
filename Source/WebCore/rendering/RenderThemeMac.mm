/*
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
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
#import "ElementInlines.h"
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
#import "HTMLMeterElement.h"
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
#import "RenderMeter.h"
#import "RenderProgress.h"
#import "RenderSlider.h"
#import "RenderView.h"
#import "SharedBuffer.h"
#import "SliderThumbElement.h"
#import "StringTruncator.h"
#import "ThemeMac.h"
#import "TimeRanges.h"
#import "UTIUtilities.h"
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
#import <pal/spi/mac/NSServicesRolloverButtonCellSPI.h>
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#import <wtf/MathExtras.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/StringBuilder.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

// FIXME: This should go into an SPI.h file in the spi directory.
@interface NSTextFieldCell ()
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus;
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus maskOnly:(BOOL)maskOnly;
@end

// FIXME: This should go into an SPI.h file in the spi directory.
@interface NSSearchFieldCell ()
@property (getter=isCenteredLook) BOOL centeredLook;
@end

constexpr Seconds progressAnimationRepeatInterval = 33_ms; // 30 fps

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
    auto coreUIDrawOptions = adoptCF(CFDictionaryCreateMutableCopy(NULL, 0, defaultOptions));
    CFDictionarySetValue(coreUIDrawOptions.get(), CFSTR("borders only"), kCFBooleanTrue);
    return coreUIDrawOptions.autorelease();
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

bool RenderThemeMac::canPaint(const PaintInfo& paintInfo, const Settings&, ControlPartType type) const
{
    switch (type) {
#if ENABLE(ATTACHMENT_ELEMENT)
    case ControlPartType::Attachment:
    case ControlPartType::BorderlessAttachment:
        return true;
#endif
#if ENABLE(APPLE_PAY)
    case ControlPartType::ApplePayButton:
        return true;
#endif
    default:
        break;
    }
    return paintInfo.context().hasPlatformContext();
}

RenderThemeMac::RenderThemeMac()
    : m_notificationObserver(adoptNS([[WebCoreRenderThemeNotificationObserver alloc] init]))
{
}

bool RenderThemeMac::canCreateControlPartForRenderer(const RenderObject& renderer) const
{
    ControlPartType type = renderer.style().effectiveAppearance();
    return type == ControlPartType::Meter;
}

bool RenderThemeMac::useFormSemanticContext() const
{
    return ThemeMac::useFormSemanticContext();
}

bool RenderThemeMac::supportsLargeFormControls() const
{
#if HAVE(LARGE_CONTROL_SIZE)
    return ThemeMac::supportsLargeFormControls();
#else
    return false;
#endif
}

NSView *RenderThemeMac::documentViewFor(const RenderObject& o) const
{
    LocalDefaultSystemAppearance localAppearance(o.useDarkAppearance());
    ControlStates states(extractControlStatesForRenderer(o));
    return ThemeMac::ensuredView(&o.view().frameView(), states);
}

Color RenderThemeMac::platformActiveSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor selectedTextBackgroundColor]);
}

Color RenderThemeMac::platformInactiveSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor unemphasizedSelectedTextBackgroundColor]);
#else
    UNUSED_PARAM(options);
    return colorFromCocoaColor([NSColor unemphasizedSelectedContentBackgroundColor]);
#endif
}

Color RenderThemeMac::transformSelectionBackgroundColor(const Color& color, OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    if (localAppearance.usingDarkAppearance()) {
        // Use an alpha value that is similar to results from blendWithWhite() on light colors.
        static const float darkAppearanceAlpha = 0.8;
        return !color.isOpaque() ? color : color.colorWithAlpha(darkAppearanceAlpha);
    }

    return RenderTheme::transformSelectionBackgroundColor(color, options);
}

bool RenderThemeMac::supportsSelectionForegroundColors(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return localAppearance.usingDarkAppearance();
}

Color RenderThemeMac::platformActiveSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    if (localAppearance.usingDarkAppearance())
        return colorFromCocoaColor([NSColor selectedTextColor]);
    return { };
}

Color RenderThemeMac::platformInactiveSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    if (localAppearance.usingDarkAppearance())
        return colorFromCocoaColor([NSColor unemphasizedSelectedTextColor]);
    return { };
#else
    UNUSED_PARAM(options);
    return { };
#endif
}

Color RenderThemeMac::platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor selectedContentBackgroundColor]);
#else
    UNUSED_PARAM(options);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return colorFromCocoaColor([NSColor alternateSelectedControlColor]);
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif
}

Color RenderThemeMac::platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
#else
    UNUSED_PARAM(options);
#endif
    return colorFromCocoaColor([NSColor unemphasizedSelectedContentBackgroundColor]);

}

Color RenderThemeMac::platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor alternateSelectedControlTextColor]);
}

Color RenderThemeMac::platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor unemphasizedSelectedTextColor]);
#else
    UNUSED_PARAM(options);
    return colorFromCocoaColor([NSColor selectedControlTextColor]);
#endif
}

Color RenderThemeMac::platformFocusRingColor(OptionSet<StyleColorOptions> options) const
{
    if (usesTestModeFocusRingColor())
        return oldAquaFocusRingColor();
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    // The color is expected to be opaque, since CoreGraphics will apply opacity when drawing (because opacity is normally animated).
    return colorFromCocoaColor([NSColor keyboardFocusIndicatorColor]).opaqueColor();
}

Color RenderThemeMac::platformTextSearchHighlightColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor findHighlightColor]);
}

Color RenderThemeMac::platformAnnotationHighlightColor(OptionSet<StyleColorOptions>) const
{
    // FIXME: expose the real value from AppKit.
    return SRGBA<uint8_t> { 255, 238, 190 };
}

Color RenderThemeMac::platformDefaultButtonTextColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor alternateSelectedControlTextColor]);
}

static Color activeButtonTextColor()
{
    // FIXME: <rdar://problem/77572622> There is no single corresponding NSColor for ActiveButtonText.
    // Instead, the NSColor used is dependent on NSButtonCell's interiorBackgroundStyle. Consequently,
    // we need to create an NSButtonCell just to determine the correct color.

    auto cell = adoptNS([[NSButtonCell alloc] init]);
    [cell setBezelStyle:NSBezelStyleRounded];
    [cell setHighlighted:YES];

    NSColor *activeButtonTextColor;
    if ([cell interiorBackgroundStyle] == NSBackgroundStyleEmphasized)
        activeButtonTextColor = [NSColor alternateSelectedControlTextColor];
    else
        activeButtonTextColor = [NSColor controlTextColor];

    return semanticColorFromNSColor(activeButtonTextColor);
}

static SRGBA<uint8_t> menuBackgroundColor()
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

    return makeFromComponentsClamping<SRGBA<uint8_t>>(pixel[0], pixel[1], pixel[2], pixel[3]);
}

Color RenderThemeMac::systemColor(CSSValueID cssValueID, OptionSet<StyleColorOptions> options) const
{
    const bool useSystemAppearance = options.contains(StyleColorOptions::UseSystemAppearance);
    const bool useDarkAppearance = options.contains(StyleColorOptions::UseDarkAppearance);
    const bool forVisitedLink = options.contains(StyleColorOptions::ForVisitedLink);

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
                return @selector(unemphasizedSelectedContentBackgroundColor);
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
                return @selector(unemphasizedSelectedContentBackgroundColor);
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
            return activeButtonTextColor();

        case CSSValueButtonface:
        case CSSValueThreedface:
            // We selected this value instead of [NSColor controlColor] to avoid website incompatibilities.
            // We may want to consider changing to [NSColor controlColor] some day.
            return Color::lightGray;

        case CSSValueInfobackground:
            // No corresponding NSColor for this so we use a hard coded value.
            return SRGBA<uint8_t> { 251, 252, 197 };

        case CSSValueMenu:
            return menuBackgroundColor();

        case CSSValueWebkitFocusRingColor:
        case CSSValueActiveborder:
            // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
            if (localAppearance.usingDarkAppearance())
                return { SRGBA<uint8_t> { 26, 169, 255 }, Color::Flags::Semantic };
            return { SRGBA<uint8_t> { 0, 103, 244 }, Color::Flags::Semantic };

        case CSSValueAppleSystemControlAccent:
            // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
            // Same color in light and dark appearances.
            return { SRGBA<uint8_t> { 0, 122, 255 }, Color::Flags::Semantic };

        case CSSValueAppleSystemSelectedContentBackground:
            // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
            if (localAppearance.usingDarkAppearance())
                return { SRGBA<uint8_t> { 0, 88, 208 }, Color::Flags::Semantic };
            return { SRGBA<uint8_t> { 0, 99, 225 }, Color::Flags::Semantic };

        case CSSValueHighlight:
        case CSSValueAppleSystemSelectedTextBackground:
            // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
            if (localAppearance.usingDarkAppearance())
                return { SRGBA<uint8_t> { 63, 99, 139, 204 }, Color::Flags::Semantic };
            return { SRGBA<uint8_t> { 128, 188, 254, 153 }, Color::Flags::Semantic };

#if !HAVE(OS_DARK_MODE_SUPPORT)
        case CSSValueAppleSystemContainerBorder:
            return SRGBA<uint8_t> { 197, 197, 197 };
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
    auto appearance = style.effectiveAppearance();
    if (appearance == ControlPartType::TextField || appearance == ControlPartType::TextArea || appearance == ControlPartType::SearchField || appearance == ControlPartType::Listbox)
        return style.border() != userAgentStyle.border();

    // FIXME: This is horrible, but there is not much else that can be done.  Menu lists cannot draw properly when
    // scaled.  They can't really draw properly when transformed either.  We can't detect the transform case at style
    // adjustment time so that will just have to stay broken.  We can however detect that we're zooming.  If zooming
    // is in effect we treat it like the control is styled.
    if (appearance == ControlPartType::Menulist && style.effectiveZoom() != 1.0f)
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
    auto type = renderer.style().effectiveAppearance();

#if USE(NEW_THEME)
    switch (type) {
    case ControlPartType::Checkbox:
    case ControlPartType::Radio:
    case ControlPartType::PushButton:
    case ControlPartType::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
#endif
    case ControlPartType::DefaultButton:
    case ControlPartType::Button:
    case ControlPartType::InnerSpinButton:
            return RenderTheme::adjustRepaintRect(renderer, rect);
    default:
            break;
    }
#endif

    float zoomLevel = renderer.style().effectiveZoom();

    if (type == ControlPartType::Menulist) {
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

void RenderThemeMac::updateFocusedState(NSCell *cell, const RenderObject* o)
{
    bool oldFocused = [cell showsFirstResponder];
    bool focused = o && isFocused(*o) && o->style().outlineStyleIsAuto() == OutlineIsAuto::On;
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
    if (o.style().effectiveAppearance() == ControlPartType::Checkbox)
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
        style.setWidth(Length(size.width(), LengthType::Fixed));
    if (style.height().isAuto() && size.height() > 0)
        style.setHeight(Length(size.height(), LengthType::Fixed));
}

void RenderThemeMac::setFontFromControlSize(RenderStyle& style, NSControlSize controlSize) const
{
    FontCascadeDescription fontDescription;
    fontDescription.setIsAbsoluteSize(true);

    NSFont* font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSize]];
    fontDescription.setOneFamily("-apple-system"_s);
    fontDescription.setComputedSize([font pointSize] * style.effectiveZoom());
    fontDescription.setSpecifiedSize([font pointSize] * style.effectiveZoom());

    // Reset line height
    style.setLineHeight(RenderStyle::initialLineHeight());

    if (style.setFontDescription(WTFMove(fontDescription)))
        style.fontCascade().update(nullptr);
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

#if HAVE(LARGE_CONTROL_SIZE)
    LocalDefaultSystemAppearance localAppearance(o.useDarkAppearance(), o.style().effectiveAccentColor());

    const FloatSize comboBoxSize { 40, 19 };
    const FloatSize comboBoxButtonSize { 16, 16 };
    const FloatPoint comboBoxButtonInset { 5, 1 };
    constexpr auto comboBoxButtonCornerRadii = 4;

    const FloatSize desiredComboBoxButtonSize { 12, 12 };
    constexpr auto desiredComboBoxInset = 2;

    float deviceScaleFactor = o.document().deviceScaleFactor();

    auto comboBoxImageBuffer = context.createImageBuffer(comboBoxSize, deviceScaleFactor);
    if (!comboBoxImageBuffer)
        return;

    ContextContainer cgContextContainer(comboBoxImageBuffer->context());
    CGContextRef cgContext = cgContextContainer.context();

    NSString *coreUIState;
    if (input.isPresentingAttachedView())
        coreUIState = (__bridge NSString *)kCUIStatePressed;
    else if (auto* buttonElement = input.dataListButtonElement())
        coreUIState = (__bridge NSString *)(buttonElement->active() ? kCUIStatePressed : kCUIStateActive);
    else
        coreUIState = (__bridge NSString *)kCUIStateActive;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[NSAppearance currentAppearance] _drawInRect:NSMakeRect(0, 0, comboBoxSize.width(), comboBoxSize.height()) context:cgContext options:@{
    ALLOW_DEPRECATED_DECLARATIONS_END
        (__bridge NSString *)kCUIWidgetKey : (__bridge NSString *)kCUIWidgetButtonComboBox,
        (__bridge NSString *)kCUISizeKey : (__bridge NSString *)kCUISizeRegular,
        (__bridge NSString *)kCUIStateKey : coreUIState,
        (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey : (__bridge NSString *)kCUIUserInterfaceLayoutDirectionLeftToRight,
    }];

    auto comboBoxButtonImageBuffer = context.createImageBuffer(desiredComboBoxButtonSize, deviceScaleFactor);
    if (!comboBoxButtonImageBuffer)
        return;

    auto& comboBoxButtonContext = comboBoxButtonImageBuffer->context();

    comboBoxButtonContext.scale(desiredComboBoxButtonSize.width() / comboBoxButtonSize.width());
    comboBoxButtonContext.clipRoundedRect(FloatRoundedRect(FloatRect(FloatPoint::zero(), comboBoxButtonSize), FloatRoundedRect::Radii(comboBoxButtonCornerRadii)));
    comboBoxButtonContext.translate(comboBoxButtonInset.scaled(-1));
    comboBoxButtonContext.drawConsumingImageBuffer(WTFMove(comboBoxImageBuffer), FloatPoint::zero(), ImagePaintingOptions { ImageOrientation::OriginBottomRight });

    FloatPoint listButtonLocation;
    float listButtonY = r.center().y() - desiredComboBoxButtonSize.height() / 2;
    if (o.style().isLeftToRightDirection())
        listButtonLocation = { r.maxX() - desiredComboBoxButtonSize.width() - desiredComboBoxInset, listButtonY };
    else
        listButtonLocation = { r.x() + desiredComboBoxInset, listButtonY };

    GraphicsContextStateSaver stateSaver(context);
    context.drawConsumingImageBuffer(WTFMove(comboBoxButtonImageBuffer), listButtonLocation);
#else
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
#endif // HAVE(LARGE_CONTROL_SIZE)
}

void RenderThemeMac::adjustListButtonStyle(RenderStyle& style, const Element*) const
{
    // Add a margin to place the button at end of the input field.
    if (style.isLeftToRightDirection())
        style.setMarginRight(Length(-4, LengthType::Fixed));
    else
        style.setMarginLeft(Length(-4, LengthType::Fixed));
}

#endif

#if ENABLE(SERVICE_CONTROLS)
void RenderThemeMac::adjustImageControlsButtonStyle(RenderStyle& style, const Element*) const
{
    style.setHeight(Length(imageControlsButtonSize().height(), LengthType::Fixed));
    style.setWidth(Length(imageControlsButtonSize().width(), LengthType::Fixed));
}
#endif

bool RenderThemeMac::shouldPaintCustomTextField(const RenderObject& renderer) const
{
    // <rdar://problem/88948646> Prevent AppKit from painting text fields in the light appearance
    // with increased contrast, as the border is not painted, rendering the control invisible.
#if HAVE(LARGE_CONTROL_SIZE)
    return Theme::singleton().userPrefersContrast() && !renderer.useDarkAppearance();
#else
    UNUSED_PARAM(renderer);
    return false;
#endif
}

bool RenderThemeMac::paintTextField(const RenderObject& o, const PaintInfo& paintInfo, const FloatRect& r)
{
    FloatRect paintRect(r);
    auto& context = paintInfo.context();

    LocalCurrentGraphicsContext localContext(context);
    GraphicsContextStateSaver stateSaver(context);

    auto enabled = isEnabled(o) && !isReadOnlyControl(o);

    if (shouldPaintCustomTextField(o)) {
        constexpr int strokeThickness = 1;

        FloatRect strokeRect(paintRect);
        strokeRect.inflate(-strokeThickness / 2.0f);

        context.setStrokeColor(enabled ? Color::black : Color::darkGray);
        context.setStrokeStyle(SolidStroke);
        context.strokeRect(strokeRect, strokeThickness);
    } else {
        // <rdar://problem/22896977> We adjust the paint rect here to account for how AppKit draws the text
        // field cell slightly smaller than the rect we pass to drawWithFrame.
        AffineTransform transform = context.getCTM();
        if (transform.xScale() > 1 || transform.yScale() > 1) {
            paintRect.inflateX(1 / transform.xScale());
            paintRect.inflateY(2 / transform.yScale());
            paintRect.move(0, -1 / transform.yScale());
        }

        NSTextFieldCell *textField = this->textField();
        [textField setEnabled:enabled];
        [textField drawWithFrame:NSRect(paintRect) inView:documentViewFor(o)];
        [textField setControlView:nil];
    }

#if ENABLE(DATALIST_ELEMENT)
    if (!is<HTMLInputElement>(o.generatingNode()))
        return false;

    const auto& input = downcast<HTMLInputElement>(*(o.generatingNode()));
    if (input.list())
        paintListButtonForInput(o, context, paintRect);
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

FloatSize RenderThemeMac::meterSizeForBounds(const RenderMeter& renderMeter, const FloatRect& bounds) const
{
    auto* control = const_cast<RenderMeter&>(renderMeter).ensureControlPart();
    if (!control)
        return bounds.size();

    auto controlStyle = extractControlStyleForRenderer(renderMeter);
    return control->sizeForBounds(bounds, controlStyle);
}

bool RenderThemeMac::supportsMeter(ControlPartType type, const HTMLMeterElement&) const
{
    return type == ControlPartType::Meter;
}

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
    if (ControlPartType::NoControl == renderObject.style().effectiveAppearance())
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

Seconds RenderThemeMac::animationRepeatIntervalForProgressBar(const RenderProgress&) const
{
    return progressAnimationRepeatInterval;
}

void RenderThemeMac::adjustProgressBarStyle(RenderStyle&, const Element*) const
{
}

bool RenderThemeMac::paintProgressBar(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!is<RenderProgress>(renderObject))
        return true;

    LocalDefaultSystemAppearance localAppearance(renderObject.useDarkAppearance(), renderObject.style().effectiveAccentColor());

    IntRect inflatedRect = progressBarRectForBounds(renderObject, rect);
    NSControlSize controlSize = controlSizeForFont(renderObject.style());
    const auto& renderProgress = downcast<RenderProgress>(renderObject);
    float deviceScaleFactor = renderObject.document().deviceScaleFactor();
    bool isIndeterminate = renderProgress.position() < 0;
    auto imageBuffer = paintInfo.context().createImageBuffer(inflatedRect.size(), deviceScaleFactor);
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
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[NSAppearance currentAppearance] _drawInRect:NSMakeRect(0, 0, inflatedRect.width(), inflatedRect.height()) context:cgContext options:@{
    ALLOW_DEPRECATED_DECLARATIONS_END
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)(isIndeterminate ? kCUIWidgetProgressIndeterminateBar : kCUIWidgetProgressBar),
        (__bridge NSString *)kCUIValueKey: @(isIndeterminate ? 1 : std::min(nextafter(1.0, -1), renderProgress.position())),
        (__bridge NSString *)kCUISizeKey: (__bridge NSString *)coreUISizeForProgressBarSize(controlSize),
        (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: (__bridge NSString *)kCUIUserInterfaceLayoutDirectionLeftToRight,
        (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
        (__bridge NSString *)kCUIPresentationStateKey: (__bridge NSString *)(isActive(renderObject) ? kCUIPresentationStateActiveKey : kCUIPresentationStateInactive),
        (__bridge NSString *)kCUIOrientationKey: (__bridge NSString *)kCUIOrientHorizontal,
        (__bridge NSString *)kCUIAnimationStartTimeKey: @(renderProgress.animationStartTime().secondsSinceEpoch().seconds()),
        (__bridge NSString *)kCUIAnimationTimeKey: @(MonotonicTime::now().secondsSinceEpoch().seconds())
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

void RenderThemeMac::paintMenuListButtonDecorations(const RenderBox& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
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
        return;

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

    constexpr auto leftSeparatorColor = Color::black.colorWithAlphaByte(40);
    constexpr auto rightSeparatorColor = Color::white.colorWithAlphaByte(40);

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
}

static const IntSize* menuListButtonSizes()
{
    static const IntSize sizes[4] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15), IntSize(0, 28) };
    return sizes;
}

void RenderThemeMac::adjustMenuListStyle(RenderStyle& style, const Element* e) const
{
    RenderTheme::adjustMenuListStyle(style, e);
    NSControlSize controlSize = controlSizeForFont(style);

    style.resetBorder();
    style.resetPadding();

    // Height is locked to auto.
    style.setHeight(Length(LengthType::Auto));

    // White-space is locked to pre
    style.setWhiteSpace(WhiteSpace::Pre);

    // Set the button's vertical size.
    setSizeFromFont(style, menuListButtonSizes());

    // Our font is locked to the appropriate system font size for the control.  To clarify, we first use the CSS-specified font to figure out
    // a reasonable control size, but once that control size is determined, we throw that font away and use the appropriate
    // system font for the control size instead.
    setFontFromControlSize(style, controlSize);

    style.setBoxShadow(nullptr);
}

LengthBox RenderThemeMac::popupInternalPaddingBox(const RenderStyle& style, const Settings&) const
{
    if (style.effectiveAppearance() == ControlPartType::Menulist) {
        const int* padding = popupButtonPadding(controlSizeForFont(style), style.direction() == TextDirection::RTL);
        return { static_cast<int>(padding[topPadding] * style.effectiveZoom()),
            static_cast<int>(padding[rightPadding] * style.effectiveZoom()),
            static_cast<int>(padding[bottomPadding] * style.effectiveZoom()),
            static_cast<int>(padding[leftPadding] * style.effectiveZoom()) };
    }

    if (style.effectiveAppearance() == ControlPartType::MenulistButton) {
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

    const int minHeight = 18;
    style.setMinHeight(Length(minHeight, LengthType::Fixed));

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
    LocalDefaultSystemAppearance localAppearance(renderer.useDarkAppearance(), renderer.style().effectiveAccentColor());
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

    if (o.style().effectiveAppearance() ==  ControlPartType::SliderHorizontal) {
        bounds.setHeight(zoomedTrackWidth);
        bounds.setY(r.y() + r.height() / 2 - zoomedTrackWidth / 2);
    } else if (o.style().effectiveAppearance() == ControlPartType::SliderVertical) {
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
    if (o.style().effectiveAppearance() == ControlPartType::SliderVertical)
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
    NSSliderCell* sliderThumbCell = o.style().effectiveAppearance() == ControlPartType::SliderThumbVertical
        ? sliderThumbVertical()
        : sliderThumbHorizontal();

    LocalDefaultSystemAppearance localAppearance(o.useDarkAppearance());

    LocalCurrentGraphicsContext localContext(paintInfo.context());

    // Update the various states we respond to.
    updateEnabledState(sliderThumbCell, o);
    RefPtr element = dynamicDowncast<Element>(o.node());
    RefPtr delegate = element;
    if (is<SliderThumbElement>(element))
        delegate = downcast<SliderThumbElement>(*element).hostInput();
    updateFocusedState(sliderThumbCell, delegate ? delegate->renderer() : nullptr);

    // Update the pressed state using the NSCell tracking methods, since that's how NSSliderCell keeps track of it.
    bool oldPressed;
    if (o.style().effectiveAppearance() == ControlPartType::SliderThumbVertical)
        oldPressed = m_isSliderThumbVerticalPressed;
    else
        oldPressed = m_isSliderThumbHorizontalPressed;

    bool pressed = isPressed(o);

    if (o.style().effectiveAppearance() == ControlPartType::SliderThumbVertical)
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
    if (o.style().effectiveAppearance() == ControlPartType::SliderThumbVertical)
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
    updateFocusedState(search, &o);
}

const IntSize* RenderThemeMac::searchFieldSizes() const
{
    static const IntSize sizes[4] = { IntSize(0, 22), IntSize(0, 19), IntSize(0, 17), IntSize(0, 30) };
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

    // Adjust the font size prior to adjusting height, as the adjusted size may
    // correspond to a different control size when style.effectiveZoom() != 1.
    setFontFromControlSize(style, controlSizeForFont(style));

    // Override height.
    style.setHeight(Length(LengthType::Auto));
    setSearchFieldSize(style);

    // Override padding size to match AppKit text positioning.
    const int padding = 1 * style.effectiveZoom();
    style.setPaddingLeft(Length(padding, LengthType::Fixed));
    style.setPaddingRight(Length(padding, LengthType::Fixed));
    style.setPaddingTop(Length(padding, LengthType::Fixed));
    style.setPaddingBottom(Length(padding, LengthType::Fixed));

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
    style.setWidth(Length(size.width(), LengthType::Fixed));
    style.setHeight(Length(size.height(), LengthType::Fixed));
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
    style.setWidth(Length(size.width() - emptyResultsOffset, LengthType::Fixed));
    style.setHeight(Length(size.height(), LengthType::Fixed));
    style.setBoxShadow(nullptr);
}

bool RenderThemeMac::paintSearchFieldDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&)
{
    return false;
}

void RenderThemeMac::adjustSearchFieldResultsDecorationPartStyle(RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style.setWidth(Length(size.width(), LengthType::Fixed));
    style.setHeight(Length(size.height(), LengthType::Fixed));
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

    paintCellAndSetFocusedElementNeedsRepaintIfNecessary([search searchButtonCell], inputBox, paintInfo, localBounds);
    [[search searchButtonCell] setControlView:nil];
    return false;
}

void RenderThemeMac::adjustSearchFieldResultsButtonStyle(RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style.setWidth(Length(size.width() + resultsArrowWidth, LengthType::Fixed));
    style.setHeight(Length(size.height(), LengthType::Fixed));
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

// FIXME (<rdar://problem/80870479>): Ideally, this constant should be obtained from AppKit using -[NSSliderCell knobThickness].
// However, the method currently returns an incorrect value, both with and without a control view associated with the cell.
#if HAVE(LARGE_CONTROL_SIZE)
constexpr int sliderThumbThickness = 17;
#else
constexpr int sliderThumbThickness = 15;
#endif

void RenderThemeMac::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    float zoomLevel = style.effectiveZoom();
    if (style.effectiveAppearance() == ControlPartType::SliderThumbHorizontal || style.effectiveAppearance() == ControlPartType::SliderThumbVertical) {
        style.setWidth(Length(static_cast<int>(sliderThumbThickness * zoomLevel), LengthType::Fixed));
        style.setHeight(Length(static_cast<int>(sliderThumbThickness * zoomLevel), LengthType::Fixed));
    }
}

NSPopUpButtonCell* RenderThemeMac::popupButton() const
{
    if (!m_popupButton) {
        m_popupButton = adoptNS([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
        [m_popupButton setUsesItemFromMenu:NO];
        [m_popupButton setFocusRingType:NSFocusRingTypeExterior];
        [m_popupButton setUserInterfaceLayoutDirection:NSUserInterfaceLayoutDirectionLeftToRight];
    }

    return m_popupButton.get();
}

NSSearchFieldCell* RenderThemeMac::search() const
{
    if (!m_search) {
        m_search = adoptNS([[NSSearchFieldCell alloc] initTextCell:@""]);
        [m_search setBezelStyle:NSTextFieldRoundedBezel];
        [m_search setBezeled:YES];
        [m_search setEditable:YES];
        [m_search setFocusRingType:NSFocusRingTypeExterior];
        [m_search setCenteredLook:NO];
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
        [m_sliderThumbHorizontal setSliderType:NSSliderTypeLinear];
        [m_sliderThumbHorizontal setControlSize:NSControlSizeSmall];
        [m_sliderThumbHorizontal setFocusRingType:NSFocusRingTypeExterior];
    }

    return m_sliderThumbHorizontal.get();
}

NSSliderCell* RenderThemeMac::sliderThumbVertical() const
{
    if (!m_sliderThumbVertical) {
        m_sliderThumbVertical = adoptNS([[NSSliderCell alloc] init]);
        [m_sliderThumbVertical setSliderType:NSSliderTypeLinear];
        [m_sliderThumbVertical setControlSize:NSControlSizeSmall];
        [m_sliderThumbVertical setFocusRingType:NSFocusRingTypeExterior];
    }

    return m_sliderThumbVertical.get();
}

NSTextFieldCell* RenderThemeMac::textField() const
{
    if (!m_textField) {
        m_textField = adoptNS([[WebCoreTextFieldCell alloc] initTextCell:@""]);
        [m_textField setBezeled:YES];
        [m_textField setEditable:YES];
        [m_textField setFocusRingType:NSFocusRingTypeExterior];
        // Post-Lion, WebCore can be in charge of paintinng the background thanks to
        // the workaround in place for <rdar://problem/11385461>, which is implemented
        // above as _coreUIDrawOptionsWithFrame.
        [m_textField setDrawsBackground:NO];
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
    NSServicesRolloverButtonCell *cell = servicesRolloverButtonCell();
    LocalCurrentGraphicsContext localContext(paintInfo.context());
    GraphicsContextStateSaver stateSaver(paintInfo.context());
    paintInfo.context().translate(rect.location());
    IntRect innerFrame(IntPoint(), rect.size());
    [cell drawWithFrame:innerFrame inView:documentViewFor(renderer)];
    [cell setControlView:nil];
    return false;
}

IntSize RenderThemeMac::imageControlsButtonSize() const
{
    return IntSize(servicesRolloverButtonCell().cellSize);
}

bool RenderThemeMac::isImageControl(const Element& elementPtr) const
{
    return ImageControlsMac::isImageControlsButtonElement(elementPtr);
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)

LayoutSize RenderThemeMac::attachmentIntrinsicSize(const RenderAttachment& attachment) const
{
    AttachmentLayout layout(attachment, AttachmentLayoutStyle::NonSelected);
    return LayoutSize(layout.attachmentRect.size());
}

static RefPtr<Icon> iconForAttachment(const String& fileName, const String& attachmentType, const String& title)
{
    if (!attachmentType.isEmpty() && !equalLettersIgnoringASCIICase(attachmentType, "public.data"_s)) {
        if (equalLettersIgnoringASCIICase(attachmentType, "public.directory"_s) || equalLettersIgnoringASCIICase(attachmentType, "multipart/x-folder"_s) || equalLettersIgnoringASCIICase(attachmentType, "application/vnd.apple.folder"_s)) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            auto type = kUTTypeFolder;
ALLOW_DEPRECATED_DECLARATIONS_END
            if (auto icon = Icon::createIconForUTI(type))
                return icon;
        } else {
            String type;
            if (isDeclaredUTI(attachmentType))
                type = attachmentType;
            else
                type = UTIFromMIMEType(attachmentType);

            if (auto icon = Icon::createIconForUTI(type))
                return icon;
        }
    }

    if (!fileName.isEmpty()) {
        if (auto icon = Icon::createIconForFiles({ fileName }))
            return icon;
    }

    NSString *cocoaTitle = title;
    if (auto fileExtension = cocoaTitle.pathExtension; fileExtension.length) {
        if (auto icon = Icon::createIconForFileExtension(fileExtension))
            return icon;
    }

    return Icon::createIconForUTI("public.data"_s);
}

RetainPtr<NSImage> RenderThemeMac::iconForAttachment(const String& fileName, const String& attachmentType, const String& title)
{
    if (fileName.isNull() && attachmentType.isNull() && title.isNull())
        return nil;

    if (auto icon = WebCore::iconForAttachment(fileName, attachmentType, title)) {
        auto imageForIcon = adoptNS([[NSImage alloc] initWithCGImage:icon->image()->platformImage().get() size:NSZeroSize]);
        // Need this because WebCore uses AppKit's flipped coordinate system exclusively.
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [imageForIcon setFlipped:YES];
        ALLOW_DEPRECATED_DECLARATIONS_END
        return imageForIcon;
    }

    return nil;
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

static bool shouldDrawIcon(const String& title)
{
#if HAVE(QUICKLOOK_THUMBNAILING)
    // The thumbnail will be painted by the client.
    NSString *cocoaTitle = title;
    if (auto fileExtension = cocoaTitle.pathExtension; fileExtension.length) {
        return ![fileExtension isEqualToString:@"key"]
            && ![fileExtension isEqualToString:@"pages"]
            && ![fileExtension isEqualToString:@"numbers"];
    }
#endif
    UNUSED_PARAM(title);
    return true;
}

static void paintAttachmentIcon(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout)
{
    if (auto thumbnailIcon = attachment.attachmentElement().thumbnail()) {
        context.drawImage(*thumbnailIcon, layout.iconRect);
        return;
    }

    if (context.paintingDisabled())
        return;

    auto icon = attachment.attachmentElement().icon();
    if (!icon) {
        attachment.attachmentElement().requestIconWithSize(layout.iconRect.size());
        return;
    }
    
    if (!shouldDrawIcon(attachment.attachmentElement().attachmentTitleForDisplay()))
        return;

    context.drawImage(*icon, layout.iconRect, { ImageOrientation::OriginBottomLeft });
}

static std::pair<RefPtr<Image>, float> createAttachmentPlaceholderImage(float deviceScaleFactor, const AttachmentLayout& layout)
{
#if HAVE(ALTERNATE_ICONS)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto image = [NSImage _imageWithSystemSymbolName:@"arrow.down.circle"];
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto imageSize = FloatSize([image size]);
    auto imageSizeScales = deviceScaleFactor * layout.iconRect.size() / imageSize;
    imageSize.scale(std::min(imageSizeScales.width(), imageSizeScales.height()));
    auto imageRect = NSMakeRect(0, 0, imageSize.width(), imageSize.height());
    auto cgImage = [image CGImageForProposedRect:&imageRect context:nil hints:@{
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        NSImageHintSymbolFont : [NSFont systemFontOfSize:32],
        NSImageHintSymbolScale : @(NSImageSymbolScaleMedium)
        ALLOW_DEPRECATED_DECLARATIONS_END
    }];
    return { BitmapImage::create(cgImage), deviceScaleFactor };
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

    auto backgroundRects = layout.lines.map([](auto& line) {
        return line.backgroundRect;
    });

    Color backgroundColor;
    if (attachment.frame().selection().isFocusedAndActive()) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        backgroundColor = colorFromCocoaColor([NSColor alternateSelectedControlColor]);
        ALLOW_DEPRECATED_DECLARATIONS_END
    } else
        backgroundColor = attachmentTitleInactiveBackgroundColor;

    backgroundColor = attachment.style().colorByApplyingColorFilter(backgroundColor);
    context.setFillColor(backgroundColor);

    Path backgroundPath = PathUtilities::pathWithShrinkWrappedRects(backgroundRects, attachmentTitleBackgroundRadius);
    context.fillPath(backgroundPath);
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
    paintAttachmentText(context, &layout);

    if (validProgress && progress)
        paintAttachmentProgress(attachment, context, layout, progress);

    if (usePlaceholder)
        paintAttachmentPlaceholderBorder(attachment, context, layout);

    return true;
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

} // namespace WebCore

#endif // PLATFORM(MAC)
