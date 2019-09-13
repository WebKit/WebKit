/*
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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

#include "ComposedTreeIterator.h"
#include "Document.h"
#include "Editing.h"
#include "FontCascade.h"
#include "Frame.h"
#include "HTMLBodyElement.h"
#include "HTMLElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLInputElement.h"
#include "HTMLLegendElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLParagraphElement.h"
#include "HTMLProgressElement.h"
#include "HTMLSlotElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "InlineTextBox.h"
#include "NodeTraversal.h"
#include "RenderImage.h"
#include "RenderIterator.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderTextControl.h"
#include "RenderTextFragment.h"
#include "ShadowRoot.h"
#include "SimpleLineLayout.h"
#include "SimpleLineLayoutFunctions.h"
#include "SimpleLineLayoutResolver.h"
#include "TextBoundaries.h"
#include "TextControlInnerElements.h"
#include "VisiblePosition.h"
#include "VisibleUnits.h"
#include <unicode/unorm2.h>
#include <wtf/Function.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>

#if !UCONFIG_NO_COLLATION
#include <unicode/usearch.h>
#include <wtf/text/TextBreakIteratorInternalICU.h>
#endif

namespace WebCore {

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
    ~SearchBuffer();

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
    bool isBadMatch(const UChar*, size_t length) const;
    bool isWordStartMatch(size_t start, size_t length) const;
    bool isWordEndMatch(size_t start, size_t length) const;

    const String m_target;
    const StringView::UpconvertedCharacters m_targetCharacters;
    FindOptions m_options;

    Vector<UChar> m_buffer;
    size_t m_overlap;
    size_t m_prefixLength;
    bool m_atBreak;
    bool m_needsMoreContext;

    const bool m_targetRequiresKanaWorkaround;
    Vector<UChar> m_normalizedTarget;
    mutable Vector<UChar> m_normalizedMatch;

#else

private:
    void append(UChar, bool isCharacterStart);
    size_t length() const;

    String m_target;
    FindOptions m_options;

    Vector<UChar> m_buffer;
    Vector<bool> m_isCharacterStartBuffer;
    bool m_isBufferFull;
    size_t m_cursor;

#endif
};

// --------

static const unsigned bitsInWord = sizeof(unsigned) * 8;
static const unsigned bitInWordMask = bitsInWord - 1;

BitStack::BitStack()
    : m_size(0)
{
}

BitStack::~BitStack() = default;

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
    return m_words.last() & (1U << shift);
}

unsigned BitStack::size() const
{
    return m_size;
}

// --------

// This function is like Range::pastLastNode, except for the fact that it can climb up out of shadow trees.
static Node* nextInPreOrderCrossingShadowBoundaries(Node& rangeEndContainer, int rangeEndOffset)
{
    if (rangeEndOffset >= 0 && !rangeEndContainer.isCharacterDataNode()) {
        if (Node* next = rangeEndContainer.traverseToChildAt(rangeEndOffset))
            return next;
    }
    for (Node* node = &rangeEndContainer; node; node = node->parentOrShadowHostNode()) {
        if (Node* next = node->nextSibling())
            return next;
    }
    return nullptr;
}

static inline bool fullyClipsContents(Node& node)
{
    auto* renderer = node.renderer();
    if (!renderer) {
        if (!is<Element>(node))
            return false;
        return !downcast<Element>(node).hasDisplayContents();
    }
    if (!is<RenderBox>(*renderer))
        return false;
    auto& box = downcast<RenderBox>(*renderer);
    if (!box.hasOverflowClip())
        return false;

    // Quirk to keep copy/paste in the CodeMirror editor version used in Jenkins working.
    if (is<HTMLTextAreaElement>(node))
        return box.size().isEmpty();

    return box.contentSize().isEmpty();
}

static inline bool ignoresContainerClip(Node& node)
{
    auto* renderer = node.renderer();
    if (!renderer || renderer->isTextOrLineBreak())
        return false;
    return renderer->style().hasOutOfFlowPosition();
}

static void pushFullyClippedState(BitStack& stack, Node& node)
{
    // Push true if this node full clips its contents, or if a parent already has fully
    // clipped and this is not a node that ignores its container's clip.
    stack.push(fullyClipsContents(node) || (stack.top() && !ignoresContainerClip(node)));
}

static void setUpFullyClippedStack(BitStack& stack, Node& node)
{
    // Put the nodes in a vector so we can iterate in reverse order.
    // FIXME: This (and TextIterator in general) should use ComposedTreeIterator.
    Vector<Node*, 100> ancestry;
    for (Node* parent = node.parentOrShadowHostNode(); parent; parent = parent->parentOrShadowHostNode())
        ancestry.append(parent);

    // Call pushFullyClippedState on each node starting with the earliest ancestor.
    size_t size = ancestry.size();
    for (size_t i = 0; i < size; ++i)
        pushFullyClippedState(stack, *ancestry[size - i - 1]);
    pushFullyClippedState(stack, node);
}

static bool isClippedByFrameAncestor(const Document& document, TextIteratorBehavior behavior)
{
    if (!(behavior & TextIteratorClipsToFrameAncestors))
        return false;

    for (auto* owner = document.ownerElement(); owner; owner = owner->document().ownerElement()) {
        BitStack ownerClipStack;
        setUpFullyClippedStack(ownerClipStack, *owner);
        if (ownerClipStack.top())
            return true;
    }
    return false;
}

// FIXME: editingIgnoresContent and isRendererReplacedElement try to do the same job.
// It's not good to have both of them.
bool isRendererReplacedElement(RenderObject* renderer)
{
    if (!renderer)
        return false;
    
    bool isAttachment = false;
#if ENABLE(ATTACHMENT_ELEMENT)
    isAttachment = renderer->isAttachment();
#endif
    if (renderer->isImage() || renderer->isWidget() || renderer->isMedia() || isAttachment)
        return true;

    if (is<Element>(renderer->node())) {
        Element& element = downcast<Element>(*renderer->node());
        if (is<HTMLFormControlElement>(element) || is<HTMLLegendElement>(element) || is<HTMLProgressElement>(element) || element.hasTagName(meterTag))
            return true;
        if (equalLettersIgnoringASCIICase(element.attributeWithoutSynchronization(roleAttr), "img"))
            return true;
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
    m_string = WTFMove(string);
    m_offset = 0;
    m_length = m_string.length();
}

inline void TextIteratorCopyableText::set(String&& string, unsigned offset, unsigned length)
{
    ASSERT(offset < string.length());
    ASSERT(length);
    ASSERT(length <= string.length() - offset);

    m_singleCharacter = 0;
    m_string = WTFMove(string);
    m_offset = offset;
    m_length = length;
}

inline void TextIteratorCopyableText::set(UChar singleCharacter)
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


TextIterator::TextIterator(Position start, Position end, TextIteratorBehavior behavior)
    : m_behavior(behavior)
{
    if (start.isNull() || end.isNull())
        return;
    ASSERT(comparePositions(start, end) <= 0);

    RELEASE_ASSERT(behavior & TextIteratorTraversesFlatTree || start.treeScope() == end.treeScope());

    start.document()->updateLayoutIgnorePendingStylesheets();

    // FIXME: Use Position / PositionIterator instead to avoid offset computation.
    m_startContainer = start.containerNode();
    m_startOffset = start.computeOffsetInContainerNode();

    m_endContainer = end.containerNode();
    m_endOffset = end.computeOffsetInContainerNode();

    m_node = start.firstNode().get();
    if (!m_node)
        return;

    init();
}

TextIterator::TextIterator(const Range* range, TextIteratorBehavior behavior)
    : m_behavior(behavior)
{
    if (!range)
        return;

    range->ownerDocument().updateLayoutIgnorePendingStylesheets();

    m_startContainer = &range->startContainer();

    // Callers should be handing us well-formed ranges. If we discover that this isn't
    // the case, we could consider changing this assertion to an early return.
    ASSERT(range->boundaryPointsValid());

    m_startOffset = range->startOffset();
    m_endContainer = &range->endContainer();
    m_endOffset = range->endOffset();

    m_node = range->firstNode();
    if (!m_node)
        return;

    init();
}

void TextIterator::init()
{
    if (isClippedByFrameAncestor(m_node->document(), m_behavior))
        return;

    setUpFullyClippedStack(m_fullyClippedStack, *m_node);

    m_offset = m_node == m_startContainer ? m_startOffset : 0;

    m_pastEndNode = nextInPreOrderCrossingShadowBoundaries(*m_endContainer, m_endOffset);

    m_positionNode = m_node;

    advance();
}

TextIterator::~TextIterator() = default;

// FIXME: Use ComposedTreeIterator instead. These functions are more expensive because they might do O(n) work.
static inline Node* firstChild(TextIteratorBehavior options, Node& node)
{
    if (UNLIKELY(options & TextIteratorTraversesFlatTree))
        return firstChildInComposedTreeIgnoringUserAgentShadow(node);
    return node.firstChild();
}

static inline Node* nextSibling(TextIteratorBehavior options, Node& node)
{
    if (UNLIKELY(options & TextIteratorTraversesFlatTree))
        return nextSiblingInComposedTreeIgnoringUserAgentShadow(node);
    return node.nextSibling();
}

static inline Node* nextNode(TextIteratorBehavior options, Node& node)
{
    if (UNLIKELY(options & TextIteratorTraversesFlatTree))
        return nextInComposedTreeIgnoringUserAgentShadow(node);
    return NodeTraversal::next(node);
}

static inline bool isDescendantOf(TextIteratorBehavior options, Node& node, Node& possibleAncestor)
{
    if (UNLIKELY(options & TextIteratorTraversesFlatTree))
        return node.isDescendantOrShadowDescendantOf(&possibleAncestor);
    return node.isDescendantOf(&possibleAncestor);
}

static inline Node* parentNodeOrShadowHost(TextIteratorBehavior options, Node& node)
{
    if (UNLIKELY(options & TextIteratorTraversesFlatTree))
        return node.parentInComposedTree();
    return node.parentOrShadowHostNode();
}

static inline bool hasDisplayContents(Node& node)
{
    return is<Element>(node) && downcast<Element>(node).hasDisplayContents();
}

void TextIterator::advance()
{
    ASSERT(!atEnd());

    // reset the run information
    m_positionNode = nullptr;
    m_copyableText.reset();
    m_text = StringView();

    // handle remembered node that needed a newline after the text node's newline
    if (m_nodeForAdditionalNewline) {
        // Emit the extra newline, and position it *inside* m_node, after m_node's 
        // contents, in case it's a block, in the same way that we position the first 
        // newline. The range for the emitted newline should start where the line
        // break begins.
        // FIXME: It would be cleaner if we emitted two newlines during the last 
        // iteration, instead of using m_needsAnotherNewline.
        emitCharacter('\n', *m_nodeForAdditionalNewline->parentNode(), m_nodeForAdditionalNewline, 1, 1);
        m_nodeForAdditionalNewline = nullptr;
        return;
    }

    if (!m_textBox && m_remainingTextBox) {
        m_textBox = m_remainingTextBox;
        m_remainingTextBox = nullptr;
        m_firstLetterText = nullptr;
        m_offset = 0;
    }
    // handle remembered text box
    if (m_textBox) {
        handleTextBox();
        if (m_positionNode)
            return;
    }

    while (m_node && m_node != m_pastEndNode) {
        // if the range ends at offset 0 of an element, represent the
        // position, but not the content, of that element e.g. if the
        // node is a blockflow element, emit a newline that
        // precedes the element
        if (m_node == m_endContainer && !m_endOffset) {
            representNodeOffsetZero();
            m_node = nullptr;
            return;
        }
        
        auto* renderer = m_node->renderer();
        if (!m_handledNode) {
            if (!renderer) {
                m_handledNode = true;
                m_handledChildren = !hasDisplayContents(*m_node);
            } else {
                // handle current node according to its type
                if (renderer->isText() && m_node->isTextNode())
                    m_handledNode = handleTextNode();
                else if (isRendererReplacedElement(renderer))
                    m_handledNode = handleReplacedElement();
                else
                    m_handledNode = handleNonTextNode();
                if (m_positionNode)
                    return;
            }
        }

        // find a new current node to handle in depth-first manner,
        // calling exitNode() as we come back thru a parent node
        Node* next = m_handledChildren ? nullptr : firstChild(m_behavior, *m_node);
        m_offset = 0;
        if (!next) {
            next = nextSibling(m_behavior, *m_node);
            if (!next) {
                bool pastEnd = nextNode(m_behavior, *m_node) == m_pastEndNode;
                Node* parentNode = parentNodeOrShadowHost(m_behavior, *m_node);
                while (!next && parentNode) {
                    if ((pastEnd && parentNode == m_endContainer) || isDescendantOf(m_behavior, *m_endContainer, *parentNode))
                        return;
                    bool haveRenderer = m_node->renderer();
                    Node* exitedNode = m_node;
                    m_node = parentNode;
                    m_fullyClippedStack.pop();
                    parentNode = parentNodeOrShadowHost(m_behavior, *m_node);
                    if (haveRenderer)
                        exitNode(exitedNode);
                    if (m_positionNode) {
                        m_handledNode = true;
                        m_handledChildren = true;
                        return;
                    }
                    next = nextSibling(m_behavior, *m_node);
                    if (next && m_node->renderer())
                        exitNode(m_node);
                }
            }
            m_fullyClippedStack.pop();            
        }

        // set the new current node
        m_node = next;
        if (m_node)
            pushFullyClippedState(m_fullyClippedStack, *m_node);
        m_handledNode = false;
        m_handledChildren = false;
        m_handledFirstLetter = false;
        m_firstLetterText = nullptr;

        // how would this ever be?
        if (m_positionNode)
            return;
    }
}

static bool hasVisibleTextNode(RenderText& renderer)
{
    if (renderer.style().visibility() == Visibility::Visible)
        return true;
    if (is<RenderTextFragment>(renderer)) {
        if (auto firstLetter = downcast<RenderTextFragment>(renderer).firstLetter()) {
            if (firstLetter->style().visibility() == Visibility::Visible)
                return true;
        }
    }
    return false;
}

static unsigned textNodeOffsetInFlow(const Text& firstTextNodeInRange)
{
    // Calculate the text offset for simple lines.
    RenderObject* renderer = firstTextNodeInRange.renderer();
    if (!renderer)
        return 0;
    unsigned textOffset = 0;
    for (renderer = renderer->previousSibling(); renderer; renderer = renderer->previousSibling()) {
        if (is<RenderText>(renderer))
            textOffset += downcast<RenderText>(renderer)->text().length();
    }
    return textOffset;
}

static bool isNewLineOrTabCharacter(UChar character)
{
    return character == '\n' || character == '\t';
}

bool TextIterator::handleTextNode()
{
    Text& textNode = downcast<Text>(*m_node);

    if (m_fullyClippedStack.top() && !(m_behavior & TextIteratorIgnoresStyleVisibility))
        return false;

    auto& renderer = *textNode.renderer();
    m_lastTextNode = &textNode;
    String rendererText = renderer.text();

    // handle pre-formatted text
    if (!renderer.style().collapseWhiteSpace()) {
        int runStart = m_offset;
        if (m_lastTextNodeEndedWithCollapsedSpace && hasVisibleTextNode(renderer)) {
            emitCharacter(' ', textNode, nullptr, runStart, runStart);
            return false;
        }
        if (!m_handledFirstLetter && is<RenderTextFragment>(renderer) && !m_offset) {
            handleTextNodeFirstLetter(downcast<RenderTextFragment>(renderer));
            if (m_firstLetterText) {
                String firstLetter = m_firstLetterText->text();
                emitText(textNode, *m_firstLetterText, m_offset, m_offset + firstLetter.length());
                m_firstLetterText = nullptr;
                m_textBox = nullptr;
                return false;
            }
        }
        if (renderer.style().visibility() != Visibility::Visible && !(m_behavior & TextIteratorIgnoresStyleVisibility))
            return false;
        int rendererTextLength = rendererText.length();
        int end = (&textNode == m_endContainer) ? m_endOffset : INT_MAX;
        int runEnd = std::min(rendererTextLength, end);

        if (runStart >= runEnd)
            return true;

        emitText(textNode, renderer, runStart, runEnd);
        return true;
    }

    if (const auto* layout = renderer.simpleLineLayout()) {
        if (renderer.style().visibility() != Visibility::Visible && !(m_behavior & TextIteratorIgnoresStyleVisibility))
            return true;
        ASSERT(renderer.parent());
        ASSERT(is<RenderBlockFlow>(*renderer.parent()));
        const auto& blockFlow = downcast<RenderBlockFlow>(*renderer.parent());
        // Use the simple layout runs to iterate over the text content.
        bool isNewTextNode = m_previousSimpleTextNodeInFlow && m_previousSimpleTextNodeInFlow != &textNode;
        // Simple line layout run positions are all absolute to the parent flow.
        // Offsetting is required when multiple renderers are present.
        m_accumulatedSimpleTextLengthInFlow += isNewTextNode ? m_previousSimpleTextNodeInFlow->renderer()->text().length() : 0;
        m_previousSimpleTextNodeInFlow = &textNode;

        unsigned endPosition = (m_node == m_endContainer) ? static_cast<unsigned>(m_endOffset) : rendererText.length();
        if (!m_flowRunResolverCache || &m_flowRunResolverCache->flow() != &blockFlow) {
            m_accumulatedSimpleTextLengthInFlow = m_flowRunResolverCache ? 0 : textNodeOffsetInFlow(textNode);
            m_flowRunResolverCache = makeUnique<SimpleLineLayout::RunResolver>(blockFlow, *layout);
        }
        // Skip to m_offset position.
        auto range = m_flowRunResolverCache->rangeForRenderer(renderer);
        auto it = range.begin();
        auto end = range.end();
        auto startPosition = static_cast<unsigned>(m_offset) + m_accumulatedSimpleTextLengthInFlow;
        while (it != end && (*it).end() <= startPosition)
            ++it;
        if (m_nextRunNeedsWhitespace && rendererText[m_offset - 1] == '\n') {
            emitCharacter(' ', textNode, nullptr, m_offset, m_offset + 1);
            return it == end;
        }
        if (it == end) {
            // Collapsed trailing whitespace.
            m_offset = endPosition;
            m_lastTextNodeEndedWithCollapsedSpace = true;
            return true;
        }
        if (m_nextRunNeedsWhitespace) {
            emitCharacter(' ', textNode, nullptr, m_offset, m_offset + 1);
            return false;
        }
        // If the position we are looking for is to the left of the renderer's first run, it could mean that
        // the runs and the renderers are out of sync (e.g. we skipped a renderer in between).
        // Better bail out at this point.
        auto run = *it;
        if (run.start() > startPosition) {
            ASSERT(m_flowRunResolverCache);
            if (&(rendererForPosition(m_flowRunResolverCache->flowContents(), startPosition)) != &renderer) {
                ASSERT_NOT_REACHED();
                return true;
            }
        }
        ASSERT(run.end() - run.start() <= rendererText.length());
        // contentStart skips leading whitespace.
        unsigned contentStart = std::max<unsigned>(m_offset, run.start() - m_accumulatedSimpleTextLengthInFlow);
        unsigned contentEnd = std::min(endPosition, run.end() - m_accumulatedSimpleTextLengthInFlow);
        ASSERT_WITH_SECURITY_IMPLICATION(contentStart <= contentEnd);
        // Check if whitespace adjustment is needed when crossing renderer boundary.
        if (isNewTextNode) {
            bool lastCharacterIsNotWhitespace = m_lastCharacter && !renderer.style().isCollapsibleWhiteSpace(m_lastCharacter);
            bool addTrailingWhitespaceForPrevious = m_lastTextNodeEndedWithCollapsedSpace && lastCharacterIsNotWhitespace;
            bool leadingWhitespaceIsNeededForCurrent = contentStart > static_cast<unsigned>(m_offset) && lastCharacterIsNotWhitespace;
            if (addTrailingWhitespaceForPrevious || leadingWhitespaceIsNeededForCurrent) {
                emitCharacter(' ', textNode, nullptr, m_offset, m_offset + 1);
                return false;
            }
        }
        // \n \t single whitespace characters need replacing so that the new line/tab characters don't show up.
        unsigned stopPosition = contentStart;
        while (stopPosition < contentEnd && !isNewLineOrTabCharacter(rendererText[stopPosition]))
            ++stopPosition;
        // Emit the text up to the new line/tab character.
        if (stopPosition < contentEnd) {
            if (stopPosition == contentStart) {
                emitCharacter(' ', textNode, nullptr, contentStart, contentStart + 1);
                m_offset = contentStart + 1;
                return false;
            }
            emitText(textNode, renderer, contentStart, stopPosition);
            m_offset = stopPosition + 1;
            m_nextRunNeedsWhitespace = true;
            return false;
        }
        emitText(textNode, renderer, contentStart, contentEnd);
        // When line ending with collapsed whitespace is present, we need to carry over one whitespace: foo(end of line)bar -> foo bar (otherwise we would end up with foobar).
        m_nextRunNeedsWhitespace = run.isEndOfLine() && contentEnd < endPosition && renderer.style().isCollapsibleWhiteSpace(rendererText[contentEnd]);
        m_offset = contentEnd;
        return static_cast<unsigned>(m_offset) == endPosition;
    }

    if (renderer.firstTextBox())
        m_textBox = renderer.firstTextBox();

    bool shouldHandleFirstLetter = !m_handledFirstLetter && is<RenderTextFragment>(renderer) && !m_offset;
    if (shouldHandleFirstLetter)
        handleTextNodeFirstLetter(downcast<RenderTextFragment>(renderer));

    if (!renderer.firstTextBox() && rendererText.length() && !shouldHandleFirstLetter) {
        if (renderer.style().visibility() != Visibility::Visible && !(m_behavior & TextIteratorIgnoresStyleVisibility))
            return false;
        m_lastTextNodeEndedWithCollapsedSpace = true; // entire block is collapsed space
        return true;
    }

    // Used when text boxes are out of order (Hebrew/Arabic w/ embeded LTR text)
    auto& boxesRenderer = m_firstLetterText ? *m_firstLetterText : renderer;
    if (boxesRenderer.containsReversedText()) {
        m_sortedTextBoxes.clear();
        for (InlineTextBox* textBox = boxesRenderer.firstTextBox(); textBox; textBox = textBox->nextTextBox())
            m_sortedTextBoxes.append(textBox);
        std::sort(m_sortedTextBoxes.begin(), m_sortedTextBoxes.end(), InlineTextBox::compareByStart);
        m_sortedTextBoxesPosition = 0;
        m_textBox = m_sortedTextBoxes.isEmpty() ? nullptr : m_sortedTextBoxes[0];
    }

    handleTextBox();
    return true;
}

void TextIterator::handleTextBox()
{    
    Text& textNode = downcast<Text>(*m_node);

    auto& renderer = m_firstLetterText ? *m_firstLetterText : *textNode.renderer();
    if (renderer.style().visibility() != Visibility::Visible && !(m_behavior & TextIteratorIgnoresStyleVisibility)) {
        m_textBox = nullptr;
        return;
    }
    String rendererText = renderer.text();
    unsigned start = m_offset;
    unsigned end = (&textNode == m_endContainer) ? static_cast<unsigned>(m_endOffset) : UINT_MAX;
    while (m_textBox) {
        unsigned textBoxStart = m_textBox->start();
        unsigned runStart = std::max(textBoxStart, start);

        // Check for collapsed space at the start of this run.
        InlineTextBox* firstTextBox = renderer.containsReversedText() ? (m_sortedTextBoxes.isEmpty() ? nullptr : m_sortedTextBoxes[0]) : renderer.firstTextBox();
        bool needSpace = m_lastTextNodeEndedWithCollapsedSpace || (m_textBox == firstTextBox && textBoxStart == runStart && runStart);
        if (needSpace && !renderer.style().isCollapsibleWhiteSpace(m_lastCharacter) && m_lastCharacter) {
            if (m_lastTextNode == &textNode && runStart && rendererText[runStart - 1] == ' ') {
                unsigned spaceRunStart = runStart - 1;
                while (spaceRunStart && rendererText[spaceRunStart - 1] == ' ')
                    --spaceRunStart;
                emitText(textNode, renderer, spaceRunStart, spaceRunStart + 1);
            } else
                emitCharacter(' ', textNode, nullptr, runStart, runStart);
            return;
        }
        unsigned textBoxEnd = textBoxStart + m_textBox->len();
        unsigned runEnd = std::min(textBoxEnd, end);
        
        // Determine what the next text box will be, but don't advance yet
        InlineTextBox* nextTextBox = nullptr;
        if (renderer.containsReversedText()) {
            if (m_sortedTextBoxesPosition + 1 < m_sortedTextBoxes.size())
                nextTextBox = m_sortedTextBoxes[m_sortedTextBoxesPosition + 1];
        } else 
            nextTextBox = m_textBox->nextTextBox();
        ASSERT(!nextTextBox || &nextTextBox->renderer() == &renderer);

        if (runStart < runEnd) {
            // Handle either a single newline character (which becomes a space),
            // or a run of characters that does not include a newline.
            // This effectively translates newlines to spaces without copying the text.
            if (rendererText[runStart] == '\n') {
                emitCharacter(' ', textNode, nullptr, runStart, runStart + 1);
                m_offset = runStart + 1;
            } else {
                size_t subrunEnd = rendererText.find('\n', runStart);
                if (subrunEnd == notFound || subrunEnd > runEnd) {
                    subrunEnd = runEnd;
                    bool lastSpaceCollapsedByNextNonTextBox = !nextTextBox && (m_behavior & TextIteratorBehavesAsIfNodesFollowing) && rendererText.length() > runEnd;
                    if (lastSpaceCollapsedByNextNonTextBox)
                        ++subrunEnd; // runEnd stopped before last space. Increment by one to restore the space.
                }
                m_offset = subrunEnd;
                emitText(textNode, renderer, runStart, subrunEnd);
            }

            // If we are doing a subrun that doesn't go to the end of the text box,
            // come back again to finish handling this text box; don't advance to the next one.
            if (static_cast<unsigned>(m_positionEndOffset) < textBoxEnd)
                return;

            // Advance and return
            unsigned nextRunStart = nextTextBox ? nextTextBox->start() : rendererText.length();
            if (nextRunStart > runEnd)
                m_lastTextNodeEndedWithCollapsedSpace = true; // collapsed space between runs or at the end
            m_textBox = nextTextBox;
            if (renderer.containsReversedText())
                ++m_sortedTextBoxesPosition;
            return;
        }
        // Advance and continue
        m_textBox = nextTextBox;
        if (renderer.containsReversedText())
            ++m_sortedTextBoxesPosition;
    }
    if (!m_textBox && m_remainingTextBox) {
        m_textBox = m_remainingTextBox;
        m_remainingTextBox = nullptr;
        m_firstLetterText = nullptr;
        m_offset = 0;
        handleTextBox();
    }
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
    if (auto* firstLetter = renderer.firstLetter()) {
        if (firstLetter->style().visibility() != Visibility::Visible && !(m_behavior & TextIteratorIgnoresStyleVisibility))
            return;
        if (auto* firstLetterText = firstRenderTextInFirstLetter(firstLetter)) {
            m_handledFirstLetter = true;
            m_remainingTextBox = m_textBox;
            m_textBox = firstLetterText->firstTextBox();
            m_sortedTextBoxes.clear();
            m_firstLetterText = firstLetterText;
        }
    }
    m_handledFirstLetter = true;
}

bool TextIterator::handleReplacedElement()
{
    if (m_fullyClippedStack.top())
        return false;

    auto& renderer = *m_node->renderer();
    if (renderer.style().visibility() != Visibility::Visible && !(m_behavior & TextIteratorIgnoresStyleVisibility))
        return false;

    if (m_lastTextNodeEndedWithCollapsedSpace) {
        emitCharacter(' ', *m_lastTextNode->parentNode(), m_lastTextNode, 1, 1);
        return false;
    }

    if ((m_behavior & TextIteratorEntersTextControls) && is<RenderTextControl>(renderer)) {
        if (auto innerTextElement = downcast<RenderTextControl>(renderer).textFormControlElement().innerTextElement()) {
            m_node = innerTextElement->containingShadowRoot();
            pushFullyClippedState(m_fullyClippedStack, *m_node);
            m_offset = 0;
            return false;
        }
    }

    m_hasEmitted = true;

    if ((m_behavior & TextIteratorEmitsObjectReplacementCharacters) && renderer.isReplaced()) {
        emitCharacter(objectReplacementCharacter, *m_node->parentNode(), m_node, 0, 1);
        // Don't process subtrees for embedded objects. If the text there is required,
        // it must be explicitly asked by specifying a range falling inside its boundaries.
        m_handledChildren = true;
        return true;
    }

    if (m_behavior & TextIteratorEmitsCharactersBetweenAllVisiblePositions) {
        // We want replaced elements to behave like punctuation for boundary 
        // finding, and to simply take up space for the selection preservation 
        // code in moveParagraphs, so we use a comma.
        emitCharacter(',', *m_node->parentNode(), m_node, 0, 1);
        return true;
    }

    m_positionNode = m_node->parentNode();
    m_positionOffsetBaseNode = m_node;
    m_positionStartOffset = 0;
    m_positionEndOffset = 1;

    if ((m_behavior & TextIteratorEmitsImageAltText) && is<RenderImage>(renderer)) {
        String altText = downcast<RenderImage>(renderer).altText();
        if (unsigned length = altText.length()) {
            m_lastCharacter = altText[length - 1];
            m_copyableText.set(WTFMove(altText));
            m_text = m_copyableText.text();
            return true;
        }
    }

    m_copyableText.reset();
    m_text = StringView();
    m_lastCharacter = 0;
    return true;
}

static bool shouldEmitTabBeforeNode(Node& node)
{
    auto* renderer = node.renderer();
    
    // Table cells are delimited by tabs.
    if (!renderer || !isTableCell(&node))
        return false;
    
    // Want a tab before every cell other than the first one.
    RenderTableCell& cell = downcast<RenderTableCell>(*renderer);
    RenderTable* table = cell.table();
    return table && (table->cellBefore(&cell) || table->cellAbove(&cell));
}

static bool shouldEmitNewlineForNode(Node* node, bool emitsOriginalText)
{
    auto* renderer = node->renderer();
    if (!(renderer ? renderer->isBR() : node->hasTagName(brTag)))
        return false;
    return emitsOriginalText || !(node->isInShadowTree() && is<HTMLInputElement>(*node->shadowHost()));
}

static bool hasHeaderTag(HTMLElement& element)
{
    return element.hasTagName(h1Tag)
        || element.hasTagName(h2Tag)
        || element.hasTagName(h3Tag)
        || element.hasTagName(h4Tag)
        || element.hasTagName(h5Tag)
        || element.hasTagName(h6Tag);
}

static bool shouldEmitNewlinesBeforeAndAfterNode(Node& node)
{
    // Block flow (versus inline flow) is represented by having
    // a newline both before and after the element.
    auto* renderer = node.renderer();
    if (!renderer) {
        if (!is<HTMLElement>(node))
            return false;
        auto& element = downcast<HTMLElement>(node);
        return hasHeaderTag(element)
            || element.hasTagName(blockquoteTag)
            || element.hasTagName(ddTag)
            || element.hasTagName(divTag)
            || element.hasTagName(dlTag)
            || element.hasTagName(dtTag)
            || element.hasTagName(hrTag)
            || element.hasTagName(liTag)
            || element.hasTagName(listingTag)
            || element.hasTagName(olTag)
            || element.hasTagName(pTag)
            || element.hasTagName(preTag)
            || element.hasTagName(trTag)
            || element.hasTagName(ulTag);
    }
    
    // Need to make an exception for table cells, because they are blocks, but we
    // want them tab-delimited rather than having newlines before and after.
    if (isTableCell(&node))
        return false;
    
    // Need to make an exception for table row elements, because they are neither
    // "inline" or "RenderBlock", but we want newlines for them.
    if (is<RenderTableRow>(*renderer)) {
        RenderTable* table = downcast<RenderTableRow>(*renderer).table();
        if (table && !table->isInline())
            return true;
    }
    
    return !renderer->isInline()
        && is<RenderBlock>(*renderer)
        && !renderer->isFloatingOrOutOfFlowPositioned()
        && !renderer->isBody()
        && !renderer->isRubyText();
}

static bool shouldEmitNewlineAfterNode(Node& node, bool emitsCharactersBetweenAllVisiblePositions = false)
{
    // FIXME: It should be better but slower to create a VisiblePosition here.
    if (!shouldEmitNewlinesBeforeAndAfterNode(node))
        return false;

    // Don't emit a new line at the end of the document unless we're matching the behavior of VisiblePosition.
    if (emitsCharactersBetweenAllVisiblePositions)
        return true;
    Node* subsequentNode = &node;
    while ((subsequentNode = NodeTraversal::nextSkippingChildren(*subsequentNode))) {
        if (subsequentNode->renderer())
            return true;
    }
    return false;
}

static bool shouldEmitNewlineBeforeNode(Node& node)
{
    return shouldEmitNewlinesBeforeAndAfterNode(node); 
}

static bool shouldEmitExtraNewlineForNode(Node& node)
{
    // When there is a significant collapsed bottom margin, emit an extra
    // newline for a more realistic result. We end up getting the right
    // result even without margin collapsing. For example: <div><p>text</p></div>
    // will work right even if both the <div> and the <p> have bottom margins.

    auto* renderer = node.renderer();
    if (!is<RenderBox>(renderer))
        return false;

    // NOTE: We only do this for a select set of nodes, and WinIE appears not to do this at all.
    if (!is<HTMLElement>(node))
        return false;

    HTMLElement& element = downcast<HTMLElement>(node);
    if (!hasHeaderTag(element) && !is<HTMLParagraphElement>(element))
        return false;

    auto& renderBox = downcast<RenderBox>(*renderer);
    if (!renderBox.height())
        return false;

    int bottomMargin = renderBox.collapsedMarginAfter();
    int fontSize = renderBox.style().fontDescription().computedPixelSize();
    return bottomMargin * 2 >= fontSize;
}

static int collapsedSpaceLength(RenderText& renderer, int textEnd)
{
    StringImpl& text = renderer.text();
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
    if (auto* renderer = node.renderer()) {
        if (is<RenderText>(*renderer))
            offset += collapsedSpaceLength(downcast<RenderText>(*renderer), offset);
    }
    return offset;
}

// Whether or not we should emit a character as we enter m_node (if it's a container) or as we hit it (if it's atomic).
bool TextIterator::shouldRepresentNodeOffsetZero()
{
    if ((m_behavior & TextIteratorEmitsCharactersBetweenAllVisiblePositions) && m_node->renderer() && m_node->renderer()->isTable())
        return true;
        
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
    if (m_node == m_startContainer)
        return false;
    
    // If we are outside the start container's subtree, assume we need to emit.
    // FIXME: m_startContainer could be an inline block
    if (!m_node->isDescendantOf(m_startContainer))
        return true;

    // If we started as m_startContainer offset 0 and the current node is a descendant of
    // the start container, we already had enough context to correctly decide whether to
    // emit after a preceding block. We chose not to emit (m_hasEmitted is false),
    // so don't second guess that now.
    // NOTE: Is this really correct when m_node is not a leftmost descendant? Probably
    // immaterial since we likely would have already emitted something by now.
    if (m_startOffset == 0)
        return false;
        
    // If this node is unrendered or invisible the VisiblePosition checks below won't have much meaning.
    // Additionally, if the range we are iterating over contains huge sections of unrendered content, 
    // we would create VisiblePositions on every call to this function without this check.
    if (!m_node->renderer() || m_node->renderer()->style().visibility() != Visibility::Visible
        || (is<RenderBlockFlow>(*m_node->renderer()) && !downcast<RenderBlockFlow>(*m_node->renderer()).height() && !is<HTMLBodyElement>(*m_node)))
        return false;

    // The startPos.isNotNull() check is needed because the start could be before the body,
    // and in that case we'll get null. We don't want to put in newlines at the start in that case.
    // The currPos.isNotNull() check is needed because positions in non-HTML content
    // (like SVG) do not have visible positions, and we don't want to emit for them either.
    VisiblePosition startPos = VisiblePosition(Position(m_startContainer, m_startOffset, Position::PositionIsOffsetInAnchor), DOWNSTREAM);
    VisiblePosition currPos = VisiblePosition(positionBeforeNode(m_node), DOWNSTREAM);
    return startPos.isNotNull() && currPos.isNotNull() && !inSameLine(startPos, currPos);
}

bool TextIterator::shouldEmitSpaceBeforeAndAfterNode(Node& node)
{
    return node.renderer() && node.renderer()->isTable() && (node.renderer()->isInline() || (m_behavior & TextIteratorEmitsCharactersBetweenAllVisiblePositions));
}

void TextIterator::representNodeOffsetZero()
{
    // Emit a character to show the positioning of m_node.
    
    // When we haven't been emitting any characters, shouldRepresentNodeOffsetZero() can 
    // create VisiblePositions, which is expensive. So, we perform the inexpensive checks
    // on m_node to see if it necessitates emitting a character first and will early return 
    // before encountering shouldRepresentNodeOffsetZero()s worse case behavior.
    if (shouldEmitTabBeforeNode(*m_node)) {
        if (shouldRepresentNodeOffsetZero())
            emitCharacter('\t', *m_node->parentNode(), m_node, 0, 0);
    } else if (shouldEmitNewlineBeforeNode(*m_node)) {
        if (shouldRepresentNodeOffsetZero())
            emitCharacter('\n', *m_node->parentNode(), m_node, 0, 0);
    } else if (shouldEmitSpaceBeforeAndAfterNode(*m_node)) {
        if (shouldRepresentNodeOffsetZero())
            emitCharacter(' ', *m_node->parentNode(), m_node, 0, 0);
    }
}

bool TextIterator::handleNonTextNode()
{
    if (shouldEmitNewlineForNode(m_node, m_behavior & TextIteratorEmitsOriginalText))
        emitCharacter('\n', *m_node->parentNode(), m_node, 0, 1);
    else if ((m_behavior & TextIteratorEmitsCharactersBetweenAllVisiblePositions) && m_node->renderer() && m_node->renderer()->isHR())
        emitCharacter(' ', *m_node->parentNode(), m_node, 0, 1);
    else
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
        
    // Emit with a position *inside* m_node, after m_node's contents, in 
    // case it is a block, because the run should start where the 
    // emitted character is positioned visually.
    Node* baseNode = exitedNode;
    // FIXME: This shouldn't require the m_lastTextNode to be true, but we can't change that without making
    // the logic in _web_attributedStringFromRange match. We'll get that for free when we switch to use
    // TextIterator in _web_attributedStringFromRange.
    // See <rdar://problem/5428427> for an example of how this mismatch will cause problems.
    if (m_lastTextNode && shouldEmitNewlineAfterNode(*m_node, m_behavior & TextIteratorEmitsCharactersBetweenAllVisiblePositions)) {
        // use extra newline to represent margin bottom, as needed
        bool addNewline = shouldEmitExtraNewlineForNode(*m_node);
        
        // FIXME: We need to emit a '\n' as we leave an empty block(s) that
        // contain a VisiblePosition when doing selection preservation.
        if (m_lastCharacter != '\n') {
            // insert a newline with a position following this block's contents.
            emitCharacter('\n', *baseNode->parentNode(), baseNode, 1, 1);
            // remember whether to later add a newline for the current node
            ASSERT(!m_nodeForAdditionalNewline);
            if (addNewline)
                m_nodeForAdditionalNewline = baseNode;
        } else if (addNewline)
            // insert a newline with a position following this block's contents.
            emitCharacter('\n', *baseNode->parentNode(), baseNode, 1, 1);
    }
    
    // If nothing was emitted, see if we need to emit a space.
    if (!m_positionNode && shouldEmitSpaceBeforeAndAfterNode(*m_node))
        emitCharacter(' ', *baseNode->parentNode(), baseNode, 1, 1);
}

void TextIterator::emitCharacter(UChar character, Node& characterNode, Node* offsetBaseNode, int textStartOffset, int textEndOffset)
{
    m_hasEmitted = true;
    
    // remember information with which to construct the TextIterator::range()
    m_positionNode = &characterNode;
    m_positionOffsetBaseNode = offsetBaseNode;
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;

    m_copyableText.set(character);
    m_text = m_copyableText.text();
    m_lastCharacter = character;
    m_lastTextNodeEndedWithCollapsedSpace = false;
    m_nextRunNeedsWhitespace = false;
}

void TextIterator::emitText(Text& textNode, RenderText& renderer, int textStartOffset, int textEndOffset)
{
    ASSERT(textStartOffset >= 0);
    ASSERT(textEndOffset >= 0);
    ASSERT(textStartOffset <= textEndOffset);

    // FIXME: This probably yields the wrong offsets when text-transform: lowercase turns a single character into two characters.
    String string = (m_behavior & TextIteratorEmitsOriginalText) ? renderer.originalText()
        : ((m_behavior & TextIteratorEmitsTextsWithoutTranscoding) ? renderer.textWithoutConvertingBackslashToYenSymbol() : renderer.text());

    ASSERT(string.length() >= static_cast<unsigned>(textEndOffset));

    m_positionNode = &textNode;
    m_positionOffsetBaseNode = nullptr;
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;

    m_lastCharacter = string[textEndOffset - 1];
    m_copyableText.set(WTFMove(string), textStartOffset, textEndOffset - textStartOffset);
    m_text = m_copyableText.text();

    m_lastTextNodeEndedWithCollapsedSpace = false;
    m_nextRunNeedsWhitespace = false;
    m_hasEmitted = true;
}

Ref<Range> TextIterator::range() const
{
    ASSERT(!atEnd());

    // use the current run information, if we have it
    if (m_positionOffsetBaseNode) {
        unsigned index = m_positionOffsetBaseNode->computeNodeIndex();
        m_positionStartOffset += index;
        m_positionEndOffset += index;
        m_positionOffsetBaseNode = nullptr;
    }
    return Range::create(m_positionNode->document(), m_positionNode, m_positionStartOffset, m_positionNode, m_positionEndOffset);
}
    
Node* TextIterator::node() const
{
    Ref<Range> textRange = range();

    Node& node = textRange->startContainer();
    if (node.isCharacterDataNode())
        return &node;
    
    return node.traverseToChildAt(textRange->startOffset());
}

// --------

SimplifiedBackwardsTextIterator::SimplifiedBackwardsTextIterator(const Range& range)
{
    range.ownerDocument().updateLayoutIgnorePendingStylesheets();

    Node* startNode = &range.startContainer();
    Node* endNode = &range.endContainer();
    int startOffset = range.startOffset();
    int endOffset = range.endOffset();

    if (!startNode->isCharacterDataNode()) {
        if (startOffset >= 0 && startOffset < static_cast<int>(startNode->countChildNodes())) {
            startNode = startNode->traverseToChildAt(startOffset);
            startOffset = 0;
        }
    }
    if (!endNode->isCharacterDataNode()) {
        if (endOffset > 0 && endOffset <= static_cast<int>(endNode->countChildNodes())) {
            endNode = endNode->traverseToChildAt(endOffset - 1);
            endOffset = lastOffsetInNode(endNode);
        }
    }

    m_node = endNode;
    setUpFullyClippedStack(m_fullyClippedStack, *m_node);
    m_offset = endOffset;
    m_handledNode = false;
    m_handledChildren = endOffset == 0;

    m_startContainer = startNode;
    m_startOffset = startOffset;
    m_endContainer = endNode;
    m_endOffset = endOffset;
    
    m_positionNode = endNode;

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
            auto* renderer = m_node->renderer();
            if (renderer && renderer->isText() && m_node->isTextNode()) {
                if (renderer->style().visibility() == Visibility::Visible && m_offset > 0)
                    m_handledNode = handleTextNode();
            } else if (renderer && (renderer->isImage() || renderer->isWidget())) {
                if (renderer->style().visibility() == Visibility::Visible && m_offset > 0)
                    m_handledNode = handleReplacedElement();
            } else
                m_handledNode = handleNonTextNode();
            if (m_positionNode)
                return;
        }

        if (!m_handledChildren && m_node->hasChildNodes()) {
            m_node = m_node->lastChild();
            pushFullyClippedState(m_fullyClippedStack, *m_node);
        } else {
            // Exit empty containers as we pass over them or containers
            // where [container, 0] is where we started iterating.
            if (!m_handledNode && canHaveChildrenForEditing(*m_node) && m_node->parentNode() && (!m_node->lastChild() || (m_node == m_endContainer && !m_endOffset))) {
                exitNode();
                if (m_positionNode) {
                    m_handledNode = true;
                    m_handledChildren = true;
                    return;
                }
            }

            // Exit all other containers.
            while (!m_node->previousSibling()) {
                if (!advanceRespectingRange(m_node->parentOrShadowHostNode()))
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
            if (advanceRespectingRange(m_node->previousSibling()))
                pushFullyClippedState(m_fullyClippedStack, *m_node);
            else
                m_node = nullptr;
        }

        // For the purpose of word boundary detection,
        // we should iterate all visible text and trailing (collapsed) whitespaces. 
        m_offset = m_node ? maxOffsetIncludingCollapsedSpaces(*m_node) : 0;
        m_handledNode = false;
        m_handledChildren = false;
        
        if (m_positionNode)
            return;
    }
}

bool SimplifiedBackwardsTextIterator::handleTextNode()
{
    Text& textNode = downcast<Text>(*m_node);

    m_lastTextNode = &textNode;

    int startOffset;
    int offsetInNode;
    RenderText* renderer = handleFirstLetter(startOffset, offsetInNode);
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
    m_copyableText.set(WTFMove(text), m_positionStartOffset - offsetInNode, m_positionEndOffset - m_positionStartOffset);
    m_text = m_copyableText.text();

    return !m_shouldHandleFirstLetter;
}

RenderText* SimplifiedBackwardsTextIterator::handleFirstLetter(int& startOffset, int& offsetInNode)
{
    RenderText& renderer = downcast<RenderText>(*m_node->renderer());
    startOffset = (m_node == m_startContainer) ? m_startOffset : 0;

    if (!is<RenderTextFragment>(renderer)) {
        offsetInNode = 0;
        return &renderer;
    }

    RenderTextFragment& fragment = downcast<RenderTextFragment>(renderer);
    int offsetAfterFirstLetter = fragment.start();
    if (startOffset >= offsetAfterFirstLetter) {
        ASSERT(!m_shouldHandleFirstLetter);
        offsetInNode = offsetAfterFirstLetter;
        return &renderer;
    }

    if (!m_shouldHandleFirstLetter && startOffset + offsetAfterFirstLetter < m_offset) {
        m_shouldHandleFirstLetter = true;
        offsetInNode = offsetAfterFirstLetter;
        return &renderer;
    }

    m_shouldHandleFirstLetter = false;
    offsetInNode = 0;
    RenderText* firstLetterRenderer = firstRenderTextInFirstLetter(fragment.firstLetter());

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
    emitCharacter(',', *m_node->parentNode(), index, index + 1);
    return true;
}

bool SimplifiedBackwardsTextIterator::handleNonTextNode()
{    
    // We can use a linefeed in place of a tab because this simple iterator is only used to
    // find boundaries, not actual content. A linefeed breaks words, sentences, and paragraphs.
    if (shouldEmitNewlineForNode(m_node, m_behavior & TextIteratorEmitsOriginalText) || shouldEmitNewlineAfterNode(*m_node) || shouldEmitTabBeforeNode(*m_node)) {
        if (m_lastCharacter != '\n') {
            // Corresponds to the same check in TextIterator::exitNode.
            unsigned index = m_node->computeNodeIndex();
            // The start of this emitted range is wrong. Ensuring correctness would require
            // VisiblePositions and so would be slow. previousBoundary expects this.
            emitCharacter('\n', *m_node->parentNode(), index + 1, index + 1);
        }
    }
    return true;
}

void SimplifiedBackwardsTextIterator::exitNode()
{
    if (shouldEmitNewlineForNode(m_node, m_behavior & TextIteratorEmitsOriginalText) || shouldEmitNewlineBeforeNode(*m_node) || shouldEmitTabBeforeNode(*m_node)) {
        // The start of this emitted range is wrong. Ensuring correctness would require
        // VisiblePositions and so would be slow. previousBoundary expects this.
        emitCharacter('\n', *m_node, 0, 0);
    }
}

void SimplifiedBackwardsTextIterator::emitCharacter(UChar c, Node& node, int startOffset, int endOffset)
{
    m_positionNode = &node;
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

Ref<Range> SimplifiedBackwardsTextIterator::range() const
{
    ASSERT(!atEnd());

    return Range::create(m_positionNode->document(), m_positionNode, m_positionStartOffset, m_positionNode, m_positionEndOffset);
}

// --------

CharacterIterator::CharacterIterator(const Range& range, TextIteratorBehavior behavior)
    : m_underlyingIterator(&range, behavior)
{
    while (!atEnd() && !m_underlyingIterator.text().length())
        m_underlyingIterator.advance();
}

CharacterIterator::CharacterIterator(Position start, Position end, TextIteratorBehavior behavior)
    : m_underlyingIterator(start, end, behavior)
{
    while (!atEnd() && !m_underlyingIterator.text().length())
        m_underlyingIterator.advance();
}

Ref<Range> CharacterIterator::range() const
{
    Ref<Range> range = m_underlyingIterator.range();
    if (!m_underlyingIterator.atEnd()) {
        if (m_underlyingIterator.text().length() <= 1) {
            ASSERT(m_runOffset == 0);
        } else {
            Node& node = range->startContainer();
            ASSERT(&node == &range->endContainer());
            int offset = range->startOffset() + m_runOffset;
            range->setStart(node, offset);
            range->setEnd(node, offset + 1);
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

static Ref<Range> characterSubrange(Document& document, CharacterIterator& it, int offset, int length)
{
    it.advance(offset);
    if (it.atEnd())
        return Range::create(document);

    Ref<Range> start = it.range();

    if (length > 1)
        it.advance(length - 1);
    if (it.atEnd())
        return Range::create(document);

    Ref<Range> end = it.range();

    return Range::create(document, &start->startContainer(), start->startOffset(), &end->endContainer(), end->endOffset());
}

BackwardsCharacterIterator::BackwardsCharacterIterator(const Range& range)
    : m_underlyingIterator(range)
    , m_offset(0)
    , m_runOffset(0)
    , m_atBreak(true)
{
    while (!atEnd() && !m_underlyingIterator.text().length())
        m_underlyingIterator.advance();
}

Ref<Range> BackwardsCharacterIterator::range() const
{
    Ref<Range> r = m_underlyingIterator.range();
    if (!m_underlyingIterator.atEnd()) {
        if (m_underlyingIterator.text().length() <= 1)
            ASSERT(m_runOffset == 0);
        else {
            Node& node = r->startContainer();
            ASSERT(&node == &r->endContainer());
            int offset = r->endOffset() - m_runOffset;
            r->setStart(node, offset - 1);
            r->setEnd(node, offset);
        }
    }
    return r;
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

WordAwareIterator::WordAwareIterator(const Range& range)
    : m_underlyingIterator(&range)
    , m_didLookAhead(true) // so we consider the first chunk from the text iterator
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
        if (isSpaceOrNewline(m_underlyingIterator.text()[m_underlyingIterator.text().length() - 1]))
            return;

        // If this is the first chunk that failed, save it in previousText before look ahead
        if (m_buffer.isEmpty())
            m_previousText = m_underlyingIterator.copyableText();

        // Look ahead to next chunk. If it is whitespace or a break, we can use the previous stuff
        m_underlyingIterator.advance();
        if (m_underlyingIterator.atEnd() || !m_underlyingIterator.text().length() || isSpaceOrNewline(m_underlyingIterator.text()[0])) {
            m_didLookAhead = true;
            return;
        }

        if (m_buffer.isEmpty()) {
            // Start gobbling chunks until we get to a suitable stopping point
            append(m_buffer, m_previousText.text());
            m_previousText.reset();
        }
        append(m_buffer, m_underlyingIterator.text());
    }
}

StringView WordAwareIterator::text() const
{
    if (!m_buffer.isEmpty())
        return StringView(m_buffer.data(), m_buffer.size());
    if (m_previousText.text().length())
        return m_previousText.text();
    return m_underlyingIterator.text();
}

// --------

static inline UChar foldQuoteMark(UChar c)
{
    switch (c) {
        case hebrewPunctuationGershayim:
        case leftDoubleQuotationMark:
        case leftLowDoubleQuotationMark:
        case rightDoubleQuotationMark:
            return '"';
        case hebrewPunctuationGeresh:
        case leftSingleQuotationMark:
        case leftLowSingleQuotationMark:
        case rightSingleQuotationMark:
            return '\'';
        default:
            return c;
    }
}

// FIXME: We'd like to tailor the searcher to fold quote marks for us instead
// of doing it in a separate replacement pass here, but ICU doesn't offer a way
// to add tailoring on top of the locale-specific tailoring as of this writing.
String foldQuoteMarks(const String& stringToFold)
{
    String result(stringToFold);
    result.replace(hebrewPunctuationGeresh, '\'');
    result.replace(hebrewPunctuationGershayim, '"');
    result.replace(leftDoubleQuotationMark, '"');
    result.replace(leftLowDoubleQuotationMark, '"');
    result.replace(leftSingleQuotationMark, '\'');
    result.replace(leftLowSingleQuotationMark, '\'');
    result.replace(rightDoubleQuotationMark, '"');
    result.replace(rightSingleQuotationMark, '\'');

    return result;
}

#if !UCONFIG_NO_COLLATION

const size_t minimumSearchBufferSize = 8192;

#ifndef NDEBUG
static bool searcherInUse;
#endif

static UStringSearch* createSearcher()
{
    // Provide a non-empty pattern and non-empty text so usearch_open will not fail,
    // but it doesn't matter exactly what it is, since we don't perform any searches
    // without setting both the pattern and the text.
    UErrorCode status = U_ZERO_ERROR;
    String searchCollatorName = makeString(currentSearchLocaleID(), "@collation=search");
    UStringSearch* searcher = usearch_open(&newlineCharacter, 1, &newlineCharacter, 1, searchCollatorName.utf8().data(), 0, &status);
    ASSERT(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING || status == U_USING_DEFAULT_WARNING);
    return searcher;
}

static UStringSearch* searcher()
{
    static UStringSearch* searcher = createSearcher();
    return searcher;
}

static inline void lockSearcher()
{
#ifndef NDEBUG
    ASSERT(!searcherInUse);
    searcherInUse = true;
#endif
}

static inline void unlockSearcher()
{
#ifndef NDEBUG
    ASSERT(searcherInUse);
    searcherInUse = false;
#endif
}

// ICU's search ignores the distinction between small kana letters and ones
// that are not small, and also characters that differ only in the voicing
// marks when considering only primary collation strength differences.
// This is not helpful for end users, since these differences make words
// distinct, so for our purposes we need these to be considered.
// The Unicode folks do not think the collation algorithm should be
// changed. To work around this, we would like to tailor the ICU searcher,
// but we can't get that to work yet. So instead, we check for cases where
// these differences occur, and skip those matches.

// We refer to the above technique as the "kana workaround". The next few
// functions are helper functinos for the kana workaround.

static inline bool isKanaLetter(UChar character)
{
    // Hiragana letters.
    if (character >= 0x3041 && character <= 0x3096)
        return true;

    // Katakana letters.
    if (character >= 0x30A1 && character <= 0x30FA)
        return true;
    if (character >= 0x31F0 && character <= 0x31FF)
        return true;

    // Halfwidth katakana letters.
    if (character >= 0xFF66 && character <= 0xFF9D && character != 0xFF70)
        return true;

    return false;
}

static inline bool isSmallKanaLetter(UChar character)
{
    ASSERT(isKanaLetter(character));

    switch (character) {
    case 0x3041: // HIRAGANA LETTER SMALL A
    case 0x3043: // HIRAGANA LETTER SMALL I
    case 0x3045: // HIRAGANA LETTER SMALL U
    case 0x3047: // HIRAGANA LETTER SMALL E
    case 0x3049: // HIRAGANA LETTER SMALL O
    case 0x3063: // HIRAGANA LETTER SMALL TU
    case 0x3083: // HIRAGANA LETTER SMALL YA
    case 0x3085: // HIRAGANA LETTER SMALL YU
    case 0x3087: // HIRAGANA LETTER SMALL YO
    case 0x308E: // HIRAGANA LETTER SMALL WA
    case 0x3095: // HIRAGANA LETTER SMALL KA
    case 0x3096: // HIRAGANA LETTER SMALL KE
    case 0x30A1: // KATAKANA LETTER SMALL A
    case 0x30A3: // KATAKANA LETTER SMALL I
    case 0x30A5: // KATAKANA LETTER SMALL U
    case 0x30A7: // KATAKANA LETTER SMALL E
    case 0x30A9: // KATAKANA LETTER SMALL O
    case 0x30C3: // KATAKANA LETTER SMALL TU
    case 0x30E3: // KATAKANA LETTER SMALL YA
    case 0x30E5: // KATAKANA LETTER SMALL YU
    case 0x30E7: // KATAKANA LETTER SMALL YO
    case 0x30EE: // KATAKANA LETTER SMALL WA
    case 0x30F5: // KATAKANA LETTER SMALL KA
    case 0x30F6: // KATAKANA LETTER SMALL KE
    case 0x31F0: // KATAKANA LETTER SMALL KU
    case 0x31F1: // KATAKANA LETTER SMALL SI
    case 0x31F2: // KATAKANA LETTER SMALL SU
    case 0x31F3: // KATAKANA LETTER SMALL TO
    case 0x31F4: // KATAKANA LETTER SMALL NU
    case 0x31F5: // KATAKANA LETTER SMALL HA
    case 0x31F6: // KATAKANA LETTER SMALL HI
    case 0x31F7: // KATAKANA LETTER SMALL HU
    case 0x31F8: // KATAKANA LETTER SMALL HE
    case 0x31F9: // KATAKANA LETTER SMALL HO
    case 0x31FA: // KATAKANA LETTER SMALL MU
    case 0x31FB: // KATAKANA LETTER SMALL RA
    case 0x31FC: // KATAKANA LETTER SMALL RI
    case 0x31FD: // KATAKANA LETTER SMALL RU
    case 0x31FE: // KATAKANA LETTER SMALL RE
    case 0x31FF: // KATAKANA LETTER SMALL RO
    case 0xFF67: // HALFWIDTH KATAKANA LETTER SMALL A
    case 0xFF68: // HALFWIDTH KATAKANA LETTER SMALL I
    case 0xFF69: // HALFWIDTH KATAKANA LETTER SMALL U
    case 0xFF6A: // HALFWIDTH KATAKANA LETTER SMALL E
    case 0xFF6B: // HALFWIDTH KATAKANA LETTER SMALL O
    case 0xFF6C: // HALFWIDTH KATAKANA LETTER SMALL YA
    case 0xFF6D: // HALFWIDTH KATAKANA LETTER SMALL YU
    case 0xFF6E: // HALFWIDTH KATAKANA LETTER SMALL YO
    case 0xFF6F: // HALFWIDTH KATAKANA LETTER SMALL TU
        return true;
    }
    return false;
}

enum VoicedSoundMarkType { NoVoicedSoundMark, VoicedSoundMark, SemiVoicedSoundMark };

static inline VoicedSoundMarkType composedVoicedSoundMark(UChar character)
{
    ASSERT(isKanaLetter(character));

    switch (character) {
    case 0x304C: // HIRAGANA LETTER GA
    case 0x304E: // HIRAGANA LETTER GI
    case 0x3050: // HIRAGANA LETTER GU
    case 0x3052: // HIRAGANA LETTER GE
    case 0x3054: // HIRAGANA LETTER GO
    case 0x3056: // HIRAGANA LETTER ZA
    case 0x3058: // HIRAGANA LETTER ZI
    case 0x305A: // HIRAGANA LETTER ZU
    case 0x305C: // HIRAGANA LETTER ZE
    case 0x305E: // HIRAGANA LETTER ZO
    case 0x3060: // HIRAGANA LETTER DA
    case 0x3062: // HIRAGANA LETTER DI
    case 0x3065: // HIRAGANA LETTER DU
    case 0x3067: // HIRAGANA LETTER DE
    case 0x3069: // HIRAGANA LETTER DO
    case 0x3070: // HIRAGANA LETTER BA
    case 0x3073: // HIRAGANA LETTER BI
    case 0x3076: // HIRAGANA LETTER BU
    case 0x3079: // HIRAGANA LETTER BE
    case 0x307C: // HIRAGANA LETTER BO
    case 0x3094: // HIRAGANA LETTER VU
    case 0x30AC: // KATAKANA LETTER GA
    case 0x30AE: // KATAKANA LETTER GI
    case 0x30B0: // KATAKANA LETTER GU
    case 0x30B2: // KATAKANA LETTER GE
    case 0x30B4: // KATAKANA LETTER GO
    case 0x30B6: // KATAKANA LETTER ZA
    case 0x30B8: // KATAKANA LETTER ZI
    case 0x30BA: // KATAKANA LETTER ZU
    case 0x30BC: // KATAKANA LETTER ZE
    case 0x30BE: // KATAKANA LETTER ZO
    case 0x30C0: // KATAKANA LETTER DA
    case 0x30C2: // KATAKANA LETTER DI
    case 0x30C5: // KATAKANA LETTER DU
    case 0x30C7: // KATAKANA LETTER DE
    case 0x30C9: // KATAKANA LETTER DO
    case 0x30D0: // KATAKANA LETTER BA
    case 0x30D3: // KATAKANA LETTER BI
    case 0x30D6: // KATAKANA LETTER BU
    case 0x30D9: // KATAKANA LETTER BE
    case 0x30DC: // KATAKANA LETTER BO
    case 0x30F4: // KATAKANA LETTER VU
    case 0x30F7: // KATAKANA LETTER VA
    case 0x30F8: // KATAKANA LETTER VI
    case 0x30F9: // KATAKANA LETTER VE
    case 0x30FA: // KATAKANA LETTER VO
        return VoicedSoundMark;
    case 0x3071: // HIRAGANA LETTER PA
    case 0x3074: // HIRAGANA LETTER PI
    case 0x3077: // HIRAGANA LETTER PU
    case 0x307A: // HIRAGANA LETTER PE
    case 0x307D: // HIRAGANA LETTER PO
    case 0x30D1: // KATAKANA LETTER PA
    case 0x30D4: // KATAKANA LETTER PI
    case 0x30D7: // KATAKANA LETTER PU
    case 0x30DA: // KATAKANA LETTER PE
    case 0x30DD: // KATAKANA LETTER PO
        return SemiVoicedSoundMark;
    }
    return NoVoicedSoundMark;
}

static inline bool isCombiningVoicedSoundMark(UChar character)
{
    switch (character) {
    case 0x3099: // COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
    case 0x309A: // COMBINING KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK
        return true;
    }
    return false;
}

static inline bool containsKanaLetters(const String& pattern)
{
    if (pattern.is8Bit())
        return false;
    const UChar* characters = pattern.characters16();
    unsigned length = pattern.length();
    for (unsigned i = 0; i < length; ++i) {
        if (isKanaLetter(characters[i]))
            return true;
    }
    return false;
}

static void normalizeCharacters(const UChar* characters, unsigned length, Vector<UChar>& buffer)
{
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* normalizer = unorm2_getNFCInstance(&status);
    ASSERT(U_SUCCESS(status));

    buffer.resize(length);

    auto normalizedLength = unorm2_normalize(normalizer, characters, length, buffer.data(), length, &status);
    ASSERT(U_SUCCESS(status) || status == U_BUFFER_OVERFLOW_ERROR);

    buffer.resize(normalizedLength);

    if (U_SUCCESS(status))
        return;

    status = U_ZERO_ERROR;
    unorm2_normalize(normalizer, characters, length, buffer.data(), length, &status);
    ASSERT(U_SUCCESS(status));
}

static bool isNonLatin1Separator(UChar32 character)
{
    ASSERT_ARG(character, !isLatin1(character));

    return U_GET_GC_MASK(character) & (U_GC_S_MASK | U_GC_P_MASK | U_GC_Z_MASK | U_GC_CF_MASK);
}

static inline bool isSeparator(UChar32 character)
{
    static const bool latin1SeparatorTable[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // space ! " # $ % & ' ( ) * + , - . /
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, //                         : ; < = > ?
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //   @
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //                         [ \ ] ^ _
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //   `
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, //                           { | } ~
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0
    };

    if (isLatin1(character))
        return latin1SeparatorTable[character];

    return isNonLatin1Separator(character);
}

inline SearchBuffer::SearchBuffer(const String& target, FindOptions options)
    : m_target(foldQuoteMarks(target))
    , m_targetCharacters(StringView(m_target).upconvertedCharacters())
    , m_options(options)
    , m_prefixLength(0)
    , m_atBreak(true)
    , m_needsMoreContext(options.contains(AtWordStarts))
    , m_targetRequiresKanaWorkaround(containsKanaLetters(m_target))
{
    ASSERT(!m_target.isEmpty());

    size_t targetLength = m_target.length();
    m_buffer.reserveInitialCapacity(std::max(targetLength * 8, minimumSearchBufferSize));
    m_overlap = m_buffer.capacity() / 4;

    if (m_options.contains(AtWordStarts) && targetLength) {
        UChar32 targetFirstCharacter;
        U16_GET(m_target, 0, 0u, targetLength, targetFirstCharacter);
        // Characters in the separator category never really occur at the beginning of a word,
        // so if the target begins with such a character, we just ignore the AtWordStart option.
        if (isSeparator(targetFirstCharacter)) {
            m_options.remove(AtWordStarts);
            m_needsMoreContext = false;
        }
    }

    // Grab the single global searcher.
    // If we ever have a reason to do more than once search buffer at once, we'll have
    // to move to multiple searchers.
    lockSearcher();

    UStringSearch* searcher = WebCore::searcher();
    UCollator* collator = usearch_getCollator(searcher);

    UCollationStrength strength;
    USearchAttributeValue comparator;
    if (m_options.contains(CaseInsensitive)) {
        // Without loss of generality, have 'e' match {'e', 'E', 'é', 'É'} and 'é' match {'é', 'É'}.
        strength = UCOL_SECONDARY;
        comparator = USEARCH_PATTERN_BASE_WEIGHT_IS_WILDCARD;
    } else {
        // Without loss of generality, have 'e' match {'e'} and 'é' match {'é'}.
        strength = UCOL_TERTIARY;
        comparator = USEARCH_STANDARD_ELEMENT_COMPARISON;
    }
    if (ucol_getStrength(collator) != strength) {
        ucol_setStrength(collator, strength);
        usearch_reset(searcher);
    }

    UErrorCode status = U_ZERO_ERROR;
    usearch_setAttribute(searcher, USEARCH_ELEMENT_COMPARISON, comparator, &status);
    ASSERT(status == U_ZERO_ERROR);

    usearch_setPattern(searcher, m_targetCharacters, targetLength, &status);
    ASSERT(status == U_ZERO_ERROR);

    // The kana workaround requires a normalized copy of the target string.
    if (m_targetRequiresKanaWorkaround)
        normalizeCharacters(m_targetCharacters, targetLength, m_normalizedTarget);
}

inline SearchBuffer::~SearchBuffer()
{
    // Leave the static object pointing to a valid string.
    UErrorCode status = U_ZERO_ERROR;
    usearch_setPattern(WebCore::searcher(), &newlineCharacter, 1, &status);
    ASSERT(status == U_ZERO_ERROR);
    usearch_setText(WebCore::searcher(), &newlineCharacter, 1, &status);
    ASSERT(status == U_ZERO_ERROR);

    unlockSearcher();
}

inline size_t SearchBuffer::append(StringView text)
{
    ASSERT(text.length());

    if (m_atBreak) {
        m_buffer.shrink(0);
        m_prefixLength = 0;
        m_atBreak = false;
    } else if (m_buffer.size() == m_buffer.capacity()) {
        memcpy(m_buffer.data(), m_buffer.data() + m_buffer.size() - m_overlap, m_overlap * sizeof(UChar));
        m_prefixLength -= std::min(m_prefixLength, m_buffer.size() - m_overlap);
        m_buffer.shrink(m_overlap);
    }

    size_t oldLength = m_buffer.size();
    size_t usableLength = std::min<size_t>(m_buffer.capacity() - oldLength, text.length());
    ASSERT(usableLength);
    m_buffer.grow(oldLength + usableLength);
    for (unsigned i = 0; i < usableLength; ++i)
        m_buffer[oldLength + i] = foldQuoteMark(text[i]);
    return usableLength;
}

inline bool SearchBuffer::needsMoreContext() const
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
        wordBoundaryContextStart = startOfLastWordBoundaryContext(text.substring(0, wordBoundaryContextStart));
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

inline void SearchBuffer::reachedBreak()
{
    m_atBreak = true;
}

inline bool SearchBuffer::isBadMatch(const UChar* match, size_t matchLength) const
{
    // This function implements the kana workaround. If usearch treats
    // it as a match, but we do not want to, then it's a "bad match".
    if (!m_targetRequiresKanaWorkaround)
        return false;

    // Normalize into a match buffer. We reuse a single buffer rather than
    // creating a new one each time.
    normalizeCharacters(match, matchLength, m_normalizedMatch);

    const UChar* a = m_normalizedTarget.begin();
    const UChar* aEnd = m_normalizedTarget.end();

    const UChar* b = m_normalizedMatch.begin();
    const UChar* bEnd = m_normalizedMatch.end();

    while (true) {
        // Skip runs of non-kana-letter characters. This is necessary so we can
        // correctly handle strings where the target and match have different-length
        // runs of characters that match, while still double checking the correctness
        // of matches of kana letters with other kana letters.
        while (a != aEnd && !isKanaLetter(*a))
            ++a;
        while (b != bEnd && !isKanaLetter(*b))
            ++b;

        // If we reached the end of either the target or the match, we should have
        // reached the end of both; both should have the same number of kana letters.
        if (a == aEnd || b == bEnd) {
            ASSERT(a == aEnd);
            ASSERT(b == bEnd);
            return false;
        }

        // Check for differences in the kana letter character itself.
        if (isSmallKanaLetter(*a) != isSmallKanaLetter(*b))
            return true;
        if (composedVoicedSoundMark(*a) != composedVoicedSoundMark(*b))
            return true;
        ++a;
        ++b;

        // Check for differences in combining voiced sound marks found after the letter.
        while (1) {
            if (!(a != aEnd && isCombiningVoicedSoundMark(*a))) {
                if (b != bEnd && isCombiningVoicedSoundMark(*b))
                    return true;
                break;
            }
            if (!(b != bEnd && isCombiningVoicedSoundMark(*b)))
                return true;
            if (*a != *b)
                return true;
            ++a;
            ++b;
        }
    }
}
    
inline bool SearchBuffer::isWordEndMatch(size_t start, size_t length) const
{
    ASSERT(length);
    ASSERT(m_options.contains(AtWordEnds));

    int endWord;
    // Start searching at the end of matched search, so that multiple word matches succeed.
    findEndWordBoundary(StringView(m_buffer.data(), m_buffer.size()), start + length - 1, &endWord);
    return static_cast<size_t>(endWord) == (start + length);
}

inline bool SearchBuffer::isWordStartMatch(size_t start, size_t length) const
{
    ASSERT(m_options.contains(AtWordStarts));

    if (!start)
        return true;

    int size = m_buffer.size();
    int offset = start;
    UChar32 firstCharacter;
    U16_GET(m_buffer.data(), 0, offset, size, firstCharacter);

    if (m_options.contains(TreatMedialCapitalAsWordStart)) {
        UChar32 previousCharacter;
        U16_PREV(m_buffer.data(), 0, offset, previousCharacter);

        if (isSeparator(firstCharacter)) {
            // The start of a separator run is a word start (".org" in "webkit.org").
            if (!isSeparator(previousCharacter))
                return true;
        } else if (isASCIIUpper(firstCharacter)) {
            // The start of an uppercase run is a word start ("Kit" in "WebKit").
            if (!isASCIIUpper(previousCharacter))
                return true;
            // The last character of an uppercase run followed by a non-separator, non-digit
            // is a word start ("Request" in "XMLHTTPRequest").
            offset = start;
            U16_FWD_1(m_buffer.data(), offset, size);
            UChar32 nextCharacter = 0;
            if (offset < size)
                U16_GET(m_buffer.data(), 0, offset, size, nextCharacter);
            if (!isASCIIUpper(nextCharacter) && !isASCIIDigit(nextCharacter) && !isSeparator(nextCharacter))
                return true;
        } else if (isASCIIDigit(firstCharacter)) {
            // The start of a digit run is a word start ("2" in "WebKit2").
            if (!isASCIIDigit(previousCharacter))
                return true;
        } else if (isSeparator(previousCharacter) || isASCIIDigit(previousCharacter)) {
            // The start of a non-separator, non-uppercase, non-digit run is a word start,
            // except after an uppercase. ("org" in "webkit.org", but not "ore" in "WebCore").
            return true;
        }
    }

    // Chinese and Japanese lack word boundary marks, and there is no clear agreement on what constitutes
    // a word, so treat the position before any CJK character as a word start.
    if (FontCascade::isCJKIdeographOrSymbol(firstCharacter))
        return true;

    size_t wordBreakSearchStart = start + length;
    while (wordBreakSearchStart > start)
        wordBreakSearchStart = findNextWordFromIndex(StringView(m_buffer.data(), m_buffer.size()), wordBreakSearchStart, false /* backwards */);
    return wordBreakSearchStart == start;
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

    UStringSearch* searcher = WebCore::searcher();

    UErrorCode status = U_ZERO_ERROR;
    usearch_setText(searcher, m_buffer.data(), size, &status);
    ASSERT(status == U_ZERO_ERROR);

    usearch_setOffset(searcher, m_prefixLength, &status);
    ASSERT(status == U_ZERO_ERROR);

    int matchStart = usearch_next(searcher, &status);
    ASSERT(status == U_ZERO_ERROR);

nextMatch:
    if (!(matchStart >= 0 && static_cast<size_t>(matchStart) < size)) {
        ASSERT(matchStart == USEARCH_DONE);
        return 0;
    }

    // Matches that start in the overlap area are only tentative.
    // The same match may appear later, matching more characters,
    // possibly including a combining character that's not yet in the buffer.
    if (!m_atBreak && static_cast<size_t>(matchStart) >= size - m_overlap) {
        size_t overlap = m_overlap;
        if (m_options.contains(AtWordStarts)) {
            // Ensure that there is sufficient context before matchStart the next time around for
            // determining if it is at a word boundary.
            unsigned wordBoundaryContextStart = matchStart;
            U16_BACK_1(m_buffer.data(), 0, wordBoundaryContextStart);
            wordBoundaryContextStart = startOfLastWordBoundaryContext(StringView(m_buffer.data(), wordBoundaryContextStart));
            overlap = std::min(size - 1, std::max(overlap, size - wordBoundaryContextStart));
        }
        memcpy(m_buffer.data(), m_buffer.data() + size - overlap, overlap * sizeof(UChar));
        m_prefixLength -= std::min(m_prefixLength, size - overlap);
        m_buffer.shrink(overlap);
        return 0;
    }

    size_t matchedLength = usearch_getMatchedLength(searcher);
    ASSERT_WITH_SECURITY_IMPLICATION(matchStart + matchedLength <= size);

    // If this match is "bad", move on to the next match.
    if (isBadMatch(m_buffer.data() + matchStart, matchedLength)
        || (m_options.contains(AtWordStarts) && !isWordStartMatch(matchStart, matchedLength))
        || (m_options.contains(AtWordEnds) && !isWordEndMatch(matchStart, matchedLength))) {
        matchStart = usearch_next(searcher, &status);
        ASSERT(status == U_ZERO_ERROR);
        goto nextMatch;
    }

    size_t newSize = size - (matchStart + 1);
    memmove(m_buffer.data(), m_buffer.data() + matchStart + 1, newSize * sizeof(UChar));
    m_prefixLength -= std::min<size_t>(m_prefixLength, matchStart + 1);
    m_buffer.shrink(newSize);

    start = size - matchStart;
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

inline void SearchBuffer::append(UChar c, bool isStart)
{
    m_buffer[m_cursor] = c == noBreakSpace ? ' ' : foldQuoteMark(c);
    m_isCharacterStartBuffer[m_cursor] = isStart;
    if (++m_cursor == m_target.length()) {
        m_cursor = 0;
        m_isBufferFull = true;
    }
}

inline size_t SearchBuffer::append(const UChar* characters, size_t length)
{
    ASSERT(length);
    if (!(m_options & CaseInsensitive)) {
        append(characters[0], true);
        return 1;
    }
    const int maxFoldedCharacters = 16; // sensible maximum is 3, this should be more than enough
    UChar foldedCharacters[maxFoldedCharacters];
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

void SearchBuffer::prependContext(const UChar*, size_t)
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
    if (memcmp(&m_buffer[m_cursor], m_target.characters(), tailSpace * sizeof(UChar)) != 0)
        return 0;
    if (memcmp(&m_buffer[0], m_target.characters() + tailSpace, m_cursor * sizeof(UChar)) != 0)
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

int TextIterator::rangeLength(const Range* range, bool forSelectionPreservation)
{
    unsigned length = 0;
    for (TextIterator it(range, forSelectionPreservation ? TextIteratorEmitsCharactersBetweenAllVisiblePositions : TextIteratorDefaultBehavior); !it.atEnd(); it.advance())
        length += it.text().length();
    return length;
}

Ref<Range> TextIterator::subrange(Range& entireRange, int characterOffset, int characterCount)
{
    CharacterIterator entireRangeIterator(entireRange);
    return characterSubrange(entireRange.ownerDocument(), entireRangeIterator, characterOffset, characterCount);
}

static inline bool isInsideReplacedElement(TextIterator& iterator)
{
    ASSERT(!iterator.atEnd());
    ASSERT(iterator.text().length() == 1);
    Node* node = iterator.node();
    return node && isRendererReplacedElement(node->renderer());
}

RefPtr<Range> TextIterator::rangeFromLocationAndLength(ContainerNode* scope, int rangeLocation, int rangeLength, bool forSelectionPreservation)
{
    Ref<Range> resultRange = scope->document().createRange();

    int docTextPosition = 0;
    int rangeEnd = rangeLocation + rangeLength;
    bool startRangeFound = false;

    Ref<Range> textRunRange = rangeOfContents(*scope);

    TextIterator it(textRunRange.ptr(), forSelectionPreservation ? TextIteratorEmitsCharactersBetweenAllVisiblePositions : TextIteratorDefaultBehavior);
    
    // FIXME: the atEnd() check shouldn't be necessary, workaround for <http://bugs.webkit.org/show_bug.cgi?id=6289>.
    if (!rangeLocation && !rangeLength && it.atEnd()) {
        resultRange->setStart(textRunRange->startContainer(), 0);
        resultRange->setEnd(textRunRange->startContainer(), 0);
        return resultRange;
    }

    for (; !it.atEnd(); it.advance()) {
        int length = it.text().length();
        textRunRange = it.range();

        bool foundStart = rangeLocation >= docTextPosition && rangeLocation <= docTextPosition + length;
        bool foundEnd = rangeEnd >= docTextPosition && rangeEnd <= docTextPosition + length;

        if (foundEnd) {
            // FIXME: This is a workaround for the fact that the end of a run is often at the wrong
            // position for emitted '\n's or if the renderer of the current node is a replaced element.
            if (length == 1 && (it.text()[0] == '\n' || isInsideReplacedElement(it))) {
                it.advance();
                if (!it.atEnd()) {
                    Ref<Range> range = it.range();
                    textRunRange->setEnd(range->startContainer(), range->startOffset());
                } else {
                    Position runStart = textRunRange->startPosition();
                    Position runEnd = VisiblePosition(runStart).next().deepEquivalent();
                    if (runEnd.isNotNull())
                        textRunRange->setEnd(*runEnd.containerNode(), runEnd.computeOffsetInContainerNode());
                }
            }
        }

        if (foundStart) {
            startRangeFound = true;
            if (textRunRange->startContainer().isTextNode()) {
                int offset = rangeLocation - docTextPosition;
                resultRange->setStart(textRunRange->startContainer(), offset + textRunRange->startOffset());
            } else {
                if (rangeLocation == docTextPosition)
                    resultRange->setStart(textRunRange->startContainer(), textRunRange->startOffset());
                else
                    resultRange->setStart(textRunRange->endContainer(), textRunRange->endOffset());
            }
        }

        if (foundEnd) {
            if (textRunRange->startContainer().isTextNode()) {
                int offset = rangeEnd - docTextPosition;
                resultRange->setEnd(textRunRange->startContainer(), offset + textRunRange->startOffset());
            } else {
                if (rangeEnd == docTextPosition)
                    resultRange->setEnd(textRunRange->startContainer(), textRunRange->startOffset());
                else
                    resultRange->setEnd(textRunRange->endContainer(), textRunRange->endOffset());
            }
            docTextPosition += length;
            break;
        }

        docTextPosition += length;
    }
    
    if (!startRangeFound)
        return nullptr;
    
    if (rangeLength && rangeEnd > docTextPosition) // rangeEnd is out of bounds
        resultRange->setEnd(textRunRange->endContainer(), textRunRange->endOffset());
    
    return resultRange;
}

bool TextIterator::getLocationAndLengthFromRange(Node* scope, const Range* range, size_t& location, size_t& length)
{
    location = notFound;
    length = 0;

    // The critical assumption is that this only gets called with ranges that
    // concentrate on a given area containing the selection root. This is done
    // because of text fields and textareas. The DOM for those is not
    // directly in the document DOM, so ensure that the range does not cross a
    // boundary of one of those.
    if (&range->startContainer() != scope && !range->startContainer().isDescendantOf(scope))
        return false;
    if (&range->endContainer() != scope && !range->endContainer().isDescendantOf(scope))
        return false;

    Ref<Range> testRange = Range::create(scope->document(), scope, 0, &range->startContainer(), range->startOffset());
    ASSERT(&testRange->startContainer() == scope);
    location = TextIterator::rangeLength(testRange.ptr());

    testRange->setEnd(range->endContainer(), range->endOffset());
    ASSERT(&testRange->startContainer() == scope);
    length = TextIterator::rangeLength(testRange.ptr()) - location;
    return true;
}

// --------

bool hasAnyPlainText(const Range& range, TextIteratorBehavior behavior)
{
    for (TextIterator iterator { &range, behavior }; !iterator.atEnd(); iterator.advance()) {
        if (!iterator.text().isEmpty())
            return true;
    }
    return false;
}

String plainText(Position start, Position end, TextIteratorBehavior defaultBehavior, bool isDisplayString)
{
    // The initial buffer size can be critical for performance: https://bugs.webkit.org/show_bug.cgi?id=81192
    static const unsigned initialCapacity = 1 << 15;

    if (!start.document())
        return { };
    auto document = makeRef(*start.document());

    unsigned bufferLength = 0;
    StringBuilder builder;
    builder.reserveCapacity(initialCapacity);
    TextIteratorBehavior behavior = defaultBehavior;
    if (!isDisplayString)
        behavior = static_cast<TextIteratorBehavior>(behavior | TextIteratorEmitsTextsWithoutTranscoding);

    for (TextIterator it(start, end, behavior); !it.atEnd(); it.advance()) {
        it.appendTextToStringBuilder(builder);
        bufferLength += it.text().length();
    }

    if (!bufferLength)
        return emptyString();

    String result = builder.toString();

    if (isDisplayString)
        document->displayStringModifiedByEncoding(result);

    return result;
}

String plainTextReplacingNoBreakSpace(Position start, Position end, TextIteratorBehavior defaultBehavior, bool isDisplayString)
{
    return plainText(start, end, defaultBehavior, isDisplayString).replace(noBreakSpace, ' ');
}

String plainText(const Range* range, TextIteratorBehavior defaultBehavior, bool isDisplayString)
{
    if (!range)
        return emptyString();
    return plainText(range->startPosition(), range->endPosition(), defaultBehavior, isDisplayString);
}

String plainTextUsingBackwardsTextIteratorForTesting(const Range& range)
{
    String result;
    for (SimplifiedBackwardsTextIterator backwardsIterator(range); !backwardsIterator.atEnd(); backwardsIterator.advance())
        result.insert(backwardsIterator.text().toString(), 0);
    return result;
}

String plainTextReplacingNoBreakSpace(const Range* range, TextIteratorBehavior defaultBehavior, bool isDisplayString)
{
    return plainText(range, defaultBehavior, isDisplayString).replace(noBreakSpace, ' ');
}

static Ref<Range> collapsedToBoundary(const Range& range, bool forward)
{
    Ref<Range> result = range.cloneRange();
    result->collapse(!forward);
    return result;
}

static TextIteratorBehavior findIteratorOptions(FindOptions options)
{
    TextIteratorBehavior iteratorOptions = TextIteratorEntersTextControls | TextIteratorClipsToFrameAncestors;
    if (!options.contains(DoNotTraverseFlatTree))
        iteratorOptions |= TextIteratorTraversesFlatTree;
    return iteratorOptions;
}

static void findPlainTextMatches(const Range& range, const String& target, FindOptions options, const WTF::Function<bool(size_t, size_t)>& match)
{
    SearchBuffer buffer(target, options);
    if (buffer.needsMoreContext()) {
        Ref<Range> beforeStartRange = range.ownerDocument().createRange();
        beforeStartRange->setEnd(range.startContainer(), range.startOffset());
        for (SimplifiedBackwardsTextIterator backwardsIterator(beforeStartRange.get()); !backwardsIterator.atEnd(); backwardsIterator.advance()) {
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
            if (match(lastCharacterInBufferOffset - matchStartOffset, newMatchLength))
                return;
        }
    }
}

static Ref<Range> rangeForMatch(const Range& range, FindOptions options, size_t matchStart, size_t matchLength, bool searchForward)
{
    if (!matchLength)
        return collapsedToBoundary(range, searchForward);
    CharacterIterator rangeComputeIterator(range, findIteratorOptions(options));
    return characterSubrange(range.ownerDocument(), rangeComputeIterator, matchStart, matchLength);
}

Ref<Range> findClosestPlainText(const Range& range, const String& target, FindOptions options, unsigned targetOffset)
{
    size_t matchStart = 0;
    size_t matchLength = 0;
    size_t distance = std::numeric_limits<size_t>::max();
    auto match = [targetOffset, &distance, &matchStart, &matchLength] (size_t start, size_t length) {
        size_t newDistance = std::min(abs(static_cast<signed>(start - targetOffset)), abs(static_cast<signed>(start + length - targetOffset)));
        if (newDistance < distance) {
            matchStart = start;
            matchLength = length;
            distance = newDistance;
        }
        return false;
    };

    findPlainTextMatches(range, target, options, WTFMove(match));
    return rangeForMatch(range, options, matchStart, matchLength, !options.contains(Backwards));
}

Ref<Range> findPlainText(const Range& range, const String& target, FindOptions options)
{
    bool searchForward = !options.contains(Backwards);
    size_t matchStart = 0;
    size_t matchLength = 0;
    auto match = [searchForward, &matchStart, &matchLength] (size_t start, size_t length) {
        matchStart = start;
        matchLength = length;
        // Look for the last match when searching backwards instead.
        return searchForward;
    };

    findPlainTextMatches(range, target, options, WTFMove(match));
    return rangeForMatch(range, options, matchStart, matchLength, searchForward);
}

bool findPlainText(const String& document, const String& target, FindOptions options)
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
