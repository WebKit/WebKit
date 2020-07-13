/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009, 2010, 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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
#include "markup.h"

#include "ArchiveResource.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSValue.h"
#include "CSSValueKeywords.h"
#include "CacheStorageProvider.h"
#include "ChildListMutationScope.h"
#include "Comment.h"
#include "ComposedTreeIterator.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "ElementIterator.h"
#include "EmptyClients.h"
#include "File.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLAttachmentElement.h"
#include "HTMLBRElement.h"
#include "HTMLBodyElement.h"
#include "HTMLDivElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "LibWebRTCProvider.h"
#include "MarkupAccumulator.h"
#include "NodeList.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "PasteboardItemInfo.h"
#include "Range.h"
#include "RenderBlock.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "SocketProvider.h"
#include "StyleProperties.h"
#include "TextIterator.h"
#include "TextManipulationController.h"
#include "VisibleSelection.h"
#include "VisibleUnits.h"
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/URLParser.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

static bool propertyMissingOrEqualToNone(const StyleProperties*, CSSPropertyID);

class AttributeChange {
public:
    AttributeChange()
        : m_name(nullAtom(), nullAtom(), nullAtom())
    {
    }

    AttributeChange(Element* element, const QualifiedName& name, const String& value)
        : m_element(element), m_name(name), m_value(value)
    {
    }

    void apply()
    {
        m_element->setAttribute(m_name, m_value);
    }

private:
    RefPtr<Element> m_element;
    QualifiedName m_name;
    String m_value;
};

static void completeURLs(DocumentFragment* fragment, const String& baseURL)
{
    Vector<AttributeChange> changes;

    URL parsedBaseURL({ }, baseURL);

    for (auto& element : descendantsOfType<Element>(*fragment)) {
        if (!element.hasAttributes())
            continue;
        for (const Attribute& attribute : element.attributesIterator()) {
            if (element.attributeContainsURL(attribute) && !attribute.value().isEmpty())
                changes.append(AttributeChange(&element, attribute.name(), element.completeURLsInAttributeValue(parsedBaseURL, attribute)));
        }
    }

    for (auto& change : changes)
        change.apply();
}

void replaceSubresourceURLs(Ref<DocumentFragment>&& fragment, HashMap<AtomString, AtomString>&& replacementMap)
{
    Vector<AttributeChange> changes;
    for (auto& element : descendantsOfType<Element>(fragment)) {
        if (!element.hasAttributes())
            continue;
        for (const Attribute& attribute : element.attributesIterator()) {
            // FIXME: This won't work for srcset.
            if (element.attributeContainsURL(attribute) && !attribute.value().isEmpty()) {
                auto replacement = replacementMap.get(attribute.value());
                if (!replacement.isNull())
                    changes.append({ &element, attribute.name(), replacement });
            }
        }
    }
    for (auto& change : changes)
        change.apply();
}

struct ElementAttribute {
    Ref<Element> element;
    QualifiedName attributeName;
};

void removeSubresourceURLAttributes(Ref<DocumentFragment>&& fragment, WTF::Function<bool(const URL&)> shouldRemoveURL)
{
    Vector<ElementAttribute> attributesToRemove;
    for (auto& element : descendantsOfType<Element>(fragment)) {
        if (!element.hasAttributes())
            continue;
        for (const Attribute& attribute : element.attributesIterator()) {
            // FIXME: This won't work for srcset.
            if (element.attributeContainsURL(attribute) && !attribute.value().isEmpty()) {
                URL url({ }, attribute.value());
                if (shouldRemoveURL(url))
                    attributesToRemove.append({ element, attribute.name() });
            }
        }
    }
    for (auto& item : attributesToRemove)
        item.element->removeAttribute(item.attributeName);
}

std::unique_ptr<Page> createPageForSanitizingWebContent()
{
    auto pageConfiguration = pageConfigurationWithEmptyClients(PAL::SessionID::defaultSessionID());
    
    auto page = makeUnique<Page>(WTFMove(pageConfiguration));
    page->settings().setMediaEnabled(false);
    page->settings().setScriptEnabled(false);
    page->settings().setParserScriptingFlagPolicy(SettingsBase::ParserScriptingFlagPolicy::Enabled);
    page->settings().setPluginsEnabled(false);
    page->settings().setAcceleratedCompositingEnabled(false);

    Frame& frame = page->mainFrame();
    frame.setView(FrameView::create(frame, IntSize { 800, 600 }));
    frame.init();

    FrameLoader& loader = frame.loader();
    static char markup[] = "<!DOCTYPE html><html><body></body></html>";
    ASSERT(loader.activeDocumentLoader());
    auto& writer = loader.activeDocumentLoader()->writer();
    writer.setMIMEType("text/html");
    writer.begin();
    writer.insertDataSynchronously(String(markup));
    writer.end();
    RELEASE_ASSERT(page->mainFrame().document()->body());

    return page;
}

String sanitizeMarkup(const String& rawHTML, MSOListQuirks msoListQuirks, Optional<WTF::Function<void(DocumentFragment&)>> fragmentSanitizer)
{
    auto page = createPageForSanitizingWebContent();
    Document* stagingDocument = page->mainFrame().document();
    ASSERT(stagingDocument);

    auto fragment = createFragmentFromMarkup(*stagingDocument, rawHTML, emptyString(), DisallowScriptingAndPluginContent);

    if (fragmentSanitizer)
        (*fragmentSanitizer)(fragment);

    return sanitizedMarkupForFragmentInDocument(WTFMove(fragment), *stagingDocument, msoListQuirks, rawHTML);
}

enum class MSOListMode { Preserve, DoNotPreserve };
class StyledMarkupAccumulator final : public MarkupAccumulator {
public:
    enum RangeFullySelectsNode { DoesFullySelectNode, DoesNotFullySelectNode };

    StyledMarkupAccumulator(const Position& start, const Position& end, Vector<Node*>* nodes, ResolveURLs, SerializeComposedTree,
        AnnotateForInterchange, StandardFontFamilySerializationMode, MSOListMode, bool needsPositionStyleConversion, Node* highestNodeToBeSerialized = nullptr);

    Node* serializeNodes(const Position& start, const Position& end);
    void wrapWithNode(Node&, bool convertBlocksToInlines = false, RangeFullySelectsNode = DoesFullySelectNode);
    void wrapWithStyleNode(StyleProperties*, Document&, bool isBlock = false);
    String takeResults();
    
    bool needRelativeStyleWrapper() const { return m_needRelativeStyleWrapper; }
    bool needClearingDiv() const { return m_needClearingDiv; }

    using MarkupAccumulator::append;

    ContainerNode* parentNode(Node& node)
    {
        if (UNLIKELY(m_useComposedTree))
            return node.parentInComposedTree();
        return node.parentOrShadowHostNode();
    }

    void prependMetaCharsetUTF8TagIfNonASCIICharactersArePresent()
    {
        if (!isAllASCII())
            m_reversedPrecedingMarkup.append("<meta charset=\"UTF-8\">"_s);
    }

private:
    bool isAllASCII() const;
    void appendStyleNodeOpenTag(StringBuilder&, StyleProperties*, Document&, bool isBlock = false);
    const String& styleNodeCloseTag(bool isBlock = false);

    String renderedTextRespectingRange(const Text&);
    String textContentRespectingRange(const Text&);

    bool shouldPreserveMSOListStyleForElement(const Element&);

    void appendStartTag(StringBuilder& out, const Element&, bool addDisplayInline, RangeFullySelectsNode);
    void appendEndTag(StringBuilder& out, const Element&) override;
    void appendCustomAttributes(StringBuilder&, const Element&, Namespaces*) override;

    void appendText(StringBuilder& out, const Text&) override;
    void appendStartTag(StringBuilder& out, const Element& element, Namespaces*) override
    {
        appendStartTag(out, element, false, DoesFullySelectNode);
    }

    Node* firstChild(Node& node)
    {
        if (UNLIKELY(m_useComposedTree))
            return firstChildInComposedTreeIgnoringUserAgentShadow(node);
        return node.firstChild();
    }

    Node* nextSibling(Node& node)
    {
        if (UNLIKELY(m_useComposedTree))
            return nextSiblingInComposedTreeIgnoringUserAgentShadow(node);
        return node.nextSibling();
    }
    
    Node* nextSkippingChildren(Node& node)
    {
        if (UNLIKELY(m_useComposedTree))
            return nextSkippingChildrenInComposedTreeIgnoringUserAgentShadow(node);
        return NodeTraversal::nextSkippingChildren(node);
    }

    bool hasChildNodes(Node& node)
    {
        if (UNLIKELY(m_useComposedTree))
            return firstChildInComposedTreeIgnoringUserAgentShadow(node);
        return node.hasChildNodes();
    }

    bool isDescendantOf(Node& node, Node& possibleAncestor)
    {
        if (UNLIKELY(m_useComposedTree))
            return node.isDescendantOrShadowDescendantOf(&possibleAncestor);
        return node.isDescendantOf(&possibleAncestor);
    }

    enum class NodeTraversalMode { EmitString, DoNotEmitString };
    Node* traverseNodesForSerialization(Node* startNode, Node* pastEnd, NodeTraversalMode);

    bool appendNodeToPreserveMSOList(Node&);

    bool shouldAnnotate()
    {
        return m_annotate == AnnotateForInterchange::Yes;
    }

    bool shouldApplyWrappingStyle(const Node& node) const
    {
        return m_highestNodeToBeSerialized && m_highestNodeToBeSerialized->parentNode() == node.parentNode() && m_wrappingStyle && m_wrappingStyle->style();
    }

    Position m_start;
    Position m_end;
    Vector<String> m_reversedPrecedingMarkup;
    const AnnotateForInterchange m_annotate;
    RefPtr<Node> m_highestNodeToBeSerialized;
    RefPtr<EditingStyle> m_wrappingStyle;
    bool m_useComposedTree;
    bool m_needsPositionStyleConversion;
    StandardFontFamilySerializationMode m_standardFontFamilySerializationMode;
    bool m_shouldPreserveMSOList;
    bool m_needRelativeStyleWrapper { false };
    bool m_needClearingDiv { false };
    bool m_inMSOList { false };
};

inline StyledMarkupAccumulator::StyledMarkupAccumulator(const Position& start, const Position& end, Vector<Node*>* nodes, ResolveURLs urlsToResolve, SerializeComposedTree serializeComposedTree,
    AnnotateForInterchange annotate, StandardFontFamilySerializationMode standardFontFamilySerializationMode, MSOListMode msoListMode, bool needsPositionStyleConversion, Node* highestNodeToBeSerialized)
    : MarkupAccumulator(nodes, urlsToResolve)
    , m_start(start)
    , m_end(end)
    , m_annotate(annotate)
    , m_highestNodeToBeSerialized(highestNodeToBeSerialized)
    , m_useComposedTree(serializeComposedTree == SerializeComposedTree::Yes)
    , m_needsPositionStyleConversion(needsPositionStyleConversion)
    , m_standardFontFamilySerializationMode(standardFontFamilySerializationMode)
    , m_shouldPreserveMSOList(msoListMode == MSOListMode::Preserve)
{
}

void StyledMarkupAccumulator::wrapWithNode(Node& node, bool convertBlocksToInlines, RangeFullySelectsNode rangeFullySelectsNode)
{
    StringBuilder markup;
    if (is<Element>(node))
        appendStartTag(markup, downcast<Element>(node), convertBlocksToInlines && isBlock(&node), rangeFullySelectsNode);
    else
        appendNonElementNode(markup, node, nullptr);
    m_reversedPrecedingMarkup.append(markup.toString());
    endAppendingNode(node);
    if (m_nodes)
        m_nodes->append(&node);
}

void StyledMarkupAccumulator::wrapWithStyleNode(StyleProperties* style, Document& document, bool isBlock)
{
    StringBuilder openTag;
    appendStyleNodeOpenTag(openTag, style, document, isBlock);
    m_reversedPrecedingMarkup.append(openTag.toString());
    append(styleNodeCloseTag(isBlock));
}

void StyledMarkupAccumulator::appendStyleNodeOpenTag(StringBuilder& out, StyleProperties* style, Document& document, bool isBlock)
{
    // wrappingStyleForSerialization should have removed -webkit-text-decorations-in-effect
    ASSERT(propertyMissingOrEqualToNone(style, CSSPropertyWebkitTextDecorationsInEffect));
    if (isBlock)
        out.appendLiteral("<div style=\"");
    else
        out.appendLiteral("<span style=\"");
    appendAttributeValue(out, style->asText(), document.isHTMLDocument());
    out.appendLiteral("\">");
}

const String& StyledMarkupAccumulator::styleNodeCloseTag(bool isBlock)
{
    static NeverDestroyed<const String> divClose(MAKE_STATIC_STRING_IMPL("</div>"));
    static NeverDestroyed<const String> styleSpanClose(MAKE_STATIC_STRING_IMPL("</span>"));
    return isBlock ? divClose : styleSpanClose;
}

bool StyledMarkupAccumulator::isAllASCII() const
{
    for (auto& preceding : m_reversedPrecedingMarkup) {
        if (!preceding.isAllASCII())
            return false;
    }
    return MarkupAccumulator::isAllASCII();
}

String StyledMarkupAccumulator::takeResults()
{
    CheckedUint32 length = this->length();
    for (auto& string : m_reversedPrecedingMarkup)
        length += string.length();
    StringBuilder result;
    result.reserveCapacity(length.unsafeGet());
    for (auto& string : makeReversedRange(m_reversedPrecedingMarkup))
        result.append(string);
    result.append(takeMarkup());
    // Remove '\0' characters because they are not visibly rendered to the user.
    return result.toString().replaceWithLiteral('\0', "");
}

void StyledMarkupAccumulator::appendText(StringBuilder& out, const Text& text)
{    
    const bool parentIsTextarea = is<HTMLTextAreaElement>(text.parentElement());
    const bool wrappingSpan = shouldApplyWrappingStyle(text) && !parentIsTextarea;
    if (wrappingSpan) {
        auto wrappingStyle = m_wrappingStyle->copy();
        // FIXME: <rdar://problem/5371536> Style rules that match pasted content can change it's appearance
        // Make sure spans are inline style in paste side e.g. span { display: block }.
        wrappingStyle->forceInline();
        // FIXME: Should this be included in forceInline?
        wrappingStyle->style()->setProperty(CSSPropertyFloat, CSSValueNone);

        appendStyleNodeOpenTag(out, wrappingStyle->style(), text.document());
    }

    if (!shouldAnnotate() || parentIsTextarea) {
        auto content = textContentRespectingRange(text);
        appendCharactersReplacingEntities(out, content, 0, content.length(), entityMaskForText(text));
    } else {
        const bool useRenderedText = !enclosingElementWithTag(firstPositionInNode(const_cast<Text*>(&text)), selectTag);
        String content = useRenderedText ? renderedTextRespectingRange(text) : textContentRespectingRange(text);
        StringBuilder buffer;
        appendCharactersReplacingEntities(buffer, content, 0, content.length(), EntityMaskInPCDATA);
        out.append(convertHTMLTextToInterchangeFormat(buffer.toString(), &text));
    }

    if (wrappingSpan)
        out.append(styleNodeCloseTag());
}
    
String StyledMarkupAccumulator::renderedTextRespectingRange(const Text& text)
{
    TextIteratorBehavior behavior = TextIteratorDefaultBehavior;
    Position start = &text == m_start.containerNode() ? m_start : firstPositionInNode(const_cast<Text*>(&text));
    Position end;
    if (&text == m_end.containerNode())
        end = m_end;
    else {
        end = lastPositionInNode(const_cast<Text*>(&text));
        if (!m_end.isNull())
            behavior = TextIteratorBehavesAsIfNodesFollowing;
    }

    auto startBoundary = makeBoundaryPoint(start);
    auto endBoundary = makeBoundaryPoint(end);
    if (!startBoundary || !endBoundary)
        return emptyString();

    return plainText({ WTFMove(*startBoundary), WTFMove(*endBoundary) }, behavior);
}

String StyledMarkupAccumulator::textContentRespectingRange(const Text& text)
{
    if (m_start.isNull() && m_end.isNull())
        return text.data();

    unsigned start = 0;
    unsigned end = std::numeric_limits<unsigned>::max();
    if (&text == m_start.containerNode())
        start = m_start.offsetInContainerNode();
    if (&text == m_end.containerNode())
        end = m_end.offsetInContainerNode();
    ASSERT(start < end);
    return text.data().substring(start, end - start);
}

void StyledMarkupAccumulator::appendCustomAttributes(StringBuilder& out, const Element& element, Namespaces* namespaces)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    if (!RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled())
        return;
    
    if (is<HTMLAttachmentElement>(element)) {
        auto& attachment = downcast<HTMLAttachmentElement>(element);
        appendAttribute(out, element, { webkitattachmentidAttr, attachment.uniqueIdentifier() }, namespaces);
        if (auto* file = attachment.file()) {
            // These attributes are only intended for File deserialization, and are removed from the generated attachment
            // element after we've deserialized and set its backing File, in restoreAttachmentElementsInFragment.
            appendAttribute(out, element, { webkitattachmentpathAttr, file->path() }, namespaces);
            appendAttribute(out, element, { webkitattachmentbloburlAttr, file->url().string() }, namespaces);
        }
    } else if (is<HTMLImageElement>(element)) {
        if (auto attachment = downcast<HTMLImageElement>(element).attachmentElement())
            appendAttribute(out, element, { webkitattachmentidAttr, attachment->uniqueIdentifier() }, namespaces);
    }
#else
    UNUSED_PARAM(out);
    UNUSED_PARAM(element);
    UNUSED_PARAM(namespaces);
#endif
}

bool StyledMarkupAccumulator::shouldPreserveMSOListStyleForElement(const Element& element)
{
    if (m_inMSOList)
        return true;
    if (m_shouldPreserveMSOList) {
        auto style = element.getAttribute(styleAttr);
        return style.startsWith("mso-list:") || style.contains(";mso-list:") || style.contains("\nmso-list:");
    }
    return false;
}

void StyledMarkupAccumulator::appendStartTag(StringBuilder& out, const Element& element, bool addDisplayInline, RangeFullySelectsNode rangeFullySelectsNode)
{
    const bool documentIsHTML = element.document().isHTMLDocument();
    const bool isSlotElement = is<HTMLSlotElement>(element);
    if (UNLIKELY(isSlotElement))
        out.append("<span");
    else
        appendOpenTag(out, element, nullptr);

    appendCustomAttributes(out, element, nullptr);

    const bool shouldAnnotateOrForceInline = element.isHTMLElement() && (shouldAnnotate() || addDisplayInline);
    bool shouldOverrideStyleAttr = (shouldAnnotateOrForceInline || shouldApplyWrappingStyle(element) || isSlotElement) && !shouldPreserveMSOListStyleForElement(element);
    if (element.hasAttributes()) {
        for (const Attribute& attribute : element.attributesIterator()) {
            // We'll handle the style attribute separately, below.
            if (attribute.name() == styleAttr && shouldOverrideStyleAttr)
                continue;
            if (element.isEventHandlerAttribute(attribute) || element.isJavaScriptURLAttribute(attribute))
                continue;
            appendAttribute(out, element, attribute, 0);
        }
    }

    if (shouldOverrideStyleAttr) {
        RefPtr<EditingStyle> newInlineStyle;

        if (shouldApplyWrappingStyle(element)) {
            newInlineStyle = m_wrappingStyle->copy();
            newInlineStyle->removePropertiesInElementDefaultStyle(*const_cast<Element*>(&element));
            newInlineStyle->removeStyleConflictingWithStyleOfNode(*const_cast<Element*>(&element));
        } else
            newInlineStyle = EditingStyle::create();

        if (isSlotElement)
            newInlineStyle->addDisplayContents();

        if (is<StyledElement>(element) && downcast<StyledElement>(element).inlineStyle())
            newInlineStyle->overrideWithStyle(*downcast<StyledElement>(element).inlineStyle());

        if (shouldAnnotateOrForceInline) {
            if (shouldAnnotate())
                newInlineStyle->mergeStyleFromRulesForSerialization(downcast<HTMLElement>(*const_cast<Element*>(&element)), m_standardFontFamilySerializationMode);

            if (addDisplayInline)
                newInlineStyle->forceInline();
            
            if (m_needsPositionStyleConversion) {
                m_needRelativeStyleWrapper |= newInlineStyle->convertPositionStyle();
                m_needClearingDiv |= newInlineStyle->isFloating();
            }

            // If the node is not fully selected by the range, then we don't want to keep styles that affect its relationship to the nodes around it
            // only the ones that affect it and the nodes within it.
            if (rangeFullySelectsNode == DoesNotFullySelectNode && newInlineStyle->style())
                newInlineStyle->style()->removeProperty(CSSPropertyFloat);
        }

        if (!newInlineStyle->isEmpty()) {
            out.appendLiteral(" style=\"");
            appendAttributeValue(out, newInlineStyle->style()->asText(), documentIsHTML);
            out.append('"');
        }
    }

    appendCloseTag(out, element);
}

void StyledMarkupAccumulator::appendEndTag(StringBuilder& out, const Element& element)
{
    if (UNLIKELY(is<HTMLSlotElement>(element)))
        out.append("</span>");
    else
        MarkupAccumulator::appendEndTag(out, element);
}

Node* StyledMarkupAccumulator::serializeNodes(const Position& start, const Position& end)
{
    ASSERT(comparePositions(start, end) <= 0);
    auto startNode = start.firstNode();
    Node* pastEnd = end.computeNodeAfterPosition();
    if (!pastEnd && end.containerNode())
        pastEnd = nextSkippingChildren(*end.containerNode());

    if (!m_highestNodeToBeSerialized) {
        Node* lastClosed = traverseNodesForSerialization(startNode.get(), pastEnd, NodeTraversalMode::DoNotEmitString);
        m_highestNodeToBeSerialized = lastClosed;
    }

    if (m_highestNodeToBeSerialized && m_highestNodeToBeSerialized->parentNode())
        m_wrappingStyle = EditingStyle::wrappingStyleForSerialization(*m_highestNodeToBeSerialized->parentNode(), shouldAnnotate(), m_standardFontFamilySerializationMode);

    return traverseNodesForSerialization(startNode.get(), pastEnd, NodeTraversalMode::EmitString);
}

Node* StyledMarkupAccumulator::traverseNodesForSerialization(Node* startNode, Node* pastEnd, NodeTraversalMode traversalMode)
{
    const bool shouldEmit = traversalMode == NodeTraversalMode::EmitString;

    m_inMSOList = false;

    unsigned depth = 0;
    auto enterNode = [&] (Node& node) {
        if (UNLIKELY(m_shouldPreserveMSOList) && shouldEmit) {
            if (appendNodeToPreserveMSOList(node))
                return false;
        }

        bool isDisplayContents = is<Element>(node) && downcast<Element>(node).hasDisplayContents();
        if (!node.renderer() && !isDisplayContents && !enclosingElementWithTag(firstPositionInOrBeforeNode(&node), selectTag))
            return false;

        ++depth;
        if (shouldEmit)
            startAppendingNode(node);

        return true;
    };

    Node* lastClosed = nullptr;
    auto exitNode = [&] (Node& node) {
        bool closing = depth;
        if (depth)
            --depth;
        if (shouldEmit) {
            if (closing)
                endAppendingNode(node);
            else
                wrapWithNode(node);
        }
        lastClosed = &node;
    };

    Node* lastNode = nullptr;
    Node* next = nullptr;
    for (auto* n = startNode; n != pastEnd; lastNode = n, n = next) {

        Vector<Node*, 8> exitedAncestors;
        next = nullptr;
        if (auto* child = firstChild(*n))
            next = child;
        else if (auto* sibling = nextSibling(*n))
            next = sibling;
        else {
            for (auto* ancestor = parentNode(*n); ancestor; ancestor = parentNode(*ancestor)) {
                exitedAncestors.append(ancestor);
                if (auto* sibling = nextSibling(*ancestor)) {
                    next = sibling;
                    break;
                }
            }
        }
        ASSERT(next || !pastEnd);

        if (isBlock(n) && canHaveChildrenForEditing(*n) && next == pastEnd) {
            // Don't write out empty block containers that aren't fully selected.
            continue;
        }

        if (!enterNode(*n)) {
            next = nextSkippingChildren(*n);
            // Don't skip over pastEnd.
            if (pastEnd && isDescendantOf(*pastEnd, *n))
                next = pastEnd;
            ASSERT(next || !pastEnd);
        } else {
            if (!hasChildNodes(*n))
                exitNode(*n);
        }

        for (auto* ancestor : exitedAncestors) {
            if (!depth && next == pastEnd)
                break;
            exitNode(*ancestor);
        }
    }
    
    ASSERT(lastNode || !depth);
    if (depth) {
        for (auto* ancestor = parentNode(pastEnd ? *pastEnd : *lastNode); ancestor && depth; ancestor = parentNode(*ancestor))
            exitNode(*ancestor);
    }

    return lastClosed;
}

bool StyledMarkupAccumulator::appendNodeToPreserveMSOList(Node& node)
{
    if (is<Comment>(node)) {
        auto& commentNode = downcast<Comment>(node);
        if (!m_inMSOList && commentNode.data() == "[if !supportLists]")
            m_inMSOList = true;
        else if (m_inMSOList && commentNode.data() == "[endif]")
            m_inMSOList = false;
        else
            return false;
        startAppendingNode(commentNode);
        return true;
    }
    if (is<HTMLStyleElement>(node)) {
        auto* firstChild = node.firstChild();
        if (!is<Text>(firstChild))
            return false;

        auto& textChild = downcast<Text>(*firstChild);
        auto& styleContent = textChild.data();

        const auto msoStyleDefinitionsStart = styleContent.find("/* Style Definitions */");
        const auto msoListDefinitionsStart = styleContent.find("/* List Definitions */");
        const auto lastListItem = styleContent.reverseFind("\n@list");
        if (msoListDefinitionsStart == notFound || lastListItem == notFound)
            return false;
        const auto start = msoStyleDefinitionsStart != notFound && msoStyleDefinitionsStart < msoListDefinitionsStart ? msoStyleDefinitionsStart : msoListDefinitionsStart;

        const auto msoListDefinitionsEnd = styleContent.find(";}\n", lastListItem);
        if (msoListDefinitionsEnd == notFound || start >= msoListDefinitionsEnd)
            return false;

        append("<head><style class=\"" WebKitMSOListQuirksStyle "\">\n<!--\n",
            StringView(textChild.data()).substring(start, msoListDefinitionsEnd - start + 3),
            "\n-->\n</style></head>");

        return true;
    }
    return false;
}

static Node* ancestorToRetainStructureAndAppearanceForBlock(Node* commonAncestorBlock)
{
    if (!commonAncestorBlock)
        return nullptr;

    if (commonAncestorBlock->hasTagName(tbodyTag) || commonAncestorBlock->hasTagName(trTag)) {
        ContainerNode* table = commonAncestorBlock->parentNode();
        while (table && !is<HTMLTableElement>(*table))
            table = table->parentNode();

        return table;
    }

    if (isNonTableCellHTMLBlockElement(commonAncestorBlock))
        return commonAncestorBlock;

    return nullptr;
}

static inline Node* ancestorToRetainStructureAndAppearance(Node* commonAncestor)
{
    return ancestorToRetainStructureAndAppearanceForBlock(enclosingBlock(commonAncestor));
}

static bool propertyMissingOrEqualToNone(const StyleProperties* style, CSSPropertyID propertyID)
{
    if (!style)
        return false;
    auto value = style->getPropertyCSSValue(propertyID);
    return !value || (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).valueID() == CSSValueNone);
}

static bool needInterchangeNewlineAfter(const VisiblePosition& v)
{
    VisiblePosition next = v.next();
    Node* upstreamNode = next.deepEquivalent().upstream().deprecatedNode();
    Node* downstreamNode = v.deepEquivalent().downstream().deprecatedNode();
    // Add an interchange newline if a paragraph break is selected and a br won't already be added to the markup to represent it.
    return isEndOfParagraph(v) && isStartOfParagraph(next) && !(upstreamNode->hasTagName(brTag) && upstreamNode == downstreamNode);
}

static RefPtr<EditingStyle> styleFromMatchedRulesAndInlineDecl(Node& node)
{
    if (!is<HTMLElement>(node))
        return nullptr;

    auto& element = downcast<HTMLElement>(node);
    auto style = EditingStyle::create(element.inlineStyle());
    style->mergeStyleFromRules(element);
    return style;
}

static bool isElementPresentational(const Node* node)
{
    return node->hasTagName(uTag) || node->hasTagName(sTag) || node->hasTagName(strikeTag)
        || node->hasTagName(iTag) || node->hasTagName(emTag) || node->hasTagName(bTag) || node->hasTagName(strongTag);
}

static Node* highestAncestorToWrapMarkup(const Position& start, const Position& end, Node& commonAncestor, AnnotateForInterchange annotate)
{
    Node* specialCommonAncestor = nullptr;
    if (annotate == AnnotateForInterchange::Yes) {
        // Include ancestors that aren't completely inside the range but are required to retain 
        // the structure and appearance of the copied markup.
        specialCommonAncestor = ancestorToRetainStructureAndAppearance(&commonAncestor);

        if (auto* parentListNode = enclosingNodeOfType(start, isListItem)) {
            if (!editingIgnoresContent(*parentListNode) && VisibleSelection::selectionFromContentsOfNode(parentListNode) == VisibleSelection(start, end)) {
                specialCommonAncestor = parentListNode->parentNode();
                while (specialCommonAncestor && !isListHTMLElement(specialCommonAncestor))
                    specialCommonAncestor = specialCommonAncestor->parentNode();
            }
        }

        // Retain the Mail quote level by including all ancestor mail block quotes.
        if (Node* highestMailBlockquote = highestEnclosingNodeOfType(start, isMailBlockquote, CanCrossEditingBoundary))
            specialCommonAncestor = highestMailBlockquote;
    }

    auto* checkAncestor = specialCommonAncestor ? specialCommonAncestor : &commonAncestor;
    if (checkAncestor->renderer() && checkAncestor->renderer()->containingBlock()) {
        Node* newSpecialCommonAncestor = highestEnclosingNodeOfType(firstPositionInNode(checkAncestor), &isElementPresentational, CanCrossEditingBoundary, checkAncestor->renderer()->containingBlock()->element());
        if (newSpecialCommonAncestor)
            specialCommonAncestor = newSpecialCommonAncestor;
    }

    // If a single tab is selected, commonAncestor will be a text node inside a tab span.
    // If two or more tabs are selected, commonAncestor will be the tab span.
    // In either case, if there is a specialCommonAncestor already, it will necessarily be above 
    // any tab span that needs to be included.
    if (!specialCommonAncestor && isTabSpanTextNode(&commonAncestor))
        specialCommonAncestor = commonAncestor.parentNode();
    if (!specialCommonAncestor && isTabSpanNode(&commonAncestor))
        specialCommonAncestor = &commonAncestor;

    if (auto* enclosingAnchor = enclosingElementWithTag(firstPositionInNode(specialCommonAncestor ? specialCommonAncestor : &commonAncestor), aTag))
        specialCommonAncestor = enclosingAnchor;

    return specialCommonAncestor;
}

static String serializePreservingVisualAppearanceInternal(const Position& start, const Position& end, Vector<Node*>* nodes, ResolveURLs urlsToResolve, SerializeComposedTree serializeComposedTree,
    AnnotateForInterchange annotate, ConvertBlocksToInlines convertBlocksToInlines, StandardFontFamilySerializationMode standardFontFamilySerializationMode, MSOListMode msoListMode)
{
    static NeverDestroyed<const String> interchangeNewlineString(MAKE_STATIC_STRING_IMPL("<br class=\"" AppleInterchangeNewline "\">"));

    if (!comparePositions(start, end))
        return emptyString();

    RefPtr<Node> commonAncestor = commonShadowIncludingAncestor(start, end);
    if (!commonAncestor)
        return emptyString();

    auto& document = *start.document();
    document.updateLayoutIgnorePendingStylesheets();

    VisiblePosition visibleStart { start };
    VisiblePosition visibleEnd { end };

    auto body = makeRefPtr(enclosingElementWithTag(firstPositionInNode(commonAncestor.get()), bodyTag));
    RefPtr<Element> fullySelectedRoot;
    // FIXME: Do this for all fully selected blocks, not just the body.
    if (body && VisiblePosition(firstPositionInNode(body.get())) == visibleStart && VisiblePosition(lastPositionInNode(body.get())) == visibleEnd)
        fullySelectedRoot = body;
    bool needsPositionStyleConversion = body && fullySelectedRoot == body && document.settings().shouldConvertPositionStyleOnCopy();

    Node* specialCommonAncestor = highestAncestorToWrapMarkup(start, end, *commonAncestor, annotate);

    StyledMarkupAccumulator accumulator(start, end, nodes, urlsToResolve, serializeComposedTree, annotate, standardFontFamilySerializationMode, msoListMode, needsPositionStyleConversion, specialCommonAncestor);

    Position startAdjustedForInterchangeNewline = start;
    if (annotate == AnnotateForInterchange::Yes && needInterchangeNewlineAfter(visibleStart)) {
        if (visibleStart == visibleEnd.previous())
            return interchangeNewlineString;

        accumulator.append(interchangeNewlineString.get());
        startAdjustedForInterchangeNewline = visibleStart.next().deepEquivalent();

        if (comparePositions(startAdjustedForInterchangeNewline, end) >= 0)
            return interchangeNewlineString;
    }

    Node* lastClosed = accumulator.serializeNodes(startAdjustedForInterchangeNewline, end);

    if (specialCommonAncestor && lastClosed) {
        // Also include all of the ancestors of lastClosed up to this special ancestor.
        for (ContainerNode* ancestor = accumulator.parentNode(*lastClosed); ancestor; ancestor = accumulator.parentNode(*ancestor)) {
            if (ancestor == fullySelectedRoot && convertBlocksToInlines == ConvertBlocksToInlines::No) {
                RefPtr<EditingStyle> fullySelectedRootStyle = styleFromMatchedRulesAndInlineDecl(*fullySelectedRoot);

                // Bring the background attribute over, but not as an attribute because a background attribute on a div
                // appears to have no effect.
                if ((!fullySelectedRootStyle || !fullySelectedRootStyle->style() || !fullySelectedRootStyle->style()->getPropertyCSSValue(CSSPropertyBackgroundImage))
                    && fullySelectedRoot->hasAttributeWithoutSynchronization(backgroundAttr))
                    fullySelectedRootStyle->style()->setProperty(CSSPropertyBackgroundImage, "url('" + fullySelectedRoot->getAttribute(backgroundAttr) + "')");

                if (fullySelectedRootStyle->style()) {
                    // Reset the CSS properties to avoid an assertion error in addStyleMarkup().
                    // This assertion is caused at least when we select all text of a <body> element whose
                    // 'text-decoration' property is "inherit", and copy it.
                    if (!propertyMissingOrEqualToNone(fullySelectedRootStyle->style(), CSSPropertyTextDecoration))
                        fullySelectedRootStyle->style()->setProperty(CSSPropertyTextDecoration, CSSValueNone);
                    if (!propertyMissingOrEqualToNone(fullySelectedRootStyle->style(), CSSPropertyWebkitTextDecorationsInEffect))
                        fullySelectedRootStyle->style()->setProperty(CSSPropertyWebkitTextDecorationsInEffect, CSSValueNone);
                    accumulator.wrapWithStyleNode(fullySelectedRootStyle->style(), document, true);
                }
            } else {
                // Since this node and all the other ancestors are not in the selection we want to set RangeFullySelectsNode to DoesNotFullySelectNode
                // so that styles that affect the exterior of the node are not included.
                accumulator.wrapWithNode(*ancestor, convertBlocksToInlines == ConvertBlocksToInlines::Yes, StyledMarkupAccumulator::DoesNotFullySelectNode);
            }
            if (nodes)
                nodes->append(ancestor);
            
            if (ancestor == specialCommonAncestor)
                break;
        }
    }
    
    if (accumulator.needRelativeStyleWrapper() && needsPositionStyleConversion) {
        if (accumulator.needClearingDiv())
            accumulator.append("<div style=\"clear: both;\"></div>");
        RefPtr<EditingStyle> positionRelativeStyle = styleFromMatchedRulesAndInlineDecl(*body);
        positionRelativeStyle->style()->setProperty(CSSPropertyPosition, CSSValueRelative);
        accumulator.wrapWithStyleNode(positionRelativeStyle->style(), document, true);
    }

    // FIXME: The interchange newline should be placed in the block that it's in, not after all of the content, unconditionally.
    if (annotate == AnnotateForInterchange::Yes && needInterchangeNewlineAfter(visibleEnd.previous()))
        accumulator.append(interchangeNewlineString.get());

#if PLATFORM(COCOA)
    // On Cocoa platforms, this markup is eventually persisted to the pasteboard and read back as UTF-8 data,
    // so this meta tag is needed for clients that read this data in the future from the pasteboard and load it.
    accumulator.prependMetaCharsetUTF8TagIfNonASCIICharactersArePresent();
#endif

    return accumulator.takeResults();
}

String serializePreservingVisualAppearance(const Range& range, Vector<Node*>* nodes, AnnotateForInterchange annotate, ConvertBlocksToInlines convertBlocksToInlines, ResolveURLs urlsToReslve)
{
    return serializePreservingVisualAppearanceInternal(range.startPosition(), range.endPosition(), nodes, urlsToReslve, SerializeComposedTree::No,
        annotate, convertBlocksToInlines, StandardFontFamilySerializationMode::Keep, MSOListMode::DoNotPreserve);
}

String serializePreservingVisualAppearance(const VisibleSelection& selection, ResolveURLs resolveURLs, SerializeComposedTree serializeComposedTree, Vector<Node*>* nodes)
{
    return serializePreservingVisualAppearanceInternal(selection.start(), selection.end(), nodes, resolveURLs, serializeComposedTree,
        AnnotateForInterchange::Yes, ConvertBlocksToInlines::No, StandardFontFamilySerializationMode::Keep, MSOListMode::DoNotPreserve);
}

static bool shouldPreserveMSOLists(StringView markup)
{
    if (!markup.startsWith("<html xmlns:"))
        return false;
    auto tagClose = markup.find('>');
    if (tagClose == notFound)
        return false;
    auto tag = markup.substring(0, tagClose);
    return tag.contains("xmlns:o=\"urn:schemas-microsoft-com:office:office\"")
        && tag.contains("xmlns:w=\"urn:schemas-microsoft-com:office:word\"");
}

String sanitizedMarkupForFragmentInDocument(Ref<DocumentFragment>&& fragment, Document& document, MSOListQuirks msoListQuirks, const String& originalMarkup)
{
    MSOListMode msoListMode = msoListQuirks == MSOListQuirks::CheckIfNeeded && shouldPreserveMSOLists(originalMarkup)
        ? MSOListMode::Preserve : MSOListMode::DoNotPreserve;

    auto bodyElement = makeRefPtr(document.body());
    ASSERT(bodyElement);
    bodyElement->appendChild(fragment.get());

    // SerializeComposedTree::No because there can't be a shadow tree in the pasted fragment.
    auto result = serializePreservingVisualAppearanceInternal(firstPositionInNode(bodyElement.get()), lastPositionInNode(bodyElement.get()), nullptr,
        ResolveURLs::YesExcludingLocalFileURLsForPrivacy, SerializeComposedTree::No, AnnotateForInterchange::Yes, ConvertBlocksToInlines::No,  StandardFontFamilySerializationMode::Strip, msoListMode);

    if (msoListMode != MSOListMode::Preserve)
        return result;

    return makeString(
        "<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"\n"
        "xmlns:w=\"urn:schemas-microsoft-com:office:word\"\n"
        "xmlns:m=\"http://schemas.microsoft.com/office/2004/12/omml\"\n"
        "xmlns=\"http://www.w3.org/TR/REC-html40\">",
        result,
        "</html>");
}

static void restoreAttachmentElementsInFragment(DocumentFragment& fragment)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    if (!RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled())
        return;

    // When creating a fragment we must strip the webkit-attachment-path attribute after restoring the File object.
    Vector<Ref<HTMLAttachmentElement>> attachments;
    for (auto& attachment : descendantsOfType<HTMLAttachmentElement>(fragment))
        attachments.append(attachment);

    for (auto& attachment : attachments) {
        attachment->setUniqueIdentifier(attachment->attributeWithoutSynchronization(webkitattachmentidAttr));

        auto attachmentPath = attachment->attachmentPath();
        auto blobURL = attachment->blobURL();
        if (!attachmentPath.isEmpty())
            attachment->setFile(File::create(attachmentPath));
        else if (!blobURL.isEmpty())
            attachment->setFile(File::deserialize({ }, blobURL, attachment->attachmentType(), attachment->attachmentTitle()));

        // Remove temporary attributes that were previously added in StyledMarkupAccumulator::appendCustomAttributes.
        attachment->removeAttribute(webkitattachmentidAttr);
        attachment->removeAttribute(webkitattachmentpathAttr);
        attachment->removeAttribute(webkitattachmentbloburlAttr);
    }

    Vector<Ref<HTMLImageElement>> images;
    for (auto& image : descendantsOfType<HTMLImageElement>(fragment))
        images.append(image);

    for (auto& image : images) {
        auto attachmentIdentifier = image->attributeWithoutSynchronization(webkitattachmentidAttr);
        if (attachmentIdentifier.isEmpty())
            continue;

        auto attachment = HTMLAttachmentElement::create(HTMLNames::attachmentTag, *fragment.ownerDocument());
        attachment->setUniqueIdentifier(attachmentIdentifier);
        image->setAttachmentElement(WTFMove(attachment));
        image->removeAttribute(webkitattachmentidAttr);
    }
#else
    UNUSED_PARAM(fragment);
#endif
}

Ref<DocumentFragment> createFragmentFromMarkup(Document& document, const String& markup, const String& baseURL, ParserContentPolicy parserContentPolicy)
{
    // We use a fake body element here to trick the HTML parser into using the InBody insertion mode.
    auto fakeBody = HTMLBodyElement::create(document);
    auto fragment = DocumentFragment::create(document);

    fragment->parseHTML(markup, fakeBody.ptr(), parserContentPolicy);
    restoreAttachmentElementsInFragment(fragment);
    if (!baseURL.isEmpty() && baseURL != aboutBlankURL() && baseURL != document.baseURL())
        completeURLs(fragment.ptr(), baseURL);

    return fragment;
}

String serializeFragment(const Node& node, SerializedNodes root, Vector<Node*>* nodes, ResolveURLs urlsToResolve, Vector<QualifiedName>* tagNamesToSkip, SerializationSyntax serializationSyntax)
{
    MarkupAccumulator accumulator(nodes, urlsToResolve, serializationSyntax);
    return accumulator.serializeNodes(const_cast<Node&>(node), root, tagNamesToSkip);
}

static void fillContainerFromString(ContainerNode& paragraph, const String& string)
{
    Document& document = paragraph.document();

    if (string.isEmpty()) {
        paragraph.appendChild(createBlockPlaceholderElement(document));
        return;
    }

    ASSERT(string.find('\n') == notFound);

    Vector<String> tabList = string.splitAllowingEmptyEntries('\t');
    String tabText = emptyString();
    bool first = true;
    size_t numEntries = tabList.size();
    for (size_t i = 0; i < numEntries; ++i) {
        const String& s = tabList[i];

        // append the non-tab textual part
        if (!s.isEmpty()) {
            if (!tabText.isEmpty()) {
                paragraph.appendChild(createTabSpanElement(document, tabText));
                tabText = emptyString();
            }
            Ref<Node> textNode = document.createTextNode(stringWithRebalancedWhitespace(s, first, i + 1 == numEntries));
            paragraph.appendChild(textNode);
        }

        // there is a tab after every entry, except the last entry
        // (if the last character is a tab, the list gets an extra empty entry)
        if (i + 1 != numEntries)
            tabText.append('\t');
        else if (!tabText.isEmpty())
            paragraph.appendChild(createTabSpanElement(document, tabText));

        first = false;
    }
}

bool isPlainTextMarkup(Node* node)
{
    ASSERT(node);
    if (!is<HTMLDivElement>(*node))
        return false;

    HTMLDivElement& element = downcast<HTMLDivElement>(*node);
    if (element.hasAttributes())
        return false;

    Node* firstChild = element.firstChild();
    if (!firstChild)
        return false;

    Node* secondChild = firstChild->nextSibling();
    if (!secondChild)
        return firstChild->isTextNode() || firstChild->firstChild();
    
    if (secondChild->nextSibling())
        return false;
    
    return isTabSpanTextNode(firstChild->firstChild()) && secondChild->isTextNode();
}

static bool contextPreservesNewline(const Range& context)
{
    VisiblePosition position(context.startPosition());
    Node* container = position.deepEquivalent().containerNode();
    if (!container || !container->renderer())
        return false;

    return container->renderer()->style().preserveNewline();
}

Ref<DocumentFragment> createFragmentFromText(Range& context, const String& text)
{
    Document& document = context.ownerDocument();
    Ref<DocumentFragment> fragment = document.createDocumentFragment();
    
    if (text.isEmpty())
        return fragment;

    String string = text;
    string.replace("\r\n", "\n");
    string.replace('\r', '\n');

    auto createHTMLBRElement = [&document]() {
        auto element = HTMLBRElement::create(document);
        element->setAttributeWithoutSynchronization(classAttr, AppleInterchangeNewline);
        return element;
    };

    if (contextPreservesNewline(context)) {
        fragment->appendChild(document.createTextNode(string));
        if (string.endsWith('\n')) {
            fragment->appendChild(createHTMLBRElement());
        }
        return fragment;
    }

    // A string with no newlines gets added inline, rather than being put into a paragraph.
    if (string.find('\n') == notFound) {
        fillContainerFromString(fragment, string);
        return fragment;
    }

    if (string.length() == 1 && string[0] == '\n') {
        // This is a single newline char, thus just create one HTMLBRElement.
        fragment->appendChild(createHTMLBRElement());
        return fragment;
    }

    // Break string into paragraphs. Extra line breaks turn into empty paragraphs.
    Node* blockNode = enclosingBlock(context.firstNode());
    Element* block = downcast<Element>(blockNode);
    bool useClonesOfEnclosingBlock = blockNode
        && blockNode->isElementNode()
        && !block->hasTagName(bodyTag)
        && !block->hasTagName(htmlTag)
        && block != editableRootForPosition(context.startPosition());
    bool useLineBreak = enclosingTextFormControl(context.startPosition());

    Vector<String> list = string.splitAllowingEmptyEntries('\n');
    size_t numLines = list.size();
    for (size_t i = 0; i < numLines; ++i) {
        const String& s = list[i];

        RefPtr<Element> element;
        if (s.isEmpty() && i + 1 == numLines) {
            // For last line, use the "magic BR" rather than a P.
            element = createHTMLBRElement();
        } else if (useLineBreak) {
            element = HTMLBRElement::create(document);
            fillContainerFromString(fragment, s);
        } else {
            if (useClonesOfEnclosingBlock)
                element = block->cloneElementWithoutChildren(document);
            else
                element = createDefaultParagraphElement(document);
            fillContainerFromString(*element, s);
        }
        fragment->appendChild(*element);
    }
    return fragment;
}

String documentTypeString(const Document& document)
{
    DocumentType* documentType = document.doctype();
    if (!documentType)
        return emptyString();
    return serializeFragment(*documentType, SerializedNodes::SubtreeIncludingNode);
}

String urlToMarkup(const URL& url, const String& title)
{
    StringBuilder markup;
    markup.appendLiteral("<a href=\"");
    markup.append(url.string());
    markup.appendLiteral("\">");
    MarkupAccumulator::appendCharactersReplacingEntities(markup, title, 0, title.length(), EntityMaskInPCDATA);
    markup.appendLiteral("</a>");
    return markup.toString();
}

ExceptionOr<Ref<DocumentFragment>> createFragmentForInnerOuterHTML(Element& contextElement, const String& markup, ParserContentPolicy parserContentPolicy)
{
    auto* document = &contextElement.document();
    if (contextElement.hasTagName(templateTag))
        document = &document->ensureTemplateDocument();
    auto fragment = DocumentFragment::create(*document);

    if (document->isHTMLDocument()) {
        fragment->parseHTML(markup, &contextElement, parserContentPolicy);
        return fragment;
    }

    bool wasValid = fragment->parseXML(markup, &contextElement, parserContentPolicy);
    if (!wasValid)
        return Exception { SyntaxError };
    return fragment;
}

RefPtr<DocumentFragment> createFragmentForTransformToFragment(Document& outputDoc, const String& sourceString, const String& sourceMIMEType)
{
    RefPtr<DocumentFragment> fragment = outputDoc.createDocumentFragment();
    
    if (sourceMIMEType == "text/html") {
        // As far as I can tell, there isn't a spec for how transformToFragment is supposed to work.
        // Based on the documentation I can find, it looks like we want to start parsing the fragment in the InBody insertion mode.
        // Unfortunately, that's an implementation detail of the parser.
        // We achieve that effect here by passing in a fake body element as context for the fragment.
        auto fakeBody = HTMLBodyElement::create(outputDoc);
        fragment->parseHTML(sourceString, fakeBody.ptr());
    } else if (sourceMIMEType == "text/plain")
        fragment->parserAppendChild(Text::create(outputDoc, sourceString));
    else {
        bool successfulParse = fragment->parseXML(sourceString, 0);
        if (!successfulParse)
            return nullptr;
    }
    
    // FIXME: Do we need to mess with URLs here?
    
    return fragment;
}

Ref<DocumentFragment> createFragmentForImageAndURL(Document& document, const String& url, PresentationSize preferredSize)
{
    auto imageElement = HTMLImageElement::create(document);
    imageElement->setAttributeWithoutSynchronization(HTMLNames::srcAttr, url);
    if (preferredSize.width)
        imageElement->setAttributeWithoutSynchronization(HTMLNames::widthAttr, AtomString::number(*preferredSize.width));
    if (preferredSize.height)
        imageElement->setAttributeWithoutSynchronization(HTMLNames::heightAttr, AtomString::number(*preferredSize.height));
    auto fragment = document.createDocumentFragment();
    fragment->appendChild(imageElement);

    return fragment;
}

static Vector<Ref<HTMLElement>> collectElementsToRemoveFromFragment(ContainerNode& container)
{
    Vector<Ref<HTMLElement>> toRemove;
    for (auto& element : childrenOfType<HTMLElement>(container)) {
        if (is<HTMLHtmlElement>(element)) {
            toRemove.append(element);
            collectElementsToRemoveFromFragment(element);
            continue;
        }
        if (is<HTMLHeadElement>(element) || is<HTMLBodyElement>(element))
            toRemove.append(element);
    }
    return toRemove;
}

static void removeElementFromFragmentPreservingChildren(DocumentFragment& fragment, HTMLElement& element)
{
    RefPtr<Node> nextChild;
    for (RefPtr<Node> child = element.firstChild(); child; child = nextChild) {
        nextChild = child->nextSibling();
        element.removeChild(*child);
        fragment.insertBefore(*child, &element);
    }
    fragment.removeChild(element);
}

ExceptionOr<Ref<DocumentFragment>> createContextualFragment(Element& element, const String& markup, ParserContentPolicy parserContentPolicy)
{
    auto result = createFragmentForInnerOuterHTML(element, markup, parserContentPolicy);
    if (result.hasException())
        return result.releaseException();

    auto fragment = result.releaseReturnValue();

    // We need to pop <html> and <body> elements and remove <head> to
    // accommodate folks passing complete HTML documents to make the
    // child of an element.
    auto toRemove = collectElementsToRemoveFromFragment(fragment);
    for (auto& element : toRemove)
        removeElementFromFragmentPreservingChildren(fragment, element);

    return fragment;
}

static inline bool hasOneChild(ContainerNode& node)
{
    Node* firstChild = node.firstChild();
    return firstChild && !firstChild->nextSibling();
}

static inline bool hasOneTextChild(ContainerNode& node)
{
    return hasOneChild(node) && node.firstChild()->isTextNode();
}

static inline bool hasMutationEventListeners(const Document& document)
{
    return document.hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER)
        || document.hasListenerType(Document::DOMNODEINSERTED_LISTENER)
        || document.hasListenerType(Document::DOMNODEREMOVED_LISTENER)
        || document.hasListenerType(Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)
        || document.hasListenerType(Document::DOMCHARACTERDATAMODIFIED_LISTENER);
}

// We can use setData instead of replacing Text node as long as script can't observe the difference.
static inline bool canUseSetDataOptimization(const Text& containerChild, const ChildListMutationScope& mutationScope)
{
    bool authorScriptMayHaveReference = containerChild.refCount();
    return !authorScriptMayHaveReference && !mutationScope.canObserve() && !hasMutationEventListeners(containerChild.document());
}

ExceptionOr<void> replaceChildrenWithFragment(ContainerNode& container, Ref<DocumentFragment>&& fragment)
{
    Ref<ContainerNode> containerNode(container);
    ChildListMutationScope mutation(containerNode);

    if (!fragment->firstChild()) {
        containerNode->removeChildren();
        return { };
    }

    auto* containerChild = containerNode->firstChild();
    if (containerChild && !containerChild->nextSibling()) {
        if (is<Text>(*containerChild) && hasOneTextChild(fragment) && canUseSetDataOptimization(downcast<Text>(*containerChild), mutation)) {
            ASSERT(!fragment->firstChild()->refCount());
            downcast<Text>(*containerChild).setData(downcast<Text>(*fragment->firstChild()).data());
            return { };
        }

        return containerNode->replaceChild(fragment, *containerChild);
    }

    containerNode->removeChildren();
    return containerNode->appendChild(fragment);
}

}
