/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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

#include "InputMode.h"
#include "StyledElement.h"

#if ENABLE(AUTOCAPITALIZE)
#include "Autocapitalize.h"
#endif

namespace WebCore {

class FormAssociatedElement;
class FormNamedItem;
class HTMLFormElement;

enum class EnterKeyHint : uint8_t;

class HTMLElement : public StyledElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLElement);
public:
    static Ref<HTMLElement> create(const QualifiedName& tagName, Document&);

    WEBCORE_EXPORT String title() const final;

    WEBCORE_EXPORT ExceptionOr<void> setInnerText(const String&);
    WEBCORE_EXPORT ExceptionOr<void> setOuterText(const String&);

    virtual bool hasCustomFocusLogic() const;
    bool supportsFocus() const override;

    WEBCORE_EXPORT String contentEditable() const;
    WEBCORE_EXPORT ExceptionOr<void> setContentEditable(const String&);

    static Editability editabilityFromContentEditableAttr(const Node&);

    virtual bool draggable() const;
    WEBCORE_EXPORT void setDraggable(bool);

    WEBCORE_EXPORT bool spellcheck() const;
    WEBCORE_EXPORT void setSpellcheck(bool);

    WEBCORE_EXPORT bool translate() const;
    WEBCORE_EXPORT void setTranslate(bool);

    WEBCORE_EXPORT void click();

    bool accessKeyAction(bool sendMouseEvents) override;

    String accessKeyLabel() const;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool rendererIsEverNeeded() final;

    WEBCORE_EXPORT virtual HTMLFormElement* form() const;

    WEBCORE_EXPORT const AtomString& dir() const;
    WEBCORE_EXPORT void setDir(const AtomString&);

    bool hasDirectionAuto() const;
    TextDirection directionalityIfhasDirAutoAttribute(bool& isAuto) const;

    virtual bool isHTMLUnknownElement() const { return false; }
    virtual bool isTextControlInnerTextElement() const { return false; }

    bool willRespondToMouseMoveEvents() override;
    bool willRespondToMouseWheelEvents() override;
    bool willRespondToMouseClickEvents() override;

    virtual bool isLabelable() const { return false; }
    virtual FormNamedItem* asFormNamedItem();
    virtual FormAssociatedElement* asFormAssociatedElement();

    virtual bool isInteractiveContent() const { return false; }

    bool hasTagName(const HTMLQualifiedName& name) const { return hasLocalName(name.localName()); }

    static const AtomString& eventNameForEventHandlerAttribute(const QualifiedName& attributeName);

    // Only some element types can be disabled: https://html.spec.whatwg.org/multipage/scripting.html#concept-element-disabled
    bool canBeActuallyDisabled() const;
    bool isActuallyDisabled() const;

#if ENABLE(AUTOCAPITALIZE)
    WEBCORE_EXPORT virtual AutocapitalizeType autocapitalizeType() const;
    WEBCORE_EXPORT const AtomString& autocapitalize() const;
    WEBCORE_EXPORT void setAutocapitalize(const AtomString& value);
#endif

#if ENABLE(AUTOCORRECT)
    bool autocorrect() const { return shouldAutocorrect(); }
    WEBCORE_EXPORT virtual bool shouldAutocorrect() const;
    WEBCORE_EXPORT void setAutocorrect(bool);
#endif

    WEBCORE_EXPORT InputMode canonicalInputMode() const;
    const AtomString& inputMode() const;
    void setInputMode(const AtomString& value);

    WEBCORE_EXPORT EnterKeyHint canonicalEnterKeyHint() const;
    String enterKeyHint() const;
    void setEnterKeyHint(const String& value);

protected:
    HTMLElement(const QualifiedName& tagName, Document&, ConstructionType);

    void addHTMLLengthToStyle(MutableStyleProperties&, CSSPropertyID, const String& value);
    void addHTMLColorToStyle(MutableStyleProperties&, CSSPropertyID, const String& color);

    void applyAlignmentAttributeToStyle(const AtomString&, MutableStyleProperties&);
    void applyBorderAttributeToStyle(const AtomString&, MutableStyleProperties&);

    bool matchesReadWritePseudoClass() const override;
    void parseAttribute(const QualifiedName&, const AtomString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) override;
    unsigned parseBorderWidthAttribute(const AtomString&) const;

    void childrenChanged(const ChildChange&) override;
    void calculateAndAdjustDirectionality();

    typedef HashMap<AtomStringImpl*, AtomString> EventHandlerNameMap;
    template<size_t tableSize> static void populateEventHandlerNameMap(EventHandlerNameMap&, const QualifiedName* const (&table)[tableSize]);
    static const AtomString& eventNameForEventHandlerAttribute(const QualifiedName& attributeName, const EventHandlerNameMap&);

private:
    String nodeName() const final;

    void mapLanguageAttributeToLocale(const AtomString&, MutableStyleProperties&);

    void dirAttributeChanged(const AtomString&);
    void adjustDirectionalityIfNeededAfterChildAttributeChanged(Element* child);
    void adjustDirectionalityIfNeededAfterChildrenChanged(Element* beforeChange, ChildChangeType);
    TextDirection directionality(Node** strongDirectionalityTextNode= 0) const;

    static void populateEventHandlerNameMap(EventHandlerNameMap&, const QualifiedName* const table[], size_t tableSize);
    static EventHandlerNameMap createEventHandlerNameMap();
};

inline HTMLElement::HTMLElement(const QualifiedName& tagName, Document& document, ConstructionType type = CreateHTMLElement)
    : StyledElement(tagName, document, type)
{
    ASSERT(tagName.localName().impl());
}

template<size_t tableSize> inline void HTMLElement::populateEventHandlerNameMap(EventHandlerNameMap& map, const QualifiedName* const (&table)[tableSize])
{
    populateEventHandlerNameMap(map, table, tableSize);
}

inline bool Node::hasTagName(const HTMLQualifiedName& name) const
{
    return is<HTMLElement>(*this) && downcast<HTMLElement>(*this).hasTagName(name);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLElement)
    static bool isType(const WebCore::Node& node) { return node.isHTMLElement(); }
    static bool isType(const WebCore::EventTarget& target) { return is<WebCore::Node>(target) && isType(downcast<WebCore::Node>(target)); }
SPECIALIZE_TYPE_TRAITS_END()

#include "HTMLElementTypeHelpers.h"
