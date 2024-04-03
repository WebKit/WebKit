/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "BidiRun.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderInline.h"
#include "RenderText.h"
#include "UnicodeBidi.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

struct BidiIsolatedRun {
    BidiIsolatedRun(RenderObject& object, unsigned position, RenderElement& root, BidiRun& runToReplace)
        : object(object)
        , root(root)
        , runToReplace(runToReplace)
        , position(position)
    {
    }

    RenderObject& object;
    RenderElement& root;
    BidiRun& runToReplace;
    unsigned position;
};

// This class is used to RenderInline subtrees, stepping by character within the
// text children. LegacyInlineIterator will use next to find the next RenderText
// optionally notifying a BidiResolver every time it steps into/out of a RenderInline.
class LegacyInlineIterator {
public:
    LegacyInlineIterator()
    {
    }

    LegacyInlineIterator(RenderElement* root, RenderObject* o, unsigned p)
        : m_root(root)
        , m_renderer(o)
        , m_pos(p)
        , m_refersToEndOfPreviousNode(false)
    {
    }

    void clear()
    {
        setRenderer(nullptr);
        setOffset(0);
        setNextBreakablePosition(std::numeric_limits<unsigned>::max());
    }
    void moveToStartOf(RenderObject& object)
    {
        moveTo(object, 0);
    }

    void moveTo(RenderObject& object, unsigned offset, std::optional<unsigned> nextBreak = std::optional<unsigned>())
    {
        setRenderer(&object);
        setOffset(offset);
        setNextBreakablePosition(nextBreak);
    }

    RenderObject* renderer() const { return m_renderer; }
    void setRenderer(RenderObject* renderer) { m_renderer = renderer; }
    unsigned offset() const { return m_pos; }
    void setOffset(unsigned position);
    RenderElement* root() const { return m_root; }
    std::optional<unsigned> nextBreakablePosition() const { return m_nextBreakablePosition; }
    void setNextBreakablePosition(std::optional<unsigned> position) { m_nextBreakablePosition = position; }
    bool refersToEndOfPreviousNode() const { return m_refersToEndOfPreviousNode; }
    void setRefersToEndOfPreviousNode();

    void fastIncrementInTextNode();
    void incrementByCodePointInTextNode();
    void increment(InlineBidiResolver* = nullptr);
    void fastDecrement();
    bool atEnd() const;

    inline bool atTextParagraphSeparator() const;
    inline bool atParagraphSeparator() const;

    UChar current() const;
    UChar previousInSameNode() const;
    ALWAYS_INLINE UCharDirection direction() const;

private:
    UChar characterAt(unsigned) const;

    UCharDirection surrogateTextDirection(UChar currentCodeUnit) const;

    RenderElement* m_root { nullptr };
    RenderObject* m_renderer { nullptr };

    std::optional<unsigned> m_nextBreakablePosition;
    unsigned m_pos { 0 };

    // There are a couple places where we want to decrement an LegacyInlineIterator.
    // Usually this take the form of decrementing m_pos; however, m_pos might be 0.
    // However, we shouldn't ever need to decrement an LegacyInlineIterator more than
    // once, so rather than implementing a decrement() function which traverses
    // nodes, we can simply keep track of this state and handle it.
    bool m_refersToEndOfPreviousNode { false };
};

inline bool operator==(const LegacyInlineIterator& it1, const LegacyInlineIterator& it2)
{
    return it1.offset() == it2.offset() && it1.renderer() == it2.renderer();
}

static inline UCharDirection embedCharFromDirection(TextDirection direction, UnicodeBidi unicodeBidi)
{
    if (unicodeBidi == UnicodeBidi::Embed)
        return direction == TextDirection::RTL ? U_RIGHT_TO_LEFT_EMBEDDING : U_LEFT_TO_RIGHT_EMBEDDING;
    return direction == TextDirection::RTL ? U_RIGHT_TO_LEFT_OVERRIDE : U_LEFT_TO_RIGHT_OVERRIDE;
}

template <class Observer>
static inline void notifyObserverEnteredObject(Observer* observer, RenderObject* object)
{
    if (!observer || !object || !object->isRenderInline())
        return;

    const RenderStyle& style = object->style();
    auto unicodeBidi = style.unicodeBidi();
    if (unicodeBidi == UnicodeBidi::Normal) {
        // http://dev.w3.org/csswg/css3-writing-modes/#unicode-bidi
        // "The element does not open an additional level of embedding with respect to the bidirectional algorithm."
        // Thus we ignore any possible dir= attribute on the span.
        return;
    }
    if (isIsolated(unicodeBidi)) {
        // Make sure that explicit embeddings are committed before we enter the isolated content.
        observer->commitExplicitEmbedding();
        observer->enterIsolate();
        // Embedding/Override characters implied by dir= will be handled when
        // we process the isolated span, not when laying out the "parent" run.
        return;
    }

    if (!observer->inIsolate())
        observer->embed(embedCharFromDirection(style.direction(), unicodeBidi), FromStyleOrDOM);
}

template <class Observer>
static inline void notifyObserverWillExitObject(Observer* observer, RenderObject* object)
{
    if (!observer || !object || !object->isRenderInline())
        return;

    auto unicodeBidi = object->style().unicodeBidi();
    if (unicodeBidi == UnicodeBidi::Normal)
        return; // Nothing to do for unicode-bidi: normal
    if (isIsolated(unicodeBidi)) {
        observer->exitIsolate();
        return;
    }

    // Otherwise we pop any embed/override character we added when we opened this tag.
    if (!observer->inIsolate())
        observer->embed(U_POP_DIRECTIONAL_FORMAT, FromStyleOrDOM);
}

static inline bool isIteratorTarget(RenderObject* object)
{
    ASSERT(object); // The iterator will of course return 0, but its not an expected argument to this function.
    return object->isRenderTextOrLineBreak() || object->isFloating() || object->isOutOfFlowPositioned() || object->isReplacedOrInlineBlock();
}

template <class Observer>
static inline RenderObject* nextInlineRendererSkippingEmpty(RenderElement& root, RenderObject* current, Observer* observer)
{
    RenderObject* next = nullptr;

    while (current) {
        next = nullptr;
        if (!isIteratorTarget(current)) {
            next = downcast<RenderElement>(*current).firstChild();
            notifyObserverEnteredObject(observer, next);
        }

        if (!next) {
            while (current && current != &root) {
                notifyObserverWillExitObject(observer, current);

                next = current->nextSibling();
                if (next) {
                    notifyObserverEnteredObject(observer, next);
                    break;
                }

                current = current->parent();
            }
        }

        if (!next)
            break;

        if (isIteratorTarget(next))
            break;

        auto* renderInline = dynamicDowncast<RenderInline>(*next);
        if (renderInline && isEmptyInline(*renderInline))
            break;

        current = next;
    }

    return next;
}

// This makes callers cleaner as they don't have to specify a type for the observer when not providing one.
static inline RenderObject* nextInlineRendererSkippingEmpty(RenderElement& root, RenderObject* current)
{
    InlineBidiResolver* observer = nullptr;
    return nextInlineRendererSkippingEmpty(root, current, observer);
}

static inline RenderObject* firstInlineRendererSkippingEmpty(RenderElement& root, InlineBidiResolver* resolver = nullptr)
{
    RenderObject* renderer = root.firstChild();
    if (!renderer)
        return nullptr;

    if (auto* renderInline = dynamicDowncast<RenderInline>(*renderer)) {
        notifyObserverEnteredObject(resolver, renderer);
        if (!isEmptyInline(*renderInline))
            renderer = nextInlineRendererSkippingEmpty(root, renderer, resolver);
        else {
            // Never skip empty inlines.
            if (resolver)
                resolver->commitExplicitEmbedding();
            return renderer;
        }
    }

    // FIXME: Unify this with the next call above.
    if (renderer && !isIteratorTarget(renderer))
        renderer = nextInlineRendererSkippingEmpty(root, renderer, resolver);

    if (resolver)
        resolver->commitExplicitEmbedding();
    return renderer;
}

inline void LegacyInlineIterator::fastIncrementInTextNode()
{
    ASSERT(m_renderer);
    ASSERT(m_pos <= downcast<RenderText>(*m_renderer).text().length());
    ++m_pos;
}

inline void LegacyInlineIterator::incrementByCodePointInTextNode()
{
    ASSERT(m_renderer);
    const auto& text = downcast<RenderText>(*m_renderer).text();
    ASSERT(m_pos < text.length());
    if (text.is8Bit()) {
        ++m_pos;
        return;
    }
    char32_t character;
    U16_NEXT(text.characters16(), m_pos, text.length(), character);
}

inline void LegacyInlineIterator::setOffset(unsigned position)
{
    ASSERT(position <= UINT_MAX - 10); // Sanity check
    m_pos = position;
}

inline void LegacyInlineIterator::setRefersToEndOfPreviousNode()
{
    ASSERT(!m_pos);
    ASSERT(!m_refersToEndOfPreviousNode);
    m_refersToEndOfPreviousNode = true;
}


inline void LegacyInlineIterator::increment(InlineBidiResolver* resolver)
{
    if (!m_renderer)
        return;
    if (auto* textRenderer = dynamicDowncast<RenderText>(*m_renderer)) {
        fastIncrementInTextNode();
        if (m_pos < textRenderer->text().length())
            return;
    }
    // next can return nullptr
    RenderObject* next = nextInlineRendererSkippingEmpty(*m_root, m_renderer, resolver);
    if (next)
        moveToStartOf(*next);
    else
        clear();
}

inline void LegacyInlineIterator::fastDecrement()
{
    ASSERT(!refersToEndOfPreviousNode());
    if (m_pos)
        setOffset(m_pos - 1);
    else
        setRefersToEndOfPreviousNode();
}

inline bool LegacyInlineIterator::atEnd() const
{
    return !m_renderer;
}

inline UChar LegacyInlineIterator::characterAt(unsigned index) const
{
    auto* textRenderer = dynamicDowncast<RenderText>(m_renderer);
    if (!textRenderer)
        return 0;
    return textRenderer->characterAt(index);
}

inline UChar LegacyInlineIterator::current() const
{
    return characterAt(m_pos);
}

inline UChar LegacyInlineIterator::previousInSameNode() const
{
    return characterAt(m_pos - 1);
}

ALWAYS_INLINE UCharDirection LegacyInlineIterator::direction() const
{
    if (UNLIKELY(!m_renderer))
        return U_OTHER_NEUTRAL;

    if (auto* textRenderer = dynamicDowncast<RenderText>(*m_renderer); LIKELY(textRenderer)) {
        UChar codeUnit = textRenderer->characterAt(m_pos);
        if (LIKELY(U16_IS_SINGLE(codeUnit)))
            return u_charDirection(codeUnit);
        return surrogateTextDirection(codeUnit);
    }

    if (m_renderer->isRenderListMarker())
        return m_renderer->style().isLeftToRightDirection() ? U_LEFT_TO_RIGHT : U_RIGHT_TO_LEFT;

    return U_OTHER_NEUTRAL;
}

template<>
inline void InlineBidiResolver::incrementInternal()
{
    m_current.increment(this);
}

static inline bool isIsolatedInline(RenderObject& object)
{
    return object.isRenderInline() && isIsolated(object.style().unicodeBidi());
}

static inline RenderObject* highestContainingIsolateWithinRoot(RenderObject& initialObject, RenderObject* root)
{
    RenderObject* containingIsolateObject = nullptr;
    for (RenderObject* object = &initialObject; object && object != root; object = object->parent()) {
        if (isIsolatedInline(*object))
            containingIsolateObject = object;
    }
    return containingIsolateObject;
}

static inline unsigned numberOfIsolateAncestors(const LegacyInlineIterator& iter)
{
    unsigned count = 0;
    typedef RenderObject* RenderObjectPtr;
    for (RenderObjectPtr object = iter.renderer(), root = iter.root(); object && object != root; object = object->parent()) {
        if (isIsolatedInline(*object))
            count++;
    }
    return count;
}

// FIXME: This belongs on InlineBidiResolver, except it's a template specialization
// of BidiResolver which knows nothing about RenderObjects.
static inline void addPlaceholderRunForIsolatedInline(InlineBidiResolver& resolver, RenderObject& obj, unsigned pos, RenderElement& root)
{
    std::unique_ptr<BidiRun> isolatedRun = makeUnique<BidiRun>(pos, pos, obj, resolver.context(), resolver.dir());
    // FIXME: isolatedRuns() could be a hash of object->run and then we could cheaply
    // ASSERT here that we didn't create multiple objects for the same inline.
    resolver.setWhitespaceCollapsingTransitionForIsolatedRun(*isolatedRun, resolver.whitespaceCollapsingState().currentTransition());
    resolver.isolatedRuns().append(BidiIsolatedRun(obj, pos, root, *isolatedRun));
    resolver.runs().appendRun(WTFMove(isolatedRun));
}

class IsolateTracker {
public:
    explicit IsolateTracker(unsigned nestedIsolateCount)
        : m_nestedIsolateCount(nestedIsolateCount)
        , m_haveAddedFakeRunForRootIsolate(false)
    {
    }

    void enterIsolate() { m_nestedIsolateCount++; }
    void exitIsolate()
    {
        ASSERT(m_nestedIsolateCount >= 1);
        m_nestedIsolateCount--;
        if (!inIsolate())
            m_haveAddedFakeRunForRootIsolate = false;
    }
    bool inIsolate() const { return m_nestedIsolateCount; }

    // We don't care if we encounter bidi directional overrides.
    void embed(UCharDirection, BidiEmbeddingSource) { }
    void commitExplicitEmbedding() { }

    inline void addFakeRunIfNecessary(RenderObject&, unsigned position, unsigned end, RenderElement& root, InlineBidiResolver&);

private:
    unsigned m_nestedIsolateCount;
    bool m_haveAddedFakeRunForRootIsolate;
};

template<>
inline bool InlineBidiResolver::needsContinuePastEndInternal() const
{
    // We don't collect runs beyond the endOfLine renderer. Stop traversing when the iterator moves to the next renderer to prevent O(n^2).
    return m_current.renderer() == endOfLine.renderer();
}

} // namespace WebCore
