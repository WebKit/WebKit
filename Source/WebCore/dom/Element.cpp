/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004-2024 Apple Inc. All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
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
 */

#include "config.h"
#include "Element.h"

#include "AXObjectCache.h"
#include "Attr.h"
#include "AttributeChangeInvalidation.h"
#include "CSSParser.h"
#include "CheckVisibilityOptions.h"
#include "ChildChangeInvalidation.h"
#include "ChildListMutationScope.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClassChangeInvalidation.h"
#include "ComposedTreeAncestorIterator.h"
#include "ComposedTreeIterator.h"
#include "ComputedStylePropertyMapReadOnly.h"
#include "ContainerNodeAlgorithms.h"
#include "ContentVisibilityDocumentState.h"
#include "CustomElementReactionQueue.h"
#include "CustomElementRegistry.h"
#include "CustomStateSet.h"
#include "DOMRect.h"
#include "DOMRectList.h"
#include "DOMTokenList.h"
#include "DocumentInlines.h"
#include "DocumentSharedObjectPool.h"
#include "Editing.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementAnimationRareData.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "EventDispatcher.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FocusEvent.h"
#include "FormAssociatedCustomElement.h"
#include "FrameSelection.h"
#include "FullscreenManager.h"
#include "FullscreenOptions.h"
#include "GetAnimationsOptions.h"
#include "GetHTMLOptions.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDialogElement.h"
#include "HTMLDocument.h"
#include "HTMLHtmlElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNameCollection.h"
#include "HTMLObjectElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLScriptElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTemplateElement.h"
#include "HTMLTextAreaElement.h"
#include "IdChangeInvalidation.h"
#include "IdTargetObserverRegistry.h"
#include "InputType.h"
#include "InspectorInstrumentation.h"
#include "JSDOMPromiseDeferred.h"
#include "JSLazyEventListener.h"
#include "KeyboardEvent.h"
#include "KeyframeAnimationOptions.h"
#include "KeyframeEffect.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MutationObserverInterestGroup.h"
#include "MutationRecord.h"
#include "NodeName.h"
#include "NodeRenderStyle.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "PointerCaptureController.h"
#include "PointerEvent.h"
#include "PointerLockController.h"
#include "PointerLockOptions.h"
#include "PopoverData.h"
#include "PseudoClassChangeInvalidation.h"
#include "Quirks.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderListBox.h"
#include "RenderSVGModelObject.h"
#include "RenderStyleSetters.h"
#include "RenderTheme.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ResolvedStyle.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGScriptElement.h"
#include "ScriptDisallowedScope.h"
#include "ScrollIntoViewOptions.h"
#include "ScrollLatchingController.h"
#include "SecurityPolicyViolationEvent.h"
#include "SelectorQuery.h"
#include "Settings.h"
#include "ShadowRootInit.h"
#include "SimulatedClick.h"
#include "SlotAssignment.h"
#include "StyleInvalidator.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleTreeResolver.h"
#include "TextIterator.h"
#include "TouchAction.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisibilityAdjustment.h"
#include "VoidCallback.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include "WheelEvent.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "markup.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Element);

struct SameSizeAsElement : public ContainerNode {
    QualifiedName tagName;
    void* elementData;
};

static_assert(sizeof(Element) == sizeof(SameSizeAsElement), "Element should stay small");

using namespace HTMLNames;
using namespace XMLNames;

static HashMap<WeakRef<Element, WeakPtrImplWithEventTargetData>, Vector<RefPtr<Attr>>>& attrNodeListMap()
{
    static NeverDestroyed<HashMap<WeakRef<Element, WeakPtrImplWithEventTargetData>, Vector<RefPtr<Attr>>>> map;
    return map;
}

static Vector<RefPtr<Attr>>* attrNodeListForElement(Element& element)
{
    if (!element.hasSyntheticAttrChildNodes())
        return nullptr;
    ASSERT(attrNodeListMap().contains(element));
    return &attrNodeListMap().find(element)->value;
}

static Vector<RefPtr<Attr>>& ensureAttrNodeListForElement(Element& element)
{
    if (element.hasSyntheticAttrChildNodes()) {
        ASSERT(attrNodeListMap().contains(element));
        return attrNodeListMap().find(element)->value;
    }
    ASSERT(!attrNodeListMap().contains(element));
    element.setHasSyntheticAttrChildNodes(true);
    return attrNodeListMap().add(element, Vector<RefPtr<Attr>>()).iterator->value;
}

static void removeAttrNodeListForElement(Element& element)
{
    ASSERT(element.hasSyntheticAttrChildNodes());
    ASSERT(attrNodeListMap().contains(element));
    attrNodeListMap().remove(element);
    element.setHasSyntheticAttrChildNodes(false);
}

static Attr* findAttrNodeInList(Vector<RefPtr<Attr>>& attrNodeList, const QualifiedName& name)
{
    for (auto& node : attrNodeList) {
        if (node->qualifiedName().matches(name))
            return node.get();
    }
    return nullptr;
}

static bool shouldAutofocus(const Element& element)
{
    if (!element.hasAttributeWithoutSynchronization(HTMLNames::autofocusAttr))
        return false;

    Ref document = element.document();
    if (!element.isInDocumentTree() || !document->hasBrowsingContext())
        return false;

    if (document->isSandboxed(SandboxAutomaticFeatures)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked autofocusing on a form control because the form's frame is sandboxed and the 'allow-scripts' permission is not set."_s);
        return false;
    }
    if (!document->frame()->isMainFrame() && !document->topOrigin().isSameOriginDomain(document->securityOrigin())) {
        document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked autofocusing on a form control in a cross-origin subframe."_s);
        return false;
    }

    if (document->topDocument().isAutofocusProcessed())
        return false;

    return true;
}

static HashMap<WeakRef<Element, WeakPtrImplWithEventTargetData>, ElementIdentifier>& elementIdentifiersMap()
{
    static MainThreadNeverDestroyed<HashMap<WeakRef<Element, WeakPtrImplWithEventTargetData>, ElementIdentifier>> map;
    return map;
}

Ref<Element> Element::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new Element(tagName, document, { }));
}

Element::Element(const QualifiedName& tagName, Document& document, OptionSet<TypeFlag> typeFlags)
    : ContainerNode(document, ELEMENT_NODE, typeFlags | TypeFlag::IsElement)
    , m_tagName(tagName)
{
}

Element::~Element()
{
    ASSERT(!beforePseudoElement());
    ASSERT(!afterPseudoElement());

    if (UNLIKELY(hasElementStateFlag(ElementStateFlag::HasElementIdentifier)))
        elementIdentifiersMap().remove(*this);
    else
        ASSERT(!elementIdentifiersMap().contains(*this));

    ASSERT(!is<HTMLImageElement>(*this) || !intersectionObserverDataIfExists());
    disconnectFromIntersectionObservers();

    disconnectFromResizeObservers();

    removeShadowRoot();

    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();
}

inline ElementRareData& Element::ensureElementRareData()
{
    return static_cast<ElementRareData&>(ensureRareData());
}

inline void Node::setTabIndexState(TabIndexState state)
{
    auto bitfields = rareDataBitfields();
    bitfields.tabIndexState = enumToUnderlyingType(state);
    setRareDataBitfields(bitfields);
}

void Element::setTabIndexExplicitly(std::optional<int> tabIndex)
{
    if (!tabIndex) {
        setTabIndexState(TabIndexState::NotSet);
        return;
    }
    setTabIndexState([this, value = tabIndex.value()]() {
        switch (value) {
        case 0:
            return TabIndexState::Zero;
        case -1:
            return TabIndexState::NegativeOne;
        default:
            ensureElementRareData().setUnusualTabIndex(value);
            return TabIndexState::InRareData;
        }
    }());
}

std::optional<int> Element::tabIndexSetExplicitly() const
{
    switch (tabIndexState()) {
    case TabIndexState::NotSet:
        return std::nullopt;
    case TabIndexState::Zero:
        return 0;
    case TabIndexState::NegativeOne:
        return -1;
    case TabIndexState::InRareData:
        ASSERT(hasRareData());
        return elementRareData()->unusualTabIndex();
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

int Element::defaultTabIndex() const
{
    return -1;
}

bool Element::isNonceable() const
{
    // https://www.w3.org/TR/CSP3/#is-element-nonceable
    if (elementRareData()->nonce().isNull())
        return false;

    if (hasDuplicateAttribute())
        return false;

    if (hasAttributes()
        && (is<HTMLScriptElement>(*this) || is<SVGScriptElement>(*this))) {
        static constexpr auto scriptString = "<script"_s;
        static constexpr auto styleString = "<style"_s;

        for (const auto& attribute : attributesIterator()) {
            auto name = attribute.localNameLowercase();
            auto value = attribute.value().convertToASCIILowercase();
            if (name.contains(scriptString)
                || name.contains(styleString)
                || value.contains(scriptString)
                || value.contains(styleString))
                return false;
        }
    }

    return true;
}

const AtomString& Element::nonce() const
{
    if (hasRareData() && isNonceable())
        return elementRareData()->nonce();

    return emptyAtom();
}

void Element::setNonce(const AtomString& newValue)
{
    if (newValue == emptyAtom() && !hasRareData())
        return;

    ensureElementRareData().setNonce(newValue);
}

void Element::hideNonceSlow()
{
    // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#nonce-attributes
    ASSERT(isConnected());
    ASSERT(hasAttributeWithoutSynchronization(nonceAttr));

    if (!protectedDocument()->checkedContentSecurityPolicy()->isHeaderDelivered())
        return;

    // Retain previous IDL nonce.
    AtomString currentNonce = nonce();
    setAttribute(nonceAttr, emptyAtom());
    setNonce(currentNonce);
}

bool Element::supportsFocus() const
{
    return !!tabIndexSetExplicitly();
}

int Element::tabIndexForBindings() const
{
    return valueOrCompute(tabIndexSetExplicitly(), [&] { return defaultTabIndex(); });
}

void Element::setTabIndexForBindings(int value)
{
    setIntegralAttribute(tabindexAttr, value);
}

bool Element::isKeyboardFocusable(KeyboardEvent*) const
{
    if (!(isFocusable() && !shouldBeIgnoredInSequentialFocusNavigation() && tabIndexSetExplicitly().value_or(0) >= 0))
        return false;
    if (RefPtr root = shadowRoot()) {
        if (root->delegatesFocus())
            return false;
    }
    return true;
}

bool Element::isMouseFocusable() const
{
    return isFocusable();
}

bool Element::shouldUseInputMethod()
{
    return computeEditability(UserSelectAllTreatment::NotEditable, ShouldUpdateStyle::Update) != Editability::ReadOnly;
}

static bool isForceEvent(const PlatformMouseEvent& platformEvent)
{
    return platformEvent.type() == PlatformEvent::Type::MouseForceChanged || platformEvent.type() == PlatformEvent::Type::MouseForceDown || platformEvent.type() == PlatformEvent::Type::MouseForceUp;
}

static bool isCompatibilityMouseEvent(const MouseEvent& mouseEvent)
{
    // https://www.w3.org/TR/pointerevents/#compatibility-mapping-with-mouse-events
    const auto& type = mouseEvent.type();
    auto& eventNames = WebCore::eventNames();
    return type != eventNames.clickEvent && type != eventNames.mouseoverEvent && type != eventNames.mouseoutEvent && type != eventNames.mouseenterEvent && type != eventNames.mouseleaveEvent;
}

enum class ShouldIgnoreMouseEvent : bool { No, Yes };
static ShouldIgnoreMouseEvent dispatchPointerEventIfNeeded(Element& element, const MouseEvent& mouseEvent, const PlatformMouseEvent& platformEvent, bool& didNotSwallowEvent)
{
    if (RefPtr page = element.document().page()) {
        auto& pointerCaptureController = page->pointerCaptureController();
#if ENABLE(TOUCH_EVENTS)
        if (platformEvent.pointerId() != mousePointerID && mouseEvent.type() != eventNames().clickEvent && pointerCaptureController.preventsCompatibilityMouseEventsForIdentifier(platformEvent.pointerId()))
            return ShouldIgnoreMouseEvent::Yes;
#else
        UNUSED_PARAM(platformEvent);
#endif
        if (platformEvent.syntheticClickType() != SyntheticClickType::NoTap)
            return ShouldIgnoreMouseEvent::No;

        if (RefPtr pointerEvent = pointerCaptureController.pointerEventForMouseEvent(mouseEvent, platformEvent.pointerId(), platformEvent.pointerType())) {
            pointerCaptureController.dispatchEvent(*pointerEvent, &element);
            if (isCompatibilityMouseEvent(mouseEvent) && pointerCaptureController.preventsCompatibilityMouseEventsForIdentifier(pointerEvent->pointerId()))
                return ShouldIgnoreMouseEvent::Yes;
            if (pointerEvent->defaultPrevented() || pointerEvent->defaultHandled()) {
                didNotSwallowEvent = false;
                if (pointerEvent->type() == eventNames().pointerdownEvent)
                    return ShouldIgnoreMouseEvent::Yes;
            }
        }
    }

    return ShouldIgnoreMouseEvent::No;
}

Element::DispatchMouseEventResult Element::dispatchMouseEvent(const PlatformMouseEvent& platformEvent, const AtomString& eventType, int detail, Element* relatedTarget, IsSyntheticClick isSyntheticClick)
{
    auto eventIsDefaultPrevented = Element::EventIsDefaultPrevented::No;
    if (isDisabledFormControl() && !document().settings().sendMouseEventsToDisabledFormControlsEnabled())
        return { Element::EventIsDispatched::No, eventIsDefaultPrevented };

    if (isForceEvent(platformEvent) && !document().hasListenerTypeForEventType(platformEvent.type()))
        return { Element::EventIsDispatched::No, eventIsDefaultPrevented };

    Ref mouseEvent = MouseEvent::create(eventType, document().windowProxy(), platformEvent, detail, relatedTarget);

    if (mouseEvent->type().isEmpty())
        return { Element::EventIsDispatched::Yes, eventIsDefaultPrevented }; // Shouldn't happen.

    Ref protectedThis { *this };
    bool didNotSwallowEvent = true;

    if (dispatchPointerEventIfNeeded(*this, mouseEvent.get(), platformEvent, didNotSwallowEvent) == ShouldIgnoreMouseEvent::Yes)
        return { Element::EventIsDispatched::No, eventIsDefaultPrevented };

    auto isParentProcessAFullWebBrowser = false;
#if PLATFORM(IOS_FAMILY)
    if (RefPtr frame = document().frame())
        isParentProcessAFullWebBrowser = frame->loader().client().isParentProcessAFullWebBrowser();
#elif PLATFORM(MAC)
    isParentProcessAFullWebBrowser = MacApplication::isSafari();
#endif
    if (Quirks::StorageAccessResult::ShouldCancelEvent == protectedDocument()->quirks().triggerOptionalStorageAccessQuirk(*this, platformEvent, eventType, detail, relatedTarget, isParentProcessAFullWebBrowser, isSyntheticClick))
        return { Element::EventIsDispatched::No, eventIsDefaultPrevented };

    ASSERT(!mouseEvent->target() || mouseEvent->target() != relatedTarget);
    dispatchEvent(mouseEvent);
    if (mouseEvent->defaultPrevented())
        eventIsDefaultPrevented = Element::EventIsDefaultPrevented::Yes;
    if (mouseEvent->defaultPrevented() || mouseEvent->defaultHandled())
        didNotSwallowEvent = false;

    if (mouseEvent->type() == eventNames().clickEvent && mouseEvent->detail() == 2) {
        // Special case: If it's a double click event, we also send the dblclick event. This is not part
        // of the DOM specs, but is used for compatibility with the ondblclick="" attribute. This is treated
        // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
        // FIXME: Is it okay that mouseEvent may have been mutated by scripts via initMouseEvent in dispatchEvent above?
        Ref doubleClickEvent = MouseEvent::create(eventNames().dblclickEvent,
            mouseEvent->bubbles() ? Event::CanBubble::Yes : Event::CanBubble::No,
            mouseEvent->cancelable() ? Event::IsCancelable::Yes : Event::IsCancelable::No,
            Event::IsComposed::Yes,
            mouseEvent->view(), mouseEvent->detail(),
            mouseEvent->screenX(), mouseEvent->screenY(), mouseEvent->clientX(), mouseEvent->clientY(),
            mouseEvent->modifierKeys(), mouseEvent->button(), mouseEvent->buttons(), mouseEvent->syntheticClickType(), relatedTarget);

        if (mouseEvent->defaultHandled())
            doubleClickEvent->setDefaultHandled();

        dispatchEvent(doubleClickEvent);
        if (doubleClickEvent->defaultHandled() || doubleClickEvent->defaultPrevented())
            return { Element::EventIsDispatched::No, eventIsDefaultPrevented };
    }
    return { didNotSwallowEvent ? Element::EventIsDispatched::Yes : Element::EventIsDispatched::No, eventIsDefaultPrevented };
}

bool Element::dispatchWheelEvent(const PlatformWheelEvent& platformEvent, OptionSet<EventHandling>& processing, Event::IsCancelable isCancelable)
{
    Ref event = WheelEvent::create(platformEvent, document().windowProxy(), isCancelable);

    // Events with no deltas are important because they convey platform information about scroll gestures
    // and momentum beginning or ending. However, those events should not be sent to the DOM since some
    // websites will break. They need to be dispatched because dispatching them will call into the default
    // event handler, and our platform code will correctly handle the phase changes. Calling stopPropagation()
    // will prevent the event from being sent to the DOM, but will still call the default event handler.
    // FIXME: Move this logic into WheelEvent::create.
    if (platformEvent.delta().isZero())
        event->stopPropagation();
    else
        processing.add(EventHandling::DispatchedToDOM);

    dispatchEvent(event);
    
    LOG_WITH_STREAM(Scrolling, stream << "Element " << *this << " dispatchWheelEvent: (cancelable " << event->cancelable() << ") defaultPrevented " << event->defaultPrevented() << " defaultHandled " << event->defaultHandled());
    
    if (event->defaultPrevented())
        processing.add(EventHandling::DefaultPrevented);

    if (event->defaultHandled())
        processing.add(EventHandling::DefaultHandled);

    return !event->defaultPrevented() && !event->defaultHandled();
}

bool Element::dispatchKeyEvent(const PlatformKeyboardEvent& platformEvent)
{
    Ref event = KeyboardEvent::create(platformEvent, document().windowProxy());

    if (RefPtr frame = document().frame()) {
        if (frame->eventHandler().accessibilityPreventsEventPropagation(event))
            event->stopPropagation();
    }

    dispatchEvent(event);
    return !event->defaultPrevented() && !event->defaultHandled();
}

bool Element::dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions eventOptions, SimulatedClickVisualOptions visualOptions)
{
    return simulateClick(*this, underlyingEvent, eventOptions, visualOptions, SimulatedClickSource::UserAgent);
}

Ref<Node> Element::cloneNodeInternal(Document& targetDocument, CloningOperation type)
{
    switch (type) {
    case CloningOperation::OnlySelf:
    case CloningOperation::SelfWithTemplateContent: {
        Ref clone = cloneElementWithoutChildren(targetDocument);
        ScriptDisallowedScope::EventAllowedScope eventAllowedScope { clone };
        cloneShadowTreeIfPossible(clone);
        return clone;
    }
    case CloningOperation::Everything:
        break;
    }
    return cloneElementWithChildren(targetDocument);
}

void Element::cloneShadowTreeIfPossible(Element& newHost)
{
    RefPtr oldShadowRoot = this->shadowRoot();
    if (!oldShadowRoot || !oldShadowRoot->isClonable())
        return;

    Ref clonedShadowRoot = [&] {
        Ref clone = oldShadowRoot->cloneNodeInternal(newHost.document(), Node::CloningOperation::SelfWithTemplateContent);
        return downcast<ShadowRoot>(WTFMove(clone));
    }();
    newHost.addShadowRoot(clonedShadowRoot.copyRef());
    oldShadowRoot->cloneChildNodes(clonedShadowRoot);
}

Ref<Element> Element::cloneElementWithChildren(Document& targetDocument)
{
    Ref clone = cloneElementWithoutChildren(targetDocument);
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { clone };
    cloneShadowTreeIfPossible(clone);
    cloneChildNodes(clone);
    return clone;
}

Ref<Element> Element::cloneElementWithoutChildren(Document& targetDocument)
{
    Ref clone = cloneElementWithoutAttributesAndChildren(targetDocument);

    // This will catch HTML elements in the wrong namespace that are not correctly copied.
    // This is a sanity check as HTML overloads some of the DOM methods.
    ASSERT(isHTMLElement() == clone->isHTMLElement());

    clone->cloneDataFromElement(*this);
    return clone;
}

Ref<Element> Element::cloneElementWithoutAttributesAndChildren(Document& targetDocument)
{
    return targetDocument.createElement(tagQName(), false);
}

Ref<Attr> Element::detachAttribute(unsigned index)
{
    ASSERT(elementData());

    const Attribute& attribute = elementData()->attributeAt(index);

    RefPtr attrNode = attrIfExists(attribute.name());
    if (attrNode)
        detachAttrNodeFromElementWithValue(attrNode.get(), attribute.value());
    else
        attrNode = Attr::create(protectedDocument(), attribute.name(), attribute.value());

    removeAttributeInternal(index, InSynchronizationOfLazyAttribute::No);
    return attrNode.releaseNonNull();
}

bool Element::removeAttribute(const QualifiedName& name)
{
    if (!elementData())
        return false;

    unsigned index = elementData()->findAttributeIndexByName(name);
    if (index == ElementData::attributeNotFound)
        return false;

    removeAttributeInternal(index, InSynchronizationOfLazyAttribute::No);
    return true;
}

void Element::setBooleanAttribute(const QualifiedName& name, bool value)
{
    if (value)
        setAttribute(name, emptyAtom());
    else
        removeAttribute(name);
}

NamedNodeMap& Element::attributes() const
{
    ElementRareData& rareData = const_cast<Element*>(this)->ensureElementRareData();
    if (NamedNodeMap* attributeMap = rareData.attributeMap())
        return *attributeMap;

    rareData.setAttributeMap(makeUniqueWithoutRefCountedCheck<NamedNodeMap>(const_cast<Element&>(*this)));
    return *rareData.attributeMap();
}

bool Element::hasAttribute(const QualifiedName& name) const
{
    return hasAttributeNS(name.namespaceURI(), name.localName());
}

void Element::synchronizeAllAttributes() const
{
    if (!elementData())
        return;
    if (elementData()->styleAttributeIsDirty()) {
        ASSERT(isStyledElement());
        static_cast<const StyledElement*>(this)->synchronizeStyleAttributeInternal();
    }
    
    if (auto* svgElement = dynamicDowncast<SVGElement>(*this))
        const_cast<SVGElement&>(*svgElement).synchronizeAllAttributes();
}

ALWAYS_INLINE void Element::synchronizeAttribute(const QualifiedName& name) const
{
    if (!elementData())
        return;
    if (UNLIKELY(name == styleAttr && elementData()->styleAttributeIsDirty())) {
        ASSERT_WITH_SECURITY_IMPLICATION(isStyledElement());
        static_cast<const StyledElement*>(this)->synchronizeStyleAttributeInternal();
        return;
    }

    if (auto* svgElement = dynamicDowncast<SVGElement>(*this))
        const_cast<SVGElement&>(*svgElement).synchronizeAttribute(name);
}

static ALWAYS_INLINE bool isStyleAttribute(const Element& element, const AtomString& attributeLocalName)
{
    if (shouldIgnoreAttributeCase(element))
        return equalLettersIgnoringASCIICase(attributeLocalName, "style"_s);
    return attributeLocalName == styleAttr->localName();
}

ALWAYS_INLINE void Element::synchronizeAttribute(const AtomString& localName) const
{
    // This version of synchronizeAttribute() is streamlined for the case where you don't have a full QualifiedName,
    // e.g when called from DOM API.
    if (!elementData())
        return;
    if (elementData()->styleAttributeIsDirty() && isStyleAttribute(*this, localName)) {
        ASSERT_WITH_SECURITY_IMPLICATION(isStyledElement());
        static_cast<const StyledElement*>(this)->synchronizeStyleAttributeInternal();
        return;
    }

    if (auto* svgElement = dynamicDowncast<SVGElement>(*this))
        const_cast<SVGElement&>(*svgElement).synchronizeAttribute(QualifiedName(nullAtom(), localName, nullAtom()));
}

const AtomString& Element::getAttribute(const QualifiedName& name) const
{
    if (auto* attribute = getAttributeInternal(name))
        return attribute->value();
    return nullAtom();
}

AtomString Element::getAttributeForBindings(const QualifiedName& name, ResolveURLs resolveURLs) const
{
    auto* attribute = getAttributeInternal(name);
    if (!attribute)
        return nullAtom();

    if (!attributeContainsURL(*attribute))
        return attribute->value();

    switch (resolveURLs) {
    case ResolveURLs::Yes:
    case ResolveURLs::YesExcludingURLsForPrivacy:
    case ResolveURLs::NoExcludingURLsForPrivacy:
        return AtomString(completeURLsInAttributeValue(URL(), *attribute, resolveURLs));

    case ResolveURLs::No:
        break;
    }

    return attribute->value();
}

Vector<String> Element::getAttributeNames() const
{
    if (!hasAttributes())
        return { };

    auto attributes = attributesIterator();
    return WTF::map(attributes, [](auto& attribute) {
        return attribute.name().toString();
    });
}

bool Element::hasFocusableStyle() const
{
    auto isFocusableStyle = [](const RenderStyle* style) {
        return style && style->display() != DisplayType::None && style->display() != DisplayType::Contents
            && style->visibility() == Visibility::Visible && !style->effectiveInert()
            && (style->usedContentVisibility() != ContentVisibility::Hidden || style->contentVisibility() != ContentVisibility::Visible);
    };

    if (renderStyle())
        return isFocusableStyle(renderStyle());

    // Compute style in yet unstyled subtree without resolving full style.
    auto* style = const_cast<Element&>(*this).resolveComputedStyle(ResolveComputedStyleMode::RenderedOnly);
    return isFocusableStyle(style);
}

bool Element::isFocusable() const
{
    if (!isConnected() || !supportsFocus())
        return false;

    if (!renderer()) {
        // Elements in canvas fallback content are not rendered, but they are allowed to be
        // focusable as long as their canvas is displayed and visible.
        RefPtr canvas = ancestorsOfType<HTMLCanvasElement>(*this).first();
        if (canvas && !canvas->hasFocusableStyle())
            return false;
    }

    return hasFocusableStyle();
}

bool Element::isUserActionElementInActiveChain() const
{
    ASSERT(isUserActionElement());
    return protectedDocument()->userActionElements().isInActiveChain(*this);
}

bool Element::isUserActionElementActive() const
{
    ASSERT(isUserActionElement());
    return protectedDocument()->userActionElements().isActive(*this);
}

bool Element::isUserActionElementFocused() const
{
    ASSERT(isUserActionElement());
    return protectedDocument()->userActionElements().isFocused(*this);
}

bool Element::isUserActionElementHovered() const
{
    ASSERT(isUserActionElement());
    return protectedDocument()->userActionElements().isHovered(*this);
}

bool Element::isUserActionElementDragged() const
{
    ASSERT(isUserActionElement());
    return protectedDocument()->userActionElements().isBeingDragged(*this);
}

bool Element::isUserActionElementHasFocusVisible() const
{
    ASSERT(isUserActionElement());
    return protectedDocument()->userActionElements().hasFocusVisible(*this);
}

FormListedElement* Element::asFormListedElement()
{
    return nullptr;
}

ValidatedFormListedElement* Element::asValidatedFormListedElement()
{
    return nullptr;
}

#if ENABLE(ATTACHMENT_ELEMENT)
AttachmentAssociatedElement* Element::asAttachmentAssociatedElement()
{
    return nullptr;
}
#endif

bool Element::isUserActionElementHasFocusWithin() const
{
    ASSERT(isUserActionElement());
    return protectedDocument()->userActionElements().hasFocusWithin(*this);
}

void Element::setActive(bool value, Style::InvalidationScope invalidationScope)
{
    if (value == active())
        return;
    {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Active, value, invalidationScope);
        document().userActionElements().setActive(*this, value);
    }

    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return;

    if (!isDisabledFormControl() && renderer->style().hasUsedAppearance())
        renderer->repaint();
}

static bool shouldAlwaysHaveFocusVisibleWhenFocused(const Element& element)
{
    return element.isTextField() || element.isContentEditable() || is<HTMLSelectElement>(element);
}

void Element::setFocus(bool value, FocusVisibility visibility)
{
    if (value == focused())
        return;
    
    Style::PseudoClassChangeInvalidation focusStyleInvalidation(*this, { { CSSSelector::PseudoClass::Focus, value }, { CSSSelector::PseudoClass::FocusVisible, value } });
    protectedDocument()->userActionElements().setFocused(*this, value);

    // Shadow host with a slot that contain focused element is not considered focused.
    for (RefPtr root = containingShadowRoot(); root; root = root->host()->containingShadowRoot()) {
        root->setContainsFocusedElement(value);
        root->host()->invalidateStyle();
    }

    for (RefPtr element = this; element; element = element->parentElementInComposedTree())
        element->setHasFocusWithin(value);

    setHasFocusVisible(value && (visibility == FocusVisibility::Visible || shouldAlwaysHaveFocusVisibleWhenFocused(*this)));
}

void Element::setHasFocusVisible(bool value)
{
    Ref document = this->document();

#if ASSERT_ENABLED
    ASSERT(!value || focused());
    ASSERT(!focused() || !shouldAlwaysHaveFocusVisibleWhenFocused(*this) || value);
#endif

    if (hasFocusVisible() == value)
        return;

    document->userActionElements().setHasFocusVisible(*this, value);
}

void Element::setHasFocusWithin(bool value)
{
    if (hasFocusWithin() == value)
        return;
    {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::FocusWithin, value);
        protectedDocument()->userActionElements().setHasFocusWithin(*this, value);
    }
}

void Element::setHasTentativeFocus(bool value)
{
    // Tentative focus is used when trying to set the focus on a new element.
    for (Ref ancestor : composedTreeAncestors(*this)) {
        ASSERT(ancestor->hasFocusWithin() != value);
        protectedDocument()->userActionElements().setHasFocusWithin(ancestor, value);
    }
}

void Element::setHovered(bool value, Style::InvalidationScope invalidationScope, HitTestRequest)
{
    if (value == hovered())
        return;
    {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Hover, value, invalidationScope);
        protectedDocument()->userActionElements().setHovered(*this, value);
    }

    if (auto* style = renderStyle(); style && style->hasUsedAppearance()) {
        if (CheckedPtr renderer = this->renderer(); renderer && renderer->theme().supportsHover())
            renderer->repaint();
    }
}

void Element::setBeingDragged(bool value)
{
    if (value == isBeingDragged())
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::WebKitDrag, value);
    protectedDocument()->userActionElements().setBeingDragged(*this, value);
}

inline ScrollAlignment toScrollAlignmentForInlineDirection(std::optional<ScrollLogicalPosition> position, WritingMode writingMode, bool isLTR)
{
    auto blockFlowDirection = writingModeToBlockFlowDirection(writingMode);
    switch (position.value_or(ScrollLogicalPosition::Nearest)) {
    case ScrollLogicalPosition::Start: {
        switch (blockFlowDirection) {
        case BlockFlowDirection::TopToBottom:
        case BlockFlowDirection::BottomToTop: {
            return isLTR ? ScrollAlignment::alignLeftAlways : ScrollAlignment::alignRightAlways;
        }
        case BlockFlowDirection::LeftToRight:
        case BlockFlowDirection::RightToLeft: {
            return isLTR ? ScrollAlignment::alignTopAlways : ScrollAlignment::alignBottomAlways;
        }
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignLeftAlways;
        }
    }
    case ScrollLogicalPosition::Center:
        return ScrollAlignment::alignCenterAlways;
    case ScrollLogicalPosition::End: {
        switch (blockFlowDirection) {
        case BlockFlowDirection::TopToBottom:
        case BlockFlowDirection::BottomToTop: {
            return isLTR ? ScrollAlignment::alignRightAlways : ScrollAlignment::alignLeftAlways;
        }
        case BlockFlowDirection::LeftToRight:
        case BlockFlowDirection::RightToLeft: {
            return isLTR ? ScrollAlignment::alignBottomAlways : ScrollAlignment::alignTopAlways;
        }
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignRightAlways;
        }
    }
    case ScrollLogicalPosition::Nearest:
        return ScrollAlignment::alignToEdgeIfNeeded;
    default:
        ASSERT_NOT_REACHED();
        return ScrollAlignment::alignToEdgeIfNeeded;
    }
}

inline ScrollAlignment toScrollAlignmentForBlockDirection(std::optional<ScrollLogicalPosition> position, WritingMode writingMode)
{
    auto blockFlowDirection = writingModeToBlockFlowDirection(writingMode);
    switch (position.value_or(ScrollLogicalPosition::Start)) {
    case ScrollLogicalPosition::Start: {
        switch (blockFlowDirection) {
        case BlockFlowDirection::TopToBottom:
            return ScrollAlignment::alignTopAlways;
        case BlockFlowDirection::BottomToTop:
            return ScrollAlignment::alignBottomAlways;
        case BlockFlowDirection::LeftToRight:
            return ScrollAlignment::alignLeftAlways;
        case BlockFlowDirection::RightToLeft:
            return ScrollAlignment::alignRightAlways;
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignTopAlways;
        }
    }
    case ScrollLogicalPosition::Center:
        return ScrollAlignment::alignCenterAlways;
    case ScrollLogicalPosition::End: {
        switch (blockFlowDirection) {
        case BlockFlowDirection::TopToBottom:
            return ScrollAlignment::alignBottomAlways;
        case BlockFlowDirection::BottomToTop:
            return ScrollAlignment::alignTopAlways;
        case BlockFlowDirection::LeftToRight:
            return ScrollAlignment::alignRightAlways;
        case BlockFlowDirection::RightToLeft:
            return ScrollAlignment::alignLeftAlways;
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignBottomAlways;
        }
    }
    case ScrollLogicalPosition::Nearest:
        return ScrollAlignment::alignToEdgeIfNeeded;
    default:
        ASSERT_NOT_REACHED();
        return ScrollAlignment::alignToEdgeIfNeeded;
    }
}

static std::optional<std::pair<CheckedRef<RenderElement>, LayoutRect>> listBoxElementScrollIntoView(const Element& element)
{
    auto owningSelectElement = [](const Element& element) -> HTMLSelectElement* {
        if (auto* optionElement = dynamicDowncast<HTMLOptionElement>(element))
            return optionElement->ownerSelectElement();

        if (auto* optGroupElement = dynamicDowncast<HTMLOptGroupElement>(element))
            return optGroupElement->ownerSelectElement();

        return nullptr;
    };

    RefPtr selectElement = owningSelectElement(element);
    if (!selectElement)
        return std::nullopt;

    CheckedPtr renderListBox = dynamicDowncast<RenderListBox>(selectElement->renderer());
    if (!renderListBox)
        return std::nullopt;
    
    auto itemIndex = selectElement->listItems().find(&element);
    if (itemIndex != notFound)
        renderListBox->scrollToRevealElementAtListIndex(itemIndex);
    else
        itemIndex = 0;

    auto itemLocalRect = renderListBox->itemBoundingBoxRect({ }, itemIndex);
    return std::pair<CheckedRef<RenderElement>, LayoutRect> { renderListBox.releaseNonNull(), itemLocalRect };
}

void Element::scrollIntoView(std::optional<std::variant<bool, ScrollIntoViewOptions>>&& arg)
{
    Ref document = this->document();
    document->updateContentRelevancyForScrollIfNeeded(*this);

    document->updateLayoutIgnorePendingStylesheets(LayoutOptions::UpdateCompositingLayers);

    CheckedPtr<RenderElement> renderer;
    bool insideFixed = false;
    LayoutRect absoluteBounds;

    if (auto listBoxScrollResult = listBoxElementScrollIntoView(*this)) {
        renderer = WTFMove(listBoxScrollResult->first);
        absoluteBounds = listBoxScrollResult->second;

        auto listBoxAbsoluteBounds = renderer->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;
        absoluteBounds.moveBy(listBoxAbsoluteBounds.location());
    } else {
        renderer = this->renderer();
        if (!renderer)
            return;

        absoluteBounds = renderer->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;
    }

    ScrollIntoViewOptions options;
    if (arg) {
        auto value = arg.value();
        if (std::holds_alternative<ScrollIntoViewOptions>(value))
            options = std::get<ScrollIntoViewOptions>(value);
        else if (!std::get<bool>(value))
            options.blockPosition = ScrollLogicalPosition::End;
    }

    auto writingMode = renderer->style().writingMode();
    auto alignX = toScrollAlignmentForInlineDirection(options.inlinePosition, writingMode, renderer->style().isLeftToRightDirection());
    auto alignY = toScrollAlignmentForBlockDirection(options.blockPosition, writingMode);
    alignX.disableLegacyHorizontalVisibilityThreshold();

    bool isHorizontal = renderer->style().isHorizontalWritingMode();
    auto visibleOptions = ScrollRectToVisibleOptions {
        SelectionRevealMode::Reveal,
        isHorizontal ? alignX : alignY,
        isHorizontal ? alignY : alignX,
        ShouldAllowCrossOriginScrolling::No,
        options.behavior.value_or(ScrollBehavior::Auto)
    };
    LocalFrameView::scrollRectToVisible(absoluteBounds, *renderer, insideFixed, visibleOptions);
}

void Element::scrollIntoView(bool alignToTop) 
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets(LayoutOptions::UpdateCompositingLayers);

    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return;

    bool insideFixed;
    LayoutRect absoluteBounds = renderer->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;

    // Align to the top / bottom and to the closest edge.
    auto alignY = alignToTop ? ScrollAlignment::alignTopAlways : ScrollAlignment::alignBottomAlways;
    auto alignX = ScrollAlignment::alignToEdgeIfNeeded;
    alignX.disableLegacyHorizontalVisibilityThreshold();

    LocalFrameView::scrollRectToVisible(absoluteBounds, *renderer, insideFixed, { SelectionRevealMode::Reveal, alignX, alignY, ShouldAllowCrossOriginScrolling::No });
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    Ref document = this->document();
    document->updateContentRelevancyForScrollIfNeeded(*this);

    document->updateLayoutIgnorePendingStylesheets(LayoutOptions::UpdateCompositingLayers);

    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return;

    bool insideFixed;
    LayoutRect absoluteBounds = renderer->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;

    auto alignY = centerIfNeeded ? ScrollAlignment::alignCenterIfNeeded : ScrollAlignment::alignToEdgeIfNeeded;
    auto alignX = centerIfNeeded ? ScrollAlignment::alignCenterIfNeeded : ScrollAlignment::alignToEdgeIfNeeded;
    alignX.disableLegacyHorizontalVisibilityThreshold();

    LocalFrameView::scrollRectToVisible(absoluteBounds, *renderer, insideFixed, { SelectionRevealMode::Reveal, alignX, alignY, ShouldAllowCrossOriginScrolling::No });
}

void Element::scrollIntoViewIfNotVisible(bool centerIfNotVisible)
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets(LayoutOptions::UpdateCompositingLayers);

    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return;
    
    bool insideFixed;
    LayoutRect absoluteBounds = renderer->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;
    auto align = centerIfNotVisible ? ScrollAlignment::alignCenterIfNotVisible : ScrollAlignment::alignToEdgeIfNotVisible;
    LocalFrameView::scrollRectToVisible(absoluteBounds, *renderer, insideFixed, { SelectionRevealMode::Reveal, align, align, ShouldAllowCrossOriginScrolling::No });
}

void Element::scrollBy(const ScrollToOptions& options)
{
    ScrollToOptions scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options, 0, 0);
    FloatSize originalDelta(scrollToOptions.left.value(), scrollToOptions.top.value());
    scrollToOptions.left.value() += scrollLeft();
    scrollToOptions.top.value() += scrollTop();
    scrollTo(scrollToOptions, ScrollClamping::Clamped, ScrollSnapPointSelectionMethod::Directional, originalDelta);
}

void Element::scrollBy(double x, double y)
{
    scrollBy(ScrollToOptions(x, y));
}

void Element::scrollTo(const ScrollToOptions& options, ScrollClamping clamping, ScrollSnapPointSelectionMethod snapPointSelectionMethod, std::optional<FloatSize> originalScrollDelta)
{
    LOG_WITH_STREAM(Scrolling, stream << "Element " << *this << " scrollTo " << options.left << ", " << options.top << " behavior " << options.behavior);

    Ref document = this->document();
    if (!document->settings().CSSOMViewScrollingAPIEnabled()) {
        // If the element is the root element and document is in quirks mode, terminate these steps.
        // Note that WebKit always uses quirks mode document scrolling behavior. See Document::scrollingElement().
        if (this == document->documentElement())
            return;
    }
    
    if (RefPtr view = document->view())
        view->cancelScheduledScrolls();

    // We can avoid triggering layout if this is a scrollTo(0,0) on a element that cannot be the scrollingElement,
    // and was last scrolled to 0,0.
    auto canShortCircuitScroll = [&]() {
        if (!options.left || !options.top)
            return false;

        if (*options.left || *options.top)
            return false;

        // document().scrollingElement() requires up-to-date style, so bail if this element could be the scrollingElement().
        if (document->documentElement() == this || document->body() == this)
            return false;

        if (hasEverHadSmoothScroll())
            return false;

        return savedLayerScrollPosition().isZero();
    };

    if (canShortCircuitScroll())
        return;

    document->updateLayoutIgnorePendingStylesheets(LayoutOptions::UpdateCompositingLayers);

    if (document->scrollingElement() == this) {
        // If the element is the scrolling element and is not potentially scrollable,
        // invoke scroll() on window with options as the only argument, and terminate these steps.
        // FIXME: Scrolling an independently scrollable body is broken: webkit.org/b/161612.
        RefPtr window = document->domWindow();
        if (!window)
            return;

        window->scrollTo(options, clamping, snapPointSelectionMethod, originalScrollDelta);
        return;
    }

    // If the element does not have any associated CSS layout box, the element has no associated scrolling box,
    // or the element has no overflow, terminate these steps.
    CheckedPtr renderer = renderBox();
    if (!renderer || !renderer->hasNonVisibleOverflow())
        return;

    auto scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options,
        adjustForAbsoluteZoom(renderer->scrollLeft(), *renderer),
        adjustForAbsoluteZoom(renderer->scrollTop(), *renderer)
    );
    IntPoint scrollPosition(
        clampToInteger(scrollToOptions.left.value() * renderer->style().usedZoom()),
        clampToInteger(scrollToOptions.top.value() * renderer->style().usedZoom())
    );

    auto animated = useSmoothScrolling(scrollToOptions.behavior.value_or(ScrollBehavior::Auto), this) ? ScrollIsAnimated::Yes : ScrollIsAnimated::No;
    if (animated == ScrollIsAnimated::Yes)
        setHasEverHadSmoothScroll(true);

    auto scrollPositionChangeOptions = ScrollPositionChangeOptions::createProgrammaticWithOptions(clamping, animated, snapPointSelectionMethod, originalScrollDelta);
    renderer->setScrollPosition(scrollPosition, scrollPositionChangeOptions);
}

void Element::scrollTo(double x, double y)
{
    scrollTo(ScrollToOptions(x, y));
}

static double localZoomForRenderer(const RenderElement& renderer)
{
    // FIXME: This does the wrong thing if two opposing zooms are in effect and canceled each
    // other out, but the alternative is that we'd have to crawl up the whole render tree every
    // time (or store an additional bit in the RenderStyle to indicate that a zoom was specified).
    double zoomFactor = 1;
    if (renderer.style().usedZoom() != 1) {
        // Need to find the nearest enclosing RenderElement that set up
        // a differing zoom, and then we divide our result by it to eliminate the zoom.
        CheckedPtr prev = &renderer;
        for (CheckedPtr curr = prev->parent(); curr; curr = curr->parent()) {
            if (curr->style().usedZoom() != prev->style().usedZoom()) {
                zoomFactor = prev->style().zoom();
                break;
            }
            prev = curr;
        }
        if (prev->isRenderView())
            zoomFactor = prev->style().zoom();
    }
    return zoomFactor;
}

static int adjustContentsScrollPositionOrSizeForZoom(int value, const LocalFrame& frame)
{
    double zoomFactor = frame.pageZoomFactor() * frame.frameScaleFactor();
    if (zoomFactor == 1)
        return value;
    // FIXME (webkit.org/b/189397): Why can't we just ceil/floor?
    // Needed because of truncation (rather than rounding) when scaling up.
    if (zoomFactor > 1)
        value++;
    return static_cast<int>(value / zoomFactor);
}

enum LegacyCSSOMElementMetricsRoundingStrategy { Round, Floor };

static int convertToNonSubpixelValue(double value, const LegacyCSSOMElementMetricsRoundingStrategy roundStrategy = Round)
{
    return roundStrategy == Round ? std::round(value) : std::floor(value);
}

static int adjustOffsetForZoomAndSubpixelLayout(RenderBoxModelObject& renderer, const LayoutUnit& offset)
{
    auto offsetLeft = LayoutUnit { roundToInt(offset) };
    double zoomFactor = localZoomForRenderer(renderer);
    if (zoomFactor == 1)
        return convertToNonSubpixelValue(offsetLeft, Floor);
    return convertToNonSubpixelValue(offsetLeft / zoomFactor, Round);
}

static HashSet<TreeScope*> collectAncestorTreeScopeAsHashSet(Node& node)
{
    HashSet<TreeScope*> ancestors;
    for (auto* currentScope = &node.treeScope(); currentScope; currentScope = currentScope->parentTreeScope())
        ancestors.add(currentScope);
    return ancestors;
}

int Element::offsetLeftForBindings()
{
    auto offset = offsetLeft();

    RefPtr parent = offsetParent();
    if (!parent || !parent->isInShadowTree())
        return offset;

    ASSERT(&parent->document() == &document());
    if (&parent->treeScope() == &treeScope())
        return offset;

    auto ancestorTreeScopes = collectAncestorTreeScopeAsHashSet(*this);
    while (parent && !ancestorTreeScopes.contains(&parent->treeScope())) {
        offset += parent->offsetLeft();
        parent = parent->offsetParent();
    }

    return offset;
}

int Element::offsetLeft()
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);
    if (CheckedPtr renderer = renderBoxModelObject())
        return adjustOffsetForZoomAndSubpixelLayout(*renderer, renderer->offsetLeft());
    return 0;
}

int Element::offsetTopForBindings()
{
    auto offset = offsetTop();

    RefPtr parent = offsetParent();
    if (!parent || !parent->isInShadowTree())
        return offset;

    ASSERT(&parent->document() == &document());
    if (&parent->treeScope() == &treeScope())
        return offset;

    auto ancestorTreeScopes = collectAncestorTreeScopeAsHashSet(*this);
    while (parent && !ancestorTreeScopes.contains(&parent->treeScope())) {
        offset += parent->offsetTop();
        parent = parent->offsetParent();
    }

    return offset;
}

int Element::offsetTop()
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);
    if (CheckedPtr renderer = renderBoxModelObject())
        return adjustOffsetForZoomAndSubpixelLayout(*renderer, renderer->offsetTop());
    return 0;
}

int Element::offsetWidth()
{
    protectedDocument()->updateLayoutIfDimensionsOutOfDate(*this, DimensionsCheck::Width);
    if (CheckedPtr renderer = renderBoxModelObject()) {
        auto offsetWidth = LayoutUnit { roundToInt(renderer->offsetWidth()) };
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(offsetWidth, *renderer).toDouble());
    }
    return 0;
}

int Element::offsetHeight()
{
    protectedDocument()->updateLayoutIfDimensionsOutOfDate(*this, DimensionsCheck::Height);
    if (CheckedPtr renderer = renderBoxModelObject()) {
        auto offsetHeight = LayoutUnit { roundToInt(renderer->offsetHeight()) };
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(offsetHeight, *renderer).toDouble());
    }
    return 0;
}

RefPtr<Element> Element::offsetParentForBindings()
{
    RefPtr element = offsetParent();
    if (!element || !element->isInShadowTree())
        return element;
    while (element && !isDescendantOrShadowDescendantOf(&element->rootNode()))
        element = element->offsetParent();
    return element;
}

Element* Element::offsetParent()
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);
    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return nullptr;
    CheckedPtr offsetParent = renderer->offsetParent();
    return offsetParent ? offsetParent->element() : nullptr;
}

int Element::clientLeft()
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    if (CheckedPtr renderer = renderBox()) {
        auto clientLeft = LayoutUnit { roundToInt(renderer->clientLeft()) };
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(clientLeft, *renderer).toDouble());
    }
    return 0;
}

int Element::clientTop()
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    if (CheckedPtr renderer = renderBox()) {
        auto clientTop = LayoutUnit { roundToInt(renderer->clientTop()) };
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(clientTop, *renderer).toDouble());
    }
    return 0;
}

int Element::clientWidth()
{
    Ref document = this->document();
    document->updateLayoutIfDimensionsOutOfDate(*this, DimensionsCheck::Width);

    if (!document->hasLivingRenderTree())
        return 0;

    CheckedRef renderView = *document->renderView();

    // When in strict mode, clientWidth for the document element should return the width of the containing frame.
    // When in quirks mode, clientWidth for the body element should return the width of the containing frame.
    bool inQuirksMode = document->inQuirksMode();
    if ((!inQuirksMode && document->documentElement() == this) || (inQuirksMode && isHTMLElement() && document->bodyOrFrameset() == this))
        return adjustForAbsoluteZoom(renderView->frameView().layoutWidth(), renderView);
    
    if (CheckedPtr renderer = renderBox()) {
        auto clientWidth = LayoutUnit { roundToInt(renderer->clientWidth()) };
        // clientWidth/Height is the visual portion of the box content, not including
        // borders or scroll bars, but includes padding. And per
        // https://www.w3.org/TR/CSS2/tables.html#model,
        // table wrapper box is a principal block box that contains the table box
        // itself and any caption boxes, and table grid box is a block-level box that
        // contains the table's internal table boxes. When table's border is specified
        // in CSS, the border is added to table grid box, not table wrapper box.
        // Currently, WebKit doesn't have table wrapper box, and we are supposed to
        // retrieve clientWidth/Height from table wrapper box, not table grid box. So
        // when we retrieve clientWidth/Height, it includes table's border size.
        if (renderer->isRenderTable())
            clientWidth += renderer->borderLeft() + renderer->borderRight();
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(clientWidth, *renderer).toDouble());
    }
    return 0;
}

int Element::clientHeight()
{
    Ref document = this->document();
    document->updateLayoutIfDimensionsOutOfDate(*this, DimensionsCheck::Height);
    if (!document->hasLivingRenderTree())
        return 0;

    CheckedRef renderView = *document->renderView();

    // When in strict mode, clientHeight for the document element should return the height of the containing frame.
    // When in quirks mode, clientHeight for the body element should return the height of the containing frame.
    bool inQuirksMode = document->inQuirksMode();
    if ((!inQuirksMode && document->documentElement() == this) || (inQuirksMode && isHTMLElement() && document->bodyOrFrameset() == this))
        return adjustForAbsoluteZoom(renderView->frameView().layoutHeight(), renderView);

    if (CheckedPtr renderer = renderBox()) {
        auto clientHeight = LayoutUnit { roundToInt(renderer->clientHeight()) };
        // clientWidth/Height is the visual portion of the box content, not including
        // borders or scroll bars, but includes padding. And per
        // https://www.w3.org/TR/CSS2/tables.html#model,
        // table wrapper box is a principal block box that contains the table box
        // itself and any caption boxes, and table grid box is a block-level box that
        // contains the table's internal table boxes. When table's border is specified
        // in CSS, the border is added to table grid box, not table wrapper box.
        // Currently, WebKit doesn't have table wrapper box, and we are supposed to
        // retrieve clientWidth/Height from table wrapper box, not table grid box. So
        // when we retrieve clientWidth/Height, it includes table's border size.
        if (renderer->isRenderTable())
            clientHeight += renderer->borderTop() + renderer->borderBottom();
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(clientHeight, *renderer).toDouble());
    }
    return 0;
}

ALWAYS_INLINE LocalFrame* Element::documentFrameWithNonNullView() const
{
    auto* frame = document().frame();
    return frame && frame->view() ? frame : nullptr;
}

int Element::scrollLeft()
{
    Ref document = this->document();
    document->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    if (document->scrollingElement() == this) {
        if (RefPtr frame = documentFrameWithNonNullView())
            return adjustContentsScrollPositionOrSizeForZoom(frame->view()->contentsScrollPosition().x(), *frame);
        return 0;
    }

    if (CheckedPtr renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollLeft(), *renderer);
    return 0;
}

int Element::scrollTop()
{
    Ref document = this->document();
    document->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    if (document->scrollingElement() == this) {
        if (RefPtr frame = documentFrameWithNonNullView())
            return adjustContentsScrollPositionOrSizeForZoom(frame->view()->contentsScrollPosition().y(), *frame);
        return 0;
    }

    if (CheckedPtr renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollTop(), *renderer);
    return 0;
}

void Element::setScrollLeft(int newLeft)
{
    Ref document = this->document();
    document->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    auto options = ScrollPositionChangeOptions::createProgrammatic();
    options.animated = useSmoothScrolling(ScrollBehavior::Auto, this) ? ScrollIsAnimated::Yes : ScrollIsAnimated::No;
    if (options.animated == ScrollIsAnimated::Yes)
        setHasEverHadSmoothScroll(true);

    if (document->scrollingElement() == this) {
        if (RefPtr frame = documentFrameWithNonNullView()) {
            IntPoint position(static_cast<int>(newLeft * frame->pageZoomFactor() * frame->frameScaleFactor()), frame->view()->scrollY());
            frame->protectedView()->setScrollPosition(position, options);
        }
        return;
    }

    if (CheckedPtr renderer = renderBox()) {
        int clampedLeft = clampToInteger(newLeft * renderer->style().usedZoom());
        renderer->setScrollLeft(clampedLeft, options);
        if (CheckedPtr scrollableArea = renderer && renderer->layer() ? renderer->layer()->scrollableArea() : nullptr)
            scrollableArea->setScrollShouldClearLatchedState(true);
    }
}

void Element::setScrollTop(int newTop)
{
    Ref document = this->document();
    document->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    auto options = ScrollPositionChangeOptions::createProgrammatic();
    options.animated = useSmoothScrolling(ScrollBehavior::Auto, this) ? ScrollIsAnimated::Yes : ScrollIsAnimated::No;
    if (options.animated == ScrollIsAnimated::Yes)
        setHasEverHadSmoothScroll(true);

    if (document->scrollingElement() == this) {
        if (RefPtr frame = documentFrameWithNonNullView()) {
            IntPoint position(frame->view()->scrollX(), static_cast<int>(newTop * frame->pageZoomFactor() * frame->frameScaleFactor()));
            frame->protectedView()->setScrollPosition(position, options);
        }
        return;
    }

    if (CheckedPtr renderer = renderBox()) {
        int clampedTop = clampToInteger(newTop * renderer->style().usedZoom());
        renderer->setScrollTop(clampedTop, options);
        if (CheckedPtr scrollableArea = renderer && renderer->layer() ? renderer->layer()->scrollableArea() : nullptr)
            scrollableArea->setScrollShouldClearLatchedState(true);
    }
}

int Element::scrollWidth()
{
    Ref document = this->document();
    document->updateLayoutIfDimensionsOutOfDate(*this, DimensionsCheck::Width);

    if (document->scrollingElement() == this) {
        // FIXME (webkit.org/b/182289): updateLayoutIfDimensionsOutOfDate seems to ignore zoom level change.
        document->updateLayoutIgnorePendingStylesheets();
        if (RefPtr frame = documentFrameWithNonNullView())
            return adjustContentsScrollPositionOrSizeForZoom(frame->view()->contentsWidth(), *frame);
        return 0;
    }

    if (CheckedPtr renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollWidth(), *renderer);
    return 0;
}

int Element::scrollHeight()
{
    Ref document = this->document();
    document->updateLayoutIfDimensionsOutOfDate(*this, DimensionsCheck::Height);

    if (document->scrollingElement() == this) {
        // FIXME (webkit.org/b/182289): updateLayoutIfDimensionsOutOfDate seems to ignore zoom level change.
        document->updateLayoutIgnorePendingStylesheets();
        if (RefPtr frame = documentFrameWithNonNullView())
            return adjustContentsScrollPositionOrSizeForZoom(frame->view()->contentsHeight(), *frame);
        return 0;
    }

    if (CheckedPtr renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollHeight(), *renderer);
    return 0;
}

inline RefPtr<const SVGElement> elementWithSVGLayoutBox(const Element& element)
{
    RefPtr svg = dynamicDowncast<SVGElement>(element);
    return svg && svg->hasAssociatedSVGLayoutBox() ? svg : nullptr;
}

inline bool shouldObtainBoundsFromBoxModel(const Element* element)
{
    ASSERT(element);
    if (!element->renderer())
        return false;

    if (is<RenderBoxModelObject>(element->renderer()))
        return true;

    if (is<RenderSVGModelObject>(element->renderer()))
        return true;

    return false;
}

IntRect Element::boundsInRootViewSpace()
{
    Ref document = this->document();
    document->updateLayoutIgnorePendingStylesheets();

    RefPtr view = document->view();
    if (!view)
        return IntRect();

    Vector<FloatQuad> quads;

    if (RefPtr svgElement = elementWithSVGLayoutBox(*this)) {
        if (auto localRect = svgElement->getBoundingBox())
            quads.append(checkedRenderer()->localToAbsoluteQuad(*localRect));
    } else if (shouldObtainBoundsFromBoxModel(this)) {
        // Get the bounding rectangle from the box model.
        checkedRenderer()->absoluteQuads(quads);
    }

    return view->contentsToRootView(enclosingIntRect(unitedBoundingBoxes(quads)));
}

IntRect Element::boundingBoxInRootViewCoordinates() const
{
    if (CheckedPtr renderer = this->renderer())
        return protectedDocument()->view()->contentsToRootView(renderer->absoluteBoundingBoxRect());
    return IntRect();
}

static bool layoutOverflowRectContainsAllDescendants(const RenderBox& renderBox)
{
    if (renderBox.isRenderView())
        return true;

    if (!renderBox.element())
        return false;

    // If there are any position:fixed inside of us, game over.
    if (auto* viewPositionedObjects = renderBox.view().positionedObjects()) {
        for (CheckedRef positionedBox : *viewPositionedObjects) {
            if (positionedBox.ptr() == &renderBox)
                continue;
            if (positionedBox->isFixedPositioned() && renderBox.element()->contains(positionedBox->protectedElement().get()))
                return false;
        }
    }

    if (renderBox.canContainAbsolutelyPositionedObjects()) {
        // Our layout overflow will include all descendant positioned elements.
        return true;
    }

    // This renderer may have positioned descendants whose containing block is some ancestor.
    if (CheckedPtr containingBlock = RenderObject::containingBlockForPositionType(PositionType::Absolute, renderBox)) {
        if (auto* positionedObjects = containingBlock->positionedObjects()) {
            for (CheckedRef positionedBox : *positionedObjects) {
                if (positionedBox.ptr() == &renderBox)
                    continue;
                if (renderBox.protectedElement()->contains(positionedBox->protectedElement().get()))
                    return false;
            }
        }
    }
    return false;
}

LayoutRect Element::absoluteEventBounds(bool& boundsIncludeAllDescendantElements, bool& includesFixedPositionElements)
{
    boundsIncludeAllDescendantElements = false;
    includesFixedPositionElements = false;

    if (!renderer())
        return LayoutRect();

    LayoutRect result;
    if (RefPtr svgElement = elementWithSVGLayoutBox(*this)) {
        if (auto localRect = svgElement->getBoundingBox())
            result = LayoutRect(checkedRenderer()->localToAbsoluteQuad(*localRect, UseTransforms, &includesFixedPositionElements).boundingBox());
    } else {
        CheckedPtr renderer = this->renderer();
        if (auto* box = dynamicDowncast<RenderBox>(renderer.get())) {
            bool computedBounds = false;
            
            if (CheckedPtr fragmentedFlow = box->enclosingFragmentedFlow()) {
                bool wasFixed = false;
                Vector<FloatQuad> quads;
                if (fragmentedFlow->absoluteQuadsForBox(quads, &wasFixed, *box)) {
                    result = LayoutRect(unitedBoundingBoxes(quads));
                    computedBounds = true;
                } else {
                    // Probably columns. Just return the bounds of the multicol block for now.
                    // FIXME: this doesn't handle nested columns.
                    if (CheckedPtr multicolContainer = dynamicDowncast<RenderBox>(fragmentedFlow->parent())) {
                        auto overflowRect = multicolContainer->layoutOverflowRect();
                        result = LayoutRect(multicolContainer->localToAbsoluteQuad(FloatRect(overflowRect), UseTransforms, &includesFixedPositionElements).boundingBox());
                        computedBounds = true;
                    }
                }
            }

            if (!computedBounds) {
                LayoutRect overflowRect = box->layoutOverflowRect();
                result = LayoutRect(box->localToAbsoluteQuad(FloatRect(overflowRect), UseTransforms, &includesFixedPositionElements).boundingBox());
                boundsIncludeAllDescendantElements = layoutOverflowRectContainsAllDescendants(*box);
            }
        } else
            result = LayoutRect(renderer->absoluteBoundingBoxRect(true /* useTransforms */, &includesFixedPositionElements));
    }

    return result;
}

LayoutRect Element::absoluteEventBoundsOfElementAndDescendants(bool& includesFixedPositionElements)
{
    bool boundsIncludeDescendants;
    LayoutRect result = absoluteEventBounds(boundsIncludeDescendants, includesFixedPositionElements);
    if (boundsIncludeDescendants)
        return result;

    for (Ref child : childrenOfType<Element>(*this)) {
        bool includesFixedPosition = false;
        LayoutRect childBounds = child->absoluteEventBoundsOfElementAndDescendants(includesFixedPosition);
        includesFixedPositionElements |= includesFixedPosition;
        result.unite(childBounds);
    }

    return result;
}

LayoutRect Element::absoluteEventHandlerBounds(bool& includesFixedPositionElements)
{
    // This is not web-exposed, so don't call the FOUC-inducing updateLayoutIgnorePendingStylesheets().
    if (!document().view())
        return LayoutRect();

    return absoluteEventBoundsOfElementAndDescendants(includesFixedPositionElements);
}

static std::optional<std::pair<CheckedRef<RenderObject>, LayoutRect>> listBoxElementBoundingBox(const Element& element)
{
    auto owningSelectElement = [](const Element& element) -> HTMLSelectElement* {
        if (auto* optionElement = dynamicDowncast<HTMLOptionElement>(element))
            return optionElement->ownerSelectElement();
        
        if (auto* optGroupElement = dynamicDowncast<HTMLOptGroupElement>(element))
            return optGroupElement->ownerSelectElement();

        return nullptr;
    };

    RefPtr selectElement = owningSelectElement(element);
    if (!selectElement)
        return std::nullopt;

    CheckedPtr listBoxRenderer = dynamicDowncast<RenderListBox>(selectElement->renderer());
    if (!listBoxRenderer)
        return std::nullopt;

    std::optional<LayoutRect> boundingBox;
    if (RefPtr optionElement = dynamicDowncast<HTMLOptionElement>(element))
        boundingBox = listBoxRenderer->localBoundsOfOption(*optionElement);
    else if (RefPtr optGroupElement = dynamicDowncast<HTMLOptGroupElement>(element))
        boundingBox = listBoxRenderer->localBoundsOfOptGroup(*optGroupElement);

    if (!boundingBox)
        return { };

    return std::pair<CheckedRef<RenderObject>, LayoutRect> { listBoxRenderer.releaseNonNull(), boundingBox.value() };
}

Ref<DOMRectList> Element::getClientRects()
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    CheckedPtr<RenderObject> renderer = this->renderer();

    Vector<FloatQuad> quads;

    if (RefPtr svgElement = elementWithSVGLayoutBox(*this)) {
        if (auto localRect = svgElement->getBoundingBox())
            quads.append(renderer->localToAbsoluteQuad(*localRect));
    } else if (auto pair = listBoxElementBoundingBox(*this)) {
        renderer = WTFMove(pair.value().first);
        quads.append(renderer->localToAbsoluteQuad(FloatQuad { pair.value().second }));
    } else if (shouldObtainBoundsFromBoxModel(this))
        renderer->absoluteQuads(quads);

    // FIXME: Handle table/inline-table with a caption.

    if (quads.isEmpty())
        return DOMRectList::create();

    protectedDocument()->convertAbsoluteToClientQuads(quads, renderer->style());
    return DOMRectList::create(quads);
}

std::optional<std::pair<CheckedPtr<RenderObject>, FloatRect>> Element::boundingAbsoluteRectWithoutLayout() const
{
    CheckedPtr<RenderObject> renderer = this->renderer();
    Vector<FloatQuad> quads;
    if (RefPtr svgElement = elementWithSVGLayoutBox(*this)) {
        if (auto localRect = svgElement->getBoundingBox())
            quads.append(renderer->localToAbsoluteQuad(*localRect));
    } else if (auto pair = listBoxElementBoundingBox(*this)) {
        renderer = WTFMove(pair.value().first);
        quads.append(renderer->localToAbsoluteQuad(FloatQuad { pair.value().second }));
    } else if (shouldObtainBoundsFromBoxModel(this))
        renderer->absoluteQuads(quads);

    if (quads.isEmpty())
        return std::nullopt;

    return std::make_pair(WTFMove(renderer), unitedBoundingBoxes(quads));
}

FloatRect Element::boundingClientRect()
{
    Ref document = this->document();
    auto needsLayout = [&] {
        if (!document->haveStylesheetsLoaded())
            return true;

        if (RefPtr owner = document->ownerElement(); owner && owner->protectedDocument()->needsStyleRecalc())
            return true;

        document->updateRelevancyOfContentVisibilityElements();
        document->updateStyleIfNeeded();
        // FIXME: Expand this optimization to other elements.
        if (!renderer() || !renderer()->isBody() || !renderer()->containingBlock() || !renderer()->containingBlock()->isDocumentElementRenderer())
            return true;
        auto& bodyRenderer = *renderer();
        if (bodyRenderer.selfNeedsLayout() || bodyRenderer.containingBlock()->selfNeedsLayout() || (document->renderView() && document->renderView()->selfNeedsLayout()))
            return true;
        auto& bodyStyle = bodyRenderer.style();
        if (!bodyStyle.logicalWidth().isFixed() && !bodyStyle.logicalWidth().isPercent())
            return true;
        if (!bodyStyle.logicalHeight().isFixed() && !bodyStyle.logicalHeight().isPercent())
            return true;
        // FIXME: Add support for scroll position.
        return bodyStyle.hasViewportConstrainedPosition();
    };

    if (needsLayout())
        document->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    auto pair = boundingAbsoluteRectWithoutLayout();
    if (!pair)
        return { };
    CheckedPtr renderer = WTFMove(pair->first);
    FloatRect result = pair->second;
    document->convertAbsoluteToClientRect(result, renderer->style());
    return result;
}

Ref<DOMRect> Element::getBoundingClientRect()
{
    return DOMRect::create(boundingClientRect());
}

IntRect Element::screenRect() const
{
    if (CheckedPtr renderer = this->renderer())
        return document().view()->contentsToScreen(renderer->absoluteBoundingBoxRect());
    return IntRect();
}

const AtomString& Element::getAttribute(const AtomString& qualifiedName) const
{
    if (auto* attribute = getAttributeInternal(qualifiedName))
        return attribute->value();
    return nullAtom();
}

AtomString Element::getAttributeForBindings(const AtomString& qualifiedName, ResolveURLs resolveURLs) const
{
    auto* attribute = getAttributeInternal(qualifiedName);
    if (!attribute)
        return nullAtom();

    if (!attributeContainsURL(*attribute))
        return attribute->value();

    switch (resolveURLs) {
    case ResolveURLs::Yes:
    case ResolveURLs::YesExcludingURLsForPrivacy:
    case ResolveURLs::NoExcludingURLsForPrivacy:
        return AtomString(completeURLsInAttributeValue(URL(), *attribute, resolveURLs));

    case ResolveURLs::No:
        break;
    }

    return attribute->value();
}

const AtomString& Element::getAttributeNS(const AtomString& namespaceURI, const AtomString& localName) const
{
    return getAttribute(QualifiedName(nullAtom(), localName, namespaceURI));
}

// https://dom.spec.whatwg.org/#dom-element-toggleattribute
ExceptionOr<bool> Element::toggleAttribute(const AtomString& qualifiedName, std::optional<bool> force)
{
    if (!Document::isValidName(qualifiedName))
        return Exception { ExceptionCode::InvalidCharacterError, makeString("Invalid qualified name: '", qualifiedName, "'") };

    synchronizeAttribute(qualifiedName);

    auto caseAdjustedQualifiedName = shouldIgnoreAttributeCase(*this) ? qualifiedName.convertToASCIILowercase() : qualifiedName;
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(caseAdjustedQualifiedName, false) : ElementData::attributeNotFound;
    if (index == ElementData::attributeNotFound) {
        if (!force || *force) {
            setAttributeInternal(index, QualifiedName { nullAtom(), caseAdjustedQualifiedName, nullAtom() }, emptyAtom(), InSynchronizationOfLazyAttribute::No);
            return true;
        }
        return false;
    }

    if (!force || !*force) {
        removeAttributeInternal(index, InSynchronizationOfLazyAttribute::No);
        return false;
    }
    return true;
}

ExceptionOr<void> Element::setAttribute(const AtomString& qualifiedName, const AtomString& value)
{
    if (!Document::isValidName(qualifiedName))
        return Exception { ExceptionCode::InvalidCharacterError, makeString("Invalid qualified name: '", qualifiedName, "'") };

    synchronizeAttribute(qualifiedName);
    auto caseAdjustedQualifiedName = shouldIgnoreAttributeCase(*this) ? qualifiedName.convertToASCIILowercase() : qualifiedName;
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(caseAdjustedQualifiedName, false) : ElementData::attributeNotFound;
    auto name = index != ElementData::attributeNotFound ? attributeAt(index).name() : QualifiedName { nullAtom(), caseAdjustedQualifiedName, nullAtom() };
    setAttributeInternal(index, name, value, InSynchronizationOfLazyAttribute::No);

    return { };
}

void Element::setAttribute(const QualifiedName& name, const AtomString& value)
{
    synchronizeAttribute(name);
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, InSynchronizationOfLazyAttribute::No);
}

void Element::setAttributeWithoutOverwriting(const QualifiedName& name, const AtomString& value)
{
    synchronizeAttribute(name);
    if (!elementData() || elementData()->findAttributeIndexByName(name) == ElementData::attributeNotFound)
        addAttributeInternal(name, value, InSynchronizationOfLazyAttribute::No);
}

void Element::setAttributeWithoutSynchronization(const QualifiedName& name, const AtomString& value)
{
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, InSynchronizationOfLazyAttribute::No);
}

void Element::setSynchronizedLazyAttribute(const QualifiedName& name, const AtomString& value)
{
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, InSynchronizationOfLazyAttribute::Yes);
}

inline void Element::setAttributeInternal(unsigned index, const QualifiedName& name, const AtomString& newValue, InSynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ASSERT_WITH_MESSAGE(refCount() || parentNode(), "Attribute must not be set on an element before adoptRef");

    if (newValue.isNull()) {
        if (index != ElementData::attributeNotFound)
            removeAttributeInternal(index, inSynchronizationOfLazyAttribute);
        return;
    }

    if (index == ElementData::attributeNotFound) {
        addAttributeInternal(name, newValue, inSynchronizationOfLazyAttribute);
        return;
    }

    if (inSynchronizationOfLazyAttribute == InSynchronizationOfLazyAttribute::Yes) {
        ensureUniqueElementData().attributeAt(index).setValue(newValue);
        return;
    }

    const Attribute& attribute = attributeAt(index);
    QualifiedName attributeName = attribute.name();
    AtomString oldValue = attribute.value();

    willModifyAttribute(attributeName, oldValue, newValue);

    if (newValue != oldValue) {
        Style::AttributeChangeInvalidation styleInvalidation(*this, name, oldValue, newValue);
        ensureUniqueElementData().attributeAt(index).setValue(newValue);
    }

    didModifyAttribute(attributeName, oldValue, newValue);
}

static inline AtomString makeIdForStyleResolution(const AtomString& value, bool inQuirksMode)
{
    if (inQuirksMode)
        return value.convertToASCIILowercase();
    return value;
}

bool Element::isElementReflectionAttribute(const Settings& settings, const QualifiedName& name)
{
    return name == HTMLNames::aria_activedescendantAttr
        || (settings.popoverAttributeEnabled() && name == HTMLNames::popovertargetAttr)
        || (settings.invokerAttributesEnabled() && name == HTMLNames::invoketargetAttr);
}

bool Element::isElementsArrayReflectionAttribute(const QualifiedName& name)
{
    switch (name.nodeName()) {
    case AttributeNames::aria_controlsAttr:
    case AttributeNames::aria_describedbyAttr:
    case AttributeNames::aria_detailsAttr:
    case AttributeNames::aria_errormessageAttr:
    case AttributeNames::aria_flowtoAttr:
    case AttributeNames::aria_labelledbyAttr:
    case AttributeNames::aria_ownsAttr:
        return true;
    default:
        break;
    }
    return false;
}

void Element::notifyAttributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason reason)
{
    attributeChanged(name, oldValue, newValue, reason);

    document().incDOMTreeVersion();

    if (UNLIKELY(isDefinedCustomElement()))
        CustomElementReactionQueue::enqueueAttributeChangedCallbackIfNeeded(*this, name, oldValue, newValue);

    if (oldValue != newValue) {
        invalidateNodeListAndCollectionCachesInAncestorsForAttribute(name);

        if (CheckedPtr cache = document().existingAXObjectCache())
            cache->deferAttributeChangeIfNeeded(*this, name, oldValue, newValue);
    }
}

void Element::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason)
{
    if (oldValue == newValue)
        return;

    switch (name.nodeName()) {
    case AttributeNames::classAttr:
        classAttributeChanged(newValue);
        break;
    case AttributeNames::idAttr: {
        AtomString oldId = elementData()->idForStyleResolution();
        AtomString newId = makeIdForStyleResolution(newValue, document().inQuirksMode());
        if (newId != oldId) {
            Style::IdChangeInvalidation styleInvalidation(*this, oldId, newId);
            elementData()->setIdForStyleResolution(newId);
        }

        if (CheckedPtr observerRegistry = treeScope().idTargetObserverRegistryIfExists()) {
            if (!oldValue.isEmpty())
                observerRegistry->notifyObservers(oldValue);
            if (!newValue.isEmpty())
                observerRegistry->notifyObservers(newValue);
        }
        break;
    }
    case AttributeNames::nameAttr:
        elementData()->setHasNameAttribute(!newValue.isNull());
        break;
    case AttributeNames::nonceAttr:
        if (is<HTMLElement>(*this) || is<SVGElement>(*this))
            setNonce(newValue.isNull() ? emptyAtom() : newValue);
        break;
    case AttributeNames::useragentpartAttr:
        if (needsStyleInvalidation() && isInUserAgentShadowTree())
            invalidateStyleForSubtree();
        break;
    case AttributeNames::slotAttr:
        if (RefPtr parent = parentElement()) {
            if (RefPtr shadowRoot = parent->shadowRoot())
                shadowRoot->hostChildElementDidChangeSlotAttribute(*this, oldValue, newValue);
        }
        break;
    case AttributeNames::partAttr:
        partAttributeChanged(newValue);
        break;
    case AttributeNames::exportpartsAttr:
        if (RefPtr shadowRoot = this->shadowRoot()) {
            shadowRoot->invalidatePartMappings();
            Style::Invalidator::invalidateShadowParts(*shadowRoot);
        }
        break;
    case AttributeNames::accesskeyAttr:
        protectedDocument()->invalidateAccessKeyCache();
        break;
    case AttributeNames::XML::langAttr:
    case AttributeNames::langAttr: {
        if (name == HTMLNames::langAttr)
            setHasLangAttr(!newValue.isNull() && (isHTMLElement() || isSVGElement()));
        else
            setHasXMLLangAttr(!newValue.isNull());
        Ref document = this->document();
        if (document->documentElement() == this)
            document->setDocumentElementLanguage(langFromAttribute());
        else
            updateEffectiveLangStateAndPropagateToDescendants();
        break;
    }
    default: {
        Ref document = this->document();
        if (isElementReflectionAttribute(document->settings(), name) || isElementsArrayReflectionAttribute(name)) {
            if (auto* map = explicitlySetAttrElementsMapIfExists())
                map->remove(name);
        }
        break;
    }
    }
}

void Element::updateEffectiveLangStateAndPropagateToDescendants()
{
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Lang, Style::PseudoClassChangeInvalidation::AnyValue);
    updateEffectiveLangState();

    for (auto it = descendantsOfType<Element>(*this).begin(); it;) {
        Ref element = *it;
        if (element->hasLanguageAttribute()) {
            it.traverseNextSkippingChildren();
            continue;
        }
        Style::PseudoClassChangeInvalidation styleInvalidation(element, CSSSelector::PseudoClass::Lang, Style::PseudoClassChangeInvalidation::AnyValue);
        element->updateEffectiveLangStateFromParent();
        it.traverseNext();
    }
}

ExplicitlySetAttrElementsMap& Element::explicitlySetAttrElementsMap()
{
    return ensureElementRareData().explicitlySetAttrElementsMap();
}

ExplicitlySetAttrElementsMap* Element::explicitlySetAttrElementsMapIfExists() const
{
    return hasRareData() ? &elementRareData()->explicitlySetAttrElementsMap() : nullptr;
}

RefPtr<Element> Element::getElementAttribute(const QualifiedName& attributeName) const
{
    ASSERT(isElementReflectionAttribute(document().settings(), attributeName));

    if (auto* map = explicitlySetAttrElementsMapIfExists()) {
        auto it = map->find(attributeName);
        if (it != map->end()) {
            ASSERT(it->value.size() == 1);
            RefPtr element = it->value[0].get();
            if (element && isDescendantOrShadowDescendantOf(element->rootNode()))
                return element;
            return nullptr;
        }
    }

    auto id = getAttribute(attributeName);
    if (id.isNull())
        return nullptr;

    return treeScope().getElementById(id);
}

void Element::setElementAttribute(const QualifiedName& attributeName, Element* element)
{
    ASSERT(isElementReflectionAttribute(document().settings(), attributeName));

    if (!element) {
        if (auto* map = explicitlySetAttrElementsMapIfExists())
            map->remove(attributeName);
        removeAttribute(attributeName);
        return;
    }

    setAttribute(attributeName, emptyAtom());

    explicitlySetAttrElementsMap().set(attributeName, Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> { element });
    
    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->updateRelations(*this, attributeName);
}

std::optional<Vector<Ref<Element>>> Element::getElementsArrayAttribute(const QualifiedName& attributeName) const
{
    ASSERT(isElementsArrayReflectionAttribute(attributeName));

    if (auto* map = explicitlySetAttrElementsMapIfExists()) {
        if (auto it = map->find(attributeName); it != map->end()) {
            return compactMap(it->value, [&](auto& weakElement) -> std::optional<Ref<Element>> {
                RefPtr element = weakElement.get();
                if (element && isDescendantOrShadowDescendantOf(element->rootNode()))
                    return element.releaseNonNull();
                return std::nullopt;
            });
        }
    }

    auto attr = attributeName;
    if (attr == HTMLNames::aria_labelledbyAttr && !hasAttribute(HTMLNames::aria_labelledbyAttr) && hasAttribute(HTMLNames::aria_labeledbyAttr))
        attr = HTMLNames::aria_labeledbyAttr;

    if (!hasAttribute(attr))
        return std::nullopt;

    SpaceSplitString ids(getAttribute(attr), SpaceSplitString::ShouldFoldCase::No);
    Vector<Ref<Element>> elements;
    for (unsigned i = 0; i < ids.size(); ++i) {
        if (RefPtr element = treeScope().getElementById(ids[i]))
            elements.append(element.releaseNonNull());
    }
    return elements;
}

void Element::setElementsArrayAttribute(const QualifiedName& attributeName, std::optional<Vector<Ref<Element>>>&& elements)
{
    ASSERT(isElementsArrayReflectionAttribute(attributeName));

    if (!elements) {
        if (auto* map = explicitlySetAttrElementsMapIfExists())
            map->remove(attributeName);
        removeAttribute(attributeName);
        return;
    }

    setAttribute(attributeName, emptyAtom());

    auto newElements = copyToVectorOf<WeakPtr<Element, WeakPtrImplWithEventTargetData>>(*elements);
    explicitlySetAttrElementsMap().set(attributeName, WTFMove(newElements));
    
    if (CheckedPtr cache = document().existingAXObjectCache()) {
        for (auto element : elements.value()) {
            // FIXME: Should this pass `element` instead of `*this`?
            cache->updateRelations(*this, attributeName);
        }
    }
}

void Element::classAttributeChanged(const AtomString& newClassString)
{
    // Note: We'll need ElementData, but it doesn't have to be UniqueElementData.
    if (!elementData())
        ensureUniqueElementData();

    {
        auto shouldFoldCase = document().inQuirksMode() ? SpaceSplitString::ShouldFoldCase::Yes : SpaceSplitString::ShouldFoldCase::No;
        SpaceSplitString newClassNames(newClassString, shouldFoldCase);
        Style::ClassChangeInvalidation styleInvalidation(*this, elementData()->classNames(), newClassNames);
        document().invalidateQuerySelectorAllResultsForClassAttributeChange(*this, elementData()->classNames(), newClassNames);
        elementData()->setClassNames(WTFMove(newClassNames));
    }

    if (hasRareData()) {
        if (auto* classList = elementRareData()->classList())
            classList->associatedAttributeValueChanged(newClassString);
    }
}

void Element::partAttributeChanged(const AtomString& newValue)
{
    SpaceSplitString newParts(newValue, SpaceSplitString::ShouldFoldCase::No);
    if (!newParts.isEmpty() || !partNames().isEmpty())
        ensureElementRareData().setPartNames(WTFMove(newParts));

    if (hasRareData()) {
        if (auto* partList = elementRareData()->partList())
            partList->associatedAttributeValueChanged(newValue);
    }

    if (needsStyleInvalidation() && isInShadowTree())
        invalidateStyleInternal();
}

URL Element::absoluteLinkURL() const
{
    if (!isLink())
        return URL();

    AtomString linkAttribute;
    if (hasTagName(SVGNames::aTag))
        linkAttribute = getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr);
    else
        linkAttribute = getAttribute(HTMLNames::hrefAttr);

    if (linkAttribute.isEmpty())
        return URL();

    return document().completeURL(linkAttribute);
}

void Element::setIsLink(bool flag)
{
    if (isLink() == flag)
        return;
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, {
        { CSSSelector::PseudoClass::AnyLink, flag },
        { CSSSelector::PseudoClass::Link, flag }
    });
    setStateFlag(StateFlag::IsLink, flag);
}

#if ENABLE(TOUCH_EVENTS)

bool Element::allowsDoubleTapGesture() const
{
    if (renderStyle() && renderStyle()->touchActions() != TouchAction::Auto)
        return false;

    RefPtr parent = parentElement();
    return !parent || parent->allowsDoubleTapGesture();
}

#endif

Style::Resolver& Element::styleResolver()
{
    if (auto* shadowRoot = containingShadowRoot())
        return shadowRoot->styleScope().resolver();

    return document().styleScope().resolver();
}

Style::ResolvedStyle Element::resolveStyle(const Style::ResolutionContext& resolutionContext)
{
    return styleResolver().styleForElement(*this, resolutionContext);
}

void invalidateForSiblingCombinators(Element* sibling)
{
    for (; sibling; sibling = sibling->nextElementSibling()) {
        if (sibling->styleIsAffectedByPreviousSibling())
            sibling->invalidateStyleInternal();
        if (sibling->descendantsAffectedByPreviousSibling()) {
            for (RefPtr siblingChild = sibling->firstElementChild(); siblingChild; siblingChild = siblingChild->nextElementSibling())
                siblingChild->invalidateStyleForSubtreeInternal();
        }
        if (!sibling->affectsNextSiblingElementStyle())
            return;
    }
}

static void invalidateSiblingsIfNeeded(Element& element)
{
    if (!element.affectsNextSiblingElementStyle())
        return;
    auto* parent = element.parentElement();
    if (parent && parent->styleValidity() >= Style::Validity::SubtreeInvalid)
        return;

    invalidateForSiblingCombinators(element.nextElementSibling());
}

void Element::invalidateStyle()
{
    Node::invalidateStyle(Style::Validity::ElementInvalid);
    invalidateSiblingsIfNeeded(*this);
}

void Element::invalidateStyleAndLayerComposition()
{
    Node::invalidateStyle(Style::Validity::ElementInvalid, Style::InvalidationMode::RecompositeLayer);
    invalidateSiblingsIfNeeded(*this);
}

void Element::invalidateStyleForSubtree()
{
    Node::invalidateStyle(Style::Validity::SubtreeInvalid);
    invalidateSiblingsIfNeeded(*this);
}

void Element::invalidateStyleAndRenderersForSubtree()
{
    Node::invalidateStyle(Style::Validity::SubtreeInvalid, Style::InvalidationMode::RebuildRenderer);
}

void Element::invalidateRenderer()
{
    Node::invalidateStyle(Style::Validity::Valid, Style::InvalidationMode::RebuildRenderer);
}

void Element::invalidateStyleInternal()
{
    Node::invalidateStyle(Style::Validity::ElementInvalid);
}

void Element::invalidateStyleForAnimation()
{
    Node::invalidateStyle(Style::Validity::AnimationInvalid);
}

void Element::invalidateStyleForSubtreeInternal()
{
    Node::invalidateStyle(Style::Validity::SubtreeInvalid);
}

void Element::invalidateForQueryContainerSizeChange()
{
    // FIXME: Ideally we would just recompute things that are actually affected by containers queries within the subtree.
    Node::invalidateStyle(Style::Validity::SubtreeInvalid);
    setElementStateFlag(ElementStateFlag::NeedsUpdateQueryContainerDependentStyle);
}

void Element::invalidateForResumingQueryContainerResolution()
{
    markAncestorsForInvalidatedStyle();
}

bool Element::needsUpdateQueryContainerDependentStyle() const
{
    return hasElementStateFlag(ElementStateFlag::NeedsUpdateQueryContainerDependentStyle);
}

void Element::clearNeedsUpdateQueryContainerDependentStyle()
{
    clearElementStateFlag(ElementStateFlag::NeedsUpdateQueryContainerDependentStyle);
}

void Element::invalidateEventListenerRegions()
{
    // Event listener region is updated via style update.
    invalidateStyleInternal();
}

bool Element::hasDisplayContents() const
{
    if (!hasRareData())
        return false;
    auto* style = elementRareData()->displayContentsOrNoneStyle();
    return style && style->display() == DisplayType::Contents;
}

bool Element::hasDisplayNone() const
{
    if (!hasRareData())
        return false;
    auto* style = elementRareData()->displayContentsOrNoneStyle();
    return style && style->display() == DisplayType::None;
}

void Element::storeDisplayContentsOrNoneStyle(std::unique_ptr<RenderStyle> style)
{
    // This is used by RenderTreeBuilder to store the style for Elements with display:{contents|none}.
    // Normally style is held in renderers but display:contents doesn't generate one.
    // This is kept distinct from ElementRareData::computedStyle() which can update outside style resolution.
    // This way renderOrDisplayContentsStyle() always returns consistent styles matching the rendering state.
    ASSERT(style && (style->display() == DisplayType::Contents || style->display() == DisplayType::None));
    ASSERT(!renderer() || isPseudoElement());
    ensureElementRareData().setDisplayContentsOrNoneStyle(WTFMove(style));
}

void Element::clearDisplayContentsOrNoneStyle()
{
    if (!hasRareData())
        return;
    elementRareData()->setDisplayContentsOrNoneStyle(nullptr);
}

// Returns true is the given attribute is an event handler.
// We consider an event handler any attribute that begins with "on".
// It is a simple solution that has the advantage of not requiring any
// code or configuration change if a new event handler is defined.

bool Element::isEventHandlerAttribute(const Attribute& attribute) const
{
    return attribute.name().namespaceURI().isNull() && attribute.name().localName().startsWith("on"_s);
}

bool Element::attributeContainsJavaScriptURL(const Attribute& attribute) const
{
    return isURLAttribute(attribute) && WTF::protocolIsJavaScript(attribute.value());
}

void Element::stripScriptingAttributes(Vector<Attribute>& attributeVector) const
{
    attributeVector.removeAllMatching([this](auto& attribute) -> bool {
        return this->isEventHandlerAttribute(attribute)
            || this->attributeContainsJavaScriptURL(attribute)
            || this->isHTMLContentAttribute(attribute);
    });
}

void Element::parserSetAttributes(std::span<const Attribute> attributes)
{
    ASSERT(!isConnected());
    ASSERT(!parentNode());
    ASSERT(!m_elementData);

    if (attributes.size()) {
        if (auto* sharedObjectPool = document().sharedObjectPool())
            m_elementData = sharedObjectPool->cachedShareableElementDataWithAttributes(attributes);
        else
            m_elementData = ShareableElementData::createWithAttributes(attributes);
    }

    if (auto* inputElement = dynamicDowncast<HTMLInputElement>(*this)) {
        DelayedUpdateValidityScope delayedUpdateValidityScope(*inputElement);
        inputElement->initializeInputTypeAfterParsingOrCloning();
    }

    // Use attributes instead of m_elementData because attributeChanged might modify m_elementData.
    for (const auto& attribute : attributes)
        notifyAttributeChanged(attribute.name(), nullAtom(), attribute.value(), AttributeModificationReason::Parser);
}

void Element::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    ASSERT_WITH_SECURITY_IMPLICATION(&document() == &newDocument);

    if (oldDocument.inQuirksMode() != document().inQuirksMode()) {
        ensureUniqueElementData();
        // ElementData::m_classNames or ElementData::m_idForStyleResolution need to be updated with the right case.
        if (hasID())
            notifyAttributeChanged(idAttr, nullAtom(), getIdAttribute());
        if (hasClass())
            notifyAttributeChanged(classAttr, nullAtom(), getAttribute(classAttr));
    }

    if (UNLIKELY(isDefinedCustomElement()))
        CustomElementReactionQueue::enqueueAdoptedCallbackIfNeeded(*this, oldDocument, newDocument);

    if (auto* observerData = intersectionObserverDataIfExists()) {
        for (const auto& observer : observerData->observers) {
            if (observer->hasObservationTargets()) {
                oldDocument.removeIntersectionObserver(*observer);
                newDocument.addIntersectionObserver(*observer);
            }
        }
    }

    if (hasLangAttrKnownToMatchDocumentElement()) {
        oldDocument.removeElementWithLangAttrMatchingDocumentElement(*this);
        setEffectiveLangKnownToMatchDocumentElement(false);
    }

    updateEffectiveLangState();
}

void Element::updateEffectiveLangStateFromParent()
{
    ASSERT(!hasLanguageAttribute());
    ASSERT(parentNode() != &document());

    RefPtr parent = parentOrShadowHostElement();

    if (!parent || parent == document().documentElement()) {
        setEffectiveLangKnownToMatchDocumentElement(parent.get());
        if (hasRareData())
            elementRareData()->setEffectiveLang(nullAtom());
        return;
    }

    setEffectiveLangKnownToMatchDocumentElement(parent->effectiveLangKnownToMatchDocumentElement());
    if (UNLIKELY(parent->hasRareData()) && !parent->elementRareData()->effectiveLang().isNull())
        ensureElementRareData().setEffectiveLang(parent->elementRareData()->effectiveLang());
    else if (hasRareData())
        elementRareData()->setEffectiveLang(nullAtom());
}

void Element::updateEffectiveLangState()
{
    auto& lang = langFromAttribute();
    if (!lang) {
        updateEffectiveLangStateFromParent();
        return;
    }

    if (lang == document().effectiveDocumentElementLanguage()) {
        if (hasRareData())
            elementRareData()->setEffectiveLang(nullAtom());
        document().addElementWithLangAttrMatchingDocumentElement(*this);
        setEffectiveLangKnownToMatchDocumentElement(true);
        return;
    }

    if (hasLangAttrKnownToMatchDocumentElement()) {
        // Unable to protect the document as it may have started destruction.
        document().removeElementWithLangAttrMatchingDocumentElement(*this);
    }

    setEffectiveLangKnownToMatchDocumentElement(false);
    ensureElementRareData().setEffectiveLang(lang);
}

bool Element::hasAttributes() const
{
    synchronizeAllAttributes();
    return elementData() && elementData()->length();
}

bool Element::hasEquivalentAttributes(const Element& other) const
{
    synchronizeAllAttributes();
    other.synchronizeAllAttributes();
    if (elementData() == other.elementData())
        return true;
    if (elementData())
        return elementData()->isEquivalent(other.elementData());
    if (other.elementData())
        return other.elementData()->isEquivalent(elementData());
    return true;
}

String Element::nodeName() const
{
    return m_tagName.toString();
}

ExceptionOr<void> Element::setPrefix(const AtomString& prefix)
{
    auto result = checkSetPrefix(prefix);
    if (result.hasException())
        return result.releaseException();

    m_tagName.setPrefix(prefix.isEmpty() ? nullAtom() : prefix);
    return { };
}

const AtomString& Element::imageSourceURL() const
{
    return attributeWithoutSynchronization(srcAttr);
}

bool Element::rendererIsNeeded(const RenderStyle& style)
{
    return style.display() != DisplayType::None && style.display() != DisplayType::Contents;
}

RenderPtr<RenderElement> Element::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return RenderElement::createFor(*this, WTFMove(style));
}

Node::InsertedIntoAncestorResult Element::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    ContainerNode::insertedIntoAncestor(insertionType, parentOfInsertedTree);

    if (insertionType.treeScopeChanged) {
        RefPtr<HTMLDocument> newHTMLDocument = insertionType.connectedToDocument && parentOfInsertedTree.isInDocumentTree()
            ? dynamicDowncast<HTMLDocument>(treeScope().documentScope()) : nullptr;
        if (auto& idValue = getIdAttribute(); !idValue.isEmpty()) {
            treeScope().addElementById(idValue, *this);
            if (newHTMLDocument)
                updateIdForDocument(*newHTMLDocument, nullAtom(), idValue, HTMLDocumentNamedItemMapsUpdatingCondition::Always);
        }
        if (auto& nameValue = getNameAttribute(); !nameValue.isEmpty()) {
            treeScope().addElementByName(nameValue, *this);
            if (newHTMLDocument)
                updateNameForDocument(*newHTMLDocument, nullAtom(), nameValue);
        }
    }

    if (insertionType.connectedToDocument) {
        if (UNLIKELY(isCustomElementUpgradeCandidate())) {
            ASSERT(isConnected());
            CustomElementReactionQueue::tryToUpgradeElement(*this);
        }
        if (UNLIKELY(isDefinedCustomElement()))
            CustomElementReactionQueue::enqueueConnectedCallbackIfNeeded(*this);
        if (shouldAutofocus(*this))
            Ref { document().topDocument() }->appendAutofocusCandidate(*this);
    }

    if (parentNode() == &parentOfInsertedTree) {
        if (RefPtr shadowRoot = parentNode()->shadowRoot())
            shadowRoot->hostChildElementDidChange(*this);
    }

    if (parentNode() == &parentOfInsertedTree && is<Document>(*parentNode())) {
        clearEffectiveLangStateOnNewDocumentElement();
        protectedDocument()->setDocumentElementLanguage(langFromAttribute());
    } else if (!hasLanguageAttribute())
        updateEffectiveLangStateFromParent();

    return InsertedIntoAncestorResult::Done;
}

void Element::clearEffectiveLangStateOnNewDocumentElement()
{
    ASSERT(parentNode() == &document());

    if (hasLangAttrKnownToMatchDocumentElement()) {
        protectedDocument()->removeElementWithLangAttrMatchingDocumentElement(*this);
        setEffectiveLangKnownToMatchDocumentElement(false);
    }

    if (hasRareData())
        elementRareData()->setEffectiveLang(nullAtom());
}

void Element::setEffectiveLangStateOnOldDocumentElement()
{
    if (auto& lang = langFromAttribute(); !lang.isNull() || hasRareData())
        ensureElementRareData().setEffectiveLang(lang);
}

bool Element::hasEffectiveLangState() const
{
    return effectiveLangKnownToMatchDocumentElement() || (UNLIKELY(hasRareData()) && !elementRareData()->effectiveLang().isNull());
}

void Element::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    ContainerNode::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (RefPtr page = document().page()) {
#if ENABLE(POINTER_LOCK)
        page->pointerLockController().elementWasRemoved(*this);
#endif
        page->pointerCaptureController().elementWasRemoved(*this);
#if ENABLE(WHEEL_EVENT_LATCHING)
        if (auto* scrollLatchingController = page->scrollLatchingControllerIfExists())
            scrollLatchingController->removeLatchingStateForTarget(*this);
#endif
    }

    if (removalType.treeScopeChanged) {
        auto& oldTreeScope = oldParentOfRemovedTree.treeScope();
        RefPtrAllowingPartiallyDestroyed<HTMLDocument> oldHTMLDocument = removalType.disconnectedFromDocument
            && oldParentOfRemovedTree.isInDocumentTree() ? dynamicDowncast<HTMLDocument>(oldTreeScope.documentScope()) : nullptr;

        if (auto& idValue = getIdAttribute(); !idValue.isEmpty()) {
            oldTreeScope.removeElementById(idValue, *this);
            if (oldHTMLDocument)
                updateIdForDocument(*oldHTMLDocument, idValue, nullAtom(), HTMLDocumentNamedItemMapsUpdatingCondition::Always);
        }
        if (auto& nameValue = getNameAttribute(); !nameValue.isEmpty()) {
            oldTreeScope.removeElementByName(nameValue, *this);
            if (oldHTMLDocument)
                updateNameForDocument(*oldHTMLDocument, nameValue, nullAtom());
        }
    }

    if (removalType.disconnectedFromDocument) {
        RefAllowingPartiallyDestroyed<Document> oldDocument = oldParentOfRemovedTree.treeScope().documentScope();
        ASSERT(&document() == oldDocument.ptr());

        if (lastRememberedLogicalWidth() || lastRememberedLogicalHeight()) {
            // The disconnected element could be unobserved because of other properties, here we need to make sure it is observed,
            // so that deliver could be triggered and it would clear lastRememberedSize.
            oldDocument->observeForContainIntrinsicSize(*this);
            oldDocument->resetObservationSizeForContainIntrinsicSize(*this);
        }

        setSavedLayerScrollPosition({ });

        clearBeforePseudoElement();
        clearAfterPseudoElement();

#if ENABLE(FULLSCREEN_API)
        if (UNLIKELY(hasFullscreenFlag()))
            oldDocument->fullscreenManager().exitRemovedFullscreenElement(*this);
#endif

        if (UNLIKELY(isInTopLayer()))
            removeFromTopLayer();

        if (oldDocument->cssTarget() == this)
            oldDocument->setCSSTarget(nullptr);

        if (UNLIKELY(isDefinedCustomElement()))
            CustomElementReactionQueue::enqueueDisconnectedCallbackIfNeeded(*this);
    }

    if (!parentNode()) {
        if (RefPtr shadowRoot = oldParentOfRemovedTree.shadowRoot())
            shadowRoot->hostChildElementDidChange(*this);
    }

    if (!parentNode() && is<Document>(oldParentOfRemovedTree)) {
        setEffectiveLangStateOnOldDocumentElement();
        document().setDocumentElementLanguage(nullAtom());
    } else if (!hasLanguageAttribute())
        updateEffectiveLangStateFromParent();

    Styleable::fromElement(*this).elementWasRemoved();

    document().userActionElements().clearAllForElement(*this);
}

PopoverData* Element::popoverData() const
{
    return hasRareData() ? elementRareData()->popoverData() : nullptr;
}

PopoverData& Element::ensurePopoverData()
{
    ElementRareData& data = ensureElementRareData();
    if (!data.popoverData())
        data.setPopoverData(makeUnique<PopoverData>());
    return *data.popoverData();
}

void Element::clearPopoverData()
{
    if (hasRareData())
        elementRareData()->setPopoverData(nullptr);
}

void Element::addShadowRoot(Ref<ShadowRoot>&& newShadowRoot)
{
    ASSERT(!newShadowRoot->hasChildNodes());
    ASSERT(!shadowRoot());

    Ref shadowRoot = newShadowRoot;
    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        if (renderer() || hasDisplayContents())
            RenderTreeUpdater::tearDownRenderers(*this);

        ensureElementRareData().setShadowRoot(WTFMove(newShadowRoot));

        shadowRoot->setHost(*this);
        shadowRoot->setParentTreeScope(treeScope());

        NodeVector postInsertionNotificationTargets;
        notifyChildNodeInserted(*this, shadowRoot, postInsertionNotificationTargets);
        ASSERT_UNUSED(postInsertionNotificationTargets, postInsertionNotificationTargets.isEmpty());

        InspectorInstrumentation::didPushShadowRoot(*this, shadowRoot);

        invalidateStyleAndRenderersForSubtree();
    }

    if (shadowRoot->mode() == ShadowRootMode::UserAgent)
        didAddUserAgentShadowRoot(shadowRoot);
}

void Element::removeShadowRootSlow(ShadowRoot& oldRoot)
{
    ASSERT(&oldRoot == shadowRoot());

    InspectorInstrumentation::willPopShadowRoot(*this, oldRoot);
    document().adjustFocusedNodeOnNodeRemoval(oldRoot);

    ASSERT(!oldRoot.renderer());

    elementRareData()->clearShadowRoot();

    oldRoot.setHost(nullptr);
    oldRoot.setParentTreeScope(document());
}

static bool canAttachAuthorShadowRoot(const Element& element)
{
    using namespace ElementNames;

    if (!is<HTMLElement>(element))
        return false;

    switch (element.elementName()) {
    case HTML::article:
    case HTML::aside:
    case HTML::blockquote:
    case HTML::body:
    case HTML::div:
    case HTML::footer:
    case HTML::h1:
    case HTML::h2:
    case HTML::h3:
    case HTML::h4:
    case HTML::h5:
    case HTML::h6:
    case HTML::header:
    case HTML::main:
    case HTML::nav:
    case HTML::p:
    case HTML::section:
    case HTML::span:
        return true;
    default:
        break;
    }

    if (auto localName = element.localName(); Document::validateCustomElementName(localName) == CustomElementNameValidationStatus::Valid) {
        if (RefPtr window = element.document().domWindow()) {
            RefPtr registry = window->customElementRegistry();
            if (registry && registry->isShadowDisabled(localName))
                return false;
        }

        return true;
    }

    return false;
}

ExceptionOr<ShadowRoot&> Element::attachShadow(const ShadowRootInit& init)
{
    if (init.mode == ShadowRootMode::UserAgent)
        return Exception { ExceptionCode::TypeError };
    if (!canAttachAuthorShadowRoot(*this))
        return Exception { ExceptionCode::NotSupportedError };
    if (RefPtr shadowRoot = this->shadowRoot()) {
        if (shadowRoot->isDeclarativeShadowRoot()) {
            if (init.mode != shadowRoot->mode())
                return Exception { ExceptionCode::NotSupportedError };
            ChildListMutationScope mutation(*shadowRoot);
            shadowRoot->removeChildren();
            shadowRoot->setIsDeclarativeShadowRoot(false);
            return *shadowRoot;
        }
        return Exception { ExceptionCode::NotSupportedError };
    }
    Ref shadow = ShadowRoot::create(document(), init.mode, init.slotAssignment,
        init.delegatesFocus ? ShadowRoot::DelegatesFocus::Yes : ShadowRoot::DelegatesFocus::No,
        init.clonable ? ShadowRoot::Clonable::Yes : ShadowRoot::Clonable::No,
        init.serializable ? ShadowRoot::Serializable::Yes : ShadowRoot::Serializable::No,
        isPrecustomizedOrDefinedCustomElement() ? ShadowRoot::AvailableToElementInternals::Yes : ShadowRoot::AvailableToElementInternals::No);
    addShadowRoot(shadow.copyRef());
    return shadow.get();
}

ExceptionOr<ShadowRoot&> Element::attachDeclarativeShadow(ShadowRootMode mode, ShadowRootDelegatesFocus delegatesFocus, ShadowRootClonable clonable, ShadowRootSerializable serializable)
{
    if (this->shadowRoot())
        return Exception { ExceptionCode::NotSupportedError };
    auto exceptionOrShadowRoot = attachShadow({
        mode,
        delegatesFocus == ShadowRootDelegatesFocus::Yes,
        clonable == ShadowRootClonable::Yes,
        serializable == ShadowRootSerializable::Yes
    });
    if (exceptionOrShadowRoot.hasException())
        return exceptionOrShadowRoot.releaseException();
    Ref shadowRoot = exceptionOrShadowRoot.releaseReturnValue();
    shadowRoot->setIsDeclarativeShadowRoot(true);
    shadowRoot->setIsAvailableToElementInternals(true);
    return shadowRoot.get();
}

RefPtr<ShadowRoot> Element::shadowRootForBindings(JSC::JSGlobalObject& lexicalGlobalObject) const
{
    RefPtr shadow = shadowRoot();
    if (!shadow)
        return nullptr;
    if (shadow->mode() == ShadowRootMode::Open)
        return shadow;
    if (JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject)->world().shadowRootIsAlwaysOpen())
        return shadow;
    return nullptr;
}

ShadowRoot* Element::userAgentShadowRoot() const
{
    ASSERT(!shadowRoot() || shadowRoot()->mode() == ShadowRootMode::UserAgent);
    return shadowRoot();
}

RefPtr<ShadowRoot> Element::protectedUserAgentShadowRoot() const
{
    return userAgentShadowRoot();
}

ShadowRoot& Element::ensureUserAgentShadowRoot()
{
    if (RefPtr shadow = userAgentShadowRoot())
        return *shadow;
    return createUserAgentShadowRoot();
}

ShadowRoot& Element::createUserAgentShadowRoot()
{
    ASSERT(!userAgentShadowRoot());
    Ref newShadow = ShadowRoot::create(document(), ShadowRootMode::UserAgent);
    ShadowRoot& shadow = newShadow;
    addShadowRoot(WTFMove(newShadow));
    return shadow;
}

inline void Node::setCustomElementState(CustomElementState state)
{
    Style::PseudoClassChangeInvalidation styleInvalidation(downcast<Element>(*this),
        CSSSelector::PseudoClass::Defined,
        state == CustomElementState::Custom || state == CustomElementState::Uncustomized
    );
    auto bitfields = rareDataBitfields();
    bitfields.customElementState = enumToUnderlyingType(state);
    setRareDataBitfields(bitfields);
}

void Element::setIsDefinedCustomElement(JSCustomElementInterface& elementInterface)
{
    setCustomElementState(CustomElementState::Custom);
    auto& data = ensureElementRareData();
    if (!data.customElementReactionQueue())
        data.setCustomElementReactionQueue(makeUnique<CustomElementReactionQueue>(elementInterface));
    InspectorInstrumentation::didChangeCustomElementState(*this);
}

void Element::setIsFailedCustomElement()
{
    ASSERT(isUnknownElement());
    setIsFailedOrPrecustomizedCustomElementWithoutClearingReactionQueue();
    clearReactionQueueFromFailedCustomElement();
    ASSERT(isFailedCustomElement());
}

void Element::setIsFailedOrPrecustomizedCustomElementWithoutClearingReactionQueue()
{
    ASSERT(customElementState() == CustomElementState::Undefined);
    setCustomElementState(CustomElementState::FailedOrPrecustomized);
    InspectorInstrumentation::didChangeCustomElementState(*this);
}

void Element::clearReactionQueueFromFailedCustomElement()
{
    ASSERT(isFailedOrPrecustomizedCustomElement());
    if (hasRareData()) {
        // Clear the queue instead of deleting it since this function can be called inside CustomElementReactionQueue::invokeAll during upgrades.
        if (auto* queue = elementRareData()->customElementReactionQueue())
            queue->clear();
    }
}

void Element::setIsCustomElementUpgradeCandidate()
{
    ASSERT(customElementState() == CustomElementState::Uncustomized);
    setCustomElementState(CustomElementState::Undefined);
    InspectorInstrumentation::didChangeCustomElementState(*this);
}

void Element::enqueueToUpgrade(JSCustomElementInterface& elementInterface)
{
    ASSERT(isCustomElementUpgradeCandidate());
    auto& data = ensureElementRareData();
    bool alreadyScheduledToUpgrade = data.customElementReactionQueue();
    if (!alreadyScheduledToUpgrade)
        data.setCustomElementReactionQueue(makeUnique<CustomElementReactionQueue>(elementInterface));
    data.customElementReactionQueue()->enqueueElementUpgrade(*this, alreadyScheduledToUpgrade);
}

CustomElementReactionQueue* Element::reactionQueue() const
{
#if ASSERT_ENABLED
    if (isFailedOrPrecustomizedCustomElement()) {
        auto* queue = elementRareData()->customElementReactionQueue();
        ASSERT(queue);
    }
#endif
    if (!hasRareData())
        return nullptr;
    return elementRareData()->customElementReactionQueue();
}

CustomElementDefaultARIA& Element::customElementDefaultARIA()
{
    ASSERT(isPrecustomizedOrDefinedCustomElement());
    auto* deafultARIA = elementRareData()->customElementDefaultARIA();
    if (!deafultARIA) {
        elementRareData()->setCustomElementDefaultARIA(makeUnique<CustomElementDefaultARIA>());
        deafultARIA = elementRareData()->customElementDefaultARIA();
    }
    return *deafultARIA;
}

CheckedRef<CustomElementDefaultARIA> Element::checkedCustomElementDefaultARIA()
{
    return customElementDefaultARIA();
}

CustomElementDefaultARIA* Element::customElementDefaultARIAIfExists() const
{
    return isPrecustomizedOrDefinedCustomElement() && hasRareData() ? elementRareData()->customElementDefaultARIA() : nullptr;
}

bool Element::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ELEMENT_NODE:
    case TEXT_NODE:
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case CDATA_SECTION_NODE:
        return true;
    default:
        break;
    }
    return false;
}
void Element::childrenChanged(const ChildChange& change)
{
    ContainerNode::childrenChanged(change);

    if (RefPtr shadowRoot = this->shadowRoot()) {
        switch (change.type) {
        case ChildChange::Type::ElementInserted:
        case ChildChange::Type::ElementRemoved:
            // For elements, we notify shadowRoot in Element::insertedIntoAncestor and Element::removedFromAncestor.
            break;
        case ChildChange::Type::AllChildrenRemoved:
        case ChildChange::Type::AllChildrenReplaced:
            shadowRoot->didRemoveAllChildrenOfShadowHost();
            break;
        case ChildChange::Type::TextInserted:
        case ChildChange::Type::TextRemoved:
        case ChildChange::Type::TextChanged:
            shadowRoot->didMutateTextNodesOfShadowHost();
            break;
        case ChildChange::Type::NonContentsChildInserted:
        case ChildChange::Type::NonContentsChildRemoved:
            break;
        }
    }
}

void Element::setAttributeEventListener(const AtomString& eventType, const QualifiedName& attributeName, const AtomString& attributeValue)
{
    setAttributeEventListener(eventType, JSLazyEventListener::create(*this, attributeName, attributeValue), mainThreadNormalWorld());
}

void Element::removeAllEventListeners()
{
    ContainerNode::removeAllEventListeners();
    if (RefPtr shadowRoot = this->shadowRoot())
        shadowRoot->removeAllEventListeners();
}

void Element::finishParsingChildren()
{
    if (hasHeldBackChildrenChanged())
        parserNotifyChildrenChanged();

    setIsParsingChildrenFinished();

    Style::ChildChangeInvalidation::invalidateAfterFinishedParsingChildren(*this);
}

static void appendAttributes(StringBuilder& builder, const Element& element)
{
    if (element.hasID())
        builder.append(" id=\'", element.getIdAttribute(), '\'');

    if (element.hasClass()) {
        builder.append(" class=\'");
        size_t classNamesToDump = element.classNames().size();
        constexpr size_t maxNumClassNames = 7;
        bool addEllipsis = false;
        if (classNamesToDump > maxNumClassNames) {
            classNamesToDump = maxNumClassNames;
            addEllipsis = true;
        }
        
        for (size_t i = 0; i < classNamesToDump; ++i) {
            if (i > 0)
                builder.append(' ');
            builder.append(element.classNames()[i]);
        }
        if (addEllipsis)
            builder.append(" ...");
        builder.append('\'');
    }
}

String Element::description() const
{
    StringBuilder builder;

    builder.append(ContainerNode::description());
    appendAttributes(builder, *this);

    return builder.toString();
}

String Element::debugDescription() const
{
    StringBuilder builder;

    builder.append(ContainerNode::debugDescription());
    appendAttributes(builder, *this);

    return builder.toString();
}

const Vector<RefPtr<Attr>>& Element::attrNodeList()
{
    ASSERT(hasSyntheticAttrChildNodes());
    return *attrNodeListForElement(*this);
}

void Element::attachAttributeNodeIfNeeded(Attr& attrNode)
{
    ASSERT(!attrNode.ownerElement() || attrNode.ownerElement() == this);
    if (attrNode.ownerElement() == this)
        return;

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    attrNode.attachToElement(*this);
    ensureAttrNodeListForElement(*this).append(&attrNode);
}

ExceptionOr<RefPtr<Attr>> Element::setAttributeNode(Attr& attrNode)
{
    RefPtr oldAttrNode = attrIfExists(attrNode.qualifiedName());
    if (oldAttrNode.get() == &attrNode)
        return oldAttrNode;

    // InUseAttributeError: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode.ownerElement() && attrNode.ownerElement() != this)
        return Exception { ExceptionCode::InUseAttributeError };

    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        synchronizeAllAttributes();
    }

    auto& elementData = ensureUniqueElementData();

    auto existingAttributeIndex = elementData.findAttributeIndexByName(attrNode.qualifiedName());

    // Attr::value() will return its 'm_standaloneValue' member any time its Element is set to nullptr. We need to cache this value
    // before making changes to attrNode's Element connections.
    auto attrNodeValue = attrNode.value();

    if (existingAttributeIndex == ElementData::attributeNotFound) {
        attachAttributeNodeIfNeeded(attrNode);
        setAttributeInternal(elementData.findAttributeIndexByName(attrNode.qualifiedName()), attrNode.qualifiedName(), attrNodeValue, InSynchronizationOfLazyAttribute::No);
    } else {
        const Attribute& attribute = attributeAt(existingAttributeIndex);
        if (oldAttrNode)
            detachAttrNodeFromElementWithValue(oldAttrNode.get(), attribute.value());
        else
            oldAttrNode = Attr::create(protectedDocument(), attrNode.qualifiedName(), attribute.value());

        attachAttributeNodeIfNeeded(attrNode);

        if (attribute.name().matches(attrNode.qualifiedName()))
            setAttributeInternal(existingAttributeIndex, attrNode.qualifiedName(), attrNodeValue, InSynchronizationOfLazyAttribute::No);
        else {
            removeAttributeInternal(existingAttributeIndex, InSynchronizationOfLazyAttribute::No);
            setAttributeInternal(ensureUniqueElementData().findAttributeIndexByName(attrNode.qualifiedName()), attrNode.qualifiedName(), attrNodeValue, InSynchronizationOfLazyAttribute::No);
        }
    }

    return oldAttrNode;
}

ExceptionOr<RefPtr<Attr>> Element::setAttributeNodeNS(Attr& attrNode)
{
    RefPtr oldAttrNode = attrIfExists(attrNode.qualifiedName());
    if (oldAttrNode == &attrNode)
        return oldAttrNode;

    // InUseAttributeError: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode.ownerElement() && attrNode.ownerElement() != this)
        return Exception { ExceptionCode::InUseAttributeError };

    // Attr::value() will return its 'm_standaloneValue' member any time its Element is set to nullptr. We need to cache this value
    // before making changes to attrNode's Element connections.
    auto attrNodeValue = attrNode.value();
    unsigned index = 0;
    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        synchronizeAllAttributes();
        auto& elementData = ensureUniqueElementData();

        index = elementData.findAttributeIndexByName(attrNode.qualifiedName());

        if (index != ElementData::attributeNotFound) {
            if (oldAttrNode)
                detachAttrNodeFromElementWithValue(oldAttrNode.get(), elementData.attributeAt(index).value());
            else
                oldAttrNode = Attr::create(protectedDocument(), attrNode.qualifiedName(), elementData.attributeAt(index).value());
        }
    }

    attachAttributeNodeIfNeeded(attrNode);
    setAttributeInternal(index, attrNode.qualifiedName(), attrNodeValue, InSynchronizationOfLazyAttribute::No);

    return oldAttrNode;
}

ExceptionOr<Ref<Attr>> Element::removeAttributeNode(Attr& attr)
{
    if (attr.ownerElement() != this)
        return Exception { ExceptionCode::NotFoundError };

    ASSERT(&document() == &attr.document());

    synchronizeAllAttributes();

    if (!m_elementData)
        return Exception { ExceptionCode::NotFoundError };

    auto existingAttributeIndex = m_elementData->findAttributeIndexByName(attr.qualifiedName());
    if (existingAttributeIndex == ElementData::attributeNotFound)
        return Exception { ExceptionCode::NotFoundError };

    Ref oldAttrNode { attr };

    detachAttrNodeFromElementWithValue(oldAttrNode.ptr(), m_elementData->attributeAt(existingAttributeIndex).value());
    removeAttributeInternal(existingAttributeIndex, InSynchronizationOfLazyAttribute::No);

    return oldAttrNode;
}

ExceptionOr<QualifiedName> Element::parseAttributeName(const AtomString& namespaceURI, const AtomString& qualifiedName)
{
    auto parseResult = Document::parseQualifiedName(namespaceURI, qualifiedName);
    if (parseResult.hasException())
        return parseResult.releaseException();
    QualifiedName parsedAttributeName { parseResult.releaseReturnValue() };
    if (!Document::hasValidNamespaceForAttributes(parsedAttributeName))
        return Exception { ExceptionCode::NamespaceError };
    return parsedAttributeName;
}

ExceptionOr<void> Element::setAttributeNS(const AtomString& namespaceURI, const AtomString& qualifiedName, const AtomString& value)
{
    auto result = parseAttributeName(namespaceURI, qualifiedName);
    if (result.hasException())
        return result.releaseException();
    setAttribute(result.releaseReturnValue(), value);
    return { };
}

void Element::removeAttributeInternal(unsigned index, InSynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < attributeCount());

    UniqueElementData& elementData = ensureUniqueElementData();

    QualifiedName name = elementData.attributeAt(index).name();
    AtomString valueBeingRemoved = elementData.attributeAt(index).value();

    if (RefPtr attrNode = attrIfExists(name))
        detachAttrNodeFromElementWithValue(attrNode.get(), elementData.attributeAt(index).value());

    if (inSynchronizationOfLazyAttribute == InSynchronizationOfLazyAttribute::Yes) {
        elementData.removeAttribute(index);
        return;
    }

    ASSERT(!valueBeingRemoved.isNull());
    willModifyAttribute(name, valueBeingRemoved, nullAtom());
    {
        Style::AttributeChangeInvalidation styleInvalidation(*this, name, valueBeingRemoved, nullAtom());
        elementData.removeAttribute(index);
    }

    didRemoveAttribute(name, valueBeingRemoved);
}

void Element::addAttributeInternal(const QualifiedName& name, const AtomString& value, InSynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (inSynchronizationOfLazyAttribute == InSynchronizationOfLazyAttribute::Yes) {
        ensureUniqueElementData().addAttribute(name, value);
        return;
    }

    willModifyAttribute(name, nullAtom(), value);
    {
        Style::AttributeChangeInvalidation styleInvalidation(*this, name, nullAtom(), value);
        ensureUniqueElementData().addAttribute(name, value);
    }
    didAddAttribute(name, value);
}

bool Element::removeAttribute(const AtomString& qualifiedName)
{
    if (!elementData())
        return false;

    AtomString caseAdjustedQualifiedName = shouldIgnoreAttributeCase(*this) ? qualifiedName.convertToASCIILowercase() : qualifiedName;
    unsigned index = elementData()->findAttributeIndexByName(caseAdjustedQualifiedName, false);
    if (index == ElementData::attributeNotFound) {
        if (UNLIKELY(caseAdjustedQualifiedName == styleAttr) && elementData()->styleAttributeIsDirty()) {
            if (auto* styledElement = dynamicDowncast<StyledElement>(*this))
                styledElement->removeAllInlineStyleProperties();
        }
        return false;
    }

    removeAttributeInternal(index, InSynchronizationOfLazyAttribute::No);
    return true;
}

bool Element::removeAttributeNS(const AtomString& namespaceURI, const AtomString& localName)
{
    return removeAttribute(QualifiedName(nullAtom(), localName, namespaceURI));
}

RefPtr<Attr> Element::getAttributeNode(const AtomString& qualifiedName)
{
    if (!elementData())
        return nullptr;
    synchronizeAttribute(qualifiedName);
    const Attribute* attribute = elementData()->findAttributeByName(qualifiedName, shouldIgnoreAttributeCase(*this));
    if (!attribute)
        return nullptr;
    return ensureAttr(attribute->name());
}

RefPtr<Attr> Element::getAttributeNodeNS(const AtomString& namespaceURI, const AtomString& localName)
{
    if (!elementData())
        return nullptr;
    QualifiedName qName(nullAtom(), localName, namespaceURI);
    synchronizeAttribute(qName);
    const Attribute* attribute = elementData()->findAttributeByName(qName);
    if (!attribute)
        return nullptr;
    return ensureAttr(attribute->name());
}

bool Element::hasAttribute(const AtomString& qualifiedName) const
{
    if (!elementData())
        return false;
    synchronizeAttribute(qualifiedName);
    return elementData()->findAttributeByName(qualifiedName, shouldIgnoreAttributeCase(*this));
}

bool Element::hasAttributeNS(const AtomString& namespaceURI, const AtomString& localName) const
{
    if (!elementData())
        return false;
    QualifiedName qName(nullAtom(), localName, namespaceURI);
    synchronizeAttribute(qName);
    return elementData()->findAttributeByName(qName);
}

static RefPtr<ShadowRoot> shadowRootWithDelegatesFocus(const Element& element)
{
    if (RefPtr root = element.shadowRoot()) {
        if (root->delegatesFocus())
            return root;
    }
    return nullptr;
}

static bool isProgramaticallyFocusable(Element& element)
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (shadowRootWithDelegatesFocus(element))
        return false;

    // If the stylesheets have already been loaded we can reliably check isFocusable.
    // If not, we continue and set the focused node on the focus controller below so that it can be updated soon after attach.
    if (element.document().haveStylesheetsLoaded()) {
        if (!element.isFocusable())
            return false;
    }
    return element.supportsFocus();
}

// https://html.spec.whatwg.org/multipage/interaction.html#autofocus-delegate
static RefPtr<Element> autoFocusDelegate(ContainerNode& target, FocusTrigger trigger)
{
    if (RefPtr root = target.shadowRoot(); root && !root->delegatesFocus())
        return nullptr;

    for (Ref element : descendantsOfType<Element>(target)) {
        if (!element->hasAttributeWithoutSynchronization(HTMLNames::autofocusAttr))
            continue;
        if (auto root = shadowRootWithDelegatesFocus(element)) {
            if (RefPtr target = Element::findFocusDelegateForTarget(*root, trigger))
                return target;
        }
        switch (trigger) {
        case FocusTrigger::Click:
            if (element->isMouseFocusable())
                return element;
            break;
        case FocusTrigger::Other:
        case FocusTrigger::Bindings:
            if (isProgramaticallyFocusable(element))
                return element;
            break;
        }
    }
    return nullptr;
}

// https://html.spec.whatwg.org/multipage/interaction.html#focus-delegate
RefPtr<Element> Element::findFocusDelegateForTarget(ContainerNode& target, FocusTrigger trigger)
{
    if (RefPtr root = target.shadowRoot(); root && !root->delegatesFocus())
        return nullptr;
    if (RefPtr element = autoFocusDelegate(target, trigger))
        return element;
    for (Ref element : descendantsOfType<Element>(target)) {
        switch (trigger) {
        case FocusTrigger::Click:
            if (element->isMouseFocusable())
                return element;
            break;
        case FocusTrigger::Other:
        case FocusTrigger::Bindings:
            if (isProgramaticallyFocusable(element))
                return element;
            break;
        }
        if (RefPtr root = shadowRootWithDelegatesFocus(element)) {
            if (RefPtr target = findFocusDelegateForTarget(*root, trigger))
                return target;
        }
    }
    return nullptr;
}

// https://html.spec.whatwg.org/multipage/interaction.html#autofocus-delegate
RefPtr<Element> Element::findAutofocusDelegate(FocusTrigger trigger)
{
    return autoFocusDelegate(*this, trigger);
}

// https://html.spec.whatwg.org/multipage/interaction.html#focus-delegate
RefPtr<Element> Element::findFocusDelegate(FocusTrigger trigger)
{
    return findFocusDelegateForTarget(*this, trigger);
}

void Element::focus(const FocusOptions& options)
{
    if (!isConnected())
        return;

    Ref document { this->document() };
    if (document->focusedElement() == this) {
        if (RefPtr page = document->page())
            page->chrome().client().elementDidRefocus(*this, options);
        return;
    }

    RefPtr newTarget { this };

    // If we don't have renderer yet, isFocusable will compute it without style update.
    // FIXME: Expand it to avoid style update in all cases.
    if (renderer() && document->haveStylesheetsLoaded())
        document->updateStyleIfNeeded();

    if (&newTarget->document() != document.ptr())
        return;

    if (RefPtr root = shadowRootWithDelegatesFocus(*this)) {
        RefPtr currentlyFocusedElement = document->focusedElement();
        if (root->containsIncludingShadowDOM(currentlyFocusedElement.get())) {
            if (RefPtr page = document->page())
                page->chrome().client().elementDidRefocus(*currentlyFocusedElement, options);
            return;
        }

        newTarget = findFocusDelegateForTarget(*root, options.trigger);
        if (!newTarget)
            return;
    } else if (!isProgramaticallyFocusable(*newTarget))
        return;

    if (RefPtr page = document->page()) {
        Ref frame = *document->frame();
        if (!frame->hasHadUserInteraction() && !frame->isMainFrame() && !document->topOrigin().isSameOriginDomain(document->securityOrigin()))
            return;

        FocusOptions optionsWithVisibility = options;
        if (options.trigger == FocusTrigger::Bindings && document->wasLastFocusByClick())
            optionsWithVisibility.visibility = FocusVisibility::Invisible;
        else if (options.trigger != FocusTrigger::Click)
            optionsWithVisibility.visibility = FocusVisibility::Visible;

        // Focus and change event handlers can cause us to lose our last ref.
        // If a focus event handler changes the focus to a different node it
        // does not make sense to continue and update appearence.
        if (!page->checkedFocusController()->setFocusedElement(newTarget.get(), frame, optionsWithVisibility))
            return;
    }

    newTarget->findTargetAndUpdateFocusAppearance(options.selectionRestorationMode, options.preventScroll ? SelectionRevealMode::DoNotReveal : SelectionRevealMode::Reveal);
}

void Element::focusForBindings(FocusOptions&& options)
{
    options.trigger = FocusTrigger::Bindings;
    focus(WTFMove(options));
}

void Element::findTargetAndUpdateFocusAppearance(SelectionRestorationMode selectionMode, SelectionRevealMode revealMode)
{
#if PLATFORM(IOS_FAMILY)
    // Focusing a form element triggers animation in UIKit to scroll to the right position.
    // Calling updateFocusAppearance() would generate an unnecessary call to ScrollView::setScrollPosition(),
    // which would jump us around during this animation. See <rdar://problem/6699741>.
    if (revealMode == SelectionRevealMode::Reveal && is<HTMLFormControlElement>(*this))
        revealMode = SelectionRevealMode::RevealUpToMainFrame;
#endif

    RefPtr target = focusAppearanceUpdateTarget();
    if (!target)
        return;

    target->updateFocusAppearance(selectionMode, revealMode);
}

// https://html.spec.whatwg.org/#focus-processing-model
RefPtr<Element> Element::focusAppearanceUpdateTarget()
{
    return this;
}

void Element::updateFocusAppearance(SelectionRestorationMode, SelectionRevealMode revealMode)
{
    if (isRootEditableElement()) {
        // Keep frame alive in this method, since setSelection() may release the last reference to |frame|.
        RefPtr frame { document().frame() };
        if (!frame)
            return;
        
        // When focusing an editable element in an iframe, don't reset the selection if it already contains a selection.
        if (this == frame->selection().selection().rootEditableElement())
            return;

        // FIXME: We should restore the previous selection if there is one.
        VisibleSelection newSelection = VisibleSelection(firstPositionInOrBeforeNode(this));
        
        if (frame->selection().shouldChangeSelection(newSelection)) {
            frame->selection().setSelection(newSelection, FrameSelection::defaultSetSelectionOptions(), Element::defaultFocusTextStateChangeIntent());
            frame->selection().revealSelection(revealMode);
            return;
        }
    }

    if (RefPtr view = document().view())
        view->scheduleScrollToFocusedElement(revealMode);
}

void Element::blur()
{
    if (treeScope().focusedElementInScope() == this) {
        if (RefPtr frame = document().frame())
            frame->page()->checkedFocusController()->setFocusedElement(nullptr, *frame);
        else
            protectedDocument()->setFocusedElement(nullptr);
    }
}

void Element::runFocusingStepsForAutofocus()
{
    focus();
}

void Element::dispatchFocusInEventIfNeeded(RefPtr<Element>&& oldFocusedElement)
{
    Ref document = this->document();
    if (!document->hasListenerType(Document::ListenerType::FocusIn))
        return;
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    dispatchScopedEvent(FocusEvent::create(eventNames().focusinEvent, Event::CanBubble::Yes, Event::IsCancelable::No, document->windowProxy(), 0, WTFMove(oldFocusedElement)));
}

void Element::dispatchFocusOutEventIfNeeded(RefPtr<Element>&& newFocusedElement)
{
    Ref document = this->document();
    if (!document->hasListenerType(Document::ListenerType::FocusOut))
        return;
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    dispatchScopedEvent(FocusEvent::create(eventNames().focusoutEvent, Event::CanBubble::Yes, Event::IsCancelable::No, document->windowProxy(), 0, WTFMove(newFocusedElement)));
}

void Element::dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, const FocusOptions& options)
{
    if (RefPtr page = document().page())
        page->chrome().client().elementDidFocus(*this, options);
    dispatchEvent(FocusEvent::create(eventNames().focusEvent, Event::CanBubble::No, Event::IsCancelable::No, document().windowProxy(), 0, WTFMove(oldFocusedElement)));
}

void Element::dispatchBlurEvent(RefPtr<Element>&& newFocusedElement)
{
    if (RefPtr page = document().page())
        page->chrome().client().elementDidBlur(*this);
    dispatchEvent(FocusEvent::create(eventNames().blurEvent, Event::CanBubble::No, Event::IsCancelable::No, document().windowProxy(), 0, WTFMove(newFocusedElement)));
}

void Element::dispatchWebKitImageReadyEventForTesting()
{
    if (document().settings().webkitImageReadyEventEnabled())
        dispatchEvent(Event::create("webkitImageFrameReady"_s, Event::CanBubble::Yes, Event::IsCancelable::Yes));
}

bool Element::dispatchMouseForceWillBegin()
{
    if (!document().hasListenerType(Document::ListenerType::ForceWillBegin))
        return false;

    RefPtr frame = document().frame();
    if (!frame)
        return false;

    PlatformMouseEvent platformMouseEvent { frame->eventHandler().lastKnownMousePosition(), frame->eventHandler().lastKnownMouseGlobalPosition(), MouseButton::None, PlatformEvent::Type::NoType, 1, { }, WallTime::now(), ForceAtClick, SyntheticClickType::NoTap };
    auto mouseForceWillBeginEvent = MouseEvent::create(eventNames().webkitmouseforcewillbeginEvent, document().windowProxy(), platformMouseEvent, 0, nullptr);
    mouseForceWillBeginEvent->setTarget(Ref { *this });
    dispatchEvent(mouseForceWillBeginEvent);

    if (mouseForceWillBeginEvent->defaultHandled() || mouseForceWillBeginEvent->defaultPrevented())
        return true;

    return false;
}

void Element::enqueueSecurityPolicyViolationEvent(SecurityPolicyViolationEventInit&& eventInit)
{
    document().eventLoop().queueTask(TaskSource::DOMManipulation, [this, protectedThis = Ref { *this }, event = SecurityPolicyViolationEvent::create(eventNames().securitypolicyviolationEvent, WTFMove(eventInit), Event::IsTrusted::Yes)] {
        if (!isConnected())
            protectedDocument()->dispatchEvent(event);
        else
            dispatchEvent(event);
    });
}

ExceptionOr<void> Element::replaceChildrenWithMarkup(const String& markup, OptionSet<ParserContentPolicy> parserContentPolicy)
{
    auto policy = OptionSet<ParserContentPolicy> { ParserContentPolicy::AllowScriptingContent } | parserContentPolicy;

    Ref container = [this]() -> Ref<ContainerNode> {
        if (auto* templateElement = dynamicDowncast<HTMLTemplateElement>(*this))
            return templateElement->content();
        return *this;
    }();

    // Parsing empty string creates additional elements only inside <html> container
    // https://html.spec.whatwg.org/multipage/parsing.html#parsing-main-inhtml
    if (markup.isEmpty() && !is<HTMLHtmlElement>(container)) {
        ChildListMutationScope mutation(container);
        container->removeChildren();
        return { };
    }

    auto fragment = createFragmentForInnerOuterHTML(*this, markup, policy);
    if (fragment.hasException())
        return fragment.releaseException();

    return replaceChildrenWithFragment(container, fragment.releaseReturnValue());
}

ExceptionOr<void> Element::setHTMLUnsafe(const String& html)
{
    return replaceChildrenWithMarkup(html, { ParserContentPolicy::AllowDeclarativeShadowRoots, ParserContentPolicy::AlwaysParseAsHTML });
}

String Element::getHTML(GetHTMLOptions&& options) const
{
    return serializeFragment(*this, SerializedNodes::SubtreesOfChildren, nullptr, ResolveURLs::NoExcludingURLsForPrivacy, SerializationSyntax::HTML, { }, { }, options.serializableShadowRoots ? SerializeShadowRoots::Serializable : SerializeShadowRoots::Explicit, WTFMove(options.shadowRoots));
}

ExceptionOr<void> Element::mergeWithNextTextNode(Text& node)
{
    RefPtr textNext = dynamicDowncast<Text>(node.nextSibling());
    if (!textNext)
        return { };
    node.appendData(textNext->data());
    return textNext->remove();
}

String Element::innerHTML() const
{
    return serializeFragment(*this, SerializedNodes::SubtreesOfChildren, nullptr, ResolveURLs::NoExcludingURLsForPrivacy);
}

String Element::outerHTML() const
{
    return serializeFragment(*this, SerializedNodes::SubtreeIncludingNode, nullptr, ResolveURLs::NoExcludingURLsForPrivacy);
}

ExceptionOr<void> Element::setOuterHTML(const String& html)
{
    // The specification allows setting outerHTML on an Element whose parent is a DocumentFragment and Gecko supports this.
    // https://w3c.github.io/DOM-Parsing/#dom-element-outerhtml
    RefPtr parent = parentElement();
    if (UNLIKELY(!parent)) {
        if (!parentNode())
            return { };
        return Exception { ExceptionCode::NoModificationAllowedError, "Cannot set outerHTML on element because its parent is not an Element"_s };
    }

    RefPtr previous = previousSibling();
    RefPtr next = nextSibling();

    auto fragment = createFragmentForInnerOuterHTML(*parent, html, { ParserContentPolicy::AllowScriptingContent });
    if (fragment.hasException())
        return fragment.releaseException();

    auto replaceResult = parent->replaceChild(fragment.releaseReturnValue().get(), *this);
    if (replaceResult.hasException())
        return replaceResult.releaseException();

    // The following is not part of the specification but matches Blink's behavior as of June 2021.
    if (RefPtr nodeText = next ? dynamicDowncast<Text>(next->previousSibling()) : nullptr) {
        auto result = mergeWithNextTextNode(*nodeText);
        if (result.hasException())
            return result.releaseException();
    }
    if (RefPtr previousText = dynamicDowncast<Text>(WTFMove(previous))) {
        auto result = mergeWithNextTextNode(*previousText);
        if (result.hasException())
            return result.releaseException();
    }
    return { };
}

ExceptionOr<void> Element::setInnerHTML(const String& markup)
{
    return replaceChildrenWithMarkup(markup, { });
}

String Element::innerText()
{
    // We need to update layout, since plainText uses line boxes in the render tree.
    protectedDocument()->updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return textContent(true);

    if (renderer()->isSkippedContent())
        return String();

    return plainText(makeRangeSelectingNodeContents(*this));
}

String Element::outerText()
{
    // Getting outerText is the same as getting innerText, only
    // setting is different. You would think this should get the plain
    // text for the outer range, but this is wrong, <br> for instance
    // would return different values for inner and outer text by such
    // a rule, but it doesn't in WinIE, and we want to match that.
    return innerText();
}

String Element::title() const
{
    return String();
}

const AtomString& Element::userAgentPart() const
{
    ASSERT(isInUserAgentShadowTree());
    return attributeWithoutSynchronization(useragentpartAttr);
}

void Element::setUserAgentPart(const AtomString& value)
{
    // We may set the useragentpart attribute on elements before appending them to the shadow tree.
    ASSERT(!isConnected() || isInUserAgentShadowTree());
    setAttributeWithoutSynchronization(useragentpartAttr, value);
}

void Element::willBecomeFullscreenElement()
{
    for (Ref child : descendantsOfType<Element>(*this))
        child->ancestorWillEnterFullscreen();
}

static void forEachRenderLayer(Element& element, const std::function<void(RenderLayer&)>& function)
{
    CheckedPtr layerModelObject = dynamicDowncast<RenderLayerModelObject>(element.renderer());
    if (!layerModelObject)
        return;


    CheckedPtr renderBoxModelObject = dynamicDowncast<RenderBoxModelObject>(*layerModelObject);
    if (!renderBoxModelObject) {
        if (layerModelObject->hasLayer())
            function(*layerModelObject->layer());
        return;
    }

    RenderBoxModelObject::forRendererAndContinuations(*renderBoxModelObject, [function](RenderBoxModelObject& renderer) {
        if (renderer.hasLayer())
            function(*renderer.layer());
    });
}

void Element::addToTopLayer()
{
    RELEASE_ASSERT(!isInTopLayer());
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    forEachRenderLayer(*this, [](RenderLayer& layer) {
        layer.establishesTopLayerWillChange();
    });

    Ref document = this->document();
    document->addTopLayerElement(*this);
    setEventTargetFlag(EventTargetFlag::IsInTopLayer);

    document->scheduleContentRelevancyUpdate(ContentRelevancy::IsInTopLayer);

    // Invalidate inert state
    invalidateStyleInternal();
    if (RefPtr documentElement = document->documentElement())
        documentElement->invalidateStyleInternal();

    forEachRenderLayer(*this, [](RenderLayer& layer) {
        layer.establishesTopLayerDidChange();
    });
}

void Element::removeFromTopLayer()
{
    RELEASE_ASSERT(isInTopLayer());
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    forEachRenderLayer(*this, [](RenderLayer& layer) {
        layer.establishesTopLayerWillChange();
    });

    // We need to call Styleable::fromRenderer() while this element is still contained in
    // Document::topLayerElements(), since Styleable::fromRenderer() relies on this to
    // find the backdrop's associated element.
    if (CheckedPtr renderer = this->renderer()) {
        if (CheckedPtr backdrop = renderer->backdropRenderer().get()) {
            if (auto styleable = Styleable::fromRenderer(*backdrop))
                styleable->cancelStyleOriginatedAnimations();
        }
    }

    // Unable to protect the document as it may have started destruction.
    document().removeTopLayerElement(*this);
    clearEventTargetFlag(EventTargetFlag::IsInTopLayer);

    document().scheduleContentRelevancyUpdate(ContentRelevancy::IsInTopLayer);

    // Invalidate inert state
    invalidateStyleInternal();
    if (RefPtr documentElement = document().documentElement())
        documentElement->invalidateStyleInternal();
    if (RefPtr modalElement = document().activeModalDialog())
        modalElement->invalidateStyleInternal();

    forEachRenderLayer(*this, [](RenderLayer& layer) {
        layer.establishesTopLayerDidChange();
    });
}

static PseudoElement* beforeOrAfterPseudoElement(const Element& host, PseudoId pseudoElementSpecifier)
{
    switch (pseudoElementSpecifier) {
    case PseudoId::Before:
        return host.beforePseudoElement();
    case PseudoId::After:
        return host.afterPseudoElement();
    default:
        return nullptr;
    }
}

const RenderStyle* Element::existingComputedStyle() const
{
    if (hasRareData()) {
        if (auto* style = elementRareData()->computedStyle())
            return style;
    }

    if (hasDisplayNone())
        return elementRareData()->displayContentsOrNoneStyle();

    return renderOrDisplayContentsStyle();
}

const RenderStyle* Element::renderOrDisplayContentsStyle() const
{
    return renderOrDisplayContentsStyle({ });
}

const RenderStyle* Element::renderOrDisplayContentsStyle(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    if (pseudoElementIdentifier) {
        if (auto* pseudoElement = beforeOrAfterPseudoElement(*this, pseudoElementIdentifier->pseudoId))
            return pseudoElement->renderOrDisplayContentsStyle();

        if (auto* style = renderOrDisplayContentsStyle()) {
            if (auto* cachedPseudoStyle = style->getCachedPseudoStyle(*pseudoElementIdentifier))
                return cachedPseudoStyle;
        }

        return nullptr;
    }

    if (hasDisplayContents())
        return elementRareData()->displayContentsOrNoneStyle();

    return renderStyle();
}

const RenderStyle* Element::resolveComputedStyle(ResolveComputedStyleMode mode)
{
    ASSERT(isConnected());

    Ref document = this->document();
    document->styleScope().flushPendingUpdate();

    bool isInDisplayNoneTree = false;

    // Traverse the ancestor chain to find the rootmost element that has invalid computed style.
    RefPtr rootmostInvalidElement = [&]() -> RefPtr<const Element> {
        // In ResolveComputedStyleMode::RenderedOnly case we check for display:none ancestors.
        if (mode != ResolveComputedStyleMode::RenderedOnly && !document->hasPendingStyleRecalc() && existingComputedStyle())
            return nullptr;

        if (document->hasPendingFullStyleRebuild())
            return document->documentElement();

        if (!document->documentElement() || document->documentElement()->hasStateFlag(StateFlag::IsComputedStyleInvalidFlag))
            return document->documentElement();

        RefPtr<const Element> rootmost;

        for (RefPtr element = this; element; element = element->parentElementInComposedTree()) {
            if (element->hasStateFlag(StateFlag::IsComputedStyleInvalidFlag)) {
                rootmost = element;
                continue;
            }
            auto* existing = element->existingComputedStyle();
            if (!existing) {
                rootmost = element;
                continue;
            }
            if (mode == ResolveComputedStyleMode::RenderedOnly && existing->display() == DisplayType::None) {
                isInDisplayNoneTree = true;
                // Invalid ancestor style may still affect this display:none style.
                rootmost = nullptr;
            }
        }
        return rootmost;
    }();

    if (!rootmostInvalidElement) {
        if (isInDisplayNoneTree)
            return nullptr;
        return existingComputedStyle();
    }

    RefPtr ancestorWithValidStyle = rootmostInvalidElement->parentElementInComposedTree();

    Vector<RefPtr<Element>, 32> elementsRequiringComputedStyle;
    for (RefPtr toResolve = this; toResolve != ancestorWithValidStyle; toResolve = toResolve->parentElementInComposedTree())
        elementsRequiringComputedStyle.append(toResolve);

    auto* computedStyle = ancestorWithValidStyle ? ancestorWithValidStyle->existingComputedStyle() : nullptr;

    // On iOS request delegates called during styleForElement may result in re-entering WebKit and killing the style resolver.
    Style::PostResolutionCallbackDisabler disabler(document, Style::PostResolutionCallbackDisabler::DrainCallbacks::No);

    // Resolve and cache styles starting from the most distant ancestor.
    // FIXME: This is not as efficient as it could be. For example if an ancestor has a non-inherited style change but
    // the styles are otherwise clean we would not need to re-resolve descendants.
    for (auto& element : makeReversedRange(elementsRequiringComputedStyle)) {
        if (computedStyle && computedStyle->containerType() != ContainerType::Normal && mode != ResolveComputedStyleMode::Editability) {
            // If we find a query container we need to bail out and do full style update to resolve it.
            if (document->updateStyleIfNeeded())
                return this->computedStyle();
        };
        auto style = document->styleForElementIgnoringPendingStylesheets(*element, computedStyle);
        computedStyle = style.get();
        ElementRareData& rareData = element->ensureElementRareData();
        if (auto* existing = rareData.computedStyle()) {
            auto change = Style::determineChange(*existing, *style);
            if (change > Style::Change::NonInherited) {
                for (Ref child : composedTreeChildren(*element)) {
                    if (RefPtr childElement = dynamicDowncast<Element>(WTFMove(child)))
                        childElement->setStateFlag(StateFlag::IsComputedStyleInvalidFlag);
                }
            }
        }
        rareData.setComputedStyle(WTFMove(style));
        element->clearStateFlag(StateFlag::IsComputedStyleInvalidFlag);

        if (mode == ResolveComputedStyleMode::RenderedOnly && computedStyle->display() == DisplayType::None)
            return nullptr;
    }

    return computedStyle;
}

const RenderStyle& Element::resolvePseudoElementStyle(const Style::PseudoElementIdentifier& pseudoElementIdentifier)
{
    ASSERT(!isPseudoElement());

    auto* parentStyle = existingComputedStyle();
    ASSERT(parentStyle);
    ASSERT(!parentStyle->getCachedPseudoStyle(pseudoElementIdentifier));

    Ref document = this->document();
    Style::PostResolutionCallbackDisabler disabler(document, Style::PostResolutionCallbackDisabler::DrainCallbacks::No);

    auto style = document->styleForElementIgnoringPendingStylesheets(*this, parentStyle, pseudoElementIdentifier);
    if (!style) {
        style = RenderStyle::createPtr();
        style->inheritFrom(*parentStyle);
        // FIXME: RenderStyle should switch to use PseudoElementIdentifier.
        style->setPseudoElementType(pseudoElementIdentifier.pseudoId);
        style->setPseudoElementNameArgument(pseudoElementIdentifier.nameArgument);
    }

    auto* computedStyle = style.get();
    const_cast<RenderStyle*>(parentStyle)->addCachedPseudoStyle(WTFMove(style));
    ASSERT(parentStyle->getCachedPseudoStyle(pseudoElementIdentifier));
    return *computedStyle;
}

const RenderStyle* Element::computedStyle(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    if (!isConnected())
        return nullptr;

    if (pseudoElementIdentifier) {
        if (RefPtr pseudoElement = beforeOrAfterPseudoElement(*this, pseudoElementIdentifier->pseudoId))
            return pseudoElement->computedStyle();
    }

    // FIXME: This should call resolveComputedStyle() unconditionally so we check if the style is valid.
    auto* style = existingComputedStyle();
    if (!style)
        style = resolveComputedStyle();

    if (pseudoElementIdentifier) {
        if (auto* cachedPseudoStyle = style->getCachedPseudoStyle(*pseudoElementIdentifier))
            return cachedPseudoStyle;
        return &resolvePseudoElementStyle(*pseudoElementIdentifier);
    }

    return style;
}

// FIXME: The caller should be able to just use computedStyle().
const RenderStyle* Element::computedStyleForEditability()
{
    if (!isConnected())
        return nullptr;

    return resolveComputedStyle(ResolveComputedStyleMode::Editability);
}

bool Element::needsStyleInvalidation() const
{
    if (!inRenderedDocument())
        return false;
    // If :has() is present a change in an element may affect elements outside its subtree.
    if (styleValidity() >= Style::Validity::SubtreeInvalid && !Style::Scope::forNode(*this).usesHasPseudoClass())
        return false;
    if (document().documentElement() && document().documentElement()->styleValidity() >= Style::Validity::SubtreeInvalid)
        return false;
    if (document().hasPendingFullStyleRebuild())
        return false;

    return true;
}

void Element::setChildIndex(unsigned index)
{
    ElementRareData& rareData = ensureElementRareData();
    rareData.setChildIndex(index);
}

bool Element::hasFlagsSetDuringStylingOfChildren() const
{
    return childrenAffectedByFirstChildRules()
        || childrenAffectedByLastChildRules()
        || childrenAffectedByForwardPositionalRules()
        || descendantsAffectedByForwardPositionalRules()
        || childrenAffectedByBackwardPositionalRules()
        || descendantsAffectedByBackwardPositionalRules();
}

unsigned Element::rareDataChildIndex() const
{
    ASSERT(hasRareData());
    return elementRareData()->childIndex();
}

const AtomString& Element::effectiveLang() const
{
    if (effectiveLangKnownToMatchDocumentElement())
        return document().effectiveDocumentElementLanguage();

    if (hasRareData()) {
        if (auto& lang = elementRareData()->effectiveLang(); !lang.isNull())
            return lang;
    }

    return isConnected() ? document().effectiveDocumentElementLanguage() : nullAtom();
}

const AtomString& Element::langFromAttribute() const
{
    // Spec: xml:lang takes precedence over html:lang -- http://www.w3.org/TR/xhtml1/#C_7
    if (hasXMLLangAttr())
        return getAttribute(XMLNames::langAttr);
    if (hasLangAttr())
        return getAttribute(HTMLNames::langAttr);
    return nullAtom();
}

Locale& Element::locale() const
{
    return protectedDocument()->getCachedLocale(effectiveLang());
}

void Element::normalizeAttributes()
{
    if (!hasAttributes())
        return;

    auto* attrNodeList = attrNodeListForElement(*this);
    if (!attrNodeList)
        return;

    // Copy the Attr Vector because Node::normalize() can fire synchronous JS
    // events (e.g. DOMSubtreeModified) and a JS listener could add / remove
    // attributes while we are iterating.
    auto copyOfAttrNodeList = *attrNodeList;
    for (auto& attrNode : copyOfAttrNodeList)
        attrNode->normalize();
}

PseudoElement& Element::ensurePseudoElement(PseudoId pseudoId)
{
    if (pseudoId == PseudoId::Before) {
        if (!beforePseudoElement())
            ensureElementRareData().setBeforePseudoElement(PseudoElement::create(*this, pseudoId));
        return *beforePseudoElement();
    }

    ASSERT(pseudoId == PseudoId::After);
    if (!afterPseudoElement())
        ensureElementRareData().setAfterPseudoElement(PseudoElement::create(*this, pseudoId));
    return *afterPseudoElement();
}

PseudoElement* Element::beforePseudoElement() const
{
    return hasRareData() ? elementRareData()->beforePseudoElement() : nullptr;
}

PseudoElement* Element::afterPseudoElement() const
{
    return hasRareData() ? elementRareData()->afterPseudoElement() : nullptr;
}

static void disconnectPseudoElement(PseudoElement* pseudoElement)
{
    if (!pseudoElement)
        return;
    ASSERT(!pseudoElement->renderer());
    ASSERT(pseudoElement->hostElement());
    pseudoElement->clearHostElement();
}

void Element::clearBeforePseudoElementSlow()
{
    ASSERT(hasRareData());
    disconnectPseudoElement(elementRareData()->beforePseudoElement());
    elementRareData()->setBeforePseudoElement(nullptr);
}

void Element::clearAfterPseudoElementSlow()
{
    ASSERT(hasRareData());
    disconnectPseudoElement(elementRareData()->afterPseudoElement());
    elementRareData()->setAfterPseudoElement(nullptr);
}

bool Element::matchesValidPseudoClass() const
{
    return false;
}

bool Element::matchesInvalidPseudoClass() const
{
    return false;
}

bool Element::matchesUserValidPseudoClass() const
{
    return false;
}

bool Element::matchesUserInvalidPseudoClass() const
{
    return false;
}

bool Element::matchesReadWritePseudoClass() const
{
    return false;
}

bool Element::matchesIndeterminatePseudoClass() const
{
    return false;
}

bool Element::matchesDefaultPseudoClass() const
{
    return false;
}

ExceptionOr<bool> Element::matches(const String& selector)
{
    auto query = document().selectorQueryForString(selector);
    if (query.hasException())
        return query.releaseException();
    return query.releaseReturnValue().matches(*this);
}

ExceptionOr<Element*> Element::closest(const String& selector)
{
    auto query = document().selectorQueryForString(selector);
    if (query.hasException())
        return query.releaseException();
    return query.releaseReturnValue().closest(*this);
}

bool Element::mayCauseRepaintInsideViewport(const IntRect* visibleRect) const
{
    return renderer() && renderer()->mayCauseRepaintInsideViewport(visibleRect);
}

DOMTokenList& Element::classList()
{
    ElementRareData& data = ensureElementRareData();
    if (!data.classList())
        data.setClassList(makeUniqueWithoutRefCountedCheck<DOMTokenList>(*this, HTMLNames::classAttr));
    return *data.classList();
}

SpaceSplitString Element::partNames() const
{
    return hasRareData() ? elementRareData()->partNames() : SpaceSplitString();
}

DOMTokenList& Element::part()
{
    auto& data = ensureElementRareData();
    if (!data.partList())
        data.setPartList(makeUniqueWithoutRefCountedCheck<DOMTokenList>(*this, HTMLNames::partAttr));
    return *data.partList();
}

DatasetDOMStringMap& Element::dataset()
{
    ElementRareData& data = ensureElementRareData();
    if (!data.dataset())
        data.setDataset(makeUniqueWithoutRefCountedCheck<DatasetDOMStringMap>(*this));
    return *data.dataset();
}

URL Element::getURLAttribute(const QualifiedName& name) const
{
#if ASSERT_ENABLED
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    return document().completeURL(getAttribute(name));
}

URL Element::getNonEmptyURLAttribute(const QualifiedName& name) const
{
#if ASSERT_ENABLED
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    auto value = getAttribute(name).string().trim(isASCIIWhitespace);
    if (value.isEmpty())
        return URL();
    return document().completeURL(value);
}

int Element::getIntegralAttribute(const QualifiedName& attributeName) const
{
    return parseHTMLInteger(getAttribute(attributeName)).value_or(0);
}

void Element::setIntegralAttribute(const QualifiedName& attributeName, int value)
{
    setAttribute(attributeName, AtomString::number(value));
}

unsigned Element::getUnsignedIntegralAttribute(const QualifiedName& attributeName) const
{
    return parseHTMLNonNegativeInteger(getAttribute(attributeName)).value_or(0);
}

void Element::setUnsignedIntegralAttribute(const QualifiedName& attributeName, unsigned value)
{
    setAttribute(attributeName, AtomString::number(limitToOnlyHTMLNonNegative(value)));
}

bool Element::childShouldCreateRenderer(const Node& child) const
{
    // Only create renderers for SVG elements whose parents are SVG elements, or for proper <svg xmlns="svgNS"> subdocuments.
    if (auto* childElement = dynamicDowncast<SVGElement>(child)) {
        ASSERT(!isSVGElement());
        return is<SVGSVGElement>(*childElement) && childElement->isValid();
    }
    return true;
}

#if ENABLE(FULLSCREEN_API)

void Element::webkitRequestFullscreen()
{
    requestFullscreen({ }, nullptr);
}

// FIXME: Options are currently ignored.
void Element::requestFullscreen(FullscreenOptions&&, RefPtr<DeferredPromise>&& promise)
{
    protectedDocument()->fullscreenManager().requestFullscreenForElement(*this, WTFMove(promise), FullscreenManager::EnforceIFrameAllowFullscreenRequirement);
}

void Element::setFullscreenFlag(bool flag)
{
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, { { CSSSelector::PseudoClass::Fullscreen, flag }, { CSSSelector::PseudoClass::Modal, flag } });
    if (flag)
        setElementStateFlag(ElementStateFlag::IsFullscreen);
    else
        clearElementStateFlag(ElementStateFlag::IsFullscreen);
}

#endif

ExceptionOr<void> Element::setPointerCapture(int32_t pointerId)
{
    if (RefPtr page = document().page())
        return page->pointerCaptureController().setPointerCapture(this, pointerId);
    return { };
}

ExceptionOr<void> Element::releasePointerCapture(int32_t pointerId)
{
    if (RefPtr page = document().page())
        return page->pointerCaptureController().releasePointerCapture(this, pointerId);
    return { };
}

bool Element::hasPointerCapture(int32_t pointerId)
{
    if (RefPtr page = document().page())
        return page->pointerCaptureController().hasPointerCapture(this, pointerId);
    return false;
}

#if ENABLE(POINTER_LOCK)

JSC::JSValue Element::requestPointerLock(JSC::JSGlobalObject& lexicalGlobalObject, PointerLockOptions&& options)
{
    RefPtr<DeferredPromise> promise;
    if (RefPtr page = document().page()) {
        bool optionsEnabled = document().settings().pointerLockOptionsEnabled();

        if (optionsEnabled)
            promise = DeferredPromise::create(*JSC::jsSecureCast<JSDOMGlobalObject*>(&lexicalGlobalObject), DeferredPromise::Mode::RetainPromiseOnResolve);

        page->pointerLockController().requestPointerLock(this, optionsEnabled ? std::optional(WTFMove(options)) : std::nullopt, promise);
    }
    return promise ? promise->promise() : JSC::jsUndefined();
}

void Element::requestPointerLock()
{
    if (RefPtr page = document().page())
        page->pointerLockController().requestPointerLock(this);
}

#endif

void Element::disconnectFromIntersectionObserversSlow(IntersectionObserverData& observerData)
{
    for (const auto& registration : observerData.registrations) {
        if (registration.observer)
            registration.observer->targetDestroyed(*this);
    }
    observerData.registrations.clear();

    for (const auto& observer : observerData.observers) {
        if (observer)
            observer->rootDestroyed();
    }
    observerData.observers.clear();
}

IntersectionObserverData& Element::ensureIntersectionObserverData()
{
    ASSERT(!is<HTMLImageElement>(*this));
    auto& rareData = ensureElementRareData();
    if (!rareData.intersectionObserverData())
        rareData.setIntersectionObserverData(makeUnique<IntersectionObserverData>());
    return *rareData.intersectionObserverData();
}

IntersectionObserverData* Element::intersectionObserverDataIfExists()
{
    return hasRareData() ? elementRareData()->intersectionObserverData() : nullptr;
}

ElementAnimationRareData* Element::animationRareData(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    return hasRareData() ? elementRareData()->animationRareData(pseudoElementIdentifier) : nullptr;
}

ElementAnimationRareData& Element::ensureAnimationRareData(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    return ensureElementRareData().ensureAnimationRareData(pseudoElementIdentifier);
}

KeyframeEffectStack* Element::keyframeEffectStack(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier))
        return animationData->keyframeEffectStack();
    return nullptr;
}

KeyframeEffectStack& Element::ensureKeyframeEffectStack(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    return ensureAnimationRareData(pseudoElementIdentifier).ensureKeyframeEffectStack();
}

bool Element::hasKeyframeEffects(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier)) {
        if (auto* keyframeEffectStack = animationData->keyframeEffectStack())
            return keyframeEffectStack->hasEffects();
    }
    return false;
}

const AnimationCollection* Element::animations(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier))
        return &animationData->animations();
    return nullptr;
}

bool Element::hasCompletedTransitionForProperty(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier, const AnimatableCSSProperty& property) const
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier))
        return animationData->completedTransitionsByProperty().contains(property);
    return false;
}

bool Element::hasRunningTransitionForProperty(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier, const AnimatableCSSProperty& property) const
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier))
        return animationData->runningTransitionsByProperty().contains(property);
    return false;
}

bool Element::hasRunningTransitions(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier))
        return !animationData->runningTransitionsByProperty().isEmpty();
    return false;
}

const AnimatableCSSPropertyToTransitionMap* Element::completedTransitionsByProperty(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier))
        return &animationData->completedTransitionsByProperty();
    return nullptr;
}

const AnimatableCSSPropertyToTransitionMap* Element::runningTransitionsByProperty(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier))
        return &animationData->runningTransitionsByProperty();
    return nullptr;
}

AnimationCollection& Element::ensureAnimations(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    return ensureAnimationRareData(pseudoElementIdentifier).animations();
}

CSSAnimationCollection& Element::animationsCreatedByMarkup(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    return ensureAnimationRareData(pseudoElementIdentifier).animationsCreatedByMarkup();
}

void Element::setAnimationsCreatedByMarkup(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier, CSSAnimationCollection&& animations)
{
    if (animations.isEmpty() && !animationRareData(pseudoElementIdentifier))
        return;
    ensureAnimationRareData(pseudoElementIdentifier).setAnimationsCreatedByMarkup(WTFMove(animations));
}

AnimatableCSSPropertyToTransitionMap& Element::ensureCompletedTransitionsByProperty(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    return ensureAnimationRareData(pseudoElementIdentifier).completedTransitionsByProperty();
}

AnimatableCSSPropertyToTransitionMap& Element::ensureRunningTransitionsByProperty(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    return ensureAnimationRareData(pseudoElementIdentifier).runningTransitionsByProperty();
}

const RenderStyle* Element::lastStyleChangeEventStyle(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier))
        return animationData->lastStyleChangeEventStyle();
    return nullptr;
}

void Element::setLastStyleChangeEventStyle(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier, std::unique_ptr<const RenderStyle>&& style)
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier))
        animationData->setLastStyleChangeEventStyle(WTFMove(style));
    else if (style)
        ensureAnimationRareData(pseudoElementIdentifier).setLastStyleChangeEventStyle(WTFMove(style));
}

bool Element::hasPropertiesOverridenAfterAnimation(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier))
        return animationData->hasPropertiesOverridenAfterAnimation();
    return false;
}

void Element::setHasPropertiesOverridenAfterAnimation(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier, bool value)
{
    if (auto* animationData = animationRareData(pseudoElementIdentifier)) {
        animationData->setHasPropertiesOverridenAfterAnimation(value);
        return;
    }
    if (value)
        ensureAnimationRareData(pseudoElementIdentifier).setHasPropertiesOverridenAfterAnimation(true);
}

void Element::cssAnimationsDidUpdate(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    ensureAnimationRareData(pseudoElementIdentifier).cssAnimationsDidUpdate();
}

void Element::keyframesRuleDidChange(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    ensureAnimationRareData(pseudoElementIdentifier).keyframesRuleDidChange();
}

bool Element::hasPendingKeyframesUpdate(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    auto* data = animationRareData(pseudoElementIdentifier);
    return data && data->hasPendingKeyframesUpdate();
}

void Element::disconnectFromResizeObserversSlow(ResizeObserverData& observerData)
{
    for (const auto& observer : observerData.observers)
        observer->targetDestroyed(*this);
    observerData.observers.clear();
}

ResizeObserverData& Element::ensureResizeObserverData()
{
    auto& rareData = ensureElementRareData();
    if (!rareData.resizeObserverData())
        rareData.setResizeObserverData(makeUnique<ResizeObserverData>());
    return *rareData.resizeObserverData();
}

ResizeObserverData* Element::resizeObserverDataIfExists()
{
    return hasRareData() ? elementRareData()->resizeObserverData() : nullptr;
}

std::optional<LayoutUnit> Element::lastRememberedLogicalWidth() const
{
    return hasRareData() ? elementRareData()->lastRememberedLogicalWidth() : std::nullopt;
}

std::optional<LayoutUnit> Element::lastRememberedLogicalHeight() const
{
    return hasRareData() ? elementRareData()->lastRememberedLogicalHeight() : std::nullopt;
}

void Element::setLastRememberedLogicalWidth(LayoutUnit width)
{
    ASSERT(width >= 0);
    ensureElementRareData().setLastRememberedLogicalWidth(width);
}

void Element::clearLastRememberedLogicalWidth()
{
    if (!hasRareData())
        return;
    elementRareData()->clearLastRememberedLogicalWidth();
}

void Element::setLastRememberedLogicalHeight(LayoutUnit height)
{
    ASSERT(height >= 0);
    ensureElementRareData().setLastRememberedLogicalHeight(height);
}

void Element::clearLastRememberedLogicalHeight()
{
    if (!hasRareData())
        return;
    elementRareData()->clearLastRememberedLogicalHeight();
}

bool Element::isSpellCheckingEnabled() const
{
    for (auto* ancestor = this; ancestor; ancestor = ancestor->parentOrShadowHostElement()) {
        auto& value = ancestor->attributeWithoutSynchronization(HTMLNames::spellcheckAttr);
        if (value.isNull())
            continue;
        if (value.isEmpty() || equalLettersIgnoringASCIICase(value, "true"_s))
            return true;
        if (equalLettersIgnoringASCIICase(value, "false"_s))
            return false;
    }
    return true;
}

bool Element::isWritingSuggestionsEnabled() const
{
    // If none of the following conditions are true, then return `false`.

    // `element` is an `input` element whose `type` attribute is in either the
    // `Text`, `Search`, `URL`, `Email` state and is `mutable`.
    auto isEligibleInputElement = [&] {
        RefPtr input = dynamicDowncast<HTMLInputElement>(*this);
        if (!input)
            return false;

        return !input->isDisabledFormControl() && input->supportsWritingSuggestions();
    };

    // `element` is a `textarea` element that is `mutable`.
    auto isEligibleTextArea = [&] {
        RefPtr textArea = dynamicDowncast<HTMLTextAreaElement>(*this);
        if (!textArea)
            return false;

        return !textArea->isDisabledFormControl();
    };

    // `element` is an `editing host` or is `editable`.

    if (!isEligibleInputElement() && !isEligibleTextArea() && !hasEditableStyle())
        return false;

    // If `element` has an 'inclusive ancestor' with a `writingsuggestions` content attribute that's
    // not in the `default` state and the nearest such ancestor's `writingsuggestions` content attribute
    // is in the `false` state, then return `false`.

    for (RefPtr ancestor = this; ancestor; ancestor = ancestor->parentElementInComposedTree()) {
        auto& value = ancestor->attributeWithoutSynchronization(HTMLNames::writingsuggestionsAttr);

        if (value.isNull())
            continue;
        if (value.isEmpty() || equalLettersIgnoringASCIICase(value, "true"_s))
            return true;
        if (equalLettersIgnoringASCIICase(value, "false"_s))
            return false;
    }

    // This is not yet part of the spec, but it improves web-compatibility; if autocomplete
    // is intentionally off, the site author probably wants writingsuggestions off too.
    auto autocompleteValue = attributeWithoutSynchronization(HTMLNames::autocompleteAttr);
    if (equalLettersIgnoringASCIICase(autocompleteValue, "off"_s))
        return false;

    if (protectedDocument()->quirks().shouldDisableWritingSuggestionsByDefaultQuirk())
        return false;

    // Otherwise, return `true`.
    return true;
}

#if ASSERT_ENABLED
bool Element::fastAttributeLookupAllowed(const QualifiedName& name) const
{
    if (name == HTMLNames::styleAttr)
        return false;

    if (auto* svgElement = dynamicDowncast<SVGElement>(*this))
        return !svgElement->isAnimatedPropertyAttribute(name);

    return true;
}
#endif

#if DUMP_NODE_STATISTICS
bool Element::hasNamedNodeMap() const
{
    return hasRareData() && elementRareData()->attributeMap();
}
#endif

inline void Element::updateName(const AtomString& oldName, const AtomString& newName)
{
    if (!isInTreeScope())
        return;

    if (oldName == newName)
        return;

    updateNameForTreeScope(treeScope(), oldName, newName);

    if (!isInDocumentTree())
        return;
    if (RefPtr htmlDocument = dynamicDowncast<HTMLDocument>(document()))
        updateNameForDocument(*htmlDocument, oldName, newName);
}

void Element::updateNameForTreeScope(TreeScope& scope, const AtomString& oldName, const AtomString& newName)
{
    ASSERT(oldName != newName);

    if (!oldName.isEmpty())
        scope.removeElementByName(oldName, *this);
    if (!newName.isEmpty())
        scope.addElementByName(newName, *this);
}

void Element::updateNameForDocument(HTMLDocument& document, const AtomString& oldName, const AtomString& newName)
{
    ASSERT(oldName != newName);

    if (WindowNameCollection::elementMatchesIfNameAttributeMatch(*this)) {
        const AtomString& id = WindowNameCollection::elementMatchesIfIdAttributeMatch(*this) ? getIdAttribute() : nullAtom();
        if (!oldName.isEmpty() && oldName != id)
            document.removeWindowNamedItem(oldName, *this);
        if (!newName.isEmpty() && newName != id)
            document.addWindowNamedItem(newName, *this);
    }

    if (DocumentNameCollection::elementMatchesIfNameAttributeMatch(*this)) {
        const AtomString& id = DocumentNameCollection::elementMatchesIfIdAttributeMatch(*this) ? getIdAttribute() : nullAtom();
        if (!oldName.isEmpty() && oldName != id)
            document.removeDocumentNamedItem(oldName, *this);
        if (!newName.isEmpty() && newName != id)
            document.addDocumentNamedItem(newName, *this);
    }
}

inline void Element::updateId(const AtomString& oldId, const AtomString& newId, NotifyObservers notifyObservers)
{
    if (!isInTreeScope())
        return;

    if (oldId == newId)
        return;

    updateIdForTreeScope(treeScope(), oldId, newId, notifyObservers);

    if (!isInDocumentTree())
        return;
    if (RefPtr htmlDocument = dynamicDowncast<HTMLDocument>(document()))
        updateIdForDocument(*htmlDocument, oldId, newId, HTMLDocumentNamedItemMapsUpdatingCondition::UpdateOnlyIfDiffersFromNameAttribute);
}

void Element::updateIdForTreeScope(TreeScope& scope, const AtomString& oldId, const AtomString& newId, NotifyObservers notifyObservers)
{
    ASSERT(oldId != newId);

    if (!oldId.isEmpty())
        scope.removeElementById(oldId, *this, notifyObservers == NotifyObservers::Yes);
    if (!newId.isEmpty())
        scope.addElementById(newId, *this, notifyObservers == NotifyObservers::Yes);
}

void Element::updateIdForDocument(HTMLDocument& document, const AtomString& oldId, const AtomString& newId, HTMLDocumentNamedItemMapsUpdatingCondition condition)
{
    ASSERT(oldId != newId);

    if (WindowNameCollection::elementMatchesIfIdAttributeMatch(*this)) {
        const AtomString& name = condition == HTMLDocumentNamedItemMapsUpdatingCondition::UpdateOnlyIfDiffersFromNameAttribute && WindowNameCollection::elementMatchesIfNameAttributeMatch(*this) ? getNameAttribute() : nullAtom();
        if (!oldId.isEmpty() && oldId != name)
            document.removeWindowNamedItem(oldId, *this);
        if (!newId.isEmpty() && newId != name)
            document.addWindowNamedItem(newId, *this);
    }

    if (DocumentNameCollection::elementMatchesIfIdAttributeMatch(*this)) {
        const AtomString& name = condition == HTMLDocumentNamedItemMapsUpdatingCondition::UpdateOnlyIfDiffersFromNameAttribute && DocumentNameCollection::elementMatchesIfNameAttributeMatch(*this) ? getNameAttribute() : nullAtom();
        if (!oldId.isEmpty() && oldId != name)
            document.removeDocumentNamedItem(oldId, *this);
        if (!newId.isEmpty() && newId != name)
            document.addDocumentNamedItem(newId, *this);
    }
}

void Element::willModifyAttribute(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue)
{
    if (name == HTMLNames::idAttr)
        updateId(oldValue, newValue, NotifyObservers::No); // Will notify observers after the attribute is actually changed.
    else if (name == HTMLNames::nameAttr)
        updateName(oldValue, newValue);
    else if (name == HTMLNames::forAttr) {
        if (auto* label = dynamicDowncast<HTMLLabelElement>(*this)) {
            if (treeScope().shouldCacheLabelsByForAttribute())
                label->updateLabel(treeScope(), oldValue, newValue);
        }
    }

    if (auto recipients = MutationObserverInterestGroup::createForAttributesMutation(*this, name))
        recipients->enqueueMutationRecord(MutationRecord::createAttributes(*this, name, oldValue));

    InspectorInstrumentation::willModifyDOMAttr(protectedDocument(), *this, oldValue, newValue);
}

void Element::didAddAttribute(const QualifiedName& name, const AtomString& value)
{
    notifyAttributeChanged(name, nullAtom(), value);
    InspectorInstrumentation::didModifyDOMAttr(protectedDocument(), *this, name.toAtomString(), value);
    dispatchSubtreeModifiedEvent();
}

void Element::didModifyAttribute(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue)
{
    notifyAttributeChanged(name, oldValue, newValue);
    InspectorInstrumentation::didModifyDOMAttr(protectedDocument(), *this, name.toAtomString(), newValue);
    // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::didRemoveAttribute(const QualifiedName& name, const AtomString& oldValue)
{
    notifyAttributeChanged(name, oldValue, nullAtom());
    InspectorInstrumentation::didRemoveDOMAttr(protectedDocument(), *this, name.toAtomString());
    dispatchSubtreeModifiedEvent();
}

IntPoint Element::savedLayerScrollPosition() const
{
    return hasRareData() ? elementRareData()->savedLayerScrollPosition() : IntPoint();
}

void Element::setSavedLayerScrollPositionSlow(const ScrollPosition& position)
{
    ASSERT(!position.isZero() || hasRareData());
    ensureElementRareData().setSavedLayerScrollPosition(position);
}

RefPtr<Attr> Element::attrIfExists(const QualifiedName& name)
{
    if (auto* attrNodeList = attrNodeListForElement(*this))
        return findAttrNodeInList(*attrNodeList, name);
    return nullptr;
}

Ref<Attr> Element::ensureAttr(const QualifiedName& name)
{
    auto& attrNodeList = ensureAttrNodeListForElement(*this);
    RefPtr<Attr> attrNode = findAttrNodeInList(attrNodeList, name);
    if (!attrNode) {
        attrNode = Attr::create(*this, name);
        attrNode->setTreeScopeRecursively(treeScope());
        attrNodeList.append(attrNode);
    }
    return attrNode.releaseNonNull();
}

void Element::detachAttrNodeFromElementWithValue(Attr* attrNode, const AtomString& value)
{
    ASSERT(hasSyntheticAttrChildNodes());
    attrNode->detachFromElementWithValue(value);

    auto& attrNodeList = *attrNodeListForElement(*this);
    bool found = attrNodeList.removeFirstMatching([attrNode](auto& attribute) {
        return attribute->qualifiedName() == attrNode->qualifiedName();
    });
    ASSERT_UNUSED(found, found);
    if (attrNodeList.isEmpty())
        removeAttrNodeListForElement(*this);
}

void Element::detachAllAttrNodesFromElement()
{
    auto* attrNodeList = attrNodeListForElement(*this);
    ASSERT(attrNodeList);

    for (const Attribute& attribute : attributesIterator()) {
        if (RefPtr<Attr> attrNode = findAttrNodeInList(*attrNodeList, attribute.name()))
            attrNode->detachFromElementWithValue(attribute.value());
    }

    removeAttrNodeListForElement(*this);
}

void Element::resetComputedStyle()
{
    if (!hasRareData() || !elementRareData()->computedStyle())
        return;

    auto reset = [](Element& element) {
        if (element.hasCustomStyleResolveCallbacks())
            element.willResetComputedStyle();
        element.elementRareData()->setComputedStyle(nullptr);
    };
    reset(*this);
    for (Ref child : descendantsOfType<Element>(*this)) {
        if (!child->hasRareData() || !child->elementRareData()->computedStyle() || child->hasDisplayContents() || child->hasDisplayNone())
            continue;
        reset(child);
    }
}

void Element::resetStyleRelations()
{
    clearStyleFlags(NodeStyleFlag::StyleAffectedByEmpty);
    clearStyleFlags(NodeStyleFlag::AffectedByHasWithPositionalPseudoClass);
    if (!hasRareData())
        return;
    elementRareData()->setChildIndex(0);
}

void Element::resetChildStyleRelations()
{
    clearStyleFlags({
        NodeStyleFlag::ChildrenAffectedByFirstChildRules,
        NodeStyleFlag::ChildrenAffectedByLastChildRules,
        NodeStyleFlag::ChildrenAffectedByForwardPositionalRules,
        NodeStyleFlag::ChildrenAffectedByBackwardPositionalRules
    });
}

void Element::resetAllDescendantStyleRelations()
{
    resetChildStyleRelations();
    
    clearStyleFlags({
        NodeStyleFlag::DescendantsAffectedByForwardPositionalRules,
        NodeStyleFlag::DescendantsAffectedByBackwardPositionalRules
    });
}

void Element::clearHoverAndActiveStatusBeforeDetachingRenderer()
{
    if (!isUserActionElement())
        return;
    Ref document = this->document();
    if (hovered())
        document->hoveredElementDidDetach(*this);
    if (isInActiveChain())
        document->elementInActiveChainDidDetach(*this);
    document->userActionElements().clearActiveAndHovered(*this);
}

void Element::willRecalcStyle(Style::Change)
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::didRecalcStyle(Style::Change)
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::willResetComputedStyle()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::willAttachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::didAttachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::willDetachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::didDetachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

std::optional<Style::ResolvedStyle> Element::resolveCustomStyle(const Style::ResolutionContext&, const RenderStyle*)
{
    ASSERT(hasCustomStyleResolveCallbacks());
    return std::nullopt;
}

void Element::cloneAttributesFromElement(const Element& other)
{
    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    other.synchronizeAllAttributes();
    if (!other.m_elementData) {
        m_elementData = nullptr;
        if (auto* inputElement = dynamicDowncast<HTMLInputElement>(*this)) {
            DelayedUpdateValidityScope delayedUpdateValidityScope(*inputElement);
            inputElement->initializeInputTypeAfterParsingOrCloning();
        }
        return;
    }

    // We can't update window and document's named item maps since the presence of image and object elements depend on other attributes and children.
    // Fortunately, those named item maps are only updated when this element is in the document, which should never be the case.
    ASSERT(!isConnected());

    const AtomString& oldID = getIdAttribute();
    const AtomString& newID = other.getIdAttribute();

    if (!oldID.isNull() || !newID.isNull())
        updateId(oldID, newID, NotifyObservers::No); // Will notify observers after the attribute is actually changed.

    const AtomString& oldName = getNameAttribute();
    const AtomString& newName = other.getNameAttribute();

    if (!oldName.isNull() || !newName.isNull())
        updateName(oldName, newName);

    // If 'other' has a mutable ElementData, convert it to an immutable one so we can share it between both elements.
    // We can only do this if there is no CSSOM wrapper for other's inline style, and there are no presentation attributes.
    auto* uniqueElementData = dynamicDowncast<UniqueElementData>(*other.m_elementData);
    if (uniqueElementData
        && !uniqueElementData->presentationalHintStyle()
        && (!uniqueElementData->inlineStyle() || !uniqueElementData->inlineStyle()->hasCSSOMWrapper()))
        const_cast<Element&>(other).m_elementData = uniqueElementData->makeShareableCopy();

    if (!other.m_elementData->isUnique())
        m_elementData = other.m_elementData;
    else
        m_elementData = other.m_elementData->makeUniqueCopy();

    if (auto* inputElement = dynamicDowncast<HTMLInputElement>(*this)) {
        DelayedUpdateValidityScope delayedUpdateValidityScope(*inputElement);
        inputElement->initializeInputTypeAfterParsingOrCloning();
    }

    for (const Attribute& attribute : attributesIterator())
        notifyAttributeChanged(attribute.name(), nullAtom(), attribute.value(), AttributeModificationReason::ByCloning);

    setNonce(other.nonce());
}

void Element::cloneDataFromElement(const Element& other)
{
    cloneAttributesFromElement(other);
    copyNonAttributePropertiesFromElement(other);
#if ASSERT_ENABLED
    if (auto* input = dynamicDowncast<HTMLInputElement>(*this))
        ASSERT(!input->userAgentShadowRoot());
#endif
}

void Element::createUniqueElementData()
{
    if (!m_elementData)
        m_elementData = UniqueElementData::create();
    else
        m_elementData = uncheckedDowncast<ShareableElementData>(*m_elementData).makeUniqueCopy();
}

bool Element::canContainRangeEndPoint() const
{
    return !equalLettersIgnoringASCIICase(attributeWithoutSynchronization(roleAttr), "img"_s);
}

String Element::resolveURLStringIfNeeded(const String& urlString, ResolveURLs resolveURLs, const URL& base) const
{
    if (resolveURLs == ResolveURLs::No)
        return urlString;

    static MainThreadNeverDestroyed<const AtomString> maskedURLStringForBindings(document().maskedURLStringForBindings());
    URL completeURL = base.isNull() ? document().completeURL(urlString) : URL(base, urlString);

    switch (resolveURLs) {
    case ResolveURLs::Yes:
        return completeURL.string();

    case ResolveURLs::YesExcludingURLsForPrivacy: {
        if (document().shouldMaskURLForBindings(completeURL))
            return maskedURLStringForBindings.get();
        if (!document().url().protocolIsFile())
            return completeURL.string();
        break;
    }

    case ResolveURLs::NoExcludingURLsForPrivacy:
        if (document().shouldMaskURLForBindings(completeURL))
            return maskedURLStringForBindings.get();
        break;

    case ResolveURLs::No:
        ASSERT_NOT_REACHED();
        break;
    }

    return urlString;
}

String Element::completeURLsInAttributeValue(const URL& base, const Attribute& attribute, ResolveURLs resolveURLs) const
{
    return resolveURLStringIfNeeded(attribute.value(), resolveURLs, base);
}

Attribute Element::replaceURLsInAttributeValue(const Attribute& attribute, const HashMap<String, String>&) const
{
    return attribute;
}

ExceptionOr<Node*> Element::insertAdjacent(const String& where, Ref<Node>&& newChild)
{
    // In Internet Explorer if the element has no parent and where is "beforeBegin" or "afterEnd",
    // a document fragment is created and the elements appended in the correct order. This document
    // fragment isn't returned anywhere.
    //
    // This is impossible for us to implement as the DOM tree does not allow for such structures,
    // Opera also appears to disallow such usage.

    if (equalLettersIgnoringASCIICase(where, "beforebegin"_s)) {
        RefPtr parent = this->parentNode();
        if (!parent)
            return nullptr;
        auto result = parent->insertBefore(newChild, this);
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    if (equalLettersIgnoringASCIICase(where, "afterbegin"_s)) {
        auto result = insertBefore(newChild, protectedFirstChild());
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    if (equalLettersIgnoringASCIICase(where, "beforeend"_s)) {
        auto result = appendChild(newChild);
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    if (equalLettersIgnoringASCIICase(where, "afterend"_s)) {
        RefPtr parent = this->parentNode();
        if (!parent)
            return nullptr;
        auto result = parent->insertBefore(newChild, protectedNextSibling());
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    return Exception { ExceptionCode::SyntaxError };
}

ExceptionOr<Element*> Element::insertAdjacentElement(const String& where, Element& newChild)
{
    auto result = insertAdjacent(where, newChild);
    if (result.hasException())
        return result.releaseException();
    return downcast<Element>(result.releaseReturnValue());
}

// Step 1 of https://w3c.github.io/DOM-Parsing/#dom-element-insertadjacenthtml.
static ExceptionOr<ContainerNode&> contextNodeForInsertion(const String& where, Element& element)
{
    if (equalLettersIgnoringASCIICase(where, "beforebegin"_s) || equalLettersIgnoringASCIICase(where, "afterend"_s)) {
        RefPtr parent = element.parentNode();
        if (!parent || is<Document>(*parent))
            return Exception { ExceptionCode::NoModificationAllowedError };
        return *parent;
    }
    if (equalLettersIgnoringASCIICase(where, "afterbegin"_s) || equalLettersIgnoringASCIICase(where, "beforeend"_s))
        return element;
    return Exception { ExceptionCode::SyntaxError };
}

// Step 2 of https://w3c.github.io/DOM-Parsing/#dom-element-insertadjacenthtml.
static ExceptionOr<Ref<Element>> contextElementForInsertion(const String& where, Element& element)
{
    auto contextNodeResult = contextNodeForInsertion(where, element);
    if (contextNodeResult.hasException())
        return contextNodeResult.releaseException();
    auto& contextNode = contextNodeResult.releaseReturnValue();
    RefPtr contextElement = dynamicDowncast<Element>(contextNode);
    if (!contextElement || (contextNode.document().isHTMLDocument() && is<HTMLHtmlElement>(contextNode)))
        return Ref<Element> { HTMLBodyElement::create(contextNode.protectedDocument()) };
    return contextElement.releaseNonNull();
}

// https://w3c.github.io/DOM-Parsing/#dom-element-insertadjacenthtml
ExceptionOr<void> Element::insertAdjacentHTML(const String& where, const String& markup, NodeVector* addedNodes)
{
    // Steps 1 and 2.
    auto contextElement = contextElementForInsertion(where, *this);
    if (contextElement.hasException())
        return contextElement.releaseException();
    // Step 3.
    auto fragment = createFragmentForInnerOuterHTML(contextElement.releaseReturnValue(), markup, { ParserContentPolicy::AllowScriptingContent });
    if (fragment.hasException())
        return fragment.releaseException();

    if (UNLIKELY(addedNodes)) {
        // Must be called before insertAdjacent, as otherwise the children of fragment will be moved
        // to their new parent and will be harder to keep track of.
        collectChildNodes(fragment.returnValue(), *addedNodes);
    }

    // Step 4.
    auto result = insertAdjacent(where, fragment.releaseReturnValue());
    if (result.hasException())
        return result.releaseException();
    return { };
}

ExceptionOr<void> Element::insertAdjacentHTML(const String& where, const String& markup)
{
    return insertAdjacentHTML(where, markup, nullptr);
}

ExceptionOr<void> Element::insertAdjacentText(const String& where, String&& text)
{
    auto result = insertAdjacent(where, document().createTextNode(WTFMove(text)));
    if (result.hasException())
        return result.releaseException();
    return { };
}

RefPtr<Element> Element::findAnchorElementForLink(String& outAnchorName)
{
    if (!isLink())
        return nullptr;

    const AtomString& href = attributeWithoutSynchronization(HTMLNames::hrefAttr);
    if (href.isNull())
        return nullptr;

    Ref document = this->document();
    URL url = document->completeURL(href);
    if (!url.isValid())
        return nullptr;

    if (url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(url, document->baseURL())) {
        outAnchorName = url.fragmentIdentifier().toString();
        return document->findAnchor(outAnchorName);
    }

    return nullptr;
}

ExceptionOr<Ref<WebAnimation>> Element::animate(JSC::JSGlobalObject& lexicalGlobalObject, JSC::Strong<JSC::JSObject>&& keyframes, std::optional<std::variant<double, KeyframeAnimationOptions>>&& options)
{
    String id = emptyString();
    std::optional<RefPtr<AnimationTimeline>> timeline;
    std::variant<FramesPerSecond, AnimationFrameRatePreset> frameRate = AnimationFrameRatePreset::Auto;
    std::optional<std::variant<double, KeyframeEffectOptions>> keyframeEffectOptions;
    if (options) {
        auto optionsValue = options.value();
        std::variant<double, KeyframeEffectOptions> keyframeEffectOptionsVariant;
        if (std::holds_alternative<double>(optionsValue))
            keyframeEffectOptionsVariant = std::get<double>(optionsValue);
        else {
            auto keyframeEffectOptions = std::get<KeyframeAnimationOptions>(optionsValue);
            id = keyframeEffectOptions.id;
            frameRate = keyframeEffectOptions.frameRate;
            timeline = keyframeEffectOptions.timeline;
            keyframeEffectOptionsVariant = WTFMove(keyframeEffectOptions);
        }
        keyframeEffectOptions = keyframeEffectOptionsVariant;
    }

    Ref document = this->document();
    auto keyframeEffectResult = KeyframeEffect::create(lexicalGlobalObject, document, this, WTFMove(keyframes), WTFMove(keyframeEffectOptions));
    if (keyframeEffectResult.hasException())
        return keyframeEffectResult.releaseException();

    Ref animation = WebAnimation::create(document, &keyframeEffectResult.returnValue().get());
    animation->setId(WTFMove(id));
    if (timeline)
        animation->setTimeline(timeline->get());
    animation->setBindingsFrameRate(WTFMove(frameRate));

    auto animationPlayResult = animation->play();
    if (animationPlayResult.hasException())
        return animationPlayResult.releaseException();

    return animation;
}

Vector<RefPtr<WebAnimation>> Element::getAnimations(std::optional<GetAnimationsOptions> options)
{
    // If we are to return animations in the subtree, we can get all of the document's animations and filter
    // animations targeting that are not registered on this element, one of its pseudo elements or a child's
    // pseudo element.
    Ref document = this->document();
    if (options && options->subtree) {
        return document->matchingAnimations([&] (Element& target) -> bool {
            return contains(&target);
        });
    }

    // For the list of animations to be current, we need to account for any pending CSS changes,
    // such as updates to CSS Animations and CSS Transitions. This requires updating layout as
    // well since resolving layout-dependent media queries could yield animations.
    // FIXME: We might be able to use ComputedStyleExtractor which is more optimized.
    if (RefPtr owner = document->ownerElement())
        owner->protectedDocument()->updateLayout();
    document->updateStyleIfNeeded();

    Vector<RefPtr<WebAnimation>> animations;
    if (auto* effectStack = keyframeEffectStack({ })) {
        for (auto& effect : effectStack->sortedEffects()) {
            if (effect->animation()->isRelevant())
                animations.append(effect->animation());
        }
    }
    return animations;
}

ElementIdentifier Element::identifier() const
{
    return elementIdentifiersMap().ensure(const_cast<Element&>(*this), [&] {
        setElementStateFlag(ElementStateFlag::HasElementIdentifier);
        return ElementIdentifier::generate();
    }).iterator->value;
}

Element* Element::fromIdentifier(ElementIdentifier identifier)
{
    for (auto& [element, elementIdentifier] : elementIdentifiersMap()) {
        if (elementIdentifier == identifier)
            return element.ptr();
    }
    return nullptr;
}

StylePropertyMap* Element::attributeStyleMap()
{
    if (!hasRareData())
        return nullptr;
    return elementRareData()->attributeStyleMap();
}

void Element::setAttributeStyleMap(Ref<StylePropertyMap>&& map)
{
    ensureElementRareData().setAttributeStyleMap(WTFMove(map));
}

void Element::ensureFormAssociatedCustomElement()
{
    auto& customElement = downcast<HTMLMaybeFormAssociatedCustomElement>(*this);
    auto& data = ensureElementRareData();
    if (!data.formAssociatedCustomElement())
        data.setFormAssociatedCustomElement(makeUniqueWithoutRefCountedCheck<FormAssociatedCustomElement>(customElement));
}

FormAssociatedCustomElement& Element::formAssociatedCustomElementUnsafe() const
{
    RELEASE_ASSERT(is<HTMLMaybeFormAssociatedCustomElement>(*this));
    ASSERT(hasRareData());
    auto* customElement = elementRareData()->formAssociatedCustomElement();
    ASSERT(customElement);
    return *customElement;
}

StylePropertyMapReadOnly* Element::computedStyleMap()
{
    auto& rareData = ensureElementRareData();
    if (auto* map = rareData.computedStyleMap())
        return map;

    auto map = ComputedStylePropertyMapReadOnly::create(*this);
    rareData.setComputedStyleMap(WTFMove(map));
    return rareData.computedStyleMap();
}

bool Element::hasDuplicateAttribute() const
{
    return hasEventTargetFlag(EventTargetFlag::HasDuplicateAttribute);
}

void Element::setHasDuplicateAttribute(bool hasDuplicateAttribute)
{
    setEventTargetFlag(EventTargetFlag::HasDuplicateAttribute, hasDuplicateAttribute);
}

bool Element::isPopoverShowing() const
{
    return popoverData() && popoverData()->visibilityState() == PopoverVisibilityState::Showing;
}

// https://drafts.csswg.org/css-contain/#relevant-to-the-user
bool Element::isRelevantToUser() const
{
    if (auto relevancy = contentRelevancy())
        return !relevancy->isEmpty();
    return false;
}

std::optional<OptionSet<ContentRelevancy>> Element::contentRelevancy() const
{
    if (!hasRareData())
        return std::nullopt;
    return elementRareData()->contentRelevancy();
}

void Element::setContentRelevancy(OptionSet<ContentRelevancy> contentRelevancy)
{
    ensureElementRareData().setContentRelevancy(contentRelevancy);
}

bool Element::checkVisibility(const CheckVisibilityOptions& options)
{
    document().updateStyleIfNeeded();

    auto* style = computedStyle();

    // Disconnected node, not rendered.
    if (!style)
        return false;

    // See https://github.com/w3c/csswg-drafts/issues/9478.
    if (style->display() == DisplayType::Contents)
        return false;

    if ((options.visibilityProperty || options.checkVisibilityCSS) && style->visibility() != Visibility::Visible)
        return false;

    RefPtr parent = parentElementInComposedTree();
    auto isSkippedContentWithReason = [&](ContentVisibility reason) -> bool {
        ASSERT(!parent || parent->computedStyle());
        if (style->usedContentVisibility() != reason)
            return false;

        // usedContentVisibility() includes the skipped content root, so we query the parent to make sure roots are not considered as skipped.
        if (!parent || parent->computedStyle()->usedContentVisibility() != reason)
            return false;

        return true;
    };

    if (isSkippedContentWithReason(ContentVisibility::Hidden))
        return false;

    if (options.contentVisibilityAuto && isSkippedContentWithReason(ContentVisibility::Auto))
        return false;

    for (RefPtr ancestor = this; ancestor; ancestor = ancestor->parentElementInComposedTree()) {
        auto* ancestorStyle = ancestor->computedStyle();
        if (ancestorStyle->display() == DisplayType::None)
            return false;

        if ((options.opacityProperty || options.checkOpacity) && ancestorStyle->opacity() == 0.0f)
            return false;
    }

    return true;
}

AtomString Element::makeTargetBlankIfHasDanglingMarkup(const AtomString& target)
{
    if ((target.contains('\n') || target.contains('\r') || target.contains('\t')) && target.contains('<'))
        return "_blank"_s;
    return target;
}

bool Element::hasCustomState(const AtomString& state) const
{
    if (!document().settings().customStateSetEnabled())
        return false;

    if (hasRareData()) {
        RefPtr customStates = elementRareData()->customStateSet();
        return customStates && customStates->has(state);
    }

    return false;
}

CustomStateSet& Element::ensureCustomStateSet()
{
    auto& rareData = const_cast<Element*>(this)->ensureElementRareData();
    if (!rareData.customStateSet())
        rareData.setCustomStateSet(CustomStateSet::create(*this));
    return *rareData.customStateSet();
}

OptionSet<VisibilityAdjustment> Element::visibilityAdjustment() const
{
    if (!hasRareData())
        return { };
    return elementRareData()->visibilityAdjustment();
}

void Element::setVisibilityAdjustment(OptionSet<VisibilityAdjustment> adjustment)
{
    ensureElementRareData().setVisibilityAdjustment(adjustment);
}

} // namespace WebCore
