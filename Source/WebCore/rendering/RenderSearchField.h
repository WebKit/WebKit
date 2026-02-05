/*
 * Copyright (C) 2006, 2007, 2009, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "RenderTextControlSingleLine.h"
#include "SearchPopupMenu.h"

namespace WebCore {

class HTMLInputElement;

class RenderSearchField final : public RenderTextControlSingleLine {
    WTF_MAKE_TZONE_ALLOCATED(RenderSearchField);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSearchField);
public:
    RenderSearchField(HTMLInputElement&, RenderStyle&&);
    virtual ~RenderSearchField();

    void updateCancelButtonVisibility() const;

    bool popupIsVisible() const { return m_searchPopupIsVisible; }
    void showPopup();
    void hidePopup();
    void popupDidHide();
    WEBCORE_EXPORT std::span<const RecentSearch> recentSearches();

    void updatePopup(const AtomString& name, const Vector<WebCore::RecentSearch>& searchItems);
#if PLATFORM(WIN)
    int clientInsetRight() const;
    int clientInsetLeft() const;
    LayoutUnit clientPaddingRight() const;
    LayoutUnit clientPaddingLeft() const;
    FontSelector* fontSelector() const;
    HostWindow* hostWindow() const;
#endif

private:
    void willBeDestroyed() override;
    LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const override;
    void updateFromElement() override;
    Visibility visibilityForCancelButton() const;
    const AtomString& autosaveName() const;

    RefPtr<SearchPopupMenu> protectedSearchPopup() const { return m_searchPopup; };

    HTMLElement* resultsButtonElement() const;
    HTMLElement* cancelButtonElement() const;

    bool m_searchPopupIsVisible;
    RefPtr<SearchPopupMenu> m_searchPopup;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSearchField, isRenderSearchField())
