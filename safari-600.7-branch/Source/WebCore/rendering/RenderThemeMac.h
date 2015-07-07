/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
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
#if !PLATFORM(IOS)

#ifndef RenderThemeMac_h
#define RenderThemeMac_h

#import "RenderTheme.h"
#import <wtf/RetainPtr.h>
#import <wtf/HashMap.h>

#if ENABLE(SERVICE_CONTROLS)
OBJC_CLASS NSServicesRolloverButtonCell;
#endif

OBJC_CLASS WebCoreRenderThemeNotificationObserver;

namespace WebCore {

class RenderProgress;
class RenderStyle;

class RenderThemeMac final : public RenderTheme {
public:
    static PassRefPtr<RenderTheme> create();

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject&) const override;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const override { return true; }

    virtual void adjustRepaintRect(const RenderObject&, FloatRect&) override;

    virtual bool isControlStyled(const RenderStyle&, const BorderData&, const FillLayer&, const Color& backgroundColor) const override;

    virtual Color platformActiveSelectionBackgroundColor() const override;
    virtual Color platformInactiveSelectionBackgroundColor() const override;
    virtual Color platformActiveListBoxSelectionBackgroundColor() const override;
    virtual Color platformActiveListBoxSelectionForegroundColor() const override;
    virtual Color platformInactiveListBoxSelectionBackgroundColor() const override;
    virtual Color platformInactiveListBoxSelectionForegroundColor() const override;
    virtual Color platformFocusRingColor() const override;
    virtual int platformFocusRingMaxWidth() const override;

    virtual ScrollbarControlSize scrollbarControlSizeForPart(ControlPart) override { return SmallScrollbar; }

    virtual void platformColorsDidChange() override;

    // System fonts.
    virtual void systemFont(CSSValueID, FontDescription&) const override;

    virtual int minimumMenuListSize(RenderStyle&) const override;

    virtual void adjustSliderThumbSize(RenderStyle&, Element&) const override;

#if ENABLE(DATALIST_ELEMENT)
    virtual IntSize sliderTickSize() const override;
    virtual int sliderTickOffsetFromTrackCenter() const override;
#endif

    virtual int popupInternalPaddingLeft(RenderStyle&) const override;
    virtual int popupInternalPaddingRight(RenderStyle&) const override;
    virtual int popupInternalPaddingTop(RenderStyle&) const override;
    virtual int popupInternalPaddingBottom(RenderStyle&) const override;
    virtual PopupMenuStyle::PopupMenuSize popupMenuSize(const RenderStyle&, IntRect&) const override;

    virtual bool paintCapsLockIndicator(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual bool popsMenuByArrowKeys() const override { return true; }

#if ENABLE(METER_ELEMENT)
    virtual IntSize meterSizeForBounds(const RenderMeter&, const IntRect&) const override;
    virtual bool paintMeter(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual bool supportsMeter(ControlPart) const override;
#endif

    // Returns the repeat interval of the animation for the progress bar.
    virtual double animationRepeatIntervalForProgressBar(RenderProgress&) const override;
    // Returns the duration of the animation for the progress bar.
    virtual double animationDurationForProgressBar(RenderProgress&) const override;
    virtual IntRect progressBarRectForBounds(const RenderObject&, const IntRect&) const override;

    virtual Color systemColor(CSSValueID) const override;
    // Controls color values returned from platformFocusRingColor(). systemColor() will be used when false.
    bool usesTestModeFocusRingColor() const;
    // A view associated to the contained document.
    NSView* documentViewFor(const RenderObject&) const;

protected:
    RenderThemeMac();
    virtual ~RenderThemeMac();

#if ENABLE(VIDEO)
    // Media controls
    virtual String mediaControlsStyleSheet() override;
    virtual String mediaControlsScript() override;
#endif

#if ENABLE(SERVICE_CONTROLS)
    virtual String imageControlsStyleSheet() const override;
#endif

    virtual bool supportsSelectionForegroundColors() const override { return false; }

    virtual bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    virtual void adjustTextFieldStyle(StyleResolver&, RenderStyle&, Element&) const override;

    virtual bool paintTextArea(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    virtual void adjustTextAreaStyle(StyleResolver&, RenderStyle&, Element&) const override;

    virtual bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    virtual void adjustMenuListStyle(StyleResolver&, RenderStyle&, Element&) const override;

    virtual bool paintMenuListButtonDecorations(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    virtual void adjustMenuListButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;

    virtual void adjustProgressBarStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintProgressBar(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual void adjustSliderTrackStyle(StyleResolver&, RenderStyle&, Element&) const override;

    virtual bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual void adjustSliderThumbStyle(StyleResolver&, RenderStyle&, Element&) const override;

    virtual bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual void adjustSearchFieldStyle(StyleResolver&, RenderStyle&, Element&) const override;

    virtual void adjustSearchFieldCancelButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldCancelButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldDecorationPartStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldResultsDecorationPartStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldResultsDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldResultsButtonStyle(StyleResolver&, RenderStyle&, Element&) const override;
    virtual bool paintSearchFieldResultsButton(const RenderObject&, const PaintInfo&, const IntRect&) override;

#if ENABLE(VIDEO)
    virtual bool supportsClosedCaptioning() const override { return true; }
#endif

    virtual bool shouldShowPlaceholderWhenFocused() const override;

    virtual bool paintSnapshottedPluginOverlay(const RenderObject&, const PaintInfo&, const IntRect&) override;

private:
    virtual String fileListNameForWidth(const FileList*, const Font&, int width, bool multipleFilesAllowed) const override;

    FloatRect convertToPaintingRect(const RenderObject& inputRenderer, const RenderObject& partRenderer, const FloatRect& inputRect, const IntRect&) const;

    // Get the control size based off the font. Used by some of the controls (like buttons).
    NSControlSize controlSizeForFont(RenderStyle&) const;
    NSControlSize controlSizeForSystemFont(RenderStyle&) const;
    NSControlSize controlSizeForCell(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f) const;
    void setControlSize(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f);
    void setSizeFromFont(RenderStyle&, const IntSize* sizes) const;
    IntSize sizeForFont(RenderStyle&, const IntSize* sizes) const;
    IntSize sizeForSystemFont(RenderStyle&, const IntSize* sizes) const;
    void setFontFromControlSize(StyleResolver&, RenderStyle&, NSControlSize) const;

    void updateCheckedState(NSCell*, const RenderObject&);
    void updateEnabledState(NSCell*, const RenderObject&);
    void updateFocusedState(NSCell*, const RenderObject&);
    void updatePressedState(NSCell*, const RenderObject&);

    // Helpers for adjusting appearance and for painting

    void setPopupButtonCellState(const RenderObject&, const IntSize&);
    const IntSize* popupButtonSizes() const;
    const int* popupButtonMargins() const;
    const int* popupButtonPadding(NSControlSize) const;
    void paintMenuListButtonGradients(const RenderObject&, const PaintInfo&, const IntRect&);
    const IntSize* menuListSizes() const;

    const IntSize* searchFieldSizes() const;
    const IntSize* cancelButtonSizes() const;
    const IntSize* resultsButtonSizes() const;
    void setSearchCellState(const RenderObject&, const IntRect&);
    void setSearchFieldSize(RenderStyle&) const;

    NSPopUpButtonCell *popupButton() const;
    NSSearchFieldCell *search() const;
    NSMenu *searchMenuTemplate() const;
    NSSliderCell *sliderThumbHorizontal() const;
    NSSliderCell *sliderThumbVertical() const;
    NSTextFieldCell *textField() const;

#if ENABLE(METER_ELEMENT)
    NSLevelIndicatorStyle levelIndicatorStyleFor(ControlPart) const;
    NSLevelIndicatorCell *levelIndicatorFor(const RenderMeter&) const;
#endif

    int minimumProgressBarHeight(RenderStyle&) const;
    const IntSize* progressBarSizes() const;
    const int* progressBarMargins(NSControlSize) const;

#if ENABLE(SERVICE_CONTROLS)
    virtual bool paintImageControlsButton(const RenderObject&, const PaintInfo&, const IntRect&) override;
    virtual IntSize imageControlsButtonSize(const RenderObject&) const override;
    virtual IntSize imageControlsButtonPositionOffset() const override;

    NSServicesRolloverButtonCell *servicesRolloverButtonCell() const;
#endif

    mutable RetainPtr<NSPopUpButtonCell> m_popupButton;
    mutable RetainPtr<NSSearchFieldCell> m_search;
    mutable RetainPtr<NSMenu> m_searchMenuTemplate;
    mutable RetainPtr<NSSliderCell> m_sliderThumbHorizontal;
    mutable RetainPtr<NSSliderCell> m_sliderThumbVertical;
    mutable RetainPtr<NSLevelIndicatorCell> m_levelIndicator;
    mutable RetainPtr<NSTextFieldCell> m_textField;
#if ENABLE(SERVICE_CONTROLS)
    mutable RetainPtr<NSServicesRolloverButtonCell> m_servicesRolloverButton;
#endif

    bool m_isSliderThumbHorizontalPressed;
    bool m_isSliderThumbVerticalPressed;

    mutable HashMap<int, RGBA32> m_systemColorCache;

    RetainPtr<WebCoreRenderThemeNotificationObserver> m_notificationObserver;

    String m_mediaControlsScript;
    String m_mediaControlsStyleSheet;
};

} // namespace WebCore

#endif // RenderThemeMac_h

#endif // !PLATFORM(IOS)
