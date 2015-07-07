/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
 * All rights reserved.
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
 *
 */

#ifndef RenderThemeEfl_h
#define RenderThemeEfl_h

#include "RenderTheme.h"

#include <Eina.h>
#include <cairo.h>
#include <wtf/efl/UniquePtrEfl.h>

namespace WebCore {

enum FormType { // KEEP IN SYNC WITH edjeGroupFromFormType()
    Button,
    RadioButton,
    TextField,
    CheckBox,
    ComboBox,
    ProgressBar,
    SearchField,
    SearchFieldResultsButton,
    SearchFieldResultsDecoration,
    SearchFieldCancelButton,
    SliderVertical,
    SliderHorizontal,
    SliderThumbVertical,
    SliderThumbHorizontal,
    Spinner,
    FormTypeLast
};

class RenderThemeEfl final : public RenderTheme {
private:
    explicit RenderThemeEfl(Page*);
    virtual ~RenderThemeEfl();

public:
    static PassRefPtr<RenderTheme> create(Page*);

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle&) const override { return true; }

    // A method Returning whether the control is styled by css or not e.g specifying background-color.
    virtual bool isControlStyled(const RenderStyle&, const BorderData&, const FillLayer&, const Color& backgroundColor) const override;

    // A method asking if the theme is able to draw the focus ring.
    virtual bool supportsFocusRing(const RenderStyle&) const override;

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject&) const override;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const override { return true; }

    // A general method asking if foreground colors of selection are supported.
    virtual bool supportsSelectionForegroundColors() const override;

    // A method to obtain the baseline position for a "leaf" control. This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    virtual int baselinePosition(const RenderObject&) const override;

    virtual Color platformActiveSelectionBackgroundColor() const override;
    virtual Color platformInactiveSelectionBackgroundColor() const override;
    virtual Color platformActiveSelectionForegroundColor() const override;
    virtual Color platformInactiveSelectionForegroundColor() const override;
    virtual Color platformFocusRingColor() const override;

    // Set platform colors; remember to call platformColorDidChange after.
    void setColorFromThemeClass(const char* colorClass);

    void setButtonTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA);
    void setComboTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA);
    void setEntryTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA);
    void setSearchTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA);

    void adjustSizeConstraints(RenderStyle&, FormType) const;

    // System fonts.
    virtual void systemFont(CSSValueID, FontDescription&) const override;

    virtual void adjustCheckboxStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintCheckbox(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustRadioStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintRadio(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustTextFieldStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) override;

    virtual void adjustTextAreaStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintTextArea(const RenderObject&, const PaintInfo&, const FloatRect&) override;

    virtual void adjustMenuListStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) override;

    virtual void adjustMenuListButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintMenuListButtonDecorations(const RenderObject&, const PaintInfo&, const FloatRect&) override;

    virtual void adjustSearchFieldResultsDecorationPartStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldResultsDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldResultsButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldResultsButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldCancelButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldCancelButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSliderTrackStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSliderThumbStyle(StyleResolver&, RenderStyle&, Element&) const override;

    virtual void adjustSliderThumbSize(RenderStyle&, Element&) const override;

#if ENABLE(DATALIST_ELEMENT)
    virtual IntSize sliderTickSize() const override;
    virtual int sliderTickOffsetFromTrackCenter() const override;
    virtual LayoutUnit sliderTickSnappingThreshold() const override;
#endif

    virtual bool supportsDataListUI(const AtomicString&) const override;

    virtual bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustInnerSpinButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintInnerSpinButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    static void setDefaultFontSize(int fontsize);

    virtual void adjustProgressBarStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintProgressBar(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual double animationRepeatIntervalForProgressBar(RenderProgress&) const override;
    virtual double animationDurationForProgressBar(RenderProgress&) const override;

#if ENABLE(VIDEO)
    virtual String mediaControlsStyleSheet() override;
    virtual String mediaControlsScript() override;
#endif
#if ENABLE(VIDEO_TRACK)
    virtual bool supportsClosedCaptioning() const override { return true; }
#endif
    virtual bool shouldShowPlaceholderWhenFocused() const override { return true; }

    void setThemePath(const String&);
    String themePath() const;

protected:
    static float defaultFontSize;

private:
    bool loadTheme();
    ALWAYS_INLINE bool loadThemeIfNeeded() const
    {
        return m_edje || (!m_themePath.isEmpty() && const_cast<RenderThemeEfl*>(this)->loadTheme());
    }

    ALWAYS_INLINE Ecore_Evas* canvas() const { return m_canvas.get(); }
    ALWAYS_INLINE Evas_Object* edje() const { return m_edje.get(); }

    void applyPartDescriptionsFrom(const String& themePath);

    void applyEdjeStateFromForm(Evas_Object*, const ControlStates*, bool);
    void applyEdjeRTLState(Evas_Object*, const RenderObject&, FormType, const IntRect&);
    bool paintThemePart(const RenderObject&, FormType, const PaintInfo&, const IntRect&);

    Page* m_page;
    Color m_activeSelectionBackgroundColor;
    Color m_activeSelectionForegroundColor;
    Color m_inactiveSelectionBackgroundColor;
    Color m_inactiveSelectionForegroundColor;
    Color m_focusRingColor;
    Color m_sliderThumbColor;

    String m_themePath;
    // Order so that the canvas gets destroyed at last.
    EflUniquePtr<Ecore_Evas> m_canvas;
    EflUniquePtr<Evas_Object> m_edje;

    bool m_supportsSelectionForegroundColor;

    struct ThemePartDesc {
        FormType type;
        LengthSize min;
        LengthSize max;
        LengthBox padding;
    };
    void applyPartDescriptionFallback(struct ThemePartDesc*);
    void applyPartDescription(Evas_Object*, struct ThemePartDesc*);

    struct ThemePartCacheEntry {
        static PassOwnPtr<RenderThemeEfl::ThemePartCacheEntry> create(const String& themePath, FormType, const IntSize&);
        void reuse(const String& themePath, FormType, const IntSize&);

        ALWAYS_INLINE Ecore_Evas* canvas() { return m_canvas.get(); }
        ALWAYS_INLINE Evas_Object* edje() { return m_edje.get(); }
        ALWAYS_INLINE cairo_surface_t* surface() { return m_surface.get(); }

        FormType type;
        IntSize size;

    private:
        // Order so that the canvas gets destroyed at last.
        EflUniquePtr<Ecore_Evas> m_canvas;
        EflUniquePtr<Evas_Object> m_edje;
        RefPtr<cairo_surface_t> m_surface;
    };

    struct ThemePartDesc m_partDescs[FormTypeLast];

    // List of ThemePartCacheEntry* sorted so that the most recently
    // used entries come first. We use a list for efficient moving
    // of items within the container.
    Eina_List* m_partCache;

    ThemePartCacheEntry* getThemePartFromCache(FormType, const IntSize&);
    void clearThemePartCache();
};
}

#endif // RenderThemeEfl_h
