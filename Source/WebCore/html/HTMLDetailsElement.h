/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef HTMLDetailsElement_h
#define HTMLDetailsElement_h

#include "HTMLElement.h"

namespace WebCore {

class HTMLDetailsElement FINAL : public HTMLElement {
public:
    static PassRefPtr<HTMLDetailsElement> create(const QualifiedName& tagName, Document& document);
    void toggleOpen();

    Element* findMainSummary() const;

private:
    HTMLDetailsElement(const QualifiedName&, Document&);

    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual bool childShouldCreateRenderer(const Node&) const override;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;

    virtual void didAddUserAgentShadowRoot(ShadowRoot*) override;

    bool m_isOpen;
};

NODE_TYPE_CASTS(HTMLDetailsElement)

} // namespace WebCore

#endif // HTMLDetailsElement_h
