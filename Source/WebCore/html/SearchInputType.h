/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BaseTextInputType.h"
#include "PopupMenuClient.h"
#include "SearchPopupMenu.h"
#include "Timer.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class SearchFieldResultsButtonElement;

class SearchInputType final : public BaseTextInputType, public PopupMenuClient {
    WTF_MAKE_TZONE_ALLOCATED(SearchInputType);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SearchInputType);
public:
    static Ref<SearchInputType> create(HTMLInputElement& element)
    {
        return adoptRef(*new SearchInputType(element));
    }

    // PopupMenuClient ref-counting (disambiguating from InputType)
    void ref() const final { BaseTextInputType::ref(); }
    void deref() const final { BaseTextInputType::deref(); }

    // PopupMenuClient methods
    void valueChanged(unsigned listIndex, bool fireEvents = true) override;
    void selectionChanged(unsigned, bool) override { }
    void selectionCleared() override { }
    String itemText(unsigned listIndex) const override;
    String itemLabel(unsigned listIndex) const override;
    String itemIcon(unsigned listIndex) const override;
    String itemToolTip(unsigned) const override { return String(); }
    String itemAccessibilityText(unsigned) const override { return String(); }
    bool itemIsEnabled(unsigned listIndex) const override;
    PopupMenuStyle itemStyle(unsigned listIndex) const override;
    PopupMenuStyle menuStyle() const override;
    int clientInsetLeft() const override;
    int clientInsetRight() const override;
    LayoutUnit clientPaddingLeft() const override;
    LayoutUnit clientPaddingRight() const override;
    int listSize() const override;
    int popupSelectedIndex() const override;
    void popupDidHide() override;
    bool itemIsSeparator(unsigned listIndex) const override;
    bool itemIsLabel(unsigned listIndex) const override;
    bool itemIsSelected(unsigned listIndex) const override;
    bool shouldPopOver() const override { return false; }
    void setTextFromItem(unsigned listIndex) override;
    FontSelector* fontSelector() const override;
    HostWindow* hostWindow() const override;
    Ref<Scrollbar> createScrollbar(ScrollableArea&, ScrollbarOrientation, ScrollbarWidth) override;

    Vector<RecentSearch>& recentSearches() { return m_recentSearches; }

private:
    explicit SearchInputType(HTMLInputElement&);

    void addSearchResult() final;
    void attributeChanged(const QualifiedName&) final;
    RenderPtr<RenderElement> createInputRenderer(RenderStyle&&) final;
    const AtomString& formControlType() const final;
    bool needsContainer() const final;
    void createShadowSubtree() final;
    void removeShadowSubtree() final;
    HTMLElement* resultsButtonElement() const final;
    HTMLElement* cancelButtonElement() const final;
    ShouldCallBaseEventHandler handleKeydownEvent(KeyboardEvent&) final;
    void didSetValueByUserEdit() final;
    bool sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const final;
    float decorationWidth(float inputWidth) const final;
    void setValue(const String&, bool valueChanged, TextFieldEventBehavior, TextControlSetValueSelection) final;

    RefPtr<SearchFieldResultsButtonElement> m_resultsButton;
    RefPtr<HTMLElement> m_cancelButton;

    Vector<RecentSearch> m_recentSearches;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INPUT_TYPE(SearchInputType, Type::Search)
