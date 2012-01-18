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
#include "RenderTheme.h"

typedef struct _GdkColormap GdkColormap;

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

    virtual void adjustRepaintRect(const RenderObject*, IntRect&);

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

    virtual bool popsMenuBySpaceOrReturn() const OVERRIDE { return true; }

#if ENABLE(VIDEO)
    virtual String extraMediaControlsStyleSheet();
    virtual String formatMediaControlsCurrentTime(float currentTime, float duration) const;

#if ENABLE(FULLSCREEN_API)
    virtual String extraFullScreenStyleSheet();
#endif
#endif

#ifdef GTK_API_VERSION_2
    GtkWidget* gtkContainer() const;
    GtkWidget* gtkEntry() const;
    GtkWidget* gtkVScrollbar() const;
    GtkWidget* gtkHScrollbar() const;
    static void getIndicatorMetrics(ControlPart, int& indicatorSize, int& indicatorSpacing);
#else
    GtkStyleContext* gtkScrollbarStyle();
#endif

protected:
    virtual bool paintCheckbox(RenderObject* o, const PaintInfo& i, const IntRect& r);
    virtual void setCheckboxSize(RenderStyle* style) const;

    virtual bool paintRadio(RenderObject* o, const PaintInfo& i, const IntRect& r);
    virtual void setRadioSize(RenderStyle* style) const;

    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintButton(RenderObject*, const PaintInfo&, const IntRect&);

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

    virtual void adjustSliderThumbSize(RenderStyle*) const;

#if ENABLE(VIDEO)
    void initMediaColors();
    void initMediaButtons();
    void adjustMediaSliderThumbSize(RenderStyle*) const;
    virtual bool hasOwnDisabledStateHandlingFor(ControlPart) const { return true; }
    virtual bool paintMediaFullscreenButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaPlayButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaMuteButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSeekBackButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSeekForwardButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaVolumeSliderContainer(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaVolumeSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaVolumeSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool paintMediaCurrentTime(RenderObject*, const PaintInfo&, const IntRect&);
#endif

#if ENABLE(PROGRESS_TAG)
    virtual double animationRepeatIntervalForProgressBar(RenderProgress*) const;
    virtual double animationDurationForProgressBar(RenderProgress*) const;
    virtual void adjustProgressBarStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintProgressBar(RenderObject*, const PaintInfo&, const IntRect&);
#endif

    virtual bool paintCapsLockIndicator(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustInnerSpinButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintInnerSpinButton(RenderObject*, const PaintInfo&, const IntRect&);

private:
    virtual String fileListNameForWidth(const Vector<String>& filenames, const Font&, int width, bool multipleFilesAllowed);

    void platformInit();
    static void setTextInputBorders(RenderStyle*);
    static double getScreenDPI();

#if ENABLE(VIDEO)
    bool paintMediaButton(RenderObject*, GraphicsContext*, const IntRect&, const char* iconName);
#endif

#if ENABLE(PROGRESS_TAG)
    static IntRect calculateProgressRect(RenderObject*, const IntRect&);
#endif

    mutable Color m_panelColor;
    mutable Color m_sliderColor;
    mutable Color m_sliderThumbColor;
    const int m_mediaIconSize;
    const int m_mediaSliderHeight;
    const int m_mediaSliderThumbWidth;
    const int m_mediaSliderThumbHeight;

#ifdef GTK_API_VERSION_2
    void setupWidgetAndAddToContainer(GtkWidget*, GtkWidget*) const;
    void refreshComboBoxChildren() const;
    void getComboBoxPadding(RenderStyle*, int& left, int& top, int& right, int& bottom) const;
    int getComboBoxSeparatorWidth() const;
    int comboBoxArrowSize(RenderStyle*) const;

    GtkWidget* gtkButton() const;
    GtkWidget* gtkTreeView() const;
    GtkWidget* gtkVScale() const;
    GtkWidget* gtkHScale() const;
    GtkWidget* gtkRadioButton() const;
    GtkWidget* gtkCheckButton() const;
    GtkWidget* gtkProgressBar() const;
    GtkWidget* gtkComboBox() const;
    GtkWidget* gtkComboBoxButton() const;
    GtkWidget* gtkComboBoxArrow() const;
    GtkWidget* gtkComboBoxSeparator() const;
    GtkWidget* gtkSpinButton() const;

    GdkColormap* m_colormap;
    mutable GtkWidget* m_gtkWindow;
    mutable GtkWidget* m_gtkContainer;
    mutable GtkWidget* m_gtkButton;
    mutable GtkWidget* m_gtkEntry;
    mutable GtkWidget* m_gtkTreeView;
    mutable GtkWidget* m_gtkVScale;
    mutable GtkWidget* m_gtkHScale;
    mutable GtkWidget* m_gtkRadioButton;
    mutable GtkWidget* m_gtkCheckButton;
    mutable GtkWidget* m_gtkProgressBar;
    mutable GtkWidget* m_gtkComboBox;
    mutable GtkWidget* m_gtkComboBoxButton;
    mutable GtkWidget* m_gtkComboBoxArrow;
    mutable GtkWidget* m_gtkComboBoxSeparator;
    mutable GtkWidget* m_gtkVScrollbar;
    mutable GtkWidget* m_gtkHScrollbar;
    mutable GtkWidget* m_gtkSpinButton;
    bool m_themePartsHaveRGBAColormap;
    friend class WidgetRenderingContext;
#endif
};

}

#endif // RenderThemeGtk_h
