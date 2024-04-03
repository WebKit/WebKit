/*
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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
#include "RenderCounter.h"

#include "CSSCounterStyleRegistry.h"
#include "CounterDirectives.h"
#include "CounterNode.h"
#include "Document.h"
#include "ElementInlines.h"
#include "ElementTraversal.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "PseudoElement.h"
#include "RenderElementInlines.h"
#include "RenderListItem.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(TREE_DEBUGGING)
#include <stdio.h>
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderCounter);

using CounterMap = HashMap<AtomString, Ref<CounterNode>>;
using CounterMaps = SingleThreadWeakHashMap<RenderElement, std::unique_ptr<CounterMap>>;

static CounterNode* makeCounterNode(RenderElement&, const AtomString& identifier, bool alwaysCreateCounter);

static CounterMaps& counterMaps()
{
    static NeverDestroyed<CounterMaps> staticCounterMaps;
    return staticCounterMaps;
}

static Element* ancestorStyleContainmentObject(const Element& element)
{
    auto* pseudoElement = dynamicDowncast<PseudoElement>(element);
    Element* ancestor = pseudoElement ? pseudoElement->hostElement() : element.parentElement();
    while (ancestor) {
        if (auto* style = ancestor->existingComputedStyle()) {
            if (style->containsStyle())
                break;
        }
        // FIXME: this should use parentInComposedTree but for now matches the rest of RenderCounter.
        ancestor = ancestor->parentElement();
    }
    return ancestor;
}

// This function processes the renderer tree in the order of the DOM tree
// including pseudo elements as defined in CSS 2.1. This method will always
// return either a previous element within the same contain: style scope or nullptr.
static RenderElement* previousInPreOrderRespectingContainment(const RenderElement& renderer)
{
    ASSERT(renderer.element());
    Element* previous = ElementTraversal::previousIncludingPseudo(*renderer.element());
    Element* styleContainmentAncestor = ancestorStyleContainmentObject(*renderer.element());

    while (previous) {
        while (previous && !previous->renderer())
            previous = ElementTraversal::previousIncludingPseudo(*previous, styleContainmentAncestor);
        if (!previous)
            break;
        auto* previousStyleContainmentAncestor = ancestorStyleContainmentObject(*previous);
        // If the candidate's containment ancestor is the same as elements, then
        // that's a valid candidate.
        if (previousStyleContainmentAncestor == styleContainmentAncestor)
            return previous->renderer();

        // If the candidate does have a containment ancestor, it could be
        // that we entered a new sub-containment. Try again starting from the
        // contain ancestor.
        previous = previousStyleContainmentAncestor;
    }
    return nullptr;
}

static inline Element* parentOrPseudoHostElement(const RenderElement& renderer)
{
    if (renderer.isPseudoElement())
        return renderer.generatingElement();
    return renderer.element() ? renderer.element()->parentElement() : nullptr;
}

static Element* previousSiblingOrParentElement(const Element& element)
{
    if (auto* previous = ElementTraversal::pseudoAwarePreviousSibling(element)) {
        while (previous && !previous->renderer())
            previous = ElementTraversal::pseudoAwarePreviousSibling(*previous);

        if (previous)
            return previous;
    }

    if (auto* pseudoElement = dynamicDowncast<PseudoElement>(element)) {
        auto* hostElement = pseudoElement->hostElement();
        ASSERT(hostElement);
        if (hostElement->renderer())
            return hostElement;
        return previousSiblingOrParentElement(*hostElement);
    }
    
    auto* parent = element.parentElement();
    if (parent && !parent->renderer())
        parent = previousSiblingOrParentElement(*parent);
    if (parent && parent->renderer() && parent->renderer()->style().containsStyle())
        return nullptr;
    return parent;
}

// This function processes the renderer tree in the order of the DOM tree
// including pseudo elements as defined in CSS 2.1.
static RenderElement* previousSiblingOrParent(const RenderElement& renderer)
{
    ASSERT(renderer.element());

    auto* previous = previousSiblingOrParentElement(*renderer.element());
    return previous ? previous->renderer() : nullptr;
}

static inline bool areRenderersElementsSiblings(const RenderElement& first, const RenderElement& second)
{
    return parentOrPseudoHostElement(first) == parentOrPseudoHostElement(second);
}

// This function processes the renderer tree in the order of the DOM tree
// including pseudo elements as defined in CSS 2.1.
static RenderElement* nextInPreOrder(const RenderElement& renderer, const Element* stayWithin, bool skipDescendants = false)
{
    ASSERT(renderer.element());
    Element& self = *renderer.element();
    Element* next = skipDescendants ? ElementTraversal::nextIncludingPseudoSkippingChildren(self, stayWithin) : ElementTraversal::nextIncludingPseudo(self, stayWithin);
    while (next && !next->renderer())
        next = skipDescendants ? ElementTraversal::nextIncludingPseudoSkippingChildren(*next, stayWithin) : ElementTraversal::nextIncludingPseudo(*next, stayWithin);
    return next ? next->renderer() : nullptr;
}

static CounterDirectives listItemCounterDirectives(RenderElement& renderer)
{
    if (auto* item = dynamicDowncast<RenderListItem>(renderer)) {
        return {
            .resetValue = std::nullopt,
            .incrementValue = item->isInReversedOrderedList() ? -1 : 1,
            .setValue = std::nullopt
        };
    }
    if (auto element = renderer.element()) {
        if (auto* list = dynamicDowncast<HTMLOListElement>(*element)) {
            return {
                .resetValue = list->start(),
                .incrementValue = list->isReversed() ? 1 : -1,
                .setValue = std::nullopt
            };
        }
        if (isHTMLListElement(*element))
            return {
                .resetValue = 0,
                .incrementValue = std::nullopt,
                .setValue = std::nullopt
            };
    }
    return { };
}

struct CounterPlan {
    OptionSet<CounterNode::Type> type;
    int value;
};

static std::optional<CounterPlan> planCounter(RenderElement& renderer, const AtomString& identifier)
{
    // We must have a generating node or else we cannot have a counter.
    Element* generatingElement = renderer.generatingElement();
    if (!generatingElement)
        return std::nullopt;

    auto& style = renderer.style();

    switch (style.pseudoElementType()) {
    case PseudoId::None:
        // Sometimes elements have more then one renderer. Only the first one gets the counter
        // LayoutTests/http/tests/css/counter-crash.html
        if (generatingElement->renderer() != &renderer)
            return std::nullopt;
        break;
    case PseudoId::Before:
    case PseudoId::After:
        break;
    default:
        return std::nullopt; // Counters are forbidden from all other pseudo elements.
    }

    auto directives = style.counterDirectives().map.get(identifier);

    if (identifier == "list-item"_s) {
        auto itemDirectives = listItemCounterDirectives(renderer);
        if (!directives.resetValue)
            directives.resetValue = itemDirectives.resetValue;
        if (!directives.incrementValue)
            directives.incrementValue = itemDirectives.incrementValue;
        if (!directives.setValue)
            directives.setValue = itemDirectives.setValue;
    }

    OptionSet<CounterNode::Type> type;

    if (directives.setValue)
        type.add(CounterNode::Type::Set);
    if (directives.resetValue)
        type.add(CounterNode::Type::Reset);
    if (directives.incrementValue)
        type.add(CounterNode::Type::Increment);

    if (directives.setValue)
        return CounterPlan { type, *directives.setValue };
    if (directives.resetValue)
        return CounterPlan { type, saturatedSum<int>(*directives.resetValue, directives.incrementValue.value_or(0)) };
    if (directives.incrementValue)
        return CounterPlan { type, *directives.incrementValue };
    return std::nullopt;
}

// - Finds the insertion point for the counter described by counterOwner, isReset and 
// identifier in the CounterNode tree for identifier and sets parent and
// previousSibling accordingly.
// - The function returns true if the counter whose insertion point is searched is NOT
// the root of the tree.
// - The root of the tree is a counter reference that is not in the scope of any other
// counter with the same identifier.
// - All the counter references with the same identifier as this one that are in
// children or subsequent siblings of the renderer that owns the root of the tree
// form the rest of of the nodes of the tree.
// - The root of the tree is always a reset type reference.
// - A subtree rooted at any reset node in the tree is equivalent to all counter 
// references that are in the scope of the counter or nested counter defined by that
// reset node.
// - Non-reset CounterNodes cannot have descendants.

struct CounterInsertionPoint {
    RefPtr<CounterNode> parent;
    RefPtr<CounterNode> previousSibling;
};

static CounterInsertionPoint findPlaceForCounter(RenderElement& counterOwner, const AtomString& identifier, OptionSet<CounterNode::Type> type)
{
    // We cannot stop searching for counters with the same identifier before we also
    // check this renderer, because it may affect the positioning in the tree of our counter.
    RenderElement* searchEndRenderer = previousSiblingOrParent(counterOwner);
    // We check renderers in preOrder from the renderer that our counter is attached to
    // towards the begining of the document for counters with the same identifier as the one
    // we are trying to find a place for. This is the next renderer to be checked.
    RenderElement* currentRenderer = previousInPreOrderRespectingContainment(counterOwner);
    RefPtr<CounterNode> previousSibling;

    // Establish counter nodes previous to currentRenderer in order that calling
    // makeCounterNode on currentRenderer does not recurse into this function.
    if (currentRenderer && !currentRenderer->hasCounterNodeMap()) {
        Vector<RenderElement*> previousRenderers;
        RenderElement* current = currentRenderer;
        while (current && !current->hasCounterNodeMap()) {
            if (!current->style().counterDirectives().map.isEmpty())
                previousRenderers.append(current);
            current = previousInPreOrderRespectingContainment(*current);
        }
        while (!previousRenderers.isEmpty())
            makeCounterNode(*previousRenderers.takeLast(), identifier, false);
    }

    bool isReset = type.contains(CounterNode::Type::Reset);
    while (currentRenderer) {
        auto currentCounter = makeCounterNode(*currentRenderer, identifier, false);
        if (searchEndRenderer == currentRenderer) {
            // We may be at the end of our search.
            if (currentCounter) {
                // We have a suitable counter on the EndSearchRenderer.
                if (previousSibling) {
                    // But we already found another counter that we come after.
                    if (currentCounter->actsAsReset()) {
                        // We found a reset counter that is on a renderer that is a sibling of ours or a parent.
                        if (isReset && areRenderersElementsSiblings(*currentRenderer, counterOwner)) {
                            // We are also a reset counter and the previous reset was on a sibling renderer
                            // hence we are the next sibling of that counter if that reset is not a root or
                            // we are a root node if that reset is a root.
                            auto* parent = currentCounter->parent();
                            return { parent, parent ? currentCounter : nullptr };
                        }
                        // We are not a reset node or the previous reset must be on an ancestor of our owner renderer
                        // hence we must be a child of that reset counter.
                        // In some cases renders can be reparented (ex. nodes inside a table but not in a column or row).
                        // In these cases the identified previousSibling will be invalid as its parent is different from
                        // our identified parent.
                        if (previousSibling->parent() != currentCounter)
                            previousSibling = nullptr;
                        return { currentCounter, WTFMove(previousSibling) };
                    }
                    // CurrentCounter, the counter at the EndSearchRenderer, is not reset.
                    if (!isReset || !areRenderersElementsSiblings(*currentRenderer, counterOwner)) {
                        // If the node we are placing is not reset or we have found a counter that is attached
                        // to an ancestor of the placed counter's owner renderer we know we are a sibling of that node.
                        if (currentCounter->parent() != previousSibling->parent())
                            return { };
                        return { currentCounter->parent(), WTFMove(previousSibling) };
                    }
                } else { 
                    // We are at the potential end of the search, but we had no previous sibling candidate
                    // In this case we follow pretty much the same logic as above but no ASSERTs about 
                    // previousSibling, and when we are a sibling of the end counter we must set previousSibling
                    // to currentCounter.
                    if (currentCounter->actsAsReset()) {
                        if (isReset && areRenderersElementsSiblings(*currentRenderer, counterOwner))
                            return { currentCounter->parent(), currentCounter };
                        return { currentCounter, WTFMove(previousSibling) };
                    }
                    if (!isReset || !areRenderersElementsSiblings(*currentRenderer, counterOwner))
                        return { currentCounter->parent(), currentCounter };
                    previousSibling = currentCounter;
                }
            }
            // We come here if the previous sibling or parent of our owner renderer had no
            // good counter, or we are a reset node and the counter on the previous sibling
            // of our owner renderer was not a reset counter.
            // Set a new goal for the end of the search.
            searchEndRenderer = previousSiblingOrParent(*currentRenderer);
        } else {
            // We are searching descendants of a previous sibling of the renderer that the
            // counter being placed is attached to.
            if (currentCounter) {
                // We found a suitable counter.
                if (previousSibling) {
                    // Since we had a suitable previous counter before, we should only consider this one as our 
                    // previousSibling if it is a reset counter and hence the current previousSibling is its child.
                    if (currentCounter->actsAsReset()) {
                        previousSibling = currentCounter;
                        // We are no longer interested in previous siblings of the currentRenderer or their children
                        // as counters they may have attached cannot be the previous sibling of the counter we are placing.
                        auto* parent = parentOrPseudoHostElement(*currentRenderer);
                        currentRenderer = parent ? parent->renderer() : nullptr;
                        continue;
                    }
                } else
                    previousSibling = currentCounter;
                currentRenderer = previousSiblingOrParent(*currentRenderer);
                continue;
            }
        }
        // This function is designed so that the same test is not done twice in an iteration, except for this one
        // which may be done twice in some cases. Rearranging the decision points though, to accommodate this 
        // performance improvement would create more code duplication than is worthwhile in my oppinion and may further
        // impede the readability of this already complex algorithm.
        if (previousSibling)
            currentRenderer = previousSiblingOrParent(*currentRenderer);
        else
            currentRenderer = previousInPreOrderRespectingContainment(*currentRenderer);
    }
    return { };
}

static CounterNode* makeCounterNode(RenderElement& renderer, const AtomString& identifier, bool alwaysCreateCounter)
{
    if (renderer.hasCounterNodeMap()) {
        ASSERT(counterMaps().contains(renderer));
        if (auto* node = counterMaps().find(renderer)->value->get(identifier))
            return node;
    }

    auto plan = planCounter(renderer, identifier);
    if (!plan && !alwaysCreateCounter)
        return nullptr;

    auto& maps = counterMaps();

    auto newNode = CounterNode::create(renderer, plan ? plan->type : OptionSet<CounterNode::Type> { }, plan ? plan->value : 0);

    auto place = findPlaceForCounter(renderer, identifier, plan ? plan->type : OptionSet<CounterNode::Type> { });
    if (place.parent)
        place.parent->insertAfter(newNode, place.previousSibling.get(), identifier);

    maps.add(renderer, makeUnique<CounterMap>()).iterator->value->add(identifier, newNode.copyRef());
    renderer.setHasCounterNodeMap(true);

    if (newNode->parent() || renderer.shouldApplyStyleContainment())
        return newNode.ptr();

    // Check if some nodes that were previously root nodes should become children of this node now.
    auto* currentRenderer = &renderer;
    auto* stayWithin = parentOrPseudoHostElement(renderer);
    bool skipDescendants = false;
    while ((currentRenderer = nextInPreOrder(*currentRenderer, stayWithin, skipDescendants))) {
        skipDescendants = currentRenderer->shouldApplyStyleContainment();
        if (!currentRenderer->hasCounterNodeMap())
            continue;
        RefPtr currentCounter = maps.find(*currentRenderer)->value->get(identifier);
        if (!currentCounter)
            continue;
        skipDescendants = true;
        if (currentCounter->parent())
            continue;
        if (stayWithin == parentOrPseudoHostElement(*currentRenderer) && currentCounter->hasResetType())
            break;
        newNode->insertAfter(*currentCounter, newNode->lastChild(), identifier);
    }

    return newNode.ptr();
}

RenderCounter::RenderCounter(Document& document, const CounterContent& counter)
    : RenderText(Type::Counter, document, emptyString())
    , m_counter(counter)
{
    ASSERT(isRenderCounter());
    view().addCounterNeedingUpdate(*this);
}

RenderCounter::~RenderCounter()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderCounter::willBeDestroyed()
{
    if (m_counterNode) {
        m_counterNode->removeRenderer(*this);
        ASSERT(!m_counterNode);
    }
    
    RenderText::willBeDestroyed();
}

ASCIILiteral RenderCounter::renderName() const
{
    return "RenderCounter"_s;
}

String RenderCounter::originalText() const
{
    if (!m_counterNode)
        return emptyString();

    RefPtr child = m_counterNode.get();
    int value = child->actsAsReset() ? child->value() : child->countInParent();

    auto counterText = [](const ListStyleType& styleType, int value, CSSCounterStyle* counterStyle) {
        if (styleType.type == ListStyleType::Type::None)
            return emptyString();

        if (styleType.type == ListStyleType::Type::CounterStyle) {
            ASSERT(counterStyle);
            return counterStyle->text(value);
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    };
    auto counterStyle = this->counterStyle();
    String text = counterText(m_counter.listStyleType(), value, counterStyle.get());

    if (!m_counter.separator().isNull()) {
        if (!child->actsAsReset())
            child = child->parent();
        while (CounterNode* parent = child->parent()) {
            text = counterText(m_counter.listStyleType(), child->countInParent(), counterStyle.get())
                + m_counter.separator() + text;
            child = parent;
        }
    }

    return text;
}

void RenderCounter::updateCounter()
{
    if (!m_counterNode) {
        RenderElement* beforeAfterContainer = parent();
        while (true) {
            if (!beforeAfterContainer)
                return;
            if (!beforeAfterContainer->isAnonymous() && !beforeAfterContainer->isPseudoElement())
                return;
            auto containerStyle = beforeAfterContainer->style().pseudoElementType();
            if (containerStyle == PseudoId::Before || containerStyle == PseudoId::After)
                break;
            beforeAfterContainer = beforeAfterContainer->parent();
        }
        makeCounterNode(*beforeAfterContainer, m_counter.identifier(), true)->addRenderer(const_cast<RenderCounter&>(*this));
    }

    setText(originalText(), true);
}

static void destroyCounterNodeWithoutMapRemoval(const AtomString& identifier, CounterNode& node)
{
    RefPtr<CounterNode> previous;
    for (RefPtr<CounterNode> child = node.lastDescendant(); child && child != &node; child = WTFMove(previous)) {
        previous = child->previousInPreOrder();
        child->parent()->removeChild(*child);
        ASSERT(counterMaps().find(child->owner())->value->get(identifier) == child);
        counterMaps().find(child->owner())->value->remove(identifier);
    }
    if (auto* parent = node.parent())
        parent->removeChild(node);
}

void RenderCounter::destroyCounterNodes(RenderElement& owner)
{
    ASSERT(owner.hasCounterNodeMap());
    auto& maps = counterMaps();
    ASSERT(maps.contains(owner));
    auto counterMap = maps.take(owner);
    for (auto& counterMapEntry : *counterMap)
        destroyCounterNodeWithoutMapRemoval(counterMapEntry.key, counterMapEntry.value);
    owner.setHasCounterNodeMap(false);
}

void RenderCounter::destroyCounterNode(RenderElement& owner, const AtomString& identifier)
{
    auto map = counterMaps().find(owner);
    if (map == counterMaps().end())
        return;
    auto node = map->value->take(identifier);
    if (!node)
        return;
    destroyCounterNodeWithoutMapRemoval(identifier, *node);
    // We do not delete the map here even if it is now empty because we expect to
    // reuse it. In order for a renderer to lose all its counters permanently,
    // a style change for the renderer involving removal of all counter
    // directives must occur, in which case, RenderCounter::destroyCounterNodes()
    // will be called.
}

void RenderCounter::rendererStyleChangedSlowCase(RenderElement& renderer, const RenderStyle* oldStyle, const RenderStyle& newStyle)
{
    Element* element = renderer.generatingElement();
    if (!element || !element->renderer())
        return; // cannot have generated content or if it can have, it will be handled during attaching

    const CounterDirectiveMap* oldCounterDirectives;
    if (oldStyle && !(oldCounterDirectives = &oldStyle->counterDirectives())->map.isEmpty()) {
        if (auto& newCounterDirectives = newStyle.counterDirectives().map; !newCounterDirectives.isEmpty()) {
            for (auto& keyValue : newCounterDirectives) {
                auto existingEntry = oldCounterDirectives->map.find(keyValue.key);
                if (existingEntry != oldCounterDirectives->map.end()) {
                    if (existingEntry->value == keyValue.value)
                        continue;
                    RenderCounter::destroyCounterNode(renderer, keyValue.key);
                }
                // We must create this node here, because the changed node may be a node with no display such as
                // as those created by the increment or reset directives and the re-layout that will happen will
                // not catch the change if the node had no children.
                makeCounterNode(renderer, keyValue.key, false);
            }
            // Destroying old counters that do not exist in the new counterDirective map.
            for (auto& key : oldCounterDirectives->map.keys()) {
                if (!newCounterDirectives.contains(key))
                    RenderCounter::destroyCounterNode(renderer, key);
            }
        } else {
            if (renderer.hasCounterNodeMap())
                RenderCounter::destroyCounterNodes(renderer);
        }
    } else {
        if (renderer.hasCounterNodeMap())
            RenderCounter::destroyCounterNodes(renderer);

        for (auto& key : newStyle.counterDirectives().map.keys()) {
            // We must create this node here, because the added node may be a node with no display such as
            // as those created by the increment or reset directives and the re-layout that will happen will
            // not catch the change if the node had no children.
            makeCounterNode(renderer, key, false);
        }
    }
}

RefPtr<CSSCounterStyle> RenderCounter::counterStyle() const
{
    return document().counterStyleRegistry().resolvedCounterStyle(m_counter.listStyleType());
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showCounterRendererTree(const WebCore::RenderObject* renderer, const char* counterName)
{
    if (!renderer)
        return;
    auto* root = renderer;
    while (root->parent())
        root = root->parent();

    auto identifier = AtomString::fromLatin1(counterName);
    for (auto* current = root; current; current = current->nextInPreOrder()) {
        auto* element = dynamicDowncast<WebCore::RenderElement>(*current);
        if (!element)
            continue;
        fprintf(stderr, "%c", (current == renderer) ? '*' : ' ');
        for (auto* ancestor = current; ancestor && ancestor != root; ancestor = ancestor->parent())
            fprintf(stderr, "    ");
        fprintf(stderr, "%p N:%p P:%p PS:%p NS:%p C:%p\n",
            current, current->node(), current->parent(), current->previousSibling(),
            current->nextSibling(), element->hasCounterNodeMap() ?
            counterName ? WebCore::counterMaps().find(*downcast<WebCore::RenderElement>(current))->value->get(identifier) : (WebCore::CounterNode*)1 : (WebCore::CounterNode*)0);
    }
    fflush(stderr);
}

#endif // NDEBUG
