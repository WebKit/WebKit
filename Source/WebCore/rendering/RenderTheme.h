/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "ColorHash.h"
#include "ControlStates.h"
#include "GraphicsContext.h"
#include "PaintInfo.h"
#include "PopupMenuStyle.h"
#include "ScrollTypes.h"
#include "StyleColor.h"
#include "ThemeTypes.h"
#include <wtf/HashMap.h>

namespace WebCore {

class BorderData;
class Element;
class FileList;
class FillLayer;
class HTMLInputElement;
class Icon;
class Page;
class RenderAttachment;
class RenderBox;
class RenderMeter;
class RenderObject;
class RenderProgress;
class RenderStyle;

class RenderTheme {
protected:
    RenderTheme();

    virtual ~RenderTheme() = default;

public:
    // This function is to be implemented in platform-specific theme implementations to hand back the
    // appropriate platform theme.
    WEBCORE_EXPORT static RenderTheme& singleton();

    virtual void purgeCaches();

    // This method is called whenever style has been computed for an element and the appearance
    // property has been set to a value other than "none".  The theme should map in all of the appropriate
    // metrics and defaults given the contents of the style.  This includes sophisticated operations like
    // selection of control size based off the font, the disabling of appearance when certain other properties like
    // "border" are set, or if the appearance is not supported by the theme.
    void adjustStyle(RenderStyle&, const Element*, const RenderStyle* userAgentAppearanceStyle);

    // This method is called to paint the widget as a background of the RenderObject.  A widget's foreground, e.g., the
    // text of a button, is always rendered by the engine itself.  The boolean return value indicates
    // whether the CSS border/background should also be painted.
    bool paint(const RenderBox&, ControlStates&, const PaintInfo&, const LayoutRect&);
    bool paintBorderOnly(const RenderBox&, const PaintInfo&, const LayoutRect&);
    bool paintDecorations(const RenderBox&, const PaintInfo&, const LayoutRect&);

    // The remaining methods should be implemented by the platform-specific portion of the theme, e.g.,
    // RenderThemeMac.cpp for Mac OS X.

    // These methods return the theme's extra style sheets rules, to let each platform
    // adjust the default CSS rules in html.css, quirks.css, mediaControls.css, or plugIns.css
    virtual String extraDefaultStyleSheet() { return String(); }
    virtual String extraQuirksStyleSheet() { return String(); }
    virtual String extraPlugInsStyleSheet() { return String(); }
#if ENABLE(VIDEO)
    virtual String mediaControlsStyleSheet() { return String(); }
    virtual String modernMediaControlsStyleSheet() { return String(); }
    virtual String extraMediaControlsStyleSheet() { return String(); }
    virtual String mediaControlsScript() { return String(); }
    virtual String mediaControlsBase64StringForIconNameAndType(const String&, const String&) { return String(); }
    virtual String mediaControlsFormattedStringForDuration(double) { return String(); }
#endif
#if ENABLE(FULLSCREEN_API)
    virtual String extraFullScreenStyleSheet() { return String(); }
#endif
#if ENABLE(SERVICE_CONTROLS)
    virtual String imageControlsStyleSheet() const { return String(); }
#endif
#if ENABLE(DATALIST_ELEMENT)
    String dataListStyleSheet() const;
#endif
#if ENABLE(INPUT_TYPE_COLOR)
    String colorInputStyleSheet() const;
#endif
#if ENABLE(INPUT_TYPE_DATE)
    String dateInputStyleSheet() const;
#endif
#if ENABLE(INPUT_TYPE_DATETIMELOCAL)
    String dateTimeLocalInputStyleSheet() const;
#endif
#if ENABLE(INPUT_TYPE_MONTH)
    String monthInputStyleSheet() const;
#endif
#if ENABLE(INPUT_TYPE_TIME)
    String timeInputStyleSheet() const;
#endif
#if ENABLE(INPUT_TYPE_WEEK)
    String weekInputStyleSheet() const;
#endif

    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    virtual int baselinePosition(const RenderBox&) const;

    // A method for asking if a control is a container or not.  Leaf controls have to have some special behavior (like
    // the baseline position API above).
    bool isControlContainer(ControlPart) const;

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject&) const { return false; }

    // Whether or not the control has been styled enough by the author to disable the native appearance.
    virtual bool isControlStyled(const RenderStyle&, const RenderStyle& userAgentStyle) const;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const { return false; }

    // Some controls may spill out of their containers (e.g., the check on an OS X checkbox).  When these controls repaint,
    // the theme needs to communicate this inflated rect to the engine so that it can invalidate the whole control.
    virtual void adjustRepaintRect(const RenderObject&, FloatRect&);

    // This method is called whenever a relevant state changes on a particular themed object, e.g., the mouse becomes pressed
    // or a control becomes disabled.
    virtual bool stateChanged(const RenderObject&, ControlStates::States) const;

    // This method is called whenever the theme changes on the system in order to flush cached resources from the
    // old theme.
    virtual void themeChanged() { }

    // A method asking if the theme is able to draw the focus ring.
    virtual bool supportsFocusRing(const RenderStyle&) const;

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle&) const { return false; }

    // A method asking if the platform is able to show datalist suggestions for a given input type.
    virtual bool supportsDataListUI(const AtomString&) const { return false; }

    // Text selection colors.
    Color activeSelectionBackgroundColor(OptionSet<StyleColor::Options>) const;
    Color inactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const;
    virtual Color transformSelectionBackgroundColor(const Color&, OptionSet<StyleColor::Options>) const;
    Color activeSelectionForegroundColor(OptionSet<StyleColor::Options>) const;
    Color inactiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const;

    // List box selection colors
    Color activeListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const;
    Color activeListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const;
    Color inactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const;
    Color inactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const;

    // Highlighting color for search matches.
    Color textSearchHighlightColor(OptionSet<StyleColor::Options>) const;

    virtual Color disabledTextColor(const Color& textColor, const Color& backgroundColor) const;

    WEBCORE_EXPORT Color focusRingColor(OptionSet<StyleColor::Options>) const;
    virtual Color platformFocusRingColor(OptionSet<StyleColor::Options>) const { return Color::black; }
    static void setCustomFocusRingColor(const Color&);
    static float platformFocusRingWidth() { return 3; }
    static float platformFocusRingOffset(float outlineWidth) { return std::max<float>(outlineWidth - platformFocusRingWidth(), 0); }
#if ENABLE(TOUCH_EVENTS)
    static Color tapHighlightColor();
    virtual Color platformTapHighlightColor() const;
#endif
    virtual void platformColorsDidChange();

    virtual Seconds caretBlinkInterval() const { return 500_ms; }

    // System fonts and colors for CSS.
    void systemFont(CSSValueID, FontCascadeDescription&) const;
    virtual Color systemColor(CSSValueID, OptionSet<StyleColor::Options>) const;

    virtual int minimumMenuListSize(const RenderStyle&) const { return 0; }

    virtual void adjustSliderThumbSize(RenderStyle&, const Element*) const;

    virtual LengthBox popupInternalPaddingBox(const RenderStyle&) const { return { 0, 0, 0, 0 }; }
    virtual bool popupOptionSupportsTextIndent() const { return false; }
    virtual PopupMenuStyle::PopupMenuSize popupMenuSize(const RenderStyle&, IntRect&) const { return PopupMenuStyle::PopupMenuSizeNormal; }

    virtual ScrollbarControlSize scrollbarControlSizeForPart(ControlPart) { return ScrollbarControlSize::Regular; }

    // Returns the repeat interval of the animation for the progress bar.
    virtual Seconds animationRepeatIntervalForProgressBar(RenderProgress&) const;
    // Returns the duration of the animation for the progress bar.
    virtual Seconds animationDurationForProgressBar(RenderProgress&) const;
    virtual IntRect progressBarRectForBounds(const RenderObject&, const IntRect&) const;

#if ENABLE(VIDEO)
    // Media controls
    virtual bool supportsClosedCaptioning() const { return false; }
    virtual bool hasOwnDisabledStateHandlingFor(ControlPart) const { return false; }
    virtual bool usesMediaControlStatusDisplay() { return false; }
    virtual bool usesMediaControlVolumeSlider() const { return true; }
    virtual bool usesVerticalVolumeSlider() const { return true; }
    virtual double mediaControlsFadeInDuration() { return 0.1; }
    virtual Seconds mediaControlsFadeOutDuration() { return 300_ms; }
    virtual String formatMediaControlsTime(float time) const;
    virtual String formatMediaControlsCurrentTime(float currentTime, float duration) const;
    virtual String formatMediaControlsRemainingTime(float currentTime, float duration) const;
    
    // Returns the media volume slider container's offset from the mute button.
    virtual LayoutPoint volumeSliderOffsetFromMuteButton(const RenderBox&, const LayoutSize&) const;
#endif

#if ENABLE(METER_ELEMENT)
    virtual IntSize meterSizeForBounds(const RenderMeter&, const IntRect&) const;
    virtual bool supportsMeter(ControlPart) const;
#endif

#if ENABLE(DATALIST_ELEMENT)
    // Returns the threshold distance for snapping to a slider tick mark.
    virtual LayoutUnit sliderTickSnappingThreshold() const;
    // Returns size of one slider tick mark for a horizontal track.
    // For vertical tracks we rotate it and use it. i.e. Width is always length along the track.
    virtual IntSize sliderTickSize() const = 0;
    // Returns the distance of slider tick origin from the slider track center.
    virtual int sliderTickOffsetFromTrackCenter() const = 0;
    void paintSliderTicks(const RenderObject&, const PaintInfo&, const IntRect&);
#endif

    virtual bool shouldHaveSpinButton(const HTMLInputElement&) const;
    virtual bool shouldHaveCapsLockIndicator(const HTMLInputElement&) const;

    // Functions for <select> elements.
    virtual bool delegatesMenuListRendering() const { return false; }
    virtual bool popsMenuByArrowKeys() const { return false; }
    virtual bool popsMenuBySpaceOrReturn() const { return false; }

    virtual String fileListDefaultLabel(bool multipleFilesAllowed) const;
    virtual String fileListNameForWidth(const FileList*, const FontCascade&, int width, bool multipleFilesAllowed) const;

    enum FileUploadDecorations { SingleFile, MultipleFiles };
    virtual bool paintFileUploadIconDecorations(const RenderObject& /*inputRenderer*/, const RenderObject& /*buttonRenderer*/, const PaintInfo&, const IntRect&, Icon*, FileUploadDecorations) { return true; }

#if ENABLE(SERVICE_CONTROLS)
    virtual IntSize imageControlsButtonSize(const RenderObject&) const { return IntSize(); }
    virtual IntSize imageControlsButtonPositionOffset() const { return IntSize(); }
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    virtual LayoutSize attachmentIntrinsicSize(const RenderAttachment&) const { return LayoutSize(); }
    virtual int attachmentBaseline(const RenderAttachment&) const { return -1; }
    virtual bool attachmentShouldAllowWidthToShrink(const RenderAttachment&) const { return false; }
#endif

    enum class InnerSpinButtonLayout { Vertical, HorizontalUpLeft, HorizontalUpRight };
    virtual InnerSpinButtonLayout innerSpinButtonLayout(const RenderObject&) const { return InnerSpinButtonLayout::Vertical; }

    virtual bool shouldMockBoldSystemFontForAccessibility() const { return false; }
    virtual void setShouldMockBoldSystemFontForAccessibility(bool) { }

#if USE(SYSTEM_PREVIEW)
    virtual void paintSystemPreviewBadge(Image&, const PaintInfo&, const FloatRect&);
#endif

protected:
    virtual bool canPaint(const PaintInfo&) const = 0;
    virtual FontCascadeDescription& cachedSystemFontDescription(CSSValueID systemFontID) const;
    virtual void updateCachedSystemFontDescription(CSSValueID systemFontID, FontCascadeDescription&) const = 0;

    // The platform selection color.
    virtual Color platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const;
    virtual Color platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const;
    virtual Color platformActiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const;
    virtual Color platformInactiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const;

    virtual Color platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const;
    virtual Color platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const;
    virtual Color platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const;
    virtual Color platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const;

    virtual Color platformTextSearchHighlightColor(OptionSet<StyleColor::Options>) const;

    virtual bool supportsSelectionForegroundColors(OptionSet<StyleColor::Options>) const { return true; }
    virtual bool supportsListBoxSelectionForegroundColors(OptionSet<StyleColor::Options>) const { return true; }

#if !USE(NEW_THEME)
    // Methods for each appearance value.
    virtual void adjustCheckboxStyle(RenderStyle&, const Element*) const;
    virtual bool paintCheckbox(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual void setCheckboxSize(RenderStyle&) const { }

    virtual void adjustRadioStyle(RenderStyle&, const Element*) const;
    virtual bool paintRadio(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual void setRadioSize(RenderStyle&) const { }

    virtual void adjustButtonStyle(RenderStyle&, const Element*) const;
    virtual bool paintButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual void setButtonSize(RenderStyle&) const { }

    virtual void adjustInnerSpinButtonStyle(RenderStyle&, const Element*) const;
    virtual bool paintInnerSpinButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
#endif

    virtual bool paintCheckboxDecorations(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintRadioDecorations(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintButtonDecorations(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }

    virtual void adjustTextFieldStyle(RenderStyle&, const Element*) const;
    virtual bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) { return true; }
    virtual bool paintTextFieldDecorations(const RenderObject&, const PaintInfo&, const FloatRect&) { return true; }

    virtual void adjustTextAreaStyle(RenderStyle&, const Element*) const;
    virtual bool paintTextArea(const RenderObject&, const PaintInfo&, const FloatRect&) { return true; }
    virtual bool paintTextAreaDecorations(const RenderObject&, const PaintInfo&, const FloatRect&) { return true; }

    virtual void adjustMenuListStyle(RenderStyle&, const Element*) const;
    virtual bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) { return true; }
    virtual bool paintMenuListDecorations(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }

    virtual void adjustMenuListButtonStyle(RenderStyle&, const Element*) const;
    virtual bool paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) { return true; }

    virtual bool paintPushButtonDecorations(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintSquareButtonDecorations(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }

#if ENABLE(METER_ELEMENT)
    virtual void adjustMeterStyle(RenderStyle&, const Element*) const;
    virtual bool paintMeter(const RenderObject&, const PaintInfo&, const IntRect&);
#endif

    virtual void adjustCapsLockIndicatorStyle(RenderStyle&, const Element*) const;
    virtual bool paintCapsLockIndicator(const RenderObject&, const PaintInfo&, const IntRect&);

#if ENABLE(APPLE_PAY)
    virtual void adjustApplePayButtonStyle(RenderStyle&, const Element*) const { }
    virtual bool paintApplePayButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    virtual void adjustAttachmentStyle(RenderStyle&, const Element*) const;
    virtual bool paintAttachment(const RenderObject&, const PaintInfo&, const IntRect&);
#endif

#if ENABLE(DATALIST_ELEMENT)
    virtual void adjustListButtonStyle(RenderStyle&, const Element*) const;
#endif

    virtual void adjustProgressBarStyle(RenderStyle&, const Element*) const;
    virtual bool paintProgressBar(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSliderTrackStyle(RenderStyle&, const Element*) const;
    virtual bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSliderThumbStyle(RenderStyle&, const Element*) const;
    virtual bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintSliderThumbDecorations(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSearchFieldStyle(RenderStyle&, const Element*) const;
    virtual bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintSearchFieldDecorations(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSearchFieldCancelButtonStyle(RenderStyle&, const Element*) const;
    virtual bool paintSearchFieldCancelButton(const RenderBox&, const PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSearchFieldDecorationPartStyle(RenderStyle&, const Element*) const;
    virtual bool paintSearchFieldDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSearchFieldResultsDecorationPartStyle(RenderStyle&, const Element*) const;
    virtual bool paintSearchFieldResultsDecorationPart(const RenderBox&, const PaintInfo&, const IntRect&) { return true; }

    virtual void adjustSearchFieldResultsButtonStyle(RenderStyle&, const Element*) const;
    virtual bool paintSearchFieldResultsButton(const RenderBox&, const PaintInfo&, const IntRect&) { return true; }

    virtual void adjustMediaControlStyle(RenderStyle&, const Element*) const;
    virtual bool paintMediaFullscreenButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaPlayButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaOverlayPlayButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaMuteButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaSeekBackButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaSeekForwardButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaVolumeSliderContainer(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaVolumeSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaVolumeSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaRewindButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaReturnToRealtimeButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaToggleClosedCaptionsButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaControlsBackground(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaCurrentTime(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaTimeRemaining(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaFullScreenVolumeSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
    virtual bool paintMediaFullScreenVolumeSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }

    virtual bool paintSnapshottedPluginOverlay(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }

#if ENABLE(SERVICE_CONTROLS)
    virtual bool paintImageControlsButton(const RenderObject&, const PaintInfo&, const IntRect&) { return true; }
#endif

public:
    void updateControlStatesForRenderer(const RenderBox&, ControlStates&) const;
    ControlStates::States extractControlStatesForRenderer(const RenderObject&) const;
    bool isActive(const RenderObject&) const;
    bool isChecked(const RenderObject&) const;
    bool isIndeterminate(const RenderObject&) const;
    bool isEnabled(const RenderObject&) const;
    bool isFocused(const RenderObject&) const;
    bool isPressed(const RenderObject&) const;
    bool isSpinUpButtonPartPressed(const RenderObject&) const;
    bool isHovered(const RenderObject&) const;
    bool isSpinUpButtonPartHovered(const RenderObject&) const;
    bool isPresenting(const RenderObject&) const;
    bool isReadOnlyControl(const RenderObject&) const;
    bool isDefault(const RenderObject&) const;

protected:
    struct ColorCache {
        HashMap<int, Color> systemStyleColors;

        Color systemLinkColor;
        Color systemActiveLinkColor;
        Color systemVisitedLinkColor;
        Color systemFocusRingColor;
        Color systemControlAccentColor;

        Color activeSelectionBackgroundColor;
        Color inactiveSelectionBackgroundColor;
        Color activeSelectionForegroundColor;
        Color inactiveSelectionForegroundColor;

        Color activeListBoxSelectionBackgroundColor;
        Color inactiveListBoxSelectionBackgroundColor;
        Color activeListBoxSelectionForegroundColor;
        Color inactiveListBoxSelectionForegroundColor;

        Color textSearchHighlightColor;
    };

    virtual ColorCache& colorCache(OptionSet<StyleColor::Options>) const;

private:
    mutable HashMap<uint8_t, ColorCache, DefaultHash<uint8_t>, WTF::UnsignedWithZeroKeyHashTraits<uint8_t>> m_colorCacheMap;
};

} // namespace WebCore
