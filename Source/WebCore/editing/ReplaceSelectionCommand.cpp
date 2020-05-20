/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010, 2011 Google Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ReplaceSelectionCommand.h"

#include "AXObjectCache.h"
#include "ApplyStyleCommand.h"
#include "BeforeTextInsertedEvent.h"
#include "BreakBlockquoteCommand.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSStyleDeclaration.h"
#include "DOMWrapperWorld.h"
#include "DataTransfer.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Editing.h"
#include "EditingBehavior.h"
#include "ElementIterator.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLBRElement.h"
#include "HTMLBaseElement.h"
#include "HTMLBodyElement.h"
#include "HTMLInputElement.h"
#include "HTMLLIElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "HTMLTitleElement.h"
#include "NodeList.h"
#include "NodeRenderStyle.h"
#include "Position.h"
#include "RenderInline.h"
#include "RenderText.h"
#include "ScriptElement.h"
#include "SimplifyMarkupCommand.h"
#include "SmartReplace.h"
#include "StyleProperties.h"
#include "Text.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include "markup.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

enum EFragmentType { EmptyFragment, SingleTextNodeFragment, TreeFragment };

// --- ReplacementFragment helper class

class ReplacementFragment {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ReplacementFragment);
public:
    ReplacementFragment(DocumentFragment*, const VisibleSelection&);

    DocumentFragment* fragment() { return m_fragment.get(); }

    Node* firstChild() const;
    Node* lastChild() const;

    bool isEmpty() const;
    
    bool hasInterchangeNewlineAtStart() const { return m_hasInterchangeNewlineAtStart; }
    bool hasInterchangeNewlineAtEnd() const { return m_hasInterchangeNewlineAtEnd; }
    
    void removeNode(Node&);
    void removeNodePreservingChildren(Node&);

private:
    void removeContentsWithSideEffects();
    Ref<HTMLElement> insertFragmentForTestRendering(Node* rootEditableNode);
    void removeUnrenderedNodes(Node*);
    void restoreAndRemoveTestRenderingNodesToFragment(StyledElement*);
    void removeInterchangeNodes(Node*);
    
    void insertNodeBefore(Node&, Node& refNode);

    RefPtr<DocumentFragment> m_fragment;
    bool m_hasInterchangeNewlineAtStart;
    bool m_hasInterchangeNewlineAtEnd;
};

static bool isInterchangeNewlineNode(const Node* node)
{
    static NeverDestroyed<String> interchangeNewlineClassString(AppleInterchangeNewline);
    return is<HTMLBRElement>(node) && downcast<HTMLBRElement>(*node).attributeWithoutSynchronization(classAttr) == interchangeNewlineClassString;
}

static bool isInterchangeConvertedSpaceSpan(const Node* node)
{
    static NeverDestroyed<String> convertedSpaceSpanClassString(AppleConvertedSpace);
    return is<HTMLElement>(node) && downcast<HTMLElement>(*node).attributeWithoutSynchronization(classAttr) == convertedSpaceSpanClassString;
}

static Position positionAvoidingPrecedingNodes(Position position)
{
    ASSERT(position.isNotNull());

    // If we're already on a break, it's probably a placeholder and we shouldn't change our position.
    if (editingIgnoresContent(*position.deprecatedNode()))
        return position;

    // We also stop when changing block flow elements because even though the visual position is the
    // same.  E.g.,
    //   <div>foo^</div>^
    // The two positions above are the same visual position, but we want to stay in the same block.
    auto* enclosingBlockNode = enclosingBlock(position.containerNode());
    for (Position nextPosition = position; nextPosition.containerNode() != enclosingBlockNode; position = nextPosition) {
        if (lineBreakExistsAtPosition(position))
            break;

        if (position.containerNode()->nonShadowBoundaryParentNode())
            nextPosition = positionInParentAfterNode(position.containerNode());

        if (nextPosition == position)
            break;
        if (enclosingBlock(nextPosition.containerNode()) != enclosingBlockNode)
            break;
        if (VisiblePosition(position) != VisiblePosition(nextPosition))
            break;
    }
    return position;
}

ReplacementFragment::ReplacementFragment(DocumentFragment* fragment, const VisibleSelection& selection)
    : m_fragment(fragment)
    , m_hasInterchangeNewlineAtStart(false)
    , m_hasInterchangeNewlineAtEnd(false)
{
    if (!m_fragment)
        return;
    if (!m_fragment->firstChild())
        return;

    removeContentsWithSideEffects();

    RefPtr<Element> editableRoot = selection.rootEditableElement();
    ASSERT(editableRoot);
    if (!editableRoot)
        return;

    auto* shadowHost = editableRoot->shadowHost();
    if (!editableRoot->attributeEventListener(eventNames().webkitBeforeTextInsertedEvent, mainThreadNormalWorld())
        && !(shadowHost && shadowHost->renderer() && shadowHost->renderer()->isTextControl())
        && editableRoot->hasRichlyEditableStyle()) {
        removeInterchangeNodes(m_fragment.get());
        return;
    }

    auto page = createPageForSanitizingWebContent();
    Document* stagingDocument = page->mainFrame().document();
    ASSERT(stagingDocument->body());

    ComputedStyleExtractor computedStyleOfEditableRoot(editableRoot.get());
    stagingDocument->body()->setAttributeWithoutSynchronization(styleAttr, computedStyleOfEditableRoot.copyProperties()->asText());

    RefPtr<StyledElement> holder = insertFragmentForTestRendering(stagingDocument->body());
    if (!holder) {
        removeInterchangeNodes(m_fragment.get());
        return;
    }
    
    auto range = VisibleSelection::selectionFromContentsOfNode(holder.get()).toNormalizedRange();
    String text = range ? plainText(*range, static_cast<TextIteratorBehavior>(TextIteratorEmitsOriginalText | TextIteratorIgnoresStyleVisibility)) : emptyString();

    removeInterchangeNodes(holder.get());
    removeUnrenderedNodes(holder.get());
    restoreAndRemoveTestRenderingNodesToFragment(holder.get());

    // Give the root a chance to change the text.
    auto event = BeforeTextInsertedEvent::create(text);
    editableRoot->dispatchEvent(event);
    if (text != event->text() || !editableRoot->hasRichlyEditableStyle()) {
        restoreAndRemoveTestRenderingNodesToFragment(holder.get());

        auto range = selection.toNormalizedRange();
        if (!range)
            return;

        m_fragment = createFragmentFromText(createLiveRange(*range), event->text());
        if (!m_fragment->firstChild())
            return;

        holder = insertFragmentForTestRendering(stagingDocument->body());
        removeInterchangeNodes(holder.get());
        removeUnrenderedNodes(holder.get());
        restoreAndRemoveTestRenderingNodesToFragment(holder.get());
    }
}

void ReplacementFragment::removeContentsWithSideEffects()
{
    Vector<Ref<Element>> elementsToRemove;
    Vector<std::pair<Ref<Element>, QualifiedName>> attributesToRemove;

    auto it = descendantsOfType<Element>(*m_fragment).begin();
    auto end = descendantsOfType<Element>(*m_fragment).end();
    while (it != end) {
        auto element = makeRef(*it);
        if (isScriptElement(element) || (is<HTMLStyleElement>(element) && element->getAttribute(classAttr) != WebKitMSOListQuirksStyle)
            || is<HTMLBaseElement>(element) || is<HTMLLinkElement>(element) || is<HTMLMetaElement>(element) || is<HTMLTitleElement>(element)) {
            elementsToRemove.append(WTFMove(element));
            it.traverseNextSkippingChildren();
            continue;
        }
        if (element->hasAttributes()) {
            for (auto& attribute : element->attributesIterator()) {
                if (element->isEventHandlerAttribute(attribute) || element->isJavaScriptURLAttribute(attribute))
                    attributesToRemove.append({ element.copyRef(), attribute.name() });
            }
        }
        ++it;
    }

    for (auto& element : elementsToRemove)
        removeNode(WTFMove(element));

    for (auto& item : attributesToRemove)
        item.first->removeAttribute(item.second);
}

bool ReplacementFragment::isEmpty() const
{
    return (!m_fragment || !m_fragment->firstChild()) && !m_hasInterchangeNewlineAtStart && !m_hasInterchangeNewlineAtEnd;
}

Node *ReplacementFragment::firstChild() const 
{ 
    return m_fragment ? m_fragment->firstChild() : 0; 
}

Node *ReplacementFragment::lastChild() const 
{ 
    return m_fragment ? m_fragment->lastChild() : 0; 
}

void ReplacementFragment::removeNodePreservingChildren(Node& node)
{
    Ref<Node> protectedNode = node;
    while (RefPtr<Node> n = node.firstChild()) {
        removeNode(*n);
        insertNodeBefore(*n, node);
    }
    removeNode(node);
}

void ReplacementFragment::removeNode(Node& node)
{
    ContainerNode* parent = node.nonShadowBoundaryParentNode();
    if (!parent)
        return;
    
    parent->removeChild(node);
}

void ReplacementFragment::insertNodeBefore(Node& node, Node& refNode)
{
    ContainerNode* parent = refNode.nonShadowBoundaryParentNode();
    if (!parent)
        return;
        
    parent->insertBefore(node, &refNode);
}

Ref<HTMLElement> ReplacementFragment::insertFragmentForTestRendering(Node* rootNode)
{
    auto document = makeRef(rootNode->document());
    auto holder = createDefaultParagraphElement(document.get());

    holder->appendChild(*m_fragment);
    rootNode->appendChild(holder);
    document->updateLayoutIgnorePendingStylesheets();

    return holder;
}

void ReplacementFragment::restoreAndRemoveTestRenderingNodesToFragment(StyledElement* holder)
{
    if (!holder)
        return;
    
    while (RefPtr<Node> node = holder->firstChild()) {
        holder->removeChild(*node);
        m_fragment->appendChild(*node);
    }

    removeNode(*holder);
}

void ReplacementFragment::removeUnrenderedNodes(Node* holder)
{
    Vector<Ref<Node>> unrendered;

    for (Node* node = holder->firstChild(); node; node = NodeTraversal::next(*node, holder)) {
        if (!isNodeRendered(*node) && !isTableStructureNode(node))
            unrendered.append(*node);
    }

    for (auto& node : unrendered)
        removeNode(node);
}

void ReplacementFragment::removeInterchangeNodes(Node* container)
{
    m_hasInterchangeNewlineAtStart = false;
    m_hasInterchangeNewlineAtEnd = false;

    // Interchange newlines at the "start" of the incoming fragment must be
    // either the first node in the fragment or the first leaf in the fragment.
    Node* node = container->firstChild();
    while (node) {
        if (isInterchangeNewlineNode(node)) {
            m_hasInterchangeNewlineAtStart = true;
            removeNode(*node);
            break;
        }
        node = node->firstChild();
    }
    if (!container->hasChildNodes())
        return;
    // Interchange newlines at the "end" of the incoming fragment must be
    // either the last node in the fragment or the last leaf in the fragment.
    node = container->lastChild();
    while (node) {
        if (isInterchangeNewlineNode(node)) {
            m_hasInterchangeNewlineAtEnd = true;
            removeNode(*node);
            break;
        }
        node = node->lastChild();
    }
    
    node = container->firstChild();
    while (node) {
        RefPtr<Node> next = NodeTraversal::next(*node);
        if (isInterchangeConvertedSpaceSpan(node)) {
            next = NodeTraversal::nextSkippingChildren(*node);
            removeNodePreservingChildren(*node);
        }
        node = next.get();
    }
}

inline void ReplaceSelectionCommand::InsertedNodes::respondToNodeInsertion(Node* node)
{
    if (!node)
        return;
    
    if (!m_firstNodeInserted)
        m_firstNodeInserted = node;
    
    m_lastNodeInserted = node;
}

inline void ReplaceSelectionCommand::InsertedNodes::willRemoveNodePreservingChildren(Node* node)
{
    if (m_firstNodeInserted == node)
        m_firstNodeInserted = NodeTraversal::next(*node);
    if (m_lastNodeInserted == node) {
        m_lastNodeInserted = node->lastChild() ? node->lastChild() : NodeTraversal::nextSkippingChildren(*node);
        if (!m_lastNodeInserted && m_firstNodeInserted) {
            // If the last inserted node is at the end of the document and doesn't have any children, look backwards for the
            // previous node as the last inserted node, clamping to the first inserted node if needed to ensure that the
            // document position of the last inserted node is not behind the first inserted node.
            auto* previousNode = NodeTraversal::previousSkippingChildren(*node);
            ASSERT(previousNode);
            m_lastNodeInserted = m_firstNodeInserted->compareDocumentPosition(*previousNode) & Node::DOCUMENT_POSITION_FOLLOWING ? previousNode : m_firstNodeInserted;
        }
    }
}

inline void ReplaceSelectionCommand::InsertedNodes::willRemoveNode(Node* node)
{
    if (m_firstNodeInserted == node && m_lastNodeInserted == node) {
        m_firstNodeInserted = nullptr;
        m_lastNodeInserted = nullptr;
    } else if (m_firstNodeInserted == node)
        m_firstNodeInserted = NodeTraversal::nextSkippingChildren(*m_firstNodeInserted);
    else if (m_lastNodeInserted == node)
        m_lastNodeInserted = NodeTraversal::previousSkippingChildren(*m_lastNodeInserted);
}

inline void ReplaceSelectionCommand::InsertedNodes::didReplaceNode(Node* node, Node* newNode)
{
    if (m_firstNodeInserted == node)
        m_firstNodeInserted = newNode;
    if (m_lastNodeInserted == node)
        m_lastNodeInserted = newNode;
}

ReplaceSelectionCommand::ReplaceSelectionCommand(Document& document, RefPtr<DocumentFragment>&& fragment, OptionSet<CommandOption> options, EditAction editAction)
    : CompositeEditCommand(document, editAction)
    , m_selectReplacement(options & SelectReplacement)
    , m_smartReplace(options & SmartReplace)
    , m_matchStyle(options & MatchStyle)
    , m_documentFragment(fragment)
    , m_preventNesting(options & PreventNesting)
    , m_movingParagraph(options & MovingParagraph)
    , m_sanitizeFragment(options & SanitizeFragment)
    , m_shouldMergeEnd(false)
    , m_ignoreMailBlockquote(options & IgnoreMailBlockquote)
{
}

static bool hasMatchingQuoteLevel(VisiblePosition endOfExistingContent, VisiblePosition endOfInsertedContent)
{
    Position existing = endOfExistingContent.deepEquivalent();
    Position inserted = endOfInsertedContent.deepEquivalent();
    bool isInsideMailBlockquote = enclosingNodeOfType(inserted, isMailBlockquote, CanCrossEditingBoundary);
    return isInsideMailBlockquote && (numEnclosingMailBlockquotes(existing) == numEnclosingMailBlockquotes(inserted));
}

bool ReplaceSelectionCommand::shouldMergeStart(bool selectionStartWasStartOfParagraph, bool fragmentHasInterchangeNewlineAtStart, bool selectionStartWasInsideMailBlockquote)
{
    if (m_movingParagraph)
        return false;
    
    VisiblePosition startOfInsertedContent(positionAtStartOfInsertedContent());
    VisiblePosition prev = startOfInsertedContent.previous(CannotCrossEditingBoundary);
    if (prev.isNull())
        return false;
    
    // When we have matching quote levels, its ok to merge more frequently.
    // For a successful merge, we still need to make sure that the inserted content starts with the beginning of a paragraph.
    // And we should only merge here if the selection start was inside a mail blockquote.  This prevents against removing a 
    // blockquote from newly pasted quoted content that was pasted into an unquoted position.  If that unquoted position happens 
    // to be right after another blockquote, we don't want to merge and risk stripping a valid block (and newline) from the pasted content.
    if (isStartOfParagraph(startOfInsertedContent) && selectionStartWasInsideMailBlockquote && hasMatchingQuoteLevel(prev, positionAtEndOfInsertedContent()))
        return true;

    return !selectionStartWasStartOfParagraph
        && !fragmentHasInterchangeNewlineAtStart
        && isStartOfParagraph(startOfInsertedContent)
        && !startOfInsertedContent.deepEquivalent().deprecatedNode()->hasTagName(brTag)
        && shouldMerge(startOfInsertedContent, prev);
}

bool ReplaceSelectionCommand::shouldMergeEnd(bool selectionEndWasEndOfParagraph)
{
    VisiblePosition endOfInsertedContent(positionAtEndOfInsertedContent());
    VisiblePosition next = endOfInsertedContent.next(CannotCrossEditingBoundary);
    if (next.isNull())
        return false;

    return !selectionEndWasEndOfParagraph
        && isEndOfParagraph(endOfInsertedContent)
        && !endOfInsertedContent.deepEquivalent().deprecatedNode()->hasTagName(brTag)
        && shouldMerge(endOfInsertedContent, next);
}

static bool isMailPasteAsQuotationNode(const Node* node)
{
    return node && node->hasTagName(blockquoteTag) && downcast<Element>(node)->attributeWithoutSynchronization(classAttr) == ApplePasteAsQuotation;
}

static bool isHeaderElement(const Node* a)
{
    if (!a)
        return false;
        
    return a->hasTagName(h1Tag)
        || a->hasTagName(h2Tag)
        || a->hasTagName(h3Tag)
        || a->hasTagName(h4Tag)
        || a->hasTagName(h5Tag)
        || a->hasTagName(h6Tag);
}

static bool haveSameTagName(Node* a, Node* b)
{
    return is<Element>(a) && is<Element>(b) && downcast<Element>(*a).tagName() == downcast<Element>(*b).tagName();
}

bool ReplaceSelectionCommand::shouldMerge(const VisiblePosition& source, const VisiblePosition& destination)
{
    if (source.isNull() || destination.isNull())
        return false;
        
    auto* sourceNode = source.deepEquivalent().deprecatedNode();
    auto* destinationNode = destination.deepEquivalent().deprecatedNode();
    auto* sourceBlock = enclosingBlock(sourceNode);
    auto* destinationBlock = enclosingBlock(destinationNode);
    return !enclosingNodeOfType(source.deepEquivalent(), &isMailPasteAsQuotationNode)
        && sourceBlock
        && (!sourceBlock->hasTagName(blockquoteTag) || isMailBlockquote(sourceBlock))
        && enclosingListChild(sourceBlock) == enclosingListChild(destinationNode)
        && enclosingTableCell(source.deepEquivalent()) == enclosingTableCell(destination.deepEquivalent())
        && (!isHeaderElement(sourceBlock) || haveSameTagName(sourceBlock, destinationBlock))
        // Don't merge to or from a position before or after a block because it would
        // be a no-op and cause infinite recursion.
        && !isBlock(sourceNode) && !isBlock(destinationNode);
}

static bool fragmentNeedsColorTransformed(ReplacementFragment& fragment, const Position& insertionPos)
{
    // Dark mode content that is inserted should have the inline styles inverse color
    // transformed by the color filter to match the color filtered document contents.
    // This applies to Mail and Notes when pasting from Xcode. <rdar://problem/40529867>

    RefPtr<Element> editableRoot = insertionPos.rootEditableElement();
    ASSERT(editableRoot);
    if (!editableRoot)
        return false;

    auto* editableRootRenderer = editableRoot->renderer();
    if (!editableRootRenderer || !editableRootRenderer->style().hasAppleColorFilter())
        return false;

    const auto& colorFilter = editableRootRenderer->style().appleColorFilter();
    for (const auto& colorFilterOperation : colorFilter.operations()) {
        if (colorFilterOperation->type() != FilterOperation::APPLE_INVERT_LIGHTNESS)
            return false;
    }

    auto propertyLightness = [&](const StyleProperties& inlineStyle, CSSPropertyID propertyID) -> Optional<double> {
        auto color = inlineStyle.propertyAsColor(propertyID);
        if (!color || !color.value().isVisible() || color.value().isSemantic())
            return { };

        return color.value().lightness();
    };

    const double lightnessDarkEnoughForText = 0.4;
    const double lightnessLightEnoughForBackground = 0.6;

    for (RefPtr<Node> node = fragment.firstChild(); node; node = NodeTraversal::next(*node)) {
        if (!is<StyledElement>(*node))
            continue;

        auto& element = downcast<StyledElement>(*node);
        auto* inlineStyle = element.inlineStyle();
        if (!inlineStyle)
            continue;

        auto textLightness = propertyLightness(*inlineStyle, CSSPropertyColor);
        if (textLightness && *textLightness < lightnessDarkEnoughForText)
            return false;

        auto backgroundLightness = propertyLightness(*inlineStyle, CSSPropertyBackgroundColor);
        if (backgroundLightness && *backgroundLightness > lightnessLightEnoughForBackground)
            return false;
    }

    return true;
}

void ReplaceSelectionCommand::inverseTransformColor(InsertedNodes& insertedNodes)
{
    RefPtr<Node> pastEndNode = insertedNodes.pastLastLeaf();
    for (RefPtr<Node> node = insertedNodes.firstNodeInserted(); node && node != pastEndNode; node = NodeTraversal::next(*node)) {
        if (!is<StyledElement>(*node))
            continue;

        auto& element = downcast<StyledElement>(*node);
        auto* inlineStyle = element.inlineStyle();
        if (!inlineStyle)
            continue;

        auto editingStyle = EditingStyle::create(inlineStyle);
        auto transformedStyle = editingStyle->inverseTransformColorIfNeeded(element);
        if (editingStyle.ptr() == transformedStyle.ptr())
            continue;

        setNodeAttribute(element, styleAttr, transformedStyle->style()->asText());
    }
}

// Style rules that match just inserted elements could change their appearance, like
// a div inserted into a document with div { display:inline; }.
void ReplaceSelectionCommand::removeRedundantStylesAndKeepStyleSpanInline(InsertedNodes& insertedNodes)
{
    RefPtr<Node> pastEndNode = insertedNodes.pastLastLeaf();
    RefPtr<Node> next;
    for (RefPtr<Node> node = insertedNodes.firstNodeInserted(); node && node != pastEndNode; node = next) {
        // FIXME: <rdar://problem/5371536> Style rules that match pasted content can change it's appearance

        next = NodeTraversal::next(*node);
        if (!is<StyledElement>(*node))
            continue;

        StyledElement* element = downcast<StyledElement>(node.get());

        const StyleProperties* inlineStyle = element->inlineStyle();
        auto newInlineStyle = EditingStyle::create(inlineStyle);
        if (inlineStyle) {
            if (is<HTMLElement>(*element)) {
                Vector<QualifiedName> attributes;
                HTMLElement& htmlElement = downcast<HTMLElement>(*element);

                if (newInlineStyle->conflictsWithImplicitStyleOfElement(htmlElement)) {
                    // e.g. <b style="font-weight: normal;"> is converted to <span style="font-weight: normal;">
                    node = replaceElementWithSpanPreservingChildrenAndAttributes(htmlElement);
                    element = downcast<StyledElement>(node.get());
                    insertedNodes.didReplaceNode(&htmlElement, node.get());
                } else if (newInlineStyle->extractConflictingImplicitStyleOfAttributes(htmlElement, EditingStyle::PreserveWritingDirection, nullptr, attributes, EditingStyle::DoNotExtractMatchingStyle)) {
                    // e.g. <font size="3" style="font-size: 20px;"> is converted to <font style="font-size: 20px;">
                    for (auto& attribute : attributes)
                        removeNodeAttribute(*element, attribute);
                }
            }

            ContainerNode* context = element->parentNode();

            // If Mail wraps the fragment with a Paste as Quotation blockquote, or if you're pasting into a quoted region,
            // styles from blockquoteNode are allowed to override those from the source document, see <rdar://problem/4930986> and <rdar://problem/5089327>.
            Node* blockquoteNode = isMailPasteAsQuotationNode(context) ? context : enclosingNodeOfType(firstPositionInNode(context), isMailBlockquote, CanCrossEditingBoundary);
            if (blockquoteNode)
                newInlineStyle->removeStyleFromRulesAndContext(*element, document().documentElement());

            newInlineStyle->removeStyleFromRulesAndContext(*element, context);
        }

        if (!inlineStyle || newInlineStyle->isEmpty()) {
            if (isStyleSpanOrSpanWithOnlyStyleAttribute(*element) || isEmptyFontTag(element, AllowNonEmptyStyleAttribute)) {
                insertedNodes.willRemoveNodePreservingChildren(element);
                removeNodePreservingChildren(*element);
                continue;
            }
            removeNodeAttribute(*element, styleAttr);
        } else if (newInlineStyle->style()->propertyCount() != inlineStyle->propertyCount())
            setNodeAttribute(*element, styleAttr, newInlineStyle->style()->asText());

        // FIXME: Tolerate differences in id, class, and style attributes.
        if (element->parentNode() && isNonTableCellHTMLBlockElement(element) && areIdenticalElements(*element, *element->parentNode())
            && VisiblePosition(firstPositionInNode(element->parentNode())) == VisiblePosition(firstPositionInNode(element))
            && VisiblePosition(lastPositionInNode(element->parentNode())) == VisiblePosition(lastPositionInNode(element))) {
            insertedNodes.willRemoveNodePreservingChildren(element);
            removeNodePreservingChildren(*element);
            continue;
        }

        if (element->parentNode() && element->parentNode()->hasRichlyEditableStyle())
            removeNodeAttribute(*element, contenteditableAttr);

        // WebKit used to not add display: inline and float: none on copy.
        // Keep this code around for backward compatibility
        if (isLegacyAppleStyleSpan(element)) {
            if (!element->firstChild()) {
                insertedNodes.willRemoveNodePreservingChildren(element);
                removeNodePreservingChildren(*element);
                continue;
            }
            // There are other styles that style rules can give to style spans,
            // but these are the two important ones because they'll prevent
            // inserted content from appearing in the right paragraph.
            // FIXME: Hyatt is concerned that selectively using display:inline will give inconsistent
            // results. We already know one issue because td elements ignore their display property
            // in quirks mode (which Mail.app is always in). We should look for an alternative.

            // Mutate using the CSSOM wrapper so we get the same event behavior as a script.
            if (isBlock(element))
                element->cssomStyle().setPropertyInternal(CSSPropertyDisplay, "inline", false);
            if (element->renderer() && element->renderer()->style().isFloating())
                element->cssomStyle().setPropertyInternal(CSSPropertyFloat, "none", false);
        }
    }
}

static bool isProhibitedParagraphChild(const AtomString& name)
{
    // https://dvcs.w3.org/hg/editing/raw-file/57abe6d3cb60/editing.html#prohibited-paragraph-child
    static const auto localNames = makeNeverDestroyed([] {
        static const HTMLQualifiedName* const tags[] = {
            &addressTag.get(),
            &articleTag.get(),
            &asideTag.get(),
            &blockquoteTag.get(),
            &captionTag.get(),
            &centerTag.get(),
            &colTag.get(),
            &colgroupTag.get(),
            &ddTag.get(),
            &detailsTag.get(),
            &dirTag.get(),
            &divTag.get(),
            &dlTag.get(),
            &dtTag.get(),
            &fieldsetTag.get(),
            &figcaptionTag.get(),
            &figureTag.get(),
            &footerTag.get(),
            &formTag.get(),
            &h1Tag.get(),
            &h2Tag.get(),
            &h3Tag.get(),
            &h4Tag.get(),
            &h5Tag.get(),
            &h6Tag.get(),
            &headerTag.get(),
            &hgroupTag.get(),
            &hrTag.get(),
            &liTag.get(),
            &listingTag.get(),
            &mainTag.get(), // Missing in the specification.
            &menuTag.get(),
            &navTag.get(),
            &olTag.get(),
            &pTag.get(),
            &plaintextTag.get(),
            &preTag.get(),
            &sectionTag.get(),
            &summaryTag.get(),
            &tableTag.get(),
            &tbodyTag.get(),
            &tdTag.get(),
            &tfootTag.get(),
            &thTag.get(),
            &theadTag.get(),
            &trTag.get(),
            &ulTag.get(),
            &xmpTag.get(),
        };
        HashSet<AtomString> set;
        for (auto& tag : tags)
            set.add(tag->localName());
        return set;
    }());
    return localNames.get().contains(name);
}

void ReplaceSelectionCommand::makeInsertedContentRoundTrippableWithHTMLTreeBuilder(InsertedNodes& insertedNodes)
{
    RefPtr<Node> pastEndNode = insertedNodes.pastLastLeaf();
    RefPtr<Node> next;
    for (RefPtr<Node> node = insertedNodes.firstNodeInserted(); node && node != pastEndNode; node = next) {
        next = NodeTraversal::next(*node);

        if (!is<HTMLElement>(*node))
            continue;

        if (isProhibitedParagraphChild(downcast<HTMLElement>(*node).localName())) {
            if (auto* paragraphElement = enclosingElementWithTag(positionInParentBeforeNode(node.get()), pTag)) {
                auto* parent = paragraphElement->parentNode();
                if (parent && parent->hasEditableStyle())
                    moveNodeOutOfAncestor(*node, *paragraphElement, insertedNodes);
            }
        }

        if (isHeaderElement(node.get())) {
            if (auto* headerElement = highestEnclosingNodeOfType(positionInParentBeforeNode(node.get()), isHeaderElement)) {
                if (headerElement->parentNode() && headerElement->parentNode()->isContentRichlyEditable())
                    moveNodeOutOfAncestor(*node, *headerElement, insertedNodes);
                else {
                    HTMLElement* newSpanElement = replaceElementWithSpanPreservingChildrenAndAttributes(downcast<HTMLElement>(*node));
                    insertedNodes.didReplaceNode(node.get(), newSpanElement);
                }
            }
        }
    }
}

static inline bool hasRenderedText(const Text& text)
{
    return text.renderer() && text.renderer()->hasRenderedText();
}

void ReplaceSelectionCommand::moveNodeOutOfAncestor(Node& node, Node& ancestor, InsertedNodes& insertedNodes)
{
    Ref<Node> protectedNode = node;
    Ref<Node> protectedAncestor = ancestor;

    VisiblePosition positionAtEndOfNode = lastPositionInOrAfterNode(&node);
    VisiblePosition lastPositionInParagraph = lastPositionInNode(&ancestor);
    if (positionAtEndOfNode == lastPositionInParagraph) {
        removeNode(node);
        if (ancestor.nextSibling())
            insertNodeBefore(WTFMove(protectedNode), *ancestor.nextSibling());
        else
            appendNode(WTFMove(protectedNode), *ancestor.parentNode());
    } else {
        RefPtr<Node> nodeToSplitTo = splitTreeToNode(node, ancestor, true);
        removeNode(node);
        insertNodeBefore(WTFMove(protectedNode), *nodeToSplitTo);
    }

    document().updateLayoutIgnorePendingStylesheets();

    bool safeToRemoveAncestor = true;
    for (auto* child = ancestor.firstChild(); child; child = child->nextSibling()) {
        if (is<Text>(child) && hasRenderedText(downcast<Text>(*child))) {
            safeToRemoveAncestor = false;
            break;
        }

        if (is<Element>(child)) {
            safeToRemoveAncestor = false;
            break;
        }
    }

    if (safeToRemoveAncestor) {
        insertedNodes.willRemoveNode(&ancestor);
        removeNode(ancestor);
    }
}

void ReplaceSelectionCommand::removeUnrenderedTextNodesAtEnds(InsertedNodes& insertedNodes)
{
    document().updateLayoutIgnorePendingStylesheets();

    Node* lastLeafInserted = insertedNodes.lastLeafInserted();
    if (is<Text>(lastLeafInserted) && !hasRenderedText(downcast<Text>(*lastLeafInserted))
        && !enclosingElementWithTag(firstPositionInOrBeforeNode(lastLeafInserted), selectTag)
        && !enclosingElementWithTag(firstPositionInOrBeforeNode(lastLeafInserted), scriptTag)) {
        insertedNodes.willRemoveNode(lastLeafInserted);
        removeNode(*lastLeafInserted);
    }

    // We don't have to make sure that firstNodeInserted isn't inside a select or script element
    // because it is a top level node in the fragment and the user can't insert into those elements.
    Node* firstNodeInserted = insertedNodes.firstNodeInserted();
    if (is<Text>(firstNodeInserted) && !hasRenderedText(downcast<Text>(*firstNodeInserted))) {
        insertedNodes.willRemoveNode(firstNodeInserted);
        removeNode(*firstNodeInserted);
    }
}

VisiblePosition ReplaceSelectionCommand::positionAtEndOfInsertedContent() const
{
    // FIXME: Why is this hack here?  What's special about <select> tags?
    auto* enclosingSelect = enclosingElementWithTag(m_endOfInsertedContent, selectTag);
    return enclosingSelect ? lastPositionInOrAfterNode(enclosingSelect) : m_endOfInsertedContent;
}

VisiblePosition ReplaceSelectionCommand::positionAtStartOfInsertedContent() const
{
    return m_startOfInsertedContent;
}

// Remove style spans before insertion if they are unnecessary.  It's faster because we'll 
// avoid doing a layout.
static bool handleStyleSpansBeforeInsertion(ReplacementFragment& fragment, const Position& insertionPos)
{
    Node* topNode = fragment.firstChild();

    // Handling the case where we are doing Paste as Quotation or pasting into quoted content is more complicated (see handleStyleSpans)
    // and doesn't receive the optimization.
    if (isMailPasteAsQuotationNode(topNode) || enclosingNodeOfType(firstPositionInOrBeforeNode(topNode), isMailBlockquote, CanCrossEditingBoundary))
        return false;

    // Either there are no style spans in the fragment or a WebKit client has added content to the fragment
    // before inserting it.  Look for and handle style spans after insertion.
    if (!isLegacyAppleStyleSpan(topNode))
        return false;

    Node* wrappingStyleSpan = topNode;
    auto styleAtInsertionPos = EditingStyle::create(insertionPos.parentAnchoredEquivalent());
    String styleText = styleAtInsertionPos->style()->asText();

    // FIXME: This string comparison is a naive way of comparing two styles.
    // We should be taking the diff and check that the diff is empty.
    if (styleText != downcast<Element>(*wrappingStyleSpan).getAttribute(styleAttr))
        return false;

    fragment.removeNodePreservingChildren(*wrappingStyleSpan);
    return true;
}

// At copy time, WebKit wraps copied content in a span that contains the source document's 
// default styles.  If the copied Range inherits any other styles from its ancestors, we put 
// those styles on a second span.
// This function removes redundant styles from those spans, and removes the spans if all their 
// styles are redundant. 
// We should remove the Apple-style-span class when we're done, see <rdar://problem/5685600>.
// We should remove styles from spans that are overridden by all of their children, either here
// or at copy time.
void ReplaceSelectionCommand::handleStyleSpans(InsertedNodes& insertedNodes)
{
    HTMLElement* wrappingStyleSpan = nullptr;
    // The style span that contains the source document's default style should be at
    // the top of the fragment, but Mail sometimes adds a wrapper (for Paste As Quotation),
    // so search for the top level style span instead of assuming it's at the top.
    for (Node* node = insertedNodes.firstNodeInserted(); node; node = NodeTraversal::next(*node)) {
        if (isLegacyAppleStyleSpan(node)) {
            wrappingStyleSpan = downcast<HTMLElement>(node);
            break;
        }
    }
    
    // There might not be any style spans if we're pasting from another application or if 
    // we are here because of a document.execCommand("InsertHTML", ...) call.
    if (!wrappingStyleSpan)
        return;

    auto style = EditingStyle::create(wrappingStyleSpan->inlineStyle());
    ContainerNode* context = wrappingStyleSpan->parentNode();

    // If Mail wraps the fragment with a Paste as Quotation blockquote, or if you're pasting into a quoted region,
    // styles from blockquoteNode are allowed to override those from the source document, see <rdar://problem/4930986> and <rdar://problem/5089327>.
    Node* blockquoteNode = isMailPasteAsQuotationNode(context) ? context : enclosingNodeOfType(firstPositionInNode(context), isMailBlockquote, CanCrossEditingBoundary);
    if (blockquoteNode)
        context = document().documentElement();

    // This operation requires that only editing styles to be removed from sourceDocumentStyle.
    style->prepareToApplyAt(firstPositionInNode(context));

    // Remove block properties in the span's style. This prevents properties that probably have no effect 
    // currently from affecting blocks later if the style is cloned for a new block element during a future 
    // editing operation.
    // FIXME: They *can* have an effect currently if blocks beneath the style span aren't individually marked
    // with block styles by the editing engine used to style them.  WebKit doesn't do this, but others might.
    style->removeBlockProperties();

    if (style->isEmpty() || !wrappingStyleSpan->firstChild()) {
        insertedNodes.willRemoveNodePreservingChildren(wrappingStyleSpan);
        removeNodePreservingChildren(*wrappingStyleSpan);
    } else
        setNodeAttribute(*wrappingStyleSpan, styleAttr, style->style()->asText());
}

void ReplaceSelectionCommand::mergeEndIfNeeded()
{
    if (!m_shouldMergeEnd)
        return;

    VisiblePosition startOfInsertedContent(positionAtStartOfInsertedContent());
    VisiblePosition endOfInsertedContent(positionAtEndOfInsertedContent());
    
    // Bail to avoid infinite recursion.
    if (m_movingParagraph) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    // Merging two paragraphs will destroy the moved one's block styles.  Always move the end of inserted forward 
    // to preserve the block style of the paragraph already in the document, unless the paragraph to move would 
    // include the what was the start of the selection that was pasted into, so that we preserve that paragraph's
    // block styles.
    bool mergeForward = !(inSameParagraph(startOfInsertedContent, endOfInsertedContent) && !isStartOfParagraph(startOfInsertedContent));
    
    VisiblePosition destination = mergeForward ? endOfInsertedContent.next() : endOfInsertedContent;
    VisiblePosition startOfParagraphToMove = mergeForward ? startOfParagraph(endOfInsertedContent) : endOfInsertedContent.next();
   
    // Merging forward could result in deleting the destination anchor node.
    // To avoid this, we add a placeholder node before the start of the paragraph.
    if (endOfParagraph(startOfParagraphToMove) == destination) {
        auto placeholder = HTMLBRElement::create(document());
        auto* placeholderPtr = placeholder.ptr();
        insertNodeBefore(WTFMove(placeholder), *startOfParagraphToMove.deepEquivalent().deprecatedNode());
        destination = VisiblePosition(positionBeforeNode(placeholderPtr));
    }

    moveParagraph(startOfParagraphToMove, endOfParagraph(startOfParagraphToMove), destination);
    
    // Merging forward will remove m_endOfInsertedContent from the document.
    if (mergeForward) {
        if (m_startOfInsertedContent.isOrphan())
            m_startOfInsertedContent = endingSelection().visibleStart().deepEquivalent();
         m_endOfInsertedContent = endingSelection().visibleEnd().deepEquivalent();
        // If we merged text nodes, m_endOfInsertedContent could be null. If this is the case, we use m_startOfInsertedContent.
        if (m_endOfInsertedContent.isNull())
            m_endOfInsertedContent = m_startOfInsertedContent;
    }
}

static Node* enclosingInline(Node* node)
{
    while (ContainerNode* parent = node->parentNode()) {
        if (isBlockFlowElement(*parent) || parent->hasTagName(bodyTag))
            return node;
        // Stop if any previous sibling is a block.
        for (Node* sibling = node->previousSibling(); sibling; sibling = sibling->previousSibling()) {
            if (isBlockFlowElement(*sibling))
                return node;
        }
        node = parent;
    }
    return node;
}

static bool isInlineNodeWithStyle(const Node* node)
{
    // We don't want to skip over any block elements.
    if (isBlock(node))
        return false;

    if (!node->isHTMLElement())
        return false;

    // We can skip over elements whose class attribute is
    // one of our internal classes.
    const HTMLElement* element = static_cast<const HTMLElement*>(node);
    const AtomString& classAttributeValue = element->attributeWithoutSynchronization(classAttr);
    if (classAttributeValue == AppleTabSpanClass
        || classAttributeValue == AppleConvertedSpace
        || classAttributeValue == ApplePasteAsQuotation)
        return true;

    return EditingStyle::elementIsStyledSpanOrHTMLEquivalent(*element);
}

inline Node* nodeToSplitToAvoidPastingIntoInlineNodesWithStyle(const Position& insertionPos)
{
    Node* containgBlock = enclosingBlock(insertionPos.containerNode());
    return highestEnclosingNodeOfType(insertionPos, isInlineNodeWithStyle, CannotCrossEditingBoundary, containgBlock);
}

bool ReplaceSelectionCommand::willApplyCommand()
{
    m_documentFragmentPlainText = m_documentFragment->textContent();
    m_documentFragmentHTMLMarkup = serializeFragment(*m_documentFragment, SerializedNodes::SubtreeIncludingNode);
    ensureReplacementFragment();
    return CompositeEditCommand::willApplyCommand();
}
    
static bool hasBlankLineBetweenParagraphs(Position& position)
{
    bool reachedBoundaryStart = false;
    bool reachedBoundaryEnd = false;
    VisiblePosition visiblePosition(position);
    VisiblePosition previousPosition = visiblePosition.previous(CannotCrossEditingBoundary, &reachedBoundaryStart);
    VisiblePosition nextPosition = visiblePosition.next(CannotCrossEditingBoundary, &reachedBoundaryStart);
    bool hasLineBeforePosition = isEndOfLine(previousPosition);
    
    return !reachedBoundaryStart && !reachedBoundaryEnd && isBlankParagraph(visiblePosition) && hasLineBeforePosition && isStartOfLine(nextPosition);
}

void ReplaceSelectionCommand::doApply()
{
    VisibleSelection selection = endingSelection();
    ASSERT(selection.isCaretOrRange());
    ASSERT(selection.start().deprecatedNode());
    if (selection.isNoneOrOrphaned() || !selection.start().deprecatedNode() || !selection.isContentEditable())
        return;

    // In plain text only regions, we create style-less fragments, so the inserted content will automatically
    // match the style of the surrounding area and so we can avoid unnecessary work below for m_matchStyle.
    if (!selection.isContentRichlyEditable())
        m_matchStyle = false;

    ReplacementFragment& fragment = *ensureReplacementFragment();
    if (performTrivialReplace(fragment))
        return;
    
    // We can skip matching the style if the selection is plain text.
    if ((selection.start().deprecatedNode()->renderer() && selection.start().deprecatedNode()->renderer()->style().userModify() == UserModify::ReadWritePlaintextOnly)
        && (selection.end().deprecatedNode()->renderer() && selection.end().deprecatedNode()->renderer()->style().userModify() == UserModify::ReadWritePlaintextOnly))
        m_matchStyle = false;
    
    if (m_matchStyle) {
        m_insertionStyle = EditingStyle::create(selection.start());
        m_insertionStyle->mergeTypingStyle(document());
    }

    VisiblePosition visibleStart = selection.visibleStart();
    VisiblePosition visibleEnd = selection.visibleEnd();
    
    bool selectionEndWasEndOfParagraph = isEndOfParagraph(visibleEnd);
    bool selectionStartWasStartOfParagraph = isStartOfParagraph(visibleStart);
    
    Node* startBlock = enclosingBlock(visibleStart.deepEquivalent().deprecatedNode());
    
    Position insertionPos = selection.start();
    bool shouldHandleMailBlockquote = enclosingNodeOfType(insertionPos, isMailBlockquote, CanCrossEditingBoundary) && !m_ignoreMailBlockquote;
    bool selectionIsPlainText = !selection.isContentRichlyEditable();
    Element* currentRoot = selection.rootEditableElement();

    if ((selectionStartWasStartOfParagraph && selectionEndWasEndOfParagraph && !shouldHandleMailBlockquote)
        || startBlock == currentRoot || isListItem(startBlock) || selectionIsPlainText)
        m_preventNesting = false;
    
    if (selection.isRange()) {
        // When the end of the selection being pasted into is at the end of a paragraph, and that selection
        // spans multiple blocks, not merging may leave an empty line.
        // When the start of the selection being pasted into is at the start of a block, not merging 
        // will leave hanging block(s).
        // Merge blocks if the start of the selection was in a Mail blockquote, since we handle  
        // that case specially to prevent nesting. 
        bool mergeBlocksAfterDelete = shouldHandleMailBlockquote || isEndOfParagraph(visibleEnd) || isStartOfBlock(visibleStart);
        // FIXME: We should only expand to include fully selected special elements if we are copying a 
        // selection and pasting it on top of itself.
        // FIXME: capturing the content of this delete would allow a replace accessibility notification instead of a simple insert
        deleteSelection(false, mergeBlocksAfterDelete, true, false, true);
        visibleStart = endingSelection().visibleStart();
        if (fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfEditableOrNonEditableContent(visibleStart))
                    setEndingSelection(visibleStart.next());
            } else
                insertParagraphSeparator();
        }
        insertionPos = endingSelection().start();
    } else {
        ASSERT(selection.isCaret());
        if (fragment.hasInterchangeNewlineAtStart()) {
            VisiblePosition next = visibleStart.next(CannotCrossEditingBoundary);
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart) && next.isNotNull())
                setEndingSelection(next);
            else  {
                insertParagraphSeparator();
                visibleStart = endingSelection().visibleStart();
            }
        }
        // We split the current paragraph in two to avoid nesting the blocks from the fragment inside the current block.
        // For example paste <div>foo</div><div>bar</div><div>baz</div> into <div>x^x</div>, where ^ is the caret.  
        // As long as the  div styles are the same, visually you'd expect: <div>xbar</div><div>bar</div><div>bazx</div>, 
        // not <div>xbar<div>bar</div><div>bazx</div></div>.
        // Don't do this if the selection started in a Mail blockquote.
        if (m_preventNesting && !shouldHandleMailBlockquote && !isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
            insertParagraphSeparator();
            setEndingSelection(endingSelection().visibleStart().previous());
        }
        insertionPos = endingSelection().start();
    }
    
    // We don't want any of the pasted content to end up nested in a Mail blockquote, so first break 
    // out of any surrounding Mail blockquotes. Unless we're inserting in a table, in which case
    // breaking the blockquote will prevent the content from actually being inserted in the table.
    if (shouldHandleMailBlockquote && m_preventNesting && !(enclosingNodeOfType(insertionPos, &isTableStructureNode))) {
        applyCommandToComposite(BreakBlockquoteCommand::create(document())); 
        // This will leave a br between the split. 
        Node* br = endingSelection().start().deprecatedNode(); 
        ASSERT(br->hasTagName(brTag)); 
        // Insert content between the two blockquotes, but remove the br (since it was just a placeholder). 
        insertionPos = positionInParentBeforeNode(br);
        removeNode(*br);
    }
    
    // Inserting content could cause whitespace to collapse, e.g. inserting <div>foo</div> into hello^ world.
    prepareWhitespaceAtPositionForSplit(insertionPos);

    // If the downstream node has been removed there's no point in continuing.
    if (!insertionPos.downstream().deprecatedNode())
      return;
    
    // NOTE: This would be an incorrect usage of downstream() if downstream() were changed to mean the last position after 
    // p that maps to the same visible position as p (since in the case where a br is at the end of a block and collapsed 
    // away, there are positions after the br which map to the same visible position as [br, 0]).  
    RefPtr<Node> endBR = insertionPos.downstream().deprecatedNode()->hasTagName(brTag) ? insertionPos.downstream().deprecatedNode() : nullptr;
    VisiblePosition originalVisPosBeforeEndBR;
    if (endBR)
        originalVisPosBeforeEndBR = VisiblePosition(positionBeforeNode(endBR.get()), DOWNSTREAM).previous();
    
    RefPtr<Node> insertionBlock = enclosingBlock(insertionPos.deprecatedNode());
    
    // Adjust insertionPos to prevent nesting.
    // If the start was in a Mail blockquote, we will have already handled adjusting insertionPos above.
    if (m_preventNesting && insertionBlock && !isTableCell(insertionBlock.get()) && !shouldHandleMailBlockquote) {
        ASSERT(insertionBlock != currentRoot);
        VisiblePosition visibleInsertionPos(insertionPos);
        if (isEndOfBlock(visibleInsertionPos) && !(isStartOfBlock(visibleInsertionPos) && fragment.hasInterchangeNewlineAtEnd()))
            insertionPos = positionInParentAfterNode(insertionBlock.get());
        else if (isStartOfBlock(visibleInsertionPos))
            insertionPos = positionInParentBeforeNode(insertionBlock.get());
    }
    
    // Paste at start or end of link goes outside of link.
    insertionPos = positionAvoidingSpecialElementBoundary(insertionPos);
    
    // FIXME: Can this wait until after the operation has been performed?  There doesn't seem to be
    // any work performed after this that queries or uses the typing style.
    document().selection().clearTypingStyle();

    // We don't want the destination to end up inside nodes that weren't selected.  To avoid that, we move the
    // position forward without changing the visible position so we're still at the same visible location, but
    // outside of preceding tags.
    insertionPos = positionAvoidingPrecedingNodes(insertionPos);

    // Paste into run of tabs splits the tab span.
    insertionPos = positionOutsideTabSpan(insertionPos);

    bool hasBlankLinesBetweenParagraphs = hasBlankLineBetweenParagraphs(insertionPos);
    bool handledStyleSpans = handleStyleSpansBeforeInsertion(fragment, insertionPos);
    bool needsColorTransformed = fragmentNeedsColorTransformed(fragment, insertionPos);

    // We're finished if there is nothing to add.
    if (fragment.isEmpty() || !fragment.firstChild())
        return;

    // If we are not trying to match the destination style we prefer a position
    // that is outside inline elements that provide style.
    // This way we can produce a less verbose markup.
    // We can skip this optimization for fragments not wrapped in one of
    // our style spans and for positions inside list items
    // since insertAsListItems already does the right thing.
    if (!m_matchStyle && !enclosingList(insertionPos.containerNode())) {
        if (insertionPos.containerNode()->isTextNode() && insertionPos.offsetInContainerNode() && !insertionPos.atLastEditingPositionForNode()) {
            splitTextNode(*insertionPos.containerText(), insertionPos.offsetInContainerNode());
            insertionPos = firstPositionInNode(insertionPos.containerNode());
        }

        if (RefPtr<Node> nodeToSplitTo = nodeToSplitToAvoidPastingIntoInlineNodesWithStyle(insertionPos)) {
            if (insertionPos.containerNode() != nodeToSplitTo->parentNode()) {
                Node* splitStart = insertionPos.computeNodeAfterPosition();
                if (!splitStart)
                    splitStart = insertionPos.containerNode();
                ASSERT(splitStart);
                nodeToSplitTo = splitTreeToNode(*splitStart, *nodeToSplitTo->parentNode()).get();
                insertionPos = positionInParentBeforeNode(nodeToSplitTo.get());
            }
        }
    }

    // FIXME: When pasting rich content we're often prevented from heading down the fast path by style spans.  Try
    // again here if they've been removed.

    // 1) Insert the content.
    // 2) Remove redundant styles and style tags, this inner <b> for example: <b>foo <b>bar</b> baz</b>.
    // 3) Merge the start of the added content with the content before the position being pasted into.
    // 4) Do one of the following: a) expand the last br if the fragment ends with one and it collapsed,
    // b) merge the last paragraph of the incoming fragment with the paragraph that contained the 
    // end of the selection that was pasted into, or c) handle an interchange newline at the end of the 
    // incoming fragment.
    // 5) Add spaces for smart replace.
    // 6) Select the replacement if requested, and match style if requested.

    InsertedNodes insertedNodes;
    RefPtr<Node> refNode = fragment.firstChild();
    RefPtr<Node> node = refNode->nextSibling();
    
    if (refNode)
        fragment.removeNode(*refNode);

    Node* blockStart = enclosingBlock(insertionPos.deprecatedNode());
    bool isInsertingIntoList = (isListHTMLElement(refNode.get()) || (isLegacyAppleStyleSpan(refNode.get()) && isListHTMLElement(refNode->firstChild())))
    && blockStart && blockStart->renderer()->isListItem();
    if (isInsertingIntoList)
        refNode = insertAsListItems(downcast<HTMLElement>(*refNode), blockStart, insertionPos, insertedNodes);
    else {
        insertNodeAt(*refNode, insertionPos);
        insertedNodes.respondToNodeInsertion(refNode.get());
    }

    // Mutation events (bug 22634) may have already removed the inserted content
    if (!refNode->isConnected())
        return;

    bool plainTextFragment = isPlainTextMarkup(refNode.get());

    while (node) {
        RefPtr<Node> next = node->nextSibling();
        fragment.removeNode(*node);
        insertNodeAfter(*node, *refNode);
        insertedNodes.respondToNodeInsertion(node.get());

        // Mutation events (bug 22634) may have already removed the inserted content
        if (!node->isConnected())
            return;

        refNode = node;
        if (node && plainTextFragment)
            plainTextFragment = isPlainTextMarkup(node.get());
        node = next;
    }

    if (insertedNodes.isEmpty())
        return;
    removeUnrenderedTextNodesAtEnds(insertedNodes);

    if (!handledStyleSpans)
        handleStyleSpans(insertedNodes);

    // Mutation events (bug 20161) may have already removed the inserted content
    if (insertedNodes.isEmpty())
        return;
    if (!insertedNodes.firstNodeInserted()->isConnected())
        return;

    VisiblePosition startOfInsertedContent = firstPositionInOrBeforeNode(insertedNodes.firstNodeInserted());

    // We inserted before the insertionBlock to prevent nesting, and the content before the insertionBlock wasn't in its own block and
    // didn't have a br after it, so the inserted content ended up in the same paragraph.
    if (!startOfInsertedContent.isNull() && insertionBlock && insertionPos.deprecatedNode() == insertionBlock->parentNode() && (unsigned)insertionPos.deprecatedEditingOffset() < insertionBlock->computeNodeIndex() && !isStartOfParagraph(startOfInsertedContent))
        insertNodeAt(HTMLBRElement::create(document()), startOfInsertedContent.deepEquivalent());

    if (endBR && (plainTextFragment || shouldRemoveEndBR(endBR.get(), originalVisPosBeforeEndBR))) {
        RefPtr<Node> parent = endBR->parentNode();
        insertedNodes.willRemoveNode(endBR.get());
        removeNode(*endBR);
        if (Node* nodeToRemove = highestNodeToRemoveInPruning(parent.get())) {
            insertedNodes.willRemoveNode(nodeToRemove);
            removeNode(*nodeToRemove);
        }
    }

    makeInsertedContentRoundTrippableWithHTMLTreeBuilder(insertedNodes);
    if (insertedNodes.isEmpty())
        return;

    if (needsColorTransformed)
        inverseTransformColor(insertedNodes);

    removeRedundantStylesAndKeepStyleSpanInline(insertedNodes);
    if (insertedNodes.isEmpty())
        return;

    if (m_sanitizeFragment)
        applyCommandToComposite(SimplifyMarkupCommand::create(document(), insertedNodes.firstNodeInserted(), insertedNodes.pastLastLeaf()));

    // Setup m_startOfInsertedContent and m_endOfInsertedContent. This should be the last two lines of code that access insertedNodes.
    m_startOfInsertedContent = firstPositionInOrBeforeNode(insertedNodes.firstNodeInserted());
    m_endOfInsertedContent = lastPositionInOrAfterNode(insertedNodes.lastLeafInserted());

    // Determine whether or not we should merge the end of inserted content with what's after it before we do
    // the start merge so that the start merge doesn't effect our decision.
    m_shouldMergeEnd = shouldMergeEnd(selectionEndWasEndOfParagraph);
    
    if (shouldMergeStart(selectionStartWasStartOfParagraph, fragment.hasInterchangeNewlineAtStart(), shouldHandleMailBlockquote)) {
        VisiblePosition startOfParagraphToMove = positionAtStartOfInsertedContent();
        VisiblePosition destination = startOfParagraphToMove.previous();
        // We need to handle the case where we need to merge the end
        // but our destination node is inside an inline that is the last in the block.
        // We insert a placeholder before the newly inserted content to avoid being merged into the inline.
        Node* destinationNode = destination.deepEquivalent().deprecatedNode();
        if (m_shouldMergeEnd && destinationNode != enclosingInline(destinationNode) && enclosingInline(destinationNode)->nextSibling())
            insertNodeBefore(HTMLBRElement::create(document()), *refNode);
        
        // Merging the first paragraph of inserted content with the content that came
        // before the selection that was pasted into would also move content after 
        // the selection that was pasted into if: only one paragraph was being pasted, 
        // and it was not wrapped in a block, the selection that was pasted into ended 
        // at the end of a block and the next paragraph didn't start at the start of a block.
        // Insert a line break just after the inserted content to separate it from what 
        // comes after and prevent that from happening.
        VisiblePosition endOfInsertedContent = positionAtEndOfInsertedContent();
        if (startOfParagraph(endOfInsertedContent) == startOfParagraphToMove) {
            insertNodeAt(HTMLBRElement::create(document()), endOfInsertedContent.deepEquivalent());
            // Mutation events (bug 22634) triggered by inserting the <br> might have removed the content we're about to move
            if (!startOfParagraphToMove.deepEquivalent().anchorNode()->isConnected())
                return;
        }

        // FIXME: Maintain positions for the start and end of inserted content instead of keeping nodes.  The nodes are
        // only ever used to create positions where inserted content starts/ends.
        moveParagraph(startOfParagraphToMove, endOfParagraph(startOfParagraphToMove), destination);
        m_startOfInsertedContent = endingSelection().visibleStart().deepEquivalent().downstream();
        if (m_endOfInsertedContent.isOrphan())
            m_endOfInsertedContent = endingSelection().visibleEnd().deepEquivalent().upstream();
    }

    Position lastPositionToSelect;
    if (fragment.hasInterchangeNewlineAtEnd()) {
        VisiblePosition endOfInsertedContent = positionAtEndOfInsertedContent();
        VisiblePosition next = endOfInsertedContent.next(CannotCrossEditingBoundary);

        if (selectionEndWasEndOfParagraph || !isEndOfParagraph(endOfInsertedContent) || next.isNull()) {
            if (!isStartOfParagraph(endOfInsertedContent)) {
                setEndingSelection(endOfInsertedContent);
                Node* enclosingNode = enclosingBlock(endOfInsertedContent.deepEquivalent().deprecatedNode());
                if (isListItem(enclosingNode)) {
                    auto newListItem = HTMLLIElement::create(document());
                    insertNodeAfter(newListItem.copyRef(), *enclosingNode);
                    setEndingSelection(VisiblePosition(firstPositionInNode(newListItem.ptr())));
                } else {
                    // Use a default paragraph element (a plain div) for the empty paragraph, using the last paragraph
                    // block's style seems to annoy users.
                    insertParagraphSeparator(true, !shouldHandleMailBlockquote && highestEnclosingNodeOfType(endOfInsertedContent.deepEquivalent(),
                        isMailBlockquote, CannotCrossEditingBoundary, insertedNodes.firstNodeInserted()->parentNode()));
                }

                // Select up to the paragraph separator that was added.
                lastPositionToSelect = endingSelection().visibleStart().deepEquivalent();
                updateNodesInserted(lastPositionToSelect.deprecatedNode());
            }
        } else {
            // Select up to the beginning of the next paragraph.
            lastPositionToSelect = next.deepEquivalent().downstream();
        }
        
    } else
        mergeEndIfNeeded();

    if (Node* mailBlockquote = enclosingNodeOfType(positionAtStartOfInsertedContent().deepEquivalent(), isMailPasteAsQuotationNode))
        removeNodeAttribute(downcast<Element>(*mailBlockquote), classAttr);

    if (shouldPerformSmartReplace())
        addSpacesForSmartReplace();

    if (!isInsertingIntoList && hasBlankLinesBetweenParagraphs && shouldPerformSmartParagraphReplace())
        addNewLinesForSmartReplace();

    // If we are dealing with a fragment created from plain text
    // no style matching is necessary.
    if (plainTextFragment)
        m_matchStyle = false;
        
    completeHTMLReplacement(lastPositionToSelect);
}

String ReplaceSelectionCommand::inputEventData() const
{
    if (isEditingTextAreaOrTextInput())
        return m_documentFragment->textContent();

    return CompositeEditCommand::inputEventData();
}

RefPtr<DataTransfer> ReplaceSelectionCommand::inputEventDataTransfer() const
{
    if (isEditingTextAreaOrTextInput())
        return CompositeEditCommand::inputEventDataTransfer();

    return DataTransfer::createForInputEvent(m_documentFragmentPlainText, m_documentFragmentHTMLMarkup);
}

bool ReplaceSelectionCommand::shouldRemoveEndBR(Node* endBR, const VisiblePosition& originalVisPosBeforeEndBR)
{
    if (!endBR || !endBR->isConnected())
        return false;

    VisiblePosition visiblePos(positionBeforeNode(endBR));
    
    // Don't remove the br if nothing was inserted.
    if (visiblePos.previous() == originalVisPosBeforeEndBR)
        return false;
    
    // Remove the br if it is collapsed away and so is unnecessary.
    if (!document().inNoQuirksMode() && isEndOfBlock(visiblePos) && !isStartOfParagraph(visiblePos))
        return true;
        
    // A br that was originally holding a line open should be displaced by inserted content or turned into a line break.
    // A br that was originally acting as a line break should still be acting as a line break, not as a placeholder.
    return isStartOfParagraph(visiblePos) && isEndOfParagraph(visiblePos);
}

bool ReplaceSelectionCommand::shouldPerformSmartReplace() const
{
    if (!m_smartReplace)
        return false;

    Element* textControl = enclosingTextFormControl(positionAtStartOfInsertedContent().deepEquivalent());
    if (is<HTMLInputElement>(textControl) && downcast<HTMLInputElement>(*textControl).isPasswordField())
        return false; // Disable smart replace for password fields.

    return true;
}
    
bool ReplaceSelectionCommand::shouldPerformSmartParagraphReplace() const
{
    if (!m_smartReplace)
        return false;

    if (!document().editingBehavior().shouldSmartInsertDeleteParagraphs())
        return false;

    return true;
}

static bool isCharacterSmartReplaceExemptConsideringNonBreakingSpace(UChar32 character, bool previousCharacter)
{
    return isCharacterSmartReplaceExempt(character == noBreakSpace ? ' ' : character, previousCharacter);
}

void ReplaceSelectionCommand::addNewLinesForSmartReplace()
{
    VisiblePosition startOfInsertedContent = positionAtStartOfInsertedContent();
    VisiblePosition endOfInsertedContent = positionAtEndOfInsertedContent();

    bool isPastedContentEntireParagraphs = isStartOfParagraph(startOfInsertedContent) && isEndOfParagraph(endOfInsertedContent);

    // If we aren't pasting a paragraph, no need to attempt to insert newlines.
    if (!isPastedContentEntireParagraphs)
        return;

    bool reachedBoundaryStart = false;
    bool reachedBoundaryEnd = false;
    VisiblePosition positionBeforeStart = startOfInsertedContent.previous(CannotCrossEditingBoundary, &reachedBoundaryStart);
    VisiblePosition positionAfterEnd = endOfInsertedContent.next(CannotCrossEditingBoundary, &reachedBoundaryEnd);

    if (!reachedBoundaryStart && !reachedBoundaryEnd) {
        if (!isBlankParagraph(positionBeforeStart) && !isBlankParagraph(startOfInsertedContent) && isEndOfLine(positionBeforeStart) && !isEndOfEditableOrNonEditableContent(positionAfterEnd) && !isEndOfEditableOrNonEditableContent(endOfInsertedContent)) {
            setEndingSelection(startOfInsertedContent);
            insertParagraphSeparator();
            auto newStart = endingSelection().visibleStart().previous(CannotCrossEditingBoundary, &reachedBoundaryStart);
            if (!reachedBoundaryStart)
                m_startOfInsertedContent = newStart.deepEquivalent();
        }
    }

    reachedBoundaryStart = false;
    reachedBoundaryEnd = false;
    positionAfterEnd = endOfInsertedContent.next(CannotCrossEditingBoundary, &reachedBoundaryEnd);
    positionBeforeStart = startOfInsertedContent.previous(CannotCrossEditingBoundary, &reachedBoundaryStart);

    if (!reachedBoundaryEnd && !reachedBoundaryStart) {
        if (!isBlankParagraph(positionAfterEnd) && !isBlankParagraph(endOfInsertedContent) && isStartOfLine(positionAfterEnd) && !isEndOfLine(positionAfterEnd) && !isEndOfEditableOrNonEditableContent(positionAfterEnd)) {
            setEndingSelection(endOfInsertedContent);
            insertParagraphSeparator();
            m_endOfInsertedContent = endingSelection().start();
        }
    }
}

void ReplaceSelectionCommand::addSpacesForSmartReplace()
{
    VisiblePosition startOfInsertedContent = positionAtStartOfInsertedContent();
    VisiblePosition endOfInsertedContent = positionAtEndOfInsertedContent();

    Position endUpstream = endOfInsertedContent.deepEquivalent().upstream();
    Node* endNode = endUpstream.computeNodeBeforePosition();
    int endOffset = is<Text>(endNode) ? downcast<Text>(*endNode).length() : 0;
    if (endUpstream.anchorType() == Position::PositionIsOffsetInAnchor) {
        endNode = endUpstream.containerNode();
        endOffset = endUpstream.offsetInContainerNode();
    }

    bool needsTrailingSpace = !isEndOfParagraph(endOfInsertedContent) && !isStartOfParagraph(endOfInsertedContent) && !isCharacterSmartReplaceExemptConsideringNonBreakingSpace(endOfInsertedContent.characterAfter(), false);
    if (needsTrailingSpace && endNode) {
        bool collapseWhiteSpace = !endNode->renderer() || endNode->renderer()->style().collapseWhiteSpace();
        if (is<Text>(*endNode)) {
            insertTextIntoNode(downcast<Text>(*endNode), endOffset, collapseWhiteSpace ? nonBreakingSpaceString() : " ");
            if (m_endOfInsertedContent.containerNode() == endNode)
                m_endOfInsertedContent.moveToOffset(m_endOfInsertedContent.offsetInContainerNode() + 1);
        } else {
            auto node = document().createEditingTextNode(collapseWhiteSpace ? nonBreakingSpaceString() : " ");
            insertNodeAfter(node.copyRef(), *endNode);
            updateNodesInserted(node.ptr());
        }
    }

    document().updateLayout();

    Position startDownstream = startOfInsertedContent.deepEquivalent().downstream();
    Node* startNode = startDownstream.computeNodeAfterPosition();
    unsigned startOffset = 0;
    if (startDownstream.anchorType() == Position::PositionIsOffsetInAnchor) {
        startNode = startDownstream.containerNode();
        startOffset = startDownstream.offsetInContainerNode();
    }

    bool needsLeadingSpace = !isStartOfParagraph(startOfInsertedContent) && !isEndOfParagraph(startOfInsertedContent) && !isCharacterSmartReplaceExemptConsideringNonBreakingSpace(startOfInsertedContent.previous().characterAfter(), true);
    if (needsLeadingSpace && startNode) {
        bool collapseWhiteSpace = !startNode->renderer() || startNode->renderer()->style().collapseWhiteSpace();
        if (is<Text>(*startNode)) {
            insertTextIntoNode(downcast<Text>(*startNode), startOffset, collapseWhiteSpace ? nonBreakingSpaceString() : " ");
            if (m_endOfInsertedContent.containerNode() == startNode && m_endOfInsertedContent.offsetInContainerNode())
                m_endOfInsertedContent.moveToOffset(m_endOfInsertedContent.offsetInContainerNode() + 1);
        } else {
            auto node = document().createEditingTextNode(collapseWhiteSpace ? nonBreakingSpaceString() : " ");
            auto* nodePtr = node.ptr();
            // Don't updateNodesInserted. Doing so would set m_endOfInsertedContent to be the node containing the leading space,
            // but m_endOfInsertedContent is supposed to mark the end of pasted content.
            insertNodeBefore(WTFMove(node), *startNode);
            m_startOfInsertedContent = firstPositionInNode(nodePtr);
        }
    }
}

void ReplaceSelectionCommand::completeHTMLReplacement(const Position &lastPositionToSelect)
{
    Position start = positionAtStartOfInsertedContent().deepEquivalent();
    Position end = positionAtEndOfInsertedContent().deepEquivalent();

    // Mutation events may have deleted start or end
    if (start.isNotNull() && !start.isOrphan() && end.isNotNull() && !end.isOrphan()) {
        // FIXME (11475): Remove this and require that the creator of the fragment to use nbsps.
        rebalanceWhitespaceAt(start);
        rebalanceWhitespaceAt(end);

        if (m_matchStyle) {
            ASSERT(m_insertionStyle);
            applyStyle(m_insertionStyle.get(), start, end);
        }

        if (lastPositionToSelect.isNotNull())
            end = lastPositionToSelect;

        mergeTextNodesAroundPosition(start, end);
        mergeTextNodesAroundPosition(end, start);
    } else if (lastPositionToSelect.isNotNull())
        start = end = lastPositionToSelect;
    else
        return;

    if (AXObjectCache::accessibilityEnabled() && editingAction() == EditAction::Paste)
        m_visibleSelectionForInsertedText = VisibleSelection(start, end);

    if (m_selectReplacement)
        setEndingSelection(VisibleSelection(start, end, SEL_DEFAULT_AFFINITY, endingSelection().isDirectional()));
    else
        setEndingSelection(VisibleSelection(end, SEL_DEFAULT_AFFINITY, endingSelection().isDirectional()));
}

void ReplaceSelectionCommand::mergeTextNodesAroundPosition(Position& position, Position& positionOnlyToBeUpdated)
{
    bool positionIsOffsetInAnchor = position.anchorType() == Position::PositionIsOffsetInAnchor;
    bool positionOnlyToBeUpdatedIsOffsetInAnchor = positionOnlyToBeUpdated.anchorType() == Position::PositionIsOffsetInAnchor;
    RefPtr<Text> text;
    if (positionIsOffsetInAnchor && is<Text>(position.containerNode()))
        text = downcast<Text>(position.containerNode());
    else {
        Node* before = position.computeNodeBeforePosition();
        if (is<Text>(before))
            text = downcast<Text>(before);
        else {
            Node* after = position.computeNodeAfterPosition();
            if (is<Text>(after))
                text = downcast<Text>(after);
        }
    }
    if (!text)
        return;

    if (is<Text>(text->previousSibling())) {
        Ref<Text> previous(downcast<Text>(*text->previousSibling()));
        insertTextIntoNode(*text, 0, previous->data());

        if (positionIsOffsetInAnchor)
            position.moveToOffset(previous->length() + position.offsetInContainerNode());
        else
            updatePositionForNodeRemoval(position, previous.get());

        if (positionOnlyToBeUpdatedIsOffsetInAnchor) {
            if (positionOnlyToBeUpdated.containerNode() == text)
                positionOnlyToBeUpdated.moveToOffset(previous->length() + positionOnlyToBeUpdated.offsetInContainerNode());
            else if (positionOnlyToBeUpdated.containerNode() == previous.ptr())
                positionOnlyToBeUpdated.moveToPosition(text.get(), positionOnlyToBeUpdated.offsetInContainerNode());
        } else
            updatePositionForNodeRemoval(positionOnlyToBeUpdated, previous.get());

        removeNode(previous);
    }
    if (is<Text>(text->nextSibling())) {
        Ref<Text> next(downcast<Text>(*text->nextSibling()));
        unsigned originalLength = text->length();
        insertTextIntoNode(*text, originalLength, next->data());

        if (!positionIsOffsetInAnchor)
            updatePositionForNodeRemoval(position, next.get());

        if (positionOnlyToBeUpdatedIsOffsetInAnchor && positionOnlyToBeUpdated.containerNode() == next.ptr())
            positionOnlyToBeUpdated.moveToPosition(text.get(), originalLength + positionOnlyToBeUpdated.offsetInContainerNode());
        else
            updatePositionForNodeRemoval(positionOnlyToBeUpdated, next.get());

        removeNode(next);
    }
}

static HTMLElement* singleChildList(HTMLElement& element)
{
    if (!element.hasOneChild())
        return nullptr;

    auto& child = *element.firstChild();
    return isListHTMLElement(&child) ? &downcast<HTMLElement>(child) : nullptr;
}

static HTMLElement& deepestSingleChildList(HTMLElement& topLevelList)
{
    auto* list = &topLevelList;
    while (auto* childList = singleChildList(*list))
        list = childList;
    return *list;
}

// If the user is inserting a list into an existing list, instead of nesting the list,
// we put the list items into the existing list.
Node* ReplaceSelectionCommand::insertAsListItems(HTMLElement& passedListElement, Node* insertionBlock, const Position& insertPos, InsertedNodes& insertedNodes)
{
    Ref<HTMLElement> listElement = deepestSingleChildList(passedListElement);

    bool isStart = isStartOfParagraph(insertPos);
    bool isEnd = isEndOfParagraph(insertPos);
    bool isMiddle = !isStart && !isEnd;
    Node* lastNode = insertionBlock;

    // If we're in the middle of a list item, we should split it into two separate
    // list items and insert these nodes between them.
    if (isMiddle) {
        int textNodeOffset = insertPos.offsetInContainerNode();
        if (is<Text>(*insertPos.deprecatedNode()) && textNodeOffset > 0)
            splitTextNode(downcast<Text>(*insertPos.deprecatedNode()), textNodeOffset);
        splitTreeToNode(*insertPos.deprecatedNode(), *lastNode, true);
    }

    while (RefPtr<Node> listItem = listElement->firstChild()) {
        listElement->removeChild(*listItem);
        if (isStart || isMiddle) {
            insertNodeBefore(*listItem, *lastNode);
            insertedNodes.respondToNodeInsertion(listItem.get());
        } else if (isEnd) {
            insertNodeAfter(*listItem, *lastNode);
            insertedNodes.respondToNodeInsertion(listItem.get());
            lastNode = listItem.get();
        } else
            ASSERT_NOT_REACHED();
    }
    if ((isStart || isMiddle) && lastNode->previousSibling())
        lastNode = lastNode->previousSibling();
    return lastNode;
}

void ReplaceSelectionCommand::updateNodesInserted(Node *node)
{
    if (!node)
        return;

    if (m_startOfInsertedContent.isNull())
        m_startOfInsertedContent = firstPositionInOrBeforeNode(node);

    m_endOfInsertedContent = lastPositionInOrAfterNode(node->lastDescendant());
}

ReplacementFragment* ReplaceSelectionCommand::ensureReplacementFragment()
{
    if (!m_replacementFragment)
        m_replacementFragment = makeUnique<ReplacementFragment>(m_documentFragment.get(), endingSelection());
    return m_replacementFragment.get();
}

// During simple pastes, where we're just pasting a text node into a run of text, we insert the text node
// directly into the text node that holds the selection.  This is much faster than the generalized code in
// ReplaceSelectionCommand, and works around <https://bugs.webkit.org/show_bug.cgi?id=6148> since we don't 
// split text nodes.
bool ReplaceSelectionCommand::performTrivialReplace(const ReplacementFragment& fragment)
{
    if (!is<Text>(fragment.firstChild()) || fragment.firstChild() != fragment.lastChild())
        return false;

    // FIXME: Would be nice to handle smart replace in the fast path.
    if (m_smartReplace || fragment.hasInterchangeNewlineAtStart() || fragment.hasInterchangeNewlineAtEnd())
        return false;

    // e.g. when "bar" is inserted after "foo" in <div><u>foo</u></div>, "bar" should not be underlined.
    if (nodeToSplitToAvoidPastingIntoInlineNodesWithStyle(endingSelection().start()))
        return false;

    RefPtr<Node> nodeAfterInsertionPos = endingSelection().end().downstream().anchorNode();
    Text& textNode = downcast<Text>(*fragment.firstChild());
    // Our fragment creation code handles tabs, spaces, and newlines, so we don't have to worry about those here.

    Position start = endingSelection().start();
    Position end = replaceSelectedTextInNode(textNode.data());
    if (end.isNull())
        return false;

    if (nodeAfterInsertionPos && nodeAfterInsertionPos->parentNode() && nodeAfterInsertionPos->hasTagName(brTag)
        && shouldRemoveEndBR(nodeAfterInsertionPos.get(), positionBeforeNode(nodeAfterInsertionPos.get())))
        removeNodeAndPruneAncestors(*nodeAfterInsertionPos);

    VisibleSelection selectionAfterReplace(m_selectReplacement ? start : end, end);

    if (AXObjectCache::accessibilityEnabled() && editingAction() == EditAction::Paste)
        m_visibleSelectionForInsertedText = VisibleSelection(start, end);

    setEndingSelection(selectionAfterReplace);

    return true;
}

RefPtr<Range> ReplaceSelectionCommand::insertedContentRange() const
{
    if (auto document = makeRefPtr(m_startOfInsertedContent.document()))
        return Range::create(*document, m_startOfInsertedContent, m_endOfInsertedContent);

    return nullptr;
}

} // namespace WebCore
