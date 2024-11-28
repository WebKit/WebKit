/*
 * Copyright (C) 2004-2024 Apple Inc. All rights reserved.
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
#include "AttachmentAssociatedElement.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSValue.h"
#include "CSSValueKeywords.h"
#include "CacheStorageProvider.h"
#include "ChildListMutationScope.h"
#include "Comment.h"
#include "CommonAtomStrings.h"
#include "ComposedTreeIterator.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "EmptyClients.h"
#include "File.h"
#include "FrameLoader.h"
#include "HTMLAttachmentElement.h"
#include "HTMLBRElement.h"
#include "HTMLBaseElement.h"
#include "HTMLBodyElement.h"
#include "HTMLDivElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLPictureElement.h"
#include "HTMLSourceElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "LocalFrame.h"
#include "MarkupAccumulator.h"
#include "MutableStyleProperties.h"
#include "NodeList.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "PasteboardItemInfo.h"
#include "Quirks.h"
#include "Range.h"
#include "RenderBlock.h"
#include "ScriptWrappableInlines.h"
#include "Settings.h"
#include "SocketProvider.h"
#include "TextIterator.h"
#include "TextManipulationController.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisibleSelection.h"
#include "VisibleUnits.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/URLParser.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(DATA_DETECTION)
#include "DataDetection.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static bool propertyMissingOrEqualToNone(const StyleProperties*, CSSPropertyID);

class AttributeChange {
public:
    AttributeChange()
        : m_name(nullAtom(), nullAtom(), nullAtom())
    {
    }

    AttributeChange(RefPtr<Element>&& element, QualifiedName&& name, AtomString&& value)
        : m_element(WTFMove(element))
        , m_name(WTFMove(name))
        , m_value(WTFMove(value))
    {
    }

    void apply()
    {
        m_element->setAttribute(m_name, m_value);
    }

private:
    RefPtr<Element> m_element;
    QualifiedName m_name;
    AtomString m_value;
};

static void completeURLs(DocumentFragment* fragment, const String& baseURL)
{
    Vector<AttributeChange> changes;

    URL parsedBaseURL({ }, baseURL);

    for (Ref element : descendantsOfType<Element>(*fragment)) {
        if (!element->hasAttributes())
            continue;
        for (const Attribute& attribute : element->attributesIterator()) {
            if (element->attributeContainsURL(attribute) && !attribute.value().isEmpty())
                changes.append(AttributeChange(element.copyRef(), QualifiedName { attribute.name() }, AtomString { element->completeURLsInAttributeValue(parsedBaseURL, attribute) }));
        }
    }

    for (auto& change : changes)
        change.apply();
}

void replaceSubresourceURLs(Ref<DocumentFragment>&& fragment, UncheckedKeyHashMap<AtomString, AtomString>&& replacementMap)
{
    Vector<AttributeChange> changes;
    for (Ref element : descendantsOfType<Element>(fragment)) {
        if (!element->hasAttributes())
            continue;
        for (const Attribute& attribute : element->attributesIterator()) {
            // FIXME: This won't work for srcset.
            if (element->attributeContainsURL(attribute) && !attribute.value().isEmpty()) {
                auto replacement = replacementMap.get(attribute.value());
                if (!replacement.isNull())
                    changes.append({ element.copyRef(), QualifiedName { attribute.name() }, WTFMove(replacement) });
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

void removeSubresourceURLAttributes(Ref<DocumentFragment>&& fragment, Function<bool(const URL&)> shouldRemoveURL)
{
    Vector<ElementAttribute> attributesToRemove;
    for (Ref element : descendantsOfType<Element>(fragment)) {
        if (!element->hasAttributes())
            continue;
        for (const Attribute& attribute : element->attributesIterator()) {
            // FIXME: This won't work for srcset.
            if (element->attributeContainsURL(attribute) && !attribute.value().isEmpty()) {
                if (shouldRemoveURL(URL { attribute.value() }))
                    attributesToRemove.append({ element.copyRef(), attribute.name() });
            }
        }
    }
    for (auto& [element, attribute] : attributesToRemove)
        element->removeAttribute(attribute);
}

Ref<Page> createPageForSanitizingWebContent()
{
    auto pageConfiguration = pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID());
    
    Ref page = Page::create(WTFMove(pageConfiguration));
#if ENABLE(VIDEO)
    page->settings().setMediaEnabled(false);
#endif
    page->settings().setScriptEnabled(false);
    page->settings().setHTMLParserScriptingFlagPolicy(HTMLParserScriptingFlagPolicy::Enabled);
    page->settings().setAcceleratedCompositingEnabled(false);
    page->settings().setLinkPreloadEnabled(false);

    RefPtr frame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!frame)
        return page;

    frame->setView(LocalFrameView::create(*frame, IntSize { 800, 600 }));
    frame->init();

    FrameLoader& loader = frame->loader();
    static constexpr ASCIILiteral markup = "<!DOCTYPE html><html><body></body></html>"_s;
    RefPtr activeDocumentLoader = loader.activeDocumentLoader();
    ASSERT(activeDocumentLoader);
    auto& writer = activeDocumentLoader->writer();
    writer.setMIMEType("text/html"_s);
    writer.begin();
    writer.insertDataSynchronously(markup);
    writer.end();
    RELEASE_ASSERT(frame->document()->body());

    return page;
}

String sanitizeMarkup(const String& rawHTML, MSOListQuirks msoListQuirks, std::optional<Function<void(DocumentFragment&)>> fragmentSanitizer)
{
    Ref page = createPageForSanitizingWebContent();
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!localMainFrame)
        return String();

    RefPtr stagingDocument = localMainFrame->document();
    ASSERT(stagingDocument);

    auto fragment = createFragmentFromMarkup(*stagingDocument, rawHTML, emptyString(), { });

    if (fragmentSanitizer)
        (*fragmentSanitizer)(fragment);

    return sanitizedMarkupForFragmentInDocument(WTFMove(fragment), *stagingDocument, msoListQuirks, rawHTML);
}

UserSelectNoneStateCache::UserSelectNoneStateCache(TreeType treeType)
    : m_useComposedTree(treeType == ComposedTree)
{
    ASSERT(treeType == Tree || treeType == ComposedTree);
}

ContainerNode* UserSelectNoneStateCache::parentNode(Node& node)
{
    if (m_useComposedTree)
        return node.parentInComposedTree();
    return node.parentOrShadowHostNode();
}

Node* UserSelectNoneStateCache::firstChild(Node& node)
{
    if (m_useComposedTree)
        return firstChildInComposedTreeIgnoringUserAgentShadow(node);
    return node.firstChild();
}

Node* UserSelectNoneStateCache::nextSibling(Node& node)
{
    if (m_useComposedTree)
        return nextSiblingInComposedTreeIgnoringUserAgentShadow(node);
    return node.nextSibling();
}

auto UserSelectNoneStateCache::computeState(Node& targetNode) -> State
{
    auto it = m_cache.find(targetNode);
    if (it != m_cache.end())
        return it->value;
    if (!Position::nodeIsUserSelectNone(&targetNode))
        return State::NotUserSelectNone;
    auto state = State::OnlyUserSelectNone;
    RefPtr currentNode = &targetNode;
    bool foundMixed = false;
    while (currentNode) {
        if (!Position::nodeIsUserSelectNone(currentNode.get())) {
            state = State::Mixed;
            // Only traverse upward once any mixed content is found
            // since inner element may only contain user-select: none but we won't be able to tell apart.
            foundMixed = true;
        }
        if (RefPtr child = firstChild(*currentNode); child && !foundMixed)
            currentNode = WTFMove(child);
        else if (currentNode == &targetNode)
            break;
        else if (RefPtr sibling = nextSibling(*currentNode); sibling && !foundMixed)
            currentNode = WTFMove(sibling);
        else {
            RefPtr<Node> ancestor;
            for (ancestor = parentNode(*currentNode); ancestor; ancestor = parentNode(*ancestor)) {
                m_cache.set(*ancestor, state);
                if (ancestor == &targetNode) {
                    currentNode = nullptr;
                    break;
                }
                if (RefPtr sibling = nextSibling(*ancestor); sibling && !foundMixed) {
                    currentNode = WTFMove(sibling);
                    break;
                }
            }
            if (!ancestor)
                currentNode = nullptr;
        }
    }
    return state;
}

enum class MSOListMode : bool { Preserve, DoNotPreserve };
class StyledMarkupAccumulator final : public MarkupAccumulator {
public:
    enum RangeFullySelectsNode { DoesFullySelectNode, DoesNotFullySelectNode };

    StyledMarkupAccumulator(const Position& start, const Position& end, Vector<Ref<Node>>* nodes, ResolveURLs, SerializeComposedTree, IgnoreUserSelectNone,
        AnnotateForInterchange, StandardFontFamilySerializationMode, MSOListMode, bool needsPositionStyleConversion, Node* highestNodeToBeSerialized = nullptr);

    RefPtr<Node> serializeNodes(const Position& start, const Position& end);
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

    void prependHeadIfNecessary(const HTMLBaseElement* baseElement)
    {
#if PLATFORM(COCOA)
        // On Cocoa platforms, this markup is eventually persisted to the pasteboard and read back as UTF-8 data,
        // so this meta tag is needed for clients that read this data in the future from the pasteboard and load it.
        bool shouldAppendMetaCharset = !containsOnlyASCII();
#else
        bool shouldAppendMetaCharset = false;
#endif
        if (!shouldAppendMetaCharset && !baseElement)
            return;

        m_reversedPrecedingMarkup.append("</head>"_s);
        if (baseElement) {
            StringBuilder markupForBase;
            appendStartTag(markupForBase, *baseElement, false, DoesNotFullySelectNode);
            m_reversedPrecedingMarkup.append(markupForBase.toString());
        }
        if (shouldAppendMetaCharset)
            m_reversedPrecedingMarkup.append("<meta charset=\"UTF-8\">"_s);
        m_reversedPrecedingMarkup.append("<head>"_s);
    }

private:
    bool containsOnlyASCII() const;
    void appendStyleNodeOpenTag(StringBuilder&, StyleProperties*, Document&, bool isBlock = false);
    const String& styleNodeCloseTag(bool isBlock = false);

    String renderedTextRespectingRange(const Text&);
    String textContentRespectingRange(const Text&);

    bool shouldPreserveMSOListStyleForElement(const Element&);

    enum class SpanReplacementType : uint8_t {
        None,
        Slot,
#if ENABLE(DATA_DETECTION)
        DataDetector,
#endif
    };
    SpanReplacementType spanReplacementForElement(const Element&);

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
    RefPtr<Node> traverseNodesForSerialization(Node& startNode, Node* pastEnd, NodeTraversalMode);

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
    bool m_ignoresUserSelectNone;
    bool m_needsPositionStyleConversion;
    StandardFontFamilySerializationMode m_standardFontFamilySerializationMode;
    bool m_shouldPreserveMSOList;
    bool m_needRelativeStyleWrapper { false };
    bool m_needClearingDiv { false };
    bool m_inMSOList { false };
};

inline StyledMarkupAccumulator::StyledMarkupAccumulator(const Position& start, const Position& end, Vector<Ref<Node>>* nodes, ResolveURLs resolveURLs,
    SerializeComposedTree serializeComposedTree, IgnoreUserSelectNone ignoreUserSelectNone, AnnotateForInterchange annotate,
    StandardFontFamilySerializationMode standardFontFamilySerializationMode, MSOListMode msoListMode, bool needsPositionStyleConversion, Node* highestNodeToBeSerialized)
    : MarkupAccumulator(nodes, resolveURLs, start.document()->isHTMLDocument() ? SerializationSyntax::HTML : SerializationSyntax::XML)
    , m_start(start)
    , m_end(end)
    , m_annotate(annotate)
    , m_highestNodeToBeSerialized(highestNodeToBeSerialized)
    , m_useComposedTree(serializeComposedTree == SerializeComposedTree::Yes)
    , m_ignoresUserSelectNone(ignoreUserSelectNone == IgnoreUserSelectNone::Yes && !start.document()->quirks().needsToCopyUserSelectNoneQuirk())
    , m_needsPositionStyleConversion(needsPositionStyleConversion)
    , m_standardFontFamilySerializationMode(standardFontFamilySerializationMode)
    , m_shouldPreserveMSOList(msoListMode == MSOListMode::Preserve)
{
}

void StyledMarkupAccumulator::wrapWithNode(Node& node, bool convertBlocksToInlines, RangeFullySelectsNode rangeFullySelectsNode)
{
    StringBuilder markup;
    if (RefPtr element = dynamicDowncast<Element>(node))
        appendStartTag(markup, *element, convertBlocksToInlines && isBlock(node), rangeFullySelectsNode);
    else
        appendNonElementNode(markup, node, nullptr);
    m_reversedPrecedingMarkup.append(markup.toString());
    endAppendingNode(node);
    if (m_nodes)
        m_nodes->append(node);
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
    // With AnnotateForInterchange::Yes, wrappingStyleForSerialization should have removed -webkit-text-decorations-in-effect
    ASSERT(!shouldAnnotate() || propertyMissingOrEqualToNone(style, CSSPropertyWebkitTextDecorationsInEffect));
    if (isBlock)
        out.append("<div style=\""_s);
    else
        out.append("<span style=\""_s);
    appendAttributeValue(out, style->asText(), document.isHTMLDocument());
    out.append("\">"_s);
}

const String& StyledMarkupAccumulator::styleNodeCloseTag(bool isBlock)
{
    static NeverDestroyed<const String> divClose(MAKE_STATIC_STRING_IMPL("</div>"));
    static NeverDestroyed<const String> styleSpanClose(MAKE_STATIC_STRING_IMPL("</span>"));
    return isBlock ? divClose : styleSpanClose;
}

bool StyledMarkupAccumulator::containsOnlyASCII() const
{
    for (auto& preceding : m_reversedPrecedingMarkup) {
        if (!preceding.containsOnlyASCII())
            return false;
    }
    return MarkupAccumulator::containsOnlyASCII();
}

String StyledMarkupAccumulator::takeResults()
{
    CheckedUint32 length = this->length();
    for (auto& string : m_reversedPrecedingMarkup)
        length += string.length();
    StringBuilder result;
    result.reserveCapacity(length);
    for (auto& string : makeReversedRange(m_reversedPrecedingMarkup))
        result.append(string);
    result.append(takeMarkup());
    // Remove '\0' characters because they are not visibly rendered to the user.
    return makeStringByReplacingAll(result.toString(), '\0', ""_s);
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

        appendStyleNodeOpenTag(out, wrappingStyle->style(), text.protectedDocument());
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
    TextIteratorBehaviors behaviors;
    Position start = &text == m_start.containerNode() ? m_start : firstPositionInNode(const_cast<Text*>(&text));
    Position end;
    if (&text == m_end.containerNode())
        end = m_end;
    else {
        end = lastPositionInNode(const_cast<Text*>(&text));
        if (!m_end.isNull())
            behaviors.add(TextIteratorBehavior::BehavesAsIfNodesFollowing);
    }
    if (m_ignoresUserSelectNone)
        behaviors.add(TextIteratorBehavior::IgnoresUserSelectNone);

    auto range = makeSimpleRange(start, end);
    return range ? plainText(*range, behaviors) : emptyString();
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
    if (!DeprecatedGlobalSettings::attachmentElementEnabled())
        return;
    
    if (RefPtr attachment = dynamicDowncast<HTMLAttachmentElement>(element)) {
        appendAttribute(out, element, { webkitattachmentidAttr, AtomString { attachment->uniqueIdentifier() } }, namespaces);
        if (RefPtr file = attachment->file()) {
            // These attributes are only intended for File deserialization, and are removed from the generated attachment
            // element after we've deserialized and set its backing File, in restoreAttachmentElementsInFragment.
            appendAttribute(out, element, { webkitattachmentpathAttr, AtomString { file->path() } }, namespaces);
            appendAttribute(out, element, { webkitattachmentbloburlAttr, AtomString { file->url().string() } }, namespaces);
        }
    } else if (RefPtr imgElement = dynamicDowncast<HTMLImageElement>(element)) {
        if (RefPtr attachment = imgElement->attachmentElement())
            appendAttribute(out, element, { webkitattachmentidAttr, AtomString { attachment->uniqueIdentifier() } }, namespaces);
    } else if (RefPtr sourceElement = dynamicDowncast<HTMLSourceElement>(element)) {
        if (RefPtr attachment = sourceElement->attachmentElement())
            appendAttribute(out, element, { webkitattachmentidAttr, AtomString { attachment->uniqueIdentifier() } }, namespaces);
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
        return style.startsWith("mso-list:"_s) || style.contains(";mso-list:"_s) || style.contains("\nmso-list:"_s);
    }
    return false;
}

StyledMarkupAccumulator::SpanReplacementType StyledMarkupAccumulator::spanReplacementForElement(const Element& element)
{
    if (is<HTMLSlotElement>(element))
        return SpanReplacementType::Slot;

#if ENABLE(DATA_DETECTION)
    if (DataDetection::isDataDetectorElement(element))
        return SpanReplacementType::DataDetector;
#endif

    return SpanReplacementType::None;
}

void StyledMarkupAccumulator::appendStartTag(StringBuilder& out, const Element& element, bool addDisplayInline, RangeFullySelectsNode rangeFullySelectsNode)
{
    const bool documentIsHTML = element.document().isHTMLDocument();

    auto replacementType = spanReplacementForElement(element);
    if (UNLIKELY(replacementType != SpanReplacementType::None))
        out.append("<span"_s);
    else
        appendOpenTag(out, element, nullptr);

    appendCustomAttributes(out, element, nullptr);

    const bool shouldAnnotateOrForceInline = element.isHTMLElement() && (shouldAnnotate() || addDisplayInline);
    bool shouldOverrideStyleAttr = (shouldAnnotateOrForceInline || shouldApplyWrappingStyle(element) || replacementType != SpanReplacementType::None) && !shouldPreserveMSOListStyleForElement(element);
    if (element.hasAttributes()) {
        for (const Attribute& attribute : element.attributesIterator()) {
            // We'll handle the style attribute separately, below.
            if (attribute.name() == styleAttr && shouldOverrideStyleAttr)
                continue;
            if (element.isEventHandlerAttribute(attribute) || element.attributeContainsJavaScriptURL(attribute))
                continue;
#if ENABLE(DATA_DETECTION)
            if (replacementType == SpanReplacementType::DataDetector && DataDetection::isDataDetectorAttribute(attribute.name()))
                continue;
#endif
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

        if (replacementType == SpanReplacementType::Slot)
            newInlineStyle->addDisplayContents();

        if (RefPtr styledElement = dynamicDowncast<StyledElement>(element); styledElement && styledElement->inlineStyle())
            newInlineStyle->overrideWithStyle(*styledElement->inlineStyle());

#if ENABLE(DATA_DETECTION)
        if (replacementType == SpanReplacementType::DataDetector && newInlineStyle->style())
            newInlineStyle->style()->removeProperty(CSSPropertyTextDecorationColor);
#endif

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
            out.append(" style=\""_s);
            appendAttributeValue(out, newInlineStyle->style()->asText(), documentIsHTML);
            out.append('"');
        }
    }

    appendCloseTag(out, element);
}

void StyledMarkupAccumulator::appendEndTag(StringBuilder& out, const Element& element)
{
    if (UNLIKELY(spanReplacementForElement(element) != SpanReplacementType::None))
        out.append("</span>"_s);
    else
        MarkupAccumulator::appendEndTag(out, element);
}

RefPtr<Node> StyledMarkupAccumulator::serializeNodes(const Position& start, const Position& end)
{
    ASSERT(start <= end);
    RefPtr startNode = start.firstNode();
    if (!startNode)
        return nullptr;
    RefPtr pastEnd = end.computeNodeAfterPosition();
    if (!pastEnd && end.containerNode())
        pastEnd = nextSkippingChildren(*end.protectedContainerNode());

    if (!m_highestNodeToBeSerialized)
        m_highestNodeToBeSerialized = traverseNodesForSerialization(*startNode, pastEnd.get(), NodeTraversalMode::DoNotEmitString);

    if (m_highestNodeToBeSerialized && m_highestNodeToBeSerialized->parentNode())
        m_wrappingStyle = EditingStyle::wrappingStyleForSerialization(*m_highestNodeToBeSerialized->protectedParentNode(), shouldAnnotate(), m_standardFontFamilySerializationMode);

    return traverseNodesForSerialization(*startNode, pastEnd.get(), NodeTraversalMode::EmitString);
}

RefPtr<Node> StyledMarkupAccumulator::traverseNodesForSerialization(Node& startNode, Node* pastEnd, NodeTraversalMode traversalMode)
{
    const bool shouldEmit = traversalMode == NodeTraversalMode::EmitString;
    UserSelectNoneStateCache userSelectNoneStateCache(m_useComposedTree ? ComposedTree : Tree);

    m_inMSOList = false;

    unsigned depth = 0;
    auto enterNode = [&] (Node& node) {
        if (UNLIKELY(m_shouldPreserveMSOList) && shouldEmit) {
            if (appendNodeToPreserveMSOList(node))
                return false;
        }

        RefPtr element = dynamicDowncast<Element>(node);
        bool isDisplayContents = element && element->hasDisplayContents();
        if (!node.renderer() && !isDisplayContents && !enclosingElementWithTag(firstPositionInOrBeforeNode(&node), selectTag))
            return false;

        if (node.renderer() && node.renderer()->isSkippedContent())
            return false;

        if (m_ignoresUserSelectNone && userSelectNoneStateCache.nodeOnlyContainsUserSelectNone(node))
            return false;

        ++depth;
        if (shouldEmit)
            startAppendingNode(node);

        return true;
    };

    RefPtr<Node> lastClosed;
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

    RefPtr<Node> lastNode;
    RefPtr<Node> next;
    for (RefPtr n = &startNode; n != pastEnd; lastNode = n, n = next) {

        Vector<RefPtr<Node>, 8> exitedAncestors;
        next = nullptr;

        auto advanceToAncestorSibling = [&]() {
            if (auto* sibling = nextSibling(*n)) {
                next = sibling;
                return;
            }
            for (RefPtr ancestor = parentNode(*n); ancestor; ancestor = parentNode(*ancestor)) {
                exitedAncestors.append(ancestor);
                if (RefPtr sibling = nextSibling(*ancestor)) {
                    next = WTFMove(sibling);
                    return;
                }
            }
        };

        if (RefPtr child = firstChild(*n))
            next = WTFMove(child);
        else
            advanceToAncestorSibling();

        ASSERT(next || !pastEnd || n->containsIncludingShadowDOM(pastEnd));

        if (isBlock(*n) && canHaveChildrenForEditing(*n) && next == pastEnd) {
            // Don't write out empty block containers that aren't fully selected.
            continue;
        }

        bool didEnterNode = false;
        if (!enterNode(*n)) {
            exitedAncestors.clear();
            advanceToAncestorSibling();
        } else if (!hasChildNodes(*n))
            exitNode(*n);
        else
            didEnterNode = true;

        bool aboutToGoPastEnd = pastEnd && !didEnterNode && (!next || isDescendantOf(*pastEnd, *n));
        if (aboutToGoPastEnd)
            next = pastEnd;

        for (auto& ancestor : exitedAncestors) {
            if (!depth && next == pastEnd)
                break;
            exitNode(*ancestor);
        }
    }
    
    ASSERT(lastNode || !depth);
    if (depth) {
        for (RefPtr ancestor = parentNode(pastEnd ? *pastEnd : *lastNode); ancestor && depth; ancestor = parentNode(*ancestor))
            exitNode(*ancestor);
    }

    return lastClosed;
}

bool StyledMarkupAccumulator::appendNodeToPreserveMSOList(Node& node)
{
    if (RefPtr commentNode = dynamicDowncast<Comment>(node)) {
        if (!m_inMSOList && commentNode->data() == "[if !supportLists]"_s)
            m_inMSOList = true;
        else if (m_inMSOList && commentNode->data() == "[endif]"_s)
            m_inMSOList = false;
        else
            return false;
        startAppendingNode(*commentNode);
        return true;
    }
    if (is<HTMLStyleElement>(node)) {
        RefPtr textChild = dynamicDowncast<Text>(node.firstChild());
        if (!textChild)
            return false;

        auto& styleContent = textChild->data();

        const auto msoStyleDefinitionsStart = styleContent.find("/* Style Definitions */"_s);
        const auto msoListDefinitionsStart = styleContent.find("/* List Definitions */"_s);
        const auto lastListItem = styleContent.reverseFind("\n@list"_s);
        if (msoListDefinitionsStart == notFound || lastListItem == notFound)
            return false;
        const auto start = msoStyleDefinitionsStart != notFound && msoStyleDefinitionsStart < msoListDefinitionsStart ? msoStyleDefinitionsStart : msoListDefinitionsStart;

        const auto msoListDefinitionsEnd = styleContent.find(";}\n"_s, lastListItem);
        if (msoListDefinitionsEnd == notFound || start >= msoListDefinitionsEnd)
            return false;

        append("<head><style class=\""_s, WebKitMSOListQuirksStyle, "\">\n<!--\n"_s,
            StringView(textChild->data()).substring(start, msoListDefinitionsEnd - start + 3),
            "\n-->\n</style></head>"_s);

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
    return ancestorToRetainStructureAndAppearanceForBlock(enclosingBlock(commonAncestor).get());
}

static bool propertyMissingOrEqualToNone(const StyleProperties* style, CSSPropertyID propertyID)
{
    if (!style)
        return false;
    return style->propertyAsValueID(propertyID).value_or(CSSValueNone) == CSSValueNone;
}

static bool needInterchangeNewlineAfter(const VisiblePosition& v)
{
    VisiblePosition next = v.next();
    RefPtr upstreamNode = next.deepEquivalent().upstream().deprecatedNode();
    RefPtr downstreamNode = v.deepEquivalent().downstream().deprecatedNode();
    // Add an interchange newline if a paragraph break is selected and a br won't already be added to the markup to represent it.
    return isEndOfParagraph(v) && isStartOfParagraph(next) && !(upstreamNode->hasTagName(brTag) && upstreamNode == downstreamNode);
}

static RefPtr<EditingStyle> styleFromMatchedRulesAndInlineDecl(Node& node)
{
    RefPtr element = dynamicDowncast<HTMLElement>(node);
    if (!element)
        return nullptr;

    Ref style = EditingStyle::create(element->inlineStyle());
    style->mergeStyleFromRules(*element);
    return style;
}

static bool isElementPresentational(const Node& node)
{
    return node.hasTagName(uTag) || node.hasTagName(sTag) || node.hasTagName(strikeTag)
        || node.hasTagName(iTag) || node.hasTagName(emTag) || node.hasTagName(bTag) || node.hasTagName(strongTag);
}

static RefPtr<Node> highestAncestorToWrapMarkup(const Position& start, const Position& end, Node& commonAncestor, AnnotateForInterchange annotate)
{
    RefPtr<Node> specialCommonAncestor;
    if (annotate == AnnotateForInterchange::Yes) {
        // Include ancestors that aren't completely inside the range but are required to retain 
        // the structure and appearance of the copied markup.
        specialCommonAncestor = ancestorToRetainStructureAndAppearance(&commonAncestor);

        if (auto parentListNode = enclosingNodeOfType(start, isListItem)) {
            if (!editingIgnoresContent(*parentListNode) && VisibleSelection::selectionFromContentsOfNode(parentListNode.get()) == VisibleSelection(start, end)) {
                specialCommonAncestor = parentListNode->parentNode();
                while (specialCommonAncestor && !isListHTMLElement(specialCommonAncestor.get()))
                    specialCommonAncestor = specialCommonAncestor->parentNode();
            }
        }

        // Retain the Mail quote level by including all ancestor mail block quotes.
        if (RefPtr highestMailBlockquote = highestEnclosingNodeOfType(start, isMailBlockquote, CanCrossEditingBoundary))
            specialCommonAncestor = WTFMove(highestMailBlockquote);
    }

    RefPtr checkAncestor = specialCommonAncestor ? specialCommonAncestor : RefPtr { &commonAncestor };
    if (checkAncestor->renderer() && checkAncestor->renderer()->containingBlock()) {
        RefPtr newSpecialCommonAncestor = highestEnclosingNodeOfType(firstPositionInNode(checkAncestor.get()), &isElementPresentational, CanCrossEditingBoundary, checkAncestor->renderer()->containingBlock()->protectedElement().get());
        if (newSpecialCommonAncestor)
            specialCommonAncestor = WTFMove(newSpecialCommonAncestor);
    }

    // If a single tab is selected, commonAncestor will be a text node inside a tab span.
    // If two or more tabs are selected, commonAncestor will be the tab span.
    // In either case, if there is a specialCommonAncestor already, it will necessarily be above 
    // any tab span that needs to be included.
    if (!specialCommonAncestor && parentTabSpanNode(&commonAncestor))
        specialCommonAncestor = commonAncestor.parentNode();
    if (!specialCommonAncestor && tabSpanNode(&commonAncestor))
        specialCommonAncestor = &commonAncestor;

    if (RefPtr enclosingAnchor = enclosingElementWithTag(firstPositionInNode(specialCommonAncestor ? specialCommonAncestor.get() : &commonAncestor), aTag))
        specialCommonAncestor = WTFMove(enclosingAnchor);

    if (RefPtr enclosingPicture = enclosingElementWithTag(firstPositionInNode(specialCommonAncestor ? specialCommonAncestor.get() : &commonAncestor), pictureTag))
        specialCommonAncestor = WTFMove(enclosingPicture);

    return specialCommonAncestor;
}

static String serializePreservingVisualAppearanceInternal(const Position& start, const Position& end, Vector<Ref<Node>>* nodes, ResolveURLs resolveURLs, SerializeComposedTree serializeComposedTree, IgnoreUserSelectNone ignoreUserSelectNone,
    AnnotateForInterchange annotate, ConvertBlocksToInlines convertBlocksToInlines, StandardFontFamilySerializationMode standardFontFamilySerializationMode, MSOListMode msoListMode, PreserveBaseElement preserveBaseElement)
{
    static NeverDestroyed<const String> interchangeNewlineString { makeString("<br class=\""_s, AppleInterchangeNewline, "\">"_s) };

    if (!(start < end))
        return emptyString();

    RefPtr commonAncestor = commonInclusiveAncestor(start, end);
    if (!commonAncestor)
        return emptyString();

    Ref document = *start.document();
    document->updateLayoutIgnorePendingStylesheets();

    VisiblePosition visibleStart { start };
    VisiblePosition visibleEnd { end };

    RefPtr body = enclosingElementWithTag(firstPositionInNode(commonAncestor.get()), bodyTag);
    RefPtr<Element> fullySelectedRoot;
    // FIXME: Do this for all fully selected blocks, not just the body.
    if (body && VisiblePosition(firstPositionInNode(body.get())) == visibleStart && VisiblePosition(lastPositionInNode(body.get())) == visibleEnd)
        fullySelectedRoot = body;
    bool needsPositionStyleConversion = body && fullySelectedRoot == body && document->settings().shouldConvertPositionStyleOnCopy();

    RefPtr specialCommonAncestor = highestAncestorToWrapMarkup(start, end, *commonAncestor, annotate);

    StyledMarkupAccumulator accumulator(start, end, nodes, resolveURLs, serializeComposedTree, ignoreUserSelectNone, annotate, standardFontFamilySerializationMode, msoListMode, needsPositionStyleConversion, specialCommonAncestor.get());

    Position adjustedStart = start;

    if (RefPtr pictureElement = enclosingElementWithTag(adjustedStart, pictureTag))
        adjustedStart = firstPositionInNode(pictureElement.get());

    if (annotate == AnnotateForInterchange::Yes && needInterchangeNewlineAfter(visibleStart)) {
        if (visibleStart == visibleEnd.previous())
            return interchangeNewlineString;

        accumulator.append(interchangeNewlineString.get());
        adjustedStart = visibleStart.next().deepEquivalent();

        if (!(adjustedStart < end))
            return interchangeNewlineString;
    }

    RefPtr lastClosed = accumulator.serializeNodes(adjustedStart, end);

    if (specialCommonAncestor && lastClosed) {
        // Also include all of the ancestors of lastClosed up to this special ancestor.
        for (RefPtr ancestor = accumulator.parentNode(*lastClosed); ancestor; ancestor = accumulator.parentNode(*ancestor)) {
            if (ancestor == fullySelectedRoot && convertBlocksToInlines == ConvertBlocksToInlines::No) {
                RefPtr<EditingStyle> fullySelectedRootStyle = styleFromMatchedRulesAndInlineDecl(*fullySelectedRoot);

                // Bring the background attribute over, but not as an attribute because a background attribute on a div
                // appears to have no effect.
                if ((!fullySelectedRootStyle || !fullySelectedRootStyle->style() || !fullySelectedRootStyle->style()->getPropertyCSSValue(CSSPropertyBackgroundImage))
                    && fullySelectedRoot->hasAttributeWithoutSynchronization(backgroundAttr))
                    fullySelectedRootStyle->style()->setProperty(CSSPropertyBackgroundImage, makeString("url('"_s, fullySelectedRoot->getAttribute(backgroundAttr), "')"_s));

                if (fullySelectedRootStyle->style()) {
                    // Reset the CSS properties to avoid an assertion error in addStyleMarkup().
                    // This assertion is caused at least when we select all text of a <body> element whose
                    // 'text-decoration' property is "inherit", and copy it.
                    if (!propertyMissingOrEqualToNone(fullySelectedRootStyle->style(), CSSPropertyTextDecorationLine))
                        fullySelectedRootStyle->style()->setProperty(CSSPropertyTextDecorationLine, CSSValueNone);
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
                nodes->append(*ancestor);
            
            if (ancestor == specialCommonAncestor)
                break;
        }
    }
    
    if (accumulator.needRelativeStyleWrapper() && needsPositionStyleConversion) {
        if (accumulator.needClearingDiv())
            accumulator.append("<div style=\"clear: both;\"></div>"_s);
        RefPtr<EditingStyle> positionRelativeStyle = styleFromMatchedRulesAndInlineDecl(*body);
        positionRelativeStyle->style()->setProperty(CSSPropertyPosition, CSSValueRelative);
        accumulator.wrapWithStyleNode(positionRelativeStyle->style(), document, true);
    }

    // FIXME: The interchange newline should be placed in the block that it's in, not after all of the content, unconditionally.
    if (annotate == AnnotateForInterchange::Yes && needInterchangeNewlineAfter(visibleEnd.previous()))
        accumulator.append(interchangeNewlineString.get());

    RefPtr baseElement = preserveBaseElement == PreserveBaseElement::Yes ? document->firstBaseElement() : nullptr;
    accumulator.prependHeadIfNecessary(baseElement.get());

    return accumulator.takeResults();
}

String serializePreservingVisualAppearance(const SimpleRange& range, Vector<Ref<Node>>* nodes, AnnotateForInterchange annotate, ConvertBlocksToInlines convertBlocksToInlines, ResolveURLs resolveURLs)
{
    return serializePreservingVisualAppearanceInternal(makeDeprecatedLegacyPosition(range.start), makeDeprecatedLegacyPosition(range.end),
        nodes, resolveURLs, SerializeComposedTree::No, IgnoreUserSelectNone::No,
        annotate, convertBlocksToInlines, StandardFontFamilySerializationMode::Keep, MSOListMode::DoNotPreserve, PreserveBaseElement::No);
}

String serializePreservingVisualAppearance(const VisibleSelection& selection, ResolveURLs resolveURLs, SerializeComposedTree serializeComposedTree, IgnoreUserSelectNone ignoreUserSelectNone, PreserveBaseElement preserveBaseElement, Vector<Ref<Node>>* nodes)
{
    return serializePreservingVisualAppearanceInternal(selection.start(), selection.end(), nodes, resolveURLs, serializeComposedTree, ignoreUserSelectNone,
        AnnotateForInterchange::Yes, ConvertBlocksToInlines::No, StandardFontFamilySerializationMode::Keep, MSOListMode::DoNotPreserve, preserveBaseElement);
}

static bool shouldPreserveMSOLists(StringView markup)
{
    if (!markup.startsWith("<html xmlns:"_s))
        return false;
    auto tagClose = markup.find('>');
    if (tagClose == notFound)
        return false;
    auto tag = markup.left(tagClose);
    return tag.contains("xmlns:o=\"urn:schemas-microsoft-com:office:office\""_s)
        && tag.contains("xmlns:w=\"urn:schemas-microsoft-com:office:word\""_s);
}

String sanitizedMarkupForFragmentInDocument(Ref<DocumentFragment>&& fragment, Document& document, MSOListQuirks msoListQuirks, const String& originalMarkup)
{
    MSOListMode msoListMode = msoListQuirks == MSOListQuirks::CheckIfNeeded && shouldPreserveMSOLists(originalMarkup)
        ? MSOListMode::Preserve : MSOListMode::DoNotPreserve;

    RefPtr bodyElement { document.body() };
    ASSERT(bodyElement);
    bodyElement->appendChild(fragment.get());

    // SerializeComposedTree::No because there can't be a shadow tree in the pasted fragment.
    auto result = serializePreservingVisualAppearanceInternal(firstPositionInNode(bodyElement.get()), lastPositionInNode(bodyElement.get()), nullptr,
        ResolveURLs::YesExcludingURLsForPrivacy, SerializeComposedTree::No, IgnoreUserSelectNone::No, AnnotateForInterchange::Yes, ConvertBlocksToInlines::No, StandardFontFamilySerializationMode::Strip, msoListMode, PreserveBaseElement::No);

    if (msoListMode != MSOListMode::Preserve)
        return result;

    return makeString(
        "<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"\n"
        "xmlns:w=\"urn:schemas-microsoft-com:office:word\"\n"
        "xmlns:m=\"http://schemas.microsoft.com/office/2004/12/omml\"\n"
        "xmlns=\"http://www.w3.org/TR/REC-html40\">"_s,
        result,
        "</html>"_s);
}

static void restoreAttachmentElementsInFragment(DocumentFragment& fragment)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    if (!DeprecatedGlobalSettings::attachmentElementEnabled())
        return;

    RefPtr ownerDocument = fragment.ownerDocument();
    // When creating a fragment we must strip the webkit-attachment-path attribute after restoring the File object.
    Vector<Ref<HTMLAttachmentElement>> attachments;
    for (auto& attachment : descendantsOfType<HTMLAttachmentElement>(fragment))
        attachments.append(attachment);

    for (auto& attachment : attachments) {
        attachment->setUniqueIdentifier(attachment->attributeWithoutSynchronization(webkitattachmentidAttr));

        auto attachmentPath = attachment->attachmentPath();
        auto blobURL = attachment->blobURL();
        if (!attachmentPath.isEmpty())
            attachment->setFile(File::create(ownerDocument.get(), attachmentPath));
        else if (!blobURL.isEmpty())
            attachment->setFile(File::deserialize(ownerDocument.get(), { }, blobURL, attachment->attachmentType(), attachment->attachmentTitle()));

        // Remove temporary attributes that were previously added in StyledMarkupAccumulator::appendCustomAttributes.
        attachment->removeAttribute(webkitattachmentidAttr);
        attachment->removeAttribute(webkitattachmentpathAttr);
        attachment->removeAttribute(webkitattachmentbloburlAttr);
    }

    Vector<Ref<AttachmentAssociatedElement>> attachmentAssociatedElements;

    for (auto& image : descendantsOfType<HTMLImageElement>(fragment))
        attachmentAssociatedElements.append(image);

    for (auto& source : descendantsOfType<HTMLSourceElement>(fragment))
        attachmentAssociatedElements.append(source);

    for (auto& attachmentAssociatedElement : attachmentAssociatedElements) {
        Ref element = attachmentAssociatedElement->asHTMLElement();

        auto attachmentIdentifier = element->attributeWithoutSynchronization(webkitattachmentidAttr);
        if (attachmentIdentifier.isEmpty())
            continue;

        auto attachment = HTMLAttachmentElement::create(HTMLNames::attachmentTag, *ownerDocument);
        attachment->setUniqueIdentifier(attachmentIdentifier);
        attachmentAssociatedElement->setAttachmentElement(WTFMove(attachment));
        element->removeAttribute(webkitattachmentidAttr);
    }
#else
    UNUSED_PARAM(fragment);
#endif
}

Ref<DocumentFragment> createFragmentFromMarkup(Document& document, const String& markup, const String& baseURL, OptionSet<ParserContentPolicy> parserContentPolicy)
{
    // We use a fake body element here to trick the HTML parser into using the InBody insertion mode.
    auto fakeBody = HTMLBodyElement::create(document);
    auto fragment = DocumentFragment::create(document);

    fragment->parseHTML(markup, fakeBody, parserContentPolicy);
    restoreAttachmentElementsInFragment(fragment);
    if (!baseURL.isEmpty() && baseURL != aboutBlankURL() && baseURL != document.baseURL())
        completeURLs(fragment.ptr(), baseURL);

    return fragment;
}

String serializeFragment(const Node& node, SerializedNodes root, Vector<Ref<Node>>* nodes, ResolveURLs resolveURLs, std::optional<SerializationSyntax> serializationSyntax, SerializeShadowRoots serializeShadowRoots, Vector<Ref<ShadowRoot>>&& explicitShadowRoots, const Vector<MarkupExclusionRule>& exclusionRules)
{
    if (!serializationSyntax)
        serializationSyntax = node.document().isHTMLDocument() ? SerializationSyntax::HTML : SerializationSyntax::XML;

    MarkupAccumulator accumulator(nodes, resolveURLs, *serializationSyntax, serializeShadowRoots, WTFMove(explicitShadowRoots), exclusionRules);
    return accumulator.serializeNodes(const_cast<Node&>(node), root);
}

String serializeFragmentWithURLReplacement(const Node& node, SerializedNodes root, Vector<Ref<Node>>* nodes, ResolveURLs resolveURLs, std::optional<SerializationSyntax> serializationSyntax, UncheckedKeyHashMap<String, String>&& replacementURLStrings, UncheckedKeyHashMap<RefPtr<CSSStyleSheet>, String>&& replacementURLStringsForCSSStyleSheet, SerializeShadowRoots serializeShadowRoots, Vector<Ref<ShadowRoot>>&& explicitShadowRoots, const Vector<MarkupExclusionRule>& exclusionRules)
{
    if (!serializationSyntax)
        serializationSyntax = node.document().isHTMLDocument() ? SerializationSyntax::HTML : SerializationSyntax::XML;

    MarkupAccumulator accumulator(nodes, resolveURLs, *serializationSyntax, serializeShadowRoots, WTFMove(explicitShadowRoots), exclusionRules);
    accumulator.enableURLReplacement(WTFMove(replacementURLStrings), WTFMove(replacementURLStringsForCSSStyleSheet));
    return accumulator.serializeNodes(const_cast<Node&>(node), root);
}

static void fillContainerFromString(ContainerNode& paragraph, const String& string)
{
    Ref document = paragraph.document();

    if (string.isEmpty()) {
        paragraph.appendChild(createBlockPlaceholderElement(document));
        return;
    }

    ASSERT(string.find('\n') == notFound);

    Vector<String> tabList = string.splitAllowingEmptyEntries('\t');
    StringBuilder tabText;
    bool first = true;
    size_t numEntries = tabList.size();
    for (size_t i = 0; i < numEntries; ++i) {
        const String& s = tabList[i];

        // append the non-tab textual part
        if (!s.isEmpty()) {
            if (!tabText.isEmpty()) {
                paragraph.appendChild(createTabSpanElement(document, tabText.toString()));
                tabText.clear();
            }
            Ref textNode = document->createTextNode(stringWithRebalancedWhitespace(s, first, i + 1 == numEntries));
            paragraph.appendChild(textNode);
        }

        // there is a tab after every entry, except the last entry
        // (if the last character is a tab, the list gets an extra empty entry)
        if (i + 1 != numEntries)
            tabText.append('\t');
        else if (!tabText.isEmpty())
            paragraph.appendChild(createTabSpanElement(document, tabText.toString()));

        first = false;
    }
}

bool isPlainTextMarkup(Node* node)
{
    ASSERT(node);
    RefPtr element = dynamicDowncast<HTMLDivElement>(*node);
    if (!element || element->hasAttributes())
        return false;

    RefPtr firstChild = element->firstChild();
    if (!firstChild)
        return false;

    RefPtr secondChild = firstChild->nextSibling();
    if (!secondChild)
        return firstChild->isTextNode() || firstChild->firstChild();
    
    if (secondChild->nextSibling())
        return false;
    
    return parentTabSpanNode(firstChild->protectedFirstChild().get()) && is<Text>(secondChild);
}

static bool contextPreservesNewline(const SimpleRange& context)
{
    auto container = VisiblePosition(makeDeprecatedLegacyPosition(context.start)).deepEquivalent().protectedContainerNode();
    return container && container->renderer() && container->renderer()->style().preserveNewline();
}

Ref<DocumentFragment> createFragmentFromText(const SimpleRange& context, const String& text)
{
    Ref document = context.start.document();
    auto fragment = document->createDocumentFragment();
    
    if (text.isEmpty())
        return fragment;

    String string = makeStringBySimplifyingNewLines(text);

    auto createHTMLBRElement = [document]() {
        auto element = HTMLBRElement::create(document);
        element->setAttributeWithoutSynchronization(classAttr, AppleInterchangeNewline);
        return element;
    };

    if (contextPreservesNewline(context)) {
        bool endsWithNewLine = string.endsWith('\n');
        fragment->appendChild(document->createTextNode(WTFMove(string)));
        if (endsWithNewLine) {
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
    auto start = makeDeprecatedLegacyPosition(context.start);
    auto block = enclosingBlock(start.firstNode().get());
    bool useClonesOfEnclosingBlock = block
        && !block->hasTagName(bodyTag)
        && !block->hasTagName(htmlTag)
        // Avoid using table as paragraphs due to its special treatment in Position::upstream/downstream.
        && !isRenderedTable(block.get())
        && block != editableRootForPosition(start);
    bool useLineBreak = enclosingTextFormControl(start);

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
    RefPtr documentType = document.doctype();
    if (!documentType)
        return emptyString();
    return serializeFragment(*documentType, SerializedNodes::SubtreeIncludingNode);
}

String urlToMarkup(const URL& url, const String& title)
{
    StringBuilder markup;
    markup.append("<a href=\""_s, url.string(), "\">"_s);
    MarkupAccumulator::appendCharactersReplacingEntities(markup, title, 0, title.length(), EntityMaskInPCDATA);
    markup.append("</a>"_s);
    return markup.toString();
}

enum class DocumentFragmentMode : bool { New, ReuseForInnerOuterHTML };
static ALWAYS_INLINE ExceptionOr<Ref<DocumentFragment>> createFragmentForMarkup(Element& contextElement, const String& markup, DocumentFragmentMode mode, OptionSet<ParserContentPolicy> parserContentPolicy)
{
    Ref document = contextElement.hasTagName(templateTag) ? contextElement.document().ensureTemplateDocument() : contextElement.document();
    auto fragment = mode == DocumentFragmentMode::New ? DocumentFragment::create(document.get()) : document->documentFragmentForInnerOuterHTML();
    ASSERT(!fragment->hasChildNodes());
    if (document->isHTMLDocument() || parserContentPolicy.contains(ParserContentPolicy::AlwaysParseAsHTML)) {
        fragment->parseHTML(markup, contextElement, parserContentPolicy);
        return fragment;
    }

    bool wasValid = fragment->parseXML(markup, &contextElement, parserContentPolicy);
    if (!wasValid)
        return Exception { ExceptionCode::SyntaxError };
    return fragment;
}

ExceptionOr<Ref<DocumentFragment>> createFragmentForInnerOuterHTML(Element& contextElement, const String& markup, OptionSet<ParserContentPolicy> parserContentPolicy)
{
    return createFragmentForMarkup(contextElement, markup, DocumentFragmentMode::ReuseForInnerOuterHTML, parserContentPolicy);
}

RefPtr<DocumentFragment> createFragmentForTransformToFragment(Document& outputDoc, String&& sourceString, const String& sourceMIMEType)
{
    RefPtr<DocumentFragment> fragment = outputDoc.createDocumentFragment();
    
    if (sourceMIMEType == "text/html"_s) {
        // As far as I can tell, there isn't a spec for how transformToFragment is supposed to work.
        // Based on the documentation I can find, it looks like we want to start parsing the fragment in the InBody insertion mode.
        // Unfortunately, that's an implementation detail of the parser.
        // We achieve that effect here by passing in a fake body element as context for the fragment.
        auto fakeBody = HTMLBodyElement::create(outputDoc);
        fragment->parseHTML(WTFMove(sourceString), fakeBody, { ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::DoNotMarkAlreadyStarted });
    } else if (sourceMIMEType == textPlainContentTypeAtom())
        fragment->parserAppendChild(Text::create(outputDoc, WTFMove(sourceString)));
    else {
        bool successfulParse = fragment->parseXML(WTFMove(sourceString), nullptr, { ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::DoNotMarkAlreadyStarted });
        if (!successfulParse)
            return nullptr;
    }
    
    // FIXME: Do we need to mess with URLs here?
    
    return fragment;
}

Ref<DocumentFragment> createFragmentForImageAndURL(Document& document, const String& url, PresentationSize preferredSize)
{
    auto imageElement = HTMLImageElement::create(document);
    imageElement->setAttributeWithoutSynchronization(HTMLNames::srcAttr, AtomString { url });
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
    for (Ref element : childrenOfType<HTMLElement>(container)) {
        if (is<HTMLHtmlElement>(element)) {
            toRemove.append(element);
            collectElementsToRemoveFromFragment(WTFMove(element));
            continue;
        }
        if (is<HTMLHeadElement>(element) || is<HTMLBodyElement>(element))
            toRemove.append(WTFMove(element));
    }
    return toRemove;
}

static void removeElementFromFragmentPreservingChildren(DocumentFragment& fragment, HTMLElement& element)
{
    RefPtr<Node> nextChild;
    for (RefPtr child = element.firstChild(); child; child = nextChild) {
        nextChild = child->nextSibling();
        element.removeChild(*child);
        fragment.insertBefore(*child, &element);
    }
    fragment.removeChild(element);
}

ExceptionOr<Ref<DocumentFragment>> createContextualFragment(Element& element, const String& markup, OptionSet<ParserContentPolicy> parserContentPolicy)
{
    auto result = createFragmentForMarkup(element, markup, DocumentFragmentMode::New, parserContentPolicy);
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

static inline RefPtr<Text> singleTextChild(ContainerNode& node)
{
    return node.hasOneChild() ? dynamicDowncast<Text>(node.firstChild()) : nullptr;
}

static inline bool hasMutationEventListeners(const Document& document)
{
    return document.hasAnyListenerOfType({ Document::ListenerType::DOMSubtreeModified, Document::ListenerType::DOMNodeInserted,
        Document::ListenerType::DOMNodeRemoved, Document::ListenerType::DOMNodeRemovedFromDocument, Document::ListenerType::DOMCharacterDataModified });
}

// We can use setData instead of replacing Text node as long as script can't observe the difference.
static inline bool canUseSetDataOptimization(const Text& containerChild, const ChildListMutationScope& mutationScope)
{
    bool authorScriptMayHaveReference = containerChild.refCount();
    return !authorScriptMayHaveReference && !mutationScope.canObserve() && !hasMutationEventListeners(containerChild.protectedDocument());
}

ExceptionOr<void> replaceChildrenWithFragment(ContainerNode& container, Ref<DocumentFragment>&& fragment)
{
    Ref containerNode(container);
    ChildListMutationScope mutation(containerNode);

    if (!fragment->firstChild()) {
        containerNode->removeChildren();
        return { };
    }

    // We don't Use RefPtr here because canUseSetDataOptimization() below relies on the
    // containerChild's ref count.
    auto* containerChild = dynamicDowncast<Text>(containerNode->firstChild());
    if (containerChild && !containerChild->nextSibling()) {
        if (RefPtr fragmentChild = singleTextChild(fragment); fragmentChild && canUseSetDataOptimization(*containerChild, mutation)) {
            Ref { *containerChild }->setData(fragmentChild->data());
            return { };
        }
        return containerNode->replaceChild(fragment, Ref { *containerChild });
    }

    containerNode->removeChildren();
    auto result = containerNode->appendChild(fragment);
    ASSERT(!fragment->hasChildNodes());
    ASSERT(!fragment->wrapper());
    return result;
}

}
