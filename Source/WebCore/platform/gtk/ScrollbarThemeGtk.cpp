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
#include "RenderThemeGadget.h"
#include "ScrollView.h"
#include "Scrollbar.h"
#include <gtk/gtk.h>

namespace WebCore {

ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeGtk theme;
    return theme;
}

ScrollbarThemeGtk::~ScrollbarThemeGtk()
{
}

#ifndef GTK_API_VERSION_2
static void themeChangedCallback()
{
    ScrollbarTheme::theme().themeChanged();
}

ScrollbarThemeGtk::ScrollbarThemeGtk()
{
#if GTK_CHECK_VERSION(3, 20, 0)
    m_usesOverlayScrollbars = g_strcmp0(g_getenv("GTK_OVERLAY_SCROLLING"), "0");
#endif
    static bool themeMonitorInitialized = false;
    if (!themeMonitorInitialized) {
        g_signal_connect(gtk_settings_get_default(), "notify::gtk-theme-name", G_CALLBACK(themeChangedCallback), nullptr);
        themeMonitorInitialized = true;
        updateThemeProperties();
    }
}

#if !GTK_CHECK_VERSION(3, 20, 0)
static GRefPtr<GtkStyleContext> createStyleContext(Scrollbar* scrollbar = nullptr)
{
    GRefPtr<GtkStyleContext> styleContext = adoptGRef(gtk_style_context_new());
    GRefPtr<GtkWidgetPath> path = adoptGRef(gtk_widget_path_new());
    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SCROLLBAR);
    gtk_widget_path_iter_add_class(path.get(), -1, scrollbar && scrollbar->orientation() == HorizontalScrollbar ? "horizontal" : "vertical");
    gtk_style_context_set_path(styleContext.get(), path.get());
    return styleContext;
}

static GRefPtr<GtkStyleContext> createChildStyleContext(GtkStyleContext* parent, const char* className)
{
    ASSERT(parent);
    GRefPtr<GtkWidgetPath> path = adoptGRef(gtk_widget_path_copy(gtk_style_context_get_path(parent)));
    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SCROLLBAR);
    gtk_widget_path_iter_add_class(path.get(), -1, className);

    GRefPtr<GtkStyleContext> styleContext = adoptGRef(gtk_style_context_new());
    gtk_style_context_set_path(styleContext.get(), path.get());
    gtk_style_context_set_parent(styleContext.get(), parent);
    return styleContext;
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

void ScrollbarThemeGtk::themeChanged()
{
    updateThemeProperties();
}

#if GTK_CHECK_VERSION(3, 20, 0)
void ScrollbarThemeGtk::updateThemeProperties()
{
    auto steppers = static_cast<RenderThemeScrollbarGadget*>(RenderThemeGadget::create({ RenderThemeGadget::Type::Scrollbar, "scrollbar", GTK_STATE_FLAG_NORMAL, { } }).get())->steppers();
    m_hasBackButtonStartPart = steppers.contains(RenderThemeScrollbarGadget::Steppers::Backward);
    m_hasForwardButtonEndPart = steppers.contains(RenderThemeScrollbarGadget::Steppers::Forward);
    m_hasBackButtonEndPart = steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryBackward);
    m_hasForwardButtonStartPart = steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryForward);
}
#else
void ScrollbarThemeGtk::updateThemeProperties()
{
    gboolean hasBackButtonStartPart, hasForwardButtonEndPart, hasBackButtonEndPart, hasForwardButtonStartPart;
    gtk_style_context_get_style(createStyleContext().get(),
        "has-backward-stepper", &hasBackButtonStartPart,
        "has-forward-stepper", &hasForwardButtonEndPart,
        "has-secondary-backward-stepper", &hasBackButtonEndPart,
        "has-secondary-forward-stepper", &hasForwardButtonStartPart,
        nullptr);
    m_hasBackButtonStartPart = hasBackButtonStartPart;
    m_hasForwardButtonEndPart = hasForwardButtonEndPart;
    m_hasBackButtonEndPart = hasBackButtonEndPart;
    m_hasForwardButtonStartPart = hasForwardButtonStartPart;
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

bool ScrollbarThemeGtk::hasButtons(Scrollbar& scrollbar)
{
    return scrollbar.enabled() && (m_hasBackButtonStartPart || m_hasForwardButtonEndPart || m_hasBackButtonEndPart || m_hasForwardButtonStartPart);
}

#if GTK_CHECK_VERSION(3, 20, 0)
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

static std::unique_ptr<RenderThemeGadget> scrollbarGadgetForLayout(Scrollbar& scrollbar)
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Scrollbar, "scrollbar", scrollbarPartStateFlags(scrollbar, AllParts), { } };
    if (scrollbar.orientation() == VerticalScrollbar) {
        info.classList.append("vertical");
        info.classList.append("right");
    } else {
        info.classList.append("horizontal");
        info.classList.append("bottom");
    }
    if (scrollbar.isOverlayScrollbar())
        info.classList.append("overlay-indicator");
    if (info.state & GTK_STATE_FLAG_PRELIGHT)
        info.classList.append("hovering");

    return RenderThemeGadget::create(info);
}

static std::unique_ptr<RenderThemeBoxGadget> contentsGadgetForLayout(Scrollbar& scrollbar, RenderThemeGadget* parent, IntRect& contentsRect, Vector<int, 4>& steppersPosition)
{
    Vector<RenderThemeGadget::Info> children;
    auto steppers = static_cast<RenderThemeScrollbarGadget*>(parent)->steppers();
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::Backward)) {
        steppersPosition[0] = 0;
        children.append({ RenderThemeGadget::Type::Generic, "button", scrollbarPartStateFlags(scrollbar, BackButtonStartPart), { "up" } });
    }
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryForward)) {
        steppersPosition[1] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", scrollbarPartStateFlags(scrollbar, ForwardButtonStartPart), { "down" } });
    }
    children.append({ RenderThemeGadget::Type::Generic, "trough", scrollbarPartStateFlags(scrollbar, BackTrackPart), { } });
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryBackward)) {
        steppersPosition[2] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", scrollbarPartStateFlags(scrollbar, BackButtonEndPart), { "up" } });
    }
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::Forward)) {
        steppersPosition[3] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", scrollbarPartStateFlags(scrollbar, ForwardButtonEndPart), { "down" } });
    }
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Generic, "contents", GTK_STATE_FLAG_NORMAL, { } };
    auto contentsGadget = std::make_unique<RenderThemeBoxGadget>(info, scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL,
        children, parent);

    GtkBorder scrollbarContentsBox = parent->contentsBox();
    GtkBorder contentsContentsBox = contentsGadget->contentsBox();
    GtkBorder padding;
    padding.left = scrollbarContentsBox.left + contentsContentsBox.left;
    padding.right = scrollbarContentsBox.right + contentsContentsBox.right;
    padding.top = scrollbarContentsBox.top + contentsContentsBox.top;
    padding.bottom = scrollbarContentsBox.bottom + contentsContentsBox.bottom;
    contentsRect = scrollbar.frameRect();
    contentsRect.move(padding.left, padding.top);
    contentsRect.contract(padding.left + padding.right, padding.top + padding.bottom);
    return contentsGadget;
}

IntRect ScrollbarThemeGtk::trackRect(Scrollbar& scrollbar, bool /*painting*/)
{
    auto scrollbarGadget = scrollbarGadgetForLayout(scrollbar);
    IntRect rect;
    Vector<int, 4> steppersPosition(4, -1);
    auto contentsGadget = contentsGadgetForLayout(scrollbar, scrollbarGadget.get(), rect, steppersPosition);

    if (steppersPosition[0] != -1) {
        IntSize stepperSize = contentsGadget->child(steppersPosition[0])->preferredSize();
        if (scrollbar.orientation() == VerticalScrollbar) {
            rect.move(0, stepperSize.height());
            rect.contract(0, stepperSize.height());
        } else {
            rect.move(stepperSize.width(), 0);
            rect.contract(stepperSize.width(), 0);
        }
    }
    if (steppersPosition[1] != -1) {
        IntSize stepperSize = contentsGadget->child(steppersPosition[1])->preferredSize();
        if (scrollbar.orientation() == VerticalScrollbar) {
            rect.move(0, stepperSize.height());
            rect.contract(0, stepperSize.height());
        } else {
            rect.move(stepperSize.width(), 0);
            rect.contract(stepperSize.width(), 0);
        }
    }
    if (steppersPosition[2] != -1) {
        if (scrollbar.orientation() == VerticalScrollbar)
            rect.contract(0, contentsGadget->child(steppersPosition[2])->preferredSize().height());
        else
            rect.contract(contentsGadget->child(steppersPosition[2])->preferredSize().width(), 0);
    }
    if (steppersPosition[3] != -1) {
        if (scrollbar.orientation() == VerticalScrollbar)
            rect.contract(0, contentsGadget->child(steppersPosition[3])->preferredSize().height());
        else
            rect.contract(contentsGadget->child(steppersPosition[3])->preferredSize().width(), 0);
    }

    if (scrollbar.orientation() == VerticalScrollbar)
        return scrollbar.height() < rect.height() ? IntRect() : rect;

    return scrollbar.width() < rect.width() ? IntRect() : rect;
}
#else
IntRect ScrollbarThemeGtk::trackRect(Scrollbar& scrollbar, bool /*painting*/)
{
    GRefPtr<GtkStyleContext> styleContext = createStyleContext(&scrollbar);
    // The padding along the thumb movement axis includes the trough border
    // plus the size of stepper spacing (the space between the stepper and
    // the place where the thumb stops). There is often no stepper spacing.
    int stepperSpacing, stepperSize, troughBorderWidth, thumbFat;
    gtk_style_context_get_style(styleContext.get(), "stepper-spacing", &stepperSpacing, "stepper-size", &stepperSize, "trough-border",
        &troughBorderWidth, "slider-width", &thumbFat, nullptr);

    // The fatness of the scrollbar on the non-movement axis.
    int thickness = thumbFat + 2 * troughBorderWidth;

    int startButtonsOffset = 0;
    int buttonsWidth = 0;
    if (m_hasForwardButtonStartPart) {
        startButtonsOffset += stepperSize;
        buttonsWidth += stepperSize;
    }
    if (m_hasBackButtonStartPart) {
        startButtonsOffset += stepperSize;
        buttonsWidth += stepperSize;
    }
    if (m_hasBackButtonEndPart)
        buttonsWidth += stepperSize;
    if (m_hasForwardButtonEndPart)
        buttonsWidth += stepperSize;

    if (scrollbar.orientation() == HorizontalScrollbar) {
        // Once the scrollbar becomes smaller than the natural size of the two buttons and the thumb, the track disappears.
        if (scrollbar.width() < buttonsWidth + minimumThumbLength(scrollbar))
            return IntRect();
        return IntRect(scrollbar.x() + troughBorderWidth + stepperSpacing + startButtonsOffset, scrollbar.y(),
            scrollbar.width() - (2 * troughBorderWidth) - (2 * stepperSpacing) - buttonsWidth, thickness);
    }

    if (scrollbar.height() < buttonsWidth + minimumThumbLength(scrollbar))
        return IntRect();
    return IntRect(scrollbar.x(), scrollbar.y() + troughBorderWidth + stepperSpacing + startButtonsOffset,
        thickness, scrollbar.height() - (2 * troughBorderWidth) - (2 * stepperSpacing) - buttonsWidth);
}
#endif

bool ScrollbarThemeGtk::hasThumb(Scrollbar& scrollbar)
{
    // This method is just called as a paint-time optimization to see if
    // painting the thumb can be skipped. We don't have to be exact here.
    return thumbLength(scrollbar) > 0;
}

#if GTK_CHECK_VERSION(3, 20, 0)
IntRect ScrollbarThemeGtk::backButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool /*painting*/)
{
    ASSERT(part == BackButtonStartPart || part == BackButtonEndPart);
    if ((part == BackButtonEndPart && !m_hasBackButtonEndPart) || (part == BackButtonStartPart && !m_hasBackButtonStartPart))
        return IntRect();

    auto scrollbarGadget = scrollbarGadgetForLayout(scrollbar);
    IntRect rect;
    Vector<int, 4> steppersPosition(4, -1);
    auto contentsGadget = contentsGadgetForLayout(scrollbar, scrollbarGadget.get(), rect, steppersPosition);

    if (part == BackButtonStartPart)
        return IntRect(rect.location(), contentsGadget->child(0)->preferredSize());

    // Secondary back.
    if (steppersPosition[1] != -1) {
        IntSize preferredSize = contentsGadget->child(steppersPosition[1])->preferredSize();
        if (scrollbar.orientation() == VerticalScrollbar) {
            rect.move(0, preferredSize.height());
            rect.contract(0, preferredSize.height());
        } else {
            rect.move(preferredSize.width(), 0);
            rect.contract(0, preferredSize.width());
        }
    }

    if (steppersPosition[3] != -1) {
        if (scrollbar.orientation() == VerticalScrollbar)
            rect.contract(0, contentsGadget->child(steppersPosition[3])->preferredSize().height());
        else
            rect.contract(contentsGadget->child(steppersPosition[3])->preferredSize().width(), 0);
    }

    IntSize preferredSize = contentsGadget->child(steppersPosition[2])->preferredSize();
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

    auto scrollbarGadget = scrollbarGadgetForLayout(scrollbar);
    IntRect rect;
    Vector<int, 4> steppersPosition(4, -1);
    auto contentsGadget = contentsGadgetForLayout(scrollbar, scrollbarGadget.get(), rect, steppersPosition);

    if (steppersPosition[0] != -1) {
        IntSize preferredSize = contentsGadget->child(steppersPosition[0])->preferredSize();
        if (scrollbar.orientation() == VerticalScrollbar) {
            rect.move(0, preferredSize.height());
            rect.contract(0, preferredSize.height());
        } else {
            rect.move(preferredSize.width(), 0);
            rect.contract(preferredSize.width(), 0);
        }
    }

    if (steppersPosition[1] != -1) {
        IntSize preferredSize = contentsGadget->child(steppersPosition[1])->preferredSize();
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

    // Forward button.
    IntSize preferredSize = contentsGadget->child(steppersPosition[3])->preferredSize();
    if (scrollbar.orientation() == VerticalScrollbar)
        rect.move(0, rect.height() - preferredSize.height());
    else
        rect.move(rect.width() - preferredSize.width(), 0);

    return IntRect(rect.location(), preferredSize);
}
#else
IntRect ScrollbarThemeGtk::backButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool /*painting*/)
{
    if ((part == BackButtonEndPart && !m_hasBackButtonEndPart) || (part == BackButtonStartPart && !m_hasBackButtonStartPart))
        return IntRect();

    GRefPtr<GtkStyleContext> styleContext = createStyleContext(&scrollbar);
    int troughBorderWidth, stepperSize, thumbFat;
    gtk_style_context_get_style(styleContext.get(), "trough-border", &troughBorderWidth, "stepper-size", &stepperSize, "slider-width", &thumbFat, nullptr);
    int x = scrollbar.x() + troughBorderWidth;
    int y = scrollbar.y() + troughBorderWidth;
    if (part == BackButtonStartPart) {
        if (scrollbar.orientation() == HorizontalScrollbar)
            return IntRect(x, y, stepperSize, thumbFat);
        return IntRect(x, y, thumbFat, stepperSize);
    }

    // BackButtonEndPart (alternate button)
    if (scrollbar.orientation() == HorizontalScrollbar)
        return IntRect(scrollbar.x() + scrollbar.width() - troughBorderWidth - (2 * stepperSize), y, stepperSize, thumbFat);

    // VerticalScrollbar alternate button
    return IntRect(x, scrollbar.y() + scrollbar.height() - troughBorderWidth - (2 * stepperSize), thumbFat, stepperSize);
}

IntRect ScrollbarThemeGtk::forwardButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool /*painting*/)
{
    if ((part == ForwardButtonStartPart && !m_hasForwardButtonStartPart) || (part == ForwardButtonEndPart && !m_hasForwardButtonEndPart))
        return IntRect();

    GRefPtr<GtkStyleContext> styleContext = createStyleContext(&scrollbar);
    int troughBorderWidth, stepperSize, thumbFat;
    gtk_style_context_get_style(styleContext.get(), "trough-border", &troughBorderWidth, "stepper-size", &stepperSize, "slider-width", &thumbFat, nullptr);
    if (scrollbar.orientation() == HorizontalScrollbar) {
        int y = scrollbar.y() + troughBorderWidth;
        if (part == ForwardButtonEndPart)
            return IntRect(scrollbar.x() + scrollbar.width() - stepperSize - troughBorderWidth, y, stepperSize, thumbFat);

        // ForwardButtonStartPart (alternate button)
        return IntRect(scrollbar.x() + troughBorderWidth + stepperSize, y, stepperSize, thumbFat);
    }

    // VerticalScrollbar
    int x = scrollbar.x() + troughBorderWidth;
    if (part == ForwardButtonEndPart)
        return IntRect(x, scrollbar.y() + scrollbar.height() - stepperSize - troughBorderWidth, thumbFat, stepperSize);

    // ForwardButtonStartPart (alternate button)
    return IntRect(x, scrollbar.y() + troughBorderWidth + stepperSize, thumbFat, stepperSize);
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

#if GTK_CHECK_VERSION(3, 20, 0)
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

    bool scrollbarOnLeft = scrollbar.scrollableArea().shouldPlaceBlockDirectionScrollbarOnLeft();

    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Scrollbar, "scrollbar", scrollbarPartStateFlags(scrollbar, AllParts, true), { } };
    if (scrollbar.orientation() == VerticalScrollbar) {
        info.classList.append("vertical");
        info.classList.append(scrollbarOnLeft ? "left" : "right");
    } else {
        info.classList.append("horizontal");
        info.classList.append("bottom");
    }
    if (m_usesOverlayScrollbars)
        info.classList.append("overlay-indicator");
    if (info.state & GTK_STATE_FLAG_PRELIGHT)
        info.classList.append("hovering");
    if (scrollbar.pressedPart() != NoPart)
        info.classList.append("dragging");
    auto scrollbarGadget = RenderThemeGadget::create(info);
    if (m_usesOverlayScrollbars)
        opacity *= scrollbarGadget->opacity();
    if (!opacity)
        return true;

    info.type = RenderThemeGadget::Type::Generic;
    info.name = "contents";
    info.state = GTK_STATE_FLAG_NORMAL;
    info.classList.clear();
    Vector<RenderThemeGadget::Info> children;
    auto steppers = static_cast<RenderThemeScrollbarGadget*>(scrollbarGadget.get())->steppers();
    unsigned steppersPosition[4] = { 0, 0, 0, 0 };
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::Backward)) {
        steppersPosition[0] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", scrollbarPartStateFlags(scrollbar, BackButtonStartPart), { "up" } });
    }
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryForward)) {
        steppersPosition[1] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", scrollbarPartStateFlags(scrollbar, ForwardButtonStartPart), { "down" } });
    }
    unsigned troughPosition = children.size();
    children.append({ RenderThemeGadget::Type::Generic, "trough", scrollbarPartStateFlags(scrollbar, BackTrackPart), { } });
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryBackward)) {
        steppersPosition[2] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", scrollbarPartStateFlags(scrollbar, BackButtonEndPart), { "up" } });
    }
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::Forward)) {
        steppersPosition[3] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", scrollbarPartStateFlags(scrollbar, ForwardButtonEndPart), { "down" } });
    }
    auto contentsGadget = std::make_unique<RenderThemeBoxGadget>(info, scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL,
        children, scrollbarGadget.get());
    RenderThemeGadget* troughGadget = contentsGadget->child(troughPosition);

    IntSize preferredSize = contentsGadget->preferredSize();
    std::unique_ptr<RenderThemeGadget> sliderGadget;
    int thumbSize = thumbLength(scrollbar);
    if (thumbSize) {
        info.name = "slider";
        info.state = scrollbarPartStateFlags(scrollbar, ThumbPart);
        sliderGadget = RenderThemeGadget::create(info, troughGadget);
        preferredSize = preferredSize.expandedTo(sliderGadget->preferredSize());
    }
    preferredSize += scrollbarGadget->preferredSize() - scrollbarGadget->minimumSize();

    FloatRect contentsRect(rect);
    // When using overlay scrollbars we always claim the size of the scrollbar when hovered, so when
    // drawing the indicator we need to adjust the rectangle to its actual size in indicator mode.
    if (scrollbar.orientation() == VerticalScrollbar) {
        if (rect.width() != preferredSize.width()) {
            if (!scrollbarOnLeft)
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

    scrollbarGadget->render(graphicsContext.platformContext()->cr(), contentsRect, &contentsRect);
    contentsGadget->render(graphicsContext.platformContext()->cr(), contentsRect, &contentsRect);

    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::Backward)) {
        RenderThemeGadget* buttonGadget = contentsGadget->child(steppersPosition[0]);
        FloatRect buttonRect = contentsRect;
        if (scrollbar.orientation() == VerticalScrollbar)
            buttonRect.setHeight(buttonGadget->preferredSize().height());
        else
            buttonRect.setWidth(buttonGadget->preferredSize().width());
        static_cast<RenderThemeScrollbarGadget*>(scrollbarGadget.get())->renderStepper(graphicsContext.platformContext()->cr(), buttonRect, buttonGadget,
            scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbarGadget::Steppers::Backward);
        if (scrollbar.orientation() == VerticalScrollbar) {
            contentsRect.move(0, buttonRect.height());
            contentsRect.contract(0, buttonRect.height());
        } else {
            contentsRect.move(buttonRect.width(), 0);
            contentsRect.contract(buttonRect.width(), 0);
        }
    }
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryForward)) {
        RenderThemeGadget* buttonGadget = contentsGadget->child(steppersPosition[1]);
        FloatRect buttonRect = contentsRect;
        if (scrollbar.orientation() == VerticalScrollbar)
            buttonRect.setHeight(buttonGadget->preferredSize().height());
        else
            buttonRect.setWidth(buttonGadget->preferredSize().width());
        static_cast<RenderThemeScrollbarGadget*>(scrollbarGadget.get())->renderStepper(graphicsContext.platformContext()->cr(), buttonRect, buttonGadget,
            scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbarGadget::Steppers::SecondaryForward);
        if (scrollbar.orientation() == VerticalScrollbar) {
            contentsRect.move(0, buttonRect.height());
            contentsRect.contract(0, buttonRect.height());
        } else {
            contentsRect.move(buttonRect.width(), 0);
            contentsRect.contract(buttonRect.width(), 0);
        }
    }

    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::Forward)) {
        RenderThemeGadget* buttonGadget = contentsGadget->child(steppersPosition[3]);
        FloatRect buttonRect = contentsRect;
        if (scrollbar.orientation() == VerticalScrollbar) {
            buttonRect.setHeight(buttonGadget->preferredSize().height());
            buttonRect.move(0, contentsRect.height() - buttonRect.height());
        } else {
            buttonRect.setWidth(buttonGadget->preferredSize().width());
            buttonRect.move(contentsRect.width() - buttonRect.width(), 0);
        }
        static_cast<RenderThemeScrollbarGadget*>(scrollbarGadget.get())->renderStepper(graphicsContext.platformContext()->cr(), buttonRect, buttonGadget,
            scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbarGadget::Steppers::Forward);
        if (scrollbar.orientation() == VerticalScrollbar)
            contentsRect.contract(0, buttonRect.height());
        else
            contentsRect.contract(buttonRect.width(), 0);
    }
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryBackward)) {
        RenderThemeGadget* buttonGadget = contentsGadget->child(steppersPosition[2]);
        FloatRect buttonRect = contentsRect;
        if (scrollbar.orientation() == VerticalScrollbar) {
            buttonRect.setHeight(buttonGadget->preferredSize().height());
            buttonRect.move(0, contentsRect.height() - buttonRect.height());
        } else {
            buttonRect.setWidth(buttonGadget->preferredSize().width());
            buttonRect.move(contentsRect.width() - buttonRect.width(), 0);
        }
        static_cast<RenderThemeScrollbarGadget*>(scrollbarGadget.get())->renderStepper(graphicsContext.platformContext()->cr(), buttonRect, buttonGadget,
            scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbarGadget::Steppers::SecondaryBackward);
        if (scrollbar.orientation() == VerticalScrollbar)
            contentsRect.contract(0, buttonRect.height());
        else
            contentsRect.contract(buttonRect.width(), 0);
    }

    troughGadget->render(graphicsContext.platformContext()->cr(), contentsRect, &contentsRect);
    if (sliderGadget) {
        if (scrollbar.orientation() == VerticalScrollbar) {
            contentsRect.move(0, thumbPosition(scrollbar));
            contentsRect.setWidth(sliderGadget->preferredSize().width());
            contentsRect.setHeight(thumbSize);
        } else {
            contentsRect.move(thumbPosition(scrollbar), 0);
            contentsRect.setWidth(thumbSize);
            contentsRect.setHeight(sliderGadget->preferredSize().height());
        }
        if (contentsRect.intersects(damageRect))
            sliderGadget->render(graphicsContext.platformContext()->cr(), contentsRect);
    }

    if (opacity != 1) {
        graphicsContext.endTransparencyLayer();
        graphicsContext.restore();
    }

    return true;
}
#else
static void paintStepper(GtkStyleContext* parentContext, GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect, ScrollbarPart part)
{
    ScrollbarOrientation orientation = scrollbar.orientation();
    GRefPtr<GtkStyleContext> styleContext = createChildStyleContext(parentContext, "button");

    unsigned flags = 0;
    if ((BackButtonStartPart == part && scrollbar.currentPos())
        || (BackButtonEndPart == part && scrollbar.currentPos())
        || (ForwardButtonEndPart == part && scrollbar.currentPos() != scrollbar.maximum())
        || (ForwardButtonStartPart == part && scrollbar.currentPos() != scrollbar.maximum())) {
        if (part == scrollbar.pressedPart())
            flags |= GTK_STATE_FLAG_ACTIVE;
        if (part == scrollbar.hoveredPart())
            flags |= GTK_STATE_FLAG_PRELIGHT;
    } else
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    gtk_style_context_set_state(styleContext.get(), static_cast<GtkStateFlags>(flags));

    gtk_render_background(styleContext.get(), context.platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(styleContext.get(), context.platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    gfloat arrowScaling;
    gtk_style_context_get_style(styleContext.get(), "arrow-scaling", &arrowScaling, nullptr);

    double arrowSize = std::min(rect.width(), rect.height()) * arrowScaling;
    FloatPoint arrowPoint(rect.x() + (rect.width() - arrowSize) / 2, rect.y() + (rect.height() - arrowSize) / 2);

    if (flags & GTK_STATE_FLAG_ACTIVE) {
        gint arrowDisplacementX, arrowDisplacementY;
        gtk_style_context_get_style(styleContext.get(), "arrow-displacement-x", &arrowDisplacementX, "arrow-displacement-y", &arrowDisplacementY, nullptr);
        arrowPoint.move(arrowDisplacementX, arrowDisplacementY);
    }

    gdouble angle;
    if (orientation == VerticalScrollbar)
        angle = (part == ForwardButtonEndPart || part == ForwardButtonStartPart) ? G_PI : 0;
    else
        angle = (part == ForwardButtonEndPart || part == ForwardButtonStartPart) ? G_PI / 2 : 3 * (G_PI / 2);

    gtk_render_arrow(styleContext.get(), context.platformContext()->cr(), angle, arrowPoint.x(), arrowPoint.y(), arrowSize);
}

static void adjustRectAccordingToMargin(GtkStyleContext* context, IntRect& rect)
{
    GtkBorder margin;
    gtk_style_context_get_margin(context, gtk_style_context_get_state(context), &margin);
    rect.move(margin.left, margin.top);
    rect.contract(margin.left + margin.right, margin.top + margin.bottom);
}

bool ScrollbarThemeGtk::paint(Scrollbar& scrollbar, GraphicsContext& graphicsContext, const IntRect& damageRect)
{
    if (graphicsContext.paintingDisabled())
        return false;

    GRefPtr<GtkStyleContext> styleContext = createStyleContext(&scrollbar);

    // Create the ScrollbarControlPartMask based on the damageRect
    ScrollbarControlPartMask scrollMask = NoPart;

    IntRect backButtonStartPaintRect;
    IntRect backButtonEndPaintRect;
    IntRect forwardButtonStartPaintRect;
    IntRect forwardButtonEndPaintRect;
    if (hasButtons(scrollbar)) {
        backButtonStartPaintRect = backButtonRect(scrollbar, BackButtonStartPart, true);
        if (damageRect.intersects(backButtonStartPaintRect))
            scrollMask |= BackButtonStartPart;
        backButtonEndPaintRect = backButtonRect(scrollbar, BackButtonEndPart, true);
        if (damageRect.intersects(backButtonEndPaintRect))
            scrollMask |= BackButtonEndPart;
        forwardButtonStartPaintRect = forwardButtonRect(scrollbar, ForwardButtonStartPart, true);
        if (damageRect.intersects(forwardButtonStartPaintRect))
            scrollMask |= ForwardButtonStartPart;
        forwardButtonEndPaintRect = forwardButtonRect(scrollbar, ForwardButtonEndPart, true);
        if (damageRect.intersects(forwardButtonEndPaintRect))
            scrollMask |= ForwardButtonEndPart;
    }

    IntRect trackPaintRect = trackRect(scrollbar, true);
    if (damageRect.intersects(trackPaintRect))
        scrollMask |= TrackBGPart;

    gboolean troughUnderSteppers;
    gtk_style_context_get_style(styleContext.get(), "trough-under-steppers", &troughUnderSteppers, nullptr);
    if (troughUnderSteppers && (scrollMask & BackButtonStartPart
            || scrollMask & BackButtonEndPart
            || scrollMask & ForwardButtonStartPart
            || scrollMask & ForwardButtonEndPart))
        scrollMask |= TrackBGPart;

    IntRect currentThumbRect;
    if (hasThumb(scrollbar)) {
        IntRect track = trackRect(scrollbar, false);
        IntRect trackRect = constrainTrackRectToTrackPieces(scrollbar, track);
        int thumbFat;
        gtk_style_context_get_style(styleContext.get(), "slider-width", &thumbFat, nullptr);
        if (scrollbar.orientation() == HorizontalScrollbar)
            currentThumbRect = IntRect(trackRect.x() + thumbPosition(scrollbar), trackRect.y() + (trackRect.height() - thumbFat) / 2, thumbLength(scrollbar), thumbFat);
        else
            currentThumbRect = IntRect(trackRect.x() + (trackRect.width() - thumbFat) / 2, trackRect.y() + thumbPosition(scrollbar), thumbFat, thumbLength(scrollbar));
        if (damageRect.intersects(currentThumbRect))
            scrollMask |= ThumbPart;
    }

    if (scrollMask == NoPart)
        return true;

    ScrollbarControlPartMask allButtons = BackButtonStartPart | BackButtonEndPart | ForwardButtonStartPart | ForwardButtonEndPart;

    // Paint the track background. If the trough-under-steppers property is true, this
    // should be the full size of the scrollbar, but if is false, it should only be the
    // track rect.
    GRefPtr<GtkStyleContext> troughStyleContext = createChildStyleContext(styleContext.get(), "trough");
    if (scrollMask & TrackBGPart || scrollMask & ThumbPart || scrollMask & allButtons) {
        IntRect fullScrollbarRect = trackPaintRect;
        if (troughUnderSteppers)
            fullScrollbarRect = scrollbar.frameRect();

        IntRect adjustedRect = fullScrollbarRect;
        adjustRectAccordingToMargin(styleContext.get(), adjustedRect);
        gtk_render_background(styleContext.get(), graphicsContext.platformContext()->cr(), adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height());
        gtk_render_frame(styleContext.get(), graphicsContext.platformContext()->cr(), adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height());

        adjustedRect = fullScrollbarRect;
        adjustRectAccordingToMargin(troughStyleContext.get(), adjustedRect);
        gtk_render_background(troughStyleContext.get(), graphicsContext.platformContext()->cr(), adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height());
        gtk_render_frame(troughStyleContext.get(), graphicsContext.platformContext()->cr(), adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height());
    }

    // Paint the back and forward buttons.
    if (scrollMask & BackButtonStartPart)
        paintStepper(styleContext.get(), graphicsContext, scrollbar, backButtonStartPaintRect, BackButtonStartPart);
    if (scrollMask & BackButtonEndPart)
        paintStepper(styleContext.get(), graphicsContext, scrollbar, backButtonEndPaintRect, BackButtonEndPart);
    if (scrollMask & ForwardButtonStartPart)
        paintStepper(styleContext.get(), graphicsContext, scrollbar, forwardButtonStartPaintRect, ForwardButtonStartPart);
    if (scrollMask & ForwardButtonEndPart)
        paintStepper(styleContext.get(), graphicsContext, scrollbar, forwardButtonEndPaintRect, ForwardButtonEndPart);

    // Paint the thumb.
    if (scrollMask & ThumbPart) {
        GRefPtr<GtkStyleContext> thumbStyleContext = createChildStyleContext(troughStyleContext.get(), "slider");
        unsigned flags = 0;
        if (scrollbar.pressedPart() == ThumbPart)
            flags |= GTK_STATE_FLAG_ACTIVE;
        if (scrollbar.hoveredPart() == ThumbPart)
            flags |= GTK_STATE_FLAG_PRELIGHT;
        gtk_style_context_set_state(thumbStyleContext.get(), static_cast<GtkStateFlags>(flags));

        IntRect thumbRect(currentThumbRect);
        adjustRectAccordingToMargin(thumbStyleContext.get(), thumbRect);
        gtk_render_slider(thumbStyleContext.get(), graphicsContext.platformContext()->cr(), thumbRect.x(), thumbRect.y(), thumbRect.width(), thumbRect.height(),
            scrollbar.orientation() == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
    }

    return true;
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

ScrollbarButtonPressAction ScrollbarThemeGtk::handleMousePressEvent(Scrollbar&, const PlatformMouseEvent& event, ScrollbarPart pressedPart)
{
    switch (pressedPart) {
    case BackTrackPart:
    case ForwardTrackPart:
        if (event.button() == LeftButton)
            return ScrollbarButtonPressAction::CenterOnThumb;
        if (event.button() == RightButton)
            return ScrollbarButtonPressAction::Scroll;
        break;
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

#if GTK_CHECK_VERSION(3, 20, 0)
int ScrollbarThemeGtk::scrollbarThickness(ScrollbarControlSize)
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Scrollbar, "scrollbar", GTK_STATE_FLAG_PRELIGHT, { "vertical", "right", "hovering" } };
    if (m_usesOverlayScrollbars)
        info.classList.append("overlay-indicator");
    auto scrollbarGadget = RenderThemeGadget::create(info);
    info.type = RenderThemeGadget::Type::Generic;
    info.name = "contents";
    info.state = GTK_STATE_FLAG_NORMAL;
    info.classList.clear();
    Vector<RenderThemeGadget::Info> children;
    auto steppers = static_cast<RenderThemeScrollbarGadget*>(scrollbarGadget.get())->steppers();
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::Backward))
        children.append({ RenderThemeGadget::Type::Generic, "button", GTK_STATE_FLAG_NORMAL, { "up" } });
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryForward))
        children.append({ RenderThemeGadget::Type::Generic, "button", GTK_STATE_FLAG_NORMAL, { "down" } });
    unsigned troughPositon = children.size();
    children.append({ RenderThemeGadget::Type::Generic, "trough", GTK_STATE_FLAG_PRELIGHT, { } });
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryBackward))
        children.append({ RenderThemeGadget::Type::Generic, "button", GTK_STATE_FLAG_NORMAL, { "up" } });
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::Forward))
        children.append({ RenderThemeGadget::Type::Generic, "button", GTK_STATE_FLAG_NORMAL, { "down" } });
    auto contentsGadget = std::make_unique<RenderThemeBoxGadget>(info, GTK_ORIENTATION_VERTICAL, children, scrollbarGadget.get());
    info.name = "slider";
    auto sliderGadget = RenderThemeGadget::create(info, contentsGadget->child(troughPositon));
    IntSize contentsPreferredSize = contentsGadget->preferredSize();
    contentsPreferredSize = contentsPreferredSize.expandedTo(sliderGadget->preferredSize());
    IntSize preferredSize = contentsPreferredSize + scrollbarGadget->preferredSize() - scrollbarGadget->minimumSize();

    return preferredSize.width();
}
#else
int ScrollbarThemeGtk::scrollbarThickness(ScrollbarControlSize)
{
    int thumbFat, troughBorderWidth;
    gtk_style_context_get_style(createStyleContext().get(), "slider-width", &thumbFat, "trough-border", &troughBorderWidth, nullptr);
    return thumbFat + 2 * troughBorderWidth;
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

#if GTK_CHECK_VERSION(3, 20, 0)
int ScrollbarThemeGtk::minimumThumbLength(Scrollbar& scrollbar)
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Scrollbar, "scrollbar", GTK_STATE_FLAG_PRELIGHT, { "vertical", "right", "hovering" } };
    if (m_usesOverlayScrollbars)
        info.classList.append("overlay-indicator");
    auto scrollbarGadget = RenderThemeGadget::create(info);
    info.type = RenderThemeGadget::Type::Generic;
    info.name = "contents";
    info.state = GTK_STATE_FLAG_NORMAL;
    info.classList.clear();
    Vector<RenderThemeGadget::Info> children = {{ RenderThemeGadget::Type::Generic, "trough", GTK_STATE_FLAG_PRELIGHT, { } } };
    auto contentsGadget = std::make_unique<RenderThemeBoxGadget>(info, GTK_ORIENTATION_VERTICAL, children, scrollbarGadget.get());
    info.name = "slider";
    IntSize minSize = RenderThemeGadget::create(info, contentsGadget->child(0))->minimumSize();
    return scrollbar.orientation() == VerticalScrollbar ? minSize.height() : minSize.width();
}
#else
int ScrollbarThemeGtk::minimumThumbLength(Scrollbar& scrollbar)
{
    int minThumbLength = 0;
    gtk_style_context_get_style(createStyleContext(&scrollbar).get(), "min-slider-length", &minThumbLength, nullptr);
    return minThumbLength;
}
#endif

#else // GTK_API_VERSION_2
bool ScrollbarThemeGtk::hasThumb(Scrollbar&)
{
    return false;
}

IntRect ScrollbarThemeGtk::backButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    return IntRect();
}

IntRect ScrollbarThemeGtk::forwardButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    return IntRect();
}

IntRect ScrollbarThemeGtk::trackRect(Scrollbar&, bool)
{
    return IntRect();
}

bool ScrollbarThemeGtk::hasButtons(Scrollbar&)
{
    return false;
}
#endif // GTK_API_VERSION_2

}
