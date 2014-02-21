/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLFieldSetElement_h
#define HTMLFieldSetElement_h

#include "HTMLFormControlElement.h"

namespace WebCore {

class FormAssociatedElement;
class HTMLCollection;

class HTMLFieldSetElement final : public HTMLFormControlElement {
public:
    static PassRefPtr<HTMLFieldSetElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    HTMLLegendElement* legend() const;
    PassRefPtr<HTMLCollection> elements();

    const Vector<FormAssociatedElement*>& associatedElements() const;
    unsigned length() const;

protected:

private:
    HTMLFieldSetElement(const QualifiedName&, Document&, HTMLFormElement*);
    ~HTMLFieldSetElement();

    virtual bool isEnumeratable() const override { return true; }
    virtual bool supportsFocus() const override;
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual const AtomicString& formControlType() const override;
    virtual bool recalcWillValidate() const override { return false; }
    virtual void disabledAttributeChanged() override;
    virtual void disabledStateChanged() override;
    virtual void childrenChanged(const ChildChange&) override;
    virtual void didMoveToNewDocument(Document* oldDocument) override;

    void refreshElementsIfNeeded() const;

    mutable Vector<FormAssociatedElement*> m_associatedElements;
    // When dom tree is modified, we have to refresh the m_associatedElements array.
    mutable uint64_t m_documentVersion;
};

NODE_TYPE_CASTS(HTMLFieldSetElement)

} // namespace

#endif
