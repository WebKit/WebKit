/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLTreeBuilder.h"

#include "DocumentFragment.h"
#include "HTMLDocument.h"
#include "HTMLDocumentParser.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLScriptElement.h"
#include "HTMLTableElement.h"
#include "JSCustomElementInterface.h"
#include "LocalizedStrings.h"
#include "NotImplemented.h"
#include "SVGScriptElement.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/unicode/CharacterNames.h>

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(IOS_FAMILY)
#include "TelephoneNumberDetector.h"
#endif

namespace WebCore {

using namespace HTMLNames;

CustomElementConstructionData::CustomElementConstructionData(Ref<JSCustomElementInterface>&& customElementInterface, const AtomString& name, Vector<Attribute>&& attributes)
    : elementInterface(WTFMove(customElementInterface))
    , name(name)
    , attributes(WTFMove(attributes))
{
}

CustomElementConstructionData::~CustomElementConstructionData() = default;

namespace {

inline bool isHTMLSpaceOrReplacementCharacter(UChar character)
{
    return isHTMLSpace(character) || character == replacementCharacter;
}

}

static inline TextPosition uninitializedPositionValue1()
{
    return TextPosition(OrdinalNumber::fromOneBasedInt(-1), OrdinalNumber());
}

static inline bool isAllWhitespace(const String& string)
{
    return string.isAllSpecialCharacters<isHTMLSpace>();
}

static inline bool isAllWhitespaceOrReplacementCharacters(const String& string)
{
    return string.isAllSpecialCharacters<isHTMLSpaceOrReplacementCharacter>();
}

static bool isNumberedHeaderTag(const AtomString& tagName)
{
    return tagName == h1Tag
        || tagName == h2Tag
        || tagName == h3Tag
        || tagName == h4Tag
        || tagName == h5Tag
        || tagName == h6Tag;
}

static bool isCaptionColOrColgroupTag(const AtomString& tagName)
{
    return tagName == captionTag || tagName == colTag || tagName == colgroupTag;
}

static bool isTableCellContextTag(const AtomString& tagName)
{
    return tagName == thTag || tagName == tdTag;
}

static bool isTableBodyContextTag(const AtomString& tagName)
{
    return tagName == tbodyTag || tagName == tfootTag || tagName == theadTag;
}

static bool isNonAnchorNonNobrFormattingTag(const AtomString& tagName)
{
    return tagName == bTag
        || tagName == bigTag
        || tagName == codeTag
        || tagName == emTag
        || tagName == fontTag
        || tagName == iTag
        || tagName == sTag
        || tagName == smallTag
        || tagName == strikeTag
        || tagName == strongTag
        || tagName == ttTag
        || tagName == uTag;
}

static bool isNonAnchorFormattingTag(const AtomString& tagName)
{
    return tagName == nobrTag || isNonAnchorNonNobrFormattingTag(tagName);
}

// https://html.spec.whatwg.org/multipage/syntax.html#formatting
bool HTMLConstructionSite::isFormattingTag(const AtomString& tagName)
{
    return tagName == aTag || isNonAnchorFormattingTag(tagName);
}

class HTMLTreeBuilder::ExternalCharacterTokenBuffer {
public:
    explicit ExternalCharacterTokenBuffer(AtomicHTMLToken& token)
        : m_text(token.characters(), token.charactersLength())
        , m_isAll8BitData(token.charactersIsAll8BitData())
    {
        ASSERT(!isEmpty());
    }

    explicit ExternalCharacterTokenBuffer(const String& string)
        : m_text(string)
        , m_isAll8BitData(m_text.is8Bit())
    {
        ASSERT(!isEmpty());
    }

    ~ExternalCharacterTokenBuffer()
    {
        ASSERT(isEmpty());
    }

    bool isEmpty() const { return m_text.isEmpty(); }

    bool isAll8BitData() const { return m_isAll8BitData; }

    void skipAtMostOneLeadingNewline()
    {
        ASSERT(!isEmpty());
        if (m_text[0] == '\n')
            m_text = m_text.substring(1);
    }

    void skipLeadingWhitespace()
    {
        skipLeading<isHTMLSpace>();
    }

    String takeLeadingWhitespace()
    {
        return takeLeading<isHTMLSpace>();
    }

    void skipLeadingNonWhitespace()
    {
        skipLeading<isNotHTMLSpace>();
    }

    String takeRemaining()
    {
        String result = makeString(m_text);
        m_text = StringView();
        return result;
    }

    void giveRemainingTo(StringBuilder& recipient)
    {
        recipient.append(m_text);
        m_text = StringView();
    }

    String takeRemainingWhitespace()
    {
        ASSERT(!isEmpty());
        Vector<LChar, 8> whitespace;
        do {
            UChar character = m_text[0];
            if (isHTMLSpace(character))
                whitespace.append(character);
            m_text = m_text.substring(1);
        } while (!m_text.isEmpty());

        // Returning the null string when there aren't any whitespace
        // characters is slightly cleaner semantically because we don't want
        // to insert a text node (as opposed to inserting an empty text node).
        if (whitespace.isEmpty())
            return String();

        return String::adopt(WTFMove(whitespace));
    }

private:
    template<bool characterPredicate(UChar)> void skipLeading()
    {
        ASSERT(!isEmpty());
        while (characterPredicate(m_text[0])) {
            m_text = m_text.substring(1);
            if (m_text.isEmpty())
                return;
        }
    }

    template<bool characterPredicate(UChar)> String takeLeading()
    {
        ASSERT(!isEmpty());
        StringView start = m_text;
        skipLeading<characterPredicate>();
        if (start.length() == m_text.length())
            return String();
        return makeString(start.substring(0, start.length() - m_text.length()));
    }

    String makeString(StringView stringView) const
    {
        if (stringView.is8Bit() || !isAll8BitData())
            return stringView.toString();
        return String::make8BitFrom16BitSource(stringView.characters16(), stringView.length());
    }

    StringView m_text;
    bool m_isAll8BitData;
};

inline bool HTMLTreeBuilder::isParsingTemplateContents() const
{
    return m_tree.openElements().hasTemplateInHTMLScope();
}

inline bool HTMLTreeBuilder::isParsingFragmentOrTemplateContents() const
{
    return isParsingFragment() || isParsingTemplateContents();
}

HTMLTreeBuilder::HTMLTreeBuilder(HTMLDocumentParser& parser, HTMLDocument& document, ParserContentPolicy parserContentPolicy, const HTMLParserOptions& options)
    : m_parser(parser)
    , m_options(options)
    , m_tree(document, parserContentPolicy, options.maximumDOMTreeDepth)
    , m_scriptToProcessStartPosition(uninitializedPositionValue1())
{
#if ASSERT_ENABLED
    m_destructionProhibited = false;
#endif
}

HTMLTreeBuilder::HTMLTreeBuilder(HTMLDocumentParser& parser, DocumentFragment& fragment, Element& contextElement, ParserContentPolicy parserContentPolicy, const HTMLParserOptions& options)
    : m_parser(parser)
    , m_options(options)
    , m_fragmentContext(fragment, contextElement)
    , m_tree(fragment, parserContentPolicy, options.maximumDOMTreeDepth)
    , m_scriptToProcessStartPosition(uninitializedPositionValue1())
{
    ASSERT(isMainThread());

    // https://html.spec.whatwg.org/multipage/syntax.html#parsing-html-fragments
    // For efficiency, we skip step 5 ("Let root be a new html element with no attributes") and instead use the DocumentFragment as a root node.
    m_tree.openElements().pushRootNode(HTMLStackItem::create(fragment));

    if (contextElement.hasTagName(templateTag))
        m_templateInsertionModes.append(InsertionMode::TemplateContents);

    resetInsertionModeAppropriately();

    m_tree.setForm(is<HTMLFormElement>(contextElement) ? &downcast<HTMLFormElement>(contextElement) : HTMLFormElement::findClosestFormAncestor(contextElement));

#if ASSERT_ENABLED
    m_destructionProhibited = false;
#endif
}

HTMLTreeBuilder::FragmentParsingContext::FragmentParsingContext()
{
}

HTMLTreeBuilder::FragmentParsingContext::FragmentParsingContext(DocumentFragment& fragment, Element& contextElement)
    : m_fragment(&fragment)
{
    ASSERT(!fragment.hasChildNodes());
    m_contextElementStackItem = HTMLStackItem::create(contextElement);
}

inline Element& HTMLTreeBuilder::FragmentParsingContext::contextElement() const
{
    return contextElementStackItem().element();
}

inline HTMLStackItem& HTMLTreeBuilder::FragmentParsingContext::contextElementStackItem() const
{
    ASSERT(m_fragment);
    return *m_contextElementStackItem;
}

RefPtr<ScriptElement> HTMLTreeBuilder::takeScriptToProcess(TextPosition& scriptStartPosition)
{
    ASSERT(!m_destroyed);

    if (!m_scriptToProcess)
        return nullptr;

    // Unpause ourselves, callers may pause us again when processing the script.
    // The HTML5 spec is written as though scripts are executed inside the tree builder.
    // We pause the parser to exit the tree builder, and then resume before running scripts.
    scriptStartPosition = m_scriptToProcessStartPosition;
    m_scriptToProcessStartPosition = uninitializedPositionValue1();
    return WTFMove(m_scriptToProcess);
}

void HTMLTreeBuilder::constructTree(AtomicHTMLToken&& token)
{
#if ASSERT_ENABLED
    ASSERT(!m_destroyed);
    ASSERT(!m_destructionProhibited);
    m_destructionProhibited = true;
#endif

    if (shouldProcessTokenInForeignContent(token))
        processTokenInForeignContent(WTFMove(token));
    else
        processToken(WTFMove(token));

    bool inForeignContent = !m_tree.isEmpty()
        && !isInHTMLNamespace(adjustedCurrentStackItem())
        && !HTMLElementStack::isHTMLIntegrationPoint(m_tree.currentStackItem())
        && !HTMLElementStack::isMathMLTextIntegrationPoint(m_tree.currentStackItem());

    m_parser.tokenizer().setForceNullCharacterReplacement(m_insertionMode == InsertionMode::Text || inForeignContent);
    m_parser.tokenizer().setShouldAllowCDATA(inForeignContent);

#if ASSERT_ENABLED
    m_destructionProhibited = false;
#endif

    m_tree.executeQueuedTasks();
    // The tree builder might have been destroyed as an indirect result of executing the queued tasks.
}

void HTMLTreeBuilder::processToken(AtomicHTMLToken&& token)
{
    switch (token.type()) {
    case HTMLToken::Uninitialized:
        ASSERT_NOT_REACHED();
        break;
    case HTMLToken::DOCTYPE:
        m_shouldSkipLeadingNewline = false;
        processDoctypeToken(WTFMove(token));
        break;
    case HTMLToken::StartTag:
        m_shouldSkipLeadingNewline = false;
        processStartTag(WTFMove(token));
        break;
    case HTMLToken::EndTag:
        m_shouldSkipLeadingNewline = false;
        processEndTag(WTFMove(token));
        break;
    case HTMLToken::Comment:
        m_shouldSkipLeadingNewline = false;
        processComment(WTFMove(token));
        return;
    case HTMLToken::Character:
        processCharacter(WTFMove(token));
        break;
    case HTMLToken::EndOfFile:
        m_shouldSkipLeadingNewline = false;
        processEndOfFile(WTFMove(token));
        break;
    }
}

void HTMLTreeBuilder::processDoctypeToken(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::DOCTYPE);
    if (m_insertionMode == InsertionMode::Initial) {
        m_tree.insertDoctype(WTFMove(token));
        m_insertionMode = InsertionMode::BeforeHTML;
        return;
    }
    if (m_insertionMode == InsertionMode::InTableText) {
        defaultForInTableText();
        processDoctypeToken(WTFMove(token));
        return;
    }
    parseError(token);
}

void HTMLTreeBuilder::processFakeStartTag(const QualifiedName& tagName, Vector<Attribute>&& attributes)
{
    // FIXME: We'll need a fancier conversion than just "localName" for SVG/MathML tags.
    AtomicHTMLToken fakeToken(HTMLToken::StartTag, tagName.localName(), WTFMove(attributes));
    processStartTag(WTFMove(fakeToken));
}

void HTMLTreeBuilder::processFakeEndTag(const AtomString& tagName)
{
    AtomicHTMLToken fakeToken(HTMLToken::EndTag, tagName);
    processEndTag(WTFMove(fakeToken));
}

void HTMLTreeBuilder::processFakeEndTag(const QualifiedName& tagName)
{
    // FIXME: We'll need a fancier conversion than just "localName" for SVG/MathML tags.
    processFakeEndTag(tagName.localName());
}

void HTMLTreeBuilder::processFakeCharacters(const String& characters)
{
    ASSERT(!characters.isEmpty());
    ExternalCharacterTokenBuffer buffer(characters);
    processCharacterBuffer(buffer);
}

void HTMLTreeBuilder::processFakePEndTagIfPInButtonScope()
{
    if (!m_tree.openElements().inButtonScope(pTag->localName()))
        return;
    AtomicHTMLToken endP(HTMLToken::EndTag, pTag->localName());
    processEndTag(WTFMove(endP));
}

namespace {

bool isLi(const HTMLStackItem& item)
{
    return item.hasTagName(liTag);
}

bool isDdOrDt(const HTMLStackItem& item)
{
    return item.hasTagName(ddTag) || item.hasTagName(dtTag);
}

}

template <bool shouldClose(const HTMLStackItem&)> void HTMLTreeBuilder::processCloseWhenNestedTag(AtomicHTMLToken&& token)
{
    m_framesetOk = false;
    for (auto* nodeRecord = &m_tree.openElements().topRecord(); ; nodeRecord = nodeRecord->next()) {
        HTMLStackItem& item = nodeRecord->stackItem();
        if (shouldClose(item)) {
            ASSERT(item.isElement());
            processFakeEndTag(item.localName());
            break;
        }
        if (isSpecialNode(item) && !item.hasTagName(addressTag) && !item.hasTagName(divTag) && !item.hasTagName(pTag))
            break;
    }
    processFakePEndTagIfPInButtonScope();
    m_tree.insertHTMLElement(WTFMove(token));
}

template <typename TableQualifiedName> static HashMap<AtomString, QualifiedName> createCaseMap(const TableQualifiedName* const names[], unsigned length)
{
    HashMap<AtomString, QualifiedName> map;
    for (unsigned i = 0; i < length; ++i) {
        const QualifiedName& name = *names[i];
        const AtomString& localName = name.localName();
        AtomString loweredLocalName = localName.convertToASCIILowercase();
        if (loweredLocalName != localName)
            map.add(loweredLocalName, name);
    }
    return map;
}

static void adjustSVGTagNameCase(AtomicHTMLToken& token)
{
    static NeverDestroyed<HashMap<AtomString, QualifiedName>> map = createCaseMap(SVGNames::getSVGTags(), SVGNames::SVGTagsCount);
    const QualifiedName& casedName = map.get().get(token.name());
    if (casedName.localName().isNull())
        return;
    token.setName(casedName.localName());
}

static inline void adjustAttributes(HashMap<AtomString, QualifiedName>& map, AtomicHTMLToken& token)
{
    for (auto& attribute : token.attributes()) {
        const QualifiedName& casedName = map.get(attribute.localName());
        if (!casedName.localName().isNull())
            attribute.parserSetName(casedName);
    }
}

template<const QualifiedName* const* attributesTable(), unsigned attributesTableLength> static void adjustAttributes(AtomicHTMLToken& token)
{
    static NeverDestroyed<HashMap<AtomString, QualifiedName>> map = createCaseMap(attributesTable(), attributesTableLength);
    adjustAttributes(map, token);
}

static inline void adjustSVGAttributes(AtomicHTMLToken& token)
{
    adjustAttributes<SVGNames::getSVGAttrs, SVGNames::SVGAttrsCount>(token);
}

static inline void adjustMathMLAttributes(AtomicHTMLToken& token)
{
    adjustAttributes<MathMLNames::getMathMLAttrs, MathMLNames::MathMLAttrsCount>(token);
}

static void addNamesWithPrefix(HashMap<AtomString, QualifiedName>& map, const AtomString& prefix, const QualifiedName* const names[], unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        const QualifiedName& name = *names[i];
        const AtomString& localName = name.localName();
        map.add(prefix + ':' + localName, QualifiedName(prefix, localName, name.namespaceURI()));
    }
}

static HashMap<AtomString, QualifiedName> createForeignAttributesMap()
{
    HashMap<AtomString, QualifiedName> map;

    AtomString xlinkName("xlink", AtomString::ConstructFromLiteral);
    addNamesWithPrefix(map, xlinkName, XLinkNames::getXLinkAttrs(), XLinkNames::XLinkAttrsCount);
    addNamesWithPrefix(map, xmlAtom(), XMLNames::getXMLAttrs(), XMLNames::XMLAttrsCount);

    map.add(WTF::xmlnsAtom(), XMLNSNames::xmlnsAttr);
    map.add("xmlns:xlink", QualifiedName(xmlnsAtom(), xlinkName, XMLNSNames::xmlnsNamespaceURI));

    return map;
}

static void adjustForeignAttributes(AtomicHTMLToken& token)
{
    static NeverDestroyed<HashMap<AtomString, QualifiedName>> map = createForeignAttributesMap();
    adjustAttributes(map, token);
}

void HTMLTreeBuilder::processStartTagForInBody(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    if (token.name() == htmlTag) {
        processHtmlStartTagForInBody(WTFMove(token));
        return;
    }
    if (token.name() == baseTag
        || token.name() == basefontTag
        || token.name() == bgsoundTag
        || token.name() == commandTag
        || token.name() == linkTag
        || token.name() == metaTag
        || token.name() == noframesTag
        || token.name() == scriptTag
        || token.name() == styleTag
        || token.name() == titleTag) {
        bool didProcess = processStartTagForInHead(WTFMove(token));
        ASSERT_UNUSED(didProcess, didProcess);
        return;
    }
    if (token.name() == bodyTag) {
        parseError(token);
        bool fragmentOrTemplateCase = !m_tree.openElements().secondElementIsHTMLBodyElement() || m_tree.openElements().hasOnlyOneElement()
            || m_tree.openElements().hasTemplateInHTMLScope();
        if (fragmentOrTemplateCase) {
            ASSERT(isParsingFragmentOrTemplateContents());
            return;
        }
        m_framesetOk = false;
        m_tree.insertHTMLBodyStartTagInBody(WTFMove(token));
        return;
    }
    if (token.name() == framesetTag) {
        parseError(token);
        if (!m_tree.openElements().secondElementIsHTMLBodyElement() || m_tree.openElements().hasOnlyOneElement()) {
            ASSERT(isParsingFragmentOrTemplateContents());
            return;
        }
        if (!m_framesetOk)
            return;
        m_tree.openElements().bodyElement().remove();
        m_tree.openElements().popUntil(m_tree.openElements().bodyElement());
        m_tree.openElements().popHTMLBodyElement();
        // Note: in the fragment case the root is a DocumentFragment instead of a proper html element which is a quirk / optimization in WebKit.
        ASSERT(!isParsingFragment() || is<DocumentFragment>(m_tree.openElements().topNode()));
        ASSERT(isParsingFragment() || &m_tree.openElements().top() == &m_tree.openElements().htmlElement());
        m_tree.insertHTMLElement(WTFMove(token));
        m_insertionMode = InsertionMode::InFrameset;
        return;
    }
    if (token.name() == addressTag
        || token.name() == articleTag
        || token.name() == asideTag
        || token.name() == blockquoteTag
        || token.name() == centerTag
        || token.name() == detailsTag
        || token.name() == dirTag
        || token.name() == divTag
        || token.name() == dlTag
        || token.name() == fieldsetTag
        || token.name() == figcaptionTag
        || token.name() == figureTag
        || token.name() == footerTag
        || token.name() == headerTag
        || token.name() == hgroupTag
        || token.name() == mainTag
        || token.name() == menuTag
        || token.name() == navTag
        || token.name() == olTag
        || token.name() == pTag
        || token.name() == sectionTag
        || token.name() == summaryTag
        || token.name() == ulTag) {
        processFakePEndTagIfPInButtonScope();
        m_tree.insertHTMLElement(WTFMove(token));
        return;
    }
    if (isNumberedHeaderTag(token.name())) {
        processFakePEndTagIfPInButtonScope();
        if (isNumberedHeaderElement(m_tree.currentStackItem())) {
            parseError(token);
            m_tree.openElements().pop();
        }
        m_tree.insertHTMLElement(WTFMove(token));
        return;
    }
    if (token.name() == preTag || token.name() == listingTag) {
        processFakePEndTagIfPInButtonScope();
        m_tree.insertHTMLElement(WTFMove(token));
        m_shouldSkipLeadingNewline = true;
        m_framesetOk = false;
        return;
    }
    if (token.name() == formTag) {
        if (m_tree.form() && !isParsingTemplateContents()) {
            parseError(token);
            return;
        }
        processFakePEndTagIfPInButtonScope();
        m_tree.insertHTMLFormElement(WTFMove(token));
        return;
    }
    if (token.name() == liTag) {
        processCloseWhenNestedTag<isLi>(WTFMove(token));
        return;
    }
    if (token.name() == ddTag || token.name() == dtTag) {
        processCloseWhenNestedTag<isDdOrDt>(WTFMove(token));
        return;
    }
    if (token.name() == plaintextTag) {
        processFakePEndTagIfPInButtonScope();
        m_tree.insertHTMLElement(WTFMove(token));
        m_parser.tokenizer().setPLAINTEXTState();
        return;
    }
    if (token.name() == buttonTag) {
        if (m_tree.openElements().inScope(buttonTag)) {
            parseError(token);
            processFakeEndTag(buttonTag);
            processStartTag(WTFMove(token)); // FIXME: Could we just fall through here?
            return;
        }
        m_tree.reconstructTheActiveFormattingElements();
        m_tree.insertHTMLElement(WTFMove(token));
        m_framesetOk = false;
        return;
    }
    if (token.name() == aTag) {
        RefPtr<Element> activeATag = m_tree.activeFormattingElements().closestElementInScopeWithName(aTag->localName());
        if (activeATag) {
            parseError(token);
            processFakeEndTag(aTag);
            m_tree.activeFormattingElements().remove(*activeATag);
            if (m_tree.openElements().contains(*activeATag))
                m_tree.openElements().remove(*activeATag);
        }
        m_tree.reconstructTheActiveFormattingElements();
        m_tree.insertFormattingElement(WTFMove(token));
        return;
    }
    if (isNonAnchorNonNobrFormattingTag(token.name())) {
        m_tree.reconstructTheActiveFormattingElements();
        m_tree.insertFormattingElement(WTFMove(token));
        return;
    }
    if (token.name() == nobrTag) {
        m_tree.reconstructTheActiveFormattingElements();
        if (m_tree.openElements().inScope(nobrTag)) {
            parseError(token);
            processFakeEndTag(nobrTag);
            m_tree.reconstructTheActiveFormattingElements();
        }
        m_tree.insertFormattingElement(WTFMove(token));
        return;
    }
    if (token.name() == appletTag || token.name() == embedTag || token.name() == objectTag) {
        if (!pluginContentIsAllowed(m_tree.parserContentPolicy()))
            return;
    }
    if (token.name() == appletTag || token.name() == marqueeTag || token.name() == objectTag) {
        m_tree.reconstructTheActiveFormattingElements();
        m_tree.insertHTMLElement(WTFMove(token));
        m_tree.activeFormattingElements().appendMarker();
        m_framesetOk = false;
        return;
    }
    if (token.name() == tableTag) {
        if (!m_tree.inQuirksMode() && m_tree.openElements().inButtonScope(pTag))
            processFakeEndTag(pTag);
        m_tree.insertHTMLElement(WTFMove(token));
        m_framesetOk = false;
        m_insertionMode = InsertionMode::InTable;
        return;
    }
    if (token.name() == imageTag) {
        parseError(token);
        // Apparently we're not supposed to ask.
        token.setName(imgTag->localName());
        // Note the fall through to the imgTag handling below!
    }
    if (token.name() == areaTag
        || token.name() == brTag
        || token.name() == embedTag
        || token.name() == imgTag
        || token.name() == keygenTag
        || token.name() == wbrTag) {
        m_tree.reconstructTheActiveFormattingElements();
        m_tree.insertSelfClosingHTMLElement(WTFMove(token));
        m_framesetOk = false;
        return;
    }
    if (token.name() == inputTag) {
        m_tree.reconstructTheActiveFormattingElements();
        auto* typeAttribute = findAttribute(token.attributes(), typeAttr);
        bool shouldClearFramesetOK = !typeAttribute || !equalLettersIgnoringASCIICase(typeAttribute->value(), "hidden");
        m_tree.insertSelfClosingHTMLElement(WTFMove(token));
        if (shouldClearFramesetOK)
            m_framesetOk = false;
        return;
    }
    if (token.name() == paramTag || token.name() == sourceTag || token.name() == trackTag) {
        m_tree.insertSelfClosingHTMLElement(WTFMove(token));
        return;
    }
    if (token.name() == hrTag) {
        processFakePEndTagIfPInButtonScope();
        m_tree.insertSelfClosingHTMLElement(WTFMove(token));
        m_framesetOk = false;
        return;
    }
    if (token.name() == textareaTag) {
        m_tree.insertHTMLElement(WTFMove(token));
        m_shouldSkipLeadingNewline = true;
        m_parser.tokenizer().setRCDATAState();
        m_originalInsertionMode = m_insertionMode;
        m_framesetOk = false;
        m_insertionMode = InsertionMode::Text;
        return;
    }
    if (token.name() == xmpTag) {
        processFakePEndTagIfPInButtonScope();
        m_tree.reconstructTheActiveFormattingElements();
        m_framesetOk = false;
        processGenericRawTextStartTag(WTFMove(token));
        return;
    }
    if (token.name() == iframeTag) {
        m_framesetOk = false;
        processGenericRawTextStartTag(WTFMove(token));
        return;
    }
    if (token.name() == noembedTag) {
        processGenericRawTextStartTag(WTFMove(token));
        return;
    }
    if (token.name() == noscriptTag && m_options.scriptingFlag) {
        processGenericRawTextStartTag(WTFMove(token));
        return;
    }
    if (token.name() == selectTag) {
        m_tree.reconstructTheActiveFormattingElements();
        m_tree.insertHTMLElement(WTFMove(token));
        m_framesetOk = false;
        if (m_insertionMode == InsertionMode::InTable
            || m_insertionMode == InsertionMode::InCaption
            || m_insertionMode == InsertionMode::InColumnGroup
            || m_insertionMode == InsertionMode::InTableBody
            || m_insertionMode == InsertionMode::InRow
            || m_insertionMode == InsertionMode::InCell)
            m_insertionMode = InsertionMode::InSelectInTable;
        else
            m_insertionMode = InsertionMode::InSelect;
        return;
    }
    if (token.name() == optgroupTag || token.name() == optionTag) {
        if (is<HTMLOptionElement>(m_tree.currentStackItem().node())) {
            AtomicHTMLToken endOption(HTMLToken::EndTag, optionTag->localName());
            processEndTag(WTFMove(endOption));
        }
        m_tree.reconstructTheActiveFormattingElements();
        m_tree.insertHTMLElement(WTFMove(token));
        return;
    }
    if (token.name() == rbTag || token.name() == rtcTag) {
        if (m_tree.openElements().inScope(rubyTag->localName())) {
            m_tree.generateImpliedEndTags();
            if (!m_tree.currentStackItem().hasTagName(rubyTag))
                parseError(token);
        }
        m_tree.insertHTMLElement(WTFMove(token));
        return;
    }
    if (token.name() == rtTag || token.name() == rpTag) {
        if (m_tree.openElements().inScope(rubyTag->localName())) {
            m_tree.generateImpliedEndTagsWithExclusion(rtcTag->localName());
            if (!m_tree.currentStackItem().hasTagName(rubyTag) && !m_tree.currentStackItem().hasTagName(rtcTag))
                parseError(token);
        }
        m_tree.insertHTMLElement(WTFMove(token));
        return;
    }
    if (token.name() == MathMLNames::mathTag->localName()) {
        m_tree.reconstructTheActiveFormattingElements();
        adjustMathMLAttributes(token);
        adjustForeignAttributes(token);
        m_tree.insertForeignElement(WTFMove(token), MathMLNames::mathmlNamespaceURI);
        return;
    }
    if (token.name() == SVGNames::svgTag->localName()) {
        m_tree.reconstructTheActiveFormattingElements();
        adjustSVGAttributes(token);
        adjustForeignAttributes(token);
        m_tree.insertForeignElement(WTFMove(token), SVGNames::svgNamespaceURI);
        return;
    }
    if (isCaptionColOrColgroupTag(token.name())
        || token.name() == frameTag
        || token.name() == headTag
        || isTableBodyContextTag(token.name())
        || isTableCellContextTag(token.name())
        || token.name() == trTag) {
        parseError(token);
        return;
    }
    if (token.name() == templateTag) {
        m_framesetOk = false;
        processTemplateStartTag(WTFMove(token));
        return;
    }
    m_tree.reconstructTheActiveFormattingElements();
    insertGenericHTMLElement(WTFMove(token));
}

inline void HTMLTreeBuilder::insertGenericHTMLElement(AtomicHTMLToken&& token)
{
    m_customElementToConstruct = m_tree.insertHTMLElementOrFindCustomElementInterface(WTFMove(token));
}

void HTMLTreeBuilder::didCreateCustomOrFallbackElement(Ref<Element>&& element, CustomElementConstructionData& data)
{
    m_tree.insertCustomElement(WTFMove(element), data.name, WTFMove(data.attributes));
}

void HTMLTreeBuilder::processTemplateStartTag(AtomicHTMLToken&& token)
{
    m_tree.activeFormattingElements().appendMarker();
    m_tree.insertHTMLElement(WTFMove(token));
    m_templateInsertionModes.append(InsertionMode::TemplateContents);
    m_insertionMode = InsertionMode::TemplateContents;
}

bool HTMLTreeBuilder::processTemplateEndTag(AtomicHTMLToken&& token)
{
    ASSERT(token.name() == templateTag->localName());
    if (!m_tree.openElements().hasTemplateInHTMLScope()) {
        ASSERT(m_templateInsertionModes.isEmpty() || (m_templateInsertionModes.size() == 1 && m_fragmentContext.contextElement().hasTagName(templateTag)));
        parseError(token);
        return false;
    }
    m_tree.generateImpliedEndTags();
    if (!m_tree.currentStackItem().hasTagName(templateTag))
        parseError(token);
    m_tree.openElements().popUntilPopped(templateTag);
    m_tree.activeFormattingElements().clearToLastMarker();
    m_templateInsertionModes.removeLast();
    resetInsertionModeAppropriately();
    return true;
}

bool HTMLTreeBuilder::processEndOfFileForInTemplateContents(AtomicHTMLToken&& token)
{
    AtomicHTMLToken endTemplate(HTMLToken::EndTag, templateTag->localName());
    if (!processTemplateEndTag(WTFMove(endTemplate)))
        return false;

    processEndOfFile(WTFMove(token));
    return true;
}

bool HTMLTreeBuilder::processColgroupEndTagForInColumnGroup()
{
    bool ignoreFakeEndTag = m_tree.currentIsRootNode() || m_tree.currentNode().hasTagName(templateTag);

    if (ignoreFakeEndTag) {
        ASSERT(isParsingFragmentOrTemplateContents());
        // FIXME: parse error
        return false;
    }
    m_tree.openElements().pop();
    m_insertionMode = InsertionMode::InTable;
    return true;
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#close-the-cell
void HTMLTreeBuilder::closeTheCell()
{
    ASSERT(m_insertionMode == InsertionMode::InCell);
    if (m_tree.openElements().inTableScope(tdTag)) {
        ASSERT(!m_tree.openElements().inTableScope(thTag));
        processFakeEndTag(tdTag);
        return;
    }
    ASSERT(m_tree.openElements().inTableScope(thTag));
    processFakeEndTag(thTag);
    ASSERT(m_insertionMode == InsertionMode::InRow);
}

void HTMLTreeBuilder::processStartTagForInTable(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    if (token.name() == captionTag) {
        m_tree.openElements().popUntilTableScopeMarker();
        m_tree.activeFormattingElements().appendMarker();
        m_tree.insertHTMLElement(WTFMove(token));
        m_insertionMode = InsertionMode::InCaption;
        return;
    }
    if (token.name() == colgroupTag) {
        m_tree.openElements().popUntilTableScopeMarker();
        m_tree.insertHTMLElement(WTFMove(token));
        m_insertionMode = InsertionMode::InColumnGroup;
        return;
    }
    if (token.name() == colTag) {
        processFakeStartTag(colgroupTag);
        ASSERT(m_insertionMode == InsertionMode::InColumnGroup);
        processStartTag(WTFMove(token));
        return;
    }
    if (isTableBodyContextTag(token.name())) {
        m_tree.openElements().popUntilTableScopeMarker();
        m_tree.insertHTMLElement(WTFMove(token));
        m_insertionMode = InsertionMode::InTableBody;
        return;
    }
    if (isTableCellContextTag(token.name()) || token.name() == trTag) {
        processFakeStartTag(tbodyTag);
        ASSERT(m_insertionMode == InsertionMode::InTableBody);
        processStartTag(WTFMove(token));
        return;
    }
    if (token.name() == tableTag) {
        parseError(token);
        if (!processTableEndTagForInTable()) {
            ASSERT(isParsingFragmentOrTemplateContents());
            return;
        }
        processStartTag(WTFMove(token));
        return;
    }
    if (token.name() == styleTag || token.name() == scriptTag) {
        processStartTagForInHead(WTFMove(token));
        return;
    }
    if (token.name() == inputTag) {
        auto* typeAttribute = findAttribute(token.attributes(), typeAttr);
        if (typeAttribute && equalLettersIgnoringASCIICase(typeAttribute->value(), "hidden")) {
            parseError(token);
            m_tree.insertSelfClosingHTMLElement(WTFMove(token));
            return;
        }
        // Fall through to "anything else" case.
    }
    if (token.name() == formTag) {
        parseError(token);
        if (m_tree.form() && !isParsingTemplateContents())
            return;
        m_tree.insertHTMLFormElement(WTFMove(token), true);
        m_tree.openElements().pop();
        return;
    }
    if (token.name() == templateTag) {
        processTemplateStartTag(WTFMove(token));
        return;
    }
    parseError(token);
    HTMLConstructionSite::RedirectToFosterParentGuard redirecter(m_tree);
    processStartTagForInBody(WTFMove(token));
}

void HTMLTreeBuilder::processStartTag(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    switch (m_insertionMode) {
    case InsertionMode::Initial:
        defaultForInitial();
        ASSERT(m_insertionMode == InsertionMode::BeforeHTML);
        FALLTHROUGH;
    case InsertionMode::BeforeHTML:
        if (token.name() == htmlTag) {
            m_tree.insertHTMLHtmlStartTagBeforeHTML(WTFMove(token));
            m_insertionMode = InsertionMode::BeforeHead;
            return;
        }
        defaultForBeforeHTML();
        ASSERT(m_insertionMode == InsertionMode::BeforeHead);
        FALLTHROUGH;
    case InsertionMode::BeforeHead:
        if (token.name() == htmlTag) {
            processHtmlStartTagForInBody(WTFMove(token));
            return;
        }
        if (token.name() == headTag) {
            m_tree.insertHTMLHeadElement(WTFMove(token));
            m_insertionMode = InsertionMode::InHead;
            return;
        }
        defaultForBeforeHead();
        ASSERT(m_insertionMode == InsertionMode::InHead);
        FALLTHROUGH;
    case InsertionMode::InHead:
        if (processStartTagForInHead(WTFMove(token)))
            return;
        defaultForInHead();
        ASSERT(m_insertionMode == InsertionMode::AfterHead);
        FALLTHROUGH;
    case InsertionMode::AfterHead:
        if (token.name() == htmlTag) {
            processHtmlStartTagForInBody(WTFMove(token));
            return;
        }
        if (token.name() == bodyTag) {
            m_framesetOk = false;
            m_tree.insertHTMLBodyElement(WTFMove(token));
            m_insertionMode = InsertionMode::InBody;
            return;
        }
        if (token.name() == framesetTag) {
            m_tree.insertHTMLElement(WTFMove(token));
            m_insertionMode = InsertionMode::InFrameset;
            return;
        }
        if (token.name() == baseTag
            || token.name() == basefontTag
            || token.name() == bgsoundTag
            || token.name() == linkTag
            || token.name() == metaTag
            || token.name() == noframesTag
            || token.name() == scriptTag
            || token.name() == styleTag
            || token.name() == templateTag
            || token.name() == titleTag) {
            parseError(token);
            ASSERT(m_tree.headStackItem());
            m_tree.openElements().pushHTMLHeadElement(*m_tree.headStackItem());
            processStartTagForInHead(WTFMove(token));
            m_tree.openElements().removeHTMLHeadElement(m_tree.head());
            return;
        }
        if (token.name() == headTag) {
            parseError(token);
            return;
        }
        defaultForAfterHead();
        ASSERT(m_insertionMode == InsertionMode::InBody);
        FALLTHROUGH;
    case InsertionMode::InBody:
        processStartTagForInBody(WTFMove(token));
        break;
    case InsertionMode::InTable:
        processStartTagForInTable(WTFMove(token));
        break;
    case InsertionMode::InCaption:
        if (isCaptionColOrColgroupTag(token.name())
            || isTableBodyContextTag(token.name())
            || isTableCellContextTag(token.name())
            || token.name() == trTag) {
            parseError(token);
            if (!processCaptionEndTagForInCaption()) {
                ASSERT(isParsingFragment());
                return;
            }
            processStartTag(WTFMove(token));
            return;
        }
        processStartTagForInBody(WTFMove(token));
        break;
    case InsertionMode::InColumnGroup:
        if (token.name() == htmlTag) {
            processHtmlStartTagForInBody(WTFMove(token));
            return;
        }
        if (token.name() == colTag) {
            m_tree.insertSelfClosingHTMLElement(WTFMove(token));
            return;
        }
        if (token.name() == templateTag) {
            processTemplateStartTag(WTFMove(token));
            return;
        }
        if (!processColgroupEndTagForInColumnGroup()) {
            ASSERT(isParsingFragmentOrTemplateContents());
            return;
        }
        processStartTag(WTFMove(token));
        break;
    case InsertionMode::InTableBody:
        if (token.name() == trTag) {
            m_tree.openElements().popUntilTableBodyScopeMarker(); // How is there ever anything to pop?
            m_tree.insertHTMLElement(WTFMove(token));
            m_insertionMode = InsertionMode::InRow;
            return;
        }
        if (isTableCellContextTag(token.name())) {
            parseError(token);
            processFakeStartTag(trTag);
            ASSERT(m_insertionMode == InsertionMode::InRow);
            processStartTag(WTFMove(token));
            return;
        }
        if (isCaptionColOrColgroupTag(token.name()) || isTableBodyContextTag(token.name())) {
            // FIXME: This is slow.
            if (!m_tree.openElements().inTableScope(tbodyTag) && !m_tree.openElements().inTableScope(theadTag) && !m_tree.openElements().inTableScope(tfootTag)) {
                ASSERT(isParsingFragmentOrTemplateContents());
                parseError(token);
                return;
            }
            m_tree.openElements().popUntilTableBodyScopeMarker();
            ASSERT(isTableBodyContextTag(m_tree.currentStackItem().localName()));
            processFakeEndTag(m_tree.currentStackItem().localName());
            processStartTag(WTFMove(token));
            return;
        }
        processStartTagForInTable(WTFMove(token));
        break;
    case InsertionMode::InRow:
        if (isTableCellContextTag(token.name())) {
            m_tree.openElements().popUntilTableRowScopeMarker();
            m_tree.insertHTMLElement(WTFMove(token));
            m_insertionMode = InsertionMode::InCell;
            m_tree.activeFormattingElements().appendMarker();
            return;
        }
        if (token.name() == trTag
            || isCaptionColOrColgroupTag(token.name())
            || isTableBodyContextTag(token.name())) {
            if (!processTrEndTagForInRow()) {
                ASSERT(isParsingFragmentOrTemplateContents());
                return;
            }
            ASSERT(m_insertionMode == InsertionMode::InTableBody);
            processStartTag(WTFMove(token));
            return;
        }
        processStartTagForInTable(WTFMove(token));
        break;
    case InsertionMode::InCell:
        if (isCaptionColOrColgroupTag(token.name())
            || isTableCellContextTag(token.name())
            || token.name() == trTag
            || isTableBodyContextTag(token.name())) {
            // FIXME: This could be more efficient.
            if (!m_tree.openElements().inTableScope(tdTag) && !m_tree.openElements().inTableScope(thTag)) {
                ASSERT(isParsingFragment());
                parseError(token);
                return;
            }
            closeTheCell();
            processStartTag(WTFMove(token));
            return;
        }
        processStartTagForInBody(WTFMove(token));
        break;
    case InsertionMode::AfterBody:
    case InsertionMode::AfterAfterBody:
        if (token.name() == htmlTag) {
            processHtmlStartTagForInBody(WTFMove(token));
            return;
        }
        m_insertionMode = InsertionMode::InBody;
        processStartTag(WTFMove(token));
        break;
    case InsertionMode::InHeadNoscript:
        if (token.name() == htmlTag) {
            processHtmlStartTagForInBody(WTFMove(token));
            return;
        }
        if (token.name() == basefontTag
            || token.name() == bgsoundTag
            || token.name() == linkTag
            || token.name() == metaTag
            || token.name() == noframesTag
            || token.name() == styleTag) {
            bool didProcess = processStartTagForInHead(WTFMove(token));
            ASSERT_UNUSED(didProcess, didProcess);
            return;
        }
        if (token.name() == htmlTag || token.name() == noscriptTag) {
            parseError(token);
            return;
        }
        defaultForInHeadNoscript();
        processToken(WTFMove(token));
        break;
    case InsertionMode::InFrameset:
        if (token.name() == htmlTag) {
            processHtmlStartTagForInBody(WTFMove(token));
            return;
        }
        if (token.name() == framesetTag) {
            m_tree.insertHTMLElement(WTFMove(token));
            return;
        }
        if (token.name() == frameTag) {
            m_tree.insertSelfClosingHTMLElement(WTFMove(token));
            return;
        }
        if (token.name() == noframesTag) {
            processStartTagForInHead(WTFMove(token));
            return;
        }
        parseError(token);
        break;
    case InsertionMode::AfterFrameset:
    case InsertionMode::AfterAfterFrameset:
        if (token.name() == htmlTag) {
            processHtmlStartTagForInBody(WTFMove(token));
            return;
        }
        if (token.name() == noframesTag) {
            processStartTagForInHead(WTFMove(token));
            return;
        }
        parseError(token);
        break;
    case InsertionMode::InSelectInTable:
        if (token.name() == captionTag
            || token.name() == tableTag
            || isTableBodyContextTag(token.name())
            || token.name() == trTag
            || isTableCellContextTag(token.name())) {
            parseError(token);
            AtomicHTMLToken endSelect(HTMLToken::EndTag, selectTag->localName());
            processEndTag(WTFMove(endSelect));
            processStartTag(WTFMove(token));
            return;
        }
        FALLTHROUGH;
    case InsertionMode::InSelect:
        if (token.name() == htmlTag) {
            processHtmlStartTagForInBody(WTFMove(token));
            return;
        }
        if (token.name() == optionTag) {
            if (is<HTMLOptionElement>(m_tree.currentStackItem().node())) {
                AtomicHTMLToken endOption(HTMLToken::EndTag, optionTag->localName());
                processEndTag(WTFMove(endOption));
            }
            m_tree.insertHTMLElement(WTFMove(token));
            return;
        }
        if (token.name() == optgroupTag) {
            if (is<HTMLOptionElement>(m_tree.currentStackItem().node())) {
                AtomicHTMLToken endOption(HTMLToken::EndTag, optionTag->localName());
                processEndTag(WTFMove(endOption));
            }
            if (is<HTMLOptGroupElement>(m_tree.currentStackItem().node())) {
                AtomicHTMLToken endOptgroup(HTMLToken::EndTag, optgroupTag->localName());
                processEndTag(WTFMove(endOptgroup));
            }
            m_tree.insertHTMLElement(WTFMove(token));
            return;
        }
        if (token.name() == selectTag) {
            parseError(token);
            AtomicHTMLToken endSelect(HTMLToken::EndTag, selectTag->localName());
            processEndTag(WTFMove(endSelect));
            return;
        }
        if (token.name() == inputTag || token.name() == keygenTag || token.name() == textareaTag) {
            parseError(token);
            if (!m_tree.openElements().inSelectScope(selectTag)) {
                ASSERT(isParsingFragment());
                return;
            }
            AtomicHTMLToken endSelect(HTMLToken::EndTag, selectTag->localName());
            processEndTag(WTFMove(endSelect));
            processStartTag(WTFMove(token));
            return;
        }
        if (token.name() == scriptTag) {
            bool didProcess = processStartTagForInHead(WTFMove(token));
            ASSERT_UNUSED(didProcess, didProcess);
            return;
        }
        if (token.name() == templateTag) {
            processTemplateStartTag(WTFMove(token));
            return;
        }
        break;
    case InsertionMode::InTableText:
        defaultForInTableText();
        processStartTag(WTFMove(token));
        break;
    case InsertionMode::Text:
        ASSERT_NOT_REACHED();
        break;
    case InsertionMode::TemplateContents:
        if (token.name() == templateTag) {
            processTemplateStartTag(WTFMove(token));
            return;
        }

        if (token.name() == linkTag
            || token.name() == scriptTag
            || token.name() == styleTag
            || token.name() == metaTag) {
            processStartTagForInHead(WTFMove(token));
            return;
        }

        InsertionMode insertionMode = InsertionMode::TemplateContents;
        if (token.name() == colTag)
            insertionMode = InsertionMode::InColumnGroup;
        else if (isCaptionColOrColgroupTag(token.name()) || isTableBodyContextTag(token.name()))
            insertionMode = InsertionMode::InTable;
        else if (token.name() == trTag)
            insertionMode = InsertionMode::InTableBody;
        else if (isTableCellContextTag(token.name()))
            insertionMode = InsertionMode::InRow;
        else
            insertionMode = InsertionMode::InBody;

        ASSERT(insertionMode != InsertionMode::TemplateContents);
        ASSERT(m_templateInsertionModes.last() == InsertionMode::TemplateContents);
        m_templateInsertionModes.last() = insertionMode;
        m_insertionMode = insertionMode;

        processStartTag(WTFMove(token));
        break;
    }
}

void HTMLTreeBuilder::processHtmlStartTagForInBody(AtomicHTMLToken&& token)
{
    parseError(token);
    if (m_tree.openElements().hasTemplateInHTMLScope()) {
        ASSERT(isParsingTemplateContents());
        return;
    }
    m_tree.insertHTMLHtmlStartTagInBody(WTFMove(token));
}

bool HTMLTreeBuilder::processBodyEndTagForInBody(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::EndTag);
    ASSERT(token.name() == bodyTag);
    if (!m_tree.openElements().inScope(bodyTag->localName())) {
        parseError(token);
        return false;
    }
    notImplemented(); // Emit a more specific parse error based on stack contents.
    m_insertionMode = InsertionMode::AfterBody;
    return true;
}

void HTMLTreeBuilder::processAnyOtherEndTagForInBody(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::EndTag);
    for (auto* record = &m_tree.openElements().topRecord(); ; record = record->next()) {
        HTMLStackItem& item = record->stackItem();
        if (item.matchesHTMLTag(token.name())) {
            m_tree.generateImpliedEndTagsWithExclusion(token.name());
            if (!m_tree.currentStackItem().matchesHTMLTag(token.name()))
                parseError(token);
            m_tree.openElements().popUntilPopped(item.element());
            return;
        }
        if (isSpecialNode(item)) {
            parseError(token);
            return;
        }
    }
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
void HTMLTreeBuilder::callTheAdoptionAgency(AtomicHTMLToken& token)
{
    // The adoption agency algorithm is N^2. We limit the number of iterations
    // to stop from hanging the whole browser. This limit is specified in the
    // adoption agency algorithm: 
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#parsing-main-inbody
    static const int outerIterationLimit = 8;
    static const int innerIterationLimit = 3;

    // 1, 2, 3 and 16 are covered by the for() loop.
    for (int i = 0; i < outerIterationLimit; ++i) {
        // 4.
        RefPtr<Element> formattingElement = m_tree.activeFormattingElements().closestElementInScopeWithName(token.name());
        // 4.a
        if (!formattingElement)
            return processAnyOtherEndTagForInBody(WTFMove(token));
        // 4.c
        if ((m_tree.openElements().contains(*formattingElement)) && !m_tree.openElements().inScope(*formattingElement)) {
            parseError(token);
            notImplemented(); // Check the stack of open elements for a more specific parse error.
            return;
        }
        // 4.b
        auto* formattingElementRecord = m_tree.openElements().find(*formattingElement);
        if (!formattingElementRecord) {
            parseError(token);
            m_tree.activeFormattingElements().remove(*formattingElement);
            return;
        }
        // 4.d
        if (formattingElement != &m_tree.currentElement())
            parseError(token);
        // 5.
        auto* furthestBlock = m_tree.openElements().furthestBlockForFormattingElement(*formattingElement);
        // 6.
        if (!furthestBlock) {
            m_tree.openElements().popUntilPopped(*formattingElement);
            m_tree.activeFormattingElements().remove(*formattingElement);
            return;
        }
        // 7.
        ASSERT(furthestBlock->isAbove(*formattingElementRecord));
        Ref<HTMLStackItem> commonAncestor = formattingElementRecord->next()->stackItem();
        // 8.
        HTMLFormattingElementList::Bookmark bookmark = m_tree.activeFormattingElements().bookmarkFor(*formattingElement);
        // 9.
        auto* node = furthestBlock;
        auto* nextNode = node->next();
        auto* lastNode = furthestBlock;
        // 9.1, 9.2, 9.3 and 9.11 are covered by the for() loop.
        for (int i = 0; i < innerIterationLimit; ++i) {
            // 9.4
            node = nextNode;
            ASSERT(node);
            nextNode = node->next(); // Save node->next() for the next iteration in case node is deleted in 9.5.
            // 9.5
            if (!m_tree.activeFormattingElements().contains(node->element())) {
                m_tree.openElements().remove(node->element());
                node = 0;
                continue;
            }
            // 9.6
            if (node == formattingElementRecord)
                break;
            // 9.7
            auto newItem = m_tree.createElementFromSavedToken(node->stackItem());

            HTMLFormattingElementList::Entry* nodeEntry = m_tree.activeFormattingElements().find(node->element());
            nodeEntry->replaceElement(newItem.copyRef());
            node->replaceElement(WTFMove(newItem));

            // 9.8
            if (lastNode == furthestBlock)
                bookmark.moveToAfter(*nodeEntry);
            // 9.9
            m_tree.reparent(*node, *lastNode);
            // 9.10
            lastNode = node;
        }
        // 10.
        m_tree.insertAlreadyParsedChild(commonAncestor.get(), *lastNode);
        // 11.
        auto newItem = m_tree.createElementFromSavedToken(formattingElementRecord->stackItem());
        // 12. & 13.
        m_tree.takeAllChildrenAndReparent(newItem, *furthestBlock);
        // 14.
        m_tree.activeFormattingElements().swapTo(*formattingElement, newItem.copyRef(), bookmark);
        // 15.
        m_tree.openElements().remove(*formattingElement);
        m_tree.openElements().insertAbove(WTFMove(newItem), *furthestBlock);
    }
}

void HTMLTreeBuilder::resetInsertionModeAppropriately()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#reset-the-insertion-mode-appropriately
    bool last = false;
    for (auto* record = &m_tree.openElements().topRecord(); ; record = record->next()) {
        RefPtr<HTMLStackItem> item = &record->stackItem();
        if (&item->node() == &m_tree.openElements().rootNode()) {
            last = true;
            bool shouldCreateItem = isParsingFragment();
            if (shouldCreateItem)
                item = &m_fragmentContext.contextElementStackItem();
        }

        if (item->hasTagName(templateTag)) {
            m_insertionMode = m_templateInsertionModes.last();
            return;
        }

        if (item->hasTagName(selectTag)) {
            if (!last) {
                while (&item->node() != &m_tree.openElements().rootNode() && !item->hasTagName(templateTag)) {
                    record = record->next();
                    item = &record->stackItem();
                    if (is<HTMLTableElement>(item->node())) {
                        m_insertionMode = InsertionMode::InSelectInTable;
                        return;
                    }
                }
            }
            m_insertionMode = InsertionMode::InSelect;
            return;
        }
        if (item->hasTagName(tdTag) || item->hasTagName(thTag)) {
            m_insertionMode = InsertionMode::InCell;
            return;
        }
        if (item->hasTagName(trTag)) {
            m_insertionMode = InsertionMode::InRow;
            return;
        }
        if (item->hasTagName(tbodyTag) || item->hasTagName(theadTag) || item->hasTagName(tfootTag)) {
            m_insertionMode = InsertionMode::InTableBody;
            return;
        }
        if (item->hasTagName(captionTag)) {
            m_insertionMode = InsertionMode::InCaption;
            return;
        }
        if (item->hasTagName(colgroupTag)) {
            m_insertionMode = InsertionMode::InColumnGroup;
            return;
        }
        if (is<HTMLTableElement>(item->node())) {
            m_insertionMode = InsertionMode::InTable;
            return;
        }
        if (item->hasTagName(headTag)) {
            if (!m_fragmentContext.fragment() || &m_fragmentContext.contextElement() != &item->node()) {
                m_insertionMode = InsertionMode::InHead;
                return;
            }
            m_insertionMode = InsertionMode::InBody;
            return;
        }
        if (item->hasTagName(bodyTag)) {
            m_insertionMode = InsertionMode::InBody;
            return;
        }
        if (item->hasTagName(framesetTag)) {
            m_insertionMode = InsertionMode::InFrameset;
            return;
        }
        if (item->hasTagName(htmlTag)) {
            if (m_tree.headStackItem()) {
                m_insertionMode = InsertionMode::AfterHead;
                return;
            }
            ASSERT(isParsingFragment());
            m_insertionMode = InsertionMode::BeforeHead;
            return;
        }
        if (last) {
            ASSERT(isParsingFragment());
            m_insertionMode = InsertionMode::InBody;
            return;
        }
    }
}

void HTMLTreeBuilder::processEndTagForInTableBody(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::EndTag);
    if (isTableBodyContextTag(token.name())) {
        if (!m_tree.openElements().inTableScope(token.name())) {
            parseError(token);
            return;
        }
        m_tree.openElements().popUntilTableBodyScopeMarker();
        m_tree.openElements().pop();
        m_insertionMode = InsertionMode::InTable;
        return;
    }
    if (token.name() == tableTag) {
        // FIXME: This is slow.
        if (!m_tree.openElements().inTableScope(tbodyTag) && !m_tree.openElements().inTableScope(theadTag) && !m_tree.openElements().inTableScope(tfootTag)) {
            ASSERT(isParsingFragmentOrTemplateContents());
            parseError(token);
            return;
        }
        m_tree.openElements().popUntilTableBodyScopeMarker();
        ASSERT(isTableBodyContextTag(m_tree.currentStackItem().localName()));
        processFakeEndTag(m_tree.currentStackItem().localName());
        processEndTag(WTFMove(token));
        return;
    }
    if (token.name() == bodyTag
        || isCaptionColOrColgroupTag(token.name())
        || token.name() == htmlTag
        || isTableCellContextTag(token.name())
        || token.name() == trTag) {
        parseError(token);
        return;
    }
    processEndTagForInTable(WTFMove(token));
}

void HTMLTreeBuilder::processEndTagForInRow(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::EndTag);
    if (token.name() == trTag) {
        processTrEndTagForInRow();
        return;
    }
    if (token.name() == tableTag) {
        if (!processTrEndTagForInRow()) {
            ASSERT(isParsingFragmentOrTemplateContents());
            return;
        }
        ASSERT(m_insertionMode == InsertionMode::InTableBody);
        processEndTag(WTFMove(token));
        return;
    }
    if (isTableBodyContextTag(token.name())) {
        if (!m_tree.openElements().inTableScope(token.name())) {
            parseError(token);
            return;
        }
        processFakeEndTag(trTag);
        ASSERT(m_insertionMode == InsertionMode::InTableBody);
        processEndTag(WTFMove(token));
        return;
    }
    if (token.name() == bodyTag
        || isCaptionColOrColgroupTag(token.name())
        || token.name() == htmlTag
        || isTableCellContextTag(token.name())) {
        parseError(token);
        return;
    }
    processEndTagForInTable(WTFMove(token));
}

void HTMLTreeBuilder::processEndTagForInCell(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::EndTag);
    if (isTableCellContextTag(token.name())) {
        if (!m_tree.openElements().inTableScope(token.name())) {
            parseError(token);
            return;
        }
        m_tree.generateImpliedEndTags();
        if (!m_tree.currentStackItem().matchesHTMLTag(token.name()))
            parseError(token);
        m_tree.openElements().popUntilPopped(token.name());
        m_tree.activeFormattingElements().clearToLastMarker();
        m_insertionMode = InsertionMode::InRow;
        return;
    }
    if (token.name() == bodyTag
        || isCaptionColOrColgroupTag(token.name())
        || token.name() == htmlTag) {
        parseError(token);
        return;
    }
    if (token.name() == tableTag
        || token.name() == trTag
        || isTableBodyContextTag(token.name())) {
        if (!m_tree.openElements().inTableScope(token.name())) {
            ASSERT(isTableBodyContextTag(token.name()) || m_tree.openElements().inTableScope(templateTag) || isParsingFragment());
            parseError(token);
            return;
        }
        closeTheCell();
        processEndTag(WTFMove(token));
        return;
    }
    processEndTagForInBody(WTFMove(token));
}

void HTMLTreeBuilder::processEndTagForInBody(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::EndTag);
    if (token.name() == bodyTag) {
        processBodyEndTagForInBody(WTFMove(token));
        return;
    }
    if (token.name() == htmlTag) {
        AtomicHTMLToken endBody(HTMLToken::EndTag, bodyTag->localName());
        if (processBodyEndTagForInBody(WTFMove(endBody)))
            processEndTag(WTFMove(token));
        return;
    }
    if (token.name() == addressTag
        || token.name() == articleTag
        || token.name() == asideTag
        || token.name() == blockquoteTag
        || token.name() == buttonTag
        || token.name() == centerTag
        || token.name() == detailsTag
        || token.name() == dirTag
        || token.name() == divTag
        || token.name() == dlTag
        || token.name() == fieldsetTag
        || token.name() == figcaptionTag
        || token.name() == figureTag
        || token.name() == footerTag
        || token.name() == headerTag
        || token.name() == hgroupTag
        || token.name() == listingTag
        || token.name() == mainTag
        || token.name() == menuTag
        || token.name() == navTag
        || token.name() == olTag
        || token.name() == preTag
        || token.name() == sectionTag
        || token.name() == summaryTag
        || token.name() == ulTag) {
        if (!m_tree.openElements().inScope(token.name())) {
            parseError(token);
            return;
        }
        m_tree.generateImpliedEndTags();
        if (!m_tree.currentStackItem().matchesHTMLTag(token.name()))
            parseError(token);
        m_tree.openElements().popUntilPopped(token.name());
        return;
    }
    if (token.name() == formTag) {
        if (!isParsingTemplateContents()) {
            RefPtr<Element> formElement = m_tree.takeForm();
            if (!formElement || !m_tree.openElements().inScope(*formElement)) {
                parseError(token);
                return;
            }
            m_tree.generateImpliedEndTags();
            if (&m_tree.currentNode() != formElement.get())
                parseError(token);
            m_tree.openElements().remove(*formElement);
        } else {
            if (!m_tree.openElements().inScope(token.name())) {
                parseError(token);
                return;
            }
            m_tree.generateImpliedEndTags();
            if (!m_tree.currentNode().hasTagName(formTag))
                parseError(token);
            m_tree.openElements().popUntilPopped(token.name());
        }
    }
    if (token.name() == pTag) {
        if (!m_tree.openElements().inButtonScope(token.name())) {
            parseError(token);
            processFakeStartTag(pTag);
            ASSERT(m_tree.openElements().inScope(token.name()));
            processEndTag(WTFMove(token));
            return;
        }
        m_tree.generateImpliedEndTagsWithExclusion(token.name());
        if (!m_tree.currentStackItem().matchesHTMLTag(token.name()))
            parseError(token);
        m_tree.openElements().popUntilPopped(token.name());
        return;
    }
    if (token.name() == liTag) {
        if (!m_tree.openElements().inListItemScope(token.name())) {
            parseError(token);
            return;
        }
        m_tree.generateImpliedEndTagsWithExclusion(token.name());
        if (!m_tree.currentStackItem().matchesHTMLTag(token.name()))
            parseError(token);
        m_tree.openElements().popUntilPopped(token.name());
        return;
    }
    if (token.name() == ddTag || token.name() == dtTag) {
        if (!m_tree.openElements().inScope(token.name())) {
            parseError(token);
            return;
        }
        m_tree.generateImpliedEndTagsWithExclusion(token.name());
        if (!m_tree.currentStackItem().matchesHTMLTag(token.name()))
            parseError(token);
        m_tree.openElements().popUntilPopped(token.name());
        return;
    }
    if (isNumberedHeaderTag(token.name())) {
        if (!m_tree.openElements().hasNumberedHeaderElementInScope()) {
            parseError(token);
            return;
        }
        m_tree.generateImpliedEndTags();
        if (!m_tree.currentStackItem().matchesHTMLTag(token.name()))
            parseError(token);
        m_tree.openElements().popUntilNumberedHeaderElementPopped();
        return;
    }
    if (HTMLConstructionSite::isFormattingTag(token.name())) {
        callTheAdoptionAgency(token);
        return;
    }
    if (token.name() == appletTag || token.name() == marqueeTag || token.name() == objectTag) {
        if (!m_tree.openElements().inScope(token.name())) {
            parseError(token);
            return;
        }
        m_tree.generateImpliedEndTags();
        if (!m_tree.currentStackItem().matchesHTMLTag(token.name()))
            parseError(token);
        m_tree.openElements().popUntilPopped(token.name());
        m_tree.activeFormattingElements().clearToLastMarker();
        return;
    }
    if (token.name() == brTag) {
        parseError(token);
        processFakeStartTag(brTag);
        return;
    }
    if (token.name() == templateTag) {
        processTemplateEndTag(WTFMove(token));
        return;
    }
    processAnyOtherEndTagForInBody(WTFMove(token));
}

bool HTMLTreeBuilder::processCaptionEndTagForInCaption()
{
    if (!m_tree.openElements().inTableScope(captionTag->localName())) {
        ASSERT(isParsingFragment());
        // FIXME: parse error
        return false;
    }
    m_tree.generateImpliedEndTags();
    // FIXME: parse error if (!m_tree.currentStackItem().hasTagName(captionTag))
    m_tree.openElements().popUntilPopped(captionTag->localName());
    m_tree.activeFormattingElements().clearToLastMarker();
    m_insertionMode = InsertionMode::InTable;
    return true;
}

bool HTMLTreeBuilder::processTrEndTagForInRow()
{
    if (!m_tree.openElements().inTableScope(trTag)) {
        ASSERT(isParsingFragmentOrTemplateContents());
        // FIXME: parse error
        return false;
    }
    m_tree.openElements().popUntilTableRowScopeMarker();
    ASSERT(m_tree.currentStackItem().hasTagName(trTag));
    m_tree.openElements().pop();
    m_insertionMode = InsertionMode::InTableBody;
    return true;
}

bool HTMLTreeBuilder::processTableEndTagForInTable()
{
    if (!m_tree.openElements().inTableScope(tableTag)) {
        ASSERT(isParsingFragmentOrTemplateContents());
        // FIXME: parse error.
        return false;
    }
    m_tree.openElements().popUntilPopped(tableTag->localName());
    resetInsertionModeAppropriately();
    return true;
}

void HTMLTreeBuilder::processEndTagForInTable(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::EndTag);
    if (token.name() == tableTag) {
        processTableEndTagForInTable();
        return;
    }
    if (token.name() == bodyTag
        || isCaptionColOrColgroupTag(token.name())
        || token.name() == htmlTag
        || isTableBodyContextTag(token.name())
        || isTableCellContextTag(token.name())
        || token.name() == trTag) {
        parseError(token);
        return;
    }
    parseError(token);
    // Is this redirection necessary here?
    HTMLConstructionSite::RedirectToFosterParentGuard redirecter(m_tree);
    processEndTagForInBody(WTFMove(token));
}

void HTMLTreeBuilder::processEndTag(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::EndTag);
    switch (m_insertionMode) {
    case InsertionMode::Initial:
        defaultForInitial();
        ASSERT(m_insertionMode == InsertionMode::BeforeHTML);
        FALLTHROUGH;
    case InsertionMode::BeforeHTML:
        if (token.name() != headTag && token.name() != bodyTag && token.name() != htmlTag && token.name() != brTag) {
            parseError(token);
            return;
        }
        defaultForBeforeHTML();
        ASSERT(m_insertionMode == InsertionMode::BeforeHead);
        FALLTHROUGH;
    case InsertionMode::BeforeHead:
        if (token.name() != headTag && token.name() != bodyTag && token.name() != htmlTag && token.name() != brTag) {
            parseError(token);
            return;
        }
        defaultForBeforeHead();
        ASSERT(m_insertionMode == InsertionMode::InHead);
        FALLTHROUGH;
    case InsertionMode::InHead:
        // FIXME: This case should be broken out into processEndTagForInHead,
        // because other end tag cases now refer to it ("process the token for using the rules of the "in head" insertion mode").
        // but because the logic falls through to InsertionMode::AfterHead, that gets a little messy.
        if (token.name() == templateTag) {
            processTemplateEndTag(WTFMove(token));
            return;
        }
        if (token.name() == headTag) {
            m_tree.openElements().popHTMLHeadElement();
            m_insertionMode = InsertionMode::AfterHead;
            return;
        }
        if (token.name() != bodyTag && token.name() != htmlTag && token.name() != brTag) {
            parseError(token);
            return;
        }
        defaultForInHead();
        ASSERT(m_insertionMode == InsertionMode::AfterHead);
        FALLTHROUGH;
    case InsertionMode::AfterHead:
        if (token.name() != bodyTag && token.name() != htmlTag && token.name() != brTag) {
            parseError(token);
            return;
        }
        defaultForAfterHead();
        ASSERT(m_insertionMode == InsertionMode::InBody);
        FALLTHROUGH;
    case InsertionMode::InBody:
        processEndTagForInBody(WTFMove(token));
        break;
    case InsertionMode::InTable:
        processEndTagForInTable(WTFMove(token));
        break;
    case InsertionMode::InCaption:
        if (token.name() == captionTag) {
            processCaptionEndTagForInCaption();
            return;
        }
        if (token.name() == tableTag) {
            parseError(token);
            if (!processCaptionEndTagForInCaption()) {
                ASSERT(isParsingFragment());
                return;
            }
            processEndTag(WTFMove(token));
            return;
        }
        if (token.name() == bodyTag
            || token.name() == colTag
            || token.name() == colgroupTag
            || token.name() == htmlTag
            || isTableBodyContextTag(token.name())
            || isTableCellContextTag(token.name())
            || token.name() == trTag) {
            parseError(token);
            return;
        }
        processEndTagForInBody(WTFMove(token));
        break;
    case InsertionMode::InColumnGroup:
        if (token.name() == colgroupTag) {
            processColgroupEndTagForInColumnGroup();
            return;
        }
        if (token.name() == colTag) {
            parseError(token);
            return;
        }
        if (token.name() == templateTag) {
            processTemplateEndTag(WTFMove(token));
            return;
        }
        if (!processColgroupEndTagForInColumnGroup()) {
            ASSERT(isParsingFragmentOrTemplateContents());
            return;
        }
        processEndTag(WTFMove(token));
        break;
    case InsertionMode::InRow:
        processEndTagForInRow(WTFMove(token));
        break;
    case InsertionMode::InCell:
        processEndTagForInCell(WTFMove(token));
        break;
    case InsertionMode::InTableBody:
        processEndTagForInTableBody(WTFMove(token));
        break;
    case InsertionMode::AfterBody:
        if (token.name() == htmlTag) {
            if (isParsingFragment()) {
                parseError(token);
                return;
            }
            m_insertionMode = InsertionMode::AfterAfterBody;
            return;
        }
        FALLTHROUGH;
    case InsertionMode::AfterAfterBody:
        ASSERT(m_insertionMode == InsertionMode::AfterBody || m_insertionMode == InsertionMode::AfterAfterBody);
        parseError(token);
        m_insertionMode = InsertionMode::InBody;
        processEndTag(WTFMove(token));
        break;
    case InsertionMode::InHeadNoscript:
        if (token.name() == noscriptTag) {
            ASSERT(m_tree.currentStackItem().hasTagName(noscriptTag));
            m_tree.openElements().pop();
            ASSERT(m_tree.currentStackItem().hasTagName(headTag));
            m_insertionMode = InsertionMode::InHead;
            return;
        }
        if (token.name() != brTag) {
            parseError(token);
            return;
        }
        defaultForInHeadNoscript();
        processToken(WTFMove(token));
        break;
    case InsertionMode::Text:
        if (token.name() == scriptTag) {
            // Pause ourselves so that parsing stops until the script can be processed by the caller.
            ASSERT(m_tree.currentStackItem().hasTagName(scriptTag));
            if (scriptingContentIsAllowed(m_tree.parserContentPolicy()))
                m_scriptToProcess = &downcast<HTMLScriptElement>(m_tree.currentElement());
            m_tree.openElements().pop();
            m_insertionMode = m_originalInsertionMode;

            // This token will not have been created by the tokenizer if a
            // self-closing script tag was encountered and pre-HTML5 parser
            // quirks are enabled. We must set the tokenizer's state to
            // DataState explicitly if the tokenizer didn't have a chance to.
            ASSERT(m_parser.tokenizer().isInDataState() || m_options.usePreHTML5ParserQuirks);
            m_parser.tokenizer().setDataState();
            return;
        }
        m_tree.openElements().pop();
        m_insertionMode = m_originalInsertionMode;
        break;
    case InsertionMode::InFrameset:
        if (token.name() == framesetTag) {
            bool ignoreFramesetForFragmentParsing  = m_tree.currentIsRootNode() || m_tree.openElements().hasTemplateInHTMLScope();
            if (ignoreFramesetForFragmentParsing) {
                ASSERT(isParsingFragmentOrTemplateContents());
                parseError(token);
                return;
            }
            m_tree.openElements().pop();
            if (!isParsingFragment() && !m_tree.currentStackItem().hasTagName(framesetTag))
                m_insertionMode = InsertionMode::AfterFrameset;
            return;
        }
        break;
    case InsertionMode::AfterFrameset:
        if (token.name() == htmlTag) {
            m_insertionMode = InsertionMode::AfterAfterFrameset;
            return;
        }
        FALLTHROUGH;
    case InsertionMode::AfterAfterFrameset:
        ASSERT(m_insertionMode == InsertionMode::AfterFrameset || m_insertionMode == InsertionMode::AfterAfterFrameset);
        parseError(token);
        break;
    case InsertionMode::InSelectInTable:
        if (token.name() == captionTag
            || token.name() == tableTag
            || isTableBodyContextTag(token.name())
            || token.name() == trTag
            || isTableCellContextTag(token.name())) {
            parseError(token);
            if (m_tree.openElements().inTableScope(token.name())) {
                AtomicHTMLToken endSelect(HTMLToken::EndTag, selectTag->localName());
                processEndTag(WTFMove(endSelect));
                processEndTag(WTFMove(token));
            }
            return;
        }
        FALLTHROUGH;
    case InsertionMode::InSelect:
        ASSERT(m_insertionMode == InsertionMode::InSelect || m_insertionMode == InsertionMode::InSelectInTable);
        if (token.name() == optgroupTag) {
            if (is<HTMLOptionElement>(m_tree.currentStackItem().node()) && m_tree.oneBelowTop() && is<HTMLOptGroupElement>(m_tree.oneBelowTop()->node()))
                processFakeEndTag(optionTag);
            if (is<HTMLOptGroupElement>(m_tree.currentStackItem().node())) {
                m_tree.openElements().pop();
                return;
            }
            parseError(token);
            return;
        }
        if (token.name() == optionTag) {
            if (is<HTMLOptionElement>(m_tree.currentStackItem().node())) {
                m_tree.openElements().pop();
                return;
            }
            parseError(token);
            return;
        }
        if (token.name() == selectTag) {
            if (!m_tree.openElements().inSelectScope(token.name())) {
                ASSERT(isParsingFragment());
                parseError(token);
                return;
            }
            m_tree.openElements().popUntilPopped(selectTag->localName());
            resetInsertionModeAppropriately();
            return;
        }
        if (token.name() == templateTag) {
            processTemplateEndTag(WTFMove(token));
            return;
        }
        break;
    case InsertionMode::InTableText:
        defaultForInTableText();
        processEndTag(WTFMove(token));
        break;
    case InsertionMode::TemplateContents:
        if (token.name() == templateTag) {
            processTemplateEndTag(WTFMove(token));
            return;
        }
        break;
    }
}

void HTMLTreeBuilder::processComment(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    if (m_insertionMode == InsertionMode::Initial
        || m_insertionMode == InsertionMode::BeforeHTML
        || m_insertionMode == InsertionMode::AfterAfterBody
        || m_insertionMode == InsertionMode::AfterAfterFrameset) {
        m_tree.insertCommentOnDocument(WTFMove(token));
        return;
    }
    if (m_insertionMode == InsertionMode::AfterBody) {
        m_tree.insertCommentOnHTMLHtmlElement(WTFMove(token));
        return;
    }
    if (m_insertionMode == InsertionMode::InTableText) {
        defaultForInTableText();
        processComment(WTFMove(token));
        return;
    }
    m_tree.insertComment(WTFMove(token));
}

void HTMLTreeBuilder::processCharacter(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Character);
    ExternalCharacterTokenBuffer buffer(token);
    processCharacterBuffer(buffer);
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(IOS_FAMILY)

// FIXME: Extract the following iOS-specific code into a separate file.
// From the string 4089961010, creates a link of the form <a href="tel:4089961010">4089961010</a> and inserts it.
void HTMLTreeBuilder::insertPhoneNumberLink(const String& string)
{
    Vector<Attribute> attributes;
    attributes.append(Attribute(HTMLNames::hrefAttr, makeString("tel:"_s, string)));

    const AtomString& aTagLocalName = aTag->localName();
    AtomicHTMLToken aStartToken(HTMLToken::StartTag, aTagLocalName, WTFMove(attributes));
    AtomicHTMLToken aEndToken(HTMLToken::EndTag, aTagLocalName);

    processStartTag(WTFMove(aStartToken));
    m_tree.executeQueuedTasks();
    m_tree.insertTextNode(string);
    processEndTag(WTFMove(aEndToken));
}

// Locates the phone numbers in the string and deals with it
// 1. Appends the text before the phone number as a text node.
// 2. Wraps the phone number in a tel: link.
// 3. Goes back to step 1 if a phone number is found in the rest of the string.
// 4. Appends the rest of the string as a text node.
void HTMLTreeBuilder::linkifyPhoneNumbers(const String& string)
{
    ASSERT(TelephoneNumberDetector::isSupported());

    // relativeStartPosition and relativeEndPosition are the endpoints of the phone number range,
    // relative to the scannerPosition
    unsigned length = string.length();
    unsigned scannerPosition = 0;
    int relativeStartPosition = 0;
    int relativeEndPosition = 0;

    auto characters = StringView(string).upconvertedCharacters();

    // While there's a phone number in the rest of the string...
    while (scannerPosition < length && TelephoneNumberDetector::find(&characters[scannerPosition], length - scannerPosition, &relativeStartPosition, &relativeEndPosition)) {
        // The convention in the Data Detectors framework is that the end position is the first character NOT in the phone number
        // (that is, the length of the range is relativeEndPosition - relativeStartPosition). So substract 1 to get the same
        // convention as the old WebCore phone number parser (so that the rest of the code is still valid if we want to go back
        // to the old parser).
        --relativeEndPosition;

        ASSERT(scannerPosition + relativeEndPosition < length);

        m_tree.insertTextNode(string.substring(scannerPosition, relativeStartPosition));
        insertPhoneNumberLink(string.substring(scannerPosition + relativeStartPosition, relativeEndPosition - relativeStartPosition + 1));

        scannerPosition += relativeEndPosition + 1;
    }

    // Append the rest as a text node.
    if (scannerPosition > 0) {
        if (scannerPosition < length) {
            String after = string.substring(scannerPosition, length - scannerPosition);
            m_tree.insertTextNode(after);
        }
    } else
        m_tree.insertTextNode(string);
}

// Looks at the ancestors of the element to determine whether we're inside an element which disallows parsing phone numbers.
static inline bool disallowTelephoneNumberParsing(const ContainerNode& node)
{
    return node.isLink()
        || node.hasTagName(aTag)
        || node.hasTagName(scriptTag)
        || is<HTMLFormControlElement>(node)
        || node.hasTagName(styleTag)
        || node.hasTagName(ttTag)
        || node.hasTagName(preTag)
        || node.hasTagName(codeTag);
}

static inline bool shouldParseTelephoneNumbersInNode(const ContainerNode& node)
{
    for (const ContainerNode* ancestor = &node; ancestor; ancestor = ancestor->parentNode()) {
        if (disallowTelephoneNumberParsing(*ancestor))
            return false;
    }
    return true;
}

#endif // ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(IOS_FAMILY)

void HTMLTreeBuilder::processCharacterBuffer(ExternalCharacterTokenBuffer& buffer)
{
ReprocessBuffer:
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
    // Note that this logic is different than the generic \r\n collapsing
    // handled in the input stream preprocessor. This logic is here as an
    // "authoring convenience" so folks can write:
    //
    // <pre>
    // lorem ipsum
    // lorem ipsum
    // </pre>
    //
    // without getting an extra newline at the start of their <pre> element.
    if (m_shouldSkipLeadingNewline) {
        m_shouldSkipLeadingNewline = false;
        buffer.skipAtMostOneLeadingNewline();
        if (buffer.isEmpty())
            return;
    }

    switch (m_insertionMode) {
    case InsertionMode::Initial:
        buffer.skipLeadingWhitespace();
        if (buffer.isEmpty())
            return;
        defaultForInitial();
        ASSERT(m_insertionMode == InsertionMode::BeforeHTML);
        FALLTHROUGH;
    case InsertionMode::BeforeHTML:
        buffer.skipLeadingWhitespace();
        if (buffer.isEmpty())
            return;
        defaultForBeforeHTML();
        ASSERT(m_insertionMode == InsertionMode::BeforeHead);
        FALLTHROUGH;
    case InsertionMode::BeforeHead:
        buffer.skipLeadingWhitespace();
        if (buffer.isEmpty())
            return;
        defaultForBeforeHead();
        ASSERT(m_insertionMode == InsertionMode::InHead);
        FALLTHROUGH;
    case InsertionMode::InHead: {
        String leadingWhitespace = buffer.takeLeadingWhitespace();
        if (!leadingWhitespace.isEmpty())
            m_tree.insertTextNode(leadingWhitespace, AllWhitespace);
        if (buffer.isEmpty())
            return;
        defaultForInHead();
        ASSERT(m_insertionMode == InsertionMode::AfterHead);
        FALLTHROUGH;
    }
    case InsertionMode::AfterHead: {
        String leadingWhitespace = buffer.takeLeadingWhitespace();
        if (!leadingWhitespace.isEmpty())
            m_tree.insertTextNode(leadingWhitespace, AllWhitespace);
        if (buffer.isEmpty())
            return;
        defaultForAfterHead();
        ASSERT(m_insertionMode == InsertionMode::InBody);
        FALLTHROUGH;
    }
    case InsertionMode::InBody:
    case InsertionMode::InCaption:
    case InsertionMode::InCell:
    case InsertionMode::TemplateContents:
        processCharacterBufferForInBody(buffer);
        break;
    case InsertionMode::InTable:
    case InsertionMode::InTableBody:
    case InsertionMode::InRow:
        ASSERT(m_pendingTableCharacters.isEmpty());
        if (is<HTMLTableElement>(m_tree.currentStackItem().node())
            || m_tree.currentStackItem().hasTagName(HTMLNames::tbodyTag)
            || m_tree.currentStackItem().hasTagName(HTMLNames::tfootTag)
            || m_tree.currentStackItem().hasTagName(HTMLNames::theadTag)
            || m_tree.currentStackItem().hasTagName(HTMLNames::trTag)) {

            m_originalInsertionMode = m_insertionMode;
            m_insertionMode = InsertionMode::InTableText;
            // Note that we fall through to the InsertionMode::InTableText case below.
        } else {
            HTMLConstructionSite::RedirectToFosterParentGuard redirecter(m_tree);
            processCharacterBufferForInBody(buffer);
            break;
        }
        FALLTHROUGH;
    case InsertionMode::InTableText:
        buffer.giveRemainingTo(m_pendingTableCharacters);
        break;
    case InsertionMode::InColumnGroup: {
        String leadingWhitespace = buffer.takeLeadingWhitespace();
        if (!leadingWhitespace.isEmpty())
            m_tree.insertTextNode(leadingWhitespace, AllWhitespace);
        if (buffer.isEmpty())
            return;
        if (!processColgroupEndTagForInColumnGroup()) {
            ASSERT(isParsingFragmentOrTemplateContents());
            // The spec tells us to drop these characters on the floor.
            buffer.skipLeadingNonWhitespace();
            if (buffer.isEmpty())
                return;
        }
        goto ReprocessBuffer;
    }
    case InsertionMode::AfterBody:
    case InsertionMode::AfterAfterBody:
        // FIXME: parse error
        m_insertionMode = InsertionMode::InBody;
        goto ReprocessBuffer;
    case InsertionMode::Text:
        m_tree.insertTextNode(buffer.takeRemaining());
        break;
    case InsertionMode::InHeadNoscript: {
        String leadingWhitespace = buffer.takeLeadingWhitespace();
        if (!leadingWhitespace.isEmpty())
            m_tree.insertTextNode(leadingWhitespace, AllWhitespace);
        if (buffer.isEmpty())
            return;
        defaultForInHeadNoscript();
        goto ReprocessBuffer;
    }
    case InsertionMode::InFrameset:
    case InsertionMode::AfterFrameset: {
        String leadingWhitespace = buffer.takeRemainingWhitespace();
        if (!leadingWhitespace.isEmpty())
            m_tree.insertTextNode(leadingWhitespace, AllWhitespace);
        // FIXME: We should generate a parse error if we skipped over any
        // non-whitespace characters.
        break;
    }
    case InsertionMode::InSelectInTable:
    case InsertionMode::InSelect:
        m_tree.insertTextNode(buffer.takeRemaining());
        break;
    case InsertionMode::AfterAfterFrameset: {
        String leadingWhitespace = buffer.takeRemainingWhitespace();
        if (!leadingWhitespace.isEmpty()) {
            m_tree.reconstructTheActiveFormattingElements();
            m_tree.insertTextNode(leadingWhitespace, AllWhitespace);
        }
        // FIXME: We should generate a parse error if we skipped over any
        // non-whitespace characters.
        break;
    }
    }
}

void HTMLTreeBuilder::processCharacterBufferForInBody(ExternalCharacterTokenBuffer& buffer)
{
    m_tree.reconstructTheActiveFormattingElements();
    String characters = buffer.takeRemaining();
#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(IOS_FAMILY)
    if (!isParsingFragment() && m_tree.isTelephoneNumberParsingEnabled() && shouldParseTelephoneNumbersInNode(m_tree.currentNode()) && TelephoneNumberDetector::isSupported())
        linkifyPhoneNumbers(characters);
    else
        m_tree.insertTextNode(characters);
#else
    m_tree.insertTextNode(characters);
#endif
    if (m_framesetOk && !isAllWhitespaceOrReplacementCharacters(characters))
        m_framesetOk = false;
}

void HTMLTreeBuilder::processEndOfFile(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::EndOfFile);
    switch (m_insertionMode) {
    case InsertionMode::Initial:
        defaultForInitial();
        ASSERT(m_insertionMode == InsertionMode::BeforeHTML);
        FALLTHROUGH;
    case InsertionMode::BeforeHTML:
        defaultForBeforeHTML();
        ASSERT(m_insertionMode == InsertionMode::BeforeHead);
        FALLTHROUGH;
    case InsertionMode::BeforeHead:
        defaultForBeforeHead();
        ASSERT(m_insertionMode == InsertionMode::InHead);
        FALLTHROUGH;
    case InsertionMode::InHead:
        defaultForInHead();
        ASSERT(m_insertionMode == InsertionMode::AfterHead);
        FALLTHROUGH;
    case InsertionMode::AfterHead:
        defaultForAfterHead();
        ASSERT(m_insertionMode == InsertionMode::InBody);
        FALLTHROUGH;
    case InsertionMode::InBody:
    case InsertionMode::InCell:
    case InsertionMode::InCaption:
    case InsertionMode::InRow:
        notImplemented(); // Emit parse error based on what elements are still open.
        if (!m_templateInsertionModes.isEmpty()) {
            if (processEndOfFileForInTemplateContents(WTFMove(token)))
                return;
        }
        break;
    case InsertionMode::AfterBody:
    case InsertionMode::AfterAfterBody:
        break;
    case InsertionMode::InHeadNoscript:
        defaultForInHeadNoscript();
        processEndOfFile(WTFMove(token));
        return;
    case InsertionMode::AfterFrameset:
    case InsertionMode::AfterAfterFrameset:
        break;
    case InsertionMode::InColumnGroup:
        if (m_tree.currentIsRootNode()) {
            ASSERT(isParsingFragment());
            return; // FIXME: Should we break here instead of returning?
        }
        ASSERT(m_tree.currentNode().hasTagName(colgroupTag) || m_tree.currentNode().hasTagName(templateTag));
        processColgroupEndTagForInColumnGroup();
        FALLTHROUGH;
    case InsertionMode::InFrameset:
    case InsertionMode::InTable:
    case InsertionMode::InTableBody:
    case InsertionMode::InSelectInTable:
    case InsertionMode::InSelect:
        ASSERT(m_insertionMode == InsertionMode::InSelect || m_insertionMode == InsertionMode::InSelectInTable || m_insertionMode == InsertionMode::InTable || m_insertionMode == InsertionMode::InFrameset || m_insertionMode == InsertionMode::InTableBody || m_insertionMode == InsertionMode::InColumnGroup);
        if (&m_tree.currentNode() != &m_tree.openElements().rootNode())
            parseError(token);
        if (!m_templateInsertionModes.isEmpty()) {
            if (processEndOfFileForInTemplateContents(WTFMove(token)))
                return;
        }
        break;
    case InsertionMode::InTableText:
        defaultForInTableText();
        processEndOfFile(WTFMove(token));
        return;
    case InsertionMode::Text:
        parseError(token);
        if (m_tree.currentStackItem().hasTagName(scriptTag))
            notImplemented(); // mark the script element as "already started".
        m_tree.openElements().pop();
        ASSERT(m_originalInsertionMode != InsertionMode::Text);
        m_insertionMode = m_originalInsertionMode;
        processEndOfFile(WTFMove(token));
        return;
    case InsertionMode::TemplateContents:
        if (processEndOfFileForInTemplateContents(WTFMove(token)))
            return;
        break;
    }
    m_tree.openElements().popAll();
}

void HTMLTreeBuilder::defaultForInitial()
{
    notImplemented();
    m_tree.setDefaultCompatibilityMode();
    // FIXME: parse error
    m_insertionMode = InsertionMode::BeforeHTML;
}

void HTMLTreeBuilder::defaultForBeforeHTML()
{
    AtomicHTMLToken startHTML(HTMLToken::StartTag, htmlTag->localName());
    m_tree.insertHTMLHtmlStartTagBeforeHTML(WTFMove(startHTML));
    m_insertionMode = InsertionMode::BeforeHead;
}

void HTMLTreeBuilder::defaultForBeforeHead()
{
    AtomicHTMLToken startHead(HTMLToken::StartTag, headTag->localName());
    processStartTag(WTFMove(startHead));
}

void HTMLTreeBuilder::defaultForInHead()
{
    AtomicHTMLToken endHead(HTMLToken::EndTag, headTag->localName());
    processEndTag(WTFMove(endHead));
}

void HTMLTreeBuilder::defaultForInHeadNoscript()
{
    AtomicHTMLToken endNoscript(HTMLToken::EndTag, noscriptTag->localName());
    processEndTag(WTFMove(endNoscript));
}

void HTMLTreeBuilder::defaultForAfterHead()
{
    AtomicHTMLToken startBody(HTMLToken::StartTag, bodyTag->localName());
    processStartTag(WTFMove(startBody));
    m_framesetOk = true;
}

void HTMLTreeBuilder::defaultForInTableText()
{
    String characters = m_pendingTableCharacters.toString();
    m_pendingTableCharacters.clear();
    if (!isAllWhitespace(characters)) {
        // FIXME: parse error
        HTMLConstructionSite::RedirectToFosterParentGuard redirecter(m_tree);
        m_tree.reconstructTheActiveFormattingElements();
        m_tree.insertTextNode(characters, NotAllWhitespace);
        m_framesetOk = false;
        m_insertionMode = m_originalInsertionMode;
        return;
    }
    m_tree.insertTextNode(characters);
    m_insertionMode = m_originalInsertionMode;
}

bool HTMLTreeBuilder::processStartTagForInHead(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    if (token.name() == htmlTag) {
        processHtmlStartTagForInBody(WTFMove(token));
        return true;
    }
    if (token.name() == baseTag
        || token.name() == basefontTag
        || token.name() == bgsoundTag
        || token.name() == commandTag
        || token.name() == linkTag
        || token.name() == metaTag) {
        m_tree.insertSelfClosingHTMLElement(WTFMove(token));
        // Note: The custom processing for the <meta> tag is done in HTMLMetaElement::process().
        return true;
    }
    if (token.name() == titleTag) {
        processGenericRCDATAStartTag(WTFMove(token));
        return true;
    }
    if (token.name() == noscriptTag) {
        if (m_options.scriptingFlag) {
            processGenericRawTextStartTag(WTFMove(token));
            return true;
        }
        m_tree.insertHTMLElement(WTFMove(token));
        m_insertionMode = InsertionMode::InHeadNoscript;
        return true;
    }
    if (token.name() == noframesTag || token.name() == styleTag) {
        processGenericRawTextStartTag(WTFMove(token));
        return true;
    }
    if (token.name() == scriptTag) {
        bool isSelfClosing = token.selfClosing();
        processScriptStartTag(WTFMove(token));
        if (m_options.usePreHTML5ParserQuirks && isSelfClosing)
            processFakeEndTag(scriptTag);
        return true;
    }
    if (token.name() == templateTag) {
        m_framesetOk = false;
        processTemplateStartTag(WTFMove(token));
        return true;
    }
    if (token.name() == headTag) {
        parseError(token);
        return true;
    }
    return false;
}

void HTMLTreeBuilder::processGenericRCDATAStartTag(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    m_tree.insertHTMLElement(WTFMove(token));
    m_parser.tokenizer().setRCDATAState();
    m_originalInsertionMode = m_insertionMode;
    m_insertionMode = InsertionMode::Text;
}

void HTMLTreeBuilder::processGenericRawTextStartTag(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    m_tree.insertHTMLElement(WTFMove(token));
    m_parser.tokenizer().setRAWTEXTState();
    m_originalInsertionMode = m_insertionMode;
    m_insertionMode = InsertionMode::Text;
}

void HTMLTreeBuilder::processScriptStartTag(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    m_tree.insertScriptElement(WTFMove(token));
    m_parser.tokenizer().setScriptDataState();
    m_originalInsertionMode = m_insertionMode;

    TextPosition position = m_parser.textPosition();

    m_scriptToProcessStartPosition = position;

    m_insertionMode = InsertionMode::Text;
}

// http://www.whatwg.org/specs/web-apps/current-work/#adjusted-current-node
HTMLStackItem& HTMLTreeBuilder::adjustedCurrentStackItem() const
{
    ASSERT(!m_tree.isEmpty());
    if (isParsingFragment() && m_tree.openElements().hasOnlyOneElement())
        return m_fragmentContext.contextElementStackItem();

    return m_tree.currentStackItem();
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#tree-construction
bool HTMLTreeBuilder::shouldProcessTokenInForeignContent(const AtomicHTMLToken& token)
{
    if (m_tree.isEmpty())
        return false;
    HTMLStackItem& adjustedCurrentNode = adjustedCurrentStackItem();
    if (isInHTMLNamespace(adjustedCurrentNode))
        return false;
    if (HTMLElementStack::isMathMLTextIntegrationPoint(adjustedCurrentNode)) {
        if (token.type() == HTMLToken::StartTag
            && token.name() != MathMLNames::mglyphTag
            && token.name() != MathMLNames::malignmarkTag)
            return false;
        if (token.type() == HTMLToken::Character)
            return false;
    }
    if (adjustedCurrentNode.hasTagName(MathMLNames::annotation_xmlTag)
        && token.type() == HTMLToken::StartTag
        && token.name() == SVGNames::svgTag)
        return false;
    if (HTMLElementStack::isHTMLIntegrationPoint(adjustedCurrentNode)) {
        if (token.type() == HTMLToken::StartTag)
            return false;
        if (token.type() == HTMLToken::Character)
            return false;
    }
    if (token.type() == HTMLToken::EndOfFile)
        return false;
    return true;
}

static bool hasAttribute(const AtomicHTMLToken& token, const QualifiedName& name)
{
    return findAttribute(token.attributes(), name);
}

void HTMLTreeBuilder::processTokenInForeignContent(AtomicHTMLToken&& token)
{
    HTMLStackItem& adjustedCurrentNode = adjustedCurrentStackItem();
    
    switch (token.type()) {
    case HTMLToken::Uninitialized:
        ASSERT_NOT_REACHED();
        break;
    case HTMLToken::DOCTYPE:
        parseError(token);
        break;
    case HTMLToken::StartTag: {
        if (token.name() == bTag
            || token.name() == bigTag
            || token.name() == blockquoteTag
            || token.name() == bodyTag
            || token.name() == brTag
            || token.name() == centerTag
            || token.name() == codeTag
            || token.name() == ddTag
            || token.name() == divTag
            || token.name() == dlTag
            || token.name() == dtTag
            || token.name() == emTag
            || token.name() == embedTag
            || isNumberedHeaderTag(token.name())
            || token.name() == headTag
            || token.name() == hrTag
            || token.name() == iTag
            || token.name() == imgTag
            || token.name() == liTag
            || token.name() == listingTag
            || token.name() == menuTag
            || token.name() == metaTag
            || token.name() == nobrTag
            || token.name() == olTag
            || token.name() == pTag
            || token.name() == preTag
            || token.name() == rubyTag
            || token.name() == sTag
            || token.name() == smallTag
            || token.name() == spanTag
            || token.name() == strongTag
            || token.name() == strikeTag
            || token.name() == subTag
            || token.name() == supTag
            || token.name() == tableTag
            || token.name() == ttTag
            || token.name() == uTag
            || token.name() == ulTag
            || token.name() == varTag
            || (token.name() == fontTag && (hasAttribute(token, colorAttr) || hasAttribute(token, faceAttr) || hasAttribute(token, sizeAttr)))) {
            parseError(token);
            m_tree.openElements().popUntilForeignContentScopeMarker();
            processStartTag(WTFMove(token));
            return;
        }
        const AtomString& currentNamespace = adjustedCurrentNode.namespaceURI();
        if (currentNamespace == MathMLNames::mathmlNamespaceURI)
            adjustMathMLAttributes(token);
        if (currentNamespace == SVGNames::svgNamespaceURI) {
            adjustSVGTagNameCase(token);
            adjustSVGAttributes(token);
        }
        adjustForeignAttributes(token);
        m_tree.insertForeignElement(WTFMove(token), currentNamespace);
        break;
    }
    case HTMLToken::EndTag: {
        if (adjustedCurrentNode.namespaceURI() == SVGNames::svgNamespaceURI)
            adjustSVGTagNameCase(token);

        if (token.name() == SVGNames::scriptTag && m_tree.currentStackItem().hasTagName(SVGNames::scriptTag)) {
            if (scriptingContentIsAllowed(m_tree.parserContentPolicy()))
                m_scriptToProcess = &downcast<SVGScriptElement>(m_tree.currentElement());
            m_tree.openElements().pop();
            return;
        }
        if (!isInHTMLNamespace(m_tree.currentStackItem())) {
            // FIXME: This code just wants an Element* iterator, instead of an ElementRecord*
            auto* nodeRecord = &m_tree.openElements().topRecord();
            if (nodeRecord->stackItem().localName() != token.name())
                parseError(token);
            while (1) {
                if (nodeRecord->stackItem().localName() == token.name()) {
                    m_tree.openElements().popUntilPopped(nodeRecord->element());
                    return;
                }
                nodeRecord = nodeRecord->next();

                if (isInHTMLNamespace(nodeRecord->stackItem()))
                    break;
            }
        }
        // Otherwise, process the token according to the rules given in the section corresponding to the current insertion mode in HTML content.
        processEndTag(WTFMove(token));
        break;
    }
    case HTMLToken::Comment:
        m_tree.insertComment(WTFMove(token));
        return;
    case HTMLToken::Character: {
        String characters = String(token.characters(), token.charactersLength());
        m_tree.insertTextNode(characters);
        if (m_framesetOk && !isAllWhitespaceOrReplacementCharacters(characters))
            m_framesetOk = false;
        break;
    }
    case HTMLToken::EndOfFile:
        ASSERT_NOT_REACHED();
        break;
    }
}

void HTMLTreeBuilder::finished()
{
    ASSERT(!m_destroyed);

    if (isParsingFragment())
        return;

    ASSERT(m_templateInsertionModes.isEmpty());

    m_tree.finishedParsing();
    // The tree builder might have been destroyed as an indirect result of finishing the parsing.
}

inline void HTMLTreeBuilder::parseError(const AtomicHTMLToken&)
{
}

}
