/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2026 Apple Inc. All rights reserved.
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

#include <WebCore/HTMLNames.h>
#include <WebCore/StyledElement.h>

namespace WebCore {

class ElementInternals;
class FormListedElement;
class FormAssociatedElement;
class HTMLButtonElement;
class HTMLFormElement;
class VisibleSelection;

struct SRGBADescriptor;
template<typename, typename> struct BoundedGammaEncoded;
template<typename T> using SRGBA = BoundedGammaEncoded<T, SRGBADescriptor>;

struct SimpleRange;
struct TextRecognitionResult;

enum class AutocapitalizeType : uint8_t;
enum class EnterKeyHint : uint8_t;
enum class InputMode : uint8_t;
enum class PageIsEditable : bool;
enum class PopoverVisibilityState : bool;
enum class SelectionRenderingBehavior : bool;
enum class ToggleState : bool;

enum class FireEvents : bool { No, Yes };
enum class FocusPreviousElement : bool { No, Yes };

class HTMLElement : public StyledElement {
    WTF_MAKE_TZONE_ALLOCATED(HTMLElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(HTMLElement);
public:
    NODELETE static Ref<HTMLElement> create(const QualifiedName& tagName, Document&);

    WEBCORE_EXPORT RefPtr<StringImpl> title() const final;

    WEBCORE_EXPORT ExceptionOr<void> setInnerText(RefPtr<StringImpl>&&);
    WEBCORE_EXPORT ExceptionOr<void> setOuterText(RefPtr<StringImpl>&&);

    enum class IncludeAltText : bool { No, Yes };
    static RefPtr<StringImpl> htmlAwareTextContent(const Element&, IncludeAltText);

    NODELETE virtual bool hasCustomFocusLogic() const;
    bool supportsFocus() const override;

    WEBCORE_EXPORT RefPtr<StringImpl> contentEditable() const;
    WEBCORE_EXPORT ExceptionOr<void> setContentEditable(StringImpl*);

    static Editability editabilityFromContentEditableAttr(const Node&, PageIsEditable);

    virtual bool draggable() const;
    WEBCORE_EXPORT void setDraggable(bool);
    virtual bool isDraggableIgnoringAttributes() const { return false; }

    WEBCORE_EXPORT bool spellcheck() const;
    WEBCORE_EXPORT void setSpellcheck(bool);

    WEBCORE_EXPORT const RefPtr<AtomStringImpl>& writingSuggestions() const;

    WEBCORE_EXPORT bool translate() const;
    WEBCORE_EXPORT void setTranslate(bool);

    WEBCORE_EXPORT void click();

    bool accessKeyAction(bool sendMouseEvents) override;

    RefPtr<StringImpl> accessKeyLabel() const;

    WEBCORE_EXPORT const RefPtr<AtomStringImpl>& dir() const;

    virtual bool isTextControlInnerTextElement() const { return false; }
    virtual bool isSearchFieldResultsButtonElement() const { return false; }
    virtual bool isDataListButtonElement() const { return false; }
    virtual bool isSelectFallbackButtonElement() const { return false; }
    virtual bool isSelectPopoverElement() const { return false; }

    bool willRespondToMouseMoveEvents() const override;
    bool willRespondToMouseClickEventsWithEditability(Editability) const override;

    // Represents "labelable element": https://html.spec.whatwg.org/multipage/forms.html#category-label
    virtual bool isLabelable() const { return false; }
    WEBCORE_EXPORT RefPtr<NodeList> labels();

    NODELETE virtual FormAssociatedElement* asFormAssociatedElement();

    virtual bool isInteractiveContent() const { return false; }

    bool hasTagName(const HTMLQualifiedName& name) const { return hasLocalName(name.localName()); }

    static const RefPtr<AtomStringImpl>& eventNameForEventHandlerAttribute(const QualifiedName& attributeName);

    // Only some element types can be disabled: https://html.spec.whatwg.org/multipage/scripting.html#concept-element-disabled
    NODELETE bool canBeActuallyDisabled() const;
    virtual bool isActuallyDisabled() const;

#if ENABLE(AUTOCAPITALIZE)
    WEBCORE_EXPORT virtual AutocapitalizeType autocapitalizeType() const;
    WEBCORE_EXPORT const RefPtr<AtomStringImpl>& autocapitalize() const;
#endif

#if ENABLE(AUTOCORRECT)
    bool autocorrect() const { return shouldAutocorrect(); }
    WEBCORE_EXPORT virtual bool shouldAutocorrect() const;
    WEBCORE_EXPORT void setAutocorrect(bool);
#endif

    WEBCORE_EXPORT InputMode canonicalInputMode() const;
    const RefPtr<AtomStringImpl>& inputMode() const;

    WEBCORE_EXPORT EnterKeyHint canonicalEnterKeyHint() const;
    RefPtr<StringImpl> enterKeyHint() const;

    bool isHiddenUntilFound() const;
    std::optional<Variant<bool, double, RefPtr<StringImpl>>> hidden() const;
    void setHidden(const std::optional<Variant<bool, double, RefPtr<StringImpl>>>&);

    WEBCORE_EXPORT static bool shouldExtendSelectionToTargetNode(const Node& targetNode, const VisibleSelection& selectionBeforeUpdate);

    WEBCORE_EXPORT ExceptionOr<Ref<ElementInternals>> attachInternals();

    struct ShowPopoverOptions {
        RefPtr<HTMLElement> source;
    };

    struct TogglePopoverOptions : public ShowPopoverOptions {
        std::optional<bool> force;
    };

    void queuePopoverToggleEventTask(ToggleState oldState, ToggleState newState, Element* source);
    ExceptionOr<void> showPopover(const ShowPopoverOptions&);
    ExceptionOr<void> showPopoverInternal(HTMLElement* = nullptr);
    ExceptionOr<void> hidePopover();
    ExceptionOr<void> hidePopoverInternal(FocusPreviousElement, FireEvents, HTMLElement* = nullptr);
    ExceptionOr<bool> togglePopover(Variant<WebCore::HTMLElement::TogglePopoverOptions, bool>);

    NODELETE const RefPtr<AtomStringImpl>& popover() const;
    void setPopover(AtomStringImpl* value);
    void popoverAttributeChanged(AtomStringImpl* value);

    bool isValidCommandType(const CommandType) override;
    bool handleCommandInternal(HTMLButtonElement& invoker, const CommandType&) override;

    static SelectionRenderingBehavior selectionRenderingBehavior(const Node*);

protected:
    HTMLElement(const QualifiedName& tagName, Document&, OptionSet<TypeFlag> = { });

    enum class AllowZeroValue : bool { No, Yes };
    void addHTMLLengthToStyle(MutableStyleProperties&, CSSPropertyID, StringView value, AllowZeroValue = AllowZeroValue::Yes);
    void addHTMLMultiLengthToStyle(MutableStyleProperties&, CSSPropertyID, StringView value);
    void addHTMLPixelsToStyle(MutableStyleProperties&, CSSPropertyID, StringView value);
    void addHTMLPixelLengthToStyle(MutableStyleProperties&, CSSPropertyID, StringView value);
    void addHTMLNumberToStyle(MutableStyleProperties&, CSSPropertyID, StringView value);

    static std::optional<SRGBA<uint8_t>> parseLegacyColorValue(StringView);
    void addHTMLColorToStyle(MutableStyleProperties&, CSSPropertyID, AtomStringImpl* color);

    void applyAspectRatioFromWidthAndHeightAttributesToStyle(StringView widthAttribute, StringView heightAttribute, MutableStyleProperties&);
    void applyAspectRatioWithoutDimensionalRulesFromWidthAndHeightAttributesToStyle(StringView widthAttribute, StringView heightAttribute, MutableStyleProperties&);
    void addParsedWidthAndHeightToAspectRatioList(double width, double height, MutableStyleProperties&);
    
    void applyAlignmentAttributeToStyle(AtomStringImpl*, MutableStyleProperties&);
    void applyBorderAttributeToStyle(AtomStringImpl*, MutableStyleProperties&);

    bool matchesReadWritePseudoClass() const override;
    void attributeChanged(const QualifiedName&, AtomStringImpl* oldValue, AtomStringImpl* newValue, AttributeModificationReason) override;
    Node::NeedsPostConnectionSteps insertionSteps(InsertionType , ContainerNode& parentOfInsertedTree) override;
    void removingSteps(RemovalType, ContainerNode& oldParentOfRemovedTree) override;
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const override;
    void collectPresentationalHintsForAttribute(const QualifiedName&, AtomStringImpl*, MutableStyleProperties&) override;
    unsigned parseBorderWidthAttribute(AtomStringImpl*) const;

    virtual void effectiveSpellcheckAttributeChanged(bool);

    virtual void setPopoverVisibilityState(PopoverVisibilityState);
    virtual void popoverWasHidden() { }

    using EventHandlerNameMap = HashMap<RefPtr<AtomStringImpl>, RefPtr<AtomStringImpl>>;
    NODELETE static const RefPtr<AtomStringImpl>& eventNameForEventHandlerAttribute(const QualifiedName& attributeName, const EventHandlerNameMap&);

private:
    HTMLElement(ClangVTableWorkaroundTag, const QualifiedName&, Document&);

    void setInvoker(HTMLElement*);

    RefPtr<StringImpl> nodeName() const final;

    enum class AllowPercentage : bool { No, Yes };
    enum class UseCSSPXAsUnitType : bool { No, Yes };
    enum class IsMultiLength : bool { No, Yes };
    void addHTMLLengthToStyle(MutableStyleProperties&, CSSPropertyID, StringView value, AllowPercentage, UseCSSPXAsUnitType, IsMultiLength, AllowZeroValue = AllowZeroValue::Yes);
};

inline bool Node::hasTagName(const HTMLQualifiedName& name) const
{
    auto* htmlElement = dynamicDowncast<HTMLElement>(*this);
    return htmlElement && htmlElement->hasTagName(name);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLElement)
    static bool isType(const WebCore::Node& node) { return node.isHTMLElement(); }
    static bool isType(const WebCore::EventTarget& target)
    {
        auto* node = dynamicDowncast<WebCore::Node>(target);
        return node && isType(*node);
    }
SPECIALIZE_TYPE_TRAITS_END()

#include <WebCore/HTMLElementTypeHelpers.h>
