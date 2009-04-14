/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
 * Copyright (C) 2008, 2009 Google, Inc.
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

#ifndef RenderThemeChromiumMac_h
#define RenderThemeChromiumMac_h

#import "RenderTheme.h"
#import <AppKit/AppKit.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>

#ifdef __OBJC__
@class WebCoreRenderThemeNotificationObserver;
#else
class WebCoreRenderThemeNotificationObserver;
#endif

namespace WebCore {

    class RenderStyle;

    class RenderThemeChromiumMac : public RenderTheme {
    public:
        RenderThemeChromiumMac();
        virtual ~RenderThemeChromiumMac();

        // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
        // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
        // controls that need to do this.
        virtual int baselinePosition(const RenderObject*) const;

        // A method asking if the control changes its tint when the window has focus or not.
        virtual bool controlSupportsTints(const RenderObject*) const;

        // A general method asking if any control tinting is supported at all.
        virtual bool supportsControlTints() const { return true; }

        virtual void adjustRepaintRect(const RenderObject*, IntRect&);

        virtual bool isControlStyled(const RenderStyle*, const BorderData&,
                                     const FillLayer&, const Color& backgroundColor) const;

        virtual Color platformActiveSelectionBackgroundColor() const;
        virtual Color platformInactiveSelectionBackgroundColor() const;
        virtual Color activeListBoxSelectionBackgroundColor() const;
        
        virtual void platformColorsDidChange();

        // System fonts.
        virtual void systemFont(int cssValueId, FontDescription&) const;

        virtual int minimumMenuListSize(RenderStyle*) const;

        virtual void adjustSliderThumbSize(RenderObject*) const;
        
        virtual int popupInternalPaddingLeft(RenderStyle*) const;
        virtual int popupInternalPaddingRight(RenderStyle*) const;
        virtual int popupInternalPaddingTop(RenderStyle*) const;
        virtual int popupInternalPaddingBottom(RenderStyle*) const;
        
        virtual ScrollbarControlSize scrollbarControlSizeForPart(ControlPart) { return SmallScrollbar; }
    
        virtual bool paintCapsLockIndicator(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual Color systemColor(int cssValueId) const;

    protected:
        virtual bool supportsSelectionForegroundColors() const { return false; }

        // Methods for each appearance value.
        virtual bool paintCheckbox(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void setCheckboxSize(RenderStyle*) const;

        virtual bool paintRadio(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void setRadioSize(RenderStyle*) const;

        virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, WebCore::Element*) const;
        virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void setButtonSize(RenderStyle*) const;

        virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

        virtual bool paintTextArea(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void adjustTextAreaStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

        virtual bool paintMenuList(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

        virtual bool paintMenuListButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

        virtual bool paintSliderTrack(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void adjustSliderTrackStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

        virtual bool paintSliderThumb(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void adjustSliderThumbStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

        virtual bool paintSearchField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

        virtual void adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintSearchFieldCancelButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual void adjustSearchFieldDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintSearchFieldDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual void adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintSearchFieldResultsDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual void adjustSearchFieldResultsButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintSearchFieldResultsButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual bool paintMediaFullscreenButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual bool paintMediaPlayButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual bool paintMediaMuteButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual bool paintMediaSeekBackButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual bool paintMediaSeekForwardButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual bool paintMediaSliderTrack(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual bool paintMediaSliderThumb(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    private:
        IntRect inflateRect(const IntRect&, const IntSize&, const int* margins, float zoomLevel = 1.0f) const;

        // Get the control size based off the font.  Used by some of the controls (like buttons).
        NSControlSize controlSizeForFont(RenderStyle*) const;
        NSControlSize controlSizeForSystemFont(RenderStyle*) const;
        void setControlSize(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f);
        void setSizeFromFont(RenderStyle*, const IntSize* sizes) const;
        IntSize sizeForFont(RenderStyle*, const IntSize* sizes) const;
        IntSize sizeForSystemFont(RenderStyle*, const IntSize* sizes) const;
        void setFontFromControlSize(CSSStyleSelector*, RenderStyle*, NSControlSize) const;

        void updateCheckedState(NSCell*, const RenderObject*);
        void updateEnabledState(NSCell*, const RenderObject*);
        void updateFocusedState(NSCell*, const RenderObject*);
        void updatePressedState(NSCell*, const RenderObject*);

        // Helpers for adjusting appearance and for painting
        const IntSize* checkboxSizes() const;
        const int* checkboxMargins() const;
        void setCheckboxCellState(const RenderObject*, const IntRect&);

        const IntSize* radioSizes() const;
        const int* radioMargins() const;
        void setRadioCellState(const RenderObject*, const IntRect&);

        void setButtonPaddingFromControlSize(RenderStyle*, NSControlSize) const;
        const IntSize* buttonSizes() const;
        const int* buttonMargins() const;
        void setButtonCellState(const RenderObject*, const IntRect&);

        void setPopupButtonCellState(const RenderObject*, const IntRect&);
        const IntSize* popupButtonSizes() const;
        const int* popupButtonMargins() const;
        const int* popupButtonPadding(NSControlSize) const;
        void paintMenuListButtonGradients(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        const IntSize* menuListSizes() const;

        const IntSize* searchFieldSizes() const;
        const IntSize* cancelButtonSizes() const;
        const IntSize* resultsButtonSizes() const;
        void setSearchCellState(RenderObject*, const IntRect&);
        void setSearchFieldSize(RenderStyle*) const;
        
        NSButtonCell* checkbox() const;
        NSButtonCell* radio() const;
        NSButtonCell* button() const;
        NSPopUpButtonCell* popupButton() const;
        NSSearchFieldCell* search() const;
        NSMenu* searchMenuTemplate() const;
        NSSliderCell* sliderThumbHorizontal() const;
        NSSliderCell* sliderThumbVertical() const;

    private:
        mutable RetainPtr<NSButtonCell> m_checkbox;
        mutable RetainPtr<NSButtonCell> m_radio;
        mutable RetainPtr<NSButtonCell> m_button;
        mutable RetainPtr<NSPopUpButtonCell> m_popupButton;
        mutable RetainPtr<NSSearchFieldCell> m_search;
        mutable RetainPtr<NSMenu> m_searchMenuTemplate;
        mutable RetainPtr<NSSliderCell> m_sliderThumbHorizontal;
        mutable RetainPtr<NSSliderCell> m_sliderThumbVertical;

        bool m_isSliderThumbHorizontalPressed;
        bool m_isSliderThumbVerticalPressed;

        mutable HashMap<int, RGBA32> m_systemColorCache;

        RetainPtr<WebCoreRenderThemeNotificationObserver> m_notificationObserver;
    };

} // namespace WebCore

#endif
