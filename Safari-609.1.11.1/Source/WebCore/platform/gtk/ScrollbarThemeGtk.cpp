/*
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "ScrollbarThemeGtk.h"

#include "GRefPtrGtk.h"
#include "PlatformContextCairo.h"
#include "PlatformMouseEvent.h"
#include "RenderThemeWidget.h"
#include "ScrollView.h"
#include "Scrollbar.h"
#include <cstdlib>
#include <gtk/gtk.h>

namespace WebCore {

ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeGtk theme;
    return theme;
}

ScrollbarThemeGtk::~ScrollbarThemeGtk() = default;

static void themeChangedCallback()
{
    ScrollbarTheme::theme().themeChanged();
}

ScrollbarThemeGtk::ScrollbarThemeGtk()
{
    m_usesOverlayScrollbars = g_strcmp0(g_getenv("GTK_OVERLAY_SCROLLING"), "0");

    static bool themeMonitorInitialized = false;
    if (!themeMonitorInitialized) {
        g_signal_connect(gtk_settings_get_default(), "notify::gtk-theme-name", G_CALLBACK(themeChangedCallback), nullptr);
        themeMonitorInitialized = true;
        updateThemeProperties();
    }
}

void ScrollbarThemeGtk::themeChanged()
{
    RenderThemeWidget::clearCache();
    updateThemeProperties();
}

void ScrollbarThemeGtk::updateThemeProperties()
{
    auto& scrollbar = static_cast<RenderThemeScrollbar&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::VerticalScrollbarRight));
    m_hasBackButtonStartPart = scrollbar.stepper(RenderThemeScrollbarGadget::Steppers::Backward);
    m_hasForwardButtonEndPart = scrollbar.stepper(RenderThemeScrollbarGadget::Steppers::Forward);
    m_hasBackButtonEndPart = scrollbar.stepper(RenderThemeScrollbarGadget::Steppers::SecondaryBackward);
    m_hasForwardButtonStartPart = scrollbar.stepper(RenderThemeScrollbarGadget::Steppers::SecondaryForward);
}

bool ScrollbarThemeGtk::hasButtons(Scrollbar& scrollbar)
{
    return scrollbar.enabled() && (m_hasBackButtonStartPart || m_hasForwardButtonEndPart || m_hasBackButtonEndPart || m_hasForwardButtonStartPart);
}

static GtkStateFlags scrollbarPartStateFlags(Scrollbar& scrollbar, ScrollbarPart part, bool painting = false)
{
    unsigned stateFlags = 0;
    switch (part) {
    case AllParts:
        if (!painting || scrollbar.hoveredPart() != NoPart)
            stateFlags |= GTK_STATE_FLAG_PRELIGHT;
        break;
    case BackTrackPart:
    case ForwardTrackPart:
        if (scrollbar.hoveredPart() == BackTrackPart || scrollbar.hoveredPart() == ForwardTrackPart)
            stateFlags |= GTK_STATE_FLAG_PRELIGHT;
        if (scrollbar.pressedPart() == BackTrackPart || scrollbar.pressedPart() == ForwardTrackPart)
            stateFlags |= GTK_STATE_FLAG_ACTIVE;
        break;
    case BackButtonStartPart:
    case ForwardButtonStartPart:
    case BackButtonEndPart:
    case ForwardButtonEndPart:
        if (((part == BackButtonStartPart || part == BackButtonEndPart) && !scrollbar.currentPos())
            || ((part == ForwardButtonEndPart || part == ForwardButtonStartPart) && scrollbar.currentPos() == scrollbar.maximum())) {
            stateFlags |= GTK_STATE_FLAG_INSENSITIVE;
            break;
        }
        FALLTHROUGH;
    default:
        if (scrollbar.hoveredPart() == part)
            stateFlags |= GTK_STATE_FLAG_PRELIGHT;

        if (scrollbar.pressedPart() == part)
            stateFlags |= GTK_STATE_FLAG_ACTIVE;
        break;
    }

    return static_cast<GtkStateFlags>(stateFlags);
}

static RenderThemeWidget::Type widgetTypeForScrollbar(Scrollbar& scrollbar, GtkStateFlags scrollbarState)
{
    if (scrollbar.orientation() == VerticalScrollbar) {
        if (scrollbar.scrollableArea().shouldPlaceBlockDirectionScrollbarOnLeft())
            return scrollbarState & GTK_STATE_FLAG_PRELIGHT ? RenderThemeWidget::Type::VerticalScrollbarLeft : RenderThemeWidget::Type::VerticalScrollIndicatorLeft;
        return scrollbarState & GTK_STATE_FLAG_PRELIGHT ? RenderThemeWidget::Type::VerticalScrollbarRight : RenderThemeWidget::Type::VerticalScrollIndicatorRight;
    }
    return scrollbarState & GTK_STATE_FLAG_PRELIGHT ? RenderThemeWidget::Type::HorizontalScrollbar : RenderThemeWidget::Type::HorizontalScrollIndicator;
}

static IntRect contentsRectangle(Scrollbar& scrollbar, RenderThemeScrollbar& scrollbarWidget)
{
    GtkBorder scrollbarContentsBox = scrollbarWidget.scrollbar().contentsBox();
    GtkBorder contentsContentsBox = scrollbarWidget.contents().contentsBox();
    GtkBorder padding;
    padding.left = scrollbarContentsBox.left + contentsContentsBox.left;
    padding.right = scrollbarContentsBox.right + contentsContentsBox.right;
    padding.top = scrollbarContentsBox.top + contentsContentsBox.top;
    padding.bottom = scrollbarContentsBox.bottom + contentsContentsBox.bottom;
    IntRect contentsRect = scrollbar.frameRect();
    contentsRect.move(padding.left, padding.top);
    contentsRect.contract(padding.left + padding.right, padding.top + padding.bottom);
    return contentsRect;
}

IntRect ScrollbarThemeGtk::trackRect(Scrollbar& scrollbar, bool /*painting*/)
{
    auto scrollbarState = scrollbarPartStateFlags(scrollbar, AllParts);
    auto& scrollbarWidget = static_cast<RenderThemeScrollbar&>(RenderThemeWidget::getOrCreate(widgetTypeForScrollbar(scrollbar, scrollbarState)));
    scrollbarWidget.scrollbar().setState(scrollbarState);

    IntRect rect = contentsRectangle(scrollbar, scrollbarWidget);
    if (auto* backwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::Backward)) {
        backwardStepper->setState(scrollbarPartStateFlags(scrollbar, BackButtonStartPart));
        IntSize stepperSize = backwardStepper->preferredSize();
        if (scrollbar.orientation() == VerticalScrollbar) {
            rect.move(0, stepperSize.height());
            rect.contract(0, stepperSize.height());
        } else {
            rect.move(stepperSize.width(), 0);
            rect.contract(stepperSize.width(), 0);
        }
    }
    if (auto* secondaryForwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::SecondaryForward)) {
        secondaryForwardStepper->setState(scrollbarPartStateFlags(scrollbar, ForwardButtonStartPart));
        IntSize stepperSize = secondaryForwardStepper->preferredSize();
        if (scrollbar.orientation() == VerticalScrollbar) {
            rect.move(0, stepperSize.height());
            rect.contract(0, stepperSize.height());
        } else {
            rect.move(stepperSize.width(), 0);
            rect.contract(stepperSize.width(), 0);
        }
    }
    if (auto* secondaryBackwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::SecondaryBackward)) {
        secondaryBackwardStepper->setState(scrollbarPartStateFlags(scrollbar, BackButtonEndPart));
        if (scrollbar.orientation() == VerticalScrollbar)
            rect.contract(0, secondaryBackwardStepper->preferredSize().height());
        else
            rect.contract(secondaryBackwardStepper->preferredSize().width(), 0);
    }
    if (auto* forwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::Forward)) {
        forwardStepper->setState(scrollbarPartStateFlags(scrollbar, ForwardButtonEndPart));
        if (scrollbar.orientation() == VerticalScrollbar)
            rect.contract(0, forwardStepper->preferredSize().height());
        else
            rect.contract(forwardStepper->preferredSize().width(), 0);
    }

    if (scrollbar.orientation() == VerticalScrollbar)
        return scrollbar.height() < rect.height() ? IntRect() : rect;

    return scrollbar.width() < rect.width() ? IntRect() : rect;
}

bool ScrollbarThemeGtk::hasThumb(Scrollbar& scrollbar)
{
    // This method is just called as a paint-time optimization to see if
    // painting the thumb can be skipped. We don't have to be exact here.
    return thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeGtk::backButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool /*painting*/)
{
    ASSERT(part == BackButtonStartPart || part == BackButtonEndPart);
    if ((part == BackButtonEndPart && !m_hasBackButtonEndPart) || (part == BackButtonStartPart && !m_hasBackButtonStartPart))
        return IntRect();

    auto scrollbarState = scrollbarPartStateFlags(scrollbar, AllParts);
    auto& scrollbarWidget = static_cast<RenderThemeScrollbar&>(RenderThemeWidget::getOrCreate(widgetTypeForScrollbar(scrollbar, scrollbarState)));
    scrollbarWidget.scrollbar().setState(scrollbarState);

    IntRect rect = contentsRectangle(scrollbar, scrollbarWidget);
    if (part == BackButtonStartPart) {
        auto* backwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::Backward);
        ASSERT(backwardStepper);
        backwardStepper->setState(scrollbarPartStateFlags(scrollbar, BackButtonStartPart));
        return IntRect(rect.location(), backwardStepper->preferredSize());
    }

    if (auto* secondaryForwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::SecondaryForward)) {
        secondaryForwardStepper->setState(scrollbarPartStateFlags(scrollbar, ForwardButtonStartPart));
        IntSize preferredSize = secondaryForwardStepper->preferredSize();
        if (scrollbar.orientation() == VerticalScrollbar) {
            rect.move(0, preferredSize.height());
            rect.contract(0, preferredSize.height());
        } else {
            rect.move(preferredSize.width(), 0);
            rect.contract(0, preferredSize.width());
        }
    }

    if (auto* secondaryBackwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::SecondaryBackward)) {
        secondaryBackwardStepper->setState(scrollbarPartStateFlags(scrollbar, BackButtonEndPart));
        if (scrollbar.orientation() == VerticalScrollbar)
            rect.contract(0, secondaryBackwardStepper->preferredSize().height());
        else
            rect.contract(secondaryBackwardStepper->preferredSize().width(), 0);
    }

    auto* forwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::Forward);
    ASSERT(forwardStepper);
    forwardStepper->setState(scrollbarPartStateFlags(scrollbar, ForwardButtonEndPart));
    IntSize preferredSize = forwardStepper->preferredSize();
    if (scrollbar.orientation() == VerticalScrollbar)
        rect.move(0, rect.height() - preferredSize.height());
    else
        rect.move(rect.width() - preferredSize.width(), 0);

    return IntRect(rect.location(), preferredSize);
}

IntRect ScrollbarThemeGtk::forwardButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool /*painting*/)
{
    ASSERT(part == ForwardButtonStartPart || part == ForwardButtonEndPart);
    if ((part == ForwardButtonStartPart && !m_hasForwardButtonStartPart) || (part == ForwardButtonEndPart && !m_hasForwardButtonEndPart))
        return IntRect();

    auto scrollbarState = scrollbarPartStateFlags(scrollbar, AllParts);
    auto& scrollbarWidget = static_cast<RenderThemeScrollbar&>(RenderThemeWidget::getOrCreate(widgetTypeForScrollbar(scrollbar, scrollbarState)));
    scrollbarWidget.scrollbar().setState(scrollbarState);

    IntRect rect = contentsRectangle(scrollbar, scrollbarWidget);
    if (auto* backwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::Backward)) {
        backwardStepper->setState(scrollbarPartStateFlags(scrollbar, BackButtonStartPart));
        IntSize preferredSize = backwardStepper->preferredSize();
        if (scrollbar.orientation() == VerticalScrollbar) {
            rect.move(0, preferredSize.height());
            rect.contract(0, preferredSize.height());
        } else {
            rect.move(preferredSize.width(), 0);
            rect.contract(preferredSize.width(), 0);
        }
    }

    if (auto* secondaryForwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::SecondaryForward)) {
        secondaryForwardStepper->setState(scrollbarPartStateFlags(scrollbar, ForwardButtonStartPart));
        IntSize preferredSize = secondaryForwardStepper->preferredSize();
        if (part == ForwardButtonStartPart)
            return IntRect(rect.location(), preferredSize);

        if (scrollbar.orientation() == VerticalScrollbar) {
            rect.move(0, preferredSize.height());
            rect.contract(0, preferredSize.height());
        } else {
            rect.move(preferredSize.width(), 0);
            rect.contract(preferredSize.width(), 0);
        }
    }

    auto* forwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::Forward);
    ASSERT(forwardStepper);
    forwardStepper->setState(scrollbarPartStateFlags(scrollbar, ForwardButtonEndPart));
    IntSize preferredSize = forwardStepper->preferredSize();
    if (scrollbar.orientation() == VerticalScrollbar)
        rect.move(0, rect.height() - preferredSize.height());
    else
        rect.move(rect.width() - preferredSize.width(), 0);

    return IntRect(rect.location(), preferredSize);
}

bool ScrollbarThemeGtk::paint(Scrollbar& scrollbar, GraphicsContext& graphicsContext, const IntRect& damageRect)
{
    if (graphicsContext.paintingDisabled())
        return false;

    if (!scrollbar.enabled())
        return true;

    double opacity = scrollbar.hoveredPart() == NoPart ? scrollbar.opacity() : 1;
    if (!opacity)
        return true;

    IntRect rect = scrollbar.frameRect();
    if (!rect.intersects(damageRect))
        return true;

    auto scrollbarState = scrollbarPartStateFlags(scrollbar, AllParts, true);
    auto& scrollbarWidget = static_cast<RenderThemeScrollbar&>(RenderThemeWidget::getOrCreate(widgetTypeForScrollbar(scrollbar, scrollbarState)));
    auto& scrollbarGadget = scrollbarWidget.scrollbar();
    scrollbarGadget.setState(scrollbarState);
    if (m_usesOverlayScrollbars)
        opacity *= scrollbarGadget.opacity();
    if (!opacity)
        return true;

    auto& trough = scrollbarWidget.trough();
    trough.setState(scrollbarPartStateFlags(scrollbar, BackTrackPart));

    auto* backwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::Backward);
    if (backwardStepper)
        backwardStepper->setState(scrollbarPartStateFlags(scrollbar, BackButtonStartPart));
    auto* secondaryForwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::SecondaryForward);
    if (secondaryForwardStepper)
        secondaryForwardStepper->setState(scrollbarPartStateFlags(scrollbar, ForwardButtonStartPart));
    auto* secondaryBackwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::SecondaryBackward);
    if (secondaryBackwardStepper)
        secondaryBackwardStepper->setState(scrollbarPartStateFlags(scrollbar, BackButtonEndPart));
    auto* forwardStepper = scrollbarWidget.stepper(RenderThemeScrollbarGadget::Steppers::Forward);
    if (forwardStepper)
        forwardStepper->setState(scrollbarPartStateFlags(scrollbar, ForwardButtonEndPart));

    IntSize preferredSize = scrollbarWidget.contents().preferredSize();
    int thumbSize = thumbLength(scrollbar);
    if (thumbSize) {
        scrollbarWidget.slider().setState(scrollbarPartStateFlags(scrollbar, ThumbPart));
        preferredSize = preferredSize.expandedTo(scrollbarWidget.slider().preferredSize());
    }
    preferredSize += scrollbarGadget.preferredSize() - scrollbarGadget.minimumSize();

    FloatRect contentsRect(rect);
    // When using overlay scrollbars we always claim the size of the scrollbar when hovered, so when
    // drawing the indicator we need to adjust the rectangle to its actual size in indicator mode.
    if (scrollbar.orientation() == VerticalScrollbar) {
        if (rect.width() != preferredSize.width()) {
            if (!scrollbar.scrollableArea().shouldPlaceBlockDirectionScrollbarOnLeft())
                contentsRect.move(std::abs(rect.width() - preferredSize.width()), 0);
            contentsRect.setWidth(preferredSize.width());
        }
    } else {
        if (rect.height() != preferredSize.height()) {
            contentsRect.move(0, std::abs(rect.height() - preferredSize.height()));
            contentsRect.setHeight(preferredSize.height());
        }
    }

    if (opacity != 1) {
        graphicsContext.save();
        graphicsContext.clip(damageRect);
        graphicsContext.beginTransparencyLayer(opacity);
    }

    scrollbarGadget.render(graphicsContext.platformContext()->cr(), contentsRect, &contentsRect);
    scrollbarWidget.contents().render(graphicsContext.platformContext()->cr(), contentsRect, &contentsRect);

    if (backwardStepper) {
        FloatRect buttonRect = contentsRect;
        if (scrollbar.orientation() == VerticalScrollbar)
            buttonRect.setHeight(backwardStepper->preferredSize().height());
        else
            buttonRect.setWidth(backwardStepper->preferredSize().width());
        static_cast<RenderThemeScrollbarGadget&>(scrollbarGadget).renderStepper(graphicsContext.platformContext()->cr(), buttonRect, backwardStepper,
            scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbarGadget::Steppers::Backward);
        if (scrollbar.orientation() == VerticalScrollbar) {
            contentsRect.move(0, buttonRect.height());
            contentsRect.contract(0, buttonRect.height());
        } else {
            contentsRect.move(buttonRect.width(), 0);
            contentsRect.contract(buttonRect.width(), 0);
        }
    }
    if (secondaryForwardStepper) {
        FloatRect buttonRect = contentsRect;
        if (scrollbar.orientation() == VerticalScrollbar)
            buttonRect.setHeight(secondaryForwardStepper->preferredSize().height());
        else
            buttonRect.setWidth(secondaryForwardStepper->preferredSize().width());
        static_cast<RenderThemeScrollbarGadget&>(scrollbarGadget).renderStepper(graphicsContext.platformContext()->cr(), buttonRect, secondaryForwardStepper,
            scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbarGadget::Steppers::SecondaryForward);
        if (scrollbar.orientation() == VerticalScrollbar) {
            contentsRect.move(0, buttonRect.height());
            contentsRect.contract(0, buttonRect.height());
        } else {
            contentsRect.move(buttonRect.width(), 0);
            contentsRect.contract(buttonRect.width(), 0);
        }
    }
    if (secondaryBackwardStepper) {
        FloatRect buttonRect = contentsRect;
        if (scrollbar.orientation() == VerticalScrollbar) {
            buttonRect.setHeight(secondaryBackwardStepper->preferredSize().height());
            buttonRect.move(0, contentsRect.height() - buttonRect.height());
        } else {
            buttonRect.setWidth(secondaryBackwardStepper->preferredSize().width());
            buttonRect.move(contentsRect.width() - buttonRect.width(), 0);
        }
        static_cast<RenderThemeScrollbarGadget&>(scrollbarGadget).renderStepper(graphicsContext.platformContext()->cr(), buttonRect, secondaryBackwardStepper,
            scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbarGadget::Steppers::SecondaryBackward);
        if (scrollbar.orientation() == VerticalScrollbar)
            contentsRect.contract(0, buttonRect.height());
        else
            contentsRect.contract(buttonRect.width(), 0);
    }
    if (forwardStepper) {
        FloatRect buttonRect = contentsRect;
        if (scrollbar.orientation() == VerticalScrollbar) {
            buttonRect.setHeight(forwardStepper->preferredSize().height());
            buttonRect.move(0, contentsRect.height() - buttonRect.height());
        } else {
            buttonRect.setWidth(forwardStepper->preferredSize().width());
            buttonRect.move(contentsRect.width() - buttonRect.width(), 0);
        }
        static_cast<RenderThemeScrollbarGadget&>(scrollbarGadget).renderStepper(graphicsContext.platformContext()->cr(), buttonRect, forwardStepper,
            scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbarGadget::Steppers::Forward);
        if (scrollbar.orientation() == VerticalScrollbar)
            contentsRect.contract(0, buttonRect.height());
        else
            contentsRect.contract(buttonRect.width(), 0);
    }

    trough.render(graphicsContext.platformContext()->cr(), contentsRect, &contentsRect);

    if (thumbSize) {
        if (scrollbar.orientation() == VerticalScrollbar) {
            contentsRect.move(0, thumbPosition(scrollbar));
            contentsRect.setWidth(scrollbarWidget.slider().preferredSize().width());
            contentsRect.setHeight(thumbSize);
        } else {
            contentsRect.move(thumbPosition(scrollbar), 0);
            contentsRect.setWidth(thumbSize);
            contentsRect.setHeight(scrollbarWidget.slider().preferredSize().height());
        }
        if (contentsRect.intersects(damageRect))
            scrollbarWidget.slider().render(graphicsContext.platformContext()->cr(), contentsRect);
    }

    if (opacity != 1) {
        graphicsContext.endTransparencyLayer();
        graphicsContext.restore();
    }

    return true;
}

ScrollbarButtonPressAction ScrollbarThemeGtk::handleMousePressEvent(Scrollbar&, const PlatformMouseEvent& event, ScrollbarPart pressedPart)
{
    gboolean warpSlider = FALSE;
    switch (pressedPart) {
    case BackTrackPart:
    case ForwardTrackPart:
        g_object_get(gtk_settings_get_default(),
            "gtk-primary-button-warps-slider",
            &warpSlider, nullptr);
        // The shift key or middle/right button reverses the sense.
        if (event.shiftKey() || event.button() != LeftButton)
            warpSlider = !warpSlider;
        return warpSlider ?
            ScrollbarButtonPressAction::CenterOnThumb:
            ScrollbarButtonPressAction::Scroll;
    case ThumbPart:
        if (event.button() != RightButton)
            return ScrollbarButtonPressAction::StartDrag;
        break;
    case BackButtonStartPart:
    case ForwardButtonStartPart:
    case BackButtonEndPart:
    case ForwardButtonEndPart:
        return ScrollbarButtonPressAction::Scroll;
    default:
        break;
    }

    return ScrollbarButtonPressAction::None;
}

int ScrollbarThemeGtk::scrollbarThickness(ScrollbarControlSize, ScrollbarExpansionState)
{
    auto& scrollbarWidget = static_cast<RenderThemeScrollbar&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::VerticalScrollbarRight));
    scrollbarWidget.scrollbar().setState(GTK_STATE_FLAG_PRELIGHT);
    IntSize contentsPreferredSize = scrollbarWidget.contents().preferredSize();
    contentsPreferredSize = contentsPreferredSize.expandedTo(scrollbarWidget.slider().preferredSize());
    IntSize preferredSize = contentsPreferredSize + scrollbarWidget.scrollbar().preferredSize() - scrollbarWidget.scrollbar().minimumSize();
    return preferredSize.width();
}

int ScrollbarThemeGtk::minimumThumbLength(Scrollbar& scrollbar)
{
    auto& scrollbarWidget = static_cast<RenderThemeScrollbar&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::VerticalScrollbarRight));
    scrollbarWidget.scrollbar().setState(GTK_STATE_FLAG_PRELIGHT);
    IntSize minSize = scrollbarWidget.slider().minimumSize();
    return scrollbar.orientation() == VerticalScrollbar ? minSize.height() : minSize.width();
}

} // namespace WebCore
