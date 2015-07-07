/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLElement_h
#define HTMLElement_h

#include "StyledElement.h"

namespace WebCore {

class DocumentFragment;
class FormNamedItem;
class HTMLCollection;
class HTMLFormElement;

enum TranslateAttributeMode {
    TranslateAttributeYes,
    TranslateAttributeNo,
    TranslateAttributeInherit
};

class HTMLElement : public StyledElement {
public:
    static PassRefPtr<HTMLElement> create(const QualifiedName& tagName, Document&);

    PassRefPtr<HTMLCollection> children();

    virtual String title() const override final;

    virtual short tabIndex() const override;

    String innerHTML() const;
    String outerHTML() const;
    void setInnerHTML(const String&, ExceptionCode&);
    void setOuterHTML(const String&, ExceptionCode&);
    void setInnerText(const String&, ExceptionCode&);
    void setOuterText(const String&, ExceptionCode&);

    Element* insertAdjacentElement(const String& where, Element* newChild, ExceptionCode&);
    void insertAdjacentHTML(const String& where, const String& html, ExceptionCode&);
    void insertAdjacentText(const String& where, const String& text, ExceptionCode&);

    virtual bool hasCustomFocusLogic() const;
    virtual bool supportsFocus() const override;

    String contentEditable() const;
    void setContentEditable(const String&, ExceptionCode&);

    virtual bool draggable() const;
    void setDraggable(bool);

    bool spellcheck() const;
    void setSpellcheck(bool);

    bool translate() const;
    void setTranslate(bool);

    void click();

    virtual void accessKeyAction(bool sendMouseEvents) override;

    bool ieForbidsInsertHTML() const;

    virtual bool rendererIsNeeded(const RenderStyle&) override;
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;

    HTMLFormElement* form() const { return virtualForm(); }

    bool hasDirectionAuto() const;
    TextDirection directionalityIfhasDirAutoAttribute(bool& isAuto) const;

    bool isHTMLUnknownElement() const { return getFlag(IsHTMLUnknownElementFlag); }
    virtual bool isTextControlInnerTextElement() const { return false; }

    virtual bool willRespondToMouseMoveEvents() override;
    virtual bool willRespondToMouseWheelEvents() override;
    virtual bool willRespondToMouseClickEvents() override;

    virtual bool isLabelable() const { return false; }
    virtual FormNamedItem* asFormNamedItem() { return 0; }

    static void populateEventNameForAttributeLocalNameMap(HashMap<AtomicStringImpl*, AtomicString>&);

    bool hasTagName(const HTMLQualifiedName& name) const { return hasLocalName(name.localName()); }

protected:
    HTMLElement(const QualifiedName& tagName, Document&, ConstructionType);

    void addHTMLLengthToStyle(MutableStyleProperties&, CSSPropertyID, const String& value);
    void addHTMLColorToStyle(MutableStyleProperties&, CSSPropertyID, const String& color);

    void applyAlignmentAttributeToStyle(const AtomicString&, MutableStyleProperties&);
    void applyBorderAttributeToStyle(const AtomicString&, MutableStyleProperties&);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;
    unsigned parseBorderWidthAttribute(const AtomicString&) const;

    virtual void childrenChanged(const ChildChange&) override;
    void calculateAndAdjustDirectionality();

    virtual bool isURLAttribute(const Attribute&) const override;

private:
    virtual String nodeName() const override final;

    void mapLanguageAttributeToLocale(const AtomicString&, MutableStyleProperties&);

    virtual HTMLFormElement* virtualForm() const;

    Node* insertAdjacent(const String& where, Node* newChild, ExceptionCode&);
    PassRefPtr<DocumentFragment> textToFragment(const String&, ExceptionCode&);

    void dirAttributeChanged(const AtomicString&);
    void adjustDirectionalityIfNeededAfterChildAttributeChanged(Element* child);
    void adjustDirectionalityIfNeededAfterChildrenChanged(Element* beforeChange, ChildChangeType);
    TextDirection directionality(Node** strongDirectionalityTextNode= 0) const;

    TranslateAttributeMode translateAttributeMode() const;
};

inline HTMLElement::HTMLElement(const QualifiedName& tagName, Document& document, ConstructionType type = CreateHTMLElement)
    : StyledElement(tagName, document, type)
{
    ASSERT(tagName.localName().impl());
}

template <typename Type> bool isElementOfType(const HTMLElement&);

void isHTMLElement(const HTMLElement&); // Catch unnecessary runtime check of type known at compile time.
inline bool isHTMLElement(const Node& node) { return node.isHTMLElement(); }
template <> inline bool isElementOfType<const HTMLElement>(const HTMLElement&) { return true; }
template <> inline bool isElementOfType<const HTMLElement>(const Element& element) { return element.isHTMLElement(); }

NODE_TYPE_CASTS(HTMLElement)

inline bool Node::hasTagName(const HTMLQualifiedName& name) const
{
    return isHTMLElement() && toHTMLElement(*this).hasTagName(name);
}

} // namespace WebCore

#include "HTMLElementTypeHelpers.h"

#endif // HTMLElement_h
