/*
 * Copyright (C) 2005-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#import "CSSPropertyNames.h"
#import "CSSValueKeywords.h"
#import "CSSValueList.h"
#import "Color.h"
#import "ColorBlending.h"
#import "ColorMac.h"
#import "ColorSerialization.h"
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
#import "HTMLPlugInElement.h"
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
#import "PathOperation.h"
#import "PathUtilities.h"
#import "RenderAttachment.h"
#import "RenderMedia.h"
#import "RenderMeter.h"
#import "RenderSlider.h"
#import "RenderStyleSetters.h"
#import "RenderView.h"
#import "SliderThumbElement.h"
#import "StringTruncator.h"
#import "StylePadding.h"
#import "UTIUtilities.h"
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
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

    RetainPtr systemColorsChangedNotification = NSSystemColorsDidChangeNotification;
    RetainPtr accessibilityDisplayOptionsChangedNotification = NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification;

    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(systemColorsDidChange:) name:systemColorsChangedNotification.get() object:nil];
    [retainPtr([[NSWorkspace sharedWorkspace] notificationCenter]) addObserver:self
        selector:@selector(accessibilityDisplayOptionsDidChange:) name:accessibilityDisplayOptionsChangedNotification.get() object:nil];

    return self;
}

- (void)accessibilityDisplayOptionsDidChange:(NSNotification *)notification
{
    UNUSED_PARAM(notification);
    WebCore::RenderTheme::singleton().platformColorsDidChange();
}

- (void)systemColorsDidChange:(NSNotification *)notification
{
    UNUSED_PARAM(notification);
    WebCore::RenderTheme::singleton().platformColorsDidChange();
    WebCore::Page::updateControlTintsForAllPages();
}

@end

namespace WebCore {

using namespace CSS::Literals;
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

bool RenderThemeMac::canPaint(const PaintInfo& paintInfo, const Settings& settings, StyleAppearance appearance) const
{
#if !ENABLE(FORM_CONTROL_REFRESH)
    UNUSED_PARAM(settings);
#endif

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
    case StyleAppearance::ColorWell:
    case StyleAppearance::ColorWellSwatch:
    case StyleAppearance::ColorWellSwatchOverlay:
    case StyleAppearance::ColorWellSwatchWrapper:
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
    case StyleAppearance::SearchFieldDecoration:
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
#if ENABLE(FORM_CONTROL_REFRESH)
    case StyleAppearance::ListButton:
        return settings.formControlRefreshEnabled();
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

bool RenderThemeMac::canCreateControlPartForRenderer(const RenderElement& renderer) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (renderer.settings().formControlRefreshEnabled())
        return RenderThemeCocoa::canCreateControlPartForRendererForVectorBasedControls(renderer);
#endif

    auto type = renderer.style().usedAppearance();
    return type == StyleAppearance::Button
        || type == StyleAppearance::Checkbox
#if ENABLE(APPLE_PAY)
        || type == StyleAppearance::ApplePayButton
#endif
        || type == StyleAppearance::ColorWell
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

bool RenderThemeMac::canCreateControlPartForBorderOnly(const RenderElement& renderer) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (renderer.settings().formControlRefreshEnabled())
        return RenderThemeCocoa::canCreateControlPartForBorderOnlyForVectorBasedControls(renderer);
#endif

    auto appearance = renderer.style().usedAppearance();
    return appearance == StyleAppearance::Listbox
        || appearance == StyleAppearance::TextArea
        || appearance == StyleAppearance::TextField;
}

bool RenderThemeMac::canCreateControlPartForDecorations(const RenderElement& renderer) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (renderer.settings().formControlRefreshEnabled())
        return RenderThemeCocoa::canCreateControlPartForDecorationsForVectorBasedControls(renderer);
#endif

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

static bool supportsLargeFormControls()
{
    static bool hasSupport = [[NSAppearance currentDrawingAppearance] _usesMetricsAppearance];
    return hasSupport;
}

bool RenderThemeMac::supportsLargeFormControls() const
{
    return WebCore::supportsLargeFormControls();
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

Color RenderThemeMac::platformAnnotationHighlightBackgroundColor(OptionSet<StyleColorOptions>) const
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

#if !ENABLE(FORM_CONTROL_REFRESH)

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

#endif

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
                RetainPtr systemColor = wtfObjCMsgSend<NSColor *>([NSColor class], selector);
                color = semanticColorFromNSColor(systemColor.get());
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
            case CSSValueAppleSystemQuinaryLabel:
                return @selector(quinaryLabelColor);
#if HAVE(NSCOLOR_FILL_COLOR_HIERARCHY)
            case CSSValueAppleSystemOpaqueFill:
                return @selector(systemFillColor);
            case CSSValueAppleSystemOpaqueSecondaryFill:
                return @selector(secondarySystemFillColor);
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
            if (RetainPtr color = wtfObjCMsgSend<NSColor *>([NSColor class], selector))
                return semanticColorFromNSColor(color.get());
        }

        auto textColorForActiveButton = [&] {
#if ENABLE(FORM_CONTROL_REFRESH)
            return buttonTextColor(options, true);
#else
            return activeButtonTextColor();
#endif
        };

        switch (cssValueID) {
        case CSSValueActivebuttontext:
            return textColorForActiveButton();

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
            return semanticColorFromNSColor(retainPtr(alternateColors[0]).get());
        }

        case CSSValueAppleSystemOddAlternatingContentBackground: {
            NSArray<NSColor *> *alternateColors = [NSColor alternatingContentBackgroundColors];
            ASSERT(alternateColors.count >= 2);
            return semanticColorFromNSColor(retainPtr(alternateColors[1]).get());
        }

        // FIXME: Remove this fallback when AppKit without tertiary-fill is not used anymore; see rdar://108340604.
        case CSSValueAppleSystemTertiaryFill:
            if (localAppearance.usingDarkAppearance())
                return { SRGBA<uint8_t> { 255, 255, 255, 12 }, Color::Flags::Semantic };
            return { SRGBA<uint8_t> { 0, 0, 0, 12 }, Color::Flags::Semantic };

        case CSSValueBackground:
            // Use platform-independent value returned by base class.
            [[fallthrough]];

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

bool RenderThemeMac::searchFieldShouldAppearAsTextField(const RenderStyle& style, const Settings& settings) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (settings.formControlRefreshEnabled())
        return false;
#else
    UNUSED_PARAM(settings);
#endif

    return !style.writingMode().isHorizontal();
}

bool RenderThemeMac::isControlStyled(const RenderStyle& style) const
{
    auto appearance = style.usedAppearance();
    if (appearance == StyleAppearance::TextField || appearance == StyleAppearance::TextArea || appearance == StyleAppearance::SearchField || appearance == StyleAppearance::Listbox)
        return style.nativeAppearanceDisabled();

    // FIXME: This is horrible, but there is not much else that can be done. Menu lists cannot draw properly when
    // scaled.  They can't really draw properly when transformed either.  We can't detect the transform case at style
    // adjustment time so that will just have to stay broken.  We can however detect that we're zooming.  If zooming
    // is in effect we treat it like the control is styled. Additionally, treat the control like it is styled when
    // using a vertical writing mode, since the AppKit control is not height resizable.
    if (appearance == StyleAppearance::Menulist && (style.usedZoom() != 1.0f || !style.writingMode().isHorizontal()))
        return true;

    return RenderTheme::isControlStyled(style);
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
    if (supportsLargeFormControls()
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

// Helper functions used by a bunch of different control parts.

static NSControlSize controlSizeForFont(const FontCascade& font)
{
    auto fontSize = font.size();
    if (fontSize >= 21 && supportsLargeFormControls())
        return NSControlSizeLarge;
    if (fontSize >= 16)
        return NSControlSizeRegular;
    if (fontSize >= 11)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

static Style::PreferredSizePair sizeFromNSControlSize(NSControlSize nsControlSize, const Style::PreferredSizePair& zoomedSize, float zoomFactor, const std::span<const IntSize, 4> sizes)
{
    IntSize controlSize = sizes[nsControlSize];
    if (zoomFactor != 1.0f)
        controlSize = IntSize(controlSize.width() * zoomFactor, controlSize.height() * zoomFactor);
    auto resultWidth = zoomedSize.width();
    auto resultHeight = zoomedSize.height();
    if (zoomedSize.width().isIntrinsicOrLegacyIntrinsicOrAuto() && controlSize.width() > 0)
        resultWidth = Style::PreferredSize::Fixed { static_cast<float>(controlSize.width()) };
    if (zoomedSize.height().isIntrinsicOrLegacyIntrinsicOrAuto() && controlSize.height() > 0)
        resultHeight = Style::PreferredSize::Fixed { static_cast<float>(controlSize.height()) };
    return { WTFMove(resultWidth), WTFMove(resultHeight) };
}

static Style::PreferredSizePair sizeFromFont(const FontCascade& font, const Style::PreferredSizePair& zoomedSize, float zoomFactor, const std::span<const IntSize, 4> sizes)
{
    return sizeFromNSControlSize(controlSizeForFont(font), zoomedSize, zoomFactor, sizes);
}

// Popup button

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
    static constexpr std::array sizes {
        IntSize { 0, 21 },
        IntSize { 0, 18 },
        IntSize { 0, 15 },
        IntSize { 0, 24 },
    };
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

// Checkboxes and radio buttons

static const std::span<const IntSize, 4> checkboxSizes()
{
    static constexpr std::array sizes = {
        IntSize { 14, 14 },
        IntSize { 12, 12 },
        IntSize { 10, 10 },
        IntSize { 16, 16 },
    };
    return sizes;
}

static std::span<const int, 4> checkboxMargins(NSControlSize controlSize)
{
    static constexpr std::array margins {
        // top right bottom left
        std::array { 2, 2, 2, 2 },
        std::array { 2, 1, 2, 1 },
        std::array { 0, 0, 1, 0 },
        std::array { 2, 2, 2, 2 },
    };
    return margins[controlSize];
}

static Style::PreferredSizePair checkboxSize(const Style::PreferredSizePair& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width().isIntrinsicOrLegacyIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrLegacyIntrinsicOrAuto())
        return zoomedSize;

    return sizeFromNSControlSize(NSControlSizeSmall, zoomedSize, zoomFactor, checkboxSizes());
}

// Radio Buttons

static const std::span<const IntSize, 4> radioSizes()
{
    static std::array<IntSize, 4> sizes = [] {
        if (supportsLargeFormControls())
            return std::array<IntSize, 4> { { IntSize(14, 14), IntSize(12, 12), IntSize(10, 10), IntSize(16, 16) } };
        return std::array<IntSize, 4> { { IntSize(16, 16), IntSize(12, 12), IntSize(10, 10), IntSize(0, 0) } };
    }();
    return sizes;
}

static std::span<const int, 4> radioMargins(NSControlSize controlSize)
{
    static constexpr std::array margins {
        // top right bottom left
        std::array { 1, 0, 1, 2 },
        std::array { 1, 1, 2, 1 },
        std::array { 0, 0, 1, 1 },
        std::array { 1, 0, 1, 2 },
    };
    return margins[controlSize];
}

static Style::PreferredSizePair radioSize(const Style::PreferredSizePair& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width().isIntrinsicOrLegacyIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrLegacyIntrinsicOrAuto())
        return zoomedSize;

    return sizeFromNSControlSize(NSControlSizeSmall, zoomedSize, zoomFactor, radioSizes());
}

// Buttons

// Buttons really only constrain height. They respect width.
static const std::span<const IntSize, 4> buttonSizes()
{
    static constexpr std::array sizes = {
        IntSize { 0, 20 },
        IntSize { 0, 16 },
        IntSize { 0, 13 },
        IntSize { 0, 28 },
    };
    return sizes;
}

static std::span<const int, 4> buttonMargins(NSControlSize controlSize)
{
    // FIXME: These values may need to be reevaluated. They appear to have been originally chosen
    // to reflect the size of shadows around native form controls on macOS, but as of macOS 10.15,
    // these margins extend well past the boundaries of a native button cell's shadows.
    static constexpr std::array margins {
        std::array { 5, 7, 7, 7 },
        std::array { 4, 6, 7, 6 },
        std::array { 1, 2, 2, 2 },
        std::array { 6, 6, 6, 6 },
    };
    return margins[controlSize];
}

// Stepper

static const std::span<const IntSize, 4> stepperSizes()
{
    static constexpr std::array sizes = {
        IntSize { 19, 27 },
        IntSize { 15, 22 },
        IntSize { 13, 15 },
        IntSize { 19, 27 },
    };
    return sizes;
}

// We don't use controlSizeForFont() for steppers because the stepper height
// should be equal to or less than the corresponding text field height,
static NSControlSize stepperControlSizeForFont(const FontCascade& font)
{
    auto fontSize = font.size();
    if (fontSize >= 23 && supportsLargeFormControls())
        return NSControlSizeLarge;
    if (fontSize >= 18)
        return NSControlSizeRegular;
    if (fontSize >= 13)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

// Switch

static const std::span<const IntSize, 4> switchSizes()
{
    static constexpr std::array sizes = {
        IntSize { 38, 22 },
        IntSize { 32, 18 },
        IntSize { 26, 15 },
        IntSize { 38, 22 }
    };
    return sizes;
}

static std::span<const int, 4> visualSwitchMargins(NSControlSize controlSize, bool isVertical)
{
    static constexpr std::array switchMarginsNonMini { 2, 2, 1, 2 };
    static constexpr std::array switchMarginsMini { 1, 1, 0, 1 };
    static constexpr std::array switchMarginsNonMiniVertical { switchMarginsNonMini[3], switchMarginsNonMini[0], switchMarginsNonMini[1], switchMarginsNonMini[2] };
    static constexpr std::array switchMarginsMiniVertical { switchMarginsMini[3], switchMarginsMini[0], switchMarginsMini[1], switchMarginsMini[2] };

    if (controlSize == NSControlSizeMini)
        return isVertical ? switchMarginsMiniVertical : switchMarginsMini;

    return isVertical ? switchMarginsNonMiniVertical : switchMarginsNonMini;
}

static Style::PreferredSizePair switchSize(const Style::PreferredSizePair& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width().isIntrinsicOrLegacyIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrLegacyIntrinsicOrAuto())
        return zoomedSize;

    return sizeFromNSControlSize(NSControlSizeSmall, zoomedSize, zoomFactor, switchSizes());
}

static void inflateControlPaintRect(StyleAppearance appearance, FloatRect& zoomedRect, float zoomFactor, bool isVertical)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto zoomRectSize = IntSize(zoomedRect.size());
    switch (appearance) {
    case StyleAppearance::Checkbox: {
        auto size = controlSizeFromPixelSize(checkboxSizes(), zoomRectSize, zoomFactor);
        auto zoomedSize = checkboxSizes()[size];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
        zoomedRect = inflateRect(zoomedRect, zoomedSize, checkboxMargins(size), zoomFactor);
        break;
    }
    case StyleAppearance::Radio: {
        auto size = controlSizeFromPixelSize(radioSizes(), zoomRectSize, zoomFactor);
        auto zoomedSize = radioSizes()[size];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
        zoomedRect = inflateRect(zoomedRect, zoomedSize, radioMargins(size), zoomFactor);
        break;
    }
    case StyleAppearance::Switch: {
        auto logicalZoomRectSize = isVertical ? zoomRectSize.transposedSize() : zoomRectSize;
        auto controlSize = controlSizeFromPixelSize(switchSizes(), logicalZoomRectSize, zoomFactor);
        auto zoomedSize = switchSizes()[controlSize];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
        if (isVertical)
            zoomedSize = zoomedSize.transposedSize();
        zoomedRect = inflateRect(zoomedRect, zoomedSize, visualSwitchMargins(controlSize, isVertical), zoomFactor);
        break;
    }
    case StyleAppearance::PushButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button: {
        auto largestControlSize = supportsLargeFormControls() ? NSControlSizeLarge : NSControlSizeRegular;
        if (zoomedRect.height() > buttonSizes()[largestControlSize].height() * zoomFactor)
            break;
        auto size = controlSizeFromPixelSize(buttonSizes(), zoomRectSize, zoomFactor);
        auto zoomedSize = buttonSizes()[size];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedRect.width()); // Buttons don't ever constrain width, so the zoomed width can just be honored.
        zoomedRect = inflateRect(zoomedRect, zoomedSize, buttonMargins(size), zoomFactor);
        break;
    }
    case StyleAppearance::InnerSpinButton: {
        static constexpr std::array stepperMargin = { 0, 0, 0, 0 };
        auto controlSize = controlSizeFromPixelSize(stepperSizes(), zoomRectSize, zoomFactor);
        IntSize zoomedSize = stepperSizes()[controlSize];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
        zoomedRect = inflateRect(zoomedRect, zoomedSize, stepperMargin, zoomFactor);
        break;
    }
    default:
        break;
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

void RenderThemeMac::inflateRectForControlRenderer(const RenderElement& renderer, FloatRect& rect)
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (renderer.settings().formControlRefreshEnabled()) {
        RenderThemeCocoa::inflateRectForControlRenderer(renderer, rect);
        return;
    }
#endif

    auto appearance = renderer.style().usedAppearance();

    switch (appearance) {
    case StyleAppearance::Button:
    case StyleAppearance::Checkbox:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::InnerSpinButton:
    case StyleAppearance::PushButton:
    case StyleAppearance::Radio:
    case StyleAppearance::Switch:
        inflateControlPaintRect(renderer.style().usedAppearance(), rect, renderer.style().usedZoom(), !renderer.writingMode().isHorizontal());
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

bool RenderThemeMac::controlSupportsTints(const RenderElement& renderer) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (renderer.settings().formControlRefreshEnabled())
        return RenderThemeCocoa::controlSupportsTints(renderer);
#endif
    // An alternate way to implement this would be to get the appropriate cell object
    // and call the private _needRedrawOnWindowChangedKeyState method. An advantage of
    // that would be that we would match AppKit behavior more closely, but a disadvantage
    // would be that we would rely on an AppKit SPI method.

    if (!isEnabled(renderer))
        return false;

    // Checkboxes only have tint when checked.
    if (renderer.style().usedAppearance() == StyleAppearance::Checkbox)
        return isChecked(renderer);

    // For now assume other controls have tint if enabled.
    return true;
}

static NSControlSize controlSizeForSystemFont(const RenderStyle& style)
{
    auto fontSize = style.computedFontSize();
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSControlSizeLarge] && supportsLargeFormControls())
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
    if (fontSize >= 21 && supportsLargeFormControls())
        return NSControlSizeLarge;
    if (fontSize >= 16)
        return NSControlSizeRegular;
    if (fontSize >= 11)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

static IntSize sizeForFont(const RenderStyle& style, std::span<const IntSize, 4> sizes)
{
    if (style.usedZoom() != 1.0f && !style.evaluationTimeZoomEnabled()) {
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
    if (style.width().isIntrinsicOrLegacyIntrinsicOrAuto() && size.width() > 0)
        style.setWidth(Style::PreferredSize::Fixed { static_cast<float>(size.width()) });
    if (style.height().isAuto() && size.height() > 0)
        style.setHeight(Style::PreferredSize::Fixed { static_cast<float>(size.height()) });
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

void RenderThemeMac::adjustListButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (element && element->document().settings().formControlRefreshEnabled()) {
        RenderThemeCocoa::adjustListButtonStyle(style, element);
        return;
    }
#else
    UNUSED_PARAM(element);
#endif

    // Add a margin to place the button at end of the input field.
    style.setMarginEnd(-4_css_px / style.usedZoomForLength().value);
}

#if ENABLE(SERVICE_CONTROLS)
void RenderThemeMac::adjustImageControlsButtonStyle(RenderStyle& style, const Element*) const
{
    style.setHeight(Style::PreferredSize::Fixed { static_cast<float>(imageControlsButtonSize().height()) });
    style.setWidth(Style::PreferredSize::Fixed { static_cast<float>(imageControlsButtonSize().width()) });
}
#endif

FloatSize RenderThemeMac::meterSizeForBounds(const RenderMeter& renderMeter, const FloatRect& bounds) const
{
    RefPtr control = const_cast<RenderMeter&>(renderMeter).ensureControlPartForRenderer();
    if (!control)
        return bounds.size();

    auto controlStyle = extractControlStyleForRenderer(renderMeter);
    return control->sizeForBounds(bounds, controlStyle);
}

bool RenderThemeMac::supportsMeter(StyleAppearance appearance) const
{
    return appearance == StyleAppearance::Meter;
}

void RenderThemeMac::createColorWellSwatchSubtree(HTMLElement& swatch)
{
    Ref document = swatch.document();
    Ref div = HTMLDivElement::create(document);
    swatch.appendChild(ContainerNode::ChildChange::Source::Parser, div);
    div->setUserAgentPart(UserAgentParts::internalColorSwatchOverlay());
    div->setInlineStyleProperty(CSSPropertyHeight, "100%"_s);
    div->setInlineStyleProperty(CSSPropertyWidth, "100%"_s);
    div->setInlineStyleProperty(CSSPropertyClipPath, "polygon(0 0, 100% 0, 0 100%)"_s);
}

void RenderThemeMac::setColorWellSwatchBackground(HTMLElement& swatch, Color color)
{
    Ref swatchChild = *downcast<HTMLElement>(swatch.protectedFirstChild());

    auto backgroundColor = color.isOpaque() ? color : blendSourceOver(Color::white, color);
    auto foregroundColor = color.isOpaque() ? Color::transparentBlack : blendSourceOver(Color::black, color);

    swatch.setInlineStyleProperty(CSSPropertyBackgroundColor, serializationForHTML(backgroundColor));
    swatchChild->setInlineStyleProperty(CSSPropertyBackgroundColor, serializationForHTML(foregroundColor));
}

IntRect RenderThemeMac::progressBarRectForBounds(const RenderProgress& renderProgress, const IntRect& bounds) const
{
    RefPtr control = const_cast<RenderProgress&>(renderProgress).ensureControlPartForRenderer();
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

void RenderThemeMac::adjustMenuListStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (element && element->document().settings().formControlRefreshEnabled()) {
        RenderThemeCocoa::adjustMenuListStyle(style, element);
        return;
    }
#endif

    RenderTheme::adjustMenuListStyle(style, element);

    NSControlSize controlSize = controlSizeForFont(style);

    style.resetBorder();
    style.resetPadding();

    // Height is locked to auto.
    style.setHeight(CSS::Keyword::Auto { });

    // White-space is locked to pre
    style.setWhiteSpaceCollapse(WhiteSpaceCollapse::Preserve);
    style.setTextWrapMode(TextWrapMode::NoWrap);

    // Set the button's vertical size.
    setSizeFromFont(style, menuListButtonSizes());

    // Our font is locked to the appropriate system font size for the control.  To clarify, we first use the CSS-specified font to figure out
    // a reasonable control size, but once that control size is determined, we throw that font away and use the appropriate
    // system font for the control size instead.
    setFontFromControlSize(style, controlSize);

    style.setBoxShadow(CSS::Keyword::None { });
}

static Style::PaddingEdge toTruncatedPaddingEdge(auto value)
{
    return Style::PaddingEdge::Fixed { static_cast<float>(std::trunc(value)) };
}

Style::PaddingBox RenderThemeMac::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.usedAppearance() == StyleAppearance::Menulist) {
        auto padding = popupButtonPadding(controlSizeForFont(style), style.writingMode().isBidiRTL());
        return {
            toTruncatedPaddingEdge(padding[topPadding]),
            toTruncatedPaddingEdge(padding[rightPadding]),
            toTruncatedPaddingEdge(padding[bottomPadding]),
            toTruncatedPaddingEdge(padding[leftPadding]),
        };
    }

    if (style.usedAppearance() == StyleAppearance::MenulistButton) {
        float arrowWidth = baseArrowWidth * (style.computedFontSize() / baseFontSize);
        float rightPadding = ceilf(arrowWidth + (arrowPaddingBefore + arrowPaddingAfter + paddingBeforeSeparator) * style.usedZoom());
        float leftPadding = styledPopupPaddingLeft;
        if (style.writingMode().isBidiRTL())
            std::swap(rightPadding, leftPadding);

        return {
            toTruncatedPaddingEdge(styledPopupPaddingTop),
            toTruncatedPaddingEdge(rightPadding / style.usedZoom()),
            toTruncatedPaddingEdge(styledPopupPaddingBottom),
            toTruncatedPaddingEdge(leftPadding),
        };
    }

    return { 0_css_px };
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
        return supportsLargeFormControls() ? PopupMenuStyle::Size::Large : PopupMenuStyle::Size::Normal;
    default:
        return PopupMenuStyle::Size::Normal;
    }
}

void RenderThemeMac::adjustMenuListButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (element && element->document().settings().formControlRefreshEnabled())
        RenderThemeCocoa::adjustMenuListButtonStyle(style, element);
#else
    UNUSED_PARAM(element);
#endif
    float fontScale = style.computedFontSize() / baseFontSize;

    style.resetPadding();

    auto radius = Style::LengthPercentage<CSS::Nonnegative>::Dimension { std::trunc(baseBorderRadius + fontScale - 1) }; // FIXME: Round up?
    style.setBorderRadius({ radius, radius });

    style.setMinHeight(18_css_px);

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

void RenderThemeMac::adjustSliderTrackStyle(RenderStyle& style, const Element* element) const
{
    RenderThemeCocoa::adjustSliderTrackStyle(style, element);
    style.setBoxShadow(CSS::Keyword::None { });
}

void RenderThemeMac::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
    RenderThemeCocoa::adjustSliderThumbStyle(style, element);
    style.setBoxShadow(CSS::Keyword::None { });
}

std::span<const IntSize, 4> RenderThemeMac::searchFieldSizes() const
{
    static constexpr std::array sizes { IntSize(0, 22), IntSize(0, 19), IntSize(0, 17), IntSize(0, 30) };
    return sizes;
}

void RenderThemeMac::setSearchFieldSize(RenderStyle& style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style.width().isIntrinsicOrLegacyIntrinsicOrAuto() && !style.height().isAuto())
        return;

    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style, searchFieldSizes());
}

void RenderThemeMac::adjustSearchFieldStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (element && element->document().settings().formControlRefreshEnabled()) {
        RenderThemeCocoa::adjustSearchFieldStyle(style, element);
        return;
    }
#else
    UNUSED_PARAM(element);
#endif

    // Override border.
    style.resetBorder();
    auto borderWidth = 2_css_px * style.usedZoom();
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
    style.setHeight(CSS::Keyword::Auto { });
    setSearchFieldSize(style);

    // Override padding size to match AppKit text positioning.
    style.setPaddingBox({ toTruncatedPaddingEdge(1 * style.usedZoom()) });

    style.setBoxShadow(CSS::Keyword::None { });
}

std::span<const IntSize, 4> RenderThemeMac::cancelButtonSizes() const
{
    static constexpr std::array sizes { IntSize(22, 22), IntSize(19, 19), IntSize(15, 15), IntSize(22, 22) };
    return sizes;
}

void RenderThemeMac::adjustSearchFieldCancelButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (element && element->document().settings().formControlRefreshEnabled()) {
        RenderThemeCocoa::adjustSearchFieldCancelButtonStyle(style, element);
        return;
    }
#else
    UNUSED_PARAM(element);
#endif

    IntSize size = sizeForSystemFont(style, cancelButtonSizes());
    style.setWidth(Style::PreferredSize::Fixed { static_cast<float>(size.width()) });
    style.setHeight(Style::PreferredSize::Fixed { static_cast<float>(size.height()) });
    style.setBoxShadow(CSS::Keyword::None { });
}

constexpr int resultsArrowWidth = 5;
std::span<const IntSize, 4> RenderThemeMac::resultsButtonSizes() const
{
    static constexpr std::array sizes { IntSize(19, 22), IntSize(17, 19), IntSize(17, 15), IntSize(19, 22) };
    return sizes;
}

const int emptyResultsOffset = 9;
void RenderThemeMac::adjustSearchFieldDecorationPartStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (element && element->document().settings().formControlRefreshEnabled()) {
        RenderThemeCocoa::adjustSearchFieldDecorationPartStyle(style, element);
        return;
    }
#else
    UNUSED_PARAM(element);
#endif

    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    int widthOffset = 0;
    int heightOffset = 0;
    if (style.writingMode().isHorizontal())
        widthOffset = emptyResultsOffset;
    else
        heightOffset = emptyResultsOffset;
    style.setWidth(Style::PreferredSize::Fixed { static_cast<float>(size.width() - widthOffset) });
    style.setHeight(Style::PreferredSize::Fixed { static_cast<float>(size.height() - heightOffset) });
    style.setBoxShadow(CSS::Keyword::None { });
}

void RenderThemeMac::adjustSearchFieldResultsDecorationPartStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (element && element->document().settings().formControlRefreshEnabled()) {
        RenderThemeCocoa::adjustSearchFieldResultsDecorationPartStyle(style, element);
        return;
    }
#else
    UNUSED_PARAM(element);
#endif

    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style.setWidth(Style::PreferredSize::Fixed { static_cast<float>(size.width()) });
    style.setHeight(Style::PreferredSize::Fixed { static_cast<float>(size.height()) });
    style.setBoxShadow(CSS::Keyword::None { });
}

void RenderThemeMac::adjustSearchFieldResultsButtonStyle(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (element && element->document().settings().formControlRefreshEnabled()) {
        RenderThemeCocoa::adjustSearchFieldResultsButtonStyle(style, element);
        return;
    }
#else
    UNUSED_PARAM(element);
#endif

    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style.setWidth(Style::PreferredSize::Fixed { static_cast<float>(size.width() + resultsArrowWidth) });
    style.setHeight(Style::PreferredSize::Fixed { static_cast<float>(size.height()) });
    style.setBoxShadow(CSS::Keyword::None { });
}

IntSize RenderThemeMac::sliderTickSize() const
{
    return IntSize(1, 3);
}

int RenderThemeMac::sliderTickOffsetFromTrackCenter() const
{
    return -9;
}

// FIXME (<rdar://problem/80870479>): Ideally, this constant should be obtained from AppKit using -[NSSliderCell knobThickness].
// However, the method currently returns an incorrect value, both with and without a control view associated with the cell.
constexpr int sliderThumbThickness = 17;

void RenderThemeMac::adjustSliderThumbSize(RenderStyle& style, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (element && element->document().settings().formControlRefreshEnabled()) {
        RenderThemeCocoa::adjustSliderThumbSize(style, element);
        return;
    }
#else
    UNUSED_PARAM(element);
#endif

    float zoomLevel = style.usedZoom();
    if (style.usedAppearance() == StyleAppearance::SliderThumbHorizontal || style.usedAppearance() == StyleAppearance::SliderThumbVertical) {
        style.setWidth(Style::PreferredSize::Fixed { static_cast<float>(static_cast<int>(sliderThumbThickness * zoomLevel)) });
        style.setHeight(Style::PreferredSize::Fixed { static_cast<float>(static_cast<int>(sliderThumbThickness * zoomLevel)) });
    }
}

std::optional<FontCascadeDescription> RenderThemeMac::controlFont(StyleAppearance appearance, const FontCascade& font, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::PushButton: {
        FontCascadeDescription fontDescription;
        fontDescription.setIsAbsoluteSize(true);

        NSFont* nsFont = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSizeForFont(font)]];
        fontDescription.setOneFamily("-apple-system"_s);
        fontDescription.setComputedSize([nsFont pointSize] * zoomFactor);
        fontDescription.setSpecifiedSize([nsFont pointSize] * zoomFactor);
        return fontDescription;
    }
    default:
        return std::nullopt;
    }
}

Style::PaddingBox RenderThemeMac::controlPadding(StyleAppearance appearance, const Style::PaddingBox& padding, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::PushButton: {
        // Just use 8px. AppKit wants to use 11px for mini buttons, but that padding is just too large
        // for real-world Web sites (creating a huge necessary minimum width for buttons whose space is
        // by definition constrained, since we select mini only for small cramped environments).
        // This also guarantees the HTML <button> will match our rendering by default, since we're using
        // a consistent padding.
        auto edge = toTruncatedPaddingEdge(8 * zoomFactor);
        return { 2_css_px, edge, 3_css_px, edge };
    }
    default:
        return RenderTheme::controlPadding(appearance, padding, zoomFactor);
    }
}

Style::PreferredSizePair RenderThemeMac::controlSize(StyleAppearance appearance, const FontCascade& font, const Style::PreferredSizePair& zoomedSize, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::Checkbox:
        return checkboxSize(zoomedSize, zoomFactor);
    case StyleAppearance::Radio:
        return radioSize(zoomedSize, zoomFactor);
    case StyleAppearance::Switch:
        return switchSize(zoomedSize, zoomFactor);
    case StyleAppearance::PushButton:
        // Height is reset to auto so that specified heights can be ignored.
        return sizeFromFont(font, { zoomedSize.width(), CSS::Keyword::Auto { } }, zoomFactor, buttonSizes());
    case StyleAppearance::InnerSpinButton:
        if (!zoomedSize.width().isIntrinsicOrLegacyIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrLegacyIntrinsicOrAuto())
            return zoomedSize;
        return sizeFromNSControlSize(stepperControlSizeForFont(font), zoomedSize, zoomFactor, stepperSizes());
    default:
        return zoomedSize;
    }
}

Style::MinimumSizePair RenderThemeMac::minimumControlSize(StyleAppearance appearance, const FontCascade& font, const Style::MinimumSizePair& zoomedSize, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::SquareButton:
    case StyleAppearance::ColorWell:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
        return {
            0_css_px,
            Style::MinimumSize::Fixed { static_cast<float>(static_cast<int>(15 * zoomFactor)) },
        };
    case StyleAppearance::InnerSpinButton: {
        auto& base = stepperSizes()[NSControlSizeMini];
        return {
            Style::MinimumSize::Fixed { static_cast<float>(static_cast<int>(base.width() * zoomFactor)) },
            Style::MinimumSize::Fixed { static_cast<float>(static_cast<int>(base.height() * zoomFactor)) },
        };
    }
    default:
        return RenderTheme::minimumControlSize(appearance, font, zoomedSize, zoomFactor);
    }
}

Style::LineWidthBox RenderThemeMac::controlBorder(StyleAppearance appearance, const FontCascade& font, const Style::LineWidthBox& zoomedBox, float zoomFactor, const Element* element) const
{
#if ENABLE(FORM_CONTROL_REFRESH)
    if (element && element->document().settings().formControlRefreshEnabled())
        return RenderThemeCocoa::controlBorder(appearance, font, zoomedBox, zoomFactor, element);
#endif

    switch (appearance) {
    case StyleAppearance::SquareButton:
    case StyleAppearance::ColorWell:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
        return Style::LineWidthBox { 0_css_px, zoomedBox.right(), 0_css_px, zoomedBox.left() };
    default:
        return RenderTheme::controlBorder(appearance, font, zoomedBox, zoomFactor, element);
    }
}

bool RenderThemeMac::controlRequiresPreWhiteSpace(StyleAppearance appearance) const
{
    return appearance == StyleAppearance::PushButton;
}

String RenderThemeMac::fileListNameForWidth(const FileList* fileList, const FontCascade& font, int width, bool multipleFilesAllowed) const
{
    if (width <= 0)
        return String();

    String strToTruncate;
    if (fileList->isEmpty())
        strToTruncate = fileListDefaultLabel(multipleFilesAllowed);
    else if (fileList->length() == 1) {
        RefPtr file = fileList->item(0);
        auto path = file->path();
        strToTruncate = path.isEmpty() ? file->name() : [[NSFileManager defaultManager] displayNameAtPath:path.createNSString().get()];
    } else
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
            if (auto icon = Icon::createIconForUTI(UTTypeFolder.identifier)) {
                LOG_ATTACHMENT("-> Got icon for UTTypeFolder");
                return icon;
            }
            LOG_ATTACHMENT("-> No icon for UTTypeFolder! Will fallback to filename or title...");
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

    RetainPtr nsTitle = title.createNSString();
    if (RetainPtr<NSString> fileExtension = nsTitle.get().pathExtension; fileExtension.get().length) {
        if (auto icon = Icon::createIconForFileExtension(fileExtension.get())) {
            LOG_ATTACHMENT("-> Got icon for title file extension '%s'", String(fileExtension.get()).utf8().data());
            return icon;
        }
        LOG_ATTACHMENT("-> No icon for title file extension '%s'! Will fallback to public.data icon", String(fileExtension.get()).utf8().data());
    } else
        LOG_ATTACHMENT("-> No file extension in title! Will fallback to public.data icon");

    return Icon::createIconForUTI("public.data"_s);
#undef LOG_ATTACHMENT
}

RenderThemeCocoa::IconAndSize RenderThemeMac::iconForAttachment(const String& fileName, const String& attachmentType, const String& title)
{
    if (fileName.isNull() && attachmentType.isNull() && title.isNull())
        return IconAndSize { nil, FloatSize() };

    if (auto icon = WebCore::iconForAttachment(fileName, attachmentType, title)) {
        RetainPtr image = icon->image();
        auto size = [image size];
        return IconAndSize { WTFMove(image), FloatSize(size) };
    }

    return IconAndSize { nil, FloatSize() };
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

    CheckedRef style = attachment.style();
    Color backgroundColor = style->colorByApplyingColorFilter(attachmentIconBackgroundColor);
    context.fillRoundedRect(FloatRoundedRect(backgroundRect, FloatRoundedRect::Radii(attachmentIconBackgroundRadius)), backgroundColor);

    if (paintBorder) {
        FloatRect borderRect = layout.iconBackgroundRect;
        borderRect.inflate(-attachmentIconSelectionBorderThickness / 2);

        FloatSize iconBackgroundRadiusSize(attachmentIconBackgroundRadius, attachmentIconBackgroundRadius);
        Path borderPath;
        borderPath.addRoundedRect(borderRect, iconBackgroundRadiusSize);

        Color borderColor = style->colorByApplyingColorFilter(attachmentIconBorderColor);
        context.setStrokeColor(borderColor);
        context.setStrokeThickness(attachmentIconSelectionBorderThickness);
        context.strokePath(borderPath);
    }
}

static void paintAttachmentIcon(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout)
{
    if (context.paintingDisabled())
        return;

    Ref attachmentElement = attachment.attachmentElement();
    attachmentElement->requestIconIfNeededWithSize(layout.iconRect.size());
    auto icon = attachmentElement->icon();
    if (!icon)
        return;

    context.drawImage(*icon, layout.iconRect);
}

static std::pair<RefPtr<Image>, float> createAttachmentPlaceholderImage(float deviceScaleFactor, const AttachmentLayout& layout)
{
    RetainPtr configuration = [NSImageSymbolConfiguration configurationWithPointSize:32 weight:NSFontWeightRegular scale:NSImageSymbolScaleMedium];
    RetainPtr image = [[NSImage imageWithSystemSymbolName:@"arrow.down.circle" accessibilityDescription:nil] imageWithSymbolConfiguration:configuration.get()];
    auto imageSize = FloatSize([image size]);
    auto imageSizeScales = deviceScaleFactor * layout.iconRect.size() / imageSize;
    imageSize.scale(std::min(imageSizeScales.width(), imageSizeScales.height()));
    auto imageRect = NSMakeRect(0, 0, imageSize.width(), imageSize.height());
    RetainPtr cgImage = [image CGImageForProposedRect:&imageRect context:nil hints:nil];
    return { BitmapImage::create(cgImage.get()), deviceScaleFactor };
}

static void paintAttachmentIconPlaceholder(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout)
{
    auto [placeholderImage, imageScale] = createAttachmentPlaceholderImage(attachment.protectedDocument()->deviceScaleFactor(), layout);

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
    if (attachment.frame().checkedSelection()->isFocusedAndActive())
        backgroundColor = colorFromCocoaColor([NSColor selectedContentBackgroundColor]);
    else
        backgroundColor = attachmentTitleInactiveBackgroundColor;

    backgroundColor = attachment.checkedStyle()->colorByApplyingColorFilter(backgroundColor);
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
        progressRect = encloseRectToDevicePixels(progressRect, attachment.protectedDocument()->deviceScaleFactor());

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

    Color placeholderBorderColor = attachment.checkedStyle()->colorByApplyingColorFilter(attachmentPlaceholderBorderColor);
    context.setStrokeColor(placeholderBorderColor);
    context.setStrokeThickness(attachmentPlaceholderBorderWidth);
    context.setStrokeStyle(StrokeStyle::DashedStroke);
    context.setLineDash({attachmentPlaceholderBorderDashLength}, 0);
    context.strokePath(borderPath);
}

bool RenderThemeMac::paintAttachment(const RenderElement& renderer, const PaintInfo& paintInfo, const IntRect& paintRect)
{
    auto* attachment = dynamicDowncast<RenderAttachment>(renderer);
    if (!attachment)
        return false;

    if (attachment->paintWideLayoutAttachmentOnly(paintInfo, paintRect.location()))
        return true;

    Ref element = attachment->attachmentElement();

    auto layoutStyle = AttachmentLayoutStyle::NonSelected;
    if (attachment->selectionState() != RenderObject::HighlightState::None && paintInfo.phase != PaintPhase::Selection)
        layoutStyle = AttachmentLayoutStyle::Selected;

    AttachmentLayout layout(*attachment, layoutStyle);

    auto& progressString = element->attributeWithoutSynchronization(progressAttr);
    bool validProgress = false;
    float progress = 0;
    if (!progressString.isEmpty())
        progress = progressString.toFloat(&validProgress);

    GraphicsContext& context = paintInfo.context();
    GraphicsContextStateSaver saver(context);

    context.translate(toFloatSize(paintRect.location()));
    context.translate(floorSizeToDevicePixels({ LayoutUnit((paintRect.width() - attachmentIconBackgroundSize) / 2), 0 }, renderer.protectedDocument()->deviceScaleFactor()));

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
