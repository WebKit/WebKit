/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Igalia S.L.
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

#ifndef RenderThemeGtk_h
#define RenderThemeGtk_h

#include "GRefPtr.h"
#include "gtkdrawing.h"
#include "RenderTheme.h"

namespace WebCore {

class RenderThemeGtk : public RenderTheme {
private:
    RenderThemeGtk();
    virtual ~RenderThemeGtk();

public:
    static PassRefPtr<RenderTheme> create();

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle* style) const { return true; }

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

    // The platform selection color.
    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;

    // List Box selection color
    virtual Color activeListBoxSelectionBackgroundColor() const;
    virtual Color activeListBoxSelectionForegroundColor() const;
    virtual Color inactiveListBoxSelectionBackgroundColor() const;
    virtual Color inactiveListBoxSelectionForegroundColor() const;

    virtual double caretBlinkInterval() const;

    virtual void platformColorsDidChange();

    // System fonts and colors.
    virtual void systemFont(int propId, FontDescription&) const;
    virtual Color systemColor(int cssValueId) const;

#if ENABLE(VIDEO)
    virtual String extraMediaControlsStyleSheet();
#endif

    void getIndicatorMetrics(ControlPart, int& indicatorSize, int& indicatorSpacing) const;

    GtkWidget* gtkScrollbar();

protected:
    virtual bool paintCheckbox(RenderObject* o, const PaintInfo& i, const IntRect& r);
    virtual void setCheckboxSize(RenderStyle* style) const;

    virtual bool paintRadio(RenderObject* o, const PaintInfo& i, const IntRect& r);
    virtual void setRadioSize(RenderStyle* style) const;

    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintButton(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintTextField(RenderObject*, const PaintInfo&, const IntRect&);

    virtual bool paintTextArea(RenderObject*, const PaintInfo&, const IntRect&);

    int popupInternalPaddingLeft(RenderStyle*) const;
    int popupInternalPaddingRight(RenderStyle*) const;
    int popupInternalPaddingTop(RenderStyle*) const;
    int popupInternalPaddingBottom(RenderStyle*) const;

    // The Mac port differentiates between the "menu list" and the "menu list button."
    // The former is used when a menu list button has been styled. This is used to ensure
    // Aqua themed controls whenever possible. We always want to use GTK+ theming, so
    // we don't maintain this differentiation.
    virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual void adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element* e) const;
    virtual bool paintMenuList(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMenuListButton(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsDecoration(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchField(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsButton(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldCancelButton(RenderObject*, const PaintInfo&, const IntRect&);

    virtual bool paintSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustSliderTrackStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

    virtual bool paintSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustSliderThumbStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

    virtual void adjustSliderThumbSize(RenderObject* object) const;

#if ENABLE(VIDEO)
    virtual void initMediaStyling(GtkStyle* style, bool force);
    virtual bool paintMediaFullscreenButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaPlayButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaMuteButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSeekBackButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSeekForwardButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);
#endif

#if ENABLE(PROGRESS_TAG)
    virtual double animationRepeatIntervalForProgressBar(RenderProgress*) const;
    virtual double animationDurationForProgressBar(RenderProgress*) const;
    virtual void adjustProgressBarStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintProgressBar(RenderObject*, const PaintInfo&, const IntRect&);
#endif

private:
    /*
     * hold the state
     */
    GtkWidget* gtkButton() const;
    GtkWidget* gtkEntry() const;
    GtkWidget* gtkTreeView() const;

    /*
     * unmapped GdkWindow having a container. This is holding all
     * our fake widgets
     */
    GtkContainer* gtkContainer() const;

    bool paintRenderObject(GtkThemeWidgetType, RenderObject*, GraphicsContext*, const IntRect& rect, int flags = 0);

    mutable GtkWidget* m_gtkWindow;
    mutable GtkContainer* m_gtkContainer;
    mutable GtkWidget* m_gtkButton;
    mutable GtkWidget* m_gtkEntry;
    mutable GtkWidget* m_gtkTreeView;

    mutable Color m_panelColor;
    mutable Color m_sliderColor;
    mutable Color m_sliderThumbColor;

    const int m_mediaIconSize;
    const int m_mediaSliderHeight;
    const int m_mediaSliderThumbWidth;
    const int m_mediaSliderThumbHeight;

    RefPtr<Image> m_fullscreenButton;
    RefPtr<Image> m_muteButton;
    RefPtr<Image> m_unmuteButton;
    RefPtr<Image> m_playButton;
    RefPtr<Image> m_pauseButton;
    RefPtr<Image> m_seekBackButton;
    RefPtr<Image> m_seekForwardButton;
    GtkThemeParts m_themeParts;
#ifdef GTK_API_VERSION_2
    bool m_themePartsHaveRGBAColormap;
#endif
    friend class WidgetRenderingContext;
};

}

#endif // RenderThemeGtk_h
