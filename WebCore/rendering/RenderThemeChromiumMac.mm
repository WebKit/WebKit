/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
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

// FIXME: we still need to figure out if passing a null view to the cell
// drawing routines will work. I expect not, and if that's the case we'll have
// to figure out something else. For now, at least leave the lines commented
// in, but the procurement of the view if 0'd.

#import "config.h"
#import "RenderThemeChromiumMac.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <math.h>

#import "BitmapImage.h"
#import "ChromiumBridge.h"
#import "ColorMac.h"
#import "CSSStyleSelector.h"
#import "CSSValueKeywords.h"
#import "Element.h"
#import "FoundationExtras.h"
#import "FrameView.h"
#import "Gradient.h"
#import "GraphicsContext.h"
#import "HTMLInputElement.h"
#import "HTMLMediaElement.h"
#import "HTMLNames.h"
#import "Image.h"
#import "LocalCurrentGraphicsContext.h"
#import "MediaControlElements.h"
#import "RenderMedia.h"
#import "RenderSlider.h"
#import "RenderView.h"
#import "SharedBuffer.h"
#import "TimeRanges.h"
#import "UserAgentStyleSheets.h"
#import "WebCoreSystemInterface.h"
#import <wtf/RetainPtr.h>

#ifdef BUILDING_ON_TIGER
typedef int NSInteger;
typedef unsigned NSUInteger;
#endif

using std::min;

// The methods in this file are specific to the Mac OS X platform.

// FIXME: The platform-independent code in this class should be factored out and merged with RenderThemeSafari. 

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
    [super init];
    _theme = theme;
    
    return self;
}

- (void)systemColorsDidChange:(NSNotification *)notification
{
    ASSERT([[notification name] isEqualToString:NSSystemColorsDidChangeNotification]);
    _theme->platformColorsDidChange();
}

@end

namespace WebCore {

using namespace HTMLNames;

enum {
    TopMargin,
    RightMargin,
    BottomMargin,
    LeftMargin
};

enum {
    TopPadding,
    RightPadding,
    BottomPadding,
    LeftPadding
};

#if ENABLE(VIDEO)
// Attempt to retrieve a HTMLMediaElement from a Node. Returns 0 if one cannot be found.
static HTMLMediaElement* mediaElementParent(Node* node)
{
    if (!node)
        return 0;
    Node* mediaNode = node->shadowAncestorNode();
    if (!mediaNode || (!mediaNode->hasTagName(HTMLNames::videoTag) && !mediaNode->hasTagName(HTMLNames::audioTag)))
        return 0;

    return static_cast<HTMLMediaElement*>(mediaNode);
}
#endif

// In our Mac port, we don't define PLATFORM(MAC) and thus don't pick up the
// |operator NSRect()| on WebCore::IntRect and FloatRect. This substitues for
// that missing conversion operator.
NSRect IntRectToNSRect(const IntRect & rect)
{
    return NSMakeRect(rect.x(), rect.y(), rect.width(), rect.height());
}

NSRect FloatRectToNSRect(const FloatRect & rect)
{
    return NSMakeRect(rect.x(), rect.y(), rect.width(), rect.height());
}

IntRect NSRectToIntRect(const NSRect & rect)
{
    return IntRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
}

PassRefPtr<RenderTheme> RenderThemeChromiumMac::create()
{
    return adoptRef(new RenderThemeChromiumMac);
}

#if ENABLE(VIDEO)
String RenderThemeChromiumMac::extraMediaControlsStyleSheet()
{
    return String(mediaControlsChromiumUserAgentStyleSheet, sizeof(mediaControlsChromiumUserAgentStyleSheet));
}
#endif

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeChromiumMac::create().releaseRef();
    return rt;
}

RenderThemeChromiumMac::RenderThemeChromiumMac()
    : m_isSliderThumbHorizontalPressed(false)
    , m_isSliderThumbVerticalPressed(false)
    , m_notificationObserver(AdoptNS, [[WebCoreRenderThemeNotificationObserver alloc] initWithTheme:this])
{
    [[NSNotificationCenter defaultCenter] addObserver:m_notificationObserver.get()
                                                        selector:@selector(systemColorsDidChange:)
                                                            name:NSSystemColorsDidChangeNotification
                                                          object:nil];
}

RenderThemeChromiumMac::~RenderThemeChromiumMac()
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_notificationObserver.get()];
}

Color RenderThemeChromiumMac::platformActiveSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor selectedTextBackgroundColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color(static_cast<int>(255.0 * [color redComponent]), static_cast<int>(255.0 * [color greenComponent]), static_cast<int>(255.0 * [color blueComponent]));
}

Color RenderThemeChromiumMac::platformInactiveSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor secondarySelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color(static_cast<int>(255.0 * [color redComponent]), static_cast<int>(255.0 * [color greenComponent]), static_cast<int>(255.0 * [color blueComponent]));
}

Color RenderThemeChromiumMac::platformActiveListBoxSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor alternateSelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color(static_cast<int>(255.0 * [color redComponent]), static_cast<int>(255.0 * [color greenComponent]), static_cast<int>(255.0 * [color blueComponent]));
}

Color RenderThemeChromiumMac::platformFocusRingColor() const
{
    if (ChromiumBridge::layoutTestMode())
        return oldAquaFocusRingColor();

    return systemColor(CSSValueWebkitFocusRingColor);
}

static FontWeight toFontWeight(NSInteger appKitFontWeight)
{
    ASSERT(appKitFontWeight > 0 && appKitFontWeight < 15);
    if (appKitFontWeight > 14)
        appKitFontWeight = 14;
    else if (appKitFontWeight < 1)
        appKitFontWeight = 1;

    static FontWeight fontWeights[] = {
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

void RenderThemeChromiumMac::systemFont(int cssValueId, FontDescription& fontDescription) const
{
    static FontDescription systemFont;
    static FontDescription smallSystemFont;
    static FontDescription menuFont;
    static FontDescription labelFont;
    static FontDescription miniControlFont;
    static FontDescription smallControlFont;
    static FontDescription controlFont;

    FontDescription* cachedDesc;
    NSFont* font = nil;
    switch (cssValueId) {
    case CSSValueSmallCaption:
        cachedDesc = &smallSystemFont;
        if (!smallSystemFont.isAbsoluteSize())
            font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
        break;
    case CSSValueMenu:
        cachedDesc = &menuFont;
        if (!menuFont.isAbsoluteSize())
            font = [NSFont menuFontOfSize:[NSFont systemFontSize]];
        break;
    case CSSValueStatusBar:
        cachedDesc = &labelFont;
        if (!labelFont.isAbsoluteSize())
            font = [NSFont labelFontOfSize:[NSFont labelFontSize]];
        break;
    case CSSValueWebkitMiniControl:
        cachedDesc = &miniControlFont;
        if (!miniControlFont.isAbsoluteSize())
            font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSMiniControlSize]];
        break;
    case CSSValueWebkitSmallControl:
        cachedDesc = &smallControlFont;
        if (!smallControlFont.isAbsoluteSize())
            font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]];
        break;
    case CSSValueWebkitControl:
        cachedDesc = &controlFont;
        if (!controlFont.isAbsoluteSize())
            font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSRegularControlSize]];
        break;
    default:
        cachedDesc = &systemFont;
        if (!systemFont.isAbsoluteSize())
            font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    }

    if (font) {
        NSFontManager *fontManager = [NSFontManager sharedFontManager];
        cachedDesc->setIsAbsoluteSize(true);
        cachedDesc->setGenericFamily(FontDescription::NoFamily);
        cachedDesc->firstFamily().setFamily([font familyName]);
        cachedDesc->setSpecifiedSize([font pointSize]);
        cachedDesc->setWeight(toFontWeight([fontManager weightOfFont:font]));
        cachedDesc->setItalic([fontManager traitsOfFont:font] & NSItalicFontMask);
    }
    fontDescription = *cachedDesc;
}

static RGBA32 convertNSColorToColor(NSColor *color)
{
    NSColor *colorInColorSpace = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
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
                                                                         colorSpaceName:NSCalibratedRGBColorSpace
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
                                                                         colorSpaceName:NSCalibratedRGBColorSpace
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

void RenderThemeChromiumMac::platformColorsDidChange()
{
    m_systemColorCache.clear();
    RenderTheme::platformColorsDidChange();
}

Color RenderThemeChromiumMac::systemColor(int cssValueId) const
{
    if (m_systemColorCache.contains(cssValueId))
        return m_systemColorCache.get(cssValueId);
    
    Color color;
    switch (cssValueId) {
    case CSSValueActiveborder:
        color = convertNSColorToColor([NSColor keyboardFocusIndicatorColor]);
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
    }

    if (!color.isValid())
        color = RenderTheme::systemColor(cssValueId);

    if (color.isValid())
        m_systemColorCache.set(cssValueId, color.rgb());

    return color;
}

bool RenderThemeChromiumMac::isControlStyled(const RenderStyle* style, const BorderData& border,
                                             const FillLayer& background, const Color& backgroundColor) const
{
    if (style->appearance() == TextFieldPart || style->appearance() == TextAreaPart || style->appearance() == ListboxPart)
        return style->border() != border;
        
    // FIXME: This is horrible, but there is not much else that can be done.  Menu lists cannot draw properly when
    // scaled.  They can't really draw properly when transformed either.  We can't detect the transform case at style
    // adjustment time so that will just have to stay broken.  We can however detect that we're zooming.  If zooming
    // is in effect we treat it like the control is styled.
    if (style->appearance() == MenulistPart && style->effectiveZoom() != 1.0f)
        return true;

    return RenderTheme::isControlStyled(style, border, background, backgroundColor);
}

// FIXME: Use the code from the old upstream version, before it was converted to the new theme API in r37731.
void RenderThemeChromiumMac::adjustRepaintRect(const RenderObject* o, IntRect& r)
{
    float zoomLevel = o->style()->effectiveZoom();

    switch (o->style()->appearance()) {
    case CheckboxPart: {
        // Since we query the prototype cell, we need to update its state to match.
        setCheckboxCellState(o, r);

        // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
        // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
        IntSize size = checkboxSizes()[[checkbox() controlSize]];
        size.setHeight(size.height() * zoomLevel);
        size.setWidth(size.width() * zoomLevel);
        r = inflateRect(r, size, checkboxMargins(), zoomLevel);
        break;
    }
    case RadioPart: {
        // Since we query the prototype cell, we need to update its state to match.
        setRadioCellState(o, r);

        // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
        // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
        IntSize size = radioSizes()[[radio() controlSize]];
        size.setHeight(size.height() * zoomLevel);
        size.setWidth(size.width() * zoomLevel);
        r = inflateRect(r, size, radioMargins(), zoomLevel);
        break;
    }
    case PushButtonPart:
    case DefaultButtonPart:
    case ButtonPart: {
        // Since we query the prototype cell, we need to update its state to match.
        setButtonCellState(o, r);

        // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
        // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
        if ([button() bezelStyle] == NSRoundedBezelStyle) {
            IntSize size = buttonSizes()[[button() controlSize]];
            size.setHeight(size.height() * zoomLevel);
            size.setWidth(r.width());
            r = inflateRect(r, size, buttonMargins(), zoomLevel);
        }
        break;
    }
    case MenulistPart: {
        setPopupButtonCellState(o, r);
        IntSize size = popupButtonSizes()[[popupButton() controlSize]];
        size.setHeight(size.height() * zoomLevel);
        size.setWidth(r.width());
        r = inflateRect(r, size, popupButtonMargins(), zoomLevel);
        break;
    }
    default:
        break;
    }
}

IntRect RenderThemeChromiumMac::inflateRect(const IntRect& r, const IntSize& size, const int* margins, float zoomLevel) const
{
    // Only do the inflation if the available width/height are too small.  Otherwise try to
    // fit the glow/check space into the available box's width/height.
    int widthDelta = r.width() - (size.width() + margins[LeftMargin] * zoomLevel + margins[RightMargin] * zoomLevel);
    int heightDelta = r.height() - (size.height() + margins[TopMargin] * zoomLevel + margins[BottomMargin] * zoomLevel);
    IntRect result(r);
    if (widthDelta < 0) {
        result.setX(result.x() - margins[LeftMargin] * zoomLevel);
        result.setWidth(result.width() - widthDelta);
    }
    if (heightDelta < 0) {
        result.setY(result.y() - margins[TopMargin] * zoomLevel);
        result.setHeight(result.height() - heightDelta);
    }
    return result;
}

// Updates the control tint (a.k.a. active state) of |cell| (from |o|).
// In the Chromium port, the renderer runs as a background process and controls'
// NSCell(s) lack a parent NSView. Therefore controls don't have their tint
// color updated correctly when the application is activated/deactivated.
// FocusController's setActive() is called when the application is
// activated/deactivated, which causes a repaint at which time this code is
// called.
// This function should be called before drawing any NSCell-derived controls,
// unless you're sure it isn't needed.
void RenderThemeChromiumMac::updateActiveState(NSCell* cell, const RenderObject* o)
{
    NSControlTint oldTint = [cell controlTint];
    NSControlTint tint = isActive(o) ? [NSColor currentControlTint] :
                                       NSClearControlTint;

    if (tint != oldTint)
        [cell setControlTint:tint];
}

void RenderThemeChromiumMac::updateCheckedState(NSCell* cell, const RenderObject* o)
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

void RenderThemeChromiumMac::updateEnabledState(NSCell* cell, const RenderObject* o)
{
    bool oldEnabled = [cell isEnabled];
    bool enabled = isEnabled(o);
    if (enabled != oldEnabled)
        [cell setEnabled:enabled];
}

void RenderThemeChromiumMac::updateFocusedState(NSCell* cell, const RenderObject* o)
{
    bool oldFocused = [cell showsFirstResponder];
    bool focused = isFocused(o) && o->style()->outlineStyleIsAuto();
    if (focused != oldFocused)
        [cell setShowsFirstResponder:focused];
}

void RenderThemeChromiumMac::updatePressedState(NSCell* cell, const RenderObject* o)
{
    bool oldPressed = [cell isHighlighted];
    bool pressed = (o->node() && o->node()->active());
    if (pressed != oldPressed)
        [cell setHighlighted:pressed];
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
int RenderThemeChromiumMac::baselinePosition(const RenderObject* o) const
{
    if (!o->isBox())
        return 0;

    if (o->style()->appearance() == CheckboxPart || o->style()->appearance() == RadioPart) {
        const RenderBox* box = toRenderBox(o);
        return box->marginTop() + box->height() - 2 * o->style()->effectiveZoom(); // The baseline is 2px up from the bottom of the checkbox/radio in AppKit.
    }
    return RenderTheme::baselinePosition(o);
}

bool RenderThemeChromiumMac::controlSupportsTints(const RenderObject* o) const
{
    // An alternate way to implement this would be to get the appropriate cell object
    // and call the private _needRedrawOnWindowChangedKeyState method. An advantage of
    // that would be that we would match AppKit behavior more closely, but a disadvantage
    // would be that we would rely on an AppKit SPI method.

    if (!isEnabled(o))
        return false;

    // Checkboxes only have tint when checked.
    if (o->style()->appearance() == CheckboxPart)
        return isChecked(o);

    // For now assume other controls have tint if enabled.
    return true;
}

NSControlSize RenderThemeChromiumMac::controlSizeForFont(RenderStyle* style) const
{
    int fontSize = style->fontSize();
    if (fontSize >= 16)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

void RenderThemeChromiumMac::setControlSize(NSCell* cell, const IntSize* sizes, const IntSize& minSize, float zoomLevel)
{
    NSControlSize size;
    if (minSize.width() >= static_cast<int>(sizes[NSRegularControlSize].width() * zoomLevel) &&
        minSize.height() >= static_cast<int>(sizes[NSRegularControlSize].height() * zoomLevel))
        size = NSRegularControlSize;
    else if (minSize.width() >= static_cast<int>(sizes[NSSmallControlSize].width() * zoomLevel) &&
             minSize.height() >= static_cast<int>(sizes[NSSmallControlSize].height() * zoomLevel))
        size = NSSmallControlSize;
    else
        size = NSMiniControlSize;
    if (size != [cell controlSize]) // Only update if we have to, since AppKit does work even if the size is the same.
        [cell setControlSize:size];
}

IntSize RenderThemeChromiumMac::sizeForFont(RenderStyle* style, const IntSize* sizes) const
{
    if (style->effectiveZoom() != 1.0f) {
        IntSize result = sizes[controlSizeForFont(style)];
        return IntSize(result.width() * style->effectiveZoom(), result.height() * style->effectiveZoom());
    }
    return sizes[controlSizeForFont(style)];
}

IntSize RenderThemeChromiumMac::sizeForSystemFont(RenderStyle* style, const IntSize* sizes) const
{
    if (style->effectiveZoom() != 1.0f) {
        IntSize result = sizes[controlSizeForSystemFont(style)];
        return IntSize(result.width() * style->effectiveZoom(), result.height() * style->effectiveZoom());
    }
    return sizes[controlSizeForSystemFont(style)];
}

void RenderThemeChromiumMac::setSizeFromFont(RenderStyle* style, const IntSize* sizes) const
{
    // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
    IntSize size = sizeForFont(style, sizes);
    if (style->width().isIntrinsicOrAuto() && size.width() > 0)
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto() && size.height() > 0)
        style->setHeight(Length(size.height(), Fixed));
}

void RenderThemeChromiumMac::setFontFromControlSize(CSSStyleSelector* selector, RenderStyle* style, NSControlSize controlSize) const
{
    FontDescription fontDescription;
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::SerifFamily);

    NSFont* font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSize]];
    fontDescription.firstFamily().setFamily([font familyName]);
    fontDescription.setComputedSize([font pointSize] * style->effectiveZoom());
    fontDescription.setSpecifiedSize([font pointSize] * style->effectiveZoom());

    // Reset line height
    style->setLineHeight(RenderStyle::initialLineHeight());

    if (style->setFontDescription(fontDescription))
        style->font().update(0);
}

NSControlSize RenderThemeChromiumMac::controlSizeForSystemFont(RenderStyle* style) const
{
    int fontSize = style->fontSize();
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSRegularControlSize])
        return NSRegularControlSize;
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSSmallControlSize])
        return NSSmallControlSize;
    return NSMiniControlSize;
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
bool RenderThemeChromiumMac::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);

    // Determine the width and height needed for the control and prepare the cell for painting.
    setCheckboxCellState(o, r);

    paintInfo.context->save();

    float zoomLevel = o->style()->effectiveZoom();

    // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
    // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
    NSButtonCell* checkbox = this->checkbox();
    IntSize size = checkboxSizes()[[checkbox controlSize]];
    size.setWidth(size.width() * zoomLevel);
    size.setHeight(size.height() * zoomLevel);
    IntRect inflatedRect = inflateRect(r, size, checkboxMargins(), zoomLevel);
    
    if (zoomLevel != 1.0f) {
        inflatedRect.setWidth(inflatedRect.width() / zoomLevel);
        inflatedRect.setHeight(inflatedRect.height() / zoomLevel);
        paintInfo.context->translate(inflatedRect.x(), inflatedRect.y());
        paintInfo.context->scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context->translate(-inflatedRect.x(), -inflatedRect.y());
    }
    
    [checkbox drawWithFrame:NSRect(IntRectToNSRect(inflatedRect)) inView:nil];
    [checkbox setControlView:nil];

    paintInfo.context->restore();

    return false;
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
const IntSize* RenderThemeChromiumMac::checkboxSizes() const
{
    static const IntSize sizes[3] = { IntSize(14, 14), IntSize(12, 12), IntSize(10, 10) };
    return sizes;
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
const int* RenderThemeChromiumMac::checkboxMargins() const
{
    static const int margins[3][4] =
    {
        { 3, 4, 4, 2 },
        { 4, 3, 3, 3 },
        { 4, 3, 3, 3 },
    };
    return margins[[checkbox() controlSize]];
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
void RenderThemeChromiumMac::setCheckboxCellState(const RenderObject* o, const IntRect& r)
{
    NSButtonCell* checkbox = this->checkbox();

    // Set the control size based off the rectangle we're painting into.
    setControlSize(checkbox, checkboxSizes(), r.size(), o->style()->effectiveZoom());

    // Update the various states we respond to.
    updateActiveState(checkbox, o);
    updateCheckedState(checkbox, o);
    updateEnabledState(checkbox, o);
    updatePressedState(checkbox, o);
    updateFocusedState(checkbox, o);
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
void RenderThemeChromiumMac::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style, checkboxSizes());
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
bool RenderThemeChromiumMac::paintRadio(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);

    // Determine the width and height needed for the control and prepare the cell for painting.
    setRadioCellState(o, r);

    paintInfo.context->save();

    float zoomLevel = o->style()->effectiveZoom();

    // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
    // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
    NSButtonCell* radio = this->radio();
    IntSize size = radioSizes()[[radio controlSize]];
    size.setWidth(size.width() * zoomLevel);
    size.setHeight(size.height() * zoomLevel);
    IntRect inflatedRect = inflateRect(r, size, radioMargins(), zoomLevel);
    
    if (zoomLevel != 1.0f) {
        inflatedRect.setWidth(inflatedRect.width() / zoomLevel);
        inflatedRect.setHeight(inflatedRect.height() / zoomLevel);
        paintInfo.context->translate(inflatedRect.x(), inflatedRect.y());
        paintInfo.context->scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context->translate(-inflatedRect.x(), -inflatedRect.y());
    }

    [radio drawWithFrame:NSRect(IntRectToNSRect(inflatedRect)) inView:nil];
    [radio setControlView:nil];

    paintInfo.context->restore();

    return false;
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
const IntSize* RenderThemeChromiumMac::radioSizes() const
{
    static const IntSize sizes[3] = { IntSize(14, 15), IntSize(12, 13), IntSize(10, 10) };
    return sizes;
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
const int* RenderThemeChromiumMac::radioMargins() const
{
    static const int margins[3][4] =
    {
        { 2, 2, 4, 2 },
        { 3, 2, 3, 2 },
        { 1, 0, 2, 0 },
    };
    return margins[[radio() controlSize]];
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
void RenderThemeChromiumMac::setRadioCellState(const RenderObject* o, const IntRect& r)
{
    NSButtonCell* radio = this->radio();

    // Set the control size based off the rectangle we're painting into.
    setControlSize(radio, radioSizes(), r.size(), o->style()->effectiveZoom());

    // Update the various states we respond to.
    updateActiveState(radio, o);
    updateCheckedState(radio, o);
    updateEnabledState(radio, o);
    updatePressedState(radio, o);
    updateFocusedState(radio, o);
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
void RenderThemeChromiumMac::setRadioSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style, radioSizes());
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
void RenderThemeChromiumMac::setButtonPaddingFromControlSize(RenderStyle* style, NSControlSize size) const
{
    // Just use 8px.  AppKit wants to use 11px for mini buttons, but that padding is just too large
    // for real-world Web sites (creating a huge necessary minimum width for buttons whose space is
    // by definition constrained, since we select mini only for small cramped environments.
    // This also guarantees the HTML4 <button> will match our rendering by default, since we're using a consistent
    // padding.
    const int padding = 8 * style->effectiveZoom();
    style->setPaddingLeft(Length(padding, Fixed));
    style->setPaddingRight(Length(padding, Fixed));
    style->setPaddingTop(Length(0, Fixed));
    style->setPaddingBottom(Length(0, Fixed));
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
void RenderThemeChromiumMac::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // There are three appearance constants for buttons.
    // (1) Push-button is the constant for the default Aqua system button.  Push buttons will not scale vertically and will not allow
    // custom fonts or colors.  <input>s use this constant.  This button will allow custom colors and font weights/variants but won't
    // scale vertically.
    // (2) square-button is the constant for the square button.  This button will allow custom fonts and colors and will scale vertically.
    // (3) Button is the constant that means "pick the best button as appropriate."  <button>s use this constant.  This button will
    // also scale vertically and allow custom fonts and colors.  It will attempt to use Aqua if possible and will make this determination
    // solely on the rectangle of the control.

    // Determine our control size based off our font.
    NSControlSize controlSize = controlSizeForFont(style);

    if (style->appearance() == PushButtonPart) {
        // Ditch the border.
        style->resetBorder();

        // Height is locked to auto.
        style->setHeight(Length(Auto));

        // White-space is locked to pre
        style->setWhiteSpace(PRE);

        // Set the button's vertical size.
        setButtonSize(style);

        // Add in the padding that we'd like to use.
        setButtonPaddingFromControlSize(style, controlSize);

        // Our font is locked to the appropriate system font size for the control.  To clarify, we first use the CSS-specified font to figure out
        // a reasonable control size, but once that control size is determined, we throw that font away and use the appropriate
        // system font for the control size instead.
        setFontFromControlSize(selector, style, controlSize);
    } else {
        // Set a min-height so that we can't get smaller than the mini button.
        style->setMinHeight(Length(static_cast<int>(15 * style->effectiveZoom()), Fixed));

        // Reset the top and bottom borders.
        style->resetBorderTop();
        style->resetBorderBottom();
    }

    style->setBoxShadow(0);
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
const IntSize* RenderThemeChromiumMac::buttonSizes() const
{
    static const IntSize sizes[3] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) };
    return sizes;
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
const int* RenderThemeChromiumMac::buttonMargins() const
{
    static const int margins[3][4] =
    {
        { 4, 6, 7, 6 },
        { 4, 5, 6, 5 },
        { 0, 1, 1, 1 },
    };
    return margins[[button() controlSize]];
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
void RenderThemeChromiumMac::setButtonSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style, buttonSizes());
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
void RenderThemeChromiumMac::setButtonCellState(const RenderObject* o, const IntRect& r)
{
    NSButtonCell* button = this->button();

    // Set the control size based off the rectangle we're painting into.
    if (o->style()->appearance() == SquareButtonPart ||
        r.height() > buttonSizes()[NSRegularControlSize].height() * o->style()->effectiveZoom()) {
        // Use the square button
        if ([button bezelStyle] != NSShadowlessSquareBezelStyle)
            [button setBezelStyle:NSShadowlessSquareBezelStyle];
    } else if ([button bezelStyle] != NSRoundedBezelStyle)
        [button setBezelStyle:NSRoundedBezelStyle];

    setControlSize(button, buttonSizes(), r.size(), o->style()->effectiveZoom());

    NSWindow *window = [nil window];
    BOOL isDefaultButton = (isDefault(o) && [window isKeyWindow]);
    [button setKeyEquivalent:(isDefaultButton ? @"\r" : @"")];

    // Update the various states we respond to.
    updateActiveState(button, o);
    updateCheckedState(button, o);
    updateEnabledState(button, o);
    updatePressedState(button, o);
    updateFocusedState(button, o);
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
bool RenderThemeChromiumMac::paintButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    NSButtonCell* button = this->button();
    LocalCurrentGraphicsContext localContext(paintInfo.context);

    // Determine the width and height needed for the control and prepare the cell for painting.
    setButtonCellState(o, r);

    paintInfo.context->save();

    // We inflate the rect as needed to account for padding included in the cell to accommodate the button
    // shadow.  We don't consider this part of the bounds of the control in WebKit.
    float zoomLevel = o->style()->effectiveZoom();
    IntSize size = buttonSizes()[[button controlSize]];
    size.setWidth(r.width());
    size.setHeight(size.height() * zoomLevel);
    IntRect inflatedRect = r;
    if ([button bezelStyle] == NSRoundedBezelStyle) {
        // Center the button within the available space.
        if (inflatedRect.height() > size.height()) {
            inflatedRect.setY(inflatedRect.y() + (inflatedRect.height() - size.height()) / 2);
            inflatedRect.setHeight(size.height());
        }

        // Now inflate it to account for the shadow.
        inflatedRect = inflateRect(inflatedRect, size, buttonMargins(), zoomLevel);

        if (zoomLevel != 1.0f) {
            inflatedRect.setWidth(inflatedRect.width() / zoomLevel);
            inflatedRect.setHeight(inflatedRect.height() / zoomLevel);
            paintInfo.context->translate(inflatedRect.x(), inflatedRect.y());
            paintInfo.context->scale(FloatSize(zoomLevel, zoomLevel));
            paintInfo.context->translate(-inflatedRect.x(), -inflatedRect.y());
        }
    } 

    NSView *view = nil;
    NSWindow *window = [view window];
    NSButtonCell *previousDefaultButtonCell = [window defaultButtonCell];

    if (isDefault(o) && [window isKeyWindow]) {
        [window setDefaultButtonCell:button];
        wkAdvanceDefaultButtonPulseAnimation(button);
    } else if ([previousDefaultButtonCell isEqual:button])
        [window setDefaultButtonCell:nil];

    [button drawWithFrame:NSRect(IntRectToNSRect(inflatedRect)) inView:view];
    [button setControlView:nil];

    if (![previousDefaultButtonCell isEqual:button])
        [window setDefaultButtonCell:previousDefaultButtonCell];

    paintInfo.context->restore();

    return false;
}

bool RenderThemeChromiumMac::paintTextField(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawBezeledTextFieldCell(IntRectToNSRect(r), isEnabled(o) && !isReadOnlyControl(o));
    return false;
}

void RenderThemeChromiumMac::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

bool RenderThemeChromiumMac::paintCapsLockIndicator(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    if (paintInfo.context->paintingDisabled())
        return true;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawCapsLockIndicator(paintInfo.context->platformContext(), r);
    
    return false;
}

bool RenderThemeChromiumMac::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawBezeledTextArea(IntRectToNSRect(r), isEnabled(o) && !isReadOnlyControl(o));
    return false;
}

void RenderThemeChromiumMac::adjustTextAreaStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

const int* RenderThemeChromiumMac::popupButtonMargins() const
{
    static const int margins[3][4] =
    {
        { 0, 3, 1, 3 },
        { 0, 3, 2, 3 },
        { 0, 1, 0, 1 }
    };
    return margins[[popupButton() controlSize]];
}

const IntSize* RenderThemeChromiumMac::popupButtonSizes() const
{
    static const IntSize sizes[3] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) };
    return sizes;
}

const int* RenderThemeChromiumMac::popupButtonPadding(NSControlSize size) const
{
    static const int padding[3][4] =
    {
        { 2, 26, 3, 8 },
        { 2, 23, 3, 8 },
        { 2, 22, 3, 10 }
    };
    return padding[size];
}

bool RenderThemeChromiumMac::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);

    setPopupButtonCellState(o, r);

    NSPopUpButtonCell* popupButton = this->popupButton();

    float zoomLevel = o->style()->effectiveZoom();
    IntSize size = popupButtonSizes()[[popupButton controlSize]];
    size.setHeight(size.height() * zoomLevel);
    size.setWidth(r.width());

    // Now inflate it to account for the shadow.
    IntRect inflatedRect = r;
    if (r.width() >= minimumMenuListSize(o->style()))
        inflatedRect = inflateRect(inflatedRect, size, popupButtonMargins(), zoomLevel);

    paintInfo.context->save();
    
#ifndef BUILDING_ON_TIGER
    // On Leopard, the cell will draw outside of the given rect, so we have to clip to the rect
    paintInfo.context->clip(inflatedRect);
#endif

    if (zoomLevel != 1.0f) {
        inflatedRect.setWidth(inflatedRect.width() / zoomLevel);
        inflatedRect.setHeight(inflatedRect.height() / zoomLevel);
        paintInfo.context->translate(inflatedRect.x(), inflatedRect.y());
        paintInfo.context->scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context->translate(-inflatedRect.x(), -inflatedRect.y());
    }

    [popupButton drawWithFrame:IntRectToNSRect(inflatedRect) inView:nil];
    [popupButton setControlView:nil];

    paintInfo.context->restore();

    return false;
}

static const float baseFontSize = 11.0f;
static const float baseArrowHeight = 4.0f;
static const float baseArrowWidth = 5.0f;
static const float baseSpaceBetweenArrows = 2.0f;
static const int arrowPaddingLeft = 6;
static const int arrowPaddingRight = 6;
static const int paddingBeforeSeparator = 4;
static const int baseBorderRadius = 5;
static const int styledPopupPaddingLeft = 8;
static const int styledPopupPaddingTop = 1;
static const int styledPopupPaddingBottom = 2;

static void TopGradientInterpolate(void* info, const CGFloat* inData, CGFloat* outData)
{
    static float dark[4] = { 1.0f, 1.0f, 1.0f, 0.4f };
    static float light[4] = { 1.0f, 1.0f, 1.0f, 0.15f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

static void BottomGradientInterpolate(void* info, const CGFloat* inData, CGFloat* outData)
{
    static float dark[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
    static float light[4] = { 1.0f, 1.0f, 1.0f, 0.3f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

static void MainGradientInterpolate(void* info, const CGFloat* inData, CGFloat* outData)
{
    static float dark[4] = { 0.0f, 0.0f, 0.0f, 0.15f };
    static float light[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

static void TrackGradientInterpolate(void* info, const CGFloat* inData, CGFloat* outData)
{
    static float dark[4] = { 0.0f, 0.0f, 0.0f, 0.678f };
    static float light[4] = { 0.0f, 0.0f, 0.0f, 0.13f };
    float a = inData[0];
    int i = 0;
    for (i = 0; i < 4; i++)
        outData[i] = (1.0f - a) * dark[i] + a * light[i];
}

void RenderThemeChromiumMac::paintMenuListButtonGradients(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    CGContextRef context = paintInfo.context->platformContext();

    paintInfo.context->save();

    int radius = o->style()->borderTopLeftRadius().width();

    RetainPtr<CGColorSpaceRef> cspace(AdoptCF, CGColorSpaceCreateDeviceRGB());

    FloatRect topGradient(r.x(), r.y(), r.width(), r.height() / 2.0f);
    struct CGFunctionCallbacks topCallbacks = { 0, TopGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> topFunction(AdoptCF, CGFunctionCreate(NULL, 1, NULL, 4, NULL, &topCallbacks));
    RetainPtr<CGShadingRef> topShading(AdoptCF, CGShadingCreateAxial(cspace.get(), CGPointMake(topGradient.x(), topGradient.y()), CGPointMake(topGradient.x(), topGradient.bottom()), topFunction.get(), false, false));

    FloatRect bottomGradient(r.x() + radius, r.y() + r.height() / 2.0f, r.width() - 2.0f * radius, r.height() / 2.0f);
    struct CGFunctionCallbacks bottomCallbacks = { 0, BottomGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> bottomFunction(AdoptCF, CGFunctionCreate(NULL, 1, NULL, 4, NULL, &bottomCallbacks));
    RetainPtr<CGShadingRef> bottomShading(AdoptCF, CGShadingCreateAxial(cspace.get(), CGPointMake(bottomGradient.x(),  bottomGradient.y()), CGPointMake(bottomGradient.x(), bottomGradient.bottom()), bottomFunction.get(), false, false));

    struct CGFunctionCallbacks mainCallbacks = { 0, MainGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> mainFunction(AdoptCF, CGFunctionCreate(NULL, 1, NULL, 4, NULL, &mainCallbacks));
    RetainPtr<CGShadingRef> mainShading(AdoptCF, CGShadingCreateAxial(cspace.get(), CGPointMake(r.x(),  r.y()), CGPointMake(r.x(), r.bottom()), mainFunction.get(), false, false));

    RetainPtr<CGShadingRef> leftShading(AdoptCF, CGShadingCreateAxial(cspace.get(), CGPointMake(r.x(),  r.y()), CGPointMake(r.x() + radius, r.y()), mainFunction.get(), false, false));

    RetainPtr<CGShadingRef> rightShading(AdoptCF, CGShadingCreateAxial(cspace.get(), CGPointMake(r.right(),  r.y()), CGPointMake(r.right() - radius, r.y()), mainFunction.get(), false, false));
    paintInfo.context->save();
    CGContextClipToRect(context, r);
    paintInfo.context->addRoundedRectClip(r,
        o->style()->borderTopLeftRadius(), o->style()->borderTopRightRadius(),
        o->style()->borderBottomLeftRadius(), o->style()->borderBottomRightRadius());
    CGContextDrawShading(context, mainShading.get());
    paintInfo.context->restore();

    paintInfo.context->save();
    CGContextClipToRect(context, topGradient);
    paintInfo.context->addRoundedRectClip(enclosingIntRect(topGradient),
        o->style()->borderTopLeftRadius(), o->style()->borderTopRightRadius(),
        IntSize(), IntSize());
    CGContextDrawShading(context, topShading.get());
    paintInfo.context->restore();

    paintInfo.context->save();
    CGContextClipToRect(context, bottomGradient);
    paintInfo.context->addRoundedRectClip(enclosingIntRect(bottomGradient),
        IntSize(), IntSize(),
        o->style()->borderBottomLeftRadius(), o->style()->borderBottomRightRadius());
    CGContextDrawShading(context, bottomShading.get());
    paintInfo.context->restore();

    paintInfo.context->save();
    CGContextClipToRect(context, r);
    paintInfo.context->addRoundedRectClip(r,
        o->style()->borderTopLeftRadius(), o->style()->borderTopRightRadius(),
        o->style()->borderBottomLeftRadius(), o->style()->borderBottomRightRadius());
    CGContextDrawShading(context, leftShading.get());
    CGContextDrawShading(context, rightShading.get());
    paintInfo.context->restore();

    paintInfo.context->restore();
}

bool RenderThemeChromiumMac::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    paintInfo.context->save();

    IntRect bounds = IntRect(r.x() + o->style()->borderLeftWidth(),
                             r.y() + o->style()->borderTopWidth(),
                             r.width() - o->style()->borderLeftWidth() - o->style()->borderRightWidth(),
                             r.height() - o->style()->borderTopWidth() - o->style()->borderBottomWidth());
    // Draw the gradients to give the styled popup menu a button appearance
    paintMenuListButtonGradients(o, paintInfo, bounds);

    // Since we actually know the size of the control here, we restrict the font scale to make sure the arrows will fit vertically in the bounds
    float fontScale = min(o->style()->fontSize() / baseFontSize, bounds.height() / (baseArrowHeight * 2 + baseSpaceBetweenArrows));
    float centerY = bounds.y() + bounds.height() / 2.0f;
    float arrowHeight = baseArrowHeight * fontScale;
    float arrowWidth = baseArrowWidth * fontScale;
    float leftEdge = bounds.right() - arrowPaddingRight * o->style()->effectiveZoom() - arrowWidth;
    float spaceBetweenArrows = baseSpaceBetweenArrows * fontScale;

    if (bounds.width() < arrowWidth + arrowPaddingLeft * o->style()->effectiveZoom())
        return false;
    
    paintInfo.context->setFillColor(o->style()->color());
    paintInfo.context->setStrokeStyle(NoStroke);

    FloatPoint arrow1[3];
    arrow1[0] = FloatPoint(leftEdge, centerY - spaceBetweenArrows / 2.0f);
    arrow1[1] = FloatPoint(leftEdge + arrowWidth, centerY - spaceBetweenArrows / 2.0f);
    arrow1[2] = FloatPoint(leftEdge + arrowWidth / 2.0f, centerY - spaceBetweenArrows / 2.0f - arrowHeight);

    // Draw the top arrow
    paintInfo.context->drawConvexPolygon(3, arrow1, true);

    FloatPoint arrow2[3];
    arrow2[0] = FloatPoint(leftEdge, centerY + spaceBetweenArrows / 2.0f);
    arrow2[1] = FloatPoint(leftEdge + arrowWidth, centerY + spaceBetweenArrows / 2.0f);
    arrow2[2] = FloatPoint(leftEdge + arrowWidth / 2.0f, centerY + spaceBetweenArrows / 2.0f + arrowHeight);

    // Draw the bottom arrow
    paintInfo.context->drawConvexPolygon(3, arrow2, true);

    Color leftSeparatorColor(0, 0, 0, 40);
    Color rightSeparatorColor(255, 255, 255, 40);

    // FIXME: Should the separator thickness and space be scaled up by fontScale?
    int separatorSpace = 2; // Deliberately ignores zoom since it looks nicer if it stays thin.
    int leftEdgeOfSeparator = static_cast<int>(leftEdge - arrowPaddingLeft * o->style()->effectiveZoom()); // FIXME: Round?

    // Draw the separator to the left of the arrows
    paintInfo.context->setStrokeThickness(1.0f); // Deliberately ignores zoom since it looks nicer if it stays thin.
    paintInfo.context->setStrokeStyle(SolidStroke);
    paintInfo.context->setStrokeColor(leftSeparatorColor);
    paintInfo.context->drawLine(IntPoint(leftEdgeOfSeparator, bounds.y()),
                                IntPoint(leftEdgeOfSeparator, bounds.bottom()));

    paintInfo.context->setStrokeColor(rightSeparatorColor);
    paintInfo.context->drawLine(IntPoint(leftEdgeOfSeparator + separatorSpace, bounds.y()),
                                IntPoint(leftEdgeOfSeparator + separatorSpace, bounds.bottom()));

    paintInfo.context->restore();
    return false;
}

static const IntSize* menuListButtonSizes()
{
    static const IntSize sizes[3] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) };
    return sizes;
}

void RenderThemeChromiumMac::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    NSControlSize controlSize = controlSizeForFont(style);

    style->resetBorder();
    style->resetPadding();
    
    // Height is locked to auto.
    style->setHeight(Length(Auto));

    // White-space is locked to pre
    style->setWhiteSpace(PRE);

    // Set the foreground color to black or gray when we have the aqua look.
    // Cast to RGB32 is to work around a compiler bug.
    style->setColor(e && e->isEnabledFormControl() ? static_cast<RGBA32>(Color::black) : Color::darkGray);

    // Set the button's vertical size.
    setSizeFromFont(style, menuListButtonSizes());

    // Our font is locked to the appropriate system font size for the control.  To clarify, we first use the CSS-specified font to figure out
    // a reasonable control size, but once that control size is determined, we throw that font away and use the appropriate
    // system font for the control size instead.
    setFontFromControlSize(selector, style, controlSize);

    style->setBoxShadow(0);
}

int RenderThemeChromiumMac::popupInternalPaddingLeft(RenderStyle* style) const
{
    if (style->appearance() == MenulistPart)
        return popupButtonPadding(controlSizeForFont(style))[LeftPadding] * style->effectiveZoom();
    if (style->appearance() == MenulistButtonPart)
        return styledPopupPaddingLeft * style->effectiveZoom();
    return 0;
}

int RenderThemeChromiumMac::popupInternalPaddingRight(RenderStyle* style) const
{
    if (style->appearance() == MenulistPart)
        return popupButtonPadding(controlSizeForFont(style))[RightPadding] * style->effectiveZoom();
    if (style->appearance() == MenulistButtonPart) {
        float fontScale = style->fontSize() / baseFontSize;
        float arrowWidth = baseArrowWidth * fontScale;
        return static_cast<int>(ceilf(arrowWidth + (arrowPaddingLeft + arrowPaddingRight + paddingBeforeSeparator) * style->effectiveZoom()));
    }
    return 0;
}

int RenderThemeChromiumMac::popupInternalPaddingTop(RenderStyle* style) const
{
    if (style->appearance() == MenulistPart)
        return popupButtonPadding(controlSizeForFont(style))[TopPadding] * style->effectiveZoom();
    if (style->appearance() == MenulistButtonPart)
        return styledPopupPaddingTop * style->effectiveZoom();
    return 0;
}

int RenderThemeChromiumMac::popupInternalPaddingBottom(RenderStyle* style) const
{
    if (style->appearance() == MenulistPart)
        return popupButtonPadding(controlSizeForFont(style))[BottomPadding] * style->effectiveZoom();
    if (style->appearance() == MenulistButtonPart)
        return styledPopupPaddingBottom * style->effectiveZoom();
    return 0;
}

void RenderThemeChromiumMac::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    float fontScale = style->fontSize() / baseFontSize;

    style->resetPadding();
    style->setBorderRadius(IntSize(int(baseBorderRadius + fontScale - 1), int(baseBorderRadius + fontScale - 1))); // FIXME: Round up?

    const int minHeight = 15;
    style->setMinHeight(Length(minHeight, Fixed));
    
    style->setLineHeight(RenderStyle::initialLineHeight());
}

void RenderThemeChromiumMac::setPopupButtonCellState(const RenderObject* o, const IntRect& r)
{
    NSPopUpButtonCell* popupButton = this->popupButton();

    // Set the control size based off the rectangle we're painting into.
    setControlSize(popupButton, popupButtonSizes(), r.size(), o->style()->effectiveZoom());

    // Update the various states we respond to.
    updateActiveState(popupButton, o);
    updateCheckedState(popupButton, o);
    updateEnabledState(popupButton, o);
    updatePressedState(popupButton, o);
    updateFocusedState(popupButton, o);
}

const IntSize* RenderThemeChromiumMac::menuListSizes() const
{
    static const IntSize sizes[3] = { IntSize(9, 0), IntSize(5, 0), IntSize(0, 0) };
    return sizes;
}

int RenderThemeChromiumMac::minimumMenuListSize(RenderStyle* style) const
{
    return sizeForSystemFont(style, menuListSizes()).width();
}

static Image* mediaSliderThumbImage()
{
    static Image* mediaSliderThumb = Image::loadPlatformResource("mediaSliderThumb").releaseRef();
    return mediaSliderThumb;
}
  
static Image* mediaVolumeSliderThumbImage()
{
    static Image* mediaVolumeSliderThumb = Image::loadPlatformResource("mediaVolumeSliderThumb").releaseRef();
    return mediaVolumeSliderThumb;
}
  
void RenderThemeChromiumMac::adjustSliderTrackStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->setBoxShadow(0);
}

bool RenderThemeChromiumMac::paintSliderTrack(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    static const int trackWidth = 5;
    static const int trackRadius = 2;
  
    IntRect bounds = r;
    float zoomLevel = o->style()->effectiveZoom();
    float zoomedTrackWidth = trackWidth * zoomLevel;

    if (o->style()->appearance() ==  SliderHorizontalPart || o->style()->appearance() ==  MediaSliderPart) {
        bounds.setHeight(zoomedTrackWidth);
        bounds.setY(r.y() + r.height() / 2 - zoomedTrackWidth / 2);
    } else if (o->style()->appearance() == SliderVerticalPart) {
        bounds.setWidth(zoomedTrackWidth);
        bounds.setX(r.x() + r.width() / 2 - zoomedTrackWidth / 2);
    }

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    CGContextRef context = paintInfo.context->platformContext();
    RetainPtr<CGColorSpaceRef> cspace(AdoptCF, CGColorSpaceCreateDeviceRGB());

    paintInfo.context->save();
    CGContextClipToRect(context, bounds);

    struct CGFunctionCallbacks mainCallbacks = { 0, TrackGradientInterpolate, NULL };
    RetainPtr<CGFunctionRef> mainFunction(AdoptCF, CGFunctionCreate(NULL, 1, NULL, 4, NULL, &mainCallbacks));
    RetainPtr<CGShadingRef> mainShading;
    if (o->style()->appearance() == SliderVerticalPart)
        mainShading.adoptCF(CGShadingCreateAxial(cspace.get(), CGPointMake(bounds.x(),  bounds.bottom()), CGPointMake(bounds.right(), bounds.bottom()), mainFunction.get(), false, false));
    else
        mainShading.adoptCF(CGShadingCreateAxial(cspace.get(), CGPointMake(bounds.x(),  bounds.y()), CGPointMake(bounds.x(), bounds.bottom()), mainFunction.get(), false, false));

    IntSize radius(trackRadius, trackRadius);
    paintInfo.context->addRoundedRectClip(bounds,
        radius, radius,
        radius, radius);
    CGContextDrawShading(context, mainShading.get());
    paintInfo.context->restore();
    
    return false;
}

void RenderThemeChromiumMac::adjustSliderThumbStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->setBoxShadow(0);
}

static const float verticalSliderHeightPadding = 0.1f;

bool RenderThemeChromiumMac::paintSliderThumb(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    ASSERT(o->parent()->isSlider());

    NSSliderCell* sliderThumbCell = o->style()->appearance() == SliderThumbVerticalPart
        ? sliderThumbVertical()
        : sliderThumbHorizontal();

    LocalCurrentGraphicsContext localContext(paintInfo.context);

    // Update the various states we respond to.
    updateActiveState(sliderThumbCell, o);
    updateEnabledState(sliderThumbCell, o->parent());
    updateFocusedState(sliderThumbCell, o->parent());

    // Update the pressed state using the NSCell tracking methods, since that's how NSSliderCell keeps track of it.
    bool oldPressed;
    if (o->style()->appearance() == SliderThumbVerticalPart)
        oldPressed = m_isSliderThumbVerticalPressed;
    else
        oldPressed = m_isSliderThumbHorizontalPressed;

    bool pressed = toRenderSlider(o->parent())->inDragMode();

    if (o->style()->appearance() == SliderThumbVerticalPart)
        m_isSliderThumbVerticalPressed = pressed;
    else
        m_isSliderThumbHorizontalPressed = pressed;

    if (pressed != oldPressed) {
        if (pressed)
            [sliderThumbCell startTrackingAt:NSPoint() inView:nil];
        else
            [sliderThumbCell stopTracking:NSPoint() at:NSPoint() inView:nil mouseIsUp:YES];
    }

    FloatRect bounds = r;
    // Make the height of the vertical slider slightly larger so NSSliderCell will draw a vertical slider.
    if (o->style()->appearance() == SliderThumbVerticalPart)
        bounds.setHeight(bounds.height() + verticalSliderHeightPadding * o->style()->effectiveZoom());

    paintInfo.context->save();
    float zoomLevel = o->style()->effectiveZoom();
    
    FloatRect unzoomedRect = bounds;
    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    [sliderThumbCell drawWithFrame:FloatRectToNSRect(unzoomedRect) inView:nil];
    [sliderThumbCell setControlView:nil];

    paintInfo.context->restore();

    return false;
}

const int sliderThumbWidth = 15;
const int sliderThumbHeight = 15;

void RenderThemeChromiumMac::adjustSliderThumbSize(RenderObject* o) const
{
    float zoomLevel = o->style()->effectiveZoom();
    if (o->style()->appearance() == SliderThumbHorizontalPart || o->style()->appearance() == SliderThumbVerticalPart) {
        o->style()->setWidth(Length(static_cast<int>(sliderThumbWidth * zoomLevel), Fixed));
        o->style()->setHeight(Length(static_cast<int>(sliderThumbHeight * zoomLevel), Fixed));
    }

#if ENABLE(VIDEO)
    Image* thumbImage = 0;
    if (o->style()->appearance() == MediaSliderThumbPart)
        thumbImage = mediaSliderThumbImage();
    else if (o->style()->appearance() == MediaVolumeSliderThumbPart)
        thumbImage = mediaVolumeSliderThumbImage();
  
    if (thumbImage) {
        o->style()->setWidth(Length(thumbImage->width(), Fixed));
        o->style()->setHeight(Length(thumbImage->height(), Fixed));
    }
#endif
}

bool RenderThemeChromiumMac::paintSearchField(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    NSSearchFieldCell* search = this->search();
    LocalCurrentGraphicsContext localContext(paintInfo.context);

    setSearchCellState(o, r);

    paintInfo.context->save();

    float zoomLevel = o->style()->effectiveZoom();

    IntRect unzoomedRect = r;
    
    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    // Set the search button to nil before drawing.  Then reset it so we can draw it later.
    [search setSearchButtonCell:nil];

    [search drawWithFrame:NSRect(IntRectToNSRect(unzoomedRect)) inView:nil];
#ifdef BUILDING_ON_TIGER
    if ([search showsFirstResponder])
        wkDrawTextFieldCellFocusRing(search, NSRect(unzoomedRect));
#endif

    [search setControlView:nil];
    [search resetSearchButtonCell];

    paintInfo.context->restore();

    return false;
}

void RenderThemeChromiumMac::setSearchCellState(RenderObject* o, const IntRect& r)
{
    NSSearchFieldCell* search = this->search();

    [search setControlSize:controlSizeForFont(o->style())];

    // Update the various states we respond to.
    updateActiveState(search, o);
    updateEnabledState(search, o);
    updateFocusedState(search, o);
}

const IntSize* RenderThemeChromiumMac::searchFieldSizes() const
{
    static const IntSize sizes[3] = { IntSize(0, 22), IntSize(0, 19), IntSize(0, 17) };
    return sizes;
}

void RenderThemeChromiumMac::setSearchFieldSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;
    
    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style, searchFieldSizes());
}

void RenderThemeChromiumMac::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // Override border.
    style->resetBorder();
    const short borderWidth = 2 * style->effectiveZoom();
    style->setBorderLeftWidth(borderWidth);
    style->setBorderLeftStyle(INSET);
    style->setBorderRightWidth(borderWidth);
    style->setBorderRightStyle(INSET);
    style->setBorderBottomWidth(borderWidth);
    style->setBorderBottomStyle(INSET);
    style->setBorderTopWidth(borderWidth);
    style->setBorderTopStyle(INSET);    
    
    // Override height.
    style->setHeight(Length(Auto));
    setSearchFieldSize(style);
    
    // Override padding size to match AppKit text positioning.
    const int padding = 1 * style->effectiveZoom();
    style->setPaddingLeft(Length(padding, Fixed));
    style->setPaddingRight(Length(padding, Fixed));
    style->setPaddingTop(Length(padding, Fixed));
    style->setPaddingBottom(Length(padding, Fixed));
    
    NSControlSize controlSize = controlSizeForFont(style);
    setFontFromControlSize(selector, style, controlSize);

    style->setBoxShadow(0);
}

bool RenderThemeChromiumMac::paintSearchFieldCancelButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);

    Node* input = o->node()->shadowAncestorNode();
    setSearchCellState(input->renderer(), r);

    NSSearchFieldCell* search = this->search();

    updateActiveState([search cancelButtonCell], o);
    updatePressedState([search cancelButtonCell], o);

    paintInfo.context->save();

    float zoomLevel = o->style()->effectiveZoom();

    NSRect bounds = [search cancelButtonRectForBounds:NSRect(IntRectToNSRect(input->renderer()->absoluteBoundingBoxRect()))];
    
    IntRect unzoomedRect(NSRectToIntRect(bounds));
    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    [[search cancelButtonCell] drawWithFrame:IntRectToNSRect(unzoomedRect) inView:nil];
    [[search cancelButtonCell] setControlView:nil];

    paintInfo.context->restore();
    return false;
}

const IntSize* RenderThemeChromiumMac::cancelButtonSizes() const
{
    static const IntSize sizes[3] = { IntSize(16, 13), IntSize(13, 11), IntSize(13, 9) };
    return sizes;
}

void RenderThemeChromiumMac::adjustSearchFieldCancelButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    IntSize size = sizeForSystemFont(style, cancelButtonSizes());
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
    style->setBoxShadow(0);
}

const IntSize* RenderThemeChromiumMac::resultsButtonSizes() const
{
    static const IntSize sizes[3] = { IntSize(19, 13), IntSize(17, 11), IntSize(17, 9) };
    return sizes;
}

static const int emptyResultsOffset = 9;
void RenderThemeChromiumMac::adjustSearchFieldDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style->setWidth(Length(size.width() - emptyResultsOffset, Fixed));
    style->setHeight(Length(size.height(), Fixed));
    style->setBoxShadow(0);
}

bool RenderThemeChromiumMac::paintSearchFieldDecoration(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    return false;
}

void RenderThemeChromiumMac::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
    style->setBoxShadow(0);
}

bool RenderThemeChromiumMac::paintSearchFieldResultsDecoration(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);

    Node* input = o->node()->shadowAncestorNode();
    setSearchCellState(input->renderer(), r);

    NSSearchFieldCell* search = this->search();

    updateActiveState([search searchButtonCell], o);

    if ([search searchMenuTemplate] != nil)
        [search setSearchMenuTemplate:nil];

    NSRect bounds = [search searchButtonRectForBounds:NSRect(IntRectToNSRect(input->renderer()->absoluteBoundingBoxRect()))];
    [[search searchButtonCell] drawWithFrame:bounds inView:nil];
    [[search searchButtonCell] setControlView:nil];
    return false;
}

static const int resultsArrowWidth = 5;
void RenderThemeChromiumMac::adjustSearchFieldResultsButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style->setWidth(Length(size.width() + resultsArrowWidth, Fixed));
    style->setHeight(Length(size.height(), Fixed));
    style->setBoxShadow(0);
}

bool RenderThemeChromiumMac::paintSearchFieldResultsButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);

    Node* input = o->node()->shadowAncestorNode();
    setSearchCellState(input->renderer(), r);

    NSSearchFieldCell* search = this->search();

    updateActiveState([search searchButtonCell], o);

    if (![search searchMenuTemplate])
        [search setSearchMenuTemplate:searchMenuTemplate()];

    paintInfo.context->save();

    float zoomLevel = o->style()->effectiveZoom();

    NSRect bounds = [search searchButtonRectForBounds:NSRect(IntRectToNSRect(input->renderer()->absoluteBoundingBoxRect()))];
    
    IntRect unzoomedRect(NSRectToIntRect(bounds));
    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    [[search searchButtonCell] drawWithFrame:IntRectToNSRect(unzoomedRect) inView:nil];
    [[search searchButtonCell] setControlView:nil];
    
    paintInfo.context->restore();

    return false;
}

bool RenderThemeChromiumMac::paintMediaButtonInternal(GraphicsContext* context, const IntRect& rect, Image* image)
{
    // Create a destination rectangle for the image that is centered in the drawing rectangle, rounded left, and down.
    IntRect imageRect = image->rect();
    imageRect.setY(rect.y() + (rect.height() - image->height() + 1) / 2);
    imageRect.setX(rect.x() + (rect.width() - image->width() + 1) / 2);

    context->drawImage(image, imageRect);
    return true;
}

bool RenderThemeChromiumMac::paintMediaPlayButton(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = mediaElementParent(object->node());
    if (!mediaElement)
        return false;

    static Image* mediaPlay = Image::loadPlatformResource("mediaPlay").releaseRef();
    static Image* mediaPause = Image::loadPlatformResource("mediaPause").releaseRef();
    static Image* mediaPlayDisabled = Image::loadPlatformResource("mediaPlayDisabled").releaseRef();

    if (mediaElement->networkState() == HTMLMediaElement::NETWORK_NO_SOURCE)
        return paintMediaButtonInternal(paintInfo.context, rect, mediaPlayDisabled);

    return paintMediaButtonInternal(paintInfo.context, rect, mediaElement->paused() ? mediaPlay : mediaPause);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumMac::paintMediaMuteButton(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = mediaElementParent(object->node());
    if (!mediaElement)
        return false;

    static Image* soundFull = Image::loadPlatformResource("mediaSoundFull").releaseRef();
    static Image* soundNone = Image::loadPlatformResource("mediaSoundNone").releaseRef();
    static Image* soundDisabled = Image::loadPlatformResource("mediaSoundDisabled").releaseRef();

    if (mediaElement->networkState() == HTMLMediaElement::NETWORK_NO_SOURCE || !mediaElement->hasAudio())
        return paintMediaButtonInternal(paintInfo.context, rect, soundDisabled);

    return paintMediaButtonInternal(paintInfo.context, rect, mediaElement->muted() ? soundNone : soundFull);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumMac::paintMediaSliderTrack(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = mediaElementParent(object->node());
    if (!mediaElement)
        return false;

    RenderStyle* style = object->style();    
    GraphicsContext* context = paintInfo.context;
    context->save();
    
    context->setShouldAntialias(true);

    IntSize topLeftRadius = style->borderTopLeftRadius();
    IntSize topRightRadius = style->borderTopRightRadius();
    IntSize bottomLeftRadius = style->borderBottomLeftRadius();
    IntSize bottomRightRadius = style->borderBottomRightRadius();
    float borderWidth = style->borderLeftWidth();

    // Draw the border of the time bar.
    context->setStrokeColor(style->borderLeftColor());
    context->setStrokeThickness(borderWidth);
    context->setFillColor(style->backgroundColor());
    context->addPath(Path::createRoundedRectangle(rect, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius));
    context->drawPath();
    
    // Draw the buffered ranges.
    // FIXME: Draw multiple ranges if there are multiple buffered ranges.
    FloatRect bufferedRect = rect;
    bufferedRect.inflate(-1.0 - borderWidth);
    bufferedRect.setWidth(bufferedRect.width() * mediaElement->percentLoaded());
    bufferedRect = context->roundToDevicePixels(bufferedRect);
   
    // Don't bother drawing an empty area.
    if (bufferedRect.width() > 0 && bufferedRect.height() > 0) {
        FloatPoint sliderTopLeft = bufferedRect.location();
        FloatPoint sliderTopRight = sliderTopLeft;
        p1.move(0.0f, bufferedRect.height());
        
        RefPtr<Gradient> gradient = Gradient::create(sliderTopLeft, sliderTopRight);
        Color startColor = object->style()->color();
        gradient->addColorStop(0.0, startColor);
        gradient->addColorStop(1.0, Color(startColor.red() / 2, startColor.green() / 2, startColor.blue() / 2, startColor.alpha()));
 
        context->setFillGradient(gradient);
        context->addPath(Path::createRoundedRectangle(bufferedRect, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius));
        context->fillPath();
    }
    
    context->restore();    
    return true;
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumMac::paintMediaVolumeSliderTrack(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = mediaElementParent(object->node());
    if (!mediaElement)
        return false;

    GraphicsContext* context = paintInfo.context;
    Color originalColor = context->strokeColor();
    if (originalColor != Color::white)
        context->setStrokeColor(Color::white);

    int x = rect.x() + rect.width() / 2;
    context->drawLine(IntPoint(x, rect.y()),  IntPoint(x, rect.y() + rect.height()));
    
    if (originalColor != Color::white)
        context->setStrokeColor(originalColor);
    return true;
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumMac::paintMediaSliderThumb(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    if (!object->parent()->isSlider())
        return false;

    return paintMediaButtonInternal(paintInfo.context, rect, mediaSliderThumbImage());
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumMac::paintMediaVolumeSliderThumb(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    if (!object->parent()->isSlider())
        return false;

    return paintMediaButtonInternal(paintInfo.context, rect, mediaVolumeSliderThumbImage());
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

bool RenderThemeChromiumMac::paintMediaControlsBackground(RenderObject* object, const RenderObject::PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = mediaElementParent(object->node());
    if (!mediaElement)
        return false;

    if (!rect.isEmpty())
    {
        GraphicsContext* context = paintInfo.context;
        Color originalColor = context->strokeColor();
 
        // Draws the left border, it is always 1px wide.
        context->setStrokeColor(object->style()->borderLeftColor());
        context->drawLine(IntPoint(rect.x() + 1, rect.y()),
                          IntPoint(rect.x() + 1, rect.y() + rect.height()));
                         

        // Draws the right border, it is always 1px wide.
        context->setStrokeColor(object->style()->borderRightColor());
        context->drawLine(IntPoint(rect.x() + rect.width() - 1, rect.y()),
                          IntPoint(rect.x() + rect.width() - 1, rect.y() + rect.height()));
        
        context->setStrokeColor(originalColor);
    }
    return true;
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
NSButtonCell* RenderThemeChromiumMac::checkbox() const
{
    if (!m_checkbox) {
        m_checkbox.adoptNS([[NSButtonCell alloc] init]);
        [m_checkbox.get() setButtonType:NSSwitchButton];
        [m_checkbox.get() setTitle:nil];
        [m_checkbox.get() setAllowsMixedState:YES];
        [m_checkbox.get() setFocusRingType:NSFocusRingTypeExterior];
    }
    
    return m_checkbox.get();
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
NSButtonCell* RenderThemeChromiumMac::radio() const
{
    if (!m_radio) {
        m_radio.adoptNS([[NSButtonCell alloc] init]);
        [m_radio.get() setButtonType:NSRadioButton];
        [m_radio.get() setTitle:nil];
        [m_radio.get() setFocusRingType:NSFocusRingTypeExterior];
    }
    
    return m_radio.get();
}

// FIXME: This used to be in the upstream version until it was converted to the new theme API in r37731.
NSButtonCell* RenderThemeChromiumMac::button() const
{
    if (!m_button) {
        m_button.adoptNS([[NSButtonCell alloc] init]);
        [m_button.get() setTitle:nil];
        [m_button.get() setButtonType:NSMomentaryPushInButton];
    }
    
    return m_button.get();
}

NSPopUpButtonCell* RenderThemeChromiumMac::popupButton() const
{
    if (!m_popupButton) {
        m_popupButton.adoptNS([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
        [m_popupButton.get() setUsesItemFromMenu:NO];
        [m_popupButton.get() setFocusRingType:NSFocusRingTypeExterior];
    }
    
    return m_popupButton.get();
}

NSSearchFieldCell* RenderThemeChromiumMac::search() const
{
    if (!m_search) {
        m_search.adoptNS([[NSSearchFieldCell alloc] initTextCell:@""]);
        [m_search.get() setBezelStyle:NSTextFieldRoundedBezel];
        [m_search.get() setBezeled:YES];
        [m_search.get() setEditable:YES];
        [m_search.get() setFocusRingType:NSFocusRingTypeExterior];
    }

    return m_search.get();
}

NSMenu* RenderThemeChromiumMac::searchMenuTemplate() const
{
    if (!m_searchMenuTemplate)
        m_searchMenuTemplate.adoptNS([[NSMenu alloc] initWithTitle:@""]);

    return m_searchMenuTemplate.get();
}

NSSliderCell* RenderThemeChromiumMac::sliderThumbHorizontal() const
{
    if (!m_sliderThumbHorizontal) {
        m_sliderThumbHorizontal.adoptNS([[NSSliderCell alloc] init]);
        [m_sliderThumbHorizontal.get() setTitle:nil];
        [m_sliderThumbHorizontal.get() setSliderType:NSLinearSlider];
        [m_sliderThumbHorizontal.get() setControlSize:NSSmallControlSize];
        [m_sliderThumbHorizontal.get() setFocusRingType:NSFocusRingTypeExterior];
    }
    
    return m_sliderThumbHorizontal.get();
}

NSSliderCell* RenderThemeChromiumMac::sliderThumbVertical() const
{
    if (!m_sliderThumbVertical) {
        m_sliderThumbVertical.adoptNS([[NSSliderCell alloc] init]);
        [m_sliderThumbVertical.get() setTitle:nil];
        [m_sliderThumbVertical.get() setSliderType:NSLinearSlider];
        [m_sliderThumbVertical.get() setControlSize:NSSmallControlSize];
        [m_sliderThumbVertical.get() setFocusRingType:NSFocusRingTypeExterior];
    }
    
    return m_sliderThumbVertical.get();
}

} // namespace WebCore
