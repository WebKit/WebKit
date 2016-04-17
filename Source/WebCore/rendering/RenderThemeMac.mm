/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#if !PLATFORM(IOS)

#import "RenderThemeMac.h"

#import "BitmapImage.h"
#import "CSSValueKeywords.h"
#import "CSSValueList.h"
#import "ColorMac.h"
#import "CoreGraphicsSPI.h"
#import "Document.h"
#import "Element.h"
#import "ExceptionCodePlaceholder.h"
#import "FileList.h"
#import "FloatRoundedRect.h"
#import "FocusController.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "FrameView.h"
#import "GraphicsContextCG.h"
#import "HTMLAttachmentElement.h"
#import "HTMLAudioElement.h"
#import "HTMLInputElement.h"
#import "HTMLMediaElement.h"
#import "HTMLNames.h"
#import "HTMLPlugInImageElement.h"
#import "Icon.h"
#import "Image.h"
#import "ImageBuffer.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalizedStrings.h"
#import "MediaControlElements.h"
#import "NSColorSPI.h"
#import "NSSharingServicePickerSPI.h"
#import "Page.h"
#import "PaintInfo.h"
#import "PathUtilities.h"
#import "RenderAttachment.h"
#import "RenderLayer.h"
#import "RenderMedia.h"
#import "RenderMediaControlElements.h"
#import "RenderMediaControls.h"
#import "RenderProgress.h"
#import "RenderSlider.h"
#import "RenderSnapshottedPlugIn.h"
#import "RenderView.h"
#import "SharedBuffer.h"
#import "StringTruncator.h"
#import "StyleResolver.h"
#import "ThemeMac.h"
#import "TimeRanges.h"
#import "UserAgentScripts.h"
#import "UserAgentStyleSheets.h"
#import "WebCoreSystemInterface.h"
#import <wtf/RetainPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/StringBuilder.h>
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <math.h>

#if ENABLE(METER_ELEMENT)
#import "RenderMeter.h"
#import "HTMLMeterElement.h"
#endif

#if defined(__LP64__) && __LP64__
#define HAVE_APPKIT_SERVICE_CONTROLS_SUPPORT 1
#else
#define HAVE_APPKIT_SERVICE_CONTROLS_SUPPORT 0
#endif

#if ENABLE(SERVICE_CONTROLS) && HAVE(APPKIT_SERVICE_CONTROLS_SUPPORT)

#if USE(APPLE_INTERNAL_SDK)
#import <AppKit/AppKitDefines_Private.h>
#import <AppKit/NSServicesRolloverButtonCell.h>
#else
#define APPKIT_PRIVATE_CLASS
@interface NSServicesRolloverButtonCell : NSButtonCell
@end
#endif

@interface NSServicesRolloverButtonCell ()
+ (NSServicesRolloverButtonCell *)serviceRolloverButtonCellForStyle:(NSSharingServicePickerStyle)style;
- (NSRect)rectForBounds:(NSRect)bounds preferredEdge:(NSRectEdge)preferredEdge;
@end

#endif // ENABLE(SERVICE_CONTROLS)

// The methods in this file are specific to the Mac OS X platform.

// We estimate the animation rate of a Mac OS X progress bar is 33 fps.
// Hard code the value here because we haven't found API for it.
const double progressAnimationFrameRate = 0.033;

// Mac OS X progress bar animation seems to have 256 frames.
const double progressAnimationNumFrames = 256;

@interface WebCoreRenderThemeNotificationObserver : NSObject
{
    WebCore::RenderTheme *_theme;
}

- (id)initWithTheme:(WebCore::RenderTheme *)theme;
- (void)systemColorsDidChange:(NSNotification *)notification;

@end

@implementation WebCoreRenderThemeNotificationObserver

- (id)initWithTheme:(WebCore::RenderTheme *)theme
{
    if (!(self = [super init]))
        return nil;

    _theme = theme;
    return self;
}

- (void)systemColorsDidChange:(NSNotification *)unusedNotification
{
    ASSERT_UNUSED(unusedNotification, [[unusedNotification name] isEqualToString:NSSystemColorsDidChangeNotification]);
    _theme->platformColorsDidChange();
}

@end

@interface NSTextFieldCell (WKDetails)
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus;
@end


@interface WebCoreTextFieldCell : NSTextFieldCell
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus;
@end

@implementation WebCoreTextFieldCell
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus
{
    // FIXME: This is a post-Lion-only workaround for <rdar://problem/11385461>. When that bug is resolved, we should remove this code.
    CFMutableDictionaryRef coreUIDrawOptions = CFDictionaryCreateMutableCopy(NULL, 0, [super _coreUIDrawOptionsWithFrame:cellFrame inView:controlView includeFocus:includeFocus]);
    CFDictionarySetValue(coreUIDrawOptions, @"borders only", kCFBooleanTrue);
    return (CFDictionaryRef)[NSMakeCollectable(coreUIDrawOptions) autorelease];
}
@end

@interface WebCoreRenderThemeBundle : NSObject
@end

@implementation WebCoreRenderThemeBundle
@end

@interface NSSearchFieldCell()
@property (getter=isCenteredLook) BOOL centeredLook;
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

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page*)
{
    static RenderTheme& rt = RenderThemeMac::create().leakRef();
    return &rt;
}

Ref<RenderTheme> RenderThemeMac::create()
{
    return adoptRef(*new RenderThemeMac);
}

RenderThemeMac::RenderThemeMac()
    : m_isSliderThumbHorizontalPressed(false)
    , m_isSliderThumbVerticalPressed(false)
    , m_notificationObserver(adoptNS([[WebCoreRenderThemeNotificationObserver alloc] initWithTheme:this]))
{
    [[NSNotificationCenter defaultCenter] addObserver:m_notificationObserver.get()
                                                        selector:@selector(systemColorsDidChange:)
                                                            name:NSSystemColorsDidChangeNotification
                                                          object:nil];
}

RenderThemeMac::~RenderThemeMac()
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_notificationObserver.get()];
}

NSView* RenderThemeMac::documentViewFor(const RenderObject& o) const
{
    ControlStates states(extractControlStatesForRenderer(o));
    return ThemeMac::ensuredView(&o.view().frameView(), states);
}

#if ENABLE(VIDEO)
String RenderThemeMac::mediaControlsStyleSheet()
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (m_mediaControlsStyleSheet.isEmpty())
        m_mediaControlsStyleSheet = [NSString stringWithContentsOfFile:[[NSBundle bundleForClass:[WebCoreRenderThemeBundle class]] pathForResource:@"mediaControlsApple" ofType:@"css"] encoding:NSUTF8StringEncoding error:nil];
    return m_mediaControlsStyleSheet;
#else
    return emptyString();
#endif
}

String RenderThemeMac::mediaControlsScript()
{
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    if (m_mediaControlsScript.isEmpty()) {
        StringBuilder scriptBuilder;
        scriptBuilder.append([NSString stringWithContentsOfFile:[[NSBundle bundleForClass:[WebCoreRenderThemeBundle class]] pathForResource:@"mediaControlsLocalizedStrings" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil]);
        scriptBuilder.append([NSString stringWithContentsOfFile:[[NSBundle bundleForClass:[WebCoreRenderThemeBundle class]] pathForResource:@"mediaControlsApple" ofType:@"js"] encoding:NSUTF8StringEncoding error:nil]);
        m_mediaControlsScript = scriptBuilder.toString();
    }
    return m_mediaControlsScript;
#else
    return emptyString();
#endif
}

#endif // ENABLE(VIDEO)


#if ENABLE(SERVICE_CONTROLS)
String RenderThemeMac::imageControlsStyleSheet() const
{
    return String(imageControlsMacUserAgentStyleSheet, sizeof(imageControlsMacUserAgentStyleSheet));
}
#endif

Color RenderThemeMac::platformActiveSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor selectedTextBackgroundColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color(static_cast<int>(255.0 * [color redComponent]), static_cast<int>(255.0 * [color greenComponent]), static_cast<int>(255.0 * [color blueComponent]));
}

Color RenderThemeMac::platformInactiveSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor secondarySelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color(static_cast<int>(255.0 * [color redComponent]), static_cast<int>(255.0 * [color greenComponent]), static_cast<int>(255.0 * [color blueComponent]));
}

Color RenderThemeMac::platformActiveListBoxSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor alternateSelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color(static_cast<int>(255.0 * [color redComponent]), static_cast<int>(255.0 * [color greenComponent]), static_cast<int>(255.0 * [color blueComponent]));
}

Color RenderThemeMac::platformActiveListBoxSelectionForegroundColor() const
{
    return Color::white;
}

Color RenderThemeMac::platformInactiveListBoxSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeMac::platformFocusRingColor() const
{
    if (usesTestModeFocusRingColor())
        return oldAquaFocusRingColor();

    return systemColor(CSSValueWebkitFocusRingColor);
}

Color RenderThemeMac::platformInactiveListBoxSelectionBackgroundColor() const
{
    return platformInactiveSelectionBackgroundColor();
}

static FontWeight toFontWeight(NSInteger appKitFontWeight)
{
    ASSERT(appKitFontWeight > 0 && appKitFontWeight < 15);
    if (appKitFontWeight > 14)
        appKitFontWeight = 14;
    else if (appKitFontWeight < 1)
        appKitFontWeight = 1;

    static const FontWeight fontWeights[] = {
        FontWeight100,
        FontWeight100,
        FontWeight200,
        FontWeight300,
        FontWeight400,
        FontWeight500,
        FontWeight600,
        FontWeight600,
        FontWeight700,
        FontWeight800,
        FontWeight800,
        FontWeight900,
        FontWeight900,
        FontWeight900
    };
    return fontWeights[appKitFontWeight - 1];
}

void RenderThemeMac::updateCachedSystemFontDescription(CSSValueID cssValueId, FontCascadeDescription& fontDescription) const
{
    NSFont* font;
    // System-font-ness can't be encapsulated by simply a font name. Instead, we must use a token
    // which FontCache will look for.
    // Make sure we keep this list of possible tokens in sync with FontCascade::primaryFontIsSystemFont()
    AtomicString fontName;
    switch (cssValueId) {
        case CSSValueSmallCaption:
            font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
            break;
        case CSSValueMenu:
            font = [NSFont menuFontOfSize:[NSFont systemFontSize]];
            fontName = AtomicString("-apple-menu", AtomicString::ConstructFromLiteral);
            break;
        case CSSValueStatusBar:
            font = [NSFont labelFontOfSize:[NSFont labelFontSize]];
            fontName = AtomicString("-apple-status-bar", AtomicString::ConstructFromLiteral);
            break;
        case CSSValueWebkitMiniControl:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSMiniControlSize]];
#pragma clang diagnostic pop
            break;
        case CSSValueWebkitSmallControl:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]];
#pragma clang diagnostic pop
            break;
        case CSSValueWebkitControl:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSRegularControlSize]];
#pragma clang diagnostic pop
            break;
        default:
            font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    }

    if (!font)
        return;

    if (fontName.isNull())
        fontName = AtomicString("-apple-system", AtomicString::ConstructFromLiteral);

    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setOneFamily(fontName);
    fontDescription.setSpecifiedSize([font pointSize]);
    fontDescription.setWeight(toFontWeight([fontManager weightOfFont:font]));
    fontDescription.setIsItalic([fontManager traitsOfFont:font] & NSItalicFontMask);
}

static RGBA32 convertNSColorToColor(NSColor *color)
{
    NSColor *colorInColorSpace = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    if (colorInColorSpace) {
        static const double scaleFactor = nextafter(256.0, 0.0);
        return makeRGB(static_cast<int>(scaleFactor * [colorInColorSpace redComponent]),
            static_cast<int>(scaleFactor * [colorInColorSpace greenComponent]),
            static_cast<int>(scaleFactor * [colorInColorSpace blueComponent]));
    }

    // This conversion above can fail if the NSColor in question is an NSPatternColor
    // (as many system colors are). These colors are actually a repeating pattern
    // not just a solid color. To work around this we simply draw a 1x1 image of
    // the color and use that pixel's color. It might be better to use an average of
    // the colors in the pattern instead.
    NSBitmapImageRep *offscreenRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil
                                                                             pixelsWide:1
                                                                             pixelsHigh:1
                                                                          bitsPerSample:8
                                                                        samplesPerPixel:4
                                                                               hasAlpha:YES
                                                                               isPlanar:NO
                                                                         colorSpaceName:NSDeviceRGBColorSpace
                                                                            bytesPerRow:4
                                                                           bitsPerPixel:32];

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:offscreenRep]];
    NSEraseRect(NSMakeRect(0, 0, 1, 1));
    [color drawSwatchInRect:NSMakeRect(0, 0, 1, 1)];
    [NSGraphicsContext restoreGraphicsState];

    NSUInteger pixel[4];
    [offscreenRep getPixel:pixel atX:0 y:0];

    [offscreenRep release];

    return makeRGB(pixel[0], pixel[1], pixel[2]);
}

static RGBA32 menuBackgroundColor()
{
    NSBitmapImageRep *offscreenRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil
                                                                             pixelsWide:1
                                                                             pixelsHigh:1
                                                                          bitsPerSample:8
                                                                        samplesPerPixel:4
                                                                               hasAlpha:YES
                                                                               isPlanar:NO
                                                                         colorSpaceName:NSDeviceRGBColorSpace
                                                                            bytesPerRow:4
                                                                           bitsPerPixel:32];

    CGContextRef context = static_cast<CGContextRef>([[NSGraphicsContext graphicsContextWithBitmapImageRep:offscreenRep] graphicsPort]);
    CGRect rect = CGRectMake(0, 0, 1, 1);
    HIThemeMenuDrawInfo drawInfo;
    drawInfo.version =  0;
    drawInfo.menuType = kThemeMenuTypePopUp;
    HIThemeDrawMenuBackground(&rect, &drawInfo, context, kHIThemeOrientationInverted);

    NSUInteger pixel[4];
    [offscreenRep getPixel:pixel atX:0 y:0];

    [offscreenRep release];

    return makeRGB(pixel[0], pixel[1], pixel[2]);
}

void RenderThemeMac::platformColorsDidChange()
{
    m_systemColorCache.clear();
    RenderTheme::platformColorsDidChange();
}

Color RenderThemeMac::systemColor(CSSValueID cssValueID) const
{
    auto addResult = m_systemColorCache.add(cssValueID, Color());
    if (!addResult.isNewEntry)
        return addResult.iterator->value;

    Color color;
    switch (cssValueID) {
    case CSSValueActiveborder:
        color = convertNSColorToColor([NSColor keyboardFocusIndicatorColor]);
        break;
    case CSSValueActivebuttontext:
        // There is no corresponding NSColor for this so we use a hard coded value.
        color = Color::white;
        break;
    case CSSValueActivecaption:
        color = convertNSColorToColor([NSColor windowFrameTextColor]);
        break;
    case CSSValueAppworkspace:
        color = convertNSColorToColor([NSColor headerColor]);
        break;
    case CSSValueBackground:
        // Use theme independent default
        break;
    case CSSValueButtonface:
        // We use this value instead of NSColor's controlColor to avoid website incompatibilities.
        // We may want to change this to use the NSColor in future.
        color = 0xFFC0C0C0;
        break;
    case CSSValueButtonhighlight:
        color = convertNSColorToColor([NSColor controlHighlightColor]);
        break;
    case CSSValueButtonshadow:
        color = convertNSColorToColor([NSColor controlShadowColor]);
        break;
    case CSSValueButtontext:
        color = convertNSColorToColor([NSColor controlTextColor]);
        break;
    case CSSValueCaptiontext:
        color = convertNSColorToColor([NSColor textColor]);
        break;
    case CSSValueGraytext:
        color = convertNSColorToColor([NSColor disabledControlTextColor]);
        break;
    case CSSValueHighlight:
        color = convertNSColorToColor([NSColor selectedTextBackgroundColor]);
        break;
    case CSSValueHighlighttext:
        color = convertNSColorToColor([NSColor selectedTextColor]);
        break;
    case CSSValueInactiveborder:
        color = convertNSColorToColor([NSColor controlBackgroundColor]);
        break;
    case CSSValueInactivecaption:
        color = convertNSColorToColor([NSColor controlBackgroundColor]);
        break;
    case CSSValueInactivecaptiontext:
        color = convertNSColorToColor([NSColor textColor]);
        break;
    case CSSValueInfobackground:
        // There is no corresponding NSColor for this so we use a hard coded value.
        color = 0xFFFBFCC5;
        break;
    case CSSValueInfotext:
        color = convertNSColorToColor([NSColor textColor]);
        break;
    case CSSValueMenu:
        color = menuBackgroundColor();
        break;
    case CSSValueMenutext:
        color = convertNSColorToColor([NSColor selectedMenuItemTextColor]);
        break;
    case CSSValueScrollbar:
        color = convertNSColorToColor([NSColor scrollBarColor]);
        break;
    case CSSValueText:
        color = convertNSColorToColor([NSColor textColor]);
        break;
    case CSSValueThreeddarkshadow:
        color = convertNSColorToColor([NSColor controlDarkShadowColor]);
        break;
    case CSSValueThreedshadow:
        color = convertNSColorToColor([NSColor shadowColor]);
        break;
    case CSSValueThreedface:
        // We use this value instead of NSColor's controlColor to avoid website incompatibilities.
        // We may want to change this to use the NSColor in future.
        color = 0xFFC0C0C0;
        break;
    case CSSValueThreedhighlight:
        color = convertNSColorToColor([NSColor highlightColor]);
        break;
    case CSSValueThreedlightshadow:
        color = convertNSColorToColor([NSColor controlLightHighlightColor]);
        break;
    case CSSValueWebkitFocusRingColor:
        color = convertNSColorToColor([NSColor keyboardFocusIndicatorColor]);
        break;
    case CSSValueWindow:
        color = convertNSColorToColor([NSColor windowBackgroundColor]);
        break;
    case CSSValueWindowframe:
        color = convertNSColorToColor([NSColor windowFrameColor]);
        break;
    case CSSValueWindowtext:
        color = convertNSColorToColor([NSColor windowFrameTextColor]);
        break;
    case CSSValueAppleWirelessPlaybackTargetActive:
        color = convertNSColorToColor([NSColor systemBlueColor]);
        break;
    case CSSValueAppleSystemBlue:
        color = convertNSColorToColor([NSColor systemBlueColor]);
        break;
    case CSSValueAppleSystemBrown:
        color = convertNSColorToColor([NSColor systemBrownColor]);
        break;
    case CSSValueAppleSystemGray:
        color = convertNSColorToColor([NSColor systemGrayColor]);
        break;
    case CSSValueAppleSystemGreen:
        color = convertNSColorToColor([NSColor systemGreenColor]);
        break;
    case CSSValueAppleSystemOrange:
        color = convertNSColorToColor([NSColor systemOrangeColor]);
        break;
    case CSSValueAppleSystemPink:
        color = convertNSColorToColor([NSColor systemPinkColor]);
        break;
    case CSSValueAppleSystemPurple:
        color = convertNSColorToColor([NSColor systemPurpleColor]);
        break;
    case CSSValueAppleSystemRed:
        color = convertNSColorToColor([NSColor systemRedColor]);
        break;
    case CSSValueAppleSystemYellow:
        color = convertNSColorToColor([NSColor systemYellowColor]);
        break;
    default:
        break;
    }

    if (!color.isValid())
        color = RenderTheme::systemColor(cssValueID);

    addResult.iterator->value = color;

    return addResult.iterator->value;
}

bool RenderThemeMac::usesTestModeFocusRingColor() const
{
    return WebCore::usesTestModeFocusRingColor();
}

bool RenderThemeMac::isControlStyled(const RenderStyle& style, const BorderData& border,
                                     const FillLayer& background, const Color& backgroundColor) const
{
    if (style.appearance() == TextFieldPart || style.appearance() == TextAreaPart || style.appearance() == ListboxPart)
        return style.border() != border;

    // FIXME: This is horrible, but there is not much else that can be done.  Menu lists cannot draw properly when
    // scaled.  They can't really draw properly when transformed either.  We can't detect the transform case at style
    // adjustment time so that will just have to stay broken.  We can however detect that we're zooming.  If zooming
    // is in effect we treat it like the control is styled.
    if (style.appearance() == MenulistPart && style.effectiveZoom() != 1.0f)
        return true;

    return RenderTheme::isControlStyled(style, border, background, backgroundColor);
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
    bool oldIndeterminate = [cell state] == NSMixedState;
    bool indeterminate = isIndeterminate(o);
    bool checked = isChecked(o);

    if (oldIndeterminate != indeterminate) {
        [cell setState:indeterminate ? NSMixedState : (checked ? NSOnState : NSOffState)];
        return;
    }

    bool oldChecked = [cell state] == NSOnState;
    if (checked != oldChecked)
        [cell setState:checked ? NSOnState : NSOffState];
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
    bool focused = isFocused(o) && o.style().outlineStyleIsAuto();
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
    int fontSize = style.fontSize();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (fontSize >= 16)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
#pragma clang diagnostic pop
}

NSControlSize RenderThemeMac::controlSizeForCell(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel) const
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (minSize.width() >= static_cast<int>(sizes[NSRegularControlSize].width() * zoomLevel)
        && minSize.height() >= static_cast<int>(sizes[NSRegularControlSize].height() * zoomLevel))
        return NSRegularControlSize;

    if (minSize.width() >= static_cast<int>(sizes[NSSmallControlSize].width() * zoomLevel)
        && minSize.height() >= static_cast<int>(sizes[NSSmallControlSize].height() * zoomLevel))
        return NSSmallControlSize;

    return NSMiniControlSize;
#pragma clang diagnostic pop
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

void RenderThemeMac::setFontFromControlSize(StyleResolver&, RenderStyle& style, NSControlSize controlSize) const
{
    FontCascadeDescription fontDescription;
    fontDescription.setIsAbsoluteSize(true);

    NSFont* font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSize]];
    fontDescription.setOneFamily(AtomicString("-apple-system", AtomicString::ConstructFromLiteral));
    fontDescription.setComputedSize([font pointSize] * style.effectiveZoom());
    fontDescription.setSpecifiedSize([font pointSize] * style.effectiveZoom());

    // Reset line height
    style.setLineHeight(RenderStyle::initialLineHeight());

    if (style.setFontDescription(fontDescription))
        style.fontCascade().update(0);
}

NSControlSize RenderThemeMac::controlSizeForSystemFont(const RenderStyle& style) const
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    int fontSize = style.fontSize();
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSRegularControlSize])
        return NSRegularControlSize;
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSSmallControlSize])
        return NSSmallControlSize;
    return NSMiniControlSize;
#pragma clang diagnostic pop
}

bool RenderThemeMac::paintTextField(const RenderObject& o, const PaintInfo& paintInfo, const FloatRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context());

    // <rdar://problem/22896977> We adjust the paint rect here to account for how AppKit draws the text
    // field cell slightly smaller than the rect we pass to drawWithFrame.
    FloatRect adjustedPaintRect(r);
    AffineTransform transform = paintInfo.context().getCTM();
    if (transform.xScale() > 1 || transform.yScale() > 1) {
        adjustedPaintRect.inflateX(1 / transform.xScale());
        adjustedPaintRect.inflateY(1 / transform.yScale());
    }
    NSTextFieldCell *textField = this->textField();

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    [textField setEnabled:(isEnabled(o) && !isReadOnlyControl(o))];
    [textField drawWithFrame:NSRect(adjustedPaintRect) inView:documentViewFor(o)];

    [textField setControlView:nil];

    return false;
}

void RenderThemeMac::adjustTextFieldStyle(StyleResolver&, RenderStyle&, const Element*) const
{
}

bool RenderThemeMac::paintTextArea(const RenderObject& o, const PaintInfo& paintInfo, const FloatRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context());
    wkDrawBezeledTextArea(r, isEnabled(o) && !isReadOnlyControl(o));
    return false;
}

void RenderThemeMac::adjustTextAreaStyle(StyleResolver&, RenderStyle&, const Element*) const
{
}

const int* RenderThemeMac::popupButtonMargins() const
{
    static const int margins[3][4] =
    {
        { 0, 3, 1, 3 },
        { 0, 3, 2, 3 },
        { 0, 1, 0, 1 }
    };
    return margins[[popupButton() controlSize]];
}

const IntSize* RenderThemeMac::popupButtonSizes() const
{
    static const IntSize sizes[3] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) };
    return sizes;
}

const int* RenderThemeMac::popupButtonPadding(NSControlSize size) const
{
    static const int padding[3][4] =
    {
        { 2, 26, 3, 8 },
        { 2, 23, 3, 8 },
        { 2, 22, 3, 10 }
    };
    return padding[size];
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
        paintInfo.context().translate(inflatedRect.x(), inflatedRect.y());
        paintInfo.context().scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context().translate(-inflatedRect.x(), -inflatedRect.y());
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
        return NSRelevancyLevelIndicatorStyle;
    case DiscreteCapacityLevelIndicatorPart:
        return NSDiscreteCapacityLevelIndicatorStyle;
    case RatingLevelIndicatorPart:
        return NSRatingLevelIndicatorStyle;
    case MeterPart:
    case ContinuousCapacityLevelIndicatorPart:
    default:
        return NSContinuousCapacityLevelIndicatorStyle;
    }

}

NSLevelIndicatorCell* RenderThemeMac::levelIndicatorFor(const RenderMeter& renderMeter) const
{
    const RenderStyle& style = renderMeter.style();
    ASSERT(style.appearance() != NoControlPart);

    if (!m_levelIndicator)
        m_levelIndicator = adoptNS([[NSLevelIndicatorCell alloc] initWithLevelIndicatorStyle:NSContinuousCapacityLevelIndicatorStyle]);
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
    [cell setBaseWritingDirection:style.isLeftToRightDirection() ? NSWritingDirectionLeftToRight : NSWritingDirectionRightToLeft];
    [cell setMinValue:element->min()];
    [cell setMaxValue:element->max()];
    RetainPtr<NSNumber> valueObject = [NSNumber numberWithDouble:value];
    [cell setObjectValue:valueObject.get()];

    return cell;
}

#endif

const IntSize* RenderThemeMac::progressBarSizes() const
{
    static const IntSize sizes[3] = { IntSize(0, 20), IntSize(0, 12), IntSize(0, 12) };
    return sizes;
}

const int* RenderThemeMac::progressBarMargins(NSControlSize controlSize) const
{
    static const int margins[3][4] =
    {
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

double RenderThemeMac::animationRepeatIntervalForProgressBar(RenderProgress&) const
{
    return progressAnimationFrameRate;
}

double RenderThemeMac::animationDurationForProgressBar(RenderProgress&) const
{
    return progressAnimationNumFrames * progressAnimationFrameRate;
}

void RenderThemeMac::adjustProgressBarStyle(StyleResolver&, RenderStyle&, const Element*) const
{
}

bool RenderThemeMac::paintProgressBar(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!is<RenderProgress>(renderObject))
        return true;

    IntRect inflatedRect = progressBarRectForBounds(renderObject, rect);
    NSControlSize controlSize = controlSizeForFont(renderObject.style());

    const auto& renderProgress = downcast<RenderProgress>(renderObject);
    HIThemeTrackDrawInfo trackInfo;
    trackInfo.version = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (controlSize == NSRegularControlSize)
#pragma clang diagnostic pop
        trackInfo.kind = renderProgress.position() < 0 ? kThemeLargeIndeterminateBar : kThemeLargeProgressBar;
    else
        trackInfo.kind = renderProgress.position() < 0 ? kThemeMediumIndeterminateBar : kThemeMediumProgressBar;

    float deviceScaleFactor = renderObject.document().deviceScaleFactor();
    trackInfo.bounds = IntRect(IntPoint(), inflatedRect.size());
    trackInfo.min = 0;
    trackInfo.max = std::numeric_limits<SInt32>::max();
    trackInfo.value = lround(renderProgress.position() * nextafter(trackInfo.max, 0));
    trackInfo.trackInfo.progress.phase = lround(renderProgress.animationProgress() * nextafter(progressAnimationNumFrames, 0) * deviceScaleFactor);
    trackInfo.attributes = kThemeTrackHorizontal;
    trackInfo.enableState = isActive(renderObject) ? kThemeTrackActive : kThemeTrackInactive;
    trackInfo.reserved = 0;
    trackInfo.filler1 = 0;

    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::createCompatibleBuffer(inflatedRect.size(), deviceScaleFactor, ColorSpaceSRGB, paintInfo.context(), true);
    if (!imageBuffer)
        return true;

    ContextContainer cgContextContainer(imageBuffer->context());
    CGContextRef cgContext = cgContextContainer.context();
    HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);

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
const int arrowPaddingLeft = 6;
const int arrowPaddingRight = 6;
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
    IntRect bounds = IntRect(rect.x() + renderer.style().borderLeftWidth(),
        rect.y() + renderer.style().borderTopWidth(),
        rect.width() - renderer.style().borderLeftWidth() - renderer.style().borderRightWidth(),
        rect.height() - renderer.style().borderTopWidth() - renderer.style().borderBottomWidth());
    // Draw the gradients to give the styled popup menu a button appearance
    paintMenuListButtonGradients(renderer, paintInfo, bounds);

    // Since we actually know the size of the control here, we restrict the font scale to make sure the arrows will fit vertically in the bounds
    float fontScale = std::min(renderer.style().fontSize() / baseFontSize, bounds.height() / (baseArrowHeight * 2 + baseSpaceBetweenArrows));
    float centerY = bounds.y() + bounds.height() / 2.0f;
    float arrowHeight = baseArrowHeight * fontScale;
    float arrowWidth = baseArrowWidth * fontScale;
    float leftEdge = bounds.maxX() - arrowPaddingRight * renderer.style().effectiveZoom() - arrowWidth;
    float spaceBetweenArrows = baseSpaceBetweenArrows * fontScale;

    if (bounds.width() < arrowWidth + arrowPaddingLeft * renderer.style().effectiveZoom())
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

    Color leftSeparatorColor(0, 0, 0, 40);
    Color rightSeparatorColor(255, 255, 255, 40);

    // FIXME: Should the separator thickness and space be scaled up by fontScale?
    int separatorSpace = 2; // Deliberately ignores zoom since it looks nicer if it stays thin.
    int leftEdgeOfSeparator = static_cast<int>(leftEdge - arrowPaddingLeft * renderer.style().effectiveZoom()); // FIXME: Round?

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
    static const IntSize sizes[3] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) };
    return sizes;
}

void RenderThemeMac::adjustMenuListStyle(StyleResolver& styleResolver, RenderStyle& style, const Element* e) const
{
    NSControlSize controlSize = controlSizeForFont(style);

    style.resetBorder();
    style.resetPadding();

    // Height is locked to auto.
    style.setHeight(Length(Auto));

    // White-space is locked to pre
    style.setWhiteSpace(PRE);

    // Set the foreground color to black or gray when we have the aqua look.
    // Cast to RGB32 is to work around a compiler bug.
    style.setColor(e && !e->isDisabledFormControl() ? static_cast<RGBA32>(Color::black) : Color::darkGray);

    // Set the button's vertical size.
    setSizeFromFont(style, menuListButtonSizes());

    // Our font is locked to the appropriate system font size for the control.  To clarify, we first use the CSS-specified font to figure out
    // a reasonable control size, but once that control size is determined, we throw that font away and use the appropriate
    // system font for the control size instead.
    setFontFromControlSize(styleResolver, style, controlSize);

    style.setBoxShadow(nullptr);
}

LengthBox RenderThemeMac::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.appearance() == MenulistPart) {
        return { static_cast<int>(popupButtonPadding(controlSizeForFont(style))[topPadding] * style.effectiveZoom()),
            static_cast<int>(popupButtonPadding(controlSizeForFont(style))[rightPadding] * style.effectiveZoom()),
            static_cast<int>(popupButtonPadding(controlSizeForFont(style))[bottomPadding] * style.effectiveZoom()),
            static_cast<int>(popupButtonPadding(controlSizeForFont(style))[leftPadding] * style.effectiveZoom()) };
    }

    if (style.appearance() == MenulistButtonPart) {
        float arrowWidth = baseArrowWidth * (style.fontSize() / baseFontSize);
        return { static_cast<int>(styledPopupPaddingTop * style.effectiveZoom()),
            static_cast<int>(ceilf(arrowWidth + (arrowPaddingLeft + arrowPaddingRight + paddingBeforeSeparator) * style.effectiveZoom())),
            static_cast<int>(styledPopupPaddingBottom * style.effectiveZoom()), static_cast<int>(styledPopupPaddingLeft * style.effectiveZoom()) };
    }

    return { 0, 0, 0, 0 };
}

PopupMenuStyle::PopupMenuSize RenderThemeMac::popupMenuSize(const RenderStyle& style, IntRect& rect) const
{
    NSPopUpButtonCell* popupButton = this->popupButton();
    NSControlSize size = controlSizeForCell(popupButton, popupButtonSizes(), rect.size(), style.effectiveZoom());
    switch (size) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    case NSRegularControlSize:
        return PopupMenuStyle::PopupMenuSizeNormal;
    case NSSmallControlSize:
        return PopupMenuStyle::PopupMenuSizeSmall;
    case NSMiniControlSize:
        return PopupMenuStyle::PopupMenuSizeMini;
#pragma clang diagnostic pop
    default:
        return PopupMenuStyle::PopupMenuSizeNormal;
    }
}

void RenderThemeMac::adjustMenuListButtonStyle(StyleResolver&, RenderStyle& style, const Element*) const
{
    float fontScale = style.fontSize() / baseFontSize;

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

    // Update the various states we respond to.
    updateCheckedState(popupButton, o);
    updateEnabledState(popupButton, o);
    updatePressedState(popupButton, o);
}

void RenderThemeMac::paintCellAndSetFocusedElementNeedsRepaintIfNecessary(NSCell* cell, const RenderObject& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
    Page* page = renderer.document().page();
    bool shouldDrawFocusRing = isFocused(renderer) && renderer.style().outlineStyleIsAuto();
    bool shouldUseImageBuffer = renderer.style().effectiveZoom() != 1 || page->pageScaleFactor() != 1;
    bool shouldDrawCell = true;
    if (ThemeMac::drawCellOrFocusRingWithViewIntoContext(cell, paintInfo.context(), rect, documentViewFor(renderer), shouldDrawCell, shouldDrawFocusRing, shouldUseImageBuffer, page->deviceScaleFactor()))
        page->focusController().setFocusedElementNeedsRepaint();
}

const IntSize* RenderThemeMac::menuListSizes() const
{
    static const IntSize sizes[3] = { IntSize(9, 0), IntSize(5, 0), IntSize(0, 0) };
    return sizes;
}

int RenderThemeMac::minimumMenuListSize(const RenderStyle& style) const
{
    return sizeForSystemFont(style, menuListSizes()).width();
}

const int trackWidth = 5;
const int trackRadius = 2;

void RenderThemeMac::adjustSliderTrackStyle(StyleResolver&, RenderStyle& style, const Element*) const
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

void RenderThemeMac::adjustSliderThumbStyle(StyleResolver& styleResolver, RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(styleResolver, style, element);
    style.setBoxShadow(nullptr);
}

const float verticalSliderHeightPadding = 0.1f;

bool RenderThemeMac::paintSliderThumb(const RenderObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    NSSliderCell* sliderThumbCell = o.style().appearance() == SliderThumbVerticalPart
        ? sliderThumbVertical()
        : sliderThumbHorizontal();

    LocalCurrentGraphicsContext localContext(paintInfo.context());

    // Update the various states we respond to.
    updateEnabledState(sliderThumbCell, o);
        Element* focusDelegate = is<Element>(o.node()) ? downcast<Element>(*o.node()).focusDelegate() : nullptr;
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
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context().translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context().scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context().translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    bool shouldDrawCell = true;
    bool shouldDrawFocusRing = false;
    float deviceScaleFactor = o.document().page()->deviceScaleFactor();
    bool shouldUseImageBuffer = deviceScaleFactor != 1 || zoomLevel != 1;
    ThemeMac::drawCellOrFocusRingWithViewIntoContext(sliderThumbCell, paintInfo.context(), unzoomedRect, view, shouldDrawCell, shouldDrawFocusRing, shouldUseImageBuffer, deviceScaleFactor);
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

    IntRect unzoomedRect = r;

    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context().translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context().scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context().translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    // Set the search button to nil before drawing.  Then reset it so we can draw it later.
    [search setSearchButtonCell:nil];

    paintCellAndSetFocusedElementNeedsRepaintIfNecessary(search, o, paintInfo, unzoomedRect);
    [search setControlView:nil];
    [search resetSearchButtonCell];

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
    static const IntSize sizes[3] = { IntSize(0, 22), IntSize(0, 19), IntSize(0, 17) };
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

void RenderThemeMac::adjustSearchFieldStyle(StyleResolver& styleResolver, RenderStyle& style, const Element*) const
{
    // Override border.
    style.resetBorder();
    const short borderWidth = 2 * style.effectiveZoom();
    style.setBorderLeftWidth(borderWidth);
    style.setBorderLeftStyle(INSET);
    style.setBorderRightWidth(borderWidth);
    style.setBorderRightStyle(INSET);
    style.setBorderBottomWidth(borderWidth);
    style.setBorderBottomStyle(INSET);
    style.setBorderTopWidth(borderWidth);
    style.setBorderTopStyle(INSET);

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
    setFontFromControlSize(styleResolver, style, controlSize);

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

    float zoomLevel = box.style().effectiveZoom();

    FloatRect localBounds = adjustedCancelButtonRect([search cancelButtonRectForBounds:NSRect(snappedIntRect(inputBox.contentBoxRect()))]);
    FloatPoint paintingPos = convertToPaintingPosition(inputBox, box, localBounds.location(), r.location());

    FloatRect unzoomedRect(paintingPos, localBounds.size());
    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context().translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context().scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context().translate(-unzoomedRect.x(), -unzoomedRect.y());
    }
    [[search cancelButtonCell] drawWithFrame:unzoomedRect inView:documentViewFor(box)];
    [[search cancelButtonCell] setControlView:nil];
    return false;
}

const IntSize* RenderThemeMac::cancelButtonSizes() const
{
    static const IntSize sizes[3] = { IntSize(22, 22), IntSize(19, 19), IntSize(15, 15) };
    return sizes;
}

void RenderThemeMac::adjustSearchFieldCancelButtonStyle(StyleResolver&, RenderStyle& style, const Element*) const
{
    IntSize size = sizeForSystemFont(style, cancelButtonSizes());
    style.setWidth(Length(size.width(), Fixed));
    style.setHeight(Length(size.height(), Fixed));
    style.setBoxShadow(nullptr);
}

const int resultsArrowWidth = 5;
const IntSize* RenderThemeMac::resultsButtonSizes() const
{
    static const IntSize sizes[3] = { IntSize(19, 22), IntSize(17, 19), IntSize(17, 15) };
    return sizes;
}

const int emptyResultsOffset = 9;
void RenderThemeMac::adjustSearchFieldDecorationPartStyle(StyleResolver&, RenderStyle& style, const Element*) const
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

void RenderThemeMac::adjustSearchFieldResultsDecorationPartStyle(StyleResolver&, RenderStyle& style, const Element*) const
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

void RenderThemeMac::adjustSearchFieldResultsButtonStyle(StyleResolver&, RenderStyle& style, const Element*) const
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
    FloatPoint paintingPos = convertToPaintingPosition(inputBox, box, localBounds.location(), r.location());
    
    FloatRect unzoomedRect(paintingPos, localBounds.size());
    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context().translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context().scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context().translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    [[search searchButtonCell] drawWithFrame:unzoomedRect inView:documentViewFor(box)];
    [[search searchButtonCell] setControlView:nil];

    return false;
}

bool RenderThemeMac::paintSnapshottedPluginOverlay(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect&)
{
    if (paintInfo.phase != PaintPhaseBlockBackground)
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

    context.drawImage(*snapshot, alignedPluginRect, CompositeSourceOver);
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

bool RenderThemeMac::shouldHaveCapsLockIndicator(const HTMLInputElement& element) const
{
    return element.isPasswordField();
}

NSPopUpButtonCell* RenderThemeMac::popupButton() const
{
    if (!m_popupButton) {
        m_popupButton = adoptNS([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
        [m_popupButton.get() setUsesItemFromMenu:NO];
        [m_popupButton.get() setFocusRingType:NSFocusRingTypeExterior];
        // We don't want the app's UI layout direction to affect the appearance of popup buttons in
        // web content, which has its own layout direction.
        // FIXME: Make this depend on the directionality of the select element, once the rest of the
        // rendering code can account for the popup arrows appearing on the other side.
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [m_sliderThumbHorizontal.get() setSliderType:NSLinearSlider];
        [m_sliderThumbHorizontal.get() setControlSize:NSSmallControlSize];
        [m_sliderThumbHorizontal.get() setFocusRingType:NSFocusRingTypeExterior];
#pragma clang diagnostic pop
    }

    return m_sliderThumbHorizontal.get();
}

NSSliderCell* RenderThemeMac::sliderThumbVertical() const
{
    if (!m_sliderThumbVertical) {
        m_sliderThumbVertical = adoptNS([[NSSliderCell alloc] init]);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [m_sliderThumbVertical.get() setSliderType:NSLinearSlider];
        [m_sliderThumbVertical.get() setControlSize:NSSmallControlSize];
#pragma clang diagnostic pop
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
#if HAVE(APPKIT_SERVICE_CONTROLS_SUPPORT)
    if (!m_servicesRolloverButton) {
        m_servicesRolloverButton = [NSServicesRolloverButtonCell serviceRolloverButtonCellForStyle:NSSharingServicePickerStyleRollover];
        [m_servicesRolloverButton setBezelStyle:NSRoundedDisclosureBezelStyle];
        [m_servicesRolloverButton setButtonType:NSPushOnPushOffButton];
        [m_servicesRolloverButton setImagePosition:NSImageOnly];
        [m_servicesRolloverButton setState:NO];
    }

    return m_servicesRolloverButton.get();
#else
    return nil;
#endif
}

bool RenderThemeMac::paintImageControlsButton(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (paintInfo.phase != PaintPhaseBlockBackground)
        return true;

#if HAVE(APPKIT_SERVICE_CONTROLS_SUPPORT)
    NSServicesRolloverButtonCell *cell = servicesRolloverButtonCell();

    LocalCurrentGraphicsContext localContext(paintInfo.context());
    GraphicsContextStateSaver stateSaver(paintInfo.context());

    paintInfo.context().translate(rect.x(), rect.y());

    IntRect innerFrame(IntPoint(), rect.size());
    [cell drawWithFrame:innerFrame inView:documentViewFor(renderer)];
    [cell setControlView:nil];
#else
    UNUSED_PARAM(renderer);
    UNUSED_PARAM(rect);
#endif

    return true;
}

IntSize RenderThemeMac::imageControlsButtonSize(const RenderObject&) const
{
#if HAVE(APPKIT_SERVICE_CONTROLS_SUPPORT)
    return IntSize(servicesRolloverButtonCell().cellSize);
#else
    return IntSize();
#endif
}

IntSize RenderThemeMac::imageControlsButtonPositionOffset() const
{
#if HAVE(APPKIT_SERVICE_CONTROLS_SUPPORT)
    // FIXME: Currently the offsets will always be the same no matter what image rect you try with.
    // This may not always be true in the future.
    static const int dummyDimension = 100;
    IntRect dummyImageRect(0, 0, dummyDimension, dummyDimension);
    NSRect bounds = [servicesRolloverButtonCell() rectForBounds:dummyImageRect preferredEdge:NSMinYEdge];

    return IntSize(dummyDimension - bounds.origin.x, bounds.origin.y);
#else
    return IntSize();
#endif
}
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
const CGFloat attachmentIconSize = 48;
const CGFloat attachmentIconBackgroundPadding = 6;
const CGFloat attachmentIconBackgroundSize = attachmentIconSize + attachmentIconBackgroundPadding;
const CGFloat attachmentIconSelectionBorderThickness = 1;
const CGFloat attachmentIconBackgroundRadius = 3;
const CGFloat attachmentIconToTitleMargin = 2;

static Color attachmentIconBackgroundColor() { return Color(0, 0, 0, 30); }
static Color attachmentIconBorderColor() { return Color(255, 255, 255, 125); }

const CGFloat attachmentTitleFontSize = 12;
const CGFloat attachmentTitleBackgroundRadius = 3;
const CGFloat attachmentTitleBackgroundPadding = 3;
const CGFloat attachmentTitleMaximumWidth = 100 - (attachmentTitleBackgroundPadding * 2);
const CFIndex attachmentTitleMaximumLineCount = 2;

static Color attachmentTitleInactiveBackgroundColor() { return Color(204, 204, 204, 255); }
static Color attachmentTitleInactiveTextColor() { return Color(100, 100, 100, 255); }

const CGFloat attachmentSubtitleFontSize = 10;
static Color attachmentSubtitleTextColor() { return Color(82, 145, 214, 255); }

const CGFloat attachmentProgressBarWidth = 30;
const CGFloat attachmentProgressBarHeight = 5;
const CGFloat attachmentProgressBarOffset = -9;
const CGFloat attachmentProgressBarBorderWidth = 1;
static Color attachmentProgressBarBackgroundColor() { return Color(0, 0, 0, 89); }
static Color attachmentProgressBarFillColor() { return Color(Color::white); }
static Color attachmentProgressBarBorderColor() { return Color(0, 0, 0, 128); }

const CGFloat attachmentMargin = 3;

struct AttachmentLayout {
    explicit AttachmentLayout(const RenderAttachment&);

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

    RetainPtr<CTLineRef> subtitleLine;
    FloatRect subtitleTextRect;

private:
    void layOutTitle(const RenderAttachment&);
    void layOutSubtitle(const RenderAttachment&);

    void addTitleLine(CTLineRef, CGFloat& yOffset, Vector<CGPoint> origins, CFIndex lineIndex, const RenderAttachment&);
};

static NSColor *titleTextColorForAttachment(const RenderAttachment& attachment)
{
    if (attachment.selectionState() != RenderObject::SelectionNone) {
        if (attachment.frame().selection().isFocusedAndActive())
            return [NSColor alternateSelectedControlTextColor];    
        return (NSColor *)cachedCGColor(attachmentTitleInactiveTextColor());
    }

    return [NSColor blackColor];
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

    String title = attachment.attachmentElement().attachmentTitle();
    if (title.isEmpty())
        return;

    NSDictionary *textAttributes = @{
        (id)kCTFontAttributeName: (id)font.get(),
        (id)kCTForegroundColorAttributeName: titleTextColorForAttachment(attachment)
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
    NSRange remainingRange = NSMakeRange(remainingRangeStart, [attributedTitle length] - remainingRangeStart);
    NSAttributedString *remainingString = [attributedTitle attributedSubstringFromRange:remainingRange];
    RetainPtr<CTLineRef> remainingLine = adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)remainingString));
    RetainPtr<NSAttributedString> ellipsisString = adoptNS([[NSAttributedString alloc] initWithString:@"\u2026" attributes:textAttributes]);
    RetainPtr<CTLineRef> ellipsisLine = adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)ellipsisString.get()));
    RetainPtr<CTLineRef> truncatedLine = adoptCF(CTLineCreateTruncatedLine(remainingLine.get(), attachmentTitleMaximumWidth, kCTLineTruncationMiddle, ellipsisLine.get()));

    if (!truncatedLine)
        truncatedLine = remainingLine;

    addTitleLine(truncatedLine.get(), yOffset, origins, lineIndex, attachment);
}

void AttachmentLayout::layOutSubtitle(const RenderAttachment& attachment)
{
    String subtitleText = attachment.attachmentElement().fastGetAttribute(subtitleAttr);

    if (subtitleText.isEmpty())
        return;

    CFStringRef language = 0; // By not specifying a language we use the system language.
    RetainPtr<CTFontRef> font = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, attachmentSubtitleFontSize, language));
    NSDictionary *textAttributes = @{
        (id)kCTFontAttributeName: (id)font.get(),
        (id)kCTForegroundColorAttributeName: (NSColor *)cachedCGColor(attachmentSubtitleTextColor())
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

AttachmentLayout::AttachmentLayout(const RenderAttachment& attachment)
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
    attachmentRect.unite(subtitleTextRect);
    attachmentRect.inflate(attachmentMargin);
    attachmentRect = encloseRectToDevicePixels(attachmentRect, attachment.document().deviceScaleFactor());
}

LayoutSize RenderThemeMac::attachmentIntrinsicSize(const RenderAttachment& attachment) const
{
    AttachmentLayout layout(attachment);
    return LayoutSize(layout.attachmentRect.size());
}

int RenderThemeMac::attachmentBaseline(const RenderAttachment& attachment) const
{
    AttachmentLayout layout(attachment);
    return layout.baseline;
}

static void paintAttachmentIconBackground(const RenderAttachment&, GraphicsContext& context, AttachmentLayout& layout)
{
    // FIXME: Finder has a discontinuous behavior here when you have a background color other than white,
    // where it switches into 'bordered mode' and the border pops in on top of the background.
    bool paintBorder = true;

    FloatRect backgroundRect = layout.iconBackgroundRect;
    if (paintBorder)
        backgroundRect.inflate(-attachmentIconSelectionBorderThickness);

    context.fillRoundedRect(FloatRoundedRect(backgroundRect, FloatRoundedRect::Radii(attachmentIconBackgroundRadius)), attachmentIconBackgroundColor());

    if (paintBorder) {
        FloatRect borderRect = layout.iconBackgroundRect;
        borderRect.inflate(-attachmentIconSelectionBorderThickness / 2);

        FloatSize iconBackgroundRadiusSize(attachmentIconBackgroundRadius, attachmentIconBackgroundRadius);
        Path borderPath;
        borderPath.addRoundedRect(borderRect, iconBackgroundRadiusSize);
        context.setStrokeColor(attachmentIconBorderColor());
        context.setStrokeThickness(attachmentIconSelectionBorderThickness);
        context.strokePath(borderPath);
    }
}

static RefPtr<Icon> iconForAttachment(const RenderAttachment& attachment)
{
    String MIMEType = attachment.attachmentElement().attachmentType();
    if (!MIMEType.isEmpty()) {
        if (equalIgnoringASCIICase(MIMEType, "multipart/x-folder") || equalIgnoringASCIICase(MIMEType, "application/vnd.apple.folder")) {
            if (auto icon = Icon::createIconForUTI("public.directory"))
                return icon;
        } else if (auto icon = Icon::createIconForMIMEType(MIMEType))
            return icon;
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
    auto icon = iconForAttachment(attachment);
    if (!icon)
        return;
    icon->paint(context, layout.iconRect);
}

static void paintAttachmentTitleBackground(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout)
{
    if (layout.lines.isEmpty())
        return;

    Vector<FloatRect> backgroundRects;

    for (size_t i = 0; i < layout.lines.size(); ++i)
        backgroundRects.append(layout.lines[i].backgroundRect);

    Color backgroundColor;
    if (attachment.frame().selection().isFocusedAndActive())
        backgroundColor = convertNSColorToColor([NSColor alternateSelectedControlColor]);
    else
        backgroundColor = attachmentTitleInactiveBackgroundColor();

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

        CGContextSetTextMatrix(context.platformContext(), CGAffineTransformIdentity);
        CTLineDraw(line.line.get(), context.platformContext());
    }
}

static void paintAttachmentSubtitle(const RenderAttachment&, GraphicsContext& context, AttachmentLayout& layout)
{
    GraphicsContextStateSaver saver(context);

    context.translate(toFloatSize(layout.subtitleTextRect.minXMaxYCorner()));
    context.scale(FloatSize(1, -1));

    CGContextSetTextMatrix(context.platformContext(), CGAffineTransformIdentity);
    CTLineDraw(layout.subtitleLine.get(), context.platformContext());
}

static void paintAttachmentProgress(const RenderAttachment& attachment, GraphicsContext& context, AttachmentLayout& layout)
{
    String progressString = attachment.attachmentElement().fastGetAttribute(progressAttr);
    if (progressString.isEmpty())
        return;
    bool validProgress;
    float progress = progressString.toFloat(&validProgress);
    if (!validProgress)
        return;

    GraphicsContextStateSaver saver(context);

    FloatRect progressBounds((attachmentIconBackgroundSize - attachmentProgressBarWidth) / 2, layout.iconBackgroundRect.maxY() + attachmentProgressBarOffset - attachmentProgressBarHeight, attachmentProgressBarWidth, attachmentProgressBarHeight);

    FloatRect borderRect = progressBounds;
    borderRect.inflate(-0.5);
    FloatRect backgroundRect = borderRect;
    backgroundRect.inflate(-attachmentProgressBarBorderWidth / 2);

    FloatRoundedRect backgroundRoundedRect(backgroundRect, FloatRoundedRect::Radii(backgroundRect.height() / 2));
    context.fillRoundedRect(backgroundRoundedRect, attachmentProgressBarBackgroundColor());

    {
        GraphicsContextStateSaver clipSaver(context);
        context.clipRoundedRect(backgroundRoundedRect);

        FloatRect progressRect = progressBounds;
        progressRect.setWidth(progressRect.width() * progress);
        progressRect = encloseRectToDevicePixels(progressRect, attachment.document().deviceScaleFactor());

        context.fillRect(progressRect, attachmentProgressBarFillColor());
    }

    Path borderPath;
    float borderRadius = borderRect.height() / 2;
    borderPath.addRoundedRect(borderRect, FloatSize(borderRadius, borderRadius));
    context.setStrokeColor(attachmentProgressBarBorderColor());
    context.setStrokeThickness(attachmentProgressBarBorderWidth);
    context.strokePath(borderPath);
}

bool RenderThemeMac::paintAttachment(const RenderObject& renderer, const PaintInfo& paintInfo, const IntRect& paintRect)
{
    if (!is<RenderAttachment>(renderer))
        return false;

    const RenderAttachment& attachment = downcast<RenderAttachment>(renderer);

    AttachmentLayout layout(attachment);

    GraphicsContext& context = paintInfo.context();
    LocalCurrentGraphicsContext localContext(context);
    GraphicsContextStateSaver saver(context);

    context.translate(toFloatSize(paintRect.location()));
    context.translate(FloatSize((layout.attachmentRect.width() - attachmentIconBackgroundSize) / 2, 0));

    bool useSelectedStyle = attachment.selectionState() != RenderObject::SelectionNone;

    if (useSelectedStyle)
        paintAttachmentIconBackground(attachment, context, layout);
    paintAttachmentIcon(attachment, context, layout);
    if (useSelectedStyle)
        paintAttachmentTitleBackground(attachment, context, layout);
    paintAttachmentTitle(attachment, context, layout);
    paintAttachmentSubtitle(attachment, context, layout);
    paintAttachmentProgress(attachment, context, layout);

    return true;
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

} // namespace WebCore

#endif // !PLATFORM(IOS)
