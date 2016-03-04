/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef HTMLSummaryElement_h
#define HTMLSummaryElement_h

#include "HTMLElement.h"

namespace WebCore {

class HTMLDetailsElement;

class HTMLSummaryElement final : public HTMLElement {
public:
    static Ref<HTMLSummaryElement> create(const QualifiedName&, Document&);

    bool isActiveSummary() const;
    bool willRespondToMouseClickEvents() override;

private:
    HTMLSummaryElement(const QualifiedName&, Document&);

    RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;
    void defaultEventHandler(Event*) override;

    void didAddUserAgentShadowRoot(ShadowRoot*) override;

    // FIXME: Shadow DOM spec says we should be able to create shadow root on this element
    bool canHaveUserAgentShadowRoot() const override final { return true; }

    HTMLDetailsElement* detailsElement() const;

    bool supportsFocus() const override;
};

}

#endif // HTMLSummaryElement_h
