/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Computer, Inc.
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

#if ENABLE(IMAGE_CONTROLS)
OBJC_CLASS NSServicesRolloverButtonCell;
#endif

OBJC_CLASS WebCoreRenderThemeNotificationObserver;

namespace WebCore {

class RenderProgress;
class RenderStyle;

class RenderThemeMac : public RenderTheme {
public:
    static PassRefPtr<RenderTheme> create();

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject*) const;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const { return true; }

    virtual void adjustRepaintRect(const RenderObject*, IntRect&) override;

    virtual bool isControlStyled(const RenderStyle*, const BorderData&, const FillLayer&, const Color& backgroundColor) const;

    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color platformActiveListBoxSelectionBackgroundColor() const;
    virtual Color platformActiveListBoxSelectionForegroundColor() const;
    virtual Color platformInactiveListBoxSelectionBackgroundColor() const;
    virtual Color platformInactiveListBoxSelectionForegroundColor() const;
    virtual Color platformFocusRingColor() const;

    virtual ScrollbarControlSize scrollbarControlSizeForPart(ControlPart) { return SmallScrollbar; }

    virtual void platformColorsDidChange();

    // System fonts.
    virtual void systemFont(CSSValueID, FontDescription&) const;

    virtual int minimumMenuListSize(RenderStyle*) const;

    virtual void adjustSliderThumbSize(RenderStyle*, Element*) const;

#if ENABLE(DATALIST_ELEMENT)
    virtual IntSize sliderTickSize() const override;
    virtual int sliderTickOffsetFromTrackCenter() const override;
#endif

    virtual int popupInternalPaddingLeft(RenderStyle*) const;
    virtual int popupInternalPaddingRight(RenderStyle*) const;
    virtual int popupInternalPaddingTop(RenderStyle*) const;
    virtual int popupInternalPaddingBottom(RenderStyle*) const;

    virtual bool paintCapsLockIndicator(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual bool popsMenuByArrowKeys() const override { return true; }

#if ENABLE(METER_ELEMENT)
    virtual IntSize meterSizeForBounds(const RenderMeter*, const IntRect&) const override;
    virtual bool paintMeter(RenderObject*, const PaintInfo&, const IntRect&);
    virtual bool supportsMeter(ControlPart) const;
#endif

#if ENABLE(PROGRESS_ELEMENT)
    // Returns the repeat interval of the animation for the progress bar.
    virtual double animationRepeatIntervalForProgressBar(RenderProgress*) const;
    // Returns the duration of the animation for the progress bar.
    virtual double animationDurationForProgressBar(RenderProgress*) const;
    virtual IntRect progressBarRectForBounds(const RenderObject*, const IntRect&) const override;
#endif

    virtual Color systemColor(CSSValueID) const;
    // Controls color values returned from platformFocusRingColor(). systemColor() will be used when false.
    virtual bool usesTestModeFocusRingColor() const;
    // A view associated to the contained document. Subclasses may not have such a view and return a fake.
    NSView* documentViewFor(RenderObject*) const;


protected:
    RenderThemeMac();
    virtual ~RenderThemeMac();

#if ENABLE(VIDEO)
    // Media controls
    virtual String mediaControlsStyleSheet() override;
    virtual String mediaControlsScript() override;
#endif

#if ENABLE(IMAGE_CONTROLS)
    virtual String imageControlsStyleSheet() const override;
#endif

    virtual bool supportsSelectionForegroundColors() const { return false; }

    virtual bool paintTextField(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustTextFieldStyle(StyleResolver*, RenderStyle*, Element*) const;

    virtual bool paintTextArea(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustTextAreaStyle(StyleResolver*, RenderStyle*, Element*) const;

    virtual bool paintMenuList(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustMenuListStyle(StyleResolver*, RenderStyle*, Element*) const;

    virtual bool paintMenuListButtonDecorations(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustMenuListButtonStyle(StyleResolver*, RenderStyle*, Element*) const;

#if ENABLE(PROGRESS_ELEMENT)
    virtual void adjustProgressBarStyle(StyleResolver*, RenderStyle*, Element*) const;
    virtual bool paintProgressBar(RenderObject*, const PaintInfo&, const IntRect&);
#endif

    virtual bool paintSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustSliderTrackStyle(StyleResolver*, RenderStyle*, Element*) const;

    virtual bool paintSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustSliderThumbStyle(StyleResolver*, RenderStyle*, Element*) const;

    virtual bool paintSearchField(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustSearchFieldStyle(StyleResolver*, RenderStyle*, Element*) const;

    virtual void adjustSearchFieldCancelButtonStyle(StyleResolver*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldCancelButton(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldDecorationPartStyle(StyleResolver*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldDecorationPart(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsDecorationPartStyle(StyleResolver*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsDecorationPart(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsButtonStyle(StyleResolver*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsButton(RenderObject*, const PaintInfo&, const IntRect&);

#if ENABLE(VIDEO)
    virtual bool supportsClosedCaptioning() const { return true; }
#endif

    virtual bool shouldShowPlaceholderWhenFocused() const;

    virtual bool paintSnapshottedPluginOverlay(RenderObject*, const PaintInfo&, const IntRect&);

private:
    virtual String fileListNameForWidth(const FileList*, const Font&, int width, bool multipleFilesAllowed) const override;

    IntRect inflateRect(const IntRect&, const IntSize&, const int* margins, float zoomLevel = 1.0f) const;

    FloatRect convertToPaintingRect(const RenderObject* inputRenderer, const RenderObject* partRenderer, const FloatRect& inputRect, const IntRect&) const;

    // Get the control size based off the font. Used by some of the controls (like buttons).
    NSControlSize controlSizeForFont(RenderStyle*) const;
    NSControlSize controlSizeForSystemFont(RenderStyle*) const;
    void setControlSize(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f);
    void setSizeFromFont(RenderStyle*, const IntSize* sizes) const;
    IntSize sizeForFont(RenderStyle*, const IntSize* sizes) const;
    IntSize sizeForSystemFont(RenderStyle*, const IntSize* sizes) const;
    void setFontFromControlSize(StyleResolver*, RenderStyle*, NSControlSize) const;

    void updateCheckedState(NSCell*, const RenderObject*);
    void updateEnabledState(NSCell*, const RenderObject*);
    void updateFocusedState(NSCell*, const RenderObject*);
    void updatePressedState(NSCell*, const RenderObject*);
    // An optional hook for subclasses to update the control tint of NSCell.
    virtual void updateActiveState(NSCell*, const RenderObject*) { }

    // Helpers for adjusting appearance and for painting

    void setPopupButtonCellState(const RenderObject*, const IntRect&);
    const IntSize* popupButtonSizes() const;
    const int* popupButtonMargins() const;
    const int* popupButtonPadding(NSControlSize) const;
    void paintMenuListButtonGradients(RenderObject*, const PaintInfo&, const IntRect&);
    const IntSize* menuListSizes() const;

    const IntSize* searchFieldSizes() const;
    const IntSize* cancelButtonSizes() const;
    const IntSize* resultsButtonSizes() const;
    void setSearchCellState(RenderObject*, const IntRect&);
    void setSearchFieldSize(RenderStyle*) const;

    NSPopUpButtonCell *popupButton() const;
    NSSearchFieldCell *search() const;
    NSMenu *searchMenuTemplate() const;
    NSSliderCell *sliderThumbHorizontal() const;
    NSSliderCell *sliderThumbVertical() const;
    NSTextFieldCell *textField() const;

#if ENABLE(METER_ELEMENT)
    NSLevelIndicatorStyle levelIndicatorStyleFor(ControlPart) const;
    NSLevelIndicatorCell *levelIndicatorFor(const RenderMeter*) const;
#endif

#if ENABLE(PROGRESS_ELEMENT)
    int minimumProgressBarHeight(RenderStyle*) const;
    const IntSize* progressBarSizes() const;
    const int* progressBarMargins(NSControlSize) const;
#endif

#if ENABLE(IMAGE_CONTROLS)
    virtual bool paintImageControlsButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual IntSize imageControlsButtonSize(const RenderObject*) const override;
    NSServicesRolloverButtonCell *servicesRolloverButtonCell() const;
#endif

    mutable RetainPtr<NSPopUpButtonCell> m_popupButton;
    mutable RetainPtr<NSSearchFieldCell> m_search;
    mutable RetainPtr<NSMenu> m_searchMenuTemplate;
    mutable RetainPtr<NSSliderCell> m_sliderThumbHorizontal;
    mutable RetainPtr<NSSliderCell> m_sliderThumbVertical;
    mutable RetainPtr<NSLevelIndicatorCell> m_levelIndicator;
    mutable RetainPtr<NSTextFieldCell> m_textField;
#if ENABLE(IMAGE_CONTROLS)
    mutable RetainPtr<NSServicesRolloverButtonCell> m_servicesRolloverButton;
#endif

    bool m_isSliderThumbHorizontalPressed;
    bool m_isSliderThumbVerticalPressed;

    mutable HashMap<int, RGBA32> m_systemColorCache;

    RetainPtr<WebCoreRenderThemeNotificationObserver> m_notificationObserver;
};

} // namespace WebCore

#endif // RenderThemeMac_h

#endif // !PLATFORM(IOS)
