/*
 * Copyright (C) 2005-2021 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "RenderThemeCocoa.h"

#if ENABLE(SERVICE_CONTROLS)
OBJC_CLASS NSServicesRolloverButtonCell;
#endif

OBJC_CLASS WebCoreRenderThemeNotificationObserver;

namespace WebCore {

class RenderProgress;
class RenderStyle;

struct AttachmentLayout;

class RenderThemeMac final : public RenderThemeCocoa {
public:
    friend NeverDestroyed<RenderThemeMac>;

    CFStringRef contentSizeCategory() const final;

    // A method asking if the control changes its tint when the window has focus or not.
    bool controlSupportsTints(const RenderObject&) const final;

    // A general method asking if any control tinting is supported at all.
    bool supportsControlTints() const final { return true; }

    void adjustRepaintRect(const RenderObject&, FloatRect&) final;

    bool isControlStyled(const RenderStyle&, const RenderStyle& userAgentStyle) const final;

    bool supportsSelectionForegroundColors(OptionSet<StyleColor::Options>) const final;

    Color platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformActiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const final;
    Color transformSelectionBackgroundColor(const Color&, OptionSet<StyleColor::Options>) const final;
    Color platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformInactiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformFocusRingColor(OptionSet<StyleColor::Options>) const final;
    Color platformTextSearchHighlightColor(OptionSet<StyleColor::Options>) const final;
#if ENABLE(APP_HIGHLIGHTS)
    Color platformAppHighlightColor(OptionSet<StyleColor::Options>) const final;
#endif
    Color platformDefaultButtonTextColor(OptionSet<StyleColor::Options>) const final;

    ScrollbarControlSize scrollbarControlSizeForPart(ControlPart) final { return ScrollbarControlSize::Small; }

    int minimumMenuListSize(const RenderStyle&) const final;

    void adjustSliderThumbSize(RenderStyle&, const Element*) const final;

#if ENABLE(DATALIST_ELEMENT)
    IntSize sliderTickSize() const final;
    int sliderTickOffsetFromTrackCenter() const final;
#endif

    LengthBox popupInternalPaddingBox(const RenderStyle&, const Settings&) const final;
    PopupMenuStyle::PopupMenuSize popupMenuSize(const RenderStyle&, IntRect&) const final;

    bool popsMenuByArrowKeys() const final { return true; }

    IntSize meterSizeForBounds(const RenderMeter&, const IntRect&) const final;
    bool paintMeter(const RenderObject&, const PaintInfo&, const IntRect&) final;
    bool supportsMeter(ControlPart, const HTMLMeterElement&) const final;

    // Returns the repeat interval of the animation for the progress bar.
    Seconds animationRepeatIntervalForProgressBar(const RenderProgress&) const final;
    IntRect progressBarRectForBounds(const RenderObject&, const IntRect&) const final;

    // Controls color values returned from platformFocusRingColor(). systemColor() will be used when false.
    bool usesTestModeFocusRingColor() const;
    // A view associated to the contained document.
    NSView* documentViewFor(const RenderObject&) const;

private:
    RenderThemeMac();

    bool canPaint(const PaintInfo&, const Settings&) const final;

    bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) final;
    void adjustTextFieldStyle(RenderStyle&, const Element*) const final;

    bool paintTextArea(const RenderObject&, const PaintInfo&, const FloatRect&) final;
    void adjustTextAreaStyle(RenderStyle&, const Element*) const final;

    bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) final;
    void adjustMenuListStyle(RenderStyle&, const Element*) const final;

    void paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) final;
    void adjustMenuListButtonStyle(RenderStyle&, const Element*) const final;

    void adjustProgressBarStyle(RenderStyle&, const Element*) const final;
    bool paintProgressBar(const RenderObject&, const PaintInfo&, const IntRect&) final;

    bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) final;
    void adjustSliderTrackStyle(RenderStyle&, const Element*) const final;

    bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) final;
    void adjustSliderThumbStyle(RenderStyle&, const Element*) const final;

    bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) final;
    void adjustSearchFieldStyle(RenderStyle&, const Element*) const final;

    void adjustSearchFieldCancelButtonStyle(RenderStyle&, const Element*) const final;
    bool paintSearchFieldCancelButton(const RenderBox&, const PaintInfo&, const IntRect&) final;

    void adjustSearchFieldDecorationPartStyle(RenderStyle&, const Element*) const final;
    bool paintSearchFieldDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) final;

    void adjustSearchFieldResultsDecorationPartStyle(RenderStyle&, const Element*) const final;
    bool paintSearchFieldResultsDecorationPart(const RenderBox&, const PaintInfo&, const IntRect&) final;

    void adjustSearchFieldResultsButtonStyle(RenderStyle&, const Element*) const final;
    bool paintSearchFieldResultsButton(const RenderBox&, const PaintInfo&, const IntRect&) final;

#if ENABLE(DATALIST_ELEMENT)
    void paintListButtonForInput(const RenderObject&, GraphicsContext&, const FloatRect&);
    void adjustListButtonStyle(RenderStyle&, const Element*) const final;
#endif

#if ENABLE(VIDEO)
    bool supportsClosedCaptioning() const final { return true; }
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    LayoutSize attachmentIntrinsicSize(const RenderAttachment&) const final;
    int attachmentBaseline(const RenderAttachment&) const final;
    bool paintAttachment(const RenderObject&, const PaintInfo&, const IntRect&) final;
#endif

private:
    String fileListNameForWidth(const FileList*, const FontCascade&, int width, bool multipleFilesAllowed) const final;

    Color systemColor(CSSValueID, OptionSet<StyleColor::Options>) const final;

    // Get the control size based off the font. Used by some of the controls (like buttons).
    NSControlSize controlSizeForFont(const RenderStyle&) const;
    NSControlSize controlSizeForSystemFont(const RenderStyle&) const;
    NSControlSize controlSizeForCell(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f) const;
    void setControlSize(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f);
    void setSizeFromFont(RenderStyle&, const IntSize* sizes) const;
    IntSize sizeForFont(const RenderStyle&, const IntSize* sizes) const;
    IntSize sizeForSystemFont(const RenderStyle&, const IntSize* sizes) const;
    void setFontFromControlSize(RenderStyle&, NSControlSize) const;

    void updateCheckedState(NSCell*, const RenderObject&);
    void updateEnabledState(NSCell*, const RenderObject&);
    void updateFocusedState(NSCell *, const RenderObject*);
    void updatePressedState(NSCell*, const RenderObject&);

    // Helpers for adjusting appearance and for painting

    void paintCellAndSetFocusedElementNeedsRepaintIfNecessary(NSCell*, const RenderObject&, const PaintInfo&, const FloatRect&);
    void setPopupButtonCellState(const RenderObject&, const IntSize&);
    const IntSize* popupButtonSizes() const;
    const int* popupButtonMargins() const;
    const int* popupButtonPadding(NSControlSize, bool isRTL) const;
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
#if ENABLE(DATALIST_ELEMENT)
    NSCell *listButton() const;
#endif

    NSLevelIndicatorStyle levelIndicatorStyleFor(ControlPart) const;
    NSLevelIndicatorCell *levelIndicatorFor(const RenderMeter&) const;

    int minimumProgressBarHeight(const RenderStyle&) const;
    const IntSize* progressBarSizes() const;
    const int* progressBarMargins(NSControlSize) const;

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
#if ENABLE(DATALIST_ELEMENT)
    mutable RetainPtr<NSCell> m_listButton;
#endif

    bool m_isSliderThumbHorizontalPressed { false };
    bool m_isSliderThumbVerticalPressed { false };

    RetainPtr<WebCoreRenderThemeNotificationObserver> m_notificationObserver;
};

} // namespace WebCore

#endif // PLATFORM(MAC)
