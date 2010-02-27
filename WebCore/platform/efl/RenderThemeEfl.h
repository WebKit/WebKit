/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include <Ecore_Evas.h>
#include <Evas.h>
#include <cairo.h>

namespace WebCore {

enum FormType { // KEEP IN SYNC WITH edjeGroupFromFormType()
    Button,
    RadioButton,
    TextField,
    CheckBox,
    ComboBox,
    SearchField,
    SearchFieldDecoration,
    SearchFieldResultsButton,
    SearchFieldResultsDecoration,
    SearchFieldCancelButton,
    FormTypeLast
};

class RenderThemeEfl : public RenderTheme {
private:
    RenderThemeEfl(Page*);
    ~RenderThemeEfl();

public:
    static PassRefPtr<RenderTheme> create(Page*);

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle*) const { return true; }

    // A method asking if the theme is able to draw the focus ring.
    virtual bool supportsFocusRing(const RenderStyle*) const;

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject*) const;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const { return true; }

    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    virtual int baselinePosition(const RenderObject*) const;

    virtual Color platformActiveSelectionBackgroundColor() const { return m_activeSelectionBackgroundColor; }
    virtual Color platformInactiveSelectionBackgroundColor() const { return m_inactiveSelectionBackgroundColor; }
    virtual Color platformActiveSelectionForegroundColor() const { return m_activeSelectionForegroundColor; }
    virtual Color platformInactiveSelectionForegroundColor() const { return m_inactiveSelectionForegroundColor; }
    virtual Color platformFocusRingColor() const { return m_focusRingColor; }

    virtual void themeChanged();

    // Set platform colors and notify they changed
    void setActiveSelectionColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA);
    void setInactiveSelectionColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA);
    void setFocusRingColor(int r, int g, int b, int a);

    void setButtonTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA);
    void setComboTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA);
    void setEntryTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA);
    void setSearchTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA);

    void adjustSizeConstraints(RenderStyle* style, FormType type) const;


    // System fonts.
    virtual void systemFont(int propId, FontDescription&) const;

    virtual void adjustCheckboxStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintCheckbox(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustRadioStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintRadio(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustTextAreaStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintTextArea(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintMenuList(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldCancelButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

private:
    void createCanvas();
    void createEdje();
    void applyEdjeColors();
    void applyPartDescriptions();
    const char* edjeGroupFromFormType(FormType type) const;
    void applyEdjeStateFromForm(Evas_Object* o, ControlStates states);
    bool paintThemePart(RenderObject* o, FormType type, const RenderObject::PaintInfo& i, const IntRect& rect);

    Page* m_page;
    Color m_activeSelectionBackgroundColor;
    Color m_activeSelectionForegroundColor;
    Color m_inactiveSelectionBackgroundColor;
    Color m_inactiveSelectionForegroundColor;
    Color m_focusRingColor;
    Color m_buttonTextBackgroundColor;
    Color m_buttonTextForegroundColor;
    Color m_comboTextBackgroundColor;
    Color m_comboTextForegroundColor;
    Color m_entryTextBackgroundColor;
    Color m_entryTextForegroundColor;
    Color m_searchTextBackgroundColor;
    Color m_searchTextForegroundColor;
    Ecore_Evas* m_canvas;
    Evas_Object* m_edje;

    struct ThemePartDesc {
        FormType type;
        LengthSize min;
        LengthSize max;
        LengthBox padding;
    };
    void applyPartDescriptionFallback(struct ThemePartDesc* desc);
    void applyPartDescription(Evas_Object* o, struct ThemePartDesc* desc);

    struct ThemePartCacheEntry {
        FormType type;
        IntSize size;
        Ecore_Evas* ee;
        Evas_Object* o;
        cairo_surface_t* surface;
    };

    struct ThemePartDesc m_partDescs[FormTypeLast];

    // this should be small and not so frequently used,
    // so use a vector and do linear searches
    Vector<struct ThemePartCacheEntry *> m_partCache;

    // get (use, create or replace) entry from cache
    struct ThemePartCacheEntry* cacheThemePartGet(FormType type, const IntSize& size);
    // flush cache, deleting all entries
    void cacheThemePartFlush();

    // internal, used by cacheThemePartGet()
    bool themePartCacheEntryReset(struct ThemePartCacheEntry* ce, FormType type);
    bool themePartCacheEntrySurfaceCreate(struct ThemePartCacheEntry* ce);
    struct ThemePartCacheEntry* cacheThemePartNew(FormType type, const IntSize& size);
    struct ThemePartCacheEntry* cacheThemePartReset(FormType type, struct ThemePartCacheEntry* ce);
    struct ThemePartCacheEntry* cacheThemePartResizeAndReset(FormType type, const IntSize& size, struct ThemePartCacheEntry* ce);

};
}

#endif // RenderThemeEfl_h
