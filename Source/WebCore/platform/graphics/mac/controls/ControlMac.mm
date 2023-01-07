/*
 * Copyright (C) 2022 Apple Inc. All Rights Reserved.
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
#import "ControlMac.h"

#if PLATFORM(MAC)

#import "ColorCocoa.h"
#import "FloatRoundedRect.h"
#import "GraphicsContextCG.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "WebControlView.h"
#import <pal/spi/cocoa/NSButtonCellSPI.h>
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <pal/spi/mac/NSGraphicsSPI.h>

namespace WebCore {

ControlMac::ControlMac(ControlPart& owningPart, ControlFactoryMac& controlFactory)
    : PlatformControl(owningPart)
    , m_controlFactory(controlFactory)
{
}

bool ControlMac::userPrefersContrast()
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldIncreaseContrast];
}

FloatRect ControlMac::inflatedRect(const FloatRect& bounds, const FloatSize& size, const IntOutsets& outsets, const ControlStyle& style)
{
    auto scaledOutsets = FloatBoxExtent {
        outsets.top() * style.zoomFactor,
        outsets.right() * style.zoomFactor,
        outsets.bottom() * style.zoomFactor,
        outsets.left() * style.zoomFactor
    };

    auto inflatedRect = FloatRect { bounds.location(), size };
    inflatedRect.expand(scaledOutsets);
    return unionRect(bounds, inflatedRect);
}

void ControlMac::updateCheckedState(NSCell *cell, const ControlStyle& style)
{
    bool oldIndeterminate = [cell state] == NSControlStateValueMixed;
    bool indeterminate = style.states.contains(ControlStyle::State::Indeterminate);

    bool oldChecked = [cell state] == NSControlStateValueOn;
    bool checked = style.states.contains(ControlStyle::State::Checked);

    if (oldIndeterminate == indeterminate && oldChecked == checked)
        return;

    auto newState = indeterminate ? NSControlStateValueMixed : (checked ? NSControlStateValueOn : NSControlStateValueOff);

    if ([cell isKindOfClass:[NSButtonCell class]])
        [(NSButtonCell *)cell _setState:newState animated:false];
    else
        [cell setState:newState];
}

void ControlMac::updateEnabledState(NSCell *cell, const ControlStyle& style)
{
    bool oldEnabled = [cell isEnabled];
    bool enabled = style.states.contains(ControlStyle::State::Enabled);
    if (enabled == oldEnabled)
        return;
    [cell setEnabled:enabled];
}

void ControlMac::updateFocusedState(NSCell *cell, const ControlStyle& style)
{
    bool oldFocused = [cell showsFirstResponder];
    bool focused = style.states.contains(ControlStyle::State::Focused);
    if (focused == oldFocused)
        return;
    [cell setShowsFirstResponder:focused];
}

void ControlMac::updatePressedState(NSCell *cell, const ControlStyle& style)
{
    bool oldPressed = [cell isHighlighted];
    bool pressed = style.states.contains(ControlStyle::State::Pressed);
    if (pressed == oldPressed)
        return;

    if ([cell isKindOfClass:[NSButtonCell class]])
        [(NSButtonCell *)cell _setHighlighted:pressed animated:false];
    else
        [cell setHighlighted:pressed];
}

NSControlSize ControlMac::controlSizeForFont(const ControlStyle& style) const
{
    bool supportsLargeFormControls = style.states.contains(ControlStyle::State::LargeControls);
    if (style.fontSize >= 21 && supportsLargeFormControls)
        return NSControlSizeLarge;
    if (style.fontSize >= 16)
        return NSControlSizeRegular;
    if (style.fontSize >= 11)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

NSControlSize ControlMac::controlSizeForSystemFont(const ControlStyle& style) const
{
    bool supportsLargeFormControls = style.states.contains(ControlStyle::State::LargeControls);
    if (style.fontSize >= [NSFont systemFontSizeForControlSize:NSControlSizeLarge] && supportsLargeFormControls)
        return NSControlSizeLarge;
    if (style.fontSize >= [NSFont systemFontSizeForControlSize:NSControlSizeRegular])
        return NSControlSizeRegular;
    if (style.fontSize >= [NSFont systemFontSizeForControlSize:NSControlSizeSmall])
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

NSControlSize ControlMac::controlSizeForSize(const FloatSize& size, const ControlStyle& style) const
{
    if (style.states.contains(ControlStyle::State::LargeControls)
        && size.width() >= static_cast<int>(cellSize(NSControlSizeLarge, style).width() * style.zoomFactor)
        && size.height() >= static_cast<int>(cellSize(NSControlSizeLarge, style).height() * style.zoomFactor))
        return NSControlSizeLarge;

    if (size.width() >= static_cast<int>(cellSize(NSControlSizeRegular, style).width() * style.zoomFactor)
        && size.height() >= static_cast<int>(cellSize(NSControlSizeRegular, style).height() * style.zoomFactor))
        return NSControlSizeRegular;

    if (size.width() >= static_cast<int>(cellSize(NSControlSizeSmall, style).width() * style.zoomFactor)
        && size.height() >= static_cast<int>(cellSize(NSControlSizeSmall, style).height() * style.zoomFactor))
        return NSControlSizeSmall;

    return NSControlSizeMini;
}

IntSize ControlMac::sizeForSystemFont(const ControlStyle& style) const
{
    auto controlSize = controlSizeForSystemFont(style);
    auto size = cellSize(controlSize, style);
    size.scale(style.zoomFactor);
    return size;
}

void ControlMac::setFocusRingClipRect(const FloatRect& clipBounds)
{
    [WebControlView setClipBounds:clipBounds];
}

void ControlMac::updateCellStates(const FloatRect&, const ControlStyle& style)
{
    [WebControlWindow setHasKeyAppearance:style.states.contains(ControlStyle::State::WindowActive)];
}

static void drawFocusRing(NSRect cellFrame, NSCell *cell, NSView *view)
{
    CGContextRef cgContext = [[NSGraphicsContext currentContext] CGContext];

    CGContextStateSaver stateSaver(cgContext);

    CGFocusRingStyle focusRingStyle;
    NSInitializeCGFocusRingStyleForTime(NSFocusRingOnly, &focusRingStyle, std::numeric_limits<double>::max());

    // We want to respect the CGContext clipping and also not overpaint any
    // existing focus ring. The way to do this is set accumulate to
    // -1. According to CoreGraphics, the reasoning for this behavior has been
    // lost in time.
    focusRingStyle.accumulate = -1;

    // FIXME: This color should be shared with RenderThemeMac. For now just use the same NSColor color.
    // The color is expected to be opaque, since CoreGraphics will apply opacity when drawing (because opacity is normally animated).
    auto color = colorFromCocoaColor([NSColor keyboardFocusIndicatorColor]).opaqueColor();
    auto style = adoptCF(CGStyleCreateFocusRingWithColor(&focusRingStyle, cachedCGColor(color).get()));
    CGContextSetStyle(cgContext, style.get());

    CGContextBeginTransparencyLayerWithRect(cgContext, NSRectToCGRect(cellFrame), nullptr);
    [cell drawFocusRingMaskWithFrame:cellFrame inView:view];
    CGContextEndTransparencyLayer(cgContext);
}

static void drawCellOrFocusRing(GraphicsContext& context, const FloatRect& rect, const ControlStyle& style, NSCell *cell, NSView *view, bool drawCell)
{
    LocalCurrentGraphicsContext localContext(context);

    if (drawCell) {
        if ([cell isKindOfClass:[NSSliderCell class]]) {
            // For slider cells, draw only the knob.
            [(NSSliderCell *)cell drawKnob:rect];
        } else {
            [cell drawWithFrame:rect inView:view];
            [cell setControlView:nil];
        }
    }

    if (style.states.contains(ControlStyle::State::Focused))
        drawFocusRing(rect, cell, view);
}

void ControlMac::drawCell(GraphicsContext& context, const FloatRect& rect, float deviceScaleFactor, const ControlStyle& style, NSCell *cell, NSView *view, bool drawCell)
{
    auto platformContext = context.platformContext();
    auto userCTM = AffineTransform(CGAffineTransformConcat(CGContextGetCTM(platformContext), CGAffineTransformInvert(CGContextGetBaseCTM(platformContext))));
    bool useImageBuffer = userCTM.xScale() != 1.0 || userCTM.yScale() != 1.0;

    if (!useImageBuffer) {
        drawCellOrFocusRing(context, rect, style, cell, view, drawCell);
        return;
    }

    static constexpr float focusRingOutlineWidth = 3;
    auto focusRingPadding = FloatSize { focusRingOutlineWidth, focusRingOutlineWidth };

    auto cellDrawingRect = FloatRect { toFloatPoint(focusRingPadding), rect.size() };
    auto imageBufferSize = cellDrawingRect.size() + 2 * focusRingPadding;

    auto imageBuffer = context.createImageBuffer(imageBufferSize, deviceScaleFactor);
    if (!imageBuffer)
        return;

    drawCellOrFocusRing(imageBuffer->context(), cellDrawingRect, style, cell, view, drawCell);
    context.drawConsumingImageBuffer(WTFMove(imageBuffer), rect.location() - focusRingPadding);
}

#if ENABLE(DATALIST_ELEMENT)
void ControlMac::drawListButton(GraphicsContext& context, const FloatRect& rect, float deviceScaleFactor, const ControlStyle& style)
{
    if (!style.states.contains(ControlStyle::State::ListButton))
        return;

    // We can't paint an NSComboBoxCell since they are not height-resizable.
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    const FloatSize comboBoxSize { 40, 19 };
    const FloatSize comboBoxButtonSize { 16, 16 };
    const FloatPoint comboBoxButtonInset { 5, 1 };
    constexpr auto comboBoxButtonCornerRadii = 4;

    const FloatSize desiredComboBoxButtonSize { 12, 12 };
    constexpr auto desiredComboBoxInset = 2;

    auto comboBoxImageBuffer = context.createImageBuffer(comboBoxSize, deviceScaleFactor);
    if (!comboBoxImageBuffer)
        return;

    ContextContainer cgContextContainer(comboBoxImageBuffer->context());
    CGContextRef cgContext = cgContextContainer.context();

    NSString *coreUIState;
    if (style.states.containsAny({ ControlStyle::State::Presenting, ControlStyle::State::ListButtonPressed }))
        coreUIState = (__bridge NSString *)kCUIStatePressed;
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
    float listButtonY = rect.center().y() - desiredComboBoxButtonSize.height() / 2;
    if (style.states.contains(ControlStyle::State::RightToLeft))
        listButtonLocation = { rect.x() + desiredComboBoxInset, listButtonY };
    else
        listButtonLocation = { rect.maxX() - desiredComboBoxButtonSize.width() - desiredComboBoxInset, listButtonY };

    GraphicsContextStateSaver stateSaver(context);
    context.drawConsumingImageBuffer(WTFMove(comboBoxButtonImageBuffer), listButtonLocation);
}
#endif

} // namespace WebCore

#endif // PLATFORM(MAC)
