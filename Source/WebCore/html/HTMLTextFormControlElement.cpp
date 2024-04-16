/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "config.h"
#include "HTMLTextFormControlElement.h"

#include "AXObjectCache.h"
#include "CSSPrimitiveValueMappings.h"
#include "ChromeClient.h"
#include "CommonAtomStrings.h"
#include "DocumentInlines.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FrameSelection.h"
#include "HTMLBRElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "ImmutableStyleProperties.h"
#include "InlineIteratorBox.h"
#include "InlineIteratorLineBox.h"
#include "LayoutDisallowedScope.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderLineBreak.h"
#include "RenderStyleSetters.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLTextFormControlElement);

using namespace HTMLNames;

static Position positionForIndex(TextControlInnerTextElement*, unsigned);

HTMLTextFormControlElement::HTMLTextFormControlElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLFormControlElement(tagName, document, form)
    , m_cachedSelectionDirection(document.frame() && document.frame()->editor().behavior().shouldConsiderSelectionAsDirectional() ? SelectionHasForwardDirection : SelectionHasNoDirection)
{
}

HTMLTextFormControlElement::~HTMLTextFormControlElement() = default;

bool HTMLTextFormControlElement::childShouldCreateRenderer(const Node& child) const
{
    // FIXME: We shouldn't force the pseudo elements down into the shadow, but
    // this perserves the current behavior of WebKit.
    if (child.isPseudoElement())
        return HTMLFormControlElement::childShouldCreateRenderer(child);
    return hasShadowRootParent(child) && HTMLFormControlElement::childShouldCreateRenderer(child);
}

Node::InsertedIntoAncestorResult HTMLTextFormControlElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    InsertedIntoAncestorResult InsertedIntoAncestorResult = HTMLFormControlElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument) {
        String initialValue = value();
        setTextAsOfLastFormControlChangeEvent(initialValue.isNull() ? emptyString() : initialValue);
    }
    return InsertedIntoAncestorResult;
}

static String pointerTypeFromHitTestRequest(HitTestRequest request)
{
#if ENABLE(PENCIL_HOVER)
    if (request.penEvent())
        return WebCore::penPointerEventType();
    if (request.touchEvent())
        return WebCore::touchPointerEventType();
#else
    UNUSED_PARAM(request);
#endif
    return WebCore::mousePointerEventType();
}

void HTMLTextFormControlElement::setHovered(bool over, Style::InvalidationScope invalidationScope, HitTestRequest request)
{
    if (over == hovered())
        return;

    m_pointerType = pointerTypeFromHitTestRequest(request);

    HTMLElement::setHovered(over, invalidationScope, request);

    updatePlaceholderVisibility();
}

void HTMLTextFormControlElement::dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, const FocusOptions& options)
{
    if (supportsPlaceholder())
        updatePlaceholderVisibility();
    handleFocusEvent(oldFocusedElement.get(), options.direction);
    HTMLFormControlElement::dispatchFocusEvent(WTFMove(oldFocusedElement), options);
}

void HTMLTextFormControlElement::dispatchBlurEvent(RefPtr<Element>&& newFocusedElement)
{
    if (supportsPlaceholder())
        updatePlaceholderVisibility();
    // Match the order in Document::setFocusedElement.
    handleBlurEvent();
    HTMLFormControlElement::dispatchBlurEvent(WTFMove(newFocusedElement));
}

void HTMLTextFormControlElement::didEditInnerTextValue()
{
    if (!renderer() || !isTextField())
        return;

    LOG(Editing, "HTMLTextFormControlElement %p didEditInnerTextValue", this);

    m_lastChangeWasUserEdit = true;
    subtreeHasChanged();
}

void HTMLTextFormControlElement::forwardEvent(Event& event)
{
    auto& eventNames = WebCore::eventNames();
    if (event.type() == eventNames.blurEvent || event.type() == eventNames.focusEvent)
        return;

    if (auto innerText = innerTextElement())
        innerText->defaultEventHandler(event);
}

static bool isNotLineBreak(UChar ch) { return ch != newlineCharacter && ch != carriageReturn; }

bool HTMLTextFormControlElement::isPlaceholderEmpty() const
{
    const AtomString& attributeValue = attributeWithoutSynchronization(placeholderAttr);
    return attributeValue.string().find(isNotLineBreak) == notFound;
}

bool HTMLTextFormControlElement::placeholderShouldBeVisible() const
{
    // This function is used by the style resolver to match the :placeholder-shown pseudo class.
    // Since it is used for styling, it must not use any value depending on the style.
    return supportsPlaceholder() && isEmptyValue() && !isPlaceholderEmpty() && m_canShowPlaceholder && !(hovered() && m_pointerType == penPointerEventType());
}

void HTMLTextFormControlElement::updatePlaceholderVisibility()
{
    bool newIsPlaceholderVisible = placeholderShouldBeVisible();
    if (m_isPlaceholderVisible == newIsPlaceholderVisible)
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::PlaceholderShown, newIsPlaceholderVisible);
    m_isPlaceholderVisible = newIsPlaceholderVisible;

    if (RefPtr placeholder = placeholderElement())
        placeholder->invalidateStyle();
}

void HTMLTextFormControlElement::setCanShowPlaceholder(bool canShowPlaceholder)
{
    m_canShowPlaceholder = canShowPlaceholder;
    updatePlaceholderVisibility();
}

void HTMLTextFormControlElement::setSelectionStart(unsigned start)
{
    setSelectionRange(start, std::max(start, selectionEnd()), selectionDirection());
}

void HTMLTextFormControlElement::setSelectionEnd(unsigned end)
{
    setSelectionRange(std::min(end, selectionStart()), end, selectionDirection());
}

void HTMLTextFormControlElement::setSelectionDirection(const String& direction)
{
    setSelectionRange(selectionStart(), selectionEnd(), direction);
}

void HTMLTextFormControlElement::select(SelectionRevealMode revealMode, const AXTextStateChangeIntent& intent)
{
    if (setSelectionRange(0, std::numeric_limits<unsigned>::max(), SelectionHasNoDirection, revealMode, intent))
        scheduleSelectEvent();
}

String HTMLTextFormControlElement::selectedText() const
{
    if (!isTextField())
        return String();
    return value().substring(selectionStart(), selectionEnd() - selectionStart());
}

void HTMLTextFormControlElement::dispatchFormControlChangeEvent()
{
    if (m_textAsOfLastFormControlChangeEvent != value()) {
        setTextAsOfLastFormControlChangeEvent(value());
        dispatchChangeEvent();
    }
    setChangedSinceLastFormControlChangeEvent(false);
    setInteractedWithSinceLastFormSubmitEvent(true);
}

ExceptionOr<void> HTMLTextFormControlElement::setRangeText(StringView replacement)
{
    return setRangeText(replacement, selectionStart(), selectionEnd(), String());
}

ExceptionOr<void> HTMLTextFormControlElement::setRangeText(StringView replacement, unsigned start, unsigned end, const String& selectionMode)
{
    if (start > end)
        return Exception { ExceptionCode::IndexSizeError };

    String text = innerTextValue();
    unsigned textLength = text.length();
    unsigned replacementLength = replacement.length();
    unsigned newSelectionStart = selectionStart();
    unsigned newSelectionEnd = selectionEnd();

    start = std::min(start, textLength);
    end = std::min(end, textLength);

    if (start < end)
        text = makeStringByReplacing(text, start, end - start, replacement);
    else
        text = makeStringByInserting(text, replacement, start);

    setValue(text, TextFieldEventBehavior::DispatchNoEvent, TextControlSetValueSelection::DoNotSet);

    if (equalLettersIgnoringASCIICase(selectionMode, "select"_s)) {
        newSelectionStart = start;
        newSelectionEnd = start + replacementLength;
    } else if (equalLettersIgnoringASCIICase(selectionMode, "start"_s))
        newSelectionStart = newSelectionEnd = start;
    else if (equalLettersIgnoringASCIICase(selectionMode, "end"_s))
        newSelectionStart = newSelectionEnd = start + replacementLength;
    else {
        // Default is "preserve".
        long delta = replacementLength - (end - start);

        if (newSelectionStart > end)
            newSelectionStart += delta;
        else if (newSelectionStart > start)
            newSelectionStart = start;

        if (newSelectionEnd > end)
            newSelectionEnd += delta;
        else if (newSelectionEnd > start)
            newSelectionEnd = start + replacementLength;
    }

    if (setSelectionRange(newSelectionStart, newSelectionEnd, SelectionHasNoDirection))
        scheduleSelectEvent();

    return { };
}

void HTMLTextFormControlElement::setSelectionRange(unsigned start, unsigned end, const String& directionString, const AXTextStateChangeIntent& intent, ForBindings forBindings)
{
    TextFieldSelectionDirection direction = SelectionHasNoDirection;
    if (directionString == "forward"_s)
        direction = SelectionHasForwardDirection;
    else if (directionString == "backward"_s)
        direction = SelectionHasBackwardDirection;

    if (setSelectionRange(start, end, direction, SelectionRevealMode::DoNotReveal, intent, forBindings))
        scheduleSelectEvent();
}

bool HTMLTextFormControlElement::setSelectionRange(unsigned start, unsigned end, TextFieldSelectionDirection direction, SelectionRevealMode revealMode, const AXTextStateChangeIntent& intent, ForBindings forBindings)
{
    if (!isTextField())
        return false;

    auto innerText = innerTextElementCreatingShadowSubtreeIfNeeded();

    // Clamps to the current value length.
    unsigned innerTextValueLength = innerTextValue().length();
    end = std::min(end, innerTextValueLength);
    start = std::min(start, end);

    bool hasFocus = document().focusedElement() == this;

    RefPtr frame = document().frame();
    if (direction == SelectionHasNoDirection && frame && frame->editor().behavior().shouldConsiderSelectionAsDirectional())
        direction = SelectionHasForwardDirection;

    if (!hasFocus && innerText) {
        if (!isConnected())
            return cacheSelection(start, end, direction);

        // FIXME: Removing this synchronous layout requires fixing setSelectionWithoutUpdatingAppearance not needing up-to-date style.
        protectedDocument()->updateLayoutIgnorePendingStylesheets();

        if (!isTextField())
            return false;

        // Double-check our connected state after the layout update.
        if (!isConnected())
            return cacheSelection(start, end, direction);

        // Double-check the state of innerTextElement after the layout.
        innerText = innerTextElement();
        auto* rendererTextControl = renderer();

        if (innerText && rendererTextControl && (rendererTextControl->style().visibility() == Visibility::Hidden || !innerText->renderBox() || !innerText->renderBox()->height()))
            return cacheSelection(start, end, direction);
    }

    auto previousSelectionStart = m_cachedSelectionStart;
    auto previousSelectionEnd = m_cachedSelectionEnd;
    auto previousSelectionDirection = m_cachedSelectionDirection;

    cacheSelection(start, end, direction);
    Position startPosition = positionForIndex(innerText.get(), start);
    Position endPosition;
    if (start == end)
        endPosition = startPosition;
    else {
        if (direction == SelectionHasBackwardDirection) {
            endPosition = startPosition;
            startPosition = positionForIndex(innerText.get(), end);
        } else
            endPosition = positionForIndex(innerText.get(), end);
    }

    if (RefPtr frame = document().frame()) {
        auto options = FrameSelection::defaultSetSelectionOptions();
        if (forBindings == ForBindings::Yes)
            options.add(FrameSelection::SetSelectionOption::ForBindings);
        if (hasFocus)
            options.add(FrameSelection::SetSelectionOption::DoNotSetFocus);
        switch (revealMode) {
        case SelectionRevealMode::DoNotReveal:
            break;
        case SelectionRevealMode::Reveal:
            options.add(FrameSelection::SetSelectionOption::RevealSelection);
            break;
        case SelectionRevealMode::RevealUpToMainFrame:
            options.add(FrameSelection::SetSelectionOption::RevealSelectionUpToMainFrame);
            break;
        case SelectionRevealMode::DelegateMainFrameScroll:
            options.add(FrameSelection::SetSelectionOption::DelegateMainFrameScroll);
            break;
        }
        frame->selection().moveWithoutValidationTo(startPosition, endPosition, direction != SelectionHasNoDirection, options, intent);
    }

    return m_cachedSelectionStart != previousSelectionStart || m_cachedSelectionEnd != previousSelectionEnd || m_cachedSelectionDirection != previousSelectionDirection;
}

bool HTMLTextFormControlElement::cacheSelection(unsigned start, unsigned end, TextFieldSelectionDirection direction)
{
    bool didChange = m_cachedSelectionStart != start || m_cachedSelectionEnd != end || m_cachedSelectionDirection != direction;
    m_cachedSelectionStart = start;
    m_cachedSelectionEnd = end;
    m_cachedSelectionDirection = direction;
    m_hasCachedSelection = true;
    return didChange;
}

int HTMLTextFormControlElement::indexForVisiblePosition(const VisiblePosition& position) const
{
    auto innerText = innerTextElement();
    if (!innerText || !innerText->contains(position.deepEquivalent().anchorNode()))
        return 0;
    unsigned index = indexForPosition(position.deepEquivalent());
    ASSERT(VisiblePosition(positionForIndex(innerTextElement().get(), index)) == position);
    return index;
}

VisiblePosition HTMLTextFormControlElement::visiblePositionForIndex(int index) const
{
    ASSERT(innerTextElement());
    VisiblePosition position = positionForIndex(innerTextElement().get(), index);
    ASSERT(indexForVisiblePosition(position) == index);
    return position;
}

unsigned HTMLTextFormControlElement::selectionStart() const
{
    if (!isTextField())
        return 0;
    if (document().focusedElement() != this && hasCachedSelection())
        return m_cachedSelectionStart;

    return computeSelectionStart();
}

unsigned HTMLTextFormControlElement::computeSelectionStart() const
{
    ASSERT(isTextField());
    RefPtr frame { document().frame() };
    if (!frame)
        return 0;

    return indexForPosition(frame->selection().selection().start());
}

unsigned HTMLTextFormControlElement::selectionEnd() const
{
    if (!isTextField())
        return 0;
    if (document().focusedElement() != this && hasCachedSelection())
        return m_cachedSelectionEnd;
    return computeSelectionEnd();
}

unsigned HTMLTextFormControlElement::computeSelectionEnd() const
{
    ASSERT(isTextField());
    RefPtr frame { document().frame() };
    if (!frame)
        return 0;

    return indexForPosition(frame->selection().selection().end());
}

static const AtomString& directionString(TextFieldSelectionDirection direction)
{
    static MainThreadNeverDestroyed<const AtomString> forward("forward"_s);
    static MainThreadNeverDestroyed<const AtomString> backward("backward"_s);

    switch (direction) {
    case SelectionHasNoDirection:
        return noneAtom();
    case SelectionHasForwardDirection:
        return forward;
    case SelectionHasBackwardDirection:
        return backward;
    }

    ASSERT_NOT_REACHED();
    return noneAtom();
}

const AtomString& HTMLTextFormControlElement::selectionDirection() const
{
    if (!isTextField())
        return directionString(SelectionHasNoDirection);
    if (document().focusedElement() != this && hasCachedSelection())
        return directionString(cachedSelectionDirection());

    return directionString(computeSelectionDirection());
}

TextFieldSelectionDirection HTMLTextFormControlElement::computeSelectionDirection() const
{
    ASSERT(isTextField());
    RefPtr frame { document().frame() };
    if (!frame)
        return SelectionHasNoDirection;

    const VisibleSelection& selection = frame->selection().selection();
    return selection.isDirectional() ? (selection.isBaseFirst() ? SelectionHasForwardDirection : SelectionHasBackwardDirection) : SelectionHasNoDirection;
}

static void setContainerAndOffsetForRange(Node& node, unsigned offset, RefPtr<Node>& containerNode, unsigned& offsetInContainer)
{
    if (node.isTextNode()) {
        containerNode = &node;
        offsetInContainer = offset;
    } else {
        containerNode = node.parentNode();
        offsetInContainer = node.computeNodeIndex() + offset;
    }
}

std::optional<SimpleRange> HTMLTextFormControlElement::selection() const
{
    if (!renderer() || !isTextField() || !hasCachedSelection())
        return std::nullopt;

    unsigned start = m_cachedSelectionStart;
    unsigned end = m_cachedSelectionEnd;

    ASSERT(start <= end);
    auto innerText = innerTextElement();
    if (!innerText)
        return std::nullopt;

    if (!innerText->firstChild())
        return SimpleRange { { *innerText, 0 }, { *innerText, 0 } };

    unsigned offset = 0;
    RefPtr<Node> startNode;
    RefPtr<Node> endNode;
    for (RefPtr<Node> node = innerText->firstChild(); node; node = NodeTraversal::next(*node, innerText.get())) {
        ASSERT(!node->firstChild());
        ASSERT(node->isTextNode() || node->hasTagName(brTag));

        unsigned length = [&] {
            auto* textNode = dynamicDowncast<Text>(*node);
            return textNode ? textNode->length() : 1;
        }();

        if (offset <= start && start <= offset + length)
            setContainerAndOffsetForRange(*node, start - offset, startNode, start);

        if (offset <= end && end <= offset + length) {
            setContainerAndOffsetForRange(*node, end - offset, endNode, end);
            break;
        }

        offset += length;
    }

    if (!startNode || !endNode)
        return std::nullopt;

    return SimpleRange { { startNode.releaseNonNull(), start }, { endNode.releaseNonNull(), end } };
}

void HTMLTextFormControlElement::restoreCachedSelection(SelectionRevealMode revealMode, const AXTextStateChangeIntent& intent)
{
    if (setSelectionRange(m_cachedSelectionStart, m_cachedSelectionEnd, cachedSelectionDirection(), revealMode, intent))
        scheduleSelectEvent();
}

bool HTMLTextFormControlElement::selectionChanged(bool shouldFireSelectEvent)
{
    if (!isTextField())
        return false;

    // FIXME: Don't re-compute selection start and end if this function was called inside setSelectionRange.
    // selectionStart() or selectionEnd() will return cached selection when this node doesn't have focus
    unsigned previousSelectionStart = m_cachedSelectionStart;
    unsigned previousSelectionEnd = m_cachedSelectionEnd;
    cacheSelection(computeSelectionStart(), computeSelectionEnd(), computeSelectionDirection());

    document().setHasEverHadSelectionInsideTextFormControl();

    if (shouldFireSelectEvent && m_cachedSelectionStart != m_cachedSelectionEnd)
        dispatchEvent(Event::create(eventNames().selectEvent, Event::CanBubble::Yes, Event::IsCancelable::No));

    return previousSelectionStart != m_cachedSelectionStart || previousSelectionEnd != m_cachedSelectionEnd;
}

void HTMLTextFormControlElement::scheduleSelectionChangeEvent()
{
    if (m_hasScheduledSelectionChangeEvent)
        return;

    m_hasScheduledSelectionChangeEvent = true;
    document().eventLoop().queueTask(TaskSource::UserInteraction, [textControl = GCReachableRef { *this }] {
        textControl->m_hasScheduledSelectionChangeEvent = false;
        textControl->dispatchEvent(Event::create(eventNames().selectionchangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
    });
}

void HTMLTextFormControlElement::scheduleSelectEvent()
{
    queueTaskToDispatchEvent(TaskSource::UserInteraction, Event::create(eventNames().selectEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
    scheduleSelectionChangeEvent();
}

void HTMLTextFormControlElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == placeholderAttr) {
        updatePlaceholderText();
        updatePlaceholderVisibility();
    } else
        HTMLFormControlElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void HTMLTextFormControlElement::effectiveSpellcheckAttributeChanged(bool isSpellcheckEnabled)
{
    if (!hasCachedSelection())
        return;

    auto innerTextElement = this->innerTextElement();
    if (!innerTextElement)
        return;

    auto selection = VisibleSelection::selectionFromContentsOfNode(innerTextElement.get());
    if (isSpellcheckEnabled)
        document().editor().markMisspellingsAndBadGrammar(selection);
    else
        document().editor().clearMisspellingsAndBadGrammar(selection);
}

void HTMLTextFormControlElement::disabledStateChanged()
{
    HTMLFormControlElement::disabledStateChanged();
    updateInnerTextElementEditability();
}

void HTMLTextFormControlElement::readOnlyStateChanged()
{
    HTMLFormControlElement::readOnlyStateChanged();
    updateInnerTextElementEditability();
}

bool HTMLTextFormControlElement::isInnerTextElementEditable() const
{
    return isMutable();
}

void HTMLTextFormControlElement::updateInnerTextElementEditability()
{
    if (auto innerText = innerTextElement())
        innerText->updateInnerTextElementEditability(isInnerTextElementEditable());
}

bool HTMLTextFormControlElement::lastChangeWasUserEdit() const
{
    if (!isTextField())
        return false;
    return m_lastChangeWasUserEdit;
}

static void stripTrailingNewline(StringBuilder& result)
{
    // Remove one trailing newline; there's always one that's collapsed out by rendering.
    size_t size = result.length();
    if (size && result[size - 1] == newlineCharacter)
        result.shrink(size - 1);
}

static String innerTextValueFrom(TextControlInnerTextElement& innerText)
{
    StringBuilder result;
    for (RefPtr<Node> node = innerText.firstChild(); node; node = NodeTraversal::next(*node, &innerText)) {
        if (is<HTMLBRElement>(*node))
            result.append(newlineCharacter);
        else if (auto* textNode = dynamicDowncast<Text>(*node))
            result.append(textNode->data());
    }
    stripTrailingNewline(result);
    return result.toString();
}

void HTMLTextFormControlElement::setInnerTextValue(String&& value)
{
    LayoutDisallowedScope layoutDisallowedScope(LayoutDisallowedScope::Reason::PerformanceOptimization);
    auto innerText = innerTextElementCreatingShadowSubtreeIfNeeded();
    if (!innerText)
        return;

    ASSERT(isTextField());
    String previousValue = innerTextValueFrom(*innerText);
    bool textIsChanged = value != previousValue;
    if (textIsChanged || !innerText->hasChildNodes()) {
#if !PLATFORM(COCOA)
        if (textIsChanged && renderer()) {
#if USE(ATSPI)
            if (auto* input = dynamicDowncast<HTMLInputElement>(*this); input && input->isPasswordField()) {
                // Get the rendered text instead to not expose actual value to accessibility.
                RenderObject* renderer = this->renderer();
                while (renderer && !is<RenderText>(renderer))
                    renderer = downcast<RenderElement>(*renderer).firstChild();
                if (CheckedPtr renderText = dynamicDowncast<RenderText>(renderer))
                    previousValue = renderText->textWithoutConvertingBackslashToYenSymbol();
            }
#endif
            if (AXObjectCache* cache = document().existingAXObjectCache())
                cache->postNotification(this, AXObjectCache::AXValueChanged, PostTarget::ObservableParent);
        }
#endif

        {
            // Events dispatched on the inner text element cannot execute arbitrary author scripts.
            ScriptDisallowedScope::EventAllowedScope allowedScope(*protectedUserAgentShadowRoot());

            bool endsWithNewLine = value.endsWith('\n') || value.endsWith('\r');
            innerText->setInnerText(WTFMove(value));

            if (endsWithNewLine)
                innerText->appendChild(HTMLBRElement::create(document()));
        }

#if PLATFORM(COCOA) || USE(ATSPI)
        if (textIsChanged && renderer()) {
            if (AXObjectCache* cache = document().existingAXObjectCache())
                cache->deferTextReplacementNotificationForTextControl(*this, previousValue);
        }
#endif
    }

    setFormControlValueMatchesRenderer(true);
}

String HTMLTextFormControlElement::innerTextValue() const
{
    auto innerText = innerTextElement();
    return innerText ? innerTextValueFrom(*innerText) : emptyString();
}

static Position positionForIndex(TextControlInnerTextElement* innerText, unsigned index)
{
    unsigned remainingCharactersToMoveForward = index;
    RefPtr<Node> lastBrOrText = innerText;
    for (RefPtr<Node> node = innerText; node; node = NodeTraversal::next(*node, innerText)) {
        if (node->hasTagName(brTag)) {
            if (!remainingCharactersToMoveForward)
                return positionBeforeNode(node.get());
            remainingCharactersToMoveForward--;
            lastBrOrText = node;
        } else if (auto* text = dynamicDowncast<Text>(*node)) {
            if (remainingCharactersToMoveForward < text->length())
                return Position(text, remainingCharactersToMoveForward);
            remainingCharactersToMoveForward -= text->length();
            lastBrOrText = node;
        }
    }
    return lastPositionInOrAfterNode(lastBrOrText.get());
}

unsigned HTMLTextFormControlElement::indexForPosition(const Position& passedPosition) const
{
    auto innerText = innerTextElement();
    if (!innerText || !innerText->contains(passedPosition.anchorNode()) || passedPosition.isNull())
        return 0;

    if (positionBeforeNode(innerText.get()) == passedPosition)
        return 0;

    unsigned index = 0;
    RefPtr<Node> startNode = passedPosition.computeNodeBeforePosition();
    if (!startNode)
        startNode = passedPosition.containerNode();
    ASSERT(startNode);
    ASSERT(innerText->contains(startNode.get()));

    for (RefPtr<Node> node = startNode; node; node = NodeTraversal::previous(*node, innerText.get())) {
        if (auto* text = dynamicDowncast<Text>(*node)) {
            unsigned length = text->length();
            if (node == passedPosition.containerNode())
                index += std::min<unsigned>(length, passedPosition.offsetInContainerNode());
            else
                index += length;
        } else if (is<HTMLBRElement>(*node))
            ++index;
    }

    unsigned length = innerTextValue().length();
    index = std::min(index, length); // FIXME: We shouldn't have to call innerTextValue() just to ignore the last LF. See finishText.
#if 0
    // FIXME: This assertion code was never built, has bit rotted, and needs to be fixed before it can be enabled:
    // https://bugs.webkit.org/show_bug.cgi?id=205706.
#if ASSERT_ENABLED
    VisiblePosition visiblePosition = passedPosition;
    unsigned indexComputedByVisiblePosition = 0;
    if (visiblePosition.isNotNull())
        indexComputedByVisiblePosition = WebCore::indexForVisiblePosition(innerText, visiblePosition, false /* forSelectionPreservation */);
    ASSERT(index == indexComputedByVisiblePosition);
#endif
#endif
    return index;
}

String HTMLTextFormControlElement::valueWithHardLineBreaks() const
{
    // FIXME: It's not acceptable to ignore the HardWrap setting when there is no renderer.
    // While we have no evidence this has ever been a practical problem, it would be best to fix it some day.
    if (!isTextField())
        return value();

    auto innerText = innerTextElement();
    if (!innerText || innerText->textContent().isEmpty())
        return value();

    RenderTextControlInnerBlock* renderer = innerText->renderer();
    if (!renderer)
        return value();

    Node* breakNode = nullptr;
    unsigned breakOffset = 0;
    auto currentLineBox = InlineIterator::firstLineBoxFor(*renderer);
    if (!currentLineBox)
        return value();

    auto skipToNextSoftLineBreakPosition = [&] {
        while (currentLineBox) {
            auto lastRun = currentLineBox->lastLeafBox();
            ASSERT(lastRun);
            // Skip last line.
            currentLineBox.traverseNext();
            if (currentLineBox && !lastRun->isLineBreak()) {
                breakNode = lastRun->renderer().node();
                breakOffset = lastRun->maximumCaretOffset();
                return;
            }
        }
        breakNode = nullptr;
        breakOffset = 0;
    };

    skipToNextSoftLineBreakPosition();

    StringBuilder result;
    for (RefPtr<Node> node = innerText->firstChild(); node; node = NodeTraversal::next(*node, innerText.get())) {
        if (is<HTMLBRElement>(*node))
            result.append(newlineCharacter);
        else if (auto* textNode = dynamicDowncast<Text>(*node)) {
            String data = textNode->data();
            unsigned length = data.length();
            unsigned position = 0;
            while (breakNode == node && breakOffset <= length) {
                if (breakOffset > position) {
                    result.appendSubstring(data, position, breakOffset - position);
                    position = breakOffset;
                    result.append(newlineCharacter);
                }
                skipToNextSoftLineBreakPosition();
            }
            result.appendSubstring(data, position, length - position);
        }
        while (breakNode == node)
            skipToNextSoftLineBreakPosition();
    }
    stripTrailingNewline(result);
    return result.toString();
}

HTMLTextFormControlElement* enclosingTextFormControl(const Position& position)
{
    ASSERT(position.isNull() || position.anchorType() == Position::PositionIsOffsetInAnchor
        || position.containerNode() || !position.anchorNode()->shadowHost()
        || hasShadowRootParent(*position.anchorNode()));
        
    RefPtr<Node> container = position.containerNode();
    if (!container)
        return nullptr;
    RefPtr<Element> ancestor = container->shadowHost();
    return ancestor && ancestor->isTextField() ? downcast<HTMLTextFormControlElement>(ancestor.get()) : nullptr;
}

String HTMLTextFormControlElement::directionForFormData() const
{
    auto direction = [this] {
        for (auto& element : lineageOfType<HTMLElement>(*this)) {
            auto& value = element.attributeWithoutSynchronization(dirAttr);
            if (equalLettersIgnoringASCIICase(value, "rtl"_s))
                return TextDirection::RTL;
            if (equalLettersIgnoringASCIICase(value, "ltr"_s))
                return TextDirection::LTR;
            if (equalLettersIgnoringASCIICase(value, "auto"_s))
                return element.directionalityIfDirIsAuto().value_or(TextDirection::LTR);
        }
        return TextDirection::LTR;
    }();

    return direction == TextDirection::LTR ? "ltr"_s : "rtl"_s;
}

ExceptionOr<void> HTMLTextFormControlElement::setMaxLength(int maxLength)
{
    if (maxLength < 0 || (m_minLength >= 0 && maxLength < m_minLength))
        return Exception { ExceptionCode::IndexSizeError };
    setIntegralAttribute(maxlengthAttr, maxLength);
    return { };
}

ExceptionOr<void> HTMLTextFormControlElement::setMinLength(int minLength)
{
    if (minLength < 0 || (m_maxLength >= 0 && minLength > m_maxLength))
        return Exception { ExceptionCode::IndexSizeError };
    setIntegralAttribute(minlengthAttr, minLength);
    return { };
}

void HTMLTextFormControlElement::adjustInnerTextStyle(const RenderStyle& parentStyle, RenderStyle& textBlockStyle) const
{
    // The inner block, if present, always has its direction set to LTR,
    // so we need to inherit the direction and unicode-bidi style from the element.
    textBlockStyle.setDirection(parentStyle.direction());
    textBlockStyle.setUnicodeBidi(parentStyle.unicodeBidi());

    if (auto innerText = innerTextElement()) {
        if (auto* properties = innerText->presentationalHintStyle()) {
            if (auto value = properties->propertyAsValueID(CSSPropertyWebkitUserModify))
                textBlockStyle.setUserModify(fromCSSValueID<UserModify>(*value));
        }
    }

#if PLATFORM(IOS_FAMILY)
    if (textBlockStyle.textSecurity() != TextSecurity::None && !textBlockStyle.isLeftToRightDirection()) {
        // Preserve the alignment but force the direction to LTR so that the last-typed, unmasked character
        // (which cannot have RTL directionality) will appear to the right of the masked characters. See <rdar://problem/7024375>.
        
        switch (textBlockStyle.textAlign()) {
        case TextAlignMode::Start:
        case TextAlignMode::Justify:
            textBlockStyle.setTextAlign(TextAlignMode::Right);
            break;
        case TextAlignMode::End:
            textBlockStyle.setTextAlign(TextAlignMode::Left);
            break;
        case TextAlignMode::Left:
        case TextAlignMode::Right:
        case TextAlignMode::Center:
        case TextAlignMode::WebKitLeft:
        case TextAlignMode::WebKitRight:
        case TextAlignMode::WebKitCenter:
            break;
        }

        textBlockStyle.setDirection(TextDirection::LTR);
    }
#endif
}

} // namespace WebCore
