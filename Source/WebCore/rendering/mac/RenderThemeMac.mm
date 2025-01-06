/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
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
#import "FrameSelection.h"
#import "GeometryUtilities.h"
#import "GraphicsContext.h"
#import "HTMLAttachmentElement.h"
#import "HTMLInputElement.h"
#import "HTMLMeterElement.h"
#import "HTMLNames.h"
#import "HTMLPlugInImageElement.h"
#import "Icon.h"
#import "Image.h"
#import "ImageControlsButtonMac.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "LocalFrame.h"
#import "LocalFrameView.h"
#import "LocalizedStrings.h"
#import "Logging.h"
#import "PaintInfo.h"
#import "PathUtilities.h"
#import "RenderAttachment.h"
#import "RenderMedia.h"
#import "RenderMeter.h"
#import "RenderSlider.h"
#import "RenderStyleSetters.h"
#import "RenderView.h"
#import "SliderThumbElement.h"
#import "StringTruncator.h"
#import "ThemeMac.h"
#import "UTIUtilities.h"
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>
#import <math.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <pal/spi/mac/NSCellSPI.h>
#import <pal/spi/mac/NSColorSPI.h>
#import <pal/spi/mac/NSGraphicsSPI.h>
#import <pal/spi/mac/NSImageSPI.h>
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>
#import <wtf/MathExtras.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

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

bool RenderThemeMac::canPaint(const PaintInfo& paintInfo, const Settings&, StyleAppearance appearance) const
{
    switch (appearance) {
#if ENABLE(ATTACHMENT_ELEMENT)
    case StyleAppearance::Attachment:
    case StyleAppearance::BorderlessAttachment:
#endif
#if ENABLE(APPLE_PAY)
    case StyleAppearance::ApplePayButton:
#endif
    case StyleAppearance::Button:
    case StyleAppearance::Checkbox:
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
#endif
    case StyleAppearance::DefaultButton:
#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
#endif
    case StyleAppearance::InnerSpinButton:
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Meter:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Radio:
    case StyleAppearance::PushButton:
    case StyleAppearance::SearchField:
    case StyleAppearance::SearchFieldCancelButton:
    case StyleAppearance::SearchFieldResultsButton:
    case StyleAppearance::SearchFieldResultsDecoration:
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
    case StyleAppearance::SwitchThumb:
    case StyleAppearance::SwitchTrack:
    case StyleAppearance::SquareButton:
    case StyleAppearance::TextArea:
    case StyleAppearance::TextField:
        return true;
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
    auto type = renderer.style().usedAppearance();
    return type == StyleAppearance::Button
        || type == StyleAppearance::Checkbox
#if ENABLE(APPLE_PAY)
        || type == StyleAppearance::ApplePayButton
#endif
#if ENABLE(INPUT_TYPE_COLOR)
        || type == StyleAppearance::ColorWell
#endif
        || type == StyleAppearance::DefaultButton
#if ENABLE(SERVICE_CONTROLS)
        || type == StyleAppearance::ImageControlsButton
#endif
        || type == StyleAppearance::InnerSpinButton
        || type == StyleAppearance::Menulist
        || type == StyleAppearance::Meter
        || type == StyleAppearance::ProgressBar
        || type == StyleAppearance::PushButton
        || type == StyleAppearance::Radio
        || type == StyleAppearance::SearchField
        || type == StyleAppearance::SearchFieldCancelButton
        || type == StyleAppearance::SearchFieldResultsButton
        || type == StyleAppearance::SearchFieldResultsDecoration
        || type == StyleAppearance::SliderThumbHorizontal
        || type == StyleAppearance::SliderThumbVertical
        || type == StyleAppearance::SliderHorizontal
        || type == StyleAppearance::SliderVertical
        || type == StyleAppearance::SquareButton
        || type == StyleAppearance::SwitchThumb
        || type == StyleAppearance::SwitchTrack;
}

bool RenderThemeMac::canCreateControlPartForBorderOnly(const RenderObject& renderer) const
{
    auto appearance = renderer.style().usedAppearance();
    return appearance == StyleAppearance::Listbox
        || appearance == StyleAppearance::TextArea
        || appearance == StyleAppearance::TextField;
}

bool RenderThemeMac::canCreateControlPartForDecorations(const RenderObject& renderer) const
{
    return renderer.style().usedAppearance() == StyleAppearance::MenulistButton;
}

int RenderThemeMac::baselinePosition(const RenderBox& renderer) const
{
    auto appearance = renderer.style().usedAppearance();
    auto baseline = RenderTheme::baselinePosition(renderer);
    if ((appearance == StyleAppearance::Checkbox || appearance == StyleAppearance::Radio) && renderer.isHorizontalWritingMode())
        return baseline - (2 * renderer.style().usedZoom());
    return baseline;
}

bool RenderThemeMac::supportsLargeFormControls() const
{
    return ThemeMac::supportsLargeFormControls();
}

Color RenderThemeMac::platformActiveSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor selectedTextBackgroundColor]);
}

Color RenderThemeMac::platformInactiveSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor unemphasizedSelectedTextBackgroundColor]);
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
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    if (localAppearance.usingDarkAppearance())
        return colorFromCocoaColor([NSColor unemphasizedSelectedTextColor]);
    return { };
}

Color RenderThemeMac::platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor selectedContentBackgroundColor]);
}

Color RenderThemeMac::platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor unemphasizedSelectedContentBackgroundColor]);

}

Color RenderThemeMac::platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor alternateSelectedControlTextColor]);
}

Color RenderThemeMac::platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
    return colorFromCocoaColor([NSColor unemphasizedSelectedTextColor]);
}

inline static Color defaultFocusRingColor(OptionSet<StyleColorOptions> options)
{
    // Hardcoded to avoid exposing a user appearance preference to the web for fingerprinting.
    return {
        options.contains(StyleColorOptions::UseDarkAppearance) ? SRGBA<uint8_t> { 26, 169, 255 } : SRGBA<uint8_t> { 0, 103, 244 },
        Color::Flags::Semantic
    };
}

Color RenderThemeMac::platformFocusRingColor(OptionSet<StyleColorOptions> options) const
{
    if (usesTestModeFocusRingColor())
        return oldAquaFocusRingColor();
    if (!options.contains(StyleColorOptions::UseSystemAppearance))
        return defaultFocusRingColor(options);
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

Color RenderThemeMac::platformAutocorrectionReplacementMarkerColor(OptionSet<StyleColorOptions> options) const
{
#if HAVE(AUTOCORRECTION_ENHANCEMENTS)
    if ([NSSpellChecker respondsToSelector:@selector(correctionIndicatorUnderlineColor)]) {
        LocalDefaultSystemAppearance localAppearance(options.contains(StyleColorOptions::UseDarkAppearance));
        return colorFromCocoaColor([NSSpellChecker correctionIndicatorUnderlineColor]);
    }
#endif
    return RenderThemeCocoa::platformAutocorrectionReplacementMarkerColor(options);
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
    RetainPtr offscreenRep = adoptNS([[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil pixelsWide:1 pixelsHigh:1
        bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:4 bitsPerPixel:32]);

    {
        LocalCurrentCGContext localContext { [NSGraphicsContext graphicsContextWithBitmapImageRep:offscreenRep.get()].CGContext };

        [[NSColor clearColor] set];

        NSRect rect = NSMakeRect(0, 0, 1, 1);
        NSRectFill(rect);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        NSDrawMenuBackground(rect, NSZeroRect, NSMenuBackgroundPopupMenu);
ALLOW_DEPRECATED_DECLARATIONS_END
    }

    std::array<NSUInteger, 4> pixel;
    [offscreenRep getPixel:pixel.data() atX:0 y:0];

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
        // These are available only when the web view opts into the system appearance.
        case CSSValueWebkitFocusRingColor:
        case CSSValueActiveborder:
            return focusRingColor(options);

        case CSSValueAppleSystemControlAccent:
            return systemAppearanceColor(cache.systemControlAccentColor, @selector(controlAccentColor));

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

    auto it = cache.systemStyleColors.find(cssValueID);
    if (it != cache.systemStyleColors.end())
        return it->value;

    auto color = [this, cssValueID, options, useDarkAppearance]() -> Color {
        LocalDefaultSystemAppearance localAppearance(useDarkAppearance);

        auto selectCocoaColor = [cssValueID, useDarkAppearance] () -> SEL {
            switch (cssValueID) {
            case CSSValueActivecaption:
                return @selector(windowFrameTextColor);
            case CSSValueAppworkspace:
                return @selector(headerColor);
            case CSSValueButtonface:
            case CSSValueThreedface:
                // Fallback to hardcoded color below in light mode.
                return useDarkAppearance ? @selector(controlColor) : nullptr;
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
                return @selector(labelColor);
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
            case CSSValueWebkitControlBackground:
                return @selector(controlBackgroundColor);
            case CSSValueAppleSystemAlternateSelectedText:
                return @selector(alternateSelectedControlTextColor);
            case CSSValueAppleSystemUnemphasizedSelectedContentBackground:
                return @selector(unemphasizedSelectedContentBackgroundColor);
            case CSSValueAppleSystemSelectedText:
                return @selector(selectedTextColor);
            case CSSValueAppleSystemUnemphasizedSelectedText:
                return @selector(unemphasizedSelectedTextColor);
            case CSSValueAppleSystemUnemphasizedSelectedTextBackground:
                return @selector(unemphasizedSelectedTextBackgroundColor);
            case CSSValueAppleSystemPlaceholderText:
                return @selector(placeholderTextColor);
            case CSSValueAppleSystemFindHighlightBackground:
                return @selector(findHighlightColor);
            case CSSValueAppleSystemContainerBorder:
                return @selector(containerBorderColor);
            case CSSValueAppleSystemLabel:
                return @selector(labelColor);
            case CSSValueAppleSystemSecondaryLabel:
                return @selector(secondaryLabelColor);
            case CSSValueAppleSystemTertiaryLabel:
                return @selector(tertiaryLabelColor);
            case CSSValueAppleSystemQuaternaryLabel:
                return @selector(quaternaryLabelColor);
#if HAVE(NSCOLOR_FILL_COLOR_HIERARCHY)
            case CSSValueAppleSystemTertiaryFill:
                return @selector(tertiarySystemFillColor);
#endif
            case CSSValueAppleSystemGrid:
                return @selector(gridColor);
            case CSSValueAppleSystemSeparator:
                return @selector(separatorColor);
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
            // Dark mode uses [NSColor controlColor].
            // We selected this value instead of [NSColor controlColor] to avoid website incompatibilities.
            // We may want to consider changing to [NSColor controlColor] some day.
            ASSERT(!localAppearance.usingDarkAppearance());
            return Color::lightGray;

        case CSSValueInfobackground:
            // No corresponding NSColor for this so we use a hard coded value.
            return SRGBA<uint8_t> { 251, 252, 197 };

        case CSSValueMenu:
            return menuBackgroundColor();

        case CSSValueWebkitFocusRingColor:
        case CSSValueActiveborder:
            return defaultFocusRingColor(options);

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

        case CSSValueAppleSystemEvenAlternatingContentBackground: {
            NSArray<NSColor *> *alternateColors = [NSColor alternatingContentBackgroundColors];
            ASSERT(alternateColors.count >= 2);
            return semanticColorFromNSColor(alternateColors[0]);
        }

        case CSSValueAppleSystemOddAlternatingContentBackground: {
            NSArray<NSColor *> *alternateColors = [NSColor alternatingContentBackgroundColors];
            ASSERT(alternateColors.count >= 2);
            return semanticColorFromNSColor(alternateColors[1]);
        }

        // FIXME: Remove this fallback when AppKit without tertiary-fill is not used anymore; see rdar://108340604.
        case CSSValueAppleSystemTertiaryFill:
            if (localAppearance.usingDarkAppearance())
                return { SRGBA<uint8_t> { 255, 255, 255, 12 }, Color::Flags::Semantic };
            return { SRGBA<uint8_t> { 0, 0, 0, 12 }, Color::Flags::Semantic };

        case CSSValueBackground:
            // Use platform-independent value returned by base class.
            FALLTHROUGH;

        default:
            return RenderTheme::systemColor(cssValueID, options);
        }
    }();

    cache.systemStyleColors.add(cssValueID, color);
    return color;
}

bool RenderThemeMac::usesTestModeFocusRingColor() const
{
    return WebCore::usesTestModeFocusRingColor();
}

bool RenderThemeMac::searchFieldShouldAppearAsTextField(const RenderStyle& style) const
{
    return !style.writingMode().isHorizontal();
}

bool RenderThemeMac::isControlStyled(const RenderStyle& style, const RenderStyle& userAgentStyle) const
{
    auto appearance = style.usedAppearance();
    if (appearance == StyleAppearance::TextField || appearance == StyleAppearance::TextArea || appearance == StyleAppearance::SearchField || appearance == StyleAppearance::Listbox)
        return style.border() != userAgentStyle.border();

    // FIXME: This is horrible, but there is not much else that can be done. Menu lists cannot draw properly when
    // scaled.  They can't really draw properly when transformed either.  We can't detect the transform case at style
    // adjustment time so that will just have to stay broken.  We can however detect that we're zooming.  If zooming
    // is in effect we treat it like the control is styled. Additionally, treat the control like it is styled when
    // using a vertical writing mode, since the AppKit control is not height resizable.
    if (appearance == StyleAppearance::Menulist && (style.usedZoom() != 1.0f || !style.writingMode().isHorizontal()))
        return true;

    return RenderTheme::isControlStyled(style, userAgentStyle);
}

static FloatRect inflateRect(const FloatRect& rect, const IntSize& size, std::span<const int, 4> margins, float zoomLevel)
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

static NSControlSize controlSizeFromPixelSize(std::span<const IntSize, 4> sizes, const IntSize& minSize, float zoomLevel)
{
    if (ThemeMac::supportsLargeFormControls()
        && minSize.width() >= static_cast<int>(sizes[NSControlSizeLarge].width() * zoomLevel)
        && minSize.height() >= static_cast<int>(sizes[NSControlSizeLarge].height() * zoomLevel))
        return NSControlSizeLarge;

    if (minSize.width() >= static_cast<int>(sizes[NSControlSizeRegular].width() * zoomLevel)
        && minSize.height() >= static_cast<int>(sizes[NSControlSizeRegular].height() * zoomLevel))
        return NSControlSizeRegular;

    if (minSize.width() >= static_cast<int>(sizes[NSControlSizeSmall].width() * zoomLevel)
        && minSize.height() >= static_cast<int>(sizes[NSControlSizeSmall].height() * zoomLevel))
        return NSControlSizeSmall;

    return NSControlSizeMini;
}

static std::span<const int, 4> popupButtonMargins(NSControlSize size)
{
    static constexpr std::array margins {
        std::array { 0, 3, 1, 3 },
        std::array { 0, 3, 2, 3 },
        std::array { 0, 1, 0, 1 },
        std::array { 0, 6, 2, 6 },
    };
    return margins[size];
}

static std::span<const IntSize, 4> popupButtonSizes()
{
    static constexpr std::array sizes { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15), IntSize(0, 24) };
    return sizes;
}

static std::span<const int, 4> popupButtonPadding(NSControlSize size, bool isRTL)
{
    static constexpr std::array paddingLTR {
        std::array { 2, 26, 3, 8 },
        std::array { 2, 23, 3, 8 },
        std::array { 2, 22, 3, 10 },
        std::array { 2, 26, 3, 8 },
    };
    static constexpr std::array paddingRTL {
        std::array { 2, 8, 3, 26 },
        std::array { 2, 8, 3, 23 },
        std::array { 2, 8, 3, 22 },
        std::array { 2, 8, 3, 26 },
    };
    return isRTL ? paddingRTL[size] : paddingLTR[size];
}

void RenderThemeMac::inflateRectForControlRenderer(const RenderObject& renderer, FloatRect& rect)
{
    auto appearance = renderer.style().usedAppearance();

    switch (appearance) {
    case StyleAppearance::Button:
    case StyleAppearance::Checkbox:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::InnerSpinButton:
    case StyleAppearance::PushButton:
    case StyleAppearance::Radio:
    case StyleAppearance::Switch:
        ThemeMac::inflateControlPaintRect(renderer.style().usedAppearance(), rect, renderer.style().usedZoom(), !renderer.writingMode().isHorizontal());
        break;
    case StyleAppearance::Menulist: {
        auto zoomLevel = renderer.style().usedZoom();
        auto controlSize = controlSizeFromPixelSize(popupButtonSizes(), IntSize(rect.size()), zoomLevel);
        auto size = popupButtonSizes()[controlSize];
        size.setHeight(size.height() * zoomLevel);
        size.setWidth(rect.width());
        rect = inflateRect(rect, size, popupButtonMargins(controlSize), zoomLevel);
        break;
    }
    default:
        break;
    }
}

void RenderThemeMac::adjustRepaintRect(const RenderBox& renderer, FloatRect& rect)
{
    auto repaintRect = rect;
    inflateRectForControlRenderer(renderer, repaintRect);
    renderer.flipForWritingMode(repaintRect);
    rect = repaintRect;
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
    if (o.style().usedAppearance() == StyleAppearance::Checkbox)
        return isChecked(o);

    // For now assume other controls have tint if enabled.
    return true;
}

static NSControlSize controlSizeForSystemFont(const RenderStyle& style)
{
    auto fontSize = style.computedFontSize();
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSControlSizeLarge] && ThemeMac::supportsLargeFormControls())
        return NSControlSizeLarge;
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSControlSizeRegular])
        return NSControlSizeRegular;
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSControlSizeSmall])
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

static NSControlSize controlSizeForFont(const RenderStyle& style)
{
    auto fontSize = style.computedFontSize();
    if (fontSize >= 21 && ThemeMac::supportsLargeFormControls())
        return NSControlSizeLarge;
    if (fontSize >= 16)
        return NSControlSizeRegular;
    if (fontSize >= 11)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

static IntSize sizeForFont(const RenderStyle& style, std::span<const IntSize, 4> sizes)
{
    if (style.usedZoom() != 1.0f) {
        IntSize result = sizes[controlSizeForFont(style)];
        return IntSize(result.width() * style.usedZoom(), result.height() * style.usedZoom());
    }
    return sizes[controlSizeForFont(style)];
}

static IntSize sizeForSystemFont(const RenderStyle& style, std::span<const IntSize, 4> sizes)
{
    if (style.usedZoom() != 1.0f) {
        IntSize result = sizes[controlSizeForSystemFont(style)];
        return IntSize(result.width() * style.usedZoom(), result.height() * style.usedZoom());
    }
    return sizes[controlSizeForSystemFont(style)];
}

static void setSizeFromFont(RenderStyle& style, std::span<const IntSize, 4> sizes)
{
    // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
    IntSize size = sizeForFont(style, sizes);
    if (style.width().isIntrinsicOrAuto() && size.width() > 0)
        style.setWidth(Length(size.width(), LengthType::Fixed));
    if (style.height().isAuto() && size.height() > 0)
        style.setHeight(Length(size.height(), LengthType::Fixed));
}

static void setFontFromControlSize(RenderStyle& style, NSControlSize controlSize)
{
    FontCascadeDescription fontDescription;
    fontDescription.setIsAbsoluteSize(true);

    NSFont* font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSize]];
    fontDescription.setOneFamily("-apple-system"_s);
    fontDescription.setComputedSize([font pointSize] * style.usedZoom());
    fontDescription.setSpecifiedSize([font pointSize] * style.usedZoom());

    // Reset line height
    style.setLineHeight(RenderStyle::initialLineHeight());
    style.setFontDescription(WTFMove(fontDescription));
}

#if ENABLE(DATALIST_ELEMENT)

void RenderThemeMac::adjustListButtonStyle(RenderStyle& style, const Element*) const
{
    // Add a margin to place the button at end of the input field.
    style.setMarginEnd(Length(-4, LengthType::Fixed));
}

#endif

#if ENABLE(SERVICE_CONTROLS)
void RenderThemeMac::adjustImageControlsButtonStyle(RenderStyle& style, const Element*) const
{
    style.setHeight(Length(imageControlsButtonSize().height(), LengthType::Fixed));
    style.setWidth(Length(imageControlsButtonSize().width(), LengthType::Fixed));
}
#endif

FloatSize RenderThemeMac::meterSizeForBounds(const RenderMeter& renderMeter, const FloatRect& bounds) const
{
    auto* control = const_cast<RenderMeter&>(renderMeter).ensureControlPartForRenderer();
    if (!control)
        return bounds.size();

    auto controlStyle = extractControlStyleForRenderer(renderMeter);
    return control->sizeForBounds(bounds, controlStyle);
}

bool RenderThemeMac::supportsMeter(StyleAppearance appearance) const
{
    return appearance == StyleAppearance::Meter;
}

IntRect RenderThemeMac::progressBarRectForBounds(const RenderProgress& renderProgress, const IntRect& bounds) const
{
    auto* control = const_cast<RenderProgress&>(renderProgress).ensureControlPartForRenderer();
    if (!control)
        return bounds;

    auto controlStyle = extractControlStyleForRenderer(renderProgress);
    return IntRect(control->rectForBounds(bounds, controlStyle));
}

const float baseFontSize = 11.0f;
const float baseArrowWidth = 5.0f;
const int arrowPaddingBefore = 6;
const int arrowPaddingAfter = 6;
const int paddingBeforeSeparator = 4;
const int baseBorderRadius = 5;
const int styledPopupPaddingLeft = 8;
const int styledPopupPaddingTop = 1;
const int styledPopupPaddingBottom = 2;

static std::span<const IntSize, 4> menuListButtonSizes()
{
    static constexpr std::array sizes { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15), IntSize(0, 28) };
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
    style.setWhiteSpaceCollapse(WhiteSpaceCollapse::Preserve);
    style.setTextWrapMode(TextWrapMode::NoWrap);

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
    if (style.usedAppearance() == StyleAppearance::Menulist) {
        auto padding = popupButtonPadding(controlSizeForFont(style), style.writingMode().isBidiRTL());
        return { static_cast<int>(padding[topPadding] * style.usedZoom()),
            static_cast<int>(padding[rightPadding] * style.usedZoom()),
            static_cast<int>(padding[bottomPadding] * style.usedZoom()),
            static_cast<int>(padding[leftPadding] * style.usedZoom()) };
    }

    if (style.usedAppearance() == StyleAppearance::MenulistButton) {
        float arrowWidth = baseArrowWidth * (style.computedFontSize() / baseFontSize);
        float rightPadding = ceilf(arrowWidth + (arrowPaddingBefore + arrowPaddingAfter + paddingBeforeSeparator) * style.usedZoom());
        float leftPadding = styledPopupPaddingLeft * style.usedZoom();
        if (style.writingMode().isBidiRTL())
            std::swap(rightPadding, leftPadding);
        return { static_cast<int>(styledPopupPaddingTop * style.usedZoom()),
            static_cast<int>(rightPadding),
            static_cast<int>(styledPopupPaddingBottom * style.usedZoom()),
            static_cast<int>(leftPadding) };
    }

    return { 0, 0, 0, 0 };
}

PopupMenuStyle::Size RenderThemeMac::popupMenuSize(const RenderStyle& style, IntRect& rect) const
{
    auto size = controlSizeFromPixelSize(popupButtonSizes(), rect.size(), style.usedZoom());
    switch (size) {
    case NSControlSizeRegular:
        return PopupMenuStyle::Size::Normal;
    case NSControlSizeSmall:
        return PopupMenuStyle::Size::Small;
    case NSControlSizeMini:
        return PopupMenuStyle::Size::Mini;
    case NSControlSizeLarge:
        return ThemeMac::supportsLargeFormControls() ? PopupMenuStyle::Size::Large : PopupMenuStyle::Size::Normal;
    default:
        return PopupMenuStyle::Size::Normal;
    }
}

void RenderThemeMac::adjustMenuListButtonStyle(RenderStyle& style, const Element*) const
{
    float fontScale = style.computedFontSize() / baseFontSize;

    style.resetPadding();
    style.setBorderRadius(IntSize(int(baseBorderRadius + fontScale - 1), int(baseBorderRadius + fontScale - 1))); // FIXME: Round up?

    const int minHeight = 18;
    style.setMinHeight(Length(minHeight, LengthType::Fixed));

    style.setLineHeight(RenderStyle::initialLineHeight());
}

std::span<const IntSize, 4> RenderThemeMac::menuListSizes() const
{
    static constexpr std::array sizes { IntSize(9, 0), IntSize(5, 0), IntSize(0, 0), IntSize(13, 0) };
    return sizes;
}

int RenderThemeMac::minimumMenuListSize(const RenderStyle& style) const
{
    return sizeForSystemFont(style, menuListSizes()).width();
}

void RenderThemeMac::adjustSliderTrackStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(nullptr);
}

void RenderThemeMac::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(style, element);
    style.setBoxShadow(nullptr);
}

std::span<const IntSize, 4> RenderThemeMac::searchFieldSizes() const
{
    static constexpr std::array sizes { IntSize(0, 22), IntSize(0, 19), IntSize(0, 17), IntSize(0, 30) };
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
    const short borderWidth = 2 * style.usedZoom();
    style.setBorderLeftWidth(borderWidth);
    style.setBorderLeftStyle(BorderStyle::Inset);
    style.setBorderRightWidth(borderWidth);
    style.setBorderRightStyle(BorderStyle::Inset);
    style.setBorderBottomWidth(borderWidth);
    style.setBorderBottomStyle(BorderStyle::Inset);
    style.setBorderTopWidth(borderWidth);
    style.setBorderTopStyle(BorderStyle::Inset);

    // Adjust the font size prior to adjusting height, as the adjusted size may
    // correspond to a different control size when style.usedZoom() != 1.
    setFontFromControlSize(style, controlSizeForFont(style));

    // Override height.
    style.setHeight(Length(LengthType::Auto));
    setSearchFieldSize(style);

    // Override padding size to match AppKit text positioning.
    const int padding = 1 * style.usedZoom();
    style.setPaddingLeft(Length(padding, LengthType::Fixed));
    style.setPaddingRight(Length(padding, LengthType::Fixed));
    style.setPaddingTop(Length(padding, LengthType::Fixed));
    style.setPaddingBottom(Length(padding, LengthType::Fixed));

    style.setBoxShadow(nullptr);
}

std::span<const IntSize, 4> RenderThemeMac::cancelButtonSizes() const
{
    static constexpr std::array sizes { IntSize(22, 22), IntSize(19, 19), IntSize(15, 15), IntSize(22, 22) };
    return sizes;
}

void RenderThemeMac::adjustSearchFieldCancelButtonStyle(RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, cancelButtonSizes());
    style.setWidth(Length(size.width(), LengthType::Fixed));
    style.setHeight(Length(size.height(), LengthType::Fixed));
    style.setBoxShadow(nullptr);
}

constexpr int resultsArrowWidth = 5;
std::span<const IntSize, 4> RenderThemeMac::resultsButtonSizes() const
{
    static constexpr std::array sizes { IntSize(19, 22), IntSize(17, 19), IntSize(17, 15), IntSize(19, 22) };
    return sizes;
}

const int emptyResultsOffset = 9;
void RenderThemeMac::adjustSearchFieldDecorationPartStyle(RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    int widthOffset = 0;
    int heightOffset = 0;
    if (style.writingMode().isHorizontal())
        widthOffset = emptyResultsOffset;
    else
        heightOffset = emptyResultsOffset;
    style.setWidth(Length(size.width() - widthOffset, LengthType::Fixed));
    style.setHeight(Length(size.height() - heightOffset, LengthType::Fixed));
    style.setBoxShadow(nullptr);
}

void RenderThemeMac::adjustSearchFieldResultsDecorationPartStyle(RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style.setWidth(Length(size.width(), LengthType::Fixed));
    style.setHeight(Length(size.height(), LengthType::Fixed));
    style.setBoxShadow(nullptr);
}

void RenderThemeMac::adjustSearchFieldResultsButtonStyle(RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style.setWidth(Length(size.width() + resultsArrowWidth, LengthType::Fixed));
    style.setHeight(Length(size.height(), LengthType::Fixed));
    style.setBoxShadow(nullptr);
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
constexpr int sliderThumbThickness = 17;

void RenderThemeMac::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    float zoomLevel = style.usedZoom();
    if (style.usedAppearance() == StyleAppearance::SliderThumbHorizontal || style.usedAppearance() == StyleAppearance::SliderThumbVertical) {
        style.setWidth(Length(static_cast<int>(sliderThumbThickness * zoomLevel), LengthType::Fixed));
        style.setHeight(Length(static_cast<int>(sliderThumbThickness * zoomLevel), LengthType::Fixed));
    }
}

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
IntSize RenderThemeMac::imageControlsButtonSize() const
{
    return ImageControlsButtonMac::servicesRolloverButtonCellSize();
}

bool RenderThemeMac::isImageControlsButton(const Element& element) const
{
    return ImageControlsMac::isImageControlsButtonElement(element);
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
// FIXME: Remove after rdar://136373445 is fixed.
#define LOG_ATTACHMENT(fmt, ...) RELEASE_LOG(Editing, "iconForAttachment(type='%s') " fmt, attachmentType.utf8().data(), ##__VA_ARGS__);

    if (!attachmentType.isEmpty() && !equalLettersIgnoringASCIICase(attachmentType, "public.data"_s)) {
        if (equalLettersIgnoringASCIICase(attachmentType, "public.directory"_s) || equalLettersIgnoringASCIICase(attachmentType, "multipart/x-folder"_s) || equalLettersIgnoringASCIICase(attachmentType, "application/vnd.apple.folder"_s)) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            auto type = kUTTypeFolder;
ALLOW_DEPRECATED_DECLARATIONS_END
            if (auto icon = Icon::createIconForUTI(type)) {
                LOG_ATTACHMENT("-> Got icon for kUTTypeFolder");
                return icon;
            }
            LOG_ATTACHMENT("-> No icon for kUTTypeFolder! Will fallback to filename or title...");
        } else {
            String type;
            if (isDeclaredUTI(attachmentType))
                type = attachmentType;
            else
                type = UTIFromMIMEType(attachmentType);

            if (auto icon = Icon::createIconForUTI(type)) {
                LOG_ATTACHMENT("-> Got icon for %s '%s'", type == attachmentType ? "declared UTI" : "UTI-from-MIMEtype", type.utf8().data());
                return icon;
            }
            LOG_ATTACHMENT("-> No icon for %s '%s'! Will fallback to filename or title...", type == attachmentType ? "declared UTI" : "UTI-from-MIMEtype", type.utf8().data());
        }
    }

    if (!fileName.isEmpty()) {
        if (auto icon = Icon::createIconForFiles({ fileName })) {
            LOG_ATTACHMENT("-> Got icon for filename");
            return icon;
        }
        LOG_ATTACHMENT("-> No icon for filename! Will fallback to title...");
    }

    NSString *cocoaTitle = title;
    if (auto fileExtension = cocoaTitle.pathExtension; fileExtension.length) {
        if (auto icon = Icon::createIconForFileExtension(fileExtension)) {
            LOG_ATTACHMENT("-> Got icon for title file extension '%s'", String(fileExtension).utf8().data());
            return icon;
        }
        LOG_ATTACHMENT("-> No icon for title file extension '%s'! Will fallback to public.data icon", String(fileExtension).utf8().data());
    } else
        LOG_ATTACHMENT("-> No file extension in title! Will fallback to public.data icon");

    return Icon::createIconForUTI("public.data"_s);
#undef LOG_ATTACHMENT
}

RetainPtr<NSImage> RenderThemeMac::iconForAttachment(const String& fileName, const String& attachmentType, const String& title)
{
    if (fileName.isNull() && attachmentType.isNull() && title.isNull())
        return nil;

    if (auto icon = WebCore::iconForAttachment(fileName, attachmentType, title))
        return icon->image();

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

    attachment.attachmentElement().requestIconIfNeededWithSize(layout.iconRect.size());
    auto icon = attachment.attachmentElement().icon();
    if (!icon)
        return;
    
    if (!shouldDrawIcon(attachment.attachmentElement().attachmentTitleForDisplay()))
        return;

    context.drawImage(*icon, layout.iconRect);
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
        return { ImageAdapter::loadPlatformResource("AttachmentPlaceholder@2x"), 2 };

    return { ImageAdapter::loadPlatformResource("AttachmentPlaceholder"), 1 };
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
    if (attachment.frame().selection().isFocusedAndActive())
        backgroundColor = colorFromCocoaColor([NSColor selectedContentBackgroundColor]);
    else
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
    context.setStrokeStyle(StrokeStyle::DashedStroke);
    context.setLineDash({attachmentPlaceholderBorderDashLength}, 0);
    context.strokePath(borderPath);
}

bool RenderThemeMac::paintAttachment(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& paintRect)
{
    auto* attachment = dynamicDowncast<RenderAttachment>(renderer);
    if (!attachment)
        return false;

    if (attachment->paintWideLayoutAttachmentOnly(paintInfo, paintRect.location()))
        return true;

    HTMLAttachmentElement& element = attachment->attachmentElement();

    auto layoutStyle = AttachmentLayoutStyle::NonSelected;
    if (attachment->selectionState() != RenderObject::HighlightState::None && paintInfo.phase != PaintPhase::Selection)
        layoutStyle = AttachmentLayoutStyle::Selected;

    AttachmentLayout layout(*attachment, layoutStyle);

    auto& progressString = element.attributeWithoutSynchronization(progressAttr);
    bool validProgress = false;
    float progress = 0;
    if (!progressString.isEmpty())
        progress = progressString.toFloat(&validProgress);

    GraphicsContext& context = paintInfo.context();
    GraphicsContextStateSaver saver(context);

    context.translate(toFloatSize(paintRect.location()));
    context.translate(floorSizeToDevicePixels({ LayoutUnit((paintRect.width() - attachmentIconBackgroundSize) / 2), 0 }, renderer.document().deviceScaleFactor()));

    bool usePlaceholder = validProgress && !progress;

    paintAttachmentIconBackground(*attachment, context, layout);

    if (usePlaceholder)
        paintAttachmentIconPlaceholder(*attachment, context, layout);
    else
        paintAttachmentIcon(*attachment, context, layout);

    paintAttachmentTitleBackground(*attachment, context, layout);
    paintAttachmentText(context, &layout);

    if (validProgress && progress)
        paintAttachmentProgress(*attachment, context, layout, progress);

    if (usePlaceholder)
        paintAttachmentPlaceholderBorder(*attachment, context, layout);

    return true;
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

} // namespace WebCore

#endif // PLATFORM(MAC)
