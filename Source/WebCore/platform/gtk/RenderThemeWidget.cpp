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
#include "RenderThemeWidget.h"

#if GTK_CHECK_VERSION(3, 20, 0)

#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static HashMap<unsigned, std::unique_ptr<RenderThemeWidget>>& widgetMap()
{
    static NeverDestroyed<HashMap<unsigned, std::unique_ptr<RenderThemeWidget>>> map;
    return map;
}

RenderThemeWidget& RenderThemeWidget::getOrCreate(Type widgetType)
{
    auto addResult = widgetMap().ensure(static_cast<unsigned>(widgetType), [widgetType]() -> std::unique_ptr<RenderThemeWidget> {
        switch (widgetType) {
        case RenderThemeWidget::Type::VerticalScrollbarRight:
            return std::make_unique<RenderThemeScrollbar>(GTK_ORIENTATION_VERTICAL, RenderThemeScrollbar::Mode::Full);
        case RenderThemeWidget::Type::VerticalScrollbarLeft:
            return std::make_unique<RenderThemeScrollbar>(GTK_ORIENTATION_VERTICAL, RenderThemeScrollbar::Mode::Full, RenderThemeScrollbar::VerticalPosition::Left);
        case RenderThemeWidget::Type::HorizontalScrollbar:
            return std::make_unique<RenderThemeScrollbar>(GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbar::Mode::Full);
        case RenderThemeWidget::Type::VerticalScrollIndicatorRight:
            return std::make_unique<RenderThemeScrollbar>(GTK_ORIENTATION_VERTICAL, RenderThemeScrollbar::Mode::Indicator);
        case RenderThemeWidget::Type::VerticalScrollIndicatorLeft:
            return std::make_unique<RenderThemeScrollbar>(GTK_ORIENTATION_VERTICAL, RenderThemeScrollbar::Mode::Indicator, RenderThemeScrollbar::VerticalPosition::Left);
        case RenderThemeWidget::Type::HorizontalScrollIndicator:
            return std::make_unique<RenderThemeScrollbar>(GTK_ORIENTATION_HORIZONTAL, RenderThemeScrollbar::Mode::Indicator);
        case RenderThemeWidget::Type::CheckButton:
            return std::make_unique<RenderThemeToggleButton>(RenderThemeToggleButton::Type::Check);
        case RenderThemeWidget::Type::RadioButton:
            return std::make_unique<RenderThemeToggleButton>(RenderThemeToggleButton::Type::Radio);
        case RenderThemeWidget::Type::Button:
            return std::make_unique<RenderThemeButton>(RenderThemeButton::Default::No);
        case RenderThemeWidget::Type::ButtonDefault:
            return std::make_unique<RenderThemeButton>(RenderThemeButton::Default::Yes);
        case RenderThemeWidget::Type::ComboBox:
            return std::make_unique<RenderThemeComboBox>();
        case RenderThemeWidget::Type::Entry:
            return std::make_unique<RenderThemeEntry>();
        case RenderThemeWidget::Type::SelectedEntry:
            return std::make_unique<RenderThemeEntry>(RenderThemeEntry::Selected::Yes);
        case RenderThemeWidget::Type::SearchEntry:
            return std::make_unique<RenderThemeSearchEntry>();
        case RenderThemeWidget::Type::SpinButton:
            return std::make_unique<RenderThemeSpinButton>();
        case RenderThemeWidget::Type::VerticalSlider:
            return std::make_unique<RenderThemeSlider>(GTK_ORIENTATION_VERTICAL);
        case RenderThemeWidget::Type::HorizontalSlider:
            return std::make_unique<RenderThemeSlider>(GTK_ORIENTATION_HORIZONTAL);
        case RenderThemeWidget::Type::ProgressBar:
            return std::make_unique<RenderThemeProgressBar>(RenderThemeProgressBar::Mode::Determinate);
        case RenderThemeWidget::Type::IndeterminateProgressBar:
            return std::make_unique<RenderThemeProgressBar>(RenderThemeProgressBar::Mode::Indeterminate);
        case RenderThemeWidget::Type::ListView:
            return std::make_unique<RenderThemeListView>();
        case RenderThemeWidget::Type::Icon:
            return std::make_unique<RenderThemeIcon>();
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    });
    return *addResult.iterator->value;
}

void RenderThemeWidget::clearCache()
{
    widgetMap().clear();
}

RenderThemeWidget::~RenderThemeWidget() = default;

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
    m_contents = std::make_unique<RenderThemeBoxGadget>(info, GTK_ORIENTATION_VERTICAL, children, m_scrollbar.get());
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

RenderThemeToggleButton::RenderThemeToggleButton(Type toggleType)
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Generic, toggleType == Type::Check ? "checkbutton" : "radiobutton", { "text-button" } };
    m_button = RenderThemeGadget::create(info);
    if (toggleType == Type::Check) {
        info.type = RenderThemeGadget::Type::Check;
        info.name = "check";
    } else {
        info.type = RenderThemeGadget::Type::Radio;
        info.name = "radio";
    }
    info.classList.clear();
    m_toggle = RenderThemeGadget::create(info, m_button.get());
}

RenderThemeButton::RenderThemeButton(Default isDefault)
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Button, "button", { "text-button" } };
    if (isDefault == Default::Yes)
        info.classList.append("default");
    m_button = RenderThemeGadget::create(info);
}

RenderThemeComboBox::RenderThemeComboBox()
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Generic, "combobox", { } };
    m_comboBox = RenderThemeGadget::create(info);
    Vector<RenderThemeGadget::Info> children = {
        { RenderThemeGadget::Type::Generic, "button", { "combo" } }
    };
    info.name = "box";
    info.classList = { "horizontal", "linked" };
    m_box = std::make_unique<RenderThemeBoxGadget>(info, GTK_ORIENTATION_HORIZONTAL, children, m_comboBox.get());
    RenderThemeGadget* button = m_box->child(0);
    info.classList.removeLast();
    m_buttonBox = RenderThemeGadget::create(info, button);
    info.type = RenderThemeGadget::Type::Arrow;
    info.name = "arrow";
    info.classList = { };
    m_arrow = RenderThemeGadget::create(info, m_buttonBox.get());
}

RenderThemeEntry::RenderThemeEntry(Selected isSelected)
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::TextField, "entry", { } };
    m_entry = RenderThemeGadget::create(info);
    if (isSelected == Selected::Yes) {
        info.type = RenderThemeGadget::Type::Generic;
        info.name = "selection";
        m_selection = RenderThemeGadget::create(info, m_entry.get());
    }
}

RenderThemeSearchEntry::RenderThemeSearchEntry()
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Icon, "image", { "left" } };
    m_leftIcon = RenderThemeGadget::create(info, m_entry.get());
    static_cast<RenderThemeIconGadget*>(m_leftIcon.get())->setIconName("edit-find-symbolic");
    info.classList.clear();
    info.classList.append("right");
    m_rightIcon = RenderThemeGadget::create(info, m_entry.get());
    static_cast<RenderThemeIconGadget*>(m_rightIcon.get())->setIconName("edit-clear-symbolic");
}

RenderThemeSpinButton::RenderThemeSpinButton()
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Generic, "spinbutton", { "horizontal" } };
    m_spinButton = RenderThemeGadget::create(info);
    info.type = RenderThemeGadget::Type::TextField;
    info.name = "entry";
    info.classList.clear();
    m_entry = RenderThemeGadget::create(info, m_spinButton.get());
    info.type = RenderThemeGadget::Type::Icon;
    info.name = "button";
    info.classList.append("up");
    m_up = RenderThemeGadget::create(info, m_spinButton.get());
    auto* upIcon = static_cast<RenderThemeIconGadget*>(m_up.get());
    upIcon->setIconSize(RenderThemeIconGadget::IconSizeGtk::Menu);
    upIcon->setIconName("list-add-symbolic");
    info.classList[0] = "down";
    m_down = RenderThemeGadget::create(info, m_spinButton.get());
    auto* downIcon = static_cast<RenderThemeIconGadget*>(m_down.get());
    downIcon->setIconSize(RenderThemeIconGadget::IconSizeGtk::Menu);
    downIcon->setIconName("list-remove-symbolic");
}

RenderThemeSlider::RenderThemeSlider(GtkOrientation orientation)
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Generic, "scale", { } };
    if (orientation == GTK_ORIENTATION_VERTICAL)
        info.classList.append("vertical");
    else
        info.classList.append("horizontal");
    m_scale = RenderThemeGadget::create(info);
    info.name = "contents";
    info.classList.clear();
    m_contents = RenderThemeGadget::create(info, m_scale.get());
    info.name = "trough";
    m_trough = RenderThemeGadget::create(info, m_contents.get());
    info.name = "slider";
    m_slider = RenderThemeGadget::create(info, m_trough.get());
    info.name = "highlight";
    m_highlight = RenderThemeGadget::create(info, m_trough.get());
}

RenderThemeProgressBar::RenderThemeProgressBar(Mode mode)
{
    RenderThemeGadget::Info info = { RenderThemeGadget::Type::Generic, "progressbar", { "horizontal" } };
    m_progressBar = RenderThemeGadget::create(info);
    info.name = "trough";
    info.classList.clear();
    m_trough = RenderThemeGadget::create(info, m_progressBar.get());
    info.name = "progress";
    if (mode == Mode::Determinate)
        info.classList.append("pulse");
    m_progress = RenderThemeGadget::create(info, m_trough.get());
}

RenderThemeListView::RenderThemeListView()
    : m_treeview(RenderThemeGadget::create({ RenderThemeGadget::Type::Generic, "treeview", { "view" } }))
{
}

RenderThemeIcon::RenderThemeIcon()
    : m_icon(RenderThemeGadget::create({ RenderThemeGadget::Type::Icon, "image", { } }))
{
}

} // namepsace WebCore

#endif // GTK_CHECK_VERSION(3, 20, 0)
