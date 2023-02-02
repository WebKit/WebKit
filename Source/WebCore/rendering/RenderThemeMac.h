/*
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
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

OBJC_CLASS WebCoreRenderThemeNotificationObserver;

namespace WebCore {

class RenderProgress;
class RenderStyle;

struct AttachmentLayout;

class RenderThemeMac final : public RenderThemeCocoa {
public:
    friend NeverDestroyed<RenderThemeMac>;

    // A method asking if the control changes its tint when the window has focus or not.
    bool controlSupportsTints(const RenderObject&) const final;

    // A general method asking if any control tinting is supported at all.
    bool supportsControlTints() const final { return true; }

    void adjustRepaintRect(const RenderObject&, FloatRect&) final;

    bool isControlStyled(const RenderStyle&, const RenderStyle& userAgentStyle) const final;

    bool supportsSelectionForegroundColors(OptionSet<StyleColorOptions>) const final;

    Color platformActiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformActiveSelectionForegroundColor(OptionSet<StyleColorOptions>) const final;
    Color transformSelectionBackgroundColor(const Color&, OptionSet<StyleColorOptions>) const final;
    Color platformInactiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformInactiveSelectionForegroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformFocusRingColor(OptionSet<StyleColorOptions>) const final;
    Color platformTextSearchHighlightColor(OptionSet<StyleColorOptions>) const final;
    Color platformAnnotationHighlightColor(OptionSet<StyleColorOptions>) const final;
    Color platformDefaultButtonTextColor(OptionSet<StyleColorOptions>) const final;

    ScrollbarControlSize scrollbarControlSizeForPart(StyleAppearance) final { return ScrollbarControlSize::Small; }

    int minimumMenuListSize(const RenderStyle&) const final;

    void adjustSliderThumbSize(RenderStyle&, const Element*) const final;

#if ENABLE(DATALIST_ELEMENT)
    IntSize sliderTickSize() const final;
    int sliderTickOffsetFromTrackCenter() const final;
#endif

    LengthBox popupInternalPaddingBox(const RenderStyle&, const Settings&) const final;
    PopupMenuStyle::PopupMenuSize popupMenuSize(const RenderStyle&, IntRect&) const final;

    bool popsMenuByArrowKeys() const final { return true; }

    FloatSize meterSizeForBounds(const RenderMeter&, const FloatRect&) const final;
    bool supportsMeter(StyleAppearance, const HTMLMeterElement&) const final;

    // Returns the repeat interval of the animation for the progress bar.
    Seconds animationRepeatIntervalForProgressBar(const RenderProgress&) const final;
    IntRect progressBarRectForBounds(const RenderProgress&, const IntRect&) const final;

    // Controls color values returned from platformFocusRingColor(). systemColor() will be used when false.
    bool usesTestModeFocusRingColor() const;
    // A view associated to the contained document.
    NSView* documentViewFor(const RenderObject&) const;

    WEBCORE_EXPORT static RetainPtr<NSImage> iconForAttachment(const String& fileName, const String& attachmentType, const String& title);

private:
    RenderThemeMac();

    bool canPaint(const PaintInfo&, const Settings&, StyleAppearance) const final;
    bool canCreateControlPartForRenderer(const RenderObject&) const final;
    bool canCreateControlPartForBorderOnly(const RenderObject&) const final;
    bool canCreateControlPartForDecorations(const RenderObject&) const final;

    bool useFormSemanticContext() const final;
    bool supportsLargeFormControls() const final;

    void adjustTextFieldStyle(RenderStyle&, const Element*) const final;

    void adjustTextAreaStyle(RenderStyle&, const Element*) const final;

    void adjustMenuListStyle(RenderStyle&, const Element*) const final;

    void adjustMenuListButtonStyle(RenderStyle&, const Element*) const final;

    void adjustProgressBarStyle(RenderStyle&, const Element*) const final;

    void adjustSliderTrackStyle(RenderStyle&, const Element*) const final;

    void adjustSliderThumbStyle(RenderStyle&, const Element*) const final;

    void adjustSearchFieldStyle(RenderStyle&, const Element*) const final;

    void adjustSearchFieldCancelButtonStyle(RenderStyle&, const Element*) const final;

    void adjustSearchFieldDecorationPartStyle(RenderStyle&, const Element*) const final;

    void adjustSearchFieldResultsDecorationPartStyle(RenderStyle&, const Element*) const final;

    void adjustSearchFieldResultsButtonStyle(RenderStyle&, const Element*) const final;

#if ENABLE(DATALIST_ELEMENT)
    void adjustListButtonStyle(RenderStyle&, const Element*) const final;
#endif
    
#if ENABLE(SERVICE_CONTROLS)
    void adjustImageControlsButtonStyle(RenderStyle&, const Element*) const final;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    LayoutSize attachmentIntrinsicSize(const RenderAttachment&) const final;
    bool paintAttachment(const RenderObject&, const PaintInfo&, const IntRect&) final;
#endif

private:
    String fileListNameForWidth(const FileList*, const FontCascade&, int width, bool multipleFilesAllowed) const final;

    Color systemColor(CSSValueID, OptionSet<StyleColorOptions>) const final;

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

    void setPopupButtonCellState(const RenderObject&, const IntSize&);
    const IntSize* popupButtonSizes() const;
    const int* popupButtonMargins() const;
    const int* popupButtonPadding(NSControlSize, bool isRTL) const;
    const IntSize* menuListSizes() const;

    const IntSize* searchFieldSizes() const;
    const IntSize* cancelButtonSizes() const;
    const IntSize* resultsButtonSizes() const;
    void setSearchFieldSize(RenderStyle&) const;

    NSPopUpButtonCell *popupButton() const;

#if ENABLE(SERVICE_CONTROLS)
    IntSize imageControlsButtonSize() const final;
    bool isImageControlsButton(const Element&) const final;
#endif

    mutable RetainPtr<NSPopUpButtonCell> m_popupButton;

    bool m_isSliderThumbHorizontalPressed { false };
    bool m_isSliderThumbVerticalPressed { false };

    RetainPtr<WebCoreRenderThemeNotificationObserver> m_notificationObserver;
};

} // namespace WebCore

#endif // PLATFORM(MAC)
