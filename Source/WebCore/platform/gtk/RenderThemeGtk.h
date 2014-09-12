/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Inc.
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

#include "RenderTheme.h"

namespace WebCore {

class RenderThemeGtk final : public RenderTheme {
public:
    static PassRefPtr<RenderTheme> create();

    // System fonts.
    virtual void systemFont(CSSValueID, FontDescription&) const override;

#if ENABLE(DATALIST_ELEMENT)
    // Returns size of one slider tick mark for a horizontal track.
    // For vertical tracks we rotate it and use it. i.e. Width is always length along the track.
    virtual IntSize sliderTickSize() const override;
    // Returns the distance of slider tick origin from the slider track center.
    virtual int sliderTickOffsetFromTrackCenter() const override;
#endif

#ifndef GTK_API_VERSION_2

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle&) const override { return true; }

    // A method asking if the theme is able to draw the focus ring.
    virtual bool supportsFocusRing(const RenderStyle&) const override;

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject&) const override;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const override { return true; }

    virtual void adjustRepaintRect(const RenderObject&, FloatRect&) override;

    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    virtual int baselinePosition(const RenderObject&) const override;

    // The platform selection color.
    virtual Color platformActiveSelectionBackgroundColor() const override;
    virtual Color platformInactiveSelectionBackgroundColor() const override;
    virtual Color platformActiveSelectionForegroundColor() const override;
    virtual Color platformInactiveSelectionForegroundColor() const override;

    // List Box selection color
    virtual Color platformActiveListBoxSelectionBackgroundColor() const override;
    virtual Color platformActiveListBoxSelectionForegroundColor() const override;
    virtual Color platformInactiveListBoxSelectionBackgroundColor() const override;
    virtual Color platformInactiveListBoxSelectionForegroundColor() const override;

    virtual double caretBlinkInterval() const override;

    virtual void platformColorsDidChange() override;

    // System colors.
    virtual Color systemColor(CSSValueID) const override;

    virtual bool popsMenuBySpaceOrReturn() const override { return true; }

#if ENABLE(VIDEO)
    virtual String extraMediaControlsStyleSheet() override;
    virtual String formatMediaControlsCurrentTime(float currentTime, float duration) const override;
    virtual bool supportsClosedCaptioning() const override { return true; }
    virtual String mediaControlsScript() override;

#if ENABLE(FULLSCREEN_API)
    virtual String extraFullScreenStyleSheet() override;
#endif
#endif

private:
    RenderThemeGtk();
    virtual ~RenderThemeGtk();

    virtual bool paintCheckbox(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual void setCheckboxSize(RenderStyle&) const override;

    virtual bool paintRadio(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual void setRadioSize(RenderStyle&) const override;

    virtual void adjustButtonStyle(StyleResolver&, RenderStyle&, Element*) const override;
    virtual bool paintButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    virtual bool paintTextArea(const RenderObject&, const PaintInfo&, const FloatRect&) override;

    int popupInternalPaddingLeft(RenderStyle&) const override;
    int popupInternalPaddingRight(RenderStyle&) const override;
    int popupInternalPaddingTop(RenderStyle&) const override;
    int popupInternalPaddingBottom(RenderStyle&) const override;

    // The Mac port differentiates between the "menu list" and the "menu list button."
    // The former is used when a menu list button has been styled. This is used to ensure
    // Aqua themed controls whenever possible. We always want to use GTK+ theming, so
    // we don't maintain this differentiation.
    virtual void adjustMenuListStyle(StyleResolver&, RenderStyle&, Element*) const override;
    virtual void adjustMenuListButtonStyle(StyleResolver&, RenderStyle&, Element*) const override;
    virtual bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    virtual bool paintMenuListButtonDecorations(const RenderObject&, const PaintInfo&, const FloatRect&) override;

    virtual void adjustSearchFieldResultsDecorationPartStyle(StyleResolver&, RenderStyle&, Element*) const override;
    virtual bool paintSearchFieldResultsDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldStyle(StyleResolver&, RenderStyle&, Element*) const override;
    virtual bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldResultsButtonStyle(StyleResolver&, RenderStyle&, Element*) const override;
    virtual bool paintSearchFieldResultsButton(const RenderObject&, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldCancelButtonStyle(StyleResolver&, RenderStyle&, Element*) const override;
    virtual bool paintSearchFieldCancelButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual void adjustSliderTrackStyle(StyleResolver&, RenderStyle&, Element*) const override;

    virtual bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual void adjustSliderThumbStyle(StyleResolver&, RenderStyle&, Element*) const override;

    virtual void adjustSliderThumbSize(RenderStyle&, Element*) const override;

#if ENABLE(VIDEO)
    void initMediaColors();
    void initMediaButtons();
    virtual bool hasOwnDisabledStateHandlingFor(ControlPart) const override;
    virtual bool paintMediaFullscreenButton(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaPlayButton(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaMuteButton(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaSeekBackButton(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaSeekForwardButton(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderContainer(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaCurrentTime(const RenderObject&, const PaintInfo&, const IntRect&) override;
#if ENABLE(VIDEO_TRACK)
    virtual bool paintMediaToggleClosedCaptionsButton(const RenderObject&, const PaintInfo&, const IntRect&) override;
#endif
#endif

    virtual double animationRepeatIntervalForProgressBar(RenderProgress&) const override;
    virtual double animationDurationForProgressBar(RenderProgress&) const override;
    virtual void adjustProgressBarStyle(StyleResolver&, RenderStyle&, Element*) const override;
    virtual bool paintProgressBar(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual bool paintCapsLockIndicator(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustInnerSpinButtonStyle(StyleResolver&, RenderStyle&, Element*) const override;
    virtual bool paintInnerSpinButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual String fileListNameForWidth(const FileList*, const Font&, int width, bool multipleFilesAllowed) const override;

    static void setTextInputBorders(RenderStyle&);

#if ENABLE(VIDEO)
    bool paintMediaButton(const RenderObject&, GraphicsContext*, const IntRect&, const char* symbolicIconName, const char* fallbackStockIconName);
#endif

    static IntRect calculateProgressRect(const RenderObject&, const IntRect&);

    mutable Color m_panelColor;
    mutable Color m_sliderColor;
    mutable Color m_sliderThumbColor;
    const int m_mediaIconSize;
    const int m_mediaSliderHeight;
#endif // GTK_API_VERSION_2
};

}

#endif // RenderThemeGtk_h
