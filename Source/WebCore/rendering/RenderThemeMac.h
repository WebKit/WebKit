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
struct AttachmentLayout;

class RenderThemeMac final : public RenderTheme {
public:
    static Ref<RenderTheme> create();

    // A method asking if the control changes its tint when the window has focus or not.
    bool controlSupportsTints(const RenderObject&) const override;

    // A general method asking if any control tinting is supported at all.
    bool supportsControlTints() const override { return true; }

    void adjustRepaintRect(const RenderObject&, FloatRect&) override;

    bool isControlStyled(const RenderStyle&, const BorderData&, const FillLayer&, const Color& backgroundColor) const override;

    Color platformActiveSelectionBackgroundColor() const override;
    Color platformInactiveSelectionBackgroundColor() const override;
    Color platformActiveListBoxSelectionBackgroundColor() const override;
    Color platformActiveListBoxSelectionForegroundColor() const override;
    Color platformInactiveListBoxSelectionBackgroundColor() const override;
    Color platformInactiveListBoxSelectionForegroundColor() const override;
    Color platformFocusRingColor() const override;

    ScrollbarControlSize scrollbarControlSizeForPart(ControlPart) override { return SmallScrollbar; }

    void platformColorsDidChange() override;

    int minimumMenuListSize(const RenderStyle&) const override;

    void adjustSliderThumbSize(RenderStyle&, const Element*) const override;

#if ENABLE(DATALIST_ELEMENT)
    IntSize sliderTickSize() const override;
    int sliderTickOffsetFromTrackCenter() const override;
#endif

    LengthBox popupInternalPaddingBox(const RenderStyle&) const override;
    PopupMenuStyle::PopupMenuSize popupMenuSize(const RenderStyle&, IntRect&) const override;

    bool popsMenuByArrowKeys() const override { return true; }

#if ENABLE(METER_ELEMENT)
    IntSize meterSizeForBounds(const RenderMeter&, const IntRect&) const override;
    bool paintMeter(const RenderObject&, const PaintInfo&, const IntRect&) override;
    bool supportsMeter(ControlPart) const override;
#endif

    // Returns the repeat interval of the animation for the progress bar.
    double animationRepeatIntervalForProgressBar(RenderProgress&) const override;
    // Returns the duration of the animation for the progress bar.
    double animationDurationForProgressBar(RenderProgress&) const override;
    IntRect progressBarRectForBounds(const RenderObject&, const IntRect&) const override;

    // Controls color values returned from platformFocusRingColor(). systemColor() will be used when false.
    bool usesTestModeFocusRingColor() const;
    // A view associated to the contained document.
    NSView* documentViewFor(const RenderObject&) const;

protected:
    RenderThemeMac();
    virtual ~RenderThemeMac();

    // System fonts.
    void updateCachedSystemFontDescription(CSSValueID, FontCascadeDescription&) const override;

#if ENABLE(VIDEO)
    // Media controls
    String mediaControlsStyleSheet() override;
    String mediaControlsScript() override;
#endif

#if ENABLE(SERVICE_CONTROLS)
    String imageControlsStyleSheet() const override;
#endif

    bool supportsSelectionForegroundColors() const override { return false; }

    bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    void adjustTextFieldStyle(StyleResolver&, RenderStyle&, const Element*) const override;

    bool paintTextArea(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    void adjustTextAreaStyle(StyleResolver&, RenderStyle&, const Element*) const override;

    bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    void adjustMenuListStyle(StyleResolver&, RenderStyle&, const Element*) const override;

    bool paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;
    void adjustMenuListButtonStyle(StyleResolver&, RenderStyle&, const Element*) const override;

    void adjustProgressBarStyle(StyleResolver&, RenderStyle&, const Element*) const override;
    bool paintProgressBar(const RenderObject&, const PaintInfo&, const IntRect&) override;

    bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) override;
    void adjustSliderTrackStyle(StyleResolver&, RenderStyle&, const Element*) const override;

    bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) override;
    void adjustSliderThumbStyle(StyleResolver&, RenderStyle&, const Element*) const override;

    bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) override;
    void adjustSearchFieldStyle(StyleResolver&, RenderStyle&, const Element*) const override;

    void adjustSearchFieldCancelButtonStyle(StyleResolver&, RenderStyle&, const Element*) const override;
    bool paintSearchFieldCancelButton(const RenderBox&, const PaintInfo&, const IntRect&) override;

    void adjustSearchFieldDecorationPartStyle(StyleResolver&, RenderStyle&, const Element*) const override;
    bool paintSearchFieldDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) override;

    void adjustSearchFieldResultsDecorationPartStyle(StyleResolver&, RenderStyle&, const Element*) const override;
    bool paintSearchFieldResultsDecorationPart(const RenderBox&, const PaintInfo&, const IntRect&) override;

    void adjustSearchFieldResultsButtonStyle(StyleResolver&, RenderStyle&, const Element*) const override;
    bool paintSearchFieldResultsButton(const RenderBox&, const PaintInfo&, const IntRect&) override;

#if ENABLE(VIDEO)
    bool supportsClosedCaptioning() const override { return true; }
#endif

    bool shouldHaveCapsLockIndicator(const HTMLInputElement&) const override;

    bool paintSnapshottedPluginOverlay(const RenderObject&, const PaintInfo&, const IntRect&) override;

#if ENABLE(ATTACHMENT_ELEMENT)
    LayoutSize attachmentIntrinsicSize(const RenderAttachment&) const override;
    int attachmentBaseline(const RenderAttachment&) const override;
    bool paintAttachment(const RenderObject&, const PaintInfo&, const IntRect&) override;
#endif

private:
    String fileListNameForWidth(const FileList*, const FontCascade&, int width, bool multipleFilesAllowed) const override;

    Color systemColor(CSSValueID) const override;

    // Get the control size based off the font. Used by some of the controls (like buttons).
    NSControlSize controlSizeForFont(const RenderStyle&) const;
    NSControlSize controlSizeForSystemFont(const RenderStyle&) const;
    NSControlSize controlSizeForCell(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f) const;
    void setControlSize(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f);
    void setSizeFromFont(RenderStyle&, const IntSize* sizes) const;
    IntSize sizeForFont(const RenderStyle&, const IntSize* sizes) const;
    IntSize sizeForSystemFont(const RenderStyle&, const IntSize* sizes) const;
    void setFontFromControlSize(StyleResolver&, RenderStyle&, NSControlSize) const;

    void updateCheckedState(NSCell*, const RenderObject&);
    void updateEnabledState(NSCell*, const RenderObject&);
    void updateFocusedState(NSCell*, const RenderObject&);
    void updatePressedState(NSCell*, const RenderObject&);

    // Helpers for adjusting appearance and for painting

    void paintCellAndSetFocusedElementNeedsRepaintIfNecessary(NSCell*, const RenderObject&, const PaintInfo&, const FloatRect&);
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

    int minimumProgressBarHeight(const RenderStyle&) const;
    const IntSize* progressBarSizes() const;
    const int* progressBarMargins(NSControlSize) const;

#if ENABLE(SERVICE_CONTROLS)
    bool paintImageControlsButton(const RenderObject&, const PaintInfo&, const IntRect&) override;
    IntSize imageControlsButtonSize(const RenderObject&) const override;
    IntSize imageControlsButtonPositionOffset() const override;

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

    mutable HashMap<int, Color> m_systemColorCache;

    RetainPtr<WebCoreRenderThemeNotificationObserver> m_notificationObserver;

    String m_mediaControlsScript;
    String m_mediaControlsStyleSheet;
};

} // namespace WebCore

#endif // RenderThemeMac_h

#endif // !PLATFORM(IOS)
