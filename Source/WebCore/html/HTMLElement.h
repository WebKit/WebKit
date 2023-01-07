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

#include "HTMLNames.h"
#include "InputMode.h"
#include "StyledElement.h"

#if ENABLE(AUTOCAPITALIZE)
#include "Autocapitalize.h"
#endif

namespace WebCore {

class ElementInternals;
class FormListedElement;
class FormAssociatedElement;
class HTMLFormElement;
class VisibleSelection;
struct SimpleRange;
struct TextRecognitionResult;

enum class PageIsEditable : bool;
enum class EnterKeyHint : uint8_t;

#if PLATFORM(IOS_FAMILY)
enum class SelectionRenderingBehavior : bool;
#endif

class HTMLElement : public StyledElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLElement);
public:
    static Ref<HTMLElement> create(const QualifiedName& tagName, Document&);

    WEBCORE_EXPORT String title() const final;

    WEBCORE_EXPORT ExceptionOr<void> setInnerText(String&&);
    WEBCORE_EXPORT ExceptionOr<void> setOuterText(String&&);

    virtual bool hasCustomFocusLogic() const;
    bool supportsFocus() const override;

    WEBCORE_EXPORT String contentEditable() const;
    WEBCORE_EXPORT ExceptionOr<void> setContentEditable(const String&);

    static Editability editabilityFromContentEditableAttr(const Node&, PageIsEditable);

    virtual bool draggable() const;
    WEBCORE_EXPORT void setDraggable(bool);
    virtual bool isDraggableIgnoringAttributes() const { return false; }

    WEBCORE_EXPORT bool spellcheck() const;
    WEBCORE_EXPORT void setSpellcheck(bool);

    WEBCORE_EXPORT bool translate() const;
    WEBCORE_EXPORT void setTranslate(bool);

    WEBCORE_EXPORT void click();

    bool accessKeyAction(bool sendMouseEvents) override;

    String accessKeyLabel() const;

    bool rendererIsEverNeeded() final;

    WEBCORE_EXPORT const AtomString& dir() const;
    WEBCORE_EXPORT void setDir(const AtomString&);

    bool hasDirectionAuto() const;

    std::optional<TextDirection> directionalityIfDirIsAuto() const;

    virtual bool isTextControlInnerTextElement() const { return false; }
    virtual bool isSearchFieldResultsButtonElement() const { return false; }

    bool willRespondToMouseMoveEvents() const override;
    bool willRespondToMouseClickEventsWithEditability(Editability) const override;

    virtual bool isLabelable() const { return false; }
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
    void setEnterKeyHint(const AtomString& value);

    WEBCORE_EXPORT static bool shouldExtendSelectionToTargetNode(const Node& targetNode, const VisibleSelection& selectionBeforeUpdate);

    WEBCORE_EXPORT ExceptionOr<Ref<ElementInternals>> attachInternals();

#if PLATFORM(IOS_FAMILY)
    static SelectionRenderingBehavior selectionRenderingBehavior(const Node*);
#endif

protected:
    HTMLElement(const QualifiedName& tagName, Document&, ConstructionType);

    enum class AllowZeroValue : bool { No, Yes };
    void addHTMLLengthToStyle(MutableStyleProperties&, CSSPropertyID, StringView value, AllowZeroValue = AllowZeroValue::Yes);
    void addHTMLMultiLengthToStyle(MutableStyleProperties&, CSSPropertyID, StringView value);
    void addHTMLPixelsToStyle(MutableStyleProperties&, CSSPropertyID, StringView value);
    void addHTMLNumberToStyle(MutableStyleProperties&, CSSPropertyID, StringView value);

    static std::optional<SRGBA<uint8_t>> parseLegacyColorValue(StringView);
    void addHTMLColorToStyle(MutableStyleProperties&, CSSPropertyID, const AtomString& color);

    void applyAspectRatioFromWidthAndHeightAttributesToStyle(StringView widthAttribute, StringView heightAttribute, MutableStyleProperties&);
    void applyAspectRatioWithoutDimensionalRulesFromWidthAndHeightAttributesToStyle(StringView widthAttribute, StringView heightAttribute, MutableStyleProperties&);
    void addParsedWidthAndHeightToAspectRatioList(double width, double height, MutableStyleProperties&);
    
    void applyAlignmentAttributeToStyle(const AtomString&, MutableStyleProperties&);
    void applyBorderAttributeToStyle(const AtomString&, MutableStyleProperties&);

    bool matchesReadWritePseudoClass() const override;
    void parseAttribute(const QualifiedName&, const AtomString&) override;
    Node::InsertedIntoAncestorResult insertedIntoAncestor(InsertionType , ContainerNode& parentOfInsertedTree) override;
    void removedFromAncestor(RemovalType, ContainerNode& oldParentOfRemovedTree) override;
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const override;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) override;
    unsigned parseBorderWidthAttribute(const AtomString&) const;

    void childrenChanged(const ChildChange&) override;
    void updateEffectiveDirectionalityOfDirAuto();

    using EventHandlerNameMap = HashMap<AtomStringImpl*, AtomString>;
    static const AtomString& eventNameForEventHandlerAttribute(const QualifiedName& attributeName, const EventHandlerNameMap&);

private:
    String nodeName() const final;

    void mapLanguageAttributeToLocale(const AtomString&, MutableStyleProperties&);

    void dirAttributeChanged(const AtomString&);
    void updateEffectiveDirectionality(TextDirection);
    void adjustDirectionalityIfNeededAfterChildAttributeChanged(Element* child);
    void adjustDirectionalityIfNeededAfterChildrenChanged(Element* beforeChange, ChildChange::Type);

    struct TextDirectionWithStrongDirectionalityNode {
        TextDirection direction;
        RefPtr<Node> strongDirectionalityNode;
    };
    TextDirectionWithStrongDirectionalityNode computeDirectionalityFromText() const;

    enum class AllowPercentage : bool { No, Yes };
    enum class UseCSSPXAsUnitType : bool { No, Yes };
    enum class IsMultiLength : bool { No, Yes };
    void addHTMLLengthToStyle(MutableStyleProperties&, CSSPropertyID, StringView value, AllowPercentage, UseCSSPXAsUnitType, IsMultiLength, AllowZeroValue = AllowZeroValue::Yes);
};

inline HTMLElement::HTMLElement(const QualifiedName& tagName, Document& document, ConstructionType type = CreateHTMLElement)
    : StyledElement(tagName, document, type)
{
    ASSERT(tagName.localName().impl());
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
