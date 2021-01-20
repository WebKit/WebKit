/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "RenderThemeScrollbar.h"

#if !USE(GTK4)

#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static HashMap<unsigned, std::unique_ptr<RenderThemeScrollbar>>& widgetMap()
{
    static NeverDestroyed<HashMap<unsigned, std::unique_ptr<RenderThemeScrollbar>>> map;
    return map;
}

RenderThemeScrollbar& RenderThemeScrollbar::getOrCreate(Type widgetType)
{
    auto addResult = widgetMap().ensure(static_cast<unsigned>(widgetType), [widgetType]() -> std::unique_ptr<RenderThemeScrollbar> {
        switch (widgetType) {
        case RenderThemeScrollbar::Type::VerticalScrollbarRight:
            return makeUnique<RenderThemeScrollbar>(GTK_ORIENTATION_VERTICAL, RenderThemeScrollbar::Mode::Full);
        case RenderThemeScrollbar::Type::VerticalScrollbarLeft:
            return makeUnique<RenderThemeScrollbar>(GTK_ORIENTATION_VERTICAL, RenderThemeScrollbar::Mode::Full, RenderThemeScrollbar::VerticalPosition::Left);
        case RenderThemeScrollbar::Type::HorizontalScrollbar:
            return makeUnique<RenderThemeScrollbar>(GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbar::Mode::Full);
        case RenderThemeScrollbar::Type::VerticalScrollIndicatorRight:
            return makeUnique<RenderThemeScrollbar>(GTK_ORIENTATION_VERTICAL, RenderThemeScrollbar::Mode::Indicator);
        case RenderThemeScrollbar::Type::VerticalScrollIndicatorLeft:
            return makeUnique<RenderThemeScrollbar>(GTK_ORIENTATION_VERTICAL, RenderThemeScrollbar::Mode::Indicator, RenderThemeScrollbar::VerticalPosition::Left);
        case RenderThemeScrollbar::Type::HorizontalScrollIndicator:
            return makeUnique<RenderThemeScrollbar>(GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbar::Mode::Indicator);
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    });
    return *addResult.iterator->value;
}

void RenderThemeScrollbar::clearCache()
{
    widgetMap().clear();
}

RenderThemeScrollbar::RenderThemeScrollbar(GtkOrientation orientation, Mode mode, VerticalPosition verticalPosition)
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Scrollbar, "scrollbar", { } };
    if (orientation == GTK_ORIENTATION_VERTICAL) {
        info.classList.append("vertical");
        info.classList.append(verticalPosition == VerticalPosition::Right ? "right" : "left");
    } else {
        info.classList.append("horizontal");
        info.classList.append("bottom");
    }
    static bool usesOverlayScrollbars = g_strcmp0(g_getenv("GTK_OVERLAY_SCROLLING"), "0");
    if (usesOverlayScrollbars)
        info.classList.append("overlay-indicator");
    if (mode == Mode::Full)
        info.classList.append("hovering");
    m_scrollbar = RenderThemeGadget::create(info);

    Vector<RenderThemeGadget::Info> children;
    auto steppers = static_cast<RenderThemeScrollbarGadget*>(m_scrollbar.get())->steppers();
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::Backward)) {
        m_steppersPosition[0] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", { "up" } });
    }
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryForward)) {
        m_steppersPosition[1] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", { "down" } });
    }
    m_troughPosition = children.size();
    children.append({ RenderThemeGadget::Type::Generic, "trough", { } });
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::SecondaryBackward)) {
        m_steppersPosition[2] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", { "up" } });
    }
    if (steppers.contains(RenderThemeScrollbarGadget::Steppers::Forward)) {
        m_steppersPosition[3] = children.size();
        children.append({ RenderThemeGadget::Type::Generic, "button", { "down" } });
    }
    info.type = RenderThemeGadget::Type::Generic;
    info.name = "contents";
    info.classList.clear();
    m_contents = makeUnique<RenderThemeBoxGadget>(info, GTK_ORIENTATION_VERTICAL, children, m_scrollbar.get());
    info.name = "slider";
    m_slider = RenderThemeGadget::create(info, m_contents->child(m_troughPosition));
}

RenderThemeGadget* RenderThemeScrollbar::stepper(RenderThemeScrollbarGadget::Steppers scrollbarStepper)
{
    if (!static_cast<RenderThemeScrollbarGadget*>(m_scrollbar.get())->steppers().contains(scrollbarStepper))
        return nullptr;

    switch (scrollbarStepper) {
    case RenderThemeScrollbarGadget::Steppers::Backward:
        return m_contents->child(m_steppersPosition[0]);
    case RenderThemeScrollbarGadget::Steppers::SecondaryForward:
        return m_contents->child(m_steppersPosition[1]);
    case RenderThemeScrollbarGadget::Steppers::SecondaryBackward:
        return m_contents->child(m_steppersPosition[2]);
    case RenderThemeScrollbarGadget::Steppers::Forward:
        return m_contents->child(m_steppersPosition[3]);
    default:
        break;
    }

    return nullptr;
}

} // namespace WebCore

#endif // !USE(GTK4)
