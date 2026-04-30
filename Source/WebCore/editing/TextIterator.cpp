/*
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2015-2018 Google Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "TextIterator.h"

#include "BoundaryPointInlines.h"
#include "ComposedTreeIterator.h"
#include "Document.h"
#include "Editing.h"
#include "ElementChildIteratorInlines.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "FontCascade.h"
#include "HTMLAttachmentElement.h"
#include "HTMLBodyElement.h"
#include "HTMLElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHeadingElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLegendElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLParagraphElement.h"
#include "HTMLProgressElement.h"
#include "HTMLSelectElement.h"
#include "HTMLSlotElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "ICUSearcher.h"
#include "ImageOverlay.h"
#include "LocalFrame.h"
#include "NodeInlines.h"
#include "NodeTraversal.h"
#include "Range.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderImage.h"
#include "RenderIterator.h"
#include "RenderObjectInlines.h"
#include "RenderReplaced.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderTextControl.h"
#include "RenderTextFragment.h"
#include "ShadowRoot.h"
#include "TextBoundaries.h"
#include "TextControlInnerElements.h"
#include "TextPlaceholderElement.h"
#include "VisiblePosition.h"
#include "VisibleUnits.h"
#include <unicode/unorm2.h>
#include <wtf/Compiler.h>
#include <wtf/Function.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#if !UCONFIG_NO_COLLATION
#include <unicode/usearch.h>
#include <wtf/text/TextBreakIteratorInternalICU.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextIterator);

using namespace WTF::Unicode;
using namespace HTMLNames;

// Buffer that knows how to compare with a search target.
// Keeps enough of the previous text to be able to search in the future, but no more.
// Non-breaking spaces are always equal to normal spaces.
// Case folding is also done if the CaseInsensitive option is specified.
// Matches are further filtered if the AtWordStarts option is specified, although some
// matches inside a word are permitted if TreatMedialCapitalAsWordStart is specified as well.
class SearchBuffer {
    WTF_MAKE_NONCOPYABLE(SearchBuffer);
public:
    SearchBuffer(const String& target, FindOptions);

    // Returns number of characters appended; guaranteed to be in the range [1, length].
    size_t append(StringView);
    bool needsMoreContext() const;
    void prependContext(StringView);
    void reachedBreak();

    // Result is the size in characters of what was found.
    // And <startOffset> is the number of characters back to the start of what was found.
    size_t search(size_t& startOffset);
    bool atBreak() const;

#if !UCONFIG_NO_COLLATION

private:
    const String m_target;
    const StringView::UpconvertedCharacters m_targetCharacters;
    FindOptions m_options;

    Vector<char16_t> m_buffer;
    size_t m_overlap;
    size_t m_prefixLength;
    bool m_atBreak;
    bool m_needsMoreContext;

    const bool m_targetRequiresKanaWorkaround;
    Vector<char16_t> m_normalizedTarget;
    mutable Vector<char16_t> m_normalizedMatch;
    ICUSearcher m_ICUSearcher;

#else

private:
    void append(char16_t, bool isCharacterStart);
    size_t length() const;

    String m_target;
    FindOptions m_options;

    Vector<char16_t> m_buffer;
    Vector<bool> m_isCharacterStartBuffer;
    bool m_isBufferFull;
    size_t m_cursor;

#endif
};

// --------

constexpr unsigned bitsInWord = sizeof(unsigned) * 8;
constexpr unsigned bitInWordMask = bitsInWord - 1;

void BitStack::push(bool bit)
{
    unsigned index = m_size / bitsInWord;
    unsigned shift = m_size & bitInWordMask;
    if (!shift && index == m_words.size()) {
        m_words.grow(index + 1);
        m_words[index] = 0;
    }
    unsigned& word = m_words[index];
    unsigned mask = 1U << shift;
    if (bit)
        word |= mask;
    else
        word &= ~mask;
    ++m_size;
}

void BitStack::pop()
{
    if (m_size)
        --m_size;
}

bool BitStack::top() const
{
    if (!m_size)
        return false;
    unsigned shift = (m_size - 1) & bitInWordMask;
    unsigned index = (m_size - 1) / bitsInWord;
    return m_words[index] & (1U << shift);
}

// --------

// This function is like Range::pastLastNode, except for the fact that it can climb up out of shadow trees.
static RefPtr<Node> NODELETE nextInPreOrderCrossingShadowBoundaries(Node& rangeEndContainer, int rangeEndOffset)
{
    if (rangeEndOffset >= 0 && !rangeEndContainer.isCharacterDataNode()) {
        if (auto* next = rangeEndContainer.traverseToChildAt(rangeEndOffset))
            return next;
    }
    for (auto* node = &rangeEndContainer; node; node = node->parentOrShadowHostNode()) {
        if (auto* next = node->nextSibling())
            return next;
    }
    return nullptr;
}

static inline bool fullyClipsContents(const Node& node, TextIteratorBehaviors behaviors)
{
    CheckedPtr renderer = node.renderer();
    if (!renderer) {
        auto* element = dynamicDowncast<Element>(node);
        return element && !element->hasDisplayContents();
    }
    CheckedPtr box = dynamicDowncast<RenderBox>(*renderer);
    if (!box || !box->hasNonVisibleOverflow())
        return false;

    // Quirk to keep copy/paste in the CodeMirror editor version used in Jenkins working.
    if (is<HTMLTextAreaElement>(node))
        return box->size().isEmpty();

    if (behaviors.contains(TextIteratorBehavior::EntersSkippedContentRelevantToUser) && isSkippedContentRoot(*box)) {
        // This may reveal collapsed content to find-in-page, but it's uncommon (and highly redundant) to have computed block height 0px while applying c-v: hidden.
        return false;
    }

    return box->contentBoxSize().isEmpty();
}

static inline bool NODELETE ignoresContainerClip(const Node& node)
{
    auto* renderer = node.renderer();
    if (!renderer || renderer->isRenderTextOrLineBreak())
        return false;
    return renderer->isOutOfFlowPositioned();
}

static void pushFullyClippedState(BitStack& stack, Node& node, TextIteratorBehaviors behaviors)
{
    // Push true if this node full clips its contents, or if a parent already has fully
    // clipped and this is not a node that ignores its container's clip.
    stack.push(fullyClipsContents(node, behaviors) || (stack.top() && !ignoresContainerClip(node)));
}

static void setUpFullyClippedStack(BitStack& stack, Node& node, TextIteratorBehaviors behaviors)
{
    // Put the nodes in a vector so we can iterate in reverse order.
    // FIXME: This (and TextIterator in general) should use ComposedTreeIterator.
    Vector<Ref<Node>, 100> ancestry;
    for (RefPtr parent = node.parentOrShadowHostNode(); parent; parent = parent->parentOrShadowHostNode())
        ancestry.append(*parent);

    // Call pushFullyClippedState on each node starting with the earliest ancestor.
    size_t size = ancestry.size();
    for (size_t i = 0; i < size; ++i)
        pushFullyClippedState(stack, ancestry[size - i - 1], behaviors);
    pushFullyClippedState(stack, node, behaviors);
}

static bool isClippedByFrameAncestor(const Document& document, TextIteratorBehaviors behaviors)
{
    if (!behaviors.contains(TextIteratorBehavior::ClipsToFrameAncestors))
        return false;

    for (RefPtr owner = document.ownerElement(); owner; owner = owner->document().ownerElement()) {
        BitStack ownerClipStack;
        setUpFullyClippedStack(ownerClipStack, *owner, behaviors);
        if (ownerClipStack.top())
            return true;
    }
    return false;
}

// FIXME: editingIgnoresContent and isRendererReplacedElement try to do the same job.
// It's not good to have both of them.
bool isRendererReplacedElement(RenderObject* renderer, TextIteratorBehaviors behaviors)
{
    if (!renderer)
        return false;
    
    bool isAttachment = false;
#if ENABLE(ATTACHMENT_ELEMENT)
    isAttachment = renderer->isRenderAttachment();
#endif
    if (renderer->isImage() || renderer->isRenderWidget() || renderer->isRenderMedia() || isAttachment)
        return true;

    if (RefPtr element = dynamicDowncast<Element>(renderer->node())) {
        if (isAnyOf<HTMLFormControlElement, HTMLLegendElement, HTMLProgressElement>(*element) || element->hasTagName(meterTag))
            return true;
        if (equalLettersIgnoringASCIICase(element->attributeWithoutSynchronization(roleAttr), "img"_s))
            return true;
#if USE(ATSPI)
        // Links are also replaced with object replacement character in ATSPI.
        if (behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharacters) && element->isLink())
            return true;
#else
        UNUSED_PARAM(behaviors);
#endif
    }

    return false;
}

// --------

inline void TextIteratorCopyableText::reset()
{
    m_singleCharacter = 0;
    m_string = String();
    m_offset = 0;
    m_length = 0;
}

inline void TextIteratorCopyableText::set(String&& string)
{
    m_singleCharacter = 0;
    m_string = WTF::move(string);
    m_offset = 0;
    m_length = m_string.length();
}

inline void TextIteratorCopyableText::set(String&& string, unsigned offset, unsigned length)
{
    ASSERT(offset < string.length());
    ASSERT(length);
    ASSERT(length <= string.length() - offset);

    m_singleCharacter = 0;
    m_string = WTF::move(string);
    m_offset = offset;
    m_length = length;
}

inline void TextIteratorCopyableText::set(char16_t singleCharacter)
{
    m_singleCharacter = singleCharacter;
    m_string = String();
    m_offset = 0;
    m_length = 0;
}

void TextIteratorCopyableText::appendToStringBuilder(StringBuilder& builder) const
{
    if (m_singleCharacter)
        builder.append(m_singleCharacter);
    else
        builder.appendSubstring(m_string, m_offset, m_length);
}

// --------

static Node* NODELETE firstNode(const BoundaryPoint& point)
{
    if (point.container->isCharacterDataNode())
        return point.container.ptr();
    if (auto* child = point.container->traverseToChildAt(point.offset))
        return child;
    if (!point.offset)
        return point.container.ptr();
    return NodeTraversal::nextSkippingChildren(point.container);
}

TextIterator::TextIterator(const SimpleRange& range, TextIteratorBehaviors behaviors)
    : m_behaviors(behaviors)
{
    ASSERT(!m_behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharacters) || !m_behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharactersForImages));

    protect(range.start.document())->updateLayoutIgnorePendingStylesheets();

    m_startContainer = range.start.container.ptr();
    m_startOffset = range.start.offset;
    m_endContainer = range.end.container.ptr();
    m_endOffset = range.end.offset;

    m_currentNode = firstNode(range.start);
    if (!m_currentNode)
        return;

    init();
}

void TextIterator::init()
{
    RefPtr currentNode = m_currentNode;
    if (isClippedByFrameAncestor(protect(currentNode->document()), m_behaviors))
        return;

    setUpFullyClippedStack(m_fullyClippedStack, *currentNode, m_behaviors);

    m_offset = currentNode == m_startContainer.get() ? m_startOffset : 0;

    m_pastEndNode = nextInPreOrderCrossingShadowBoundaries(*m_endContainer, m_endOffset);

    m_positionNode = currentNode.get();

    advance();
}

TextIterator::~TextIterator() = default;

// FIXME: Use ComposedTreeIterator instead. These functions are more expensive because they might do O(n) work.
static inline Node* firstChild(TextIteratorBehaviors options, Node& node)
{
    if (options.contains(TextIteratorBehavior::TraversesFlatTree)) [[unlikely]]
        return firstChildInComposedTreeIgnoringUserAgentShadow(node);
    return node.firstChild();
}

static inline Node* nextSibling(TextIteratorBehaviors options, Node& node)
{
    if (options.contains(TextIteratorBehavior::TraversesFlatTree)) [[unlikely]]
        return nextSiblingInComposedTreeIgnoringUserAgentShadow(node);
    return node.nextSibling();
}

static inline Node* nextNode(TextIteratorBehaviors options, Node& node)
{
    if (options.contains(TextIteratorBehavior::TraversesFlatTree)) [[unlikely]]
        return nextInComposedTreeIgnoringUserAgentShadow(node);
    return NodeTraversal::next(node);
}

static inline bool NODELETE isDescendantOf(TextIteratorBehaviors options, Node& node, Node& possibleAncestor)
{
    if (options.contains(TextIteratorBehavior::TraversesFlatTree)) [[unlikely]]
        return node.isShadowIncludingDescendantOf(&possibleAncestor);
    return node.isDescendantOf(&possibleAncestor);
}

static inline Node* NODELETE parentNodeOrShadowHost(TextIteratorBehaviors options, Node& node)
{
    if (options.contains(TextIteratorBehavior::TraversesFlatTree)) [[unlikely]]
        return node.parentInComposedTree();
    return node.parentOrShadowHostNode();
}

static inline bool NODELETE hasDisplayContents(Node& node)
{
    auto* element = dynamicDowncast<Element>(node);
    return element && element->hasDisplayContents();
}

static bool isRendererAccessible(const RenderObject* renderer, TextIteratorBehaviors behaviors)
{
    if (!renderer)
        return false;

    CheckedRef style = renderer->style();
    if (style->usedUserSelect() == UserSelect::None && behaviors.contains(TextIteratorBehavior::IgnoresUserSelectNone))
        return false;

    if (renderer->isSkippedContent()) {
        if (!behaviors.contains(TextIteratorBehavior::EntersSkippedContentRelevantToUser))
            return false;
        return style->usedContentVisibility() == ContentVisibility::Auto || style->autoRevealsWhenFound();
    }

    return true;
}

static bool isConsideredSkippedContent(const RenderBox* renderBox, TextIteratorBehaviors behaviors)
{
    if (!renderBox || !isSkippedContentRoot(*renderBox))
        return false;

    if (behaviors.contains(TextIteratorBehavior::EntersSkippedContentRelevantToUser))
        return renderBox->style().usedContentVisibility() == ContentVisibility::Hidden && !renderBox->style().autoRevealsWhenFound();

    return true;
}

void TextIterator::advance()
{
    ASSERT(!atEnd());

    // reset the run information
    m_positionNode = nullptr;
    m_copyableText.reset();
    m_text = StringView();
    m_isBlockNewline = false;

    // handle remembered node that needed a newline after the text node's newline
    if (RefPtr nodeForAdditionalNewline = std::exchange(m_nodeForAdditionalNewline, nullptr).get()) {
        // Emit the extra newline, and position it *inside* m_node, after m_node's 
        // contents, in case it's a block, in the same way that we position the first 
        // newline. The range for the emitted newline should start where the line
        // break begins.
        // FIXME: It would be cleaner if we emitted two newlines during the last 
        // iteration, instead of using m_needsAnotherNewline.
        RefPtr parentNode = nodeForAdditionalNewline->parentNode();
        emitCharacter('\n', WTF::move(parentNode), WTF::move(nodeForAdditionalNewline), 1, 1);
        m_isBlockNewline = true;
        return;
    }

    if (!m_textRun && m_remainingTextRun)
        revertToRemainingTextRun();

    // handle remembered text box
    if (m_textRun) {
        handleTextRun();
        if (m_positionNode)
            return;
    }

    while (m_currentNode && m_currentNode != m_pastEndNode) {
        // if the range ends at offset 0 of an element, represent the
        // position, but not the content, of that element e.g. if the
        // node is a blockflow element, emit a newline that
        // precedes the element
        if (m_currentNode == m_endContainer && !m_endOffset) {
            representNodeOffsetZero();
            m_currentNode = nullptr;
            return;
        }
        
        CheckedPtr renderer = m_currentNode->renderer();
        if (!m_handledNode) {
            if (!isRendererAccessible(renderer.get(), m_behaviors)) {
                m_handledNode = true;
                m_handledChildren = !hasDisplayContents(*m_currentNode) && !renderer;
            } else {
                if (isConsideredSkippedContent(dynamicDowncast<RenderBox>(renderer.get()), m_behaviors))
                    m_handledChildren = true;
                else if (renderer->isRenderText() && m_currentNode->isTextNode())
                    m_handledNode = handleTextNode();
                else if (isRendererReplacedElement(renderer.get(), m_behaviors) && (renderer->isInline() || !m_behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec)))
                    m_handledNode = handleReplacedElement();
                else
                    m_handledNode = handleNonTextNode();
                if (m_positionNode)
                    return;
            }
        }

        // find a new current node to handle in depth-first manner,
        // calling exitNode() as we come back thru a parent node

        RefPtr next = m_handledChildren ? nullptr : firstChild(m_behaviors, *protect(m_currentNode));
        m_offset = 0;
        if (!next) {
            RefPtr currentNode = m_currentNode;
            next = nextSibling(m_behaviors, *currentNode);
            if (!next) {
                bool pastEnd = nextNode(m_behaviors, *currentNode) == m_pastEndNode;
                RefPtr parentNode = parentNodeOrShadowHost(m_behaviors, *currentNode);
                while (!next && parentNode) {
                    if ((pastEnd && parentNode == m_endContainer.get()) || isDescendantOf(m_behaviors, *m_endContainer, *parentNode))
                        return;
                    bool haveRenderer = isRendererAccessible(protect(currentNode->renderer()), m_behaviors);
                    RefPtr exitedNode = WTF::move(currentNode);
                    m_currentNode = WTF::move(parentNode);
                    currentNode = m_currentNode;
                    m_fullyClippedStack.pop();
                    parentNode = parentNodeOrShadowHost(m_behaviors, *currentNode);
                    if (haveRenderer)
                        exitNode(exitedNode.get());
                    if (m_positionNode) {
                        m_handledNode = true;
                        m_handledChildren = true;
                        return;
                    }
                    next = nextSibling(m_behaviors, *currentNode);
                    if (next && isRendererAccessible(protect(currentNode->renderer()), m_behaviors))
                        exitNode(currentNode.get());
                }
            }
            m_fullyClippedStack.pop();            
        }

        // set the new current node
        m_currentNode = WTF::move(next);
        if (RefPtr currentNode = m_currentNode)
            pushFullyClippedState(m_fullyClippedStack, *currentNode, m_behaviors);
        m_handledNode = false;
        m_handledChildren = false;
        m_handledFirstLetter = false;
        m_firstLetterText = nullptr;

        // how would this ever be?
        if (m_positionNode)
            return;
    }
}

static bool NODELETE hasVisibleTextNode(RenderText& renderer)
{
    if (renderer.style().visibility() == Visibility::Visible)
        return true;
    if (auto* renderTextFragment = dynamicDowncast<RenderTextFragment>(renderer)) {
        if (auto firstLetter = renderTextFragment->firstLetter()) {
            if (firstLetter->style().visibility() == Visibility::Visible)
                return true;
        }
    }
    return false;
}

bool TextIterator::handleTextNode()
{
    Ref textNode = downcast<Text>(protect(m_currentNode).releaseNonNull());

    if (m_fullyClippedStack.top() && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility))
        return false;

    CheckedRef renderer = *textNode->renderer();
    m_lastTextNode = textNode.ptr();
    auto rendererText = rendererTextForBehavior(renderer.get());
    CheckedPtr textFragmentWithRemainingTextAfterFirstLetter = dynamicDowncast<RenderTextFragment>(renderer.get());

    // handle pre-formatted text
    if (!renderer->style().collapseWhiteSpace()) {
        int runStart = m_offset;
        if (m_lastTextNodeEndedWithCollapsedSpace && hasVisibleTextNode(renderer)) {
            emitCharacter(' ', WTF::move(textNode), nullptr, runStart, runStart);
            return false;
        }
        if (textFragmentWithRemainingTextAfterFirstLetter && !m_handledFirstLetter && !m_offset) {
            handleTextNodeFirstLetter(*textFragmentWithRemainingTextAfterFirstLetter);
            if (m_firstLetterText) {
                String firstLetter = m_firstLetterText->text();
                emitText(textNode, *protect(m_firstLetterText), m_offset, m_offset + firstLetter.length());
                m_firstLetterText = nullptr;
                m_textRun = { };
                return false;
            }
        }
        if (renderer->style().visibility() != Visibility::Visible && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility))
            return false;
        int rendererTextLength = rendererText.length();
        int end = (textNode.ptr() == m_endContainer) ? m_endOffset : INT_MAX;
        if (textFragmentWithRemainingTextAfterFirstLetter && textFragmentWithRemainingTextAfterFirstLetter->firstLetter()) {
            runStart = convertNodeOffsetToOffsetInTextFragment(*textFragmentWithRemainingTextAfterFirstLetter, std::max(0, runStart));
            end = end == INT_MAX ? INT_MAX : static_cast<int>(convertNodeOffsetToOffsetInTextFragment(*textFragmentWithRemainingTextAfterFirstLetter, end));
        }
        int runEnd = std::min(rendererTextLength, end);

        if (runStart >= runEnd)
            return true;

        emitText(textNode, renderer, runStart, runEnd);
        return true;
    }

    std::tie(m_textRun, m_textRunLogicalOrderCache) = InlineIterator::firstTextBoxInLogicalOrderFor(renderer.get());

    if (textFragmentWithRemainingTextAfterFirstLetter && !m_handledFirstLetter && !m_offset)
        handleTextNodeFirstLetter(*textFragmentWithRemainingTextAfterFirstLetter);
    else if (!m_textRun && rendererText.length()) {
        if (renderer->style().visibility() != Visibility::Visible && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility))
            return false;
        m_lastTextNodeEndedWithCollapsedSpace = true; // entire block is collapsed space
        return true;
    }

    handleTextRun();
    return true;
}

void TextIterator::handleTextRun()
{
    Ref textNode = downcast<Text>(protect(m_currentNode).releaseNonNull());

    CheckedRef renderer = m_firstLetterText ? *m_firstLetterText : *textNode->renderer();
    if (renderer->style().visibility() != Visibility::Visible && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility)) {
        m_textRun = { };
        return;
    }

    auto [firstTextRun, orderCache] = InlineIterator::firstTextBoxInLogicalOrderFor(renderer);

    // For remaining text fragments after a first-letter split, text box offsets are fragment-local but m_offset/m_endOffset are DOM offsets.
    unsigned remainingFragmentStart = 0;
    if (auto* renderText = dynamicDowncast<RenderTextFragment>(renderer.get()); renderText && renderText->firstLetter())
        remainingFragmentStart = renderText->start();

    auto toFragmentLocal = [&](unsigned nodeOffset) {
        return nodeOffset > remainingFragmentStart ? nodeOffset - remainingFragmentStart : 0;
    };
    auto toNodeOffset = [&](unsigned localOffset) {
        return localOffset + remainingFragmentStart;
    };

    auto rendererText = rendererTextForBehavior(renderer.get());
    auto rangeStart = toFragmentLocal(m_offset);
    auto rangeEnd = textNode.ptr() == m_endContainer ? std::make_optional(toFragmentLocal(m_endOffset)) : std::nullopt;
    while (m_textRun) {
        auto textRunStart = m_textRun->start();
        auto textRunEnd = textRunStart + m_textRun->length();

        auto runStart = std::max(textRunStart, rangeStart);
        auto runEnd = std::min(textRunEnd, rangeEnd.value_or(textRunEnd));

        // Check if we need to emit (previously) collapsed whitespace at the start of this run.
        auto isAfterRangeEnd = rangeEnd ? runStart > *rangeEnd : false;
        auto hasPrecedingCollapsedWhitespace = m_lastTextNodeEndedWithCollapsedSpace || (m_textRun == firstTextRun && textRunStart == runStart && runStart);
        auto shouldEmitWhitespace = !isAfterRangeEnd && hasPrecedingCollapsedWhitespace && m_lastCharacter && !renderer->style().isCollapsibleWhiteSpace(m_lastCharacter);
        if (shouldEmitWhitespace) {
            if (m_lastTextNode == textNode.ptr() && runStart && renderer->style().isCollapsibleWhiteSpace(rendererText[runStart - 1])) {
                unsigned spaceRunStart = runStart - 1;
                while (spaceRunStart && renderer->style().isCollapsibleWhiteSpace(rendererText[spaceRunStart - 1]))
                    --spaceRunStart;
                emitCharacter(' ', WTF::move(textNode), nullptr, spaceRunStart, spaceRunStart + 1);
            } else
                emitCharacter(' ', WTF::move(textNode), nullptr, runStart, runStart);
            return;
        }

        // Determine what the next text run will be, but don't advance yet
        auto nextTextRun = InlineIterator::nextTextBoxInLogicalOrder(m_textRun, m_textRunLogicalOrderCache);
        if (runStart < runEnd) {
            bool shouldPreserveNewline = renderer->style().preserveNewline();
            auto isCollapsibleNewlineOrTab = [&](char16_t character) {
                return character == '\t' || (character == '\n' && !shouldPreserveNewline);
            };
            // Handle either a single collapsible newline or tab character (which becomes a space),
            // or a run of characters that does not include such characters.
            // This effectively translates collapsible newlines and tabs to spaces without copying the text.
            // For white-space:pre-line, newlines are preserved rather than collapsed to spaces.
            if (isCollapsibleNewlineOrTab(rendererText[runStart])) {
                emitCharacter(' ', textNode.copyRef(), nullptr, runStart, runStart + 1);
                m_offset = toNodeOffset(runStart + 1);
            } else {
                auto subrunEnd = runStart + 1;
                for (; subrunEnd < runEnd; ++subrunEnd) {
                    if (isCollapsibleNewlineOrTab(rendererText[subrunEnd]))
                        break;
                }
                if (subrunEnd == runEnd && m_behaviors.contains(TextIteratorBehavior::BehavesAsIfNodesFollowing)) {
                    bool lastSpaceCollapsedByNextNonTextRun = !nextTextRun && rendererText.length() > subrunEnd && rendererText[subrunEnd] == ' ';
                    if (lastSpaceCollapsedByNextNonTextRun)
                        ++subrunEnd; // runEnd stopped before last space. Increment by one to restore the space.
                }
                m_offset = toNodeOffset(subrunEnd);
                emitText(textNode, renderer, runStart, subrunEnd);
            }

            // If we are doing a subrun that doesn't go to the end of the text box,
            // come back again to finish handling this text box; don't advance to the next one.
            if (static_cast<unsigned>(m_positionEndOffset) < toNodeOffset(textRunEnd))
                return;

            // Advance and return
            unsigned nextRunStart = nextTextRun ? nextTextRun->start() : rendererText.length();
            if (nextRunStart > runEnd)
                m_lastTextNodeEndedWithCollapsedSpace = true; // collapsed space between runs or at the end
            m_textRun = nextTextRun;
            return;
        }
        // Advance and continue
        m_textRun = nextTextRun;
    }
    if (!m_textRun && m_remainingTextRun) {
        revertToRemainingTextRun();
        handleTextRun();
    }
}

void TextIterator::revertToRemainingTextRun()
{
    ASSERT(!m_textRun && m_remainingTextRun);

    m_textRun = m_remainingTextRun;
    m_textRunLogicalOrderCache = std::exchange(m_remainingTextRunLogicalOrderCache, { });
    m_remainingTextRun = { };
    m_firstLetterText = { };
    m_offset = 0;
}

static inline RenderText* firstRenderTextInFirstLetter(RenderBoxModelObject* firstLetter)
{
    if (!firstLetter)
        return nullptr;

    // FIXME: Should this check descendent objects?
    return childrenOfType<RenderText>(*firstLetter).first();
}

void TextIterator::handleTextNodeFirstLetter(RenderTextFragment& renderer)
{
    if (CheckedPtr firstLetter = renderer.firstLetter()) {
        if (firstLetter->style().visibility() != Visibility::Visible && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility))
            return;
        if (CheckedPtr firstLetterText = firstRenderTextInFirstLetter(firstLetter.get())) {
            m_handledFirstLetter = true;
            m_remainingTextRun = m_textRun;
            m_remainingTextRunLogicalOrderCache = std::exchange(m_textRunLogicalOrderCache, { });
            std::tie(m_textRun, m_textRunLogicalOrderCache) = InlineIterator::firstTextBoxInLogicalOrderFor(*firstLetterText);
            m_firstLetterText = firstLetterText.get();
        }
    }
    m_handledFirstLetter = true;
}

bool TextIterator::handleReplacedElement()
{
    if (m_fullyClippedStack.top())
        return false;

    // Note that RenderInlines can get passed in as replaced elements.
    CheckedPtr renderer = dynamicDowncast<RenderElement>(m_currentNode->renderer());
    if (!renderer) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (renderer->style().visibility() != Visibility::Visible && !m_behaviors.contains(TextIteratorBehavior::IgnoresStyleVisibility))
        return false;

    if (m_lastTextNodeEndedWithCollapsedSpace) {
        emitCharacter(' ', protect(m_lastTextNode->parentNode()), m_lastTextNode.copyRef(), 1, 1);
        return false;
    }

    if (CheckedPtr renderTextControl = dynamicDowncast<RenderTextControl>(*renderer); renderTextControl && m_behaviors.contains(TextIteratorBehavior::EntersTextControls)) {
        if (auto innerTextElement = protect(renderTextControl->textFormControlElement())->innerTextElement()) {
            m_currentNode = innerTextElement->containingShadowRoot();
            pushFullyClippedState(m_fullyClippedStack, *protect(m_currentNode), m_behaviors);
            m_offset = 0;
            return false;
        }
    }

    RefPtr currentElement = dynamicDowncast<HTMLElement>(m_currentNode.get());
    if (m_behaviors.contains(TextIteratorBehavior::EntersImageOverlays) && currentElement && ImageOverlay::hasOverlay(*currentElement)) {
        if (RefPtr shadowRoot = m_currentNode->shadowRoot()) {
            m_currentNode = WTF::move(shadowRoot);
            pushFullyClippedState(m_fullyClippedStack, *protect(m_currentNode), m_behaviors);
            m_offset = 0;
            return false;
        }
        ASSERT_NOT_REACHED();
    }

    // In innerText mode, replaced elements that produce no visible text (e.g.
    // <input>) should not count as having emitted content. This prevents
    // spurious block-boundary newlines when the only thing before a block
    // element is a replaced element with no text output. For other modes,
    // preserve the existing behavior to avoid changing test expectations.
    if (!m_behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec))
        m_hasEmitted = true;

    auto shouldEmitObjectReplacementCharacter = [&] {
        if (m_behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharacters))
            return true;

        if (m_behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharactersForImages) && is<HTMLImageElement>(m_currentNode.get()))
            return true;

#if ENABLE(ATTACHMENT_ELEMENT)
        if (m_behaviors.contains(TextIteratorBehavior::EmitsObjectReplacementCharactersForAttachments) && is<HTMLAttachmentElement>(m_currentNode.get()))
            return true;
#endif

        return false;
    }();

    if (shouldEmitObjectReplacementCharacter) {
        emitCharacter(objectReplacementCharacter, protect(m_currentNode->parentNode()), protect(m_currentNode), 0, 1);
        // Don't process subtrees for embedded objects. If the text there is required,
        // it must be explicitly asked by specifying a range falling inside its boundaries.
        m_handledChildren = true;
        return true;
    }

    if (m_behaviors.contains(TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions)) {
        // We want replaced elements to behave like punctuation for boundary
        // finding, and to simply take up space for the selection preservation
        // code in moveParagraphs, so we use a comma.
        emitCharacter(',', protect(m_currentNode->parentNode()), protect(m_currentNode), 0, 1);
        return true;
    }

    m_positionNode = m_currentNode->parentNode();
    m_positionOffsetBaseNode = m_currentNode;
    m_positionStartOffset = 0;
    m_positionEndOffset = 1;

    if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(*renderer); renderImage && m_behaviors.contains(TextIteratorBehavior::EmitsImageAltText)) {
        auto altText = renderImage->altText();
        if (unsigned length = altText.length()) {
            m_hasEmitted = true;
            m_lastCharacter = altText[length - 1];
            m_copyableText.set(WTF::move(altText));
            m_text = m_copyableText.text();
            return true;
        }
    }

    if (m_behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec)) {
        if (RefPtr selectElement = dynamicDowncast<HTMLSelectElement>(m_currentNode)) {
            m_handledChildren = true;
            if (String selectText = selectElement->collectOptionInnerText(HTMLSelectElement::EmitNewlineForEmptyItems::Yes); !selectText.isEmpty()) {
                m_hasEmitted = true;
                m_lastCharacter = selectText[selectText.length() - 1];
                m_copyableText.set(WTF::move(selectText));
                m_text = m_copyableText.text();
                return true;
            }
        }
    }

    m_copyableText.reset();
    m_text = StringView();
    m_lastCharacter = 0;
    return true;
}

static bool shouldEmitTabBeforeNode(Node& node)
{
    CheckedPtr cell = dynamicDowncast<RenderTableCell>(node.renderer());
    
    // Table cells are delimited by tabs.
    if (!cell)
        return false;
    
    // Want a tab before every cell other than the first one.
    CheckedPtr table = cell->table();
    return table && (table->cellBefore(cell.get()) || table->cellAbove(cell.get()));
}

static bool NODELETE shouldEmitNewlineForNode(Node* node, bool emitsOriginalText)
{
    auto* renderer = node->renderer();
    if (!(renderer ? renderer->isBR() : node->hasTagName(brTag)))
        return false;
    return emitsOriginalText || !(node->isInShadowTree() && is<HTMLInputElement>(*node->shadowHost()));
}

static bool NODELETE shouldEmitReplacementInsteadOfNode(const Node& node)
{
    // Placeholders should eventually disappear, so treating them as a line break doesn't make sense
    // as when they are removed the text after it is combined with the text before it.
    return is<TextPlaceholderElement>(node);
}

static bool isBlockLevelReplacedElement(Node& node)
{
    auto* renderer = node.renderer();
    return renderer && !renderer->isInline() && is<RenderReplaced>(*renderer)
        && !renderer->isFloatingOrOutOfFlowPositioned();
}

bool shouldEmitNewlinesBeforeAndAfterNode(Node& node, bool emitsNewlinesPerInnerTextSpec)
{
    // <p> elements always emit newlines regardless of their CSS display value.
    // https://html.spec.whatwg.org/multipage/dom.html#rendered-text-collection-steps
    if (emitsNewlinesPerInnerTextSpec && is<HTMLParagraphElement>(node))
        return true;

    // Block flow (versus inline flow) is represented by having
    // a newline both before and after the element.
    auto* renderer = node.renderer();
    if (!renderer) {
        if (hasDisplayContents(node))
            return false;
        auto* element = dynamicDowncast<HTMLElement>(node);
        return element && (is<HTMLHeadingElement>(*element)
            || element->hasTagName(blockquoteTag)
            || element->hasTagName(ddTag)
            || element->hasTagName(divTag)
            || element->hasTagName(dlTag)
            || element->hasTagName(dtTag)
            || element->hasTagName(hrTag)
            || element->hasTagName(liTag)
            || element->hasTagName(listingTag)
            || element->hasTagName(olTag)
            || element->hasTagName(pTag)
            || element->hasTagName(preTag)
            || element->hasTagName(trTag)
            || element->hasTagName(ulTag));
    }
    
    // Need to make an exception for table cells, because they are blocks, but we
    // want them tab-delimited rather than having newlines before and after.
    if (isTableCell(node))
        return false;
    
    // Need to make an exception for table row elements, because they are neither
    // "inline" or "RenderBlock", but we want newlines for them.
    if (auto* tableRow = dynamicDowncast<RenderTableRow>(*renderer)) {
        auto* table = tableRow->table();
        if (table && !table->isInline())
            return true;
    }

    if (shouldEmitReplacementInsteadOfNode(node))
        return false;

    // Table rows are RenderBlocks but have their own newline logic above
    // that accounts for inline tables.
    if (is<RenderTableRow>(*renderer))
        return false;

    return !renderer->isInline()
        && is<RenderBlock>(*renderer)
        && !renderer->isFloatingOrOutOfFlowPositioned()
        && !renderer->isBody();
}

static bool shouldEmitNewlineAfterNode(Node& node, bool emitsCharactersBetweenAllVisiblePositions = false, bool emitsNewlinesPerInnerTextSpec = false)
{
    // FIXME: It should be better but slower to create a VisiblePosition here.
    if (!shouldEmitNewlinesBeforeAndAfterNode(node, emitsNewlinesPerInnerTextSpec))
        return false;

    // Don't emit a new line at the end of the document unless we're matching the behavior of VisiblePosition.
    if (emitsCharactersBetweenAllVisiblePositions)
        return true;
    auto* subsequentNode = &node;
    while ((subsequentNode = NodeTraversal::nextSkippingChildren(*subsequentNode))) {
        if (subsequentNode->renderer())
            return true;
    }
    return false;
}

static bool NODELETE shouldEmitNewlineBeforeNode(Node& node, bool emitsNewlinesPerInnerTextSpec = false)
{
    return shouldEmitNewlinesBeforeAndAfterNode(node, emitsNewlinesPerInnerTextSpec);
}

static bool shouldEmitExtraNewlineForNode(Node& node, bool emitsNewlinesPerInnerTextSpec)
{
    CheckedPtr renderBox = dynamicDowncast<RenderBox>(node.renderer());
    if (!renderBox || !renderBox->height())
        return false;

    // Per the WHATWG spec, <p> elements get a required line break count of 2,
    // meaning a blank line (two newlines) before and after, unconditionally.
    // Heading elements (<h1>-<h6>) do NOT get this treatment.
    if (emitsNewlinesPerInnerTextSpec)
        return is<HTMLParagraphElement>(node);

    // For non-innerText uses (accessibility, selection, etc.), use the original
    // margin-based heuristic for both <p> and heading elements.
    RefPtr element = dynamicDowncast<HTMLElement>(node);
    if (!element || !isAnyOf<HTMLHeadingElement, HTMLParagraphElement>(*element))
        return false;

    auto bottomMargin = renderBox->collapsedMarginAfter();
    auto fontSize = renderBox->style().fontDescription().computedSize();
    return bottomMargin * 2 >= fontSize;
}

static int collapsedSpaceLength(RenderText& renderer, int textEnd)
{
    auto text = renderer.text();
    unsigned length = text.length();
    for (unsigned i = textEnd; i < length; ++i) {
        if (!renderer.style().isCollapsibleWhiteSpace(text[i]))
            return i - textEnd;
    }
    return length - textEnd;
}

static int maxOffsetIncludingCollapsedSpaces(Node& node)
{
    int offset = caretMaxOffset(node);
    if (CheckedPtr renderText = dynamicDowncast<RenderText>(node.renderer()))
        offset += collapsedSpaceLength(*renderText, offset);
    return offset;
}

// Whether or not we should emit a character as we enter m_currentNode (if it's a container) or as we hit it (if it's atomic).
bool TextIterator::shouldRepresentNodeOffsetZero()
{
    if (m_behaviors.contains(TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions)) {
        if (auto* renderer = m_currentNode->renderer(); renderer && renderer->isRenderTable())
            return true;
    }

    // Leave element positioned flush with start of a paragraph
    // (e.g. do not insert tab before a table cell at the start of a paragraph)
    if (m_lastCharacter == '\n')
        return false;
    
    // Otherwise, show the position if we have emitted any characters
    if (m_hasEmitted)
        return true;
    
    // We've not emitted anything yet. Generally, there is no need for any positioning then.
    // The only exception is when the element is visually not in the same line as
    // the start of the range (e.g. the range starts at the end of the previous paragraph).
    // NOTE: Creating VisiblePositions and comparing them is relatively expensive, so we
    // make quicker checks to possibly avoid that. Another check that we could make is
    // is whether the inline vs block flow changed since the previous visible element.
    // I think we're already in a special enough case that that won't be needed, tho.

    // No character needed if this is the first node in the range.
    if (m_currentNode == m_startContainer)
        return false;
    
    // If we are outside the start container's subtree, assume we need to emit.
    // FIXME: m_startContainer could be an inline block
    Ref currentNode = *m_currentNode;
    if (!currentNode->isDescendantOf(m_startContainer.get()))
        return true;

    // If we started as m_startContainer offset 0 and the current node is a descendant of
    // the start container, we already had enough context to correctly decide whether to
    // emit after a preceding block. We chose not to emit (m_hasEmitted is false),
    // so don't second guess that now.
    // NOTE: Is this really correct when m_currentNode is not a leftmost descendant? Probably
    // immaterial since we likely would have already emitted something by now.
    if (m_startOffset == 0)
        return false;
        
    // If this node is unrendered or invisible the VisiblePosition checks below won't have much meaning.
    // Additionally, if the range we are iterating over contains huge sections of unrendered content, 
    // we would create VisiblePositions on every call to this function without this check.
    if (!currentNode->renderer() || currentNode->renderer()->style().visibility() != Visibility::Visible)
        return false;

    if (auto* renderBlockFlow = dynamicDowncast<RenderBlockFlow>(*currentNode->renderer())) {
        if (!renderBlockFlow->height() && !is<HTMLBodyElement>(currentNode))
            return false;
    }

    // The startPos.isNotNull() check is needed because the start could be before the body,
    // and in that case we'll get null. We don't want to put in newlines at the start in that case.
    // The currPos.isNotNull() check is needed because positions in non-HTML content
    // (like SVG) do not have visible positions, and we don't want to emit for them either.
    VisiblePosition startPos = VisiblePosition(Position(protect(m_startContainer), m_startOffset, Position::PositionIsOffsetInAnchor));
    VisiblePosition currPos = VisiblePosition(positionBeforeNode(currentNode));
    return startPos.isNotNull() && currPos.isNotNull() && !inSameLine(startPos, currPos);
}

bool TextIterator::shouldEmitSpaceBeforeAndAfterNode(Node& node)
{
    return node.renderer() && node.renderer()->isRenderTable() && (node.renderer()->isInline() || m_behaviors.contains(TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions));
}

void TextIterator::representNodeOffsetZero()
{
    // Emit a character to show the positioning of m_currentNode.
    
    // When we haven't been emitting any characters, shouldRepresentNodeOffsetZero() can 
    // create VisiblePositions, which is expensive. So, we perform the inexpensive checks
    // on m_currentNode to see if it necessitates emitting a character first and will early return
    // before encountering shouldRepresentNodeOffsetZero()s worse case behavior.
    RefPtr currentNode = m_currentNode;
    bool emitsNewlinesPerInnerTextSpec = m_behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec);

    if (emitsNewlinesPerInnerTextSpec) {
        if (auto* renderer = currentNode->renderer(); renderer && renderer->style().visibility() != Visibility::Visible)
            return;
    }

    if (shouldEmitTabBeforeNode(*currentNode)) {
        if (shouldRepresentNodeOffsetZero()) {
            RefPtr parentNode = currentNode->parentNode();
            emitCharacter('\t', WTF::move(parentNode), WTF::move(currentNode), 0, 0);
        }
    } else if (shouldEmitNewlineBeforeNode(*currentNode, emitsNewlinesPerInnerTextSpec) || (emitsNewlinesPerInnerTextSpec && isBlockLevelReplacedElement(*currentNode))) {
        if (shouldRepresentNodeOffsetZero()) {
            RefPtr parentNode = currentNode->parentNode();
            emitCharacter('\n', WTF::move(parentNode), WTF::move(currentNode), 0, 0);
            // Per the spec, <p> elements require a blank line (2 newlines) before them.
            if (emitsNewlinesPerInnerTextSpec && is<HTMLParagraphElement>(*m_currentNode))
                m_nodeForAdditionalNewline = m_currentNode.get();
        } else if (emitsNewlinesPerInnerTextSpec && is<HTMLParagraphElement>(*currentNode) && m_hasEmitted && m_consecutiveNewlineCount < 2) {
            // shouldRepresentNodeOffsetZero() returned false because m_lastCharacter == '\n',
            // but <p> requires a blank line. Emit one more newline if we don't have enough.
            RefPtr parentNode = currentNode->parentNode();
            emitCharacter('\n', WTF::move(parentNode), WTF::move(currentNode), 0, 0);
            // If the preceding '\n' was a content newline (e.g. from <pre> text) rather
            // than a block-boundary newline, m_consecutiveNewlineCount was reset to 0 by
            // emitText and the single emitCharacter above only brings it to 1. Schedule
            // one more so <p> gets its full required line break count of 2.
            if (m_consecutiveNewlineCount < 2)
                m_nodeForAdditionalNewline = m_currentNode.get();
        }
    } else if (shouldEmitSpaceBeforeAndAfterNode(*currentNode)) {
        if (shouldRepresentNodeOffsetZero()) {
            RefPtr parentNode = currentNode->parentNode();
            emitCharacter(' ', WTF::move(parentNode), WTF::move(currentNode), 0, 0);
        }
    } else if (shouldEmitReplacementInsteadOfNode(*currentNode)) {
        if (shouldRepresentNodeOffsetZero()) {
            RefPtr parentNode = currentNode->parentNode();
            emitCharacter(objectReplacementCharacter, WTF::move(parentNode), WTF::move(currentNode), 0, 0);
        }
    }

    // When entering an inline-level block formatting context (e.g. inline-block),
    // suppress leading collapsed whitespace. For normal blocks, the '\n' emitted
    // above achieves this because '\n' is collapsible whitespace, preventing
    // shouldEmitWhitespace from firing. Inline-blocks don't emit '\n', so mimic
    // the same effect without emitting a visible character.
    if (emitsNewlinesPerInnerTextSpec) {
        if (CheckedPtr renderer = dynamicDowncast<RenderBlock>(m_currentNode->renderer()); renderer && renderer->isInline() && !renderer->isRenderTable()) {
            m_lastTextNodeEndedWithCollapsedSpace = false;
            m_lastCharacter = '\n';
        }
    }
}

bool TextIterator::handleNonTextNode()
{
    RefPtr currentNode = m_currentNode;
    if (shouldEmitNewlineForNode(currentNode.get(), m_behaviors.contains(TextIteratorBehavior::EmitsOriginalText))) {
        RefPtr parentNode = currentNode->parentNode();
        emitCharacter('\n', WTF::move(parentNode), WTF::move(currentNode), 0, 1);
    } else if (m_behaviors.contains(TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions) && currentNode->renderer() && currentNode->renderer()->isHR()) {
        RefPtr parentNode = currentNode->parentNode();
        emitCharacter(' ', WTF::move(parentNode), WTF::move(currentNode), 0, 1);
    } else
        representNodeOffsetZero();

    return true;
}

void TextIterator::exitNode(Node* exitedNode)
{
    // prevent emitting a newline when exiting a collapsed block at beginning of the range
    // FIXME: !m_hasEmitted does not necessarily mean there was a collapsed block... it could
    // have been an hr (e.g.). Also, a collapsed block could have height (e.g. a table) and
    // therefore look like a blank line.
    if (!m_hasEmitted)
        return;
    
    if (m_behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec)) {
        if (auto* renderer = m_currentNode->renderer(); renderer && renderer->style().visibility() != Visibility::Visible)
            return;
    }

    // Emit with a position *inside* m_currentNode, after m_currentNode's contents, in
    // case it is a block, because the run should start where the
    // emitted character is positioned visually.
    RefPtr baseNode = exitedNode;
    // FIXME: This shouldn't require the m_lastTextNode to be true, but we can't change that without making
    // the logic in _web_attributedStringFromRange match. We'll get that for free when we switch to use
    // TextIterator in _web_attributedStringFromRange.
    // See <rdar://problem/5428427> for an example of how this mismatch will cause problems.
    if (m_lastTextNode && (shouldEmitNewlineAfterNode(*protect(m_currentNode), m_behaviors.contains(TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions), m_behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec)) || (m_behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec) && isBlockLevelReplacedElement(*protect(m_currentNode))))) {
        // use extra newline to represent margin bottom, as needed
        bool addNewline = shouldEmitExtraNewlineForNode(*protect(m_currentNode), m_behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec));

        // FIXME: We need to emit a '\n' as we leave an empty block(s) that
        // contain a VisiblePosition when doing selection preservation.
        if (m_lastCharacter != '\n') {
            // insert a newline with a position following this block's contents.
            emitCharacter('\n', protect(baseNode->parentNode()), baseNode.copyRef(), 1, 1);
            m_isBlockNewline = true;
            // remember whether to later add a newline for the current node
            ASSERT(!m_nodeForAdditionalNewline);
            if (addNewline)
                m_nodeForAdditionalNewline = baseNode.get();
        } else if (addNewline) {
            // insert a newline with a position following this block's contents.
            emitCharacter('\n', protect(baseNode->parentNode()), baseNode.copyRef(), 1, 1);
            m_isBlockNewline = true;
        }
    }
    
    // If nothing was emitted, see if we need to emit a space.
    if (!m_positionNode && shouldEmitSpaceBeforeAndAfterNode(*m_currentNode)) {
        RefPtr parentNode = baseNode->parentNode();
        emitCharacter(' ', WTF::move(parentNode), WTF::move(baseNode), 1, 1);
    }

    // Trailing collapsed whitespace inside a block formatting context (e.g. inline-block)
    // should not leak into the outer flow. For block-level elements, emitCharacter('\n')
    // above already resets this flag, but for inline-level elements that establish a BFC
    // (like inline-block), no newline is emitted, so we must reset it explicitly.
    if (m_behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec) && is<RenderBlock>(exitedNode->renderer()))
        m_lastTextNodeEndedWithCollapsedSpace = false;
}

void TextIterator::emitCharacter(char16_t character, RefPtr<Node>&& characterNode, RefPtr<Node>&& offsetBaseNode, int textStartOffset, int textEndOffset)
{
    ASSERT(characterNode);
    m_hasEmitted = true;
    
    // remember information with which to construct the TextIterator::range()
    m_positionNode = WTF::move(characterNode);
    m_positionOffsetBaseNode = WTF::move(offsetBaseNode);
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;

    m_copyableText.set(character);
    m_text = m_copyableText.text();
    m_lastCharacter = character;
    if (character == '\n')
        ++m_consecutiveNewlineCount;
    else
        m_consecutiveNewlineCount = 0;
    m_lastTextNodeEndedWithCollapsedSpace = false;
}

void TextIterator::emitText(Text& textNode, RenderText& renderer, int textStartOffset, int textEndOffset)
{
    ASSERT(textStartOffset >= 0);
    ASSERT(textEndOffset >= 0);
    ASSERT(textStartOffset <= textEndOffset);

    bool shouldEmitOriginalText = m_behaviors.contains(TextIteratorBehavior::EmitsOriginalText)
        || (m_behaviors.contains(TextIteratorBehavior::IgnoresFullSizeKana) && renderer.style().textTransform().contains(Style::TextTransformValue::FullSizeKana));

    // FIXME: This probably yields the wrong offsets when text-transform: lowercase turns a single character into two characters.
    String string = [&]() -> String {
        if (shouldEmitOriginalText)
            return renderer.originalText();
        // If this text is on the first line and ::first-line has a different text-transform
        // than the base style, apply text-transform using the first-line style.
        if (m_textRun && !m_textRun->lineIndex()) {
            CheckedRef firstLineStyle = renderer.firstLineStyle();
            if (firstLineStyle->textTransform() != renderer.style().textTransform())
                return applyTextTransform(firstLineStyle, renderer.originalText());
        }
        if (m_behaviors.contains(TextIteratorBehavior::EmitsTextsWithoutTranscoding))
            return renderer.textWithoutConvertingBackslashToYenSymbol();
        return renderer.text();
    }();

    ASSERT(shouldEmitOriginalText || string.length() >= static_cast<unsigned>(textEndOffset));

    textEndOffset = std::min(string.length(), static_cast<unsigned>(textEndOffset));

    m_positionNode = textNode;
    m_positionOffsetBaseNode = nullptr;
    // For remaining text fragments after a first-letter split, the text offsets are
    // fragment-local but position offsets need to be DOM-relative for range() to
    // return correct boundary points (used by word/sentence boundary detection).
    m_positionStartOffset = convertOffsetInTextFragmentToNodeOffset(renderer, textStartOffset);
    m_positionEndOffset = convertOffsetInTextFragmentToNodeOffset(renderer, textEndOffset);

    m_lastCharacter = string[textEndOffset - 1];
    // Reset to 0 even if the text ends with '\n', because content newlines
    // (e.g. inside <pre>) are distinct from block-boundary newlines and should
    // not suppress the extra newline required before <p> elements.
    m_consecutiveNewlineCount = 0;
    m_copyableText.set(WTF::move(string), textStartOffset, textEndOffset - textStartOffset);
    m_text = m_copyableText.text();

    m_lastTextNodeEndedWithCollapsedSpace = false;
    m_hasEmitted = true;
}

SimpleRange TextIterator::range() const
{
    ASSERT(!atEnd());
    // Use the current run information, if we have it.
    if (m_positionOffsetBaseNode) {
        unsigned index = m_positionOffsetBaseNode->computeNodeIndex();
        m_positionStartOffset += index;
        m_positionEndOffset += index;
        m_positionOffsetBaseNode = nullptr;
    }
    return { { *m_positionNode, static_cast<unsigned>(m_positionStartOffset) }, { *m_positionNode, static_cast<unsigned>(m_positionEndOffset) } };
}

Node* TextIterator::node() const
{
    auto start = this->range().start;
    if (start.container->isCharacterDataNode())
        return start.container.unsafePtr();
    return start.container->traverseToChildAt(start.offset);
}

#if ENABLE(TREE_DEBUGGING)
void TextIterator::showTreeForThis() const
{
    if (m_currentNode)
        m_currentNode->showTreeForThis();
    fprintf(stderr, "offset: %d\n", m_offset);
}
#endif

// --------

SimplifiedBackwardsTextIterator::SimplifiedBackwardsTextIterator(const SimpleRange& range)
{
    protect(range.start.document())->updateLayoutIgnorePendingStylesheets();

    RefPtr startNode = range.start.container.ptr();
    RefPtr endNode = range.end.container.ptr();
    unsigned startOffset = range.start.offset;
    unsigned endOffset = range.end.offset;

    if (!startNode->isCharacterDataNode()) {
        if (startOffset < startNode->countChildNodes()) {
            startNode = startNode->traverseToChildAt(startOffset);
            startOffset = 0;
        }
    }
    if (!endNode->isCharacterDataNode()) {
        if (endOffset > 0 && endOffset <= endNode->countChildNodes()) {
            endNode = endNode->traverseToChildAt(endOffset - 1);
            endOffset = endNode->length();
        }
    }

    m_node = endNode;
    setUpFullyClippedStack(m_fullyClippedStack, *protect(m_node), m_behaviors);
    m_offset = endOffset;
    m_handledNode = false;
    m_handledChildren = endOffset == 0;

    m_startContainer = WTF::move(startNode);
    m_startOffset = startOffset;
    m_endContainer = endNode;
    m_endOffset = endOffset;
    
    m_positionNode = WTF::move(endNode);

    m_lastTextNode = nullptr;
    m_lastCharacter = '\n';

    m_havePassedStartContainer = false;

    advance();
}

void SimplifiedBackwardsTextIterator::advance()
{
    ASSERT(!atEnd());

    m_positionNode = nullptr;
    m_copyableText.reset();
    m_text = StringView();

    while (m_node && !m_havePassedStartContainer) {
        // Don't handle node if we start iterating at [node, 0].
        if (!m_handledNode && !(m_node == m_endContainer && !m_endOffset)) {
            CheckedPtr renderer = m_node->renderer();
            if (CheckedPtr renderText = dynamicDowncast<RenderText>(renderer.get())) {
                if (renderText->style().visibility() == Visibility::Visible && m_offset > 0)
                    m_handledNode = handleTextNode();
            } else if (isRendererReplacedElement(renderer.get(), m_behaviors) && (renderer->isInline() || !m_behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec))) {
                if (downcast<RenderElement>(*renderer).style().visibility() == Visibility::Visible && m_offset > 0)
                    m_handledNode = handleReplacedElement();
            } else
                m_handledNode = handleNonTextNode();
            if (m_positionNode)
                return;
        }

        if (!m_handledChildren && m_node->hasChildNodes()) {
            m_node = m_node->lastChild();
            pushFullyClippedState(m_fullyClippedStack, *protect(m_node), m_behaviors);
        } else {
            // Exit empty containers as we pass over them or containers
            // where [container, 0] is where we started iterating.
            if (!m_handledNode && canHaveChildrenForEditing(*protect(m_node)) && m_node->parentNode() && (!m_node->lastChild() || (m_node == m_endContainer && !m_endOffset))) {
                exitNode();
                if (m_positionNode) {
                    m_handledNode = true;
                    m_handledChildren = true;
                    return;
                }
            }

            // Exit all other containers.
            while (!m_node->previousSibling()) {
                if (!advanceRespectingRange(protect(m_node->parentOrShadowHostNode()).get()))
                    break;
                m_fullyClippedStack.pop();
                exitNode();
                if (m_positionNode) {
                    m_handledNode = true;
                    m_handledChildren = true;
                    return;
                }
            }

            m_fullyClippedStack.pop();
            if (advanceRespectingRange(protect(m_node->previousSibling()).get()))
                pushFullyClippedState(m_fullyClippedStack, *protect(m_node), m_behaviors);
            else
                m_node = nullptr;
        }

        // For the purpose of word boundary detection,
        // we should iterate all visible text and trailing (collapsed) whitespaces. 
        m_offset = m_node ? maxOffsetIncludingCollapsedSpaces(*protect(m_node)) : 0;
        m_handledNode = false;
        m_handledChildren = false;
        
        if (m_positionNode)
            return;
    }
}

bool SimplifiedBackwardsTextIterator::handleTextNode()
{
    m_lastTextNode = downcast<Text>(m_node.get());

    int startOffset;
    int offsetInNode;
    CheckedPtr renderer = handleFirstLetter(startOffset, offsetInNode);
    if (!renderer)
        return true;

    String text = renderer->text();
    if (!renderer->hasRenderedText() && text.length())
        return true;

    if (startOffset + offsetInNode == m_offset) {
        ASSERT(!m_shouldHandleFirstLetter);
        return true;
    }

    m_positionEndOffset = m_offset;
    m_offset = startOffset + offsetInNode;
    m_positionNode = m_node;
    m_positionStartOffset = m_offset;

    ASSERT(m_positionStartOffset < m_positionEndOffset);
    ASSERT(m_positionStartOffset - offsetInNode >= 0);
    ASSERT(m_positionEndOffset - offsetInNode > 0);
    ASSERT(m_positionEndOffset - offsetInNode <= static_cast<int>(text.length()));

    m_lastCharacter = text[m_positionEndOffset - offsetInNode - 1];
    m_copyableText.set(WTF::move(text), m_positionStartOffset - offsetInNode, m_positionEndOffset - m_positionStartOffset);
    m_text = m_copyableText.text();

    return !m_shouldHandleFirstLetter;
}

CheckedPtr<RenderText> SimplifiedBackwardsTextIterator::handleFirstLetter(int& startOffset, int& offsetInNode)
{
    CheckedRef renderer = downcast<RenderText>(*m_node->renderer());
    startOffset = (m_node == m_startContainer) ? m_startOffset : 0;

    CheckedPtr fragment = dynamicDowncast<RenderTextFragment>(renderer);
    if (!fragment) {
        offsetInNode = 0;
        return renderer;
    }

    int offsetAfterFirstLetter = fragment->start();
    if (startOffset >= offsetAfterFirstLetter) {
        ASSERT(!m_shouldHandleFirstLetter);
        offsetInNode = offsetAfterFirstLetter;
        return renderer;
    }

    if (!m_shouldHandleFirstLetter && startOffset + offsetAfterFirstLetter < m_offset) {
        m_shouldHandleFirstLetter = true;
        offsetInNode = offsetAfterFirstLetter;
        return renderer;
    }

    m_shouldHandleFirstLetter = false;
    offsetInNode = 0;
    CheckedPtr firstLetterRenderer = firstRenderTextInFirstLetter(protect(fragment->firstLetter()));
    if (!firstLetterRenderer)
        return nullptr;

    m_offset = firstLetterRenderer->caretMaxOffset();
    m_offset += collapsedSpaceLength(*firstLetterRenderer, m_offset);

    return firstLetterRenderer;
}

bool SimplifiedBackwardsTextIterator::handleReplacedElement()
{
    unsigned index = m_node->computeNodeIndex();
    // We want replaced elements to behave like punctuation for boundary
    // finding, and to simply take up space for the selection preservation
    // code in moveParagraphs, so we use a comma. Unconditionally emit
    // here because this iterator is only used for boundary finding.
    emitCharacter(',', protect(m_node->parentNode()), index, index + 1);
    return true;
}

bool SimplifiedBackwardsTextIterator::handleNonTextNode()
{
    RefPtr currentNode = m_node;
    if (shouldEmitTabBeforeNode(*currentNode)) {
        unsigned index = currentNode->computeNodeIndex();
        emitCharacter('\t', protect(currentNode->parentNode()), index + 1, index + 1);
    } else if (shouldEmitNewlineForNode(currentNode.get(), m_behaviors.contains(TextIteratorBehavior::EmitsOriginalText)) || shouldEmitNewlineAfterNode(*protect(m_node))) {
        if (m_lastCharacter != '\n') {
            // Corresponds to the same check in TextIterator::exitNode.
            unsigned index = currentNode->computeNodeIndex();
            // The start of this emitted range is wrong. Ensuring correctness would require
            // VisiblePositions and so would be slow. previousBoundary expects this.
            emitCharacter('\n', protect(currentNode->parentNode()), index + 1, index + 1);
        }
    }
    return true;
}

void SimplifiedBackwardsTextIterator::exitNode()
{
    RefPtr node = m_node;
    if (shouldEmitTabBeforeNode(*node))
        emitCharacter('\t', WTF::move(node), 0, 0);
    else if (shouldEmitNewlineForNode(node.get(), m_behaviors.contains(TextIteratorBehavior::EmitsOriginalText)) || shouldEmitNewlineBeforeNode(*m_node)) {
        // The start of this emitted range is wrong. Ensuring correctness would require
        // VisiblePositions and so would be slow. previousBoundary expects this.
        emitCharacter('\n', WTF::move(node), 0, 0);
    }
}

void SimplifiedBackwardsTextIterator::emitCharacter(char16_t c, RefPtr<Node>&& node, int startOffset, int endOffset)
{
    ASSERT(node);
    m_positionNode = WTF::move(node);
    m_positionStartOffset = startOffset;
    m_positionEndOffset = endOffset;
    m_copyableText.set(c);
    m_text = m_copyableText.text();
    m_lastCharacter = c;
}

bool SimplifiedBackwardsTextIterator::advanceRespectingRange(Node* next)
{
    if (!next)
        return false;
    m_havePassedStartContainer |= m_node == m_startContainer;
    if (m_havePassedStartContainer)
        return false;
    m_node = next;
    return true;
}

SimpleRange SimplifiedBackwardsTextIterator::range() const
{
    ASSERT(!atEnd());

    Ref positionNode = *m_positionNode;
    return { { positionNode.copyRef(), static_cast<unsigned>(m_positionStartOffset) }, { positionNode.copyRef(), static_cast<unsigned>(m_positionEndOffset) } };
}

// --------

CharacterIterator::CharacterIterator(const SimpleRange& range, TextIteratorBehaviors behaviors)
    : m_underlyingIterator(range, behaviors)
{
    while (!atEnd() && !m_underlyingIterator.text().length())
        m_underlyingIterator.advance();
}

SimpleRange CharacterIterator::range() const
{
    SimpleRange range = m_underlyingIterator.range();
    if (!m_underlyingIterator.atEnd()) {
        if (m_underlyingIterator.text().length() <= 1)
            ASSERT(m_runOffset == 0);
        else {
            unsigned offset = range.startOffset() + m_runOffset;
            range = { { range.start.container.copyRef(), offset }, { range.start.container.copyRef(), offset + 1 } };
        }
    }
    return range;
}

void CharacterIterator::advance(int count)
{
    if (count <= 0) {
        ASSERT(count == 0);
        return;
    }
    
    m_atBreak = false;

    // easy if there is enough left in the current m_underlyingIterator run
    int remaining = m_underlyingIterator.text().length() - m_runOffset;
    if (count < remaining) {
        m_runOffset += count;
        m_offset += count;
        return;
    }

    // exhaust the current m_underlyingIterator run
    count -= remaining;
    m_offset += remaining;
    
    // move to a subsequent m_underlyingIterator run
    for (m_underlyingIterator.advance(); !atEnd(); m_underlyingIterator.advance()) {
        int runLength = m_underlyingIterator.text().length();
        if (!runLength)
            m_atBreak = true;
        else {
            // see whether this is m_underlyingIterator to use
            if (count < runLength) {
                m_runOffset = count;
                m_offset += count;
                return;
            }
            
            // exhaust this m_underlyingIterator run
            count -= runLength;
            m_offset += runLength;
        }
    }

    // ran to the end of the m_underlyingIterator... no more runs left
    m_atBreak = true;
    m_runOffset = 0;
}

BackwardsCharacterIterator::BackwardsCharacterIterator(const SimpleRange& range)
    : m_underlyingIterator(range)
{
    while (!atEnd() && !m_underlyingIterator.text().length())
        m_underlyingIterator.advance();
}

SimpleRange BackwardsCharacterIterator::range() const
{
    auto range = m_underlyingIterator.range();
    if (!m_underlyingIterator.atEnd()) {
        if (m_underlyingIterator.text().length() <= 1)
            ASSERT(m_runOffset == 0);
        else {
            unsigned offset = range.end.offset - m_runOffset;
            range = { { range.start.container.copyRef(), offset - 1 }, { range.start.container.copyRef(), offset } };
        }
    }
    return range;
}

void BackwardsCharacterIterator::advance(int count)
{
    if (count <= 0) {
        ASSERT(!count);
        return;
    }

    m_atBreak = false;

    int remaining = m_underlyingIterator.text().length() - m_runOffset;
    if (count < remaining) {
        m_runOffset += count;
        m_offset += count;
        return;
    }

    count -= remaining;
    m_offset += remaining;

    for (m_underlyingIterator.advance(); !atEnd(); m_underlyingIterator.advance()) {
        int runLength = m_underlyingIterator.text().length();
        if (runLength == 0)
            m_atBreak = true;
        else {
            if (count < runLength) {
                m_runOffset = count;
                m_offset += count;
                return;
            }
            
            count -= runLength;
            m_offset += runLength;
        }
    }

    m_atBreak = true;
    m_runOffset = 0;
}

// --------

WordAwareIterator::WordAwareIterator(const SimpleRange& range)
    : m_underlyingIterator(range)
{
    advance(); // get in position over the first chunk of text
}

// We're always in one of these modes:
// - The current chunk in the text iterator is our current chunk
//      (typically its a piece of whitespace, or text that ended with whitespace)
// - The previous chunk in the text iterator is our current chunk
//      (we looked ahead to the next chunk and found a word boundary)
// - We built up our own chunk of text from many chunks from the text iterator

// FIXME: Performance could be bad for huge spans next to each other that don't fall on word boundaries.

void WordAwareIterator::advance()
{
    m_previousText.reset();
    m_buffer.clear();

    // If last time we did a look-ahead, start with that looked-ahead chunk now
    if (!m_didLookAhead) {
        ASSERT(!m_underlyingIterator.atEnd());
        m_underlyingIterator.advance();
    }
    m_didLookAhead = false;

    // Go to next non-empty chunk 
    while (!m_underlyingIterator.atEnd() && !m_underlyingIterator.text().length())
        m_underlyingIterator.advance();
    if (m_underlyingIterator.atEnd())
        return;

    while (1) {
        // If this chunk ends in whitespace we can just use it as our chunk.
        if (deprecatedIsSpaceOrNewline(m_underlyingIterator.text()[m_underlyingIterator.text().length() - 1]))
            return;

        // If this is the first chunk that failed, save it in previousText before look ahead
        if (m_buffer.isEmpty())
            m_previousText = m_underlyingIterator.copyableText();

        // Look ahead to next chunk. If it is whitespace or a break, we can use the previous stuff
        m_underlyingIterator.advance();
        if (m_underlyingIterator.atEnd() || !m_underlyingIterator.text().length() || deprecatedIsSpaceOrNewline(m_underlyingIterator.text()[0])) {
            m_didLookAhead = true;
            return;
        }

        if (m_buffer.isEmpty()) {
            // Start gobbling chunks until we get to a suitable stopping point.
            append(m_buffer, m_previousText.text());
            m_previousText.reset();
        }
        append(m_buffer, m_underlyingIterator.text());
    }
}

StringView WordAwareIterator::text() const LIFETIME_BOUND
{
    if (!m_buffer.isEmpty())
        return m_buffer.span();
    if (m_previousText.text().length())
        return m_previousText.text();
    return m_underlyingIterator.text();
}

#if !UCONFIG_NO_COLLATION

constexpr size_t minimumSearchBufferSize = 8192;

inline SearchBuffer::SearchBuffer(const String& target, FindOptions options)
    : m_target(foldQuoteMarks(target))
    , m_targetCharacters(StringView(m_target).upconvertedCharacters())
    , m_options(options)
    , m_prefixLength(0)
    , m_atBreak(true)
    , m_needsMoreContext(options.contains(FindOption::AtWordStarts))
    , m_targetRequiresKanaWorkaround(containsKanaLetters(m_target))
    , m_ICUSearcher(m_target, m_options)
{
    ASSERT(!m_target.isEmpty());

    size_t targetLength = m_target.length();
    m_buffer.reserveInitialCapacity(std::max(targetLength * 8, minimumSearchBufferSize));
    m_overlap = m_buffer.capacity() / 4;

    m_needsMoreContext = m_options.contains(FindOption::AtWordStarts);

    m_ICUSearcher.setPattern(m_targetCharacters.span());

    // The kana workaround requires a normalized copy of the target string.
    if (m_targetRequiresKanaWorkaround)
        normalizeCharacters(m_targetCharacters, targetLength, m_normalizedTarget);
}

inline size_t SearchBuffer::append(StringView text)
{
    ASSERT(text.length());

    if (m_atBreak) {
        m_buffer.shrink(0);
        m_prefixLength = 0;
        m_atBreak = false;
    } else if (m_buffer.size() == m_buffer.capacity()) {
        memcpySpan(m_buffer.mutableSpan(), m_buffer.span().last(m_overlap));
        m_prefixLength -= std::min(m_prefixLength, m_buffer.size() - m_overlap);
        m_buffer.shrink(m_overlap);
    }

    size_t oldLength = m_buffer.size();
    size_t usableLength = std::min<size_t>(m_buffer.capacity() - oldLength, text.length());
    ASSERT(usableLength);
    m_buffer.grow(oldLength + usableLength);
    for (unsigned i = 0; i < usableLength; ++i)
        m_buffer[oldLength + i] = foldQuoteMarkAndReplaceNoBreakSpace(text[i]);
    return usableLength;
}

inline bool NODELETE SearchBuffer::needsMoreContext() const
{
    return m_needsMoreContext;
}

inline void SearchBuffer::prependContext(StringView text)
{
    ASSERT(m_needsMoreContext);
    ASSERT(m_prefixLength == m_buffer.size());

    if (!text.length())
        return;

    m_atBreak = false;

    size_t wordBoundaryContextStart = text.length();
    if (wordBoundaryContextStart) {
        U16_BACK_1(text, 0, wordBoundaryContextStart);
        wordBoundaryContextStart = startOfLastWordBoundaryContext(text.left(wordBoundaryContextStart));
    }

    size_t usableLength = std::min(m_buffer.capacity() - m_prefixLength, text.length() - wordBoundaryContextStart);
    WTF::append(m_buffer, text.substring(text.length() - usableLength, usableLength));
    m_prefixLength += usableLength;

    if (wordBoundaryContextStart || m_prefixLength == m_buffer.capacity())
        m_needsMoreContext = false;
}

inline bool SearchBuffer::atBreak() const
{
    return m_atBreak;
}

inline void NODELETE SearchBuffer::reachedBreak()
{
    m_atBreak = true;
}
    
inline size_t SearchBuffer::search(size_t& start)
{
    size_t size = m_buffer.size();
    if (m_atBreak) {
        if (!size)
            return 0;
    } else {
        if (size != m_buffer.capacity())
            return 0;
    }

    m_ICUSearcher.setText(m_buffer.span().first(size));
    m_ICUSearcher.setOffset(m_prefixLength);
    std::optional matchStart = m_ICUSearcher.next();

nextMatch:
    if (!matchStart || *matchStart >= size)
        return 0;

    // Matches that start in the overlap area are only tentative.
    // The same match may appear later, matching more characters,
    // possibly including a combining character that's not yet in the buffer.
    if (!m_atBreak && *matchStart >= size - m_overlap) {
        size_t overlap = m_overlap;
        if (m_options.contains(FindOption::AtWordStarts)) {
            // Ensure that there is sufficient context before matchStart the next time around for
            // determining if it is at a word boundary.
            size_t wordBoundaryContextStart = *matchStart;
            U16_BACK_1(m_buffer.span(), 0, wordBoundaryContextStart);
            wordBoundaryContextStart = startOfLastWordBoundaryContext(m_buffer.subspan(0, wordBoundaryContextStart));
            overlap = std::min(size - 1, std::max(overlap, size - wordBoundaryContextStart));
        }
        memcpySpan(m_buffer.mutableSpan(), m_buffer.span().last(overlap));
        m_prefixLength -= std::min(m_prefixLength, size - overlap);
        m_buffer.shrink(overlap);
        return 0;
    }

    size_t matchedLength = m_ICUSearcher.matchedLength();
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(*matchStart + matchedLength <= size);

    // If this match is "bad", move on to the next match.
    if ((m_targetRequiresKanaWorkaround && isBadMatch(m_buffer.subspan(*matchStart, matchedLength), m_normalizedTarget.span(), m_normalizedMatch))
        || (m_options.contains(FindOption::AtWordStarts) && !isWordStartMatch(m_buffer, *matchStart, matchedLength, m_options))
        || (m_options.contains(FindOption::AtWordEnds) && !isWordEndMatch(m_buffer, *matchStart, matchedLength, m_options))) {
        matchStart = m_ICUSearcher.next();
        goto nextMatch;
    }

    size_t newSize = size - (*matchStart + 1);
    memmoveSpan(m_buffer.mutableSpan(), m_buffer.subspan(*matchStart + 1, newSize));
    m_prefixLength -= std::min<size_t>(m_prefixLength, *matchStart + 1);
    m_buffer.shrink(newSize);

    start = size - *matchStart;
    return matchedLength;
}

#else

inline SearchBuffer::SearchBuffer(const String& target, FindOptions options)
    : m_target(foldQuoteMarks(options & CaseInsensitive ? target.foldCase() : target))
    , m_options(options)
    , m_buffer(m_target.length())
    , m_isCharacterStartBuffer(m_target.length())
    , m_isBufferFull(false)
    , m_cursor(0)
{
    ASSERT(!m_target.isEmpty());
    m_target.replace(noBreakSpace, ' ');
}

inline SearchBuffer::~SearchBuffer() = default;

inline void SearchBuffer::reachedBreak()
{
    m_cursor = 0;
    m_isBufferFull = false;
}

inline bool SearchBuffer::atBreak() const
{
    return !m_cursor && !m_isBufferFull;
}

inline void SearchBuffer::append(char16_t c, bool isStart)
{
    m_buffer[m_cursor] = foldQuoteMarkAndReplaceNoBreakSpace(c);
    m_isCharacterStartBuffer[m_cursor] = isStart;
    if (++m_cursor == m_target.length()) {
        m_cursor = 0;
        m_isBufferFull = true;
    }
}

inline size_t SearchBuffer::append(const char16_t* characters, size_t length)
{
    ASSERT(length);
    if (!(m_options & CaseInsensitive)) {
        append(characters[0], true);
        return 1;
    }
    constexpr int maxFoldedCharacters = 16; // sensible maximum is 3, this should be more than enough
    char16_t foldedCharacters[maxFoldedCharacters];
    UErrorCode status = U_ZERO_ERROR;
    int numFoldedCharacters = u_strFoldCase(foldedCharacters, maxFoldedCharacters, characters, 1, U_FOLD_CASE_DEFAULT, &status);
    ASSERT(U_SUCCESS(status));
    ASSERT(numFoldedCharacters);
    ASSERT(numFoldedCharacters <= maxFoldedCharacters);
    if (U_SUCCESS(status) && numFoldedCharacters) {
        numFoldedCharacters = std::min(numFoldedCharacters, maxFoldedCharacters);
        append(foldedCharacters[0], true);
        for (int i = 1; i < numFoldedCharacters; ++i)
            append(foldedCharacters[i], false);
    }
    return 1;
}

inline bool SearchBuffer::needsMoreContext() const
{
    return false;
}

void SearchBuffer::prependContext(const char16_t*, size_t)
{
    ASSERT_NOT_REACHED();
}

inline size_t SearchBuffer::search(size_t& start)
{
    if (!m_isBufferFull)
        return 0;
    if (!m_isCharacterStartBuffer[m_cursor])
        return 0;

    size_t tailSpace = m_target.length() - m_cursor;
    if (memcmp(&m_buffer[m_cursor], m_target.characters(), tailSpace * sizeof(char16_t)) != 0)
        return 0;
    if (memcmp(&m_buffer[0], m_target.characters() + tailSpace, m_cursor * sizeof(char16_t)) != 0)
        return 0;

    start = length();

    // Now that we've found a match once, we don't want to find it again, because those
    // are the SearchBuffer semantics, allowing for a buffer where you append more than one
    // character at a time. To do this we take advantage of m_isCharacterStartBuffer, but if
    // we want to get rid of that in the future we could track this with a separate boolean
    // or even move the characters to the start of the buffer and set m_isBufferFull to false.
    m_isCharacterStartBuffer[m_cursor] = false;

    return start;
}

// Returns the number of characters that were appended to the buffer (what we are searching in).
// That's not necessarily the same length as the passed-in target string, because case folding
// can make two strings match even though they're not the same length.
size_t SearchBuffer::length() const
{
    size_t bufferSize = m_target.length();
    size_t length = 0;
    for (size_t i = 0; i < bufferSize; ++i)
        length += m_isCharacterStartBuffer[i];
    return length;
}

#endif

// --------

uint64_t characterCount(const SimpleRange& range, TextIteratorBehaviors behaviors)
{
    auto adjustedRange = range;
    auto ordering = treeOrder<ComposedTree>(range.start, range.end);
    if (is_gt(ordering))
        std::swap(adjustedRange.start, adjustedRange.end);
    else if (!is_lt(ordering))
        return 0;
    uint64_t length = 0;
    for (TextIterator it(adjustedRange, behaviors); !it.atEnd(); it.advance())
        length += it.text().length();
    return length;
}

static inline bool isInsideReplacedElement(TextIterator& iterator, TextIteratorBehaviors behaviors)
{
    ASSERT(!iterator.atEnd());
    ASSERT(iterator.text().length() == 1);
    RefPtr node = iterator.node();
    return node && isRendererReplacedElement(protect(node->renderer()), behaviors);
}

constexpr uint64_t NODELETE clampedAdd(uint64_t a, uint64_t b)
{
    auto sum = a + b;
    return sum >= a ? sum : std::numeric_limits<uint64_t>::max();
}

SimpleRange resolveCharacterRange(const SimpleRange& scope, CharacterRange range, TextIteratorBehaviors behaviors)
{
    auto resultRange = SimpleRange { range.location ? scope.end : scope.start, (range.location || range.length) ? scope.end : scope.start };
    auto rangeEnd = clampedAdd(range.location, range.length);
    uint64_t location = 0;
    for (TextIterator it(scope, behaviors); !it.atEnd(); it.advance()) {
        unsigned length = it.text().length();
        auto textRunRange = it.range();

        auto found = [&] (uint64_t targetLocation) -> bool {
            return targetLocation >= location && targetLocation - location <= length;
        };
        bool foundStart = found(range.location);
        bool foundEnd = found(rangeEnd);

        if (foundEnd) {
            // FIXME: This is a workaround for the fact that the end of a run is often at the wrong position for emitted '\n's or if the renderer of the current node is a replaced element.
            // FIXME: consider controlling this with TextIteratorBehavior instead of doing it unconditionally to help us eventually phase it out everywhere.
            if (length == 1 && (it.text()[0] == '\n' || isInsideReplacedElement(it, behaviors))) {
                it.advance();
                if (!it.atEnd())
                    textRunRange.end = it.range().start;
                else {
                    if (auto end = makeBoundaryPoint(VisiblePosition(makeDeprecatedLegacyPosition(textRunRange.start)).next().deepEquivalent()))
                        textRunRange.end = *end;
                }
            }
        }

        auto boundary = [&] (uint64_t targetLocation) -> BoundaryPoint {
            if (RefPtr textNode = dynamicDowncast<Text>(textRunRange.start.container)) {
                ASSERT(targetLocation - location <= textNode->length());
                unsigned offset = textRunRange.start.offset + targetLocation - location;
                return { textNode.releaseNonNull(), offset };
            }
            return targetLocation == location ? textRunRange.start : textRunRange.end;
        };

        if (foundStart)
            resultRange.start = boundary(range.location);
        if (foundEnd) {
            resultRange.end = boundary(rangeEnd);
            break;
        }

        location += length;
    }
    return resultRange;
}

// --------

bool hasAnyPlainText(const SimpleRange& range, TextIteratorBehaviors behaviors, IgnoreCollapsedRanges ignoreCollapsedRanges)
{
    for (TextIterator iterator { range, behaviors }; !iterator.atEnd(); iterator.advance()) {
        if (ignoreCollapsedRanges == IgnoreCollapsedRanges::Yes && iterator.range().collapsed())
            continue;

        if (!iterator.text().isEmpty())
            return true;
    }
    return false;
}

String plainText(const SimpleRange& range, TextIteratorBehaviors defaultBehavior, bool isDisplayString)
{
    // The initial buffer size can be critical for performance: https://bugs.webkit.org/show_bug.cgi?id=81192
    constexpr unsigned initialCapacity = 1 << 15;

    Ref document = range.start.document();

    TextIteratorBehaviors behaviors = defaultBehavior;
    if (!isDisplayString)
        behaviors.add(TextIteratorBehavior::EmitsTextsWithoutTranscoding);

    TextIterator it(range, behaviors);
    if (it.atEnd())
        return emptyString();

    bool stripsTrailingBlockNewlines = behaviors.contains(TextIteratorBehavior::EmitsNewlinesPerInnerTextSpec);
    StringBuilder builder;
    builder.reserveCapacity(initialCapacity);
    unsigned trailingBlockNewlines = 0;

    for (; !it.atEnd(); it.advance()) {
        it.appendTextToStringBuilder(builder);
        if (stripsTrailingBlockNewlines) {
            if (it.isBlockNewline())
                ++trailingBlockNewlines;
            else
                trailingBlockNewlines = 0;
        }
    }

    if (trailingBlockNewlines)
        builder.shrink(builder.length() - trailingBlockNewlines);

    if (builder.isEmpty())
        return emptyString();

    String result = builder.toString();

    if (isDisplayString)
        document->displayStringModifiedByEncoding(result);

    return result;
}

String plainTextReplacingNoBreakSpace(const SimpleRange& range, TextIteratorBehaviors defaultBehaviors, bool isDisplayString)
{
    return makeStringByReplacingAll(plainText(range, defaultBehaviors, isDisplayString), noBreakSpace, ' ');
}

static void forEachMatch(const SimpleRange& range, const String& target, FindOptions options, NOESCAPE const Function<bool(CharacterRange)>& match)
{
    SearchBuffer buffer(target, options);
    if (buffer.needsMoreContext()) {
        auto beforeStartRange = SimpleRange { makeBoundaryPointBeforeNodeContents(protect(range.start.document())), range.start };
        for (SimplifiedBackwardsTextIterator backwardsIterator(beforeStartRange); !backwardsIterator.atEnd(); backwardsIterator.advance()) {
            buffer.prependContext(backwardsIterator.text());
            if (!buffer.needsMoreContext())
                break;
        }
    }

    CharacterIterator findIterator(range, findIteratorOptions(options));
    while (!findIterator.atEnd()) {
        findIterator.advance(buffer.append(findIterator.text()));
        while (1) {
            size_t matchStartOffset;
            size_t newMatchLength = buffer.search(matchStartOffset);
            if (!newMatchLength) {
                if (findIterator.atBreak() && !buffer.atBreak()) {
                    buffer.reachedBreak();
                    continue;
                }
                break;
            }
            size_t lastCharacterInBufferOffset = findIterator.characterOffset();
            ASSERT(lastCharacterInBufferOffset >= matchStartOffset);
            if (match(CharacterRange(lastCharacterInBufferOffset - matchStartOffset, newMatchLength)))
                return;
        }
    }
}

static SimpleRange rangeForMatch(const SimpleRange& range, FindOptions options, CharacterRange match)
{
    auto noMatchResult = [&] () {
        auto& boundary = options.contains(FindOption::Backwards) ? range.start : range.end;
        return SimpleRange { boundary, boundary };
    };

    if (!match.length)
        return noMatchResult();

    CharacterIterator it(range, findIteratorOptions(options));

    it.advance(match.location);
    if (it.atEnd())
        return noMatchResult();
    auto start = it.range().start;

    it.advance(match.length - 1);
    if (it.atEnd())
        return noMatchResult();

    return { WTF::move(start), it.range().end };
}

SimpleRange findClosestPlainText(const SimpleRange& range, const String& target, FindOptions options, uint64_t targetOffset)
{
    CharacterRange closestMatch;
    uint64_t closestMatchDistance = std::numeric_limits<uint64_t>::max();
    forEachMatch(range, target, options, [&] (CharacterRange match) {
        auto distance = [] (uint64_t a, uint64_t b) -> uint64_t {
            return std::abs(static_cast<int64_t>(a - b));
        };
        auto matchDistance = std::min(distance(match.location, targetOffset), distance(match.location + match.length, targetOffset));
        if (matchDistance > closestMatchDistance)
            return false;
        if (matchDistance == closestMatchDistance && !options.contains(FindOption::Backwards))
            return false;
        closestMatch = match;
        if (!matchDistance && !options.contains(FindOption::Backwards))
            return true;
        closestMatchDistance = matchDistance;
        return false;
    });
    return rangeForMatch(range, options, closestMatch);
}

Vector<SimpleRange> findAllPlainText(const SimpleRange& range, const String& target, FindOptions options, unsigned limit)
{
    Vector<SimpleRange> matches;
    CharacterIterator it(range, findIteratorOptions(options));
    size_t currentCharacterIndex = 0;

    auto extractRange = [&](const CharacterRange& match) -> std::optional<SimpleRange> {
        if (it.atEnd())
            return std::nullopt;
        auto start = it.range().start;
        if (match.length > 1) {
            // Advance to the last character of the match. We subtract 1 because
            // it.range().end gives us the boundary *after* the current character,
            it.advance(match.length - 1);
            currentCharacterIndex += match.length - 1;
        }
        if (it.atEnd())
            return std::nullopt;
        auto end = it.range().end;
        return { { WTF::move(start), WTF::move(end) } };
    };

    forEachMatch(range, target, options, [&] (CharacterRange match) {
        // Advance iterator to match start position
        if (currentCharacterIndex < match.location) {
            it.advance(match.location - currentCharacterIndex);
            currentCharacterIndex = match.location;
        }

        auto foundRange = extractRange(match);
        if (!foundRange)
            return true;

        matches.append(WTF::move(*foundRange));

        return limit > 0 && matches.size() >= limit;
    });

    return matches;
}

// FIXME: Do not iterate over the entire range if we are searching backwards.
SimpleRange findPlainText(const SimpleRange& range, const String& target, FindOptions options)
{
    // When searching forward stop since we want the first match.
    // When searching backward keep going since we want the last match.
    bool stopAfterFindingMatch = !options.contains(FindOption::Backwards);
    CharacterRange lastMatchFound;
    forEachMatch(range, target, options, [&] (CharacterRange match) {
        lastMatchFound = match;
        return stopAfterFindingMatch;
    });
    return rangeForMatch(range, options, lastMatchFound);
}

bool containsPlainText(const String& document, const String& target, FindOptions options)
{
    SearchBuffer buffer { target, options };
    StringView remainingText { document };
    while (!remainingText.isEmpty()) {
        size_t charactersAppended = buffer.append(document);
        remainingText = remainingText.substring(charactersAppended);
        if (remainingText.isEmpty())
            buffer.reachedBreak();
        size_t matchStartOffset;
        if (buffer.search(matchStartOffset))
            return true;
    }
    return false;
}

}

#if ENABLE(TREE_DEBUGGING)

void showTree(const WebCore::TextIterator& pos)
{
    pos.showTreeForThis();
}

void showTree(const WebCore::TextIterator* pos)
{
    if (pos)
        pos->showTreeForThis();
}

#endif
