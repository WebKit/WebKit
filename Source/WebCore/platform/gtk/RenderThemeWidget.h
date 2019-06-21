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

#pragma once

#include <gtk/gtk.h>

#include "RenderThemeGadget.h"

namespace WebCore {

class RenderThemeWidget {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RenderThemeWidget);
public:
    enum class Type {
        VerticalScrollbarRight = 1,
        VerticalScrollbarLeft,
        HorizontalScrollbar,
        VerticalScrollIndicatorRight,
        VerticalScrollIndicatorLeft,
        HorizontalScrollIndicator,
        CheckButton,
        RadioButton,
        Button,
        ButtonDefault,
        ComboBox,
        Entry,
        SelectedEntry,
        SearchEntry,
        SpinButton,
        VerticalSlider,
        HorizontalSlider,
        ProgressBar,
        IndeterminateProgressBar,
        ListView,
        Icon,
        Window,
    };
    static RenderThemeWidget& getOrCreate(Type);
    static void clearCache();

    RenderThemeWidget() = default;
    virtual ~RenderThemeWidget();
};

class RenderThemeScrollbar final : public RenderThemeWidget {
public:
    enum class Mode { Full, Indicator };
    enum class VerticalPosition { Right, Left };
    RenderThemeScrollbar(GtkOrientation, Mode, VerticalPosition = VerticalPosition::Right);
    ~RenderThemeScrollbar() = default;

    RenderThemeGadget& scrollbar() const { return *m_scrollbar; }
    RenderThemeGadget& contents() const { return *m_contents; }
    RenderThemeGadget& slider() const { return *m_slider; }
    RenderThemeGadget& trough() const { return *m_contents->child(m_troughPosition); }
    RenderThemeGadget* stepper(RenderThemeScrollbarGadget::Steppers);

private:
    std::unique_ptr<RenderThemeGadget> m_scrollbar;
    std::unique_ptr<RenderThemeBoxGadget> m_contents;
    std::unique_ptr<RenderThemeGadget> m_slider;
    unsigned m_troughPosition;
    unsigned m_steppersPosition[4];
};

class RenderThemeToggleButton final : public RenderThemeWidget {
public:
    enum class Type { Check, Radio };
    explicit RenderThemeToggleButton(Type);
    ~RenderThemeToggleButton() = default;

    RenderThemeGadget& button() const { return *m_button; }
    RenderThemeGadget& toggle() const { return *m_toggle; }

private:
    std::unique_ptr<RenderThemeGadget> m_button;
    std::unique_ptr<RenderThemeGadget> m_toggle;
};

class RenderThemeButton final : public RenderThemeWidget {
public:
    enum class Default { No, Yes };
    explicit RenderThemeButton(Default);
    ~RenderThemeButton() = default;

    RenderThemeGadget& button() const { return *m_button; }

private:
    std::unique_ptr<RenderThemeGadget> m_button;
};

class RenderThemeComboBox final : public RenderThemeWidget {
public:
    RenderThemeComboBox();
    ~RenderThemeComboBox() = default;

    RenderThemeGadget& comboBox() const { return *m_comboBox; }
    RenderThemeGadget& box() const { return *m_box; }
    RenderThemeGadget& button() const { return *m_box->child(0); }
    RenderThemeGadget& buttonBox() const { return *m_buttonBox; }
    RenderThemeGadget& arrow() const { return *m_arrow; }

private:
    std::unique_ptr<RenderThemeGadget> m_comboBox;
    std::unique_ptr<RenderThemeBoxGadget> m_box;
    std::unique_ptr<RenderThemeGadget> m_buttonBox;
    std::unique_ptr<RenderThemeGadget> m_arrow;
};

class RenderThemeEntry : public RenderThemeWidget {
public:
    enum class Selected { No, Yes };
    explicit RenderThemeEntry(Selected = Selected::No);
    ~RenderThemeEntry() = default;

    RenderThemeGadget& entry() const { return *m_entry; }
    RenderThemeGadget* selection() const { return m_selection.get(); }

protected:
    std::unique_ptr<RenderThemeGadget> m_entry;
    std::unique_ptr<RenderThemeGadget> m_selection;
};

class RenderThemeSearchEntry final : public RenderThemeEntry {
public:
    RenderThemeSearchEntry();
    ~RenderThemeSearchEntry() = default;

    RenderThemeGadget& leftIcon() const { return *m_leftIcon; }
    RenderThemeGadget& rightIcon() const { return *m_rightIcon; }

private:
    std::unique_ptr<RenderThemeGadget> m_leftIcon;
    std::unique_ptr<RenderThemeGadget> m_rightIcon;
};

class RenderThemeSpinButton final : public RenderThemeWidget {
public:
    RenderThemeSpinButton();
    ~RenderThemeSpinButton() = default;

    RenderThemeGadget& spinButton() const { return *m_spinButton; }
    RenderThemeGadget& entry() const { return *m_entry; }
    RenderThemeGadget& up() const { return *m_up; }
    RenderThemeGadget& down() const { return *m_down; }

private:
    std::unique_ptr<RenderThemeGadget> m_spinButton;
    std::unique_ptr<RenderThemeGadget> m_entry;
    std::unique_ptr<RenderThemeGadget> m_up;
    std::unique_ptr<RenderThemeGadget> m_down;
};

class RenderThemeSlider final : public RenderThemeWidget {
public:
    explicit RenderThemeSlider(GtkOrientation);
    ~RenderThemeSlider() = default;

    RenderThemeGadget& scale() const { return *m_scale; }
    RenderThemeGadget& contents() const { return *m_contents; }
    RenderThemeGadget& trough() const { return *m_trough; }
    RenderThemeGadget& slider() const { return *m_slider; }
    RenderThemeGadget& highlight() const { return *m_highlight; }

private:
    std::unique_ptr<RenderThemeGadget> m_scale;
    std::unique_ptr<RenderThemeGadget> m_contents;
    std::unique_ptr<RenderThemeGadget> m_trough;
    std::unique_ptr<RenderThemeGadget> m_slider;
    std::unique_ptr<RenderThemeGadget> m_highlight;
};

class RenderThemeProgressBar final : public RenderThemeWidget {
public:
    enum class Mode { Determinate, Indeterminate };
    explicit RenderThemeProgressBar(Mode);
    ~RenderThemeProgressBar() = default;

    RenderThemeGadget& progressBar() const { return *m_progressBar; }
    RenderThemeGadget& trough() const { return *m_trough; }
    RenderThemeGadget& progress() const { return *m_progress; }

private:
    std::unique_ptr<RenderThemeGadget> m_progressBar;
    std::unique_ptr<RenderThemeGadget> m_trough;
    std::unique_ptr<RenderThemeGadget> m_progress;
};

class RenderThemeListView final : public RenderThemeWidget {
public:
    RenderThemeListView();
    ~RenderThemeListView() = default;

    RenderThemeGadget& treeview() const { return *m_treeview; }

private:
    std::unique_ptr<RenderThemeGadget> m_treeview;
};

class RenderThemeIcon final : public RenderThemeWidget {
public:
    RenderThemeIcon();
    ~RenderThemeIcon() = default;

    RenderThemeGadget& icon() const { return *m_icon; }

private:
    std::unique_ptr<RenderThemeGadget> m_icon;
};

class RenderThemeWindow final : public RenderThemeWidget {
public:
    RenderThemeWindow();
    ~RenderThemeWindow() = default;

    RenderThemeGadget& window() const { return *m_window; }

private:
    std::unique_ptr<RenderThemeGadget> m_window;

};

} // namespace WebCore
