/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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
#include "Document.h"

#include "AXObjectCache.h"
#include "AnimationController.h"
#include "Attr.h"
#include "CDATASection.h"
#include "CSSHelper.h"
#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "CString.h"
#include "CachedCSSStyleSheet.h"
#include "Comment.h"
#include "Console.h"
#include "CookieJar.h"
#include "DOMImplementation.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "EditingText.h"
#include "Editor.h"
#include "EntityReference.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "HTMLElementFactory.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHeadElement.h"
#include "HTMLInputElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMapElement.h"
#include "HTMLNameCollection.h"
#include "HTMLNames.h"
#include "HTMLParser.h"
#include "HTMLStyleElement.h"
#include "HTMLTitleElement.h"
#include "HTTPParsers.h"
#include "HistoryItem.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "ImageLoader.h"
#include "InspectorController.h"
#include "InspectorTimelineAgent.h"
#include "KeyboardEvent.h"
#include "Logging.h"
#include "MappedAttribute.h"
#include "MessageEvent.h"
#include "MouseEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "MutationEvent.h"
#include "NameNodeList.h"
#include "NodeFilter.h"
#include "NodeIterator.h"
#include "NodeWithIndex.h"
#include "OverflowEvent.h"
#include "Page.h"
#include "PageGroup.h"
#include "PageTransitionEvent.h"
#include "PlatformKeyboardEvent.h"
#include "ProcessingInstruction.h"
#include "ProgressEvent.h"
#include "RegisteredEventListener.h"
#include "RenderArena.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ScriptController.h"
#include "ScriptElement.h"
#include "ScriptEventListener.h"
#include "SecurityOrigin.h"
#include "SegmentedString.h"
#include "SelectionController.h"
#include "Settings.h"
#include "StyleSheetList.h"
#include "TextEvent.h"
#include "TextIterator.h"
#include "TextResourceDecoder.h"
#include "Timer.h"
#include "TreeWalker.h"
#include "UIEvent.h"
#include "WebKitAnimationEvent.h"
#include "WebKitTransitionEvent.h"
#include "WheelEvent.h"
#include "XMLHttpRequest.h"
#include "XMLNames.h"
#include "XMLTokenizer.h"
#include "htmlediting.h"
#include <wtf/CurrentTime.h>
#include <wtf/HashFunctions.h>
#include <wtf/MainThread.h>
#include <wtf/PassRefPtr.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(DATABASE)
#include "Database.h"
#include "DatabaseThread.h"
#endif

#if ENABLE(SHARED_WORKERS)
#include "SharedWorkerRepository.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "StorageEvent.h"
#endif

#if ENABLE(XPATH)
#include "XPathEvaluator.h"
#include "XPathExpression.h"
#include "XPathNSResolver.h"
#include "XPathResult.h"
#endif

#if ENABLE(XSLT)
#include "XSLTProcessor.h"
#endif

#if ENABLE(XBL)
#include "XBLBindingManager.h"
#endif

#if ENABLE(SVG)
#include "SVGDocumentExtensions.h"
#include "SVGElementFactory.h"
#include "SVGNames.h"
#include "SVGZoomEvent.h"
#include "SVGStyleElement.h"
#endif

#if ENABLE(WML)
#include "WMLDocument.h"
#include "WMLElement.h"
#include "WMLElementFactory.h"
#include "WMLNames.h"
#endif

#if ENABLE(MATHML)
#include "MathMLElement.h"
#include "MathMLElementFactory.h"
#include "MathMLNames.h"
#endif

#if ENABLE(XHTMLMP)
#include "HTMLNoScriptElement.h"
#endif

using namespace std;
using namespace WTF;
using namespace Unicode;

namespace WebCore {

using namespace HTMLNames;

// #define INSTRUMENT_LAYOUT_SCHEDULING 1

// This amount of time must have elapsed before we will even consider scheduling a layout without a delay.
// FIXME: For faster machines this value can really be lowered to 200.  250 is adequate, but a little high
// for dual G5s. :)
static const int cLayoutScheduleThreshold = 250;

// Use 1 to represent the document's default form.
static HTMLFormElement* const defaultForm = reinterpret_cast<HTMLFormElement*>(1);

// DOM Level 2 says (letters added):
//
// a) Name start characters must have one of the categories Ll, Lu, Lo, Lt, Nl.
// b) Name characters other than Name-start characters must have one of the categories Mc, Me, Mn, Lm, or Nd.
// c) Characters in the compatibility area (i.e. with character code greater than #xF900 and less than #xFFFE) are not allowed in XML names.
// d) Characters which have a font or compatibility decomposition (i.e. those with a "compatibility formatting tag" in field 5 of the database -- marked by field 5 beginning with a "<") are not allowed.
// e) The following characters are treated as name-start characters rather than name characters, because the property file classifies them as Alphabetic: [#x02BB-#x02C1], #x0559, #x06E5, #x06E6.
// f) Characters #x20DD-#x20E0 are excluded (in accordance with Unicode, section 5.14).
// g) Character #x00B7 is classified as an extender, because the property list so identifies it.
// h) Character #x0387 is added as a name character, because #x00B7 is its canonical equivalent.
// i) Characters ':' and '_' are allowed as name-start characters.
// j) Characters '-' and '.' are allowed as name characters.
//
// It also contains complete tables. If we decide it's better, we could include those instead of the following code.

static inline bool isValidNameStart(UChar32 c)
{
    // rule (e) above
    if ((c >= 0x02BB && c <= 0x02C1) || c == 0x559 || c == 0x6E5 || c == 0x6E6)
        return true;

    // rule (i) above
    if (c == ':' || c == '_')
        return true;

    // rules (a) and (f) above
    const uint32_t nameStartMask = Letter_Lowercase | Letter_Uppercase | Letter_Other | Letter_Titlecase | Number_Letter;
    if (!(Unicode::category(c) & nameStartMask))
        return false;

    // rule (c) above
    if (c >= 0xF900 && c < 0xFFFE)
        return false;

    // rule (d) above
    DecompositionType decompType = decompositionType(c);
    if (decompType == DecompositionFont || decompType == DecompositionCompat)
        return false;

    return true;
}

static inline bool isValidNamePart(UChar32 c)
{
    // rules (a), (e), and (i) above
    if (isValidNameStart(c))
        return true;

    // rules (g) and (h) above
    if (c == 0x00B7 || c == 0x0387)
        return true;

    // rule (j) above
    if (c == '-' || c == '.')
        return true;

    // rules (b) and (f) above
    const uint32_t otherNamePartMask = Mark_NonSpacing | Mark_Enclosing | Mark_SpacingCombining | Letter_Modifier | Number_DecimalDigit;
    if (!(Unicode::category(c) & otherNamePartMask))
        return false;

    // rule (c) above
    if (c >= 0xF900 && c < 0xFFFE)
        return false;

    // rule (d) above
    DecompositionType decompType = decompositionType(c);
    if (decompType == DecompositionFont || decompType == DecompositionCompat)
        return false;

    return true;
}

static Widget* widgetForNode(Node* focusedNode)
{
    if (!focusedNode)
        return 0;
    RenderObject* renderer = focusedNode->renderer();
    if (!renderer || !renderer->isWidget())
        return 0;
    return toRenderWidget(renderer)->widget();
}

static bool acceptsEditingFocus(Node *node)
{
    ASSERT(node);
    ASSERT(node->isContentEditable());

    Node *root = node->rootEditableElement();
    Frame* frame = node->document()->frame();
    if (!frame || !root)
        return false;

    return frame->editor()->shouldBeginEditing(rangeOfContents(root).get());
}

static bool disableRangeMutation(Page* page)
{
#if PLATFORM(MAC)
    // Disable Range mutation on document modifications in Tiger and Leopard Mail
    // See <rdar://problem/5865171>
    return page && (page->settings()->needsLeopardMailQuirks() || page->settings()->needsTigerMailQuirks());
#else
    return false;
#endif
}

static HashSet<Document*>* documentsThatNeedStyleRecalc = 0;

Document::Document(Frame* frame, bool isXHTML)
    : ContainerNode(0)
    , m_domtree_version(0)
    , m_styleSheets(StyleSheetList::create(this))
    , m_styleRecalcTimer(this, &Document::styleRecalcTimerFired)
    , m_frameElementsShouldIgnoreScrolling(false)
    , m_title("")
    , m_titleSetExplicitly(false)
    , m_updateFocusAppearanceTimer(this, &Document::updateFocusAppearanceTimerFired)
    , m_executeScriptSoonTimer(this, &Document::executeScriptSoonTimerFired)
#if ENABLE(XSLT)
    , m_transformSource(0)
#endif
    , m_xmlVersion("1.0")
    , m_xmlStandalone(false)
#if ENABLE(XBL)
    , m_bindingManager(new XBLBindingManager(this))
#endif
    , m_savedRenderer(0)
    , m_secureForms(0)
    , m_designMode(inherit)
    , m_selfOnlyRefCount(0)
#if ENABLE(SVG)
    , m_svgExtensions(0)
#endif
#if ENABLE(DASHBOARD_SUPPORT)
    , m_hasDashboardRegions(false)
    , m_dashboardRegionsDirty(false)
#endif
    , m_accessKeyMapValid(false)
    , m_createRenderers(true)
    , m_inPageCache(false)
    , m_useSecureKeyboardEntryWhenActive(false)
    , m_isXHTML(isXHTML)
    , m_numNodeListCaches(0)
#if ENABLE(DATABASE)
    , m_hasOpenDatabases(false)
#endif
    , m_usingGeolocation(false)
#if ENABLE(WML)
    , m_containsWMLContent(false)
#endif
{
    m_document = this;

    m_pageGroupUserSheetCacheValid = false;

    m_printing = false;
    
    m_ignoreAutofocus = false;

    m_frame = frame;
    m_renderArena = 0;

    m_axObjectCache = 0;
    
    m_docLoader = new DocLoader(this);

    visuallyOrdered = false;
    m_bParsing = false;
    m_tokenizer = 0;
    m_wellFormed = false;

    setParseMode(Strict);

    m_textColor = Color::black;
    m_listenerTypes = 0;
    m_inDocument = true;
    m_inStyleRecalc = false;
    m_closeAfterStyleRecalc = false;

    m_usesDescendantRules = false;
    m_usesSiblingRules = false;
    m_usesFirstLineRules = false;
    m_usesFirstLetterRules = false;
    m_usesBeforeAfterRules = false;
    m_usesRemUnits = false;

    m_gotoAnchorNeededAfterStylesheetsLoad = false;
 
    m_styleSelector = 0;
    m_didCalculateStyleSelector = false;
    m_pendingStylesheets = 0;
    m_ignorePendingStylesheets = false;
    m_hasNodesWithPlaceholderStyle = false;
    m_pendingSheetLayout = NoLayoutWithPendingSheets;

    m_cssTarget = 0;

    resetLinkColor();
    resetVisitedLinkColor();
    resetActiveLinkColor();

    m_processingLoadEvent = false;
    m_startTime = currentTime();
    m_overMinimumLayoutThreshold = false;
    
    initSecurityContext();
    initDNSPrefetch();

    static int docID = 0;
    m_docID = docID++;
#if ENABLE(XHTMLMP)
    m_shouldProcessNoScriptElement = settings() && !settings()->isJavaScriptEnabled();
#endif
}

void Document::removedLastRef()
{
    ASSERT(!m_deletionHasBegun);
    if (m_selfOnlyRefCount) {
        // If removing a child removes the last self-only ref, we don't
        // want the document to be destructed until after
        // removeAllChildren returns, so we guard ourselves with an
        // extra self-only ref.
        selfOnlyRef();

        // We must make sure not to be retaining any of our children through
        // these extra pointers or we will create a reference cycle.
        m_docType = 0;
        m_focusedNode = 0;
        m_hoverNode = 0;
        m_activeNode = 0;
        m_titleElement = 0;
        m_documentElement = 0;

        removeAllChildren();

        deleteAllValues(m_markers);
        m_markers.clear();

        delete m_tokenizer;
        m_tokenizer = 0;

        m_cssCanvasElements.clear();

#ifndef NDEBUG
        m_inRemovedLastRefFunction = false;
#endif

        selfOnlyDeref();
    } else {
#ifndef NDEBUG
        m_deletionHasBegun = true;
#endif
        delete this;
    }
}

Document::~Document()
{
    ASSERT(!renderer());
    ASSERT(!m_inPageCache);
    ASSERT(!m_savedRenderer);
    ASSERT(m_ranges.isEmpty());
    ASSERT(!m_styleRecalcTimer.isActive());

    for (size_t i = 0; i < m_scriptsToExecuteSoon.size(); ++i)
        m_scriptsToExecuteSoon[i].first->element()->deref(); // Balances ref() in executeScriptSoon().

    removeAllEventListeners();

#if USE(JSC)
    forgetAllDOMNodesForDocument(this);
#endif

    delete m_tokenizer;
    m_document = 0;
    delete m_styleSelector;
    delete m_docLoader;
    
    if (m_renderArena) {
        delete m_renderArena;
        m_renderArena = 0;
    }

#if ENABLE(XSLT)
    xmlFreeDoc((xmlDocPtr)m_transformSource);
#endif

#if ENABLE(XBL)
    delete m_bindingManager;
#endif

    deleteAllValues(m_markers);

    clearAXObjectCache();

    m_decoder = 0;
    
    unsigned count = sizeof(m_nameCollectionInfo) / sizeof(m_nameCollectionInfo[0]);
    for (unsigned i = 0; i < count; i++)
        deleteAllValues(m_nameCollectionInfo[i]);

#if ENABLE(DATABASE)
    if (m_databaseThread) {
        ASSERT(m_databaseThread->terminationRequested());
        m_databaseThread = 0;
    }
#endif

    if (m_styleSheets)
        m_styleSheets->documentDestroyed();
}

void Document::resetLinkColor()
{
    m_linkColor = Color(0, 0, 238);
}

void Document::resetVisitedLinkColor()
{
    m_visitedLinkColor = Color(85, 26, 139);    
}

void Document::resetActiveLinkColor()
{
    m_activeLinkColor.setNamedColor("red");
}

void Document::setDocType(PassRefPtr<DocumentType> docType)
{
    // This should never be called more than once.
    // Note: This is not a public DOM method and can only be called by the parser.
    ASSERT(!m_docType || !docType);
    if (m_docType && docType)
        return;
    m_docType = docType;
    if (m_docType)
        m_docType->setDocument(this);
    determineParseMode();
}

DOMImplementation* Document::implementation() const
{
    if (!m_implementation)
        m_implementation = DOMImplementation::create();
    return m_implementation.get();
}

void Document::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    ContainerNode::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    
    // Invalidate the document element we have cached in case it was replaced.
    m_documentElement = 0;
}

void Document::cacheDocumentElement() const
{
    ASSERT(!m_documentElement);
    Node* n = firstChild();
    while (n && !n->isElementNode())
        n = n->nextSibling();
    m_documentElement = static_cast<Element*>(n);
}

PassRefPtr<Element> Document::createElement(const AtomicString& name, ExceptionCode& ec)
{
    if (!isValidName(name)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }

    if (m_isXHTML)
        return HTMLElementFactory::createHTMLElement(QualifiedName(nullAtom, name, xhtmlNamespaceURI), this, 0, false);

    return createElement(QualifiedName(nullAtom, name, nullAtom), false);
}

PassRefPtr<DocumentFragment> Document::createDocumentFragment()
{
    return DocumentFragment::create(document());
}

PassRefPtr<Text> Document::createTextNode(const String& data)
{
    return Text::create(this, data);
}

PassRefPtr<Comment> Document::createComment(const String& data)
{
    return Comment::create(this, data);
}

PassRefPtr<CDATASection> Document::createCDATASection(const String& data, ExceptionCode& ec)
{
    if (isHTMLDocument()) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return CDATASection::create(this, data);
}

PassRefPtr<ProcessingInstruction> Document::createProcessingInstruction(const String& target, const String& data, ExceptionCode& ec)
{
    if (!isValidName(target)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }
    if (isHTMLDocument()) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return ProcessingInstruction::create(this, target, data);
}

PassRefPtr<EntityReference> Document::createEntityReference(const String& name, ExceptionCode& ec)
{
    if (!isValidName(name)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }
    if (isHTMLDocument()) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return EntityReference::create(this, name);
}

PassRefPtr<EditingText> Document::createEditingTextNode(const String& text)
{
    return EditingText::create(this, text);
}

PassRefPtr<CSSStyleDeclaration> Document::createCSSStyleDeclaration()
{
    return CSSMutableStyleDeclaration::create();
}

PassRefPtr<Node> Document::importNode(Node* importedNode, bool deep, ExceptionCode& ec)
{
    ec = 0;
    
    if (!importedNode
#if ENABLE(SVG) && ENABLE(DASHBOARD_SUPPORT)
        || (importedNode->isSVGElement() && page() && page()->settings()->usesDashboardBackwardCompatibilityMode())
#endif
        ) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }

    switch (importedNode->nodeType()) {
        case TEXT_NODE:
            return createTextNode(importedNode->nodeValue());
        case CDATA_SECTION_NODE:
            return createCDATASection(importedNode->nodeValue(), ec);
        case ENTITY_REFERENCE_NODE:
            return createEntityReference(importedNode->nodeName(), ec);
        case PROCESSING_INSTRUCTION_NODE:
            return createProcessingInstruction(importedNode->nodeName(), importedNode->nodeValue(), ec);
        case COMMENT_NODE:
            return createComment(importedNode->nodeValue());
        case ELEMENT_NODE: {
            Element* oldElement = static_cast<Element*>(importedNode);
            RefPtr<Element> newElement = createElementNS(oldElement->namespaceURI(), oldElement->tagQName().toString(), ec);
                        
            if (ec)
                return 0;

            NamedNodeMap* attrs = oldElement->attributes(true);
            if (attrs) {
                unsigned length = attrs->length();
                for (unsigned i = 0; i < length; i++) {
                    Attribute* attr = attrs->attributeItem(i);
                    newElement->setAttribute(attr->name(), attr->value().impl(), ec);
                    if (ec)
                        return 0;
                }
            }

            newElement->copyNonAttributeProperties(oldElement);

            if (deep) {
                for (Node* oldChild = oldElement->firstChild(); oldChild; oldChild = oldChild->nextSibling()) {
                    RefPtr<Node> newChild = importNode(oldChild, true, ec);
                    if (ec)
                        return 0;
                    newElement->appendChild(newChild.release(), ec);
                    if (ec)
                        return 0;
                }
            }

            return newElement.release();
        }
        case ATTRIBUTE_NODE:
            return Attr::create(0, this, static_cast<Attr*>(importedNode)->attr()->clone());
        case DOCUMENT_FRAGMENT_NODE: {
            DocumentFragment* oldFragment = static_cast<DocumentFragment*>(importedNode);
            RefPtr<DocumentFragment> newFragment = createDocumentFragment();
            if (deep) {
                for (Node* oldChild = oldFragment->firstChild(); oldChild; oldChild = oldChild->nextSibling()) {
                    RefPtr<Node> newChild = importNode(oldChild, true, ec);
                    if (ec)
                        return 0;
                    newFragment->appendChild(newChild.release(), ec);
                    if (ec)
                        return 0;
                }
            }
            
            return newFragment.release();
        }
        case ENTITY_NODE:
        case NOTATION_NODE:
            // FIXME: It should be possible to import these node types, however in DOM3 the DocumentType is readonly, so there isn't much sense in doing that.
            // Ability to add these imported nodes to a DocumentType will be considered for addition to a future release of the DOM.
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
        case XPATH_NAMESPACE_NODE:
            break;
    }

    ec = NOT_SUPPORTED_ERR;
    return 0;
}


PassRefPtr<Node> Document::adoptNode(PassRefPtr<Node> source, ExceptionCode& ec)
{
    if (!source) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }

    if (source->isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }

    switch (source->nodeType()) {
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
        case XPATH_NAMESPACE_NODE:
            ec = NOT_SUPPORTED_ERR;
            return 0;            
        case ATTRIBUTE_NODE: {                   
            Attr* attr = static_cast<Attr*>(source.get());
            if (attr->ownerElement())
                attr->ownerElement()->removeAttributeNode(attr, ec);
            attr->setSpecified(true);
            break;
        }       
        default:
            if (source->parentNode())
                source->parentNode()->removeChild(source.get(), ec);
    }
                
    for (Node* node = source.get(); node; node = node->traverseNextNode(source.get()))
        node->setDocument(this);

    return source;
}

bool Document::hasPrefixNamespaceMismatch(const QualifiedName& qName)
{
    DEFINE_STATIC_LOCAL(const AtomicString, xmlnsNamespaceURI, ("http://www.w3.org/2000/xmlns/"));
    DEFINE_STATIC_LOCAL(const AtomicString, xmlns, ("xmlns"));
    DEFINE_STATIC_LOCAL(const AtomicString, xml, ("xml"));

    // These checks are from DOM Core Level 2, createElementNS
    // http://www.w3.org/TR/DOM-Level-2-Core/core.html#ID-DocCrElNS
    if (!qName.prefix().isEmpty() && qName.namespaceURI().isNull()) // createElementNS(null, "html:div")
        return true;
    if (qName.prefix() == xml && qName.namespaceURI() != XMLNames::xmlNamespaceURI) // createElementNS("http://www.example.com", "xml:lang")
        return true;

    // Required by DOM Level 3 Core and unspecified by DOM Level 2 Core:
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core.html#ID-DocCrElNS
    // createElementNS("http://www.w3.org/2000/xmlns/", "foo:bar"), createElementNS(null, "xmlns:bar")
    if ((qName.prefix() == xmlns && qName.namespaceURI() != xmlnsNamespaceURI) || (qName.prefix() != xmlns && qName.namespaceURI() == xmlnsNamespaceURI))
        return true;

    return false;
}

// FIXME: This should really be in a possible ElementFactory class
PassRefPtr<Element> Document::createElement(const QualifiedName& qName, bool createdByParser)
{
    RefPtr<Element> e;

    // FIXME: Use registered namespaces and look up in a hash to find the right factory.
    if (qName.namespaceURI() == xhtmlNamespaceURI)
        e = HTMLElementFactory::createHTMLElement(qName, this, 0, createdByParser);
#if ENABLE(SVG)
    else if (qName.namespaceURI() == SVGNames::svgNamespaceURI)
        e = SVGElementFactory::createSVGElement(qName, this, createdByParser);
#endif
#if ENABLE(WML)
    else if (qName.namespaceURI() == WMLNames::wmlNamespaceURI)
        e = WMLElementFactory::createWMLElement(qName, this, createdByParser);
    else if (isWMLDocument())
        e = WMLElementFactory::createWMLElement(QualifiedName(nullAtom, qName.localName(), WMLNames::wmlNamespaceURI), this, createdByParser);
#endif
#if ENABLE(MATHML)
    else if (qName.namespaceURI() == MathMLNames::mathmlNamespaceURI)
        e = MathMLElementFactory::createMathMLElement(qName, this, createdByParser);
#endif
    
    if (!e)
        e = Element::create(qName, document());

    // <image> uses imgTag so we need a special rule.
#if ENABLE(WML)
    if (!isWMLDocument())
#endif
    ASSERT((qName.matches(imageTag) && e->tagQName().matches(imgTag) && e->tagQName().prefix() == qName.prefix()) || qName == e->tagQName());

    return e.release();
}

PassRefPtr<Element> Document::createElementNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode& ec)
{
    String prefix, localName;
    if (!parseQualifiedName(qualifiedName, prefix, localName, ec))
        return 0;

    QualifiedName qName(prefix, localName, namespaceURI);
    if (hasPrefixNamespaceMismatch(qName)) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    return createElement(qName, false);
}

Element* Document::getElementById(const AtomicString& elementId) const
{
    if (elementId.isEmpty())
        return 0;

    Element* element = m_elementsById.get(elementId.impl());
    if (element)
        return element;

    if (m_duplicateIds.contains(elementId.impl())) {
        // We know there's at least one node with this id, but we don't know what the first one is.
        for (Node *n = traverseNextNode(); n != 0; n = n->traverseNextNode()) {
            if (n->isElementNode()) {
                element = static_cast<Element*>(n);
                if (element->hasID() && element->getAttribute(idAttr) == elementId) {
                    m_duplicateIds.remove(elementId.impl());
                    m_elementsById.set(elementId.impl(), element);
                    return element;
                }
            }
        }
        ASSERT_NOT_REACHED();
    }
    return 0;
}

String Document::readyState() const
{
    if (Frame* f = frame()) {
        if (f->loader()->isComplete()) 
            return "complete";
        if (parsing()) 
            return "loading";
      return "loaded";
      // FIXME: What does "interactive" mean?
      // FIXME: Missing support for "uninitialized".
    }
    return String();
}

String Document::encoding() const
{
    if (TextResourceDecoder* d = decoder())
        return d->encoding().domName();
    return String();
}

String Document::defaultCharset() const
{
    if (Settings* settings = this->settings())
        return settings->defaultTextEncodingName();
    return String();
}

void Document::setCharset(const String& charset)
{
    if (!decoder())
        return;
    decoder()->setEncoding(charset, TextResourceDecoder::UserChosenEncoding);
}

void Document::setXMLVersion(const String& version, ExceptionCode& ec)
{
    if (!implementation()->hasFeature("XML", String())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
   
    // FIXME: Also raise NOT_SUPPORTED_ERR if the version is set to a value that is not supported by this Document.

    m_xmlVersion = version;
}

void Document::setXMLStandalone(bool standalone, ExceptionCode& ec)
{
    if (!implementation()->hasFeature("XML", String())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    m_xmlStandalone = standalone;
}

void Document::setDocumentURI(const String& uri)
{
    m_documentURI = uri;
    updateBaseURL();
}

KURL Document::baseURI() const
{
    return m_baseURL;
}

Element* Document::elementFromPoint(int x, int y) const
{
    // FIXME: Share code between this and caretRangeFromPoint.
    if (!renderer())
        return 0;
    Frame* frame = this->frame();
    if (!frame)
        return 0;
    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;

    float zoomFactor = frame->pageZoomFactor();
    IntPoint point = roundedIntPoint(FloatPoint(x * zoomFactor, y * zoomFactor)) + view()->scrollOffset();

    if (!frameView->visibleContentRect().contains(point))
        return 0;

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
    HitTestResult result(point);
    renderView()->layer()->hitTest(request, result);

    Node* n = result.innerNode();
    while (n && !n->isElementNode())
        n = n->parentNode();
    if (n)
        n = n->shadowAncestorNode();
    return static_cast<Element*>(n);
}

PassRefPtr<Range> Document::caretRangeFromPoint(int x, int y)
{
    // FIXME: Share code between this and elementFromPoint.
    if (!renderer())
        return 0;
    Frame* frame = this->frame();
    if (!frame)
        return 0;
    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;

    float zoomFactor = frame->pageZoomFactor();
    IntPoint point = roundedIntPoint(FloatPoint(x * zoomFactor, y * zoomFactor)) + view()->scrollOffset();

    if (!frameView->visibleContentRect().contains(point))
        return 0;

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
    HitTestResult result(point);
    renderView()->layer()->hitTest(request, result);

    Node* node = result.innerNode();
    if (!node)
        return 0;

    Node* shadowAncestorNode = node->shadowAncestorNode();
    if (shadowAncestorNode != node) {
        unsigned offset = shadowAncestorNode->nodeIndex();
        Node* container = shadowAncestorNode->parentNode();
        return Range::create(this, container, offset, container, offset);
    }

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return 0;
    VisiblePosition visiblePosition = renderer->positionForPoint(result.localPoint());
    if (visiblePosition.isNull())
        return 0;

    Position rangeCompliantPosition = rangeCompliantEquivalent(visiblePosition);
    return Range::create(this, rangeCompliantPosition, rangeCompliantPosition);
}

void Document::addElementById(const AtomicString& elementId, Element* element)
{
    typedef HashMap<AtomicStringImpl*, Element*>::iterator iterator;
    if (!m_duplicateIds.contains(elementId.impl())) {
        // Fast path. The ID is not already in m_duplicateIds, so we assume that it's
        // also not already in m_elementsById and do an add. If that add succeeds, we're done.
        pair<iterator, bool> addResult = m_elementsById.add(elementId.impl(), element);
        if (addResult.second)
            return;
        // The add failed, so this ID was already cached in m_elementsById.
        // There are multiple elements with this ID. Remove the m_elementsById
        // cache for this ID so getElementById searches for it next time it is called.
        m_elementsById.remove(addResult.first);
        m_duplicateIds.add(elementId.impl());
    } else {
        // There are multiple elements with this ID. If it exists, remove the m_elementsById
        // cache for this ID so getElementById searches for it next time it is called.
        iterator cachedItem = m_elementsById.find(elementId.impl());
        if (cachedItem != m_elementsById.end()) {
            m_elementsById.remove(cachedItem);
            m_duplicateIds.add(elementId.impl());
        }
    }
    m_duplicateIds.add(elementId.impl());
}

void Document::removeElementById(const AtomicString& elementId, Element* element)
{
    if (m_elementsById.get(elementId.impl()) == element)
        m_elementsById.remove(elementId.impl());
    else
        m_duplicateIds.remove(elementId.impl());
}

Element* Document::getElementByAccessKey(const String& key) const
{
    if (key.isEmpty())
        return 0;
    if (!m_accessKeyMapValid) {
        for (Node* n = firstChild(); n; n = n->traverseNextNode()) {
            if (!n->isElementNode())
                continue;
            Element* element = static_cast<Element*>(n);
            const AtomicString& accessKey = element->getAttribute(accesskeyAttr);
            if (!accessKey.isEmpty())
                m_elementsByAccessKey.set(accessKey.impl(), element);
        }
        m_accessKeyMapValid = true;
    }
    return m_elementsByAccessKey.get(key.impl());
}

void Document::updateTitle()
{
    if (Frame* f = frame())
        f->loader()->setTitle(m_title);
}

void Document::setTitle(const String& title, Element* titleElement)
{
    if (!titleElement) {
        // Title set by JavaScript -- overrides any title elements.
        m_titleSetExplicitly = true;
        if (!isHTMLDocument())
            m_titleElement = 0;
        else if (!m_titleElement) {
            if (HTMLElement* headElement = head()) {
                m_titleElement = createElement(titleTag, false);
                ExceptionCode ec = 0;
                headElement->appendChild(m_titleElement, ec);
                ASSERT(!ec);
            }
        }
    } else if (titleElement != m_titleElement) {
        if (m_titleElement || m_titleSetExplicitly)
            // Only allow the first title element to change the title -- others have no effect.
            return;
        m_titleElement = titleElement;
    }

    if (m_title == title)
        return;

    m_title = title;
    updateTitle();

    if (m_titleSetExplicitly && m_titleElement && m_titleElement->hasTagName(titleTag))
        static_cast<HTMLTitleElement*>(m_titleElement.get())->setText(m_title);
}

void Document::removeTitle(Element* titleElement)
{
    if (m_titleElement != titleElement)
        return;

    m_titleElement = 0;
    m_titleSetExplicitly = false;

    // Update title based on first title element in the head, if one exists.
    if (HTMLElement* headElement = head()) {
        for (Node* e = headElement->firstChild(); e; e = e->nextSibling())
            if (e->hasTagName(titleTag)) {
                HTMLTitleElement* titleElement = static_cast<HTMLTitleElement*>(e);
                setTitle(titleElement->text(), titleElement);
                break;
            }
    }

    if (!m_titleElement && !m_title.isEmpty()) {
        m_title = "";
        updateTitle();
    }
}

String Document::nodeName() const
{
    return "#document";
}

Node::NodeType Document::nodeType() const
{
    return DOCUMENT_NODE;
}

FrameView* Document::view() const
{
    return m_frame ? m_frame->view() : 0;
}

Page* Document::page() const
{
    return m_frame ? m_frame->page() : 0;    
}

Settings* Document::settings() const
{
    return m_frame ? m_frame->settings() : 0;
}

PassRefPtr<Range> Document::createRange()
{
    return Range::create(this);
}

PassRefPtr<NodeIterator> Document::createNodeIterator(Node* root, unsigned whatToShow, 
    PassRefPtr<NodeFilter> filter, bool expandEntityReferences, ExceptionCode& ec)
{
    if (!root) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return NodeIterator::create(root, whatToShow, filter, expandEntityReferences);
}

PassRefPtr<TreeWalker> Document::createTreeWalker(Node *root, unsigned whatToShow, 
    PassRefPtr<NodeFilter> filter, bool expandEntityReferences, ExceptionCode& ec)
{
    if (!root) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return TreeWalker::create(root, whatToShow, filter, expandEntityReferences);
}

void Document::scheduleStyleRecalc()
{
    if (m_styleRecalcTimer.isActive() || inPageCache())
        return;

    ASSERT(childNeedsStyleRecalc());

    if (!documentsThatNeedStyleRecalc)
        documentsThatNeedStyleRecalc = new HashSet<Document*>;
    documentsThatNeedStyleRecalc->add(this);
    
    // FIXME: Why on earth is this here? This is clearly misplaced.
    if (m_accessKeyMapValid) {
        m_accessKeyMapValid = false;
        m_elementsByAccessKey.clear();
    }
    
    m_styleRecalcTimer.startOneShot(0);
}

void Document::unscheduleStyleRecalc()
{
    ASSERT(!childNeedsStyleRecalc());

    if (documentsThatNeedStyleRecalc)
        documentsThatNeedStyleRecalc->remove(this);

    m_styleRecalcTimer.stop();
}

void Document::styleRecalcTimerFired(Timer<Document>*)
{
    updateStyleIfNeeded();
}

bool Document::childNeedsAndNotInStyleRecalc()
{
    return childNeedsStyleRecalc() && !m_inStyleRecalc;
}

void Document::recalcStyle(StyleChange change)
{
    // we should not enter style recalc while painting
    if (view() && view()->isPainting()) {
        ASSERT(!view()->isPainting());
        return;
    }
    
    if (m_inStyleRecalc)
        return; // Guard against re-entrancy. -dwh

#if ENABLE(INSPECTOR)
    InspectorTimelineAgent* timelineAgent = inspectorTimelineAgent();
    if (timelineAgent)
        timelineAgent->willRecalculateStyle();
#endif

    m_inStyleRecalc = true;
    suspendPostAttachCallbacks();
    if (view())
        view()->pauseScheduledEvents();
    
    ASSERT(!renderer() || renderArena());
    if (!renderer() || !renderArena())
        goto bail_out;

    if (change == Force) {
        // style selector may set this again during recalc
        m_hasNodesWithPlaceholderStyle = false;
        
        RefPtr<RenderStyle> documentStyle = RenderStyle::create();
        documentStyle->setDisplay(BLOCK);
        documentStyle->setVisuallyOrdered(visuallyOrdered);
        documentStyle->setZoom(frame()->pageZoomFactor());
        m_styleSelector->setStyle(documentStyle);
    
        FontDescription fontDescription;
        fontDescription.setUsePrinterFont(printing());
        if (Settings* settings = this->settings()) {
            fontDescription.setRenderingMode(settings->fontRenderingMode());
            if (printing() && !settings->shouldPrintBackgrounds())
                documentStyle->setForceBackgroundsToWhite(true);
            const AtomicString& stdfont = settings->standardFontFamily();
            if (!stdfont.isEmpty()) {
                fontDescription.firstFamily().setFamily(stdfont);
                fontDescription.firstFamily().appendFamily(0);
            }
            fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
            m_styleSelector->setFontSize(fontDescription, m_styleSelector->fontSizeForKeyword(CSSValueMedium, inCompatMode(), false));
        }

        documentStyle->setFontDescription(fontDescription);
        documentStyle->font().update(m_styleSelector->fontSelector());
        if (inCompatMode())
            documentStyle->setHtmlHacks(true); // enable html specific rendering tricks

        StyleChange ch = diff(documentStyle.get(), renderer()->style());
        if (renderer() && ch != NoChange)
            renderer()->setStyle(documentStyle.release());
    }

    for (Node* n = firstChild(); n; n = n->nextSibling())
        if (change >= Inherit || n->childNeedsStyleRecalc() || n->needsStyleRecalc())
            n->recalcStyle(change);

#if USE(ACCELERATED_COMPOSITING)
    if (view()) {
        bool layoutPending = view()->layoutPending() || renderer()->needsLayout();
        // If we didn't update compositing layers because of layout(), we need to do so here.
        if (!layoutPending)
            view()->updateCompositingLayers();
    }
#endif

bail_out:
    setNeedsStyleRecalc(NoStyleChange);
    setChildNeedsStyleRecalc(false);
    unscheduleStyleRecalc();

    if (view())
        view()->resumeScheduledEvents();
    resumePostAttachCallbacks();
    m_inStyleRecalc = false;

    // If we wanted to call implicitClose() during recalcStyle, do so now that we're finished.
    if (m_closeAfterStyleRecalc) {
        m_closeAfterStyleRecalc = false;
        implicitClose();
    }

#if ENABLE(INSPECTOR)
    if (timelineAgent)
        timelineAgent->didRecalculateStyle();
#endif
}

void Document::updateStyleIfNeeded()
{
    if (!childNeedsStyleRecalc() || inPageCache())
        return;
        
    if (m_frame)
        m_frame->animation()->beginAnimationUpdate();
        
    recalcStyle(NoChange);
    
    // Tell the animation controller that updateStyleIfNeeded is finished and it can do any post-processing
    if (m_frame)
        m_frame->animation()->endAnimationUpdate();
}

void Document::updateStyleForAllDocuments()
{
    if (!documentsThatNeedStyleRecalc)
        return;

    while (documentsThatNeedStyleRecalc->size()) {
        HashSet<Document*>::iterator it = documentsThatNeedStyleRecalc->begin();
        Document* doc = *it;
        documentsThatNeedStyleRecalc->remove(doc);
        ASSERT(doc->childNeedsStyleRecalc() && !doc->inPageCache());
        doc->updateStyleIfNeeded();
    }
}

void Document::updateLayout()
{
    if (Element* oe = ownerElement())
        oe->document()->updateLayout();

    updateStyleIfNeeded();

    // Only do a layout if changes have occurred that make it necessary.      
    FrameView* v = view();
    if (v && renderer() && (v->layoutPending() || renderer()->needsLayout()))
        v->layout();
}

// FIXME: This is a bad idea and needs to be removed eventually.
// Other browsers load stylesheets before they continue parsing the web page.
// Since we don't, we can run JavaScript code that needs answers before the
// stylesheets are loaded. Doing a layout ignoring the pending stylesheets
// lets us get reasonable answers. The long term solution to this problem is
// to instead suspend JavaScript execution.
void Document::updateLayoutIgnorePendingStylesheets()
{
    bool oldIgnore = m_ignorePendingStylesheets;
    
    if (!haveStylesheetsLoaded()) {
        m_ignorePendingStylesheets = true;
        // FIXME: We are willing to attempt to suppress painting with outdated style info only once.  Our assumption is that it would be
        // dangerous to try to stop it a second time, after page content has already been loaded and displayed
        // with accurate style information.  (Our suppression involves blanking the whole page at the
        // moment.  If it were more refined, we might be able to do something better.)
        // It's worth noting though that this entire method is a hack, since what we really want to do is
        // suspend JS instead of doing a layout with inaccurate information.
        if (body() && !body()->renderer() && m_pendingSheetLayout == NoLayoutWithPendingSheets) {
            m_pendingSheetLayout = DidLayoutWithPendingSheets;
            updateStyleSelector();
        } else if (m_hasNodesWithPlaceholderStyle)
            // If new nodes have been added or style recalc has been done with style sheets still pending, some nodes 
            // may not have had their real style calculated yet. Normally this gets cleaned when style sheets arrive 
            // but here we need up-to-date style immediatly.
            recalcStyle(Force);
    }

    updateLayout();

    m_ignorePendingStylesheets = oldIgnore;
}

void Document::attach()
{
    ASSERT(!attached());
    ASSERT(!m_inPageCache);
    ASSERT(!m_axObjectCache);

    if (!m_renderArena)
        m_renderArena = new RenderArena();
    
    // Create the rendering tree
    setRenderer(new (m_renderArena) RenderView(this, view()));
#if USE(ACCELERATED_COMPOSITING)
    renderView()->didMoveOnscreen();
#endif

    if (!m_styleSelector) {
        bool matchAuthorAndUserStyles = true;
        if (Settings* docSettings = settings())
            matchAuthorAndUserStyles = docSettings->authorAndUserStylesEnabled();
        m_styleSelector = new CSSStyleSelector(this, m_styleSheets.get(), m_mappedElementSheet.get(), pageUserSheet(), pageGroupUserSheets(), 
                                               !inCompatMode(), matchAuthorAndUserStyles);
    }

    recalcStyle(Force);

    RenderObject* render = renderer();
    setRenderer(0);

    ContainerNode::attach();

    setRenderer(render);
}

void Document::detach()
{
    ASSERT(attached());
    ASSERT(!m_inPageCache);

    clearAXObjectCache();
    stopActiveDOMObjects();
    
    RenderObject* render = renderer();

    // Send out documentWillBecomeInactive() notifications to registered elements,
    // in order to stop media elements
    documentWillBecomeInactive();

#if ENABLE(SHARED_WORKERS)
    SharedWorkerRepository::documentDetached(this);
#endif

    if (m_frame) {
        FrameView* view = m_frame->view();
        if (view)
            view->detachCustomScrollbars();
    }

    // indicate destruction mode,  i.e. attached() but renderer == 0
    setRenderer(0);

    m_hoverNode = 0;
    m_focusedNode = 0;
    m_activeNode = 0;

    ContainerNode::detach();

    unscheduleStyleRecalc();

    if (render)
        render->destroy();
    
    // This is required, as our Frame might delete itself as soon as it detaches
    // us. However, this violates Node::detach() symantics, as it's never
    // possible to re-attach. Eventually Document::detach() should be renamed,
    // or this setting of the frame to 0 could be made explicit in each of the
    // callers of Document::detach().
    m_frame = 0;
    
    if (m_renderArena) {
        delete m_renderArena;
        m_renderArena = 0;
    }
}

void Document::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

    if (DOMWindow* domWindow = this->domWindow())
        domWindow->removeAllEventListeners();
    for (Node* node = firstChild(); node; node = node->traverseNextNode())
        node->removeAllEventListeners();
}

RenderView* Document::renderView() const
{
    return toRenderView(renderer());
}

void Document::clearAXObjectCache()
{
    // clear cache in top document
    if (m_axObjectCache) {
        delete m_axObjectCache;
        m_axObjectCache = 0;
        return;
    }
    
    // ask the top-level document to clear its cache
    Document* doc = topDocument();
    if (doc != this)
        doc->clearAXObjectCache();
}

AXObjectCache* Document::axObjectCache() const
{
    // The only document that actually has a AXObjectCache is the top-level
    // document.  This is because we need to be able to get from any WebCoreAXObject
    // to any other WebCoreAXObject on the same page.  Using a single cache allows
    // lookups across nested webareas (i.e. multiple documents).
    
    if (m_axObjectCache) {
        // return already known top-level cache
        if (!ownerElement())
            return m_axObjectCache;
        
        // In some pages with frames, the cache is created before the sub-webarea is
        // inserted into the tree.  Here, we catch that case and just toss the old
        // cache and start over.
        // NOTE: This recovery may no longer be needed. I have been unable to trigger
        // it again. See rdar://5794454
        // FIXME: Can this be fixed when inserting the subframe instead of now?
        // FIXME: If this function was called to get the cache in order to remove
        // an AXObject, we are now deleting the cache as a whole and returning a
        // new empty cache that does not contain the AXObject! That should actually
        // be OK. I am concerned about other cases like this where accessing the
        // cache blows away the AXObject being operated on.
        delete m_axObjectCache;
        m_axObjectCache = 0;
    }

    // ask the top-level document for its cache
    Document* doc = topDocument();
    if (doc != this)
        return doc->axObjectCache();
    
    // this is the top-level document, so install a new cache
    m_axObjectCache = new AXObjectCache;
    return m_axObjectCache;
}

void Document::setVisuallyOrdered()
{
    visuallyOrdered = true;
    if (renderer())
        renderer()->style()->setVisuallyOrdered(true);
}

Tokenizer* Document::createTokenizer()
{
    // FIXME: this should probably pass the frame instead
    return new XMLTokenizer(this, view());
}

void Document::open(Document* ownerDocument)
{
    if (ownerDocument) {
        setURL(ownerDocument->url());
        m_cookieURL = ownerDocument->cookieURL();
        ScriptExecutionContext::setSecurityOrigin(ownerDocument->securityOrigin());
    }

    if (m_frame) {
        if (m_frame->loader()->isLoadingMainResource() || (tokenizer() && tokenizer()->executingScript()))
            return;
    
        if (m_frame->loader()->state() == FrameStateProvisional)
            m_frame->loader()->stopAllLoaders();
    }

    implicitOpen();

    if (m_frame)
        m_frame->loader()->didExplicitOpen();
}

void Document::cancelParsing()
{
    if (m_tokenizer) {
        // We have to clear the tokenizer to avoid possibly triggering
        // the onload handler when closing as a side effect of a cancel-style
        // change, such as opening a new document or closing the window while
        // still parsing
        delete m_tokenizer;
        m_tokenizer = 0;
        close();
    }
}

void Document::implicitOpen()
{
    cancelParsing();

    clear();
    m_tokenizer = createTokenizer();
    setParsing(true);

    if (m_frame)
        m_tokenizer->setXSSAuditor(m_frame->script()->xssAuditor());

    // If we reload, the animation controller sticks around and has
    // a stale animation time. We need to update it here.
    if (m_frame && m_frame->animation())
        m_frame->animation()->beginAnimationUpdate();
}

HTMLElement* Document::body() const
{
    Node* de = documentElement();
    if (!de)
        return 0;
    
    // try to prefer a FRAMESET element over BODY
    Node* body = 0;
    for (Node* i = de->firstChild(); i; i = i->nextSibling()) {
        if (i->hasTagName(framesetTag))
            return static_cast<HTMLElement*>(i);
        
        if (i->hasTagName(bodyTag) && !body)
            body = i;
    }
    return static_cast<HTMLElement*>(body);
}

void Document::setBody(PassRefPtr<HTMLElement> newBody, ExceptionCode& ec)
{
    if (!newBody || !documentElement()) { 
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }

    HTMLElement* b = body();
    if (!b)
        documentElement()->appendChild(newBody, ec);
    else
        documentElement()->replaceChild(newBody, b, ec);
}

HTMLHeadElement* Document::head()
{
    Node* de = documentElement();
    if (!de)
        return 0;

    for (Node* e = de->firstChild(); e; e = e->nextSibling())
        if (e->hasTagName(headTag))
            return static_cast<HTMLHeadElement*>(e);

    return 0;
}

void Document::close()
{
    Frame* frame = this->frame();
    if (frame) {
        // This code calls implicitClose() if all loading has completed.
        FrameLoader* frameLoader = frame->loader();
        frameLoader->endIfNotLoadingMainResource();
        frameLoader->checkCompleted();
    } else {
        // Because we have no frame, we don't know if all loading has completed,
        // so we just call implicitClose() immediately. FIXME: This might fire
        // the load event prematurely <http://bugs.webkit.org/show_bug.cgi?id=14568>.
        implicitClose();
    }
}

void Document::implicitClose()
{
    // If we're in the middle of recalcStyle, we need to defer the close until the style information is accurate and all elements are re-attached.
    if (m_inStyleRecalc) {
        m_closeAfterStyleRecalc = true;
        return;
    }

    bool wasLocationChangePending = frame() && frame()->loader()->isScheduledLocationChangePending();
    bool doload = !parsing() && m_tokenizer && !m_processingLoadEvent && !wasLocationChangePending;
    
    if (!doload)
        return;

    m_processingLoadEvent = true;

    m_wellFormed = m_tokenizer && m_tokenizer->wellFormed();

    // We have to clear the tokenizer, in case someone document.write()s from the
    // onLoad event handler, as in Radar 3206524.
    delete m_tokenizer;
    m_tokenizer = 0;

    // Parser should have picked up all preloads by now
    m_docLoader->clearPreloads();

    // Create a head and a body if we don't have those yet (e.g. for about:blank).
    if (!this->body() && isHTMLDocument()) {
        if (Node* documentElement = this->documentElement()) {
            ExceptionCode ec = 0;
            
            // The implicit <head> isn't expected in older versions of Mail - <rdar://problem/6863795>
            if (!head() && shouldCreateImplicitHead(this)) {
                documentElement->appendChild(new HTMLHeadElement(headTag, this), ec);
                ASSERT(!ec);
            }
            documentElement->appendChild(new HTMLBodyElement(bodyTag, this), ec);
            ASSERT(!ec);
        }
    }

    // FIXME: We kick off the icon loader when the Document is done parsing.
    // There are earlier opportunities we could start it:
    //  -When the <head> finishes parsing
    //  -When any new HTMLLinkElement is inserted into the document
    // But those add a dynamic component to the favicon that has UI 
    // ramifications, and we need to decide what is the Right Thing To Do(tm)
    Frame* f = frame();
    if (f)
        f->loader()->startIconLoader();

    // Resume the animations (or start them)
    if (f)
        f->animation()->resumeAnimations(this);

    ImageLoader::dispatchPendingLoadEvents();
    dispatchWindowLoadEvent();
    dispatchWindowEvent(PageTransitionEvent::create(eventNames().pageshowEvent, false), this);
    if (f)
        f->loader()->handledOnloadEvents();
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("onload fired at %d\n", elapsedTime());
#endif

    m_processingLoadEvent = false;

    // An event handler may have removed the frame
    if (!frame())
        return;

    // Make sure both the initial layout and reflow happen after the onload
    // fires. This will improve onload scores, and other browsers do it.
    // If they wanna cheat, we can too. -dwh

    if (frame()->loader()->isScheduledLocationChangePending() && elapsedTime() < cLayoutScheduleThreshold) {
        // Just bail out. Before or during the onload we were shifted to another page.
        // The old i-Bench suite does this. When this happens don't bother painting or laying out.        
        view()->unscheduleRelayout();
        return;
    }

    frame()->loader()->checkCallImplicitClose();
    RenderObject* renderObject = renderer();
    
    // We used to force a synchronous display and flush here.  This really isn't
    // necessary and can in fact be actively harmful if pages are loading at a rate of > 60fps
    // (if your platform is syncing flushes and limiting them to 60fps).
    m_overMinimumLayoutThreshold = true;
    if (!ownerElement() || (ownerElement()->renderer() && !ownerElement()->renderer()->needsLayout())) {
        updateStyleIfNeeded();
        
        // Always do a layout after loading if needed.
        if (view() && renderObject && (!renderObject->firstChild() || renderObject->needsLayout()))
            view()->layout();
    }

#if PLATFORM(MAC)
    if (f && renderObject && this == topDocument() && AXObjectCache::accessibilityEnabled()) {
        // The AX cache may have been cleared at this point, but we need to make sure it contains an
        // AX object to send the notification to. getOrCreate will make sure that an valid AX object
        // exists in the cache (we ignore the return value because we don't need it here). This is 
        // only safe to call when a layout is not in progress, so it can not be used in postNotification.    
        axObjectCache()->getOrCreate(renderObject);
        axObjectCache()->postNotification(renderObject, AXObjectCache::AXLoadComplete, true);
    }
#endif

#if ENABLE(SVG)
    // FIXME: Officially, time 0 is when the outermost <svg> receives its
    // SVGLoad event, but we don't implement those yet.  This is close enough
    // for now.  In some cases we should have fired earlier.
    if (svgExtensions())
        accessSVGExtensions()->startAnimations();
#endif
}

void Document::setParsing(bool b)
{
    m_bParsing = b;
    if (!m_bParsing && view())
        view()->scheduleRelayout();

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement() && !m_bParsing)
        printf("Parsing finished at %d\n", elapsedTime());
#endif
}

bool Document::shouldScheduleLayout()
{
    // This function will only be called when FrameView thinks a layout is needed.
    // This enforces a couple extra rules.
    //
    //    (a) Only schedule a layout once the stylesheets are loaded.
    //    (b) Only schedule layout once we have a body element.

    return (haveStylesheetsLoaded() && body()) ||
        (documentElement() && !documentElement()->hasTagName(htmlTag));
}

int Document::minimumLayoutDelay()
{
    if (m_overMinimumLayoutThreshold)
        return 0;
    
    int elapsed = elapsedTime();
    m_overMinimumLayoutThreshold = elapsed > cLayoutScheduleThreshold;
    
    // We'll want to schedule the timer to fire at the minimum layout threshold.
    return max(0, cLayoutScheduleThreshold - elapsed);
}

int Document::elapsedTime() const
{
    return static_cast<int>((currentTime() - m_startTime) * 1000);
}

void Document::write(const SegmentedString& text, Document* ownerDocument)
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Beginning a document.write at %d\n", elapsedTime());
#endif

    if (!m_tokenizer)
        open(ownerDocument);

    ASSERT(m_tokenizer);
    m_tokenizer->write(text, false);

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Ending a document.write at %d\n", elapsedTime());
#endif    
}

void Document::write(const String& text, Document* ownerDocument)
{
    write(SegmentedString(text), ownerDocument);
}

void Document::writeln(const String& text, Document* ownerDocument)
{
    write(text, ownerDocument);
    write("\n", ownerDocument);
}

void Document::finishParsing()
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Received all data at %d\n", elapsedTime());
#endif
    
    // Let the tokenizer go through as much data as it can.  There will be three possible outcomes after
    // finish() is called:
    // (1) All remaining data is parsed, document isn't loaded yet
    // (2) All remaining data is parsed, document is loaded, tokenizer gets deleted
    // (3) Data is still remaining to be parsed.
    if (m_tokenizer)
        m_tokenizer->finish();
}

void Document::clear()
{
    delete m_tokenizer;
    m_tokenizer = 0;

    removeChildren();
    if (DOMWindow* domWindow = this->domWindow())
        domWindow->removeAllEventListeners();
}

const KURL& Document::virtualURL() const
{
    return m_url;
}

KURL Document::virtualCompleteURL(const String& url) const
{
    return completeURL(url);
}

void Document::setURL(const KURL& url)
{
    const KURL& newURL = url.isEmpty() ? blankURL() : url;
    if (newURL == m_url)
        return;

    m_url = newURL;
    m_documentURI = m_url.string();
    updateBaseURL();
}

void Document::setBaseElementURL(const KURL& baseElementURL)
{ 
    m_baseElementURL = baseElementURL;
    updateBaseURL();
}

void Document::updateBaseURL()
{
    // DOM 3 Core: When the Document supports the feature "HTML" [DOM Level 2 HTML], the base URI is computed using
    // first the value of the href attribute of the HTML BASE element if any, and the value of the documentURI attribute
    // from the Document interface otherwise.
    if (m_baseElementURL.isEmpty()) {
        // The documentURI attribute is an arbitrary string. DOM 3 Core does not specify how it should be resolved,
        // so we use a null base URL.
        m_baseURL = KURL(KURL(), documentURI());
    } else
        m_baseURL = m_baseElementURL;
    if (!m_baseURL.isValid())
        m_baseURL = KURL();

    if (m_elemSheet)
        m_elemSheet->setHref(m_baseURL.string());
    if (m_mappedElementSheet)
        m_mappedElementSheet->setHref(m_baseURL.string());
}

String Document::userAgent(const KURL& url) const
{
    return frame() ? frame()->loader()->userAgent(url) : String();
}

CSSStyleSheet* Document::pageUserSheet()
{
    if (m_pageUserSheet)
        return m_pageUserSheet.get();
    
    Page* owningPage = page();
    if (!owningPage)
        return 0;
    
    String userSheetText = owningPage->userStyleSheet();
    if (userSheetText.isEmpty())
        return 0;
    
    // Parse the sheet and cache it.
    m_pageUserSheet = CSSStyleSheet::create(this);
    m_pageUserSheet->parseString(userSheetText, !inCompatMode());
    return m_pageUserSheet.get();
}

void Document::clearPageUserSheet()
{
    m_pageUserSheet = 0;
    updateStyleSelector();
}

const Vector<RefPtr<CSSStyleSheet> >* Document::pageGroupUserSheets() const
{
    if (m_pageGroupUserSheetCacheValid)
        return m_pageGroupUserSheets.get();
    
    m_pageGroupUserSheetCacheValid = true;
    
    Page* owningPage = page();
    if (!owningPage)
        return 0;
        
    const PageGroup& pageGroup = owningPage->group();
    const UserStyleSheetMap* sheetsMap = pageGroup.userStyleSheets();
    if (!sheetsMap)
        return 0;

    UserStyleSheetMap::const_iterator end = sheetsMap->end();
    for (UserStyleSheetMap::const_iterator it = sheetsMap->begin(); it != end; ++it) {
        const UserStyleSheetVector* sheets = it->second;
        for (unsigned i = 0; i < sheets->size(); ++i) {
            const UserStyleSheet* sheet = sheets->at(i).get();
            RefPtr<CSSStyleSheet> parsedSheet = CSSStyleSheet::create(const_cast<Document*>(this));
            parsedSheet->parseString(sheet->source(), !inCompatMode());
            if (!m_pageGroupUserSheets)
                m_pageGroupUserSheets.set(new Vector<RefPtr<CSSStyleSheet> >);
            m_pageGroupUserSheets->append(parsedSheet.release());
        }
    }

    return m_pageGroupUserSheets.get();
}

void Document::clearPageGroupUserSheets()
{
    m_pageGroupUserSheets.clear();
    m_pageGroupUserSheetCacheValid = false;
    updateStyleSelector();
}

CSSStyleSheet* Document::elementSheet()
{
    if (!m_elemSheet)
        m_elemSheet = CSSStyleSheet::create(this, m_baseURL.string());
    return m_elemSheet.get();
}

CSSStyleSheet* Document::mappedElementSheet()
{
    if (!m_mappedElementSheet)
        m_mappedElementSheet = CSSStyleSheet::create(this, m_baseURL.string());
    return m_mappedElementSheet.get();
}

static Node* nextNodeWithExactTabIndex(Node* start, int tabIndex, KeyboardEvent* event)
{
    // Search is inclusive of start
    for (Node* n = start; n; n = n->traverseNextNode())
        if (n->isKeyboardFocusable(event) && n->tabIndex() == tabIndex)
            return n;
    
    return 0;
}

static Node* previousNodeWithExactTabIndex(Node* start, int tabIndex, KeyboardEvent* event)
{
    // Search is inclusive of start
    for (Node* n = start; n; n = n->traversePreviousNode())
        if (n->isKeyboardFocusable(event) && n->tabIndex() == tabIndex)
            return n;
    
    return 0;
}

static Node* nextNodeWithGreaterTabIndex(Node* start, int tabIndex, KeyboardEvent* event)
{
    // Search is inclusive of start
    int winningTabIndex = SHRT_MAX + 1;
    Node* winner = 0;
    for (Node* n = start; n; n = n->traverseNextNode())
        if (n->isKeyboardFocusable(event) && n->tabIndex() > tabIndex && n->tabIndex() < winningTabIndex) {
            winner = n;
            winningTabIndex = n->tabIndex();
        }
    
    return winner;
}

static Node* previousNodeWithLowerTabIndex(Node* start, int tabIndex, KeyboardEvent* event)
{
    // Search is inclusive of start
    int winningTabIndex = 0;
    Node* winner = 0;
    for (Node* n = start; n; n = n->traversePreviousNode())
        if (n->isKeyboardFocusable(event) && n->tabIndex() < tabIndex && n->tabIndex() > winningTabIndex) {
            winner = n;
            winningTabIndex = n->tabIndex();
        }
    
    return winner;
}

Node* Document::nextFocusableNode(Node* start, KeyboardEvent* event)
{
    if (start) {
        // If a node is excluded from the normal tabbing cycle, the next focusable node is determined by tree order
        if (start->tabIndex() < 0) {
            for (Node* n = start->traverseNextNode(); n; n = n->traverseNextNode())
                if (n->isKeyboardFocusable(event) && n->tabIndex() >= 0)
                    return n;
        }
    
        // First try to find a node with the same tabindex as start that comes after start in the document.
        if (Node* winner = nextNodeWithExactTabIndex(start->traverseNextNode(), start->tabIndex(), event))
            return winner;

        if (start->tabIndex() == 0)
            // We've reached the last node in the document with a tabindex of 0. This is the end of the tabbing order.
            return 0;
    }

    // Look for the first node in the document that:
    // 1) has the lowest tabindex that is higher than start's tabindex (or 0, if start is null), and
    // 2) comes first in the document, if there's a tie.
    if (Node* winner = nextNodeWithGreaterTabIndex(this, start ? start->tabIndex() : 0, event))
        return winner;

    // There are no nodes with a tabindex greater than start's tabindex,
    // so find the first node with a tabindex of 0.
    return nextNodeWithExactTabIndex(this, 0, event);
}

Node* Document::previousFocusableNode(Node* start, KeyboardEvent* event)
{
    Node* last;
    for (last = this; last->lastChild(); last = last->lastChild())
        ; // Empty loop.

    // First try to find the last node in the document that comes before start and has the same tabindex as start.
    // If start is null, find the last node in the document with a tabindex of 0.
    Node* startingNode;
    int startingTabIndex;
    if (start) {
        startingNode = start->traversePreviousNode();
        startingTabIndex = start->tabIndex();
    } else {
        startingNode = last;
        startingTabIndex = 0;
    }
    
    // However, if a node is excluded from the normal tabbing cycle, the previous focusable node is determined by tree order
    if (startingTabIndex < 0) {
        for (Node* n = startingNode; n; n = n->traversePreviousNode())
            if (n->isKeyboardFocusable(event) && n->tabIndex() >= 0)
                return n;        
    }

    if (Node* winner = previousNodeWithExactTabIndex(startingNode, startingTabIndex, event))
        return winner;

    // There are no nodes before start with the same tabindex as start, so look for a node that:
    // 1) has the highest non-zero tabindex (that is less than start's tabindex), and
    // 2) comes last in the document, if there's a tie.
    startingTabIndex = (start && start->tabIndex()) ? start->tabIndex() : SHRT_MAX;
    return previousNodeWithLowerTabIndex(last, startingTabIndex, event);
}

int Document::nodeAbsIndex(Node *node)
{
    ASSERT(node->document() == this);

    int absIndex = 0;
    for (Node *n = node; n && n != this; n = n->traversePreviousNode())
        absIndex++;
    return absIndex;
}

Node *Document::nodeWithAbsIndex(int absIndex)
{
    Node *n = this;
    for (int i = 0; n && (i < absIndex); i++) {
        n = n->traverseNextNode();
    }
    return n;
}

void Document::processHttpEquiv(const String& equiv, const String& content)
{
    ASSERT(!equiv.isNull() && !content.isNull());

    Frame* frame = this->frame();

    if (equalIgnoringCase(equiv, "default-style")) {
        // The preferred style set has been overridden as per section 
        // 14.3.2 of the HTML4.0 specification.  We need to update the
        // sheet used variable and then update our style selector. 
        // For more info, see the test at:
        // http://www.hixie.ch/tests/evil/css/import/main/preferred.html
        // -dwh
        m_selectedStylesheetSet = content;
        m_preferredStylesheetSet = content;
        updateStyleSelector();
    } else if (equalIgnoringCase(equiv, "refresh")) {
        double delay;
        String url;
        if (frame && parseHTTPRefresh(content, true, delay, url)) {
            if (url.isEmpty())
                url = frame->loader()->url().string();
            else
                url = completeURL(url).string();
            frame->loader()->scheduleHTTPRedirection(delay, url);
        }
    } else if (equalIgnoringCase(equiv, "set-cookie")) {
        // FIXME: make setCookie work on XML documents too; e.g. in case of <html:meta .....>
        if (isHTMLDocument())
            static_cast<HTMLDocument*>(this)->setCookie(content);
    } else if (equalIgnoringCase(equiv, "content-language"))
        setContentLanguage(content);
    else if (equalIgnoringCase(equiv, "x-dns-prefetch-control"))
        parseDNSPrefetchControlHeader(content);
    else if (equalIgnoringCase(equiv, "x-frame-options")) {
        FrameLoader* frameLoader = frame->loader();
        if (frameLoader->shouldInterruptLoadForXFrameOptions(content, url())) {
            frameLoader->stopAllLoaders();
            frameLoader->scheduleLocationChange(blankURL(), String());
        }
    }
}

MouseEventWithHitTestResults Document::prepareMouseEvent(const HitTestRequest& request, const IntPoint& documentPoint, const PlatformMouseEvent& event)
{
    ASSERT(!renderer() || renderer()->isRenderView());

    if (!renderer())
        return MouseEventWithHitTestResults(event, HitTestResult(IntPoint()));

    HitTestResult result(documentPoint);
    renderView()->layer()->hitTest(request, result);

    if (!request.readOnly())
        updateStyleIfNeeded();

    return MouseEventWithHitTestResults(event, result);
}

// DOM Section 1.1.1
bool Document::childTypeAllowed(NodeType type)
{
    switch (type) {
        case ATTRIBUTE_NODE:
        case CDATA_SECTION_NODE:
        case DOCUMENT_FRAGMENT_NODE:
        case DOCUMENT_NODE:
        case ENTITY_NODE:
        case ENTITY_REFERENCE_NODE:
        case NOTATION_NODE:
        case TEXT_NODE:
        case XPATH_NAMESPACE_NODE:
            return false;
        case COMMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
            return true;
        case DOCUMENT_TYPE_NODE:
        case ELEMENT_NODE:
            // Documents may contain no more than one of each of these.
            // (One Element and one DocumentType.)
            for (Node* c = firstChild(); c; c = c->nextSibling())
                if (c->nodeType() == type)
                    return false;
            return true;
    }
    return false;
}

bool Document::canReplaceChild(Node* newChild, Node* oldChild)
{
    if (!oldChild)
        // ContainerNode::replaceChild will raise a NOT_FOUND_ERR.
        return true;

    if (oldChild->nodeType() == newChild->nodeType())
        return true;

    int numDoctypes = 0;
    int numElements = 0;

    // First, check how many doctypes and elements we have, not counting
    // the child we're about to remove.
    for (Node* c = firstChild(); c; c = c->nextSibling()) {
        if (c == oldChild)
            continue;
        
        switch (c->nodeType()) {
            case DOCUMENT_TYPE_NODE:
                numDoctypes++;
                break;
            case ELEMENT_NODE:
                numElements++;
                break;
            default:
                break;
        }
    }
    
    // Then, see how many doctypes and elements might be added by the new child.
    if (newChild->nodeType() == DOCUMENT_FRAGMENT_NODE) {
        for (Node* c = firstChild(); c; c = c->nextSibling()) {
            switch (c->nodeType()) {
                case ATTRIBUTE_NODE:
                case CDATA_SECTION_NODE:
                case DOCUMENT_FRAGMENT_NODE:
                case DOCUMENT_NODE:
                case ENTITY_NODE:
                case ENTITY_REFERENCE_NODE:
                case NOTATION_NODE:
                case TEXT_NODE:
                case XPATH_NAMESPACE_NODE:
                    return false;
                case COMMENT_NODE:
                case PROCESSING_INSTRUCTION_NODE:
                    break;
                case DOCUMENT_TYPE_NODE:
                    numDoctypes++;
                    break;
                case ELEMENT_NODE:
                    numElements++;
                    break;
            }
        }
    } else {
        switch (newChild->nodeType()) {
            case ATTRIBUTE_NODE:
            case CDATA_SECTION_NODE:
            case DOCUMENT_FRAGMENT_NODE:
            case DOCUMENT_NODE:
            case ENTITY_NODE:
            case ENTITY_REFERENCE_NODE:
            case NOTATION_NODE:
            case TEXT_NODE:
            case XPATH_NAMESPACE_NODE:
                return false;
            case COMMENT_NODE:
            case PROCESSING_INSTRUCTION_NODE:
                return true;
            case DOCUMENT_TYPE_NODE:
                numDoctypes++;
                break;
            case ELEMENT_NODE:
                numElements++;
                break;
        }                
    }
        
    if (numElements > 1 || numDoctypes > 1)
        return false;
    
    return true;
}

PassRefPtr<Node> Document::cloneNode(bool /*deep*/)
{
    // Spec says cloning Document nodes is "implementation dependent"
    // so we do not support it...
    return 0;
}

StyleSheetList* Document::styleSheets()
{
    return m_styleSheets.get();
}

String Document::preferredStylesheetSet() const
{
    return m_preferredStylesheetSet;
}

String Document::selectedStylesheetSet() const
{
    return m_selectedStylesheetSet;
}

void Document::setSelectedStylesheetSet(const String& aString)
{
    m_selectedStylesheetSet = aString;
    updateStyleSelector();
    if (renderer())
        renderer()->repaint();
}

// This method is called whenever a top-level stylesheet has finished loading.
void Document::removePendingSheet()
{
    // Make sure we knew this sheet was pending, and that our count isn't out of sync.
    ASSERT(m_pendingStylesheets > 0);

    m_pendingStylesheets--;
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Stylesheet loaded at time %d. %d stylesheets still remain.\n", elapsedTime(), m_pendingStylesheets);
#endif

    updateStyleSelector();
    
    if (!m_pendingStylesheets && m_tokenizer)
        m_tokenizer->executeScriptsWaitingForStylesheets();

    if (!m_pendingStylesheets && m_gotoAnchorNeededAfterStylesheetsLoad && m_frame)
        m_frame->loader()->gotoAnchor();
}

void Document::updateStyleSelector()
{
    // Don't bother updating, since we haven't loaded all our style info yet
    // and haven't calculated the style selector for the first time.
    if (!m_didCalculateStyleSelector && !haveStylesheetsLoaded())
        return;

    if (didLayoutWithPendingStylesheets() && m_pendingStylesheets <= 0) {
        m_pendingSheetLayout = IgnoreLayoutWithPendingSheets;
        if (renderer())
            renderer()->repaint();
    }

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Beginning update of style selector at time %d.\n", elapsedTime());
#endif

    recalcStyleSelector();
    recalcStyle(Force);

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Finished update of style selector at time %d\n", elapsedTime());
#endif

    if (renderer()) {
        renderer()->setNeedsLayoutAndPrefWidthsRecalc();
        if (view())
            view()->scheduleRelayout();
    }
}

void Document::addStyleSheetCandidateNode(Node* node, bool createdByParser)
{
    // Until the <body> exists, we have no choice but to compare document positions,
    // since styles outside of the body and head continue to be shunted into the head
    // (and thus can shift to end up before dynamically added DOM content that is also
    // outside the body).
    if ((createdByParser && body()) || m_styleSheetCandidateNodes.isEmpty()) {
        m_styleSheetCandidateNodes.add(node);
        return;
    }

    // Determine an appropriate insertion point.
    ListHashSet<Node*>::iterator begin = m_styleSheetCandidateNodes.begin();
    ListHashSet<Node*>::iterator end = m_styleSheetCandidateNodes.end();
    ListHashSet<Node*>::iterator it = end;
    Node* followingNode = 0;
    do {
        --it;
        Node* n = *it;
        unsigned short position = n->compareDocumentPosition(node);
        if (position == DOCUMENT_POSITION_FOLLOWING) {
            m_styleSheetCandidateNodes.insertBefore(followingNode, node);
            return;
        }
        followingNode = n;
    } while (it != begin);
    
    m_styleSheetCandidateNodes.insertBefore(followingNode, node);
}

void Document::removeStyleSheetCandidateNode(Node* node)
{
    m_styleSheetCandidateNodes.remove(node);
}

void Document::recalcStyleSelector()
{
    if (!renderer() || !attached())
        return;

    StyleSheetVector sheets;

    bool matchAuthorAndUserStyles = true;
    if (Settings* settings = this->settings())
        matchAuthorAndUserStyles = settings->authorAndUserStylesEnabled();

    ListHashSet<Node*>::iterator begin = m_styleSheetCandidateNodes.begin();
    ListHashSet<Node*>::iterator end = m_styleSheetCandidateNodes.end();
    if (!matchAuthorAndUserStyles)
        end = begin;
    for (ListHashSet<Node*>::iterator it = begin; it != end; ++it) {
        Node* n = *it;

        StyleSheet* sheet = 0;

        if (n->nodeType() == PROCESSING_INSTRUCTION_NODE) {
            // Processing instruction (XML documents only)
            ProcessingInstruction* pi = static_cast<ProcessingInstruction*>(n);
            sheet = pi->sheet();
#if ENABLE(XSLT)
            // Don't apply XSL transforms to already transformed documents -- <rdar://problem/4132806>
            if (pi->isXSL() && !transformSourceDocument()) {
                // Don't apply XSL transforms until loading is finished.
                if (!parsing())
                    applyXSLTransform(pi);
                return;
            }
#endif
            if (!sheet && !pi->localHref().isEmpty()) {
                // Processing instruction with reference to an element in this document - e.g.
                // <?xml-stylesheet href="#mystyle">, with the element
                // <foo id="mystyle">heading { color: red; }</foo> at some location in
                // the document
                Element* elem = getElementById(pi->localHref().impl());
                if (elem) {
                    String sheetText("");
                    for (Node* c = elem->firstChild(); c; c = c->nextSibling()) {
                        if (c->nodeType() == TEXT_NODE || c->nodeType() == CDATA_SECTION_NODE)
                            sheetText += c->nodeValue();
                    }

                    RefPtr<CSSStyleSheet> cssSheet = CSSStyleSheet::create(this);
                    cssSheet->parseString(sheetText);
                    pi->setCSSStyleSheet(cssSheet);
                    sheet = cssSheet.get();
                }
            }
        } else if (n->isHTMLElement() && (n->hasTagName(linkTag) || n->hasTagName(styleTag))
#if ENABLE(SVG)
            ||  (n->isSVGElement() && n->hasTagName(SVGNames::styleTag))
#endif
        ) {
            Element* e = static_cast<Element*>(n);
            AtomicString title = e->getAttribute(titleAttr);
            bool enabledViaScript = false;
            if (e->hasLocalName(linkTag)) {
                // <LINK> element
                HTMLLinkElement* l = static_cast<HTMLLinkElement*>(n);
                if (l->isDisabled())
                    continue;
                enabledViaScript = l->isEnabledViaScript();
                if (l->isLoading()) {
                    // it is loading but we should still decide which style sheet set to use
                    if (!enabledViaScript && !title.isEmpty() && m_preferredStylesheetSet.isEmpty()) {
                        const AtomicString& rel = e->getAttribute(relAttr);
                        if (!rel.contains("alternate")) {
                            m_preferredStylesheetSet = title;
                            m_selectedStylesheetSet = title;
                        }
                    }
                    continue;
                }
                if (!l->sheet())
                    title = nullAtom;
            }

            // Get the current preferred styleset.  This is the
            // set of sheets that will be enabled.
#if ENABLE(SVG)
            if (n->isSVGElement() && n->hasTagName(SVGNames::styleTag))
                sheet = static_cast<SVGStyleElement*>(n)->sheet();
            else
#endif
            if (e->hasLocalName(linkTag))
                sheet = static_cast<HTMLLinkElement*>(n)->sheet();
            else
                // <STYLE> element
                sheet = static_cast<HTMLStyleElement*>(n)->sheet();

            // Check to see if this sheet belongs to a styleset
            // (thus making it PREFERRED or ALTERNATE rather than
            // PERSISTENT).
            if (!enabledViaScript && !title.isEmpty()) {
                // Yes, we have a title.
                if (m_preferredStylesheetSet.isEmpty()) {
                    // No preferred set has been established.  If
                    // we are NOT an alternate sheet, then establish
                    // us as the preferred set.  Otherwise, just ignore
                    // this sheet.
                    AtomicString rel = e->getAttribute(relAttr);
                    if (e->hasLocalName(styleTag) || !rel.contains("alternate"))
                        m_preferredStylesheetSet = m_selectedStylesheetSet = title;
                }

                if (title != m_preferredStylesheetSet)
                    sheet = 0;
            }
        }

        if (sheet)
            sheets.append(sheet);
    }

    m_styleSheets->swap(sheets);

    // Create a new style selector
    delete m_styleSelector;
    m_styleSelector = new CSSStyleSelector(this, m_styleSheets.get(), m_mappedElementSheet.get(), 
                                           pageUserSheet(), pageGroupUserSheets(), !inCompatMode(), matchAuthorAndUserStyles);
    m_didCalculateStyleSelector = true;
}

void Document::setHoverNode(PassRefPtr<Node> newHoverNode)
{
    m_hoverNode = newHoverNode;
}

void Document::setActiveNode(PassRefPtr<Node> newActiveNode)
{
    m_activeNode = newActiveNode;
}

void Document::focusedNodeRemoved()
{
    setFocusedNode(0);
}

void Document::removeFocusedNodeOfSubtree(Node* node, bool amongChildrenOnly)
{
    if (!m_focusedNode || this->inPageCache()) // If the document is in the page cache, then we don't need to clear out the focused node.
        return;
        
    bool nodeInSubtree = false;
    if (amongChildrenOnly)
        nodeInSubtree = m_focusedNode->isDescendantOf(node);
    else
        nodeInSubtree = (m_focusedNode == node) || m_focusedNode->isDescendantOf(node);
    
    if (nodeInSubtree)
        document()->focusedNodeRemoved();
}

void Document::hoveredNodeDetached(Node* node)
{
    if (!m_hoverNode || (node != m_hoverNode && (!m_hoverNode->isTextNode() || node != m_hoverNode->parent())))
        return;

    m_hoverNode = node->parent();
    while (m_hoverNode && !m_hoverNode->renderer())
        m_hoverNode = m_hoverNode->parent();
    if (frame())
        frame()->eventHandler()->scheduleHoverStateUpdate();
}

void Document::activeChainNodeDetached(Node* node)
{
    if (!m_activeNode || (node != m_activeNode && (!m_activeNode->isTextNode() || node != m_activeNode->parent())))
        return;

    m_activeNode = node->parent();
    while (m_activeNode && !m_activeNode->renderer())
        m_activeNode = m_activeNode->parent();
}

#if ENABLE(DASHBOARD_SUPPORT)
const Vector<DashboardRegionValue>& Document::dashboardRegions() const
{
    return m_dashboardRegions;
}

void Document::setDashboardRegions(const Vector<DashboardRegionValue>& regions)
{
    m_dashboardRegions = regions;
    setDashboardRegionsDirty(false);
}
#endif

bool Document::setFocusedNode(PassRefPtr<Node> newFocusedNode)
{    
    // Make sure newFocusedNode is actually in this document
    if (newFocusedNode && (newFocusedNode->document() != this))
        return true;

    if (m_focusedNode == newFocusedNode)
        return true;

    if (m_inPageCache)
        return false;

    bool focusChangeBlocked = false;
    RefPtr<Node> oldFocusedNode = m_focusedNode;
    m_focusedNode = 0;

    // Remove focus from the existing focus node (if any)
    if (oldFocusedNode && !oldFocusedNode->m_inDetach) { 
        if (oldFocusedNode->active())
            oldFocusedNode->setActive(false);

        oldFocusedNode->setFocus(false);
                
        // Dispatch a change event for text fields or textareas that have been edited
        RenderObject* r = oldFocusedNode->renderer();
        if (r && r->isTextControl() && toRenderTextControl(r)->isEdited()) {
            oldFocusedNode->dispatchEvent(Event::create(eventNames().changeEvent, true, false));
            r = oldFocusedNode->renderer();
            if (r && r->isTextControl())
                toRenderTextControl(r)->setEdited(false);
        }

        // Dispatch the blur event and let the node do any other blur related activities (important for text fields)
        oldFocusedNode->dispatchBlurEvent();

        if (m_focusedNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusedNode = 0;
        }
        oldFocusedNode->dispatchUIEvent(eventNames().DOMFocusOutEvent, 0, 0);
        if (m_focusedNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusedNode = 0;
        }
        if (oldFocusedNode == this && oldFocusedNode->hasOneRef())
            return true;
            
        if (oldFocusedNode == oldFocusedNode->rootEditableElement())
            frame()->editor()->didEndEditing();
    }

    if (newFocusedNode) {
        if (newFocusedNode == newFocusedNode->rootEditableElement() && !acceptsEditingFocus(newFocusedNode.get())) {
            // delegate blocks focus change
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }
        // Set focus on the new node
        m_focusedNode = newFocusedNode.get();

        // Dispatch the focus event and let the node do any other focus related activities (important for text fields)
        m_focusedNode->dispatchFocusEvent();

        if (m_focusedNode != newFocusedNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }
        m_focusedNode->dispatchUIEvent(eventNames().DOMFocusInEvent, 0, 0);
        if (m_focusedNode != newFocusedNode) { 
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }
        m_focusedNode->setFocus();

        if (m_focusedNode == m_focusedNode->rootEditableElement())
            frame()->editor()->didBeginEditing();

        // eww, I suck. set the qt focus correctly
        // ### find a better place in the code for this
        if (view()) {
            Widget *focusWidget = widgetForNode(m_focusedNode.get());
            if (focusWidget) {
                // Make sure a widget has the right size before giving it focus.
                // Otherwise, we are testing edge cases of the Widget code.
                // Specifically, in WebCore this does not work well for text fields.
                updateLayout();
                // Re-get the widget in case updating the layout changed things.
                focusWidget = widgetForNode(m_focusedNode.get());
            }
            if (focusWidget)
                focusWidget->setFocus();
            else
                view()->setFocus();
        }
    }

#if ((PLATFORM(MAC) || PLATFORM(WIN)) && !PLATFORM(CHROMIUM)) || PLATFORM(GTK)
    if (!focusChangeBlocked && m_focusedNode && AXObjectCache::accessibilityEnabled()) {
        RenderObject* oldFocusedRenderer = 0;
        RenderObject* newFocusedRenderer = 0;

        if (oldFocusedNode)
            oldFocusedRenderer = oldFocusedNode->renderer();
        if (newFocusedNode)
            newFocusedRenderer = newFocusedNode->renderer();

        axObjectCache()->handleFocusedUIElementChanged(oldFocusedRenderer, newFocusedRenderer);
    }
#endif

SetFocusedNodeDone:
    updateStyleIfNeeded();
    return !focusChangeBlocked;
}
    
void Document::getFocusableNodes(Vector<RefPtr<Node> >& nodes)
{
    updateLayout();

    for (Node* node = firstChild(); node; node = node->traverseNextNode()) {
        if (node->isFocusable())
            nodes.append(node);
    }
}
  
void Document::setCSSTarget(Element* n)
{
    if (m_cssTarget)
        m_cssTarget->setNeedsStyleRecalc();
    m_cssTarget = n;
    if (n)
        n->setNeedsStyleRecalc();
}

void Document::attachNodeIterator(NodeIterator *ni)
{
    m_nodeIterators.add(ni);
}

void Document::detachNodeIterator(NodeIterator *ni)
{
    m_nodeIterators.remove(ni);
}

void Document::nodeChildrenChanged(ContainerNode* container)
{
    if (!disableRangeMutation(page())) {
        HashSet<Range*>::const_iterator end = m_ranges.end();
        for (HashSet<Range*>::const_iterator it = m_ranges.begin(); it != end; ++it)
            (*it)->nodeChildrenChanged(container);
    }
}

void Document::nodeWillBeRemoved(Node* n)
{
    HashSet<NodeIterator*>::const_iterator nodeIteratorsEnd = m_nodeIterators.end();
    for (HashSet<NodeIterator*>::const_iterator it = m_nodeIterators.begin(); it != nodeIteratorsEnd; ++it)
        (*it)->nodeWillBeRemoved(n);

    if (!disableRangeMutation(page())) {
        HashSet<Range*>::const_iterator rangesEnd = m_ranges.end();
        for (HashSet<Range*>::const_iterator it = m_ranges.begin(); it != rangesEnd; ++it)
            (*it)->nodeWillBeRemoved(n);
    }

    if (Frame* frame = this->frame()) {
        frame->selection()->nodeWillBeRemoved(n);
        frame->dragCaretController()->nodeWillBeRemoved(n);
    }
}

void Document::textInserted(Node* text, unsigned offset, unsigned length)
{
    if (!disableRangeMutation(page())) {
        HashSet<Range*>::const_iterator end = m_ranges.end();
        for (HashSet<Range*>::const_iterator it = m_ranges.begin(); it != end; ++it)
            (*it)->textInserted(text, offset, length);
    }

    // Update the markers for spelling and grammar checking.
    shiftMarkers(text, offset, length);
}

void Document::textRemoved(Node* text, unsigned offset, unsigned length)
{
    if (!disableRangeMutation(page())) {
        HashSet<Range*>::const_iterator end = m_ranges.end();
        for (HashSet<Range*>::const_iterator it = m_ranges.begin(); it != end; ++it)
            (*it)->textRemoved(text, offset, length);
    }

    // Update the markers for spelling and grammar checking.
    removeMarkers(text, offset, length);
    shiftMarkers(text, offset + length, 0 - length);
}

void Document::textNodesMerged(Text* oldNode, unsigned offset)
{
    if (!disableRangeMutation(page())) {
        NodeWithIndex oldNodeWithIndex(oldNode);
        HashSet<Range*>::const_iterator end = m_ranges.end();
        for (HashSet<Range*>::const_iterator it = m_ranges.begin(); it != end; ++it)
            (*it)->textNodesMerged(oldNodeWithIndex, offset);
    }

    // FIXME: This should update markers for spelling and grammar checking.
}

void Document::textNodeSplit(Text* oldNode)
{
    if (!disableRangeMutation(page())) {
        HashSet<Range*>::const_iterator end = m_ranges.end();
        for (HashSet<Range*>::const_iterator it = m_ranges.begin(); it != end; ++it)
            (*it)->textNodeSplit(oldNode);
    }

    // FIXME: This should update markers for spelling and grammar checking.
}

// FIXME: eventually, this should return a DOMWindow stored in the document.
DOMWindow* Document::domWindow() const
{
    if (!frame())
        return 0;

    // The m_frame pointer is not (not always?) zeroed out when the document is put into b/f cache, so the frame can hold an unrelated document/window pair.
    // FIXME: We should always zero out the frame pointer on navigation to avoid accidentally accessing the new frame content.
    if (m_frame->document() != this)
        return 0;

    return frame()->domWindow();
}

void Document::setWindowAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener)
{
    DOMWindow* domWindow = this->domWindow();
    if (!domWindow)
        return;
    domWindow->setAttributeEventListener(eventType, listener);
}

EventListener* Document::getWindowAttributeEventListener(const AtomicString& eventType)
{
    DOMWindow* domWindow = this->domWindow();
    if (!domWindow)
        return 0;
    return domWindow->getAttributeEventListener(eventType);
}

void Document::dispatchWindowEvent(PassRefPtr<Event> event,  PassRefPtr<EventTarget> target)
{
    ASSERT(!eventDispatchForbidden());
    DOMWindow* domWindow = this->domWindow();
    if (!domWindow)
        return;
    domWindow->dispatchEvent(event, target);
}

void Document::dispatchWindowLoadEvent()
{
    ASSERT(!eventDispatchForbidden());
    DOMWindow* domWindow = this->domWindow();
    if (!domWindow)
        return;
    domWindow->dispatchLoadEvent();
}

PassRefPtr<Event> Document::createEvent(const String& eventType, ExceptionCode& ec)
{
    if (eventType == "Event" || eventType == "Events" || eventType == "HTMLEvents")
        return Event::create();
    if (eventType == "KeyboardEvent" || eventType == "KeyboardEvents")
        return KeyboardEvent::create();
    if (eventType == "MessageEvent")
        return MessageEvent::create();
    if (eventType == "MouseEvent" || eventType == "MouseEvents")
        return MouseEvent::create();
    if (eventType == "MutationEvent" || eventType == "MutationEvents")
        return MutationEvent::create();
    if (eventType == "OverflowEvent")
        return OverflowEvent::create();
    if (eventType == "PageTransitionEvent")
        return PageTransitionEvent::create();
    if (eventType == "ProgressEvent")
        return ProgressEvent::create();
#if ENABLE(DOM_STORAGE)
    if (eventType == "StorageEvent")
        return StorageEvent::create();
#endif
    if (eventType == "TextEvent")
        return TextEvent::create();
    if (eventType == "UIEvent" || eventType == "UIEvents")
        return UIEvent::create();
    if (eventType == "WebKitAnimationEvent")
        return WebKitAnimationEvent::create();
    if (eventType == "WebKitTransitionEvent")
        return WebKitTransitionEvent::create();
    if (eventType == "WheelEvent")
        return WheelEvent::create();
#if ENABLE(SVG)
    if (eventType == "SVGEvents")
        return Event::create();
    if (eventType == "SVGZoomEvents")
        return SVGZoomEvent::create();
#endif
    ec = NOT_SUPPORTED_ERR;
    return 0;
}

void Document::addListenerTypeIfNeeded(const AtomicString& eventType)
{
    if (eventType == eventNames().DOMSubtreeModifiedEvent)
        addListenerType(DOMSUBTREEMODIFIED_LISTENER);
    else if (eventType == eventNames().DOMNodeInsertedEvent)
        addListenerType(DOMNODEINSERTED_LISTENER);
    else if (eventType == eventNames().DOMNodeRemovedEvent)
        addListenerType(DOMNODEREMOVED_LISTENER);
    else if (eventType == eventNames().DOMNodeRemovedFromDocumentEvent)
        addListenerType(DOMNODEREMOVEDFROMDOCUMENT_LISTENER);
    else if (eventType == eventNames().DOMNodeInsertedIntoDocumentEvent)
        addListenerType(DOMNODEINSERTEDINTODOCUMENT_LISTENER);
    else if (eventType == eventNames().DOMAttrModifiedEvent)
        addListenerType(DOMATTRMODIFIED_LISTENER);
    else if (eventType == eventNames().DOMCharacterDataModifiedEvent)
        addListenerType(DOMCHARACTERDATAMODIFIED_LISTENER);
    else if (eventType == eventNames().overflowchangedEvent)
        addListenerType(OVERFLOWCHANGED_LISTENER);
    else if (eventType == eventNames().webkitAnimationStartEvent)
        addListenerType(ANIMATIONSTART_LISTENER);
    else if (eventType == eventNames().webkitAnimationEndEvent)
        addListenerType(ANIMATIONEND_LISTENER);
    else if (eventType == eventNames().webkitAnimationIterationEvent)
        addListenerType(ANIMATIONITERATION_LISTENER);
    else if (eventType == eventNames().webkitTransitionEndEvent)
        addListenerType(TRANSITIONEND_LISTENER);
}

CSSStyleDeclaration* Document::getOverrideStyle(Element*, const String&)
{
    return 0;
}

Element* Document::ownerElement() const
{
    if (!frame())
        return 0;
    return frame()->ownerElement();
}

String Document::cookie() const
{
    if (page() && !page()->cookieEnabled())
        return String();

    KURL cookieURL = this->cookieURL();
    if (cookieURL.isEmpty())
        return String();

    return cookies(this, cookieURL);
}

void Document::setCookie(const String& value)
{
    if (page() && !page()->cookieEnabled())
        return;

    KURL cookieURL = this->cookieURL();
    if (cookieURL.isEmpty())
        return;

    setCookies(this, cookieURL, value);
}

String Document::referrer() const
{
    if (frame())
        return frame()->loader()->referrer();
    return String();
}

String Document::domain() const
{
    return securityOrigin()->domain();
}

void Document::setDomain(const String& newDomain, ExceptionCode& ec)
{
    // Both NS and IE specify that changing the domain is only allowed when
    // the new domain is a suffix of the old domain.

    // FIXME: We should add logging indicating why a domain was not allowed.

    // If the new domain is the same as the old domain, still call
    // securityOrigin()->setDomainForDOM. This will change the
    // security check behavior. For example, if a page loaded on port 8000
    // assigns its current domain using document.domain, the page will
    // allow other pages loaded on different ports in the same domain that
    // have also assigned to access this page.
    if (equalIgnoringCase(domain(), newDomain)) {
        securityOrigin()->setDomainFromDOM(newDomain);
        if (m_frame)
            m_frame->script()->updateSecurityOrigin();
        return;
    }

    int oldLength = domain().length();
    int newLength = newDomain.length();
    // e.g. newDomain = webkit.org (10) and domain() = www.webkit.org (14)
    if (newLength >= oldLength) {
        ec = SECURITY_ERR;
        return;
    }

    String test = domain();
    // Check that it's a subdomain, not e.g. "ebkit.org"
    if (test[oldLength - newLength - 1] != '.') {
        ec = SECURITY_ERR;
        return;
    }

    // Now test is "webkit.org" from domain()
    // and we check that it's the same thing as newDomain
    test.remove(0, oldLength - newLength);
    if (test != newDomain) {
        ec = SECURITY_ERR;
        return;
    }

    securityOrigin()->setDomainFromDOM(newDomain);
    if (m_frame)
        m_frame->script()->updateSecurityOrigin();
}

String Document::lastModified() const
{
    Frame* f = frame();
    if (!f)
        return String();
    DocumentLoader* loader = f->loader()->documentLoader();
    if (!loader)
        return String();
    return loader->response().httpHeaderField("Last-Modified");
}

static bool isValidNameNonASCII(const UChar* characters, unsigned length)
{
    unsigned i = 0;

    UChar32 c;
    U16_NEXT(characters, i, length, c)
    if (!isValidNameStart(c))
        return false;

    while (i < length) {
        U16_NEXT(characters, i, length, c)
        if (!isValidNamePart(c))
            return false;
    }

    return true;
}

static inline bool isValidNameASCII(const UChar* characters, unsigned length)
{
    UChar c = characters[0];
    if (!(isASCIIAlpha(c) || c == ':' || c == '_'))
        return false;

    for (unsigned i = 1; i < length; ++i) {
        c = characters[i];
        if (!(isASCIIAlphanumeric(c) || c == ':' || c == '_' || c == '-' || c == '.'))
            return false;
    }

    return true;
}

bool Document::isValidName(const String& name)
{
    unsigned length = name.length();
    if (!length)
        return false;

    const UChar* characters = name.characters();
    return isValidNameASCII(characters, length) || isValidNameNonASCII(characters, length);
}

bool Document::parseQualifiedName(const String& qualifiedName, String& prefix, String& localName, ExceptionCode& ec)
{
    unsigned length = qualifiedName.length();

    if (length == 0) {
        ec = INVALID_CHARACTER_ERR;
        return false;
    }

    bool nameStart = true;
    bool sawColon = false;
    int colonPos = 0;

    const UChar* s = qualifiedName.characters();
    for (unsigned i = 0; i < length;) {
        UChar32 c;
        U16_NEXT(s, i, length, c)
        if (c == ':') {
            if (sawColon) {
                ec = NAMESPACE_ERR;
                return false; // multiple colons: not allowed
            }
            nameStart = true;
            sawColon = true;
            colonPos = i - 1;
        } else if (nameStart) {
            if (!isValidNameStart(c)) {
                ec = INVALID_CHARACTER_ERR;
                return false;
            }
            nameStart = false;
        } else {
            if (!isValidNamePart(c)) {
                ec = INVALID_CHARACTER_ERR;
                return false;
            }
        }
    }

    if (!sawColon) {
        prefix = String();
        localName = qualifiedName;
    } else {
        prefix = qualifiedName.substring(0, colonPos);
        if (prefix.isEmpty()) {
            ec = NAMESPACE_ERR;
            return false;
        }
        localName = qualifiedName.substring(colonPos + 1);
    }

    if (localName.isEmpty()) {
        ec = NAMESPACE_ERR;
        return false;
    }

    return true;
}

void Document::addImageMap(HTMLMapElement* imageMap)
{
    const AtomicString& name = imageMap->getName();
    if (!name.impl())
        return;

    // Add the image map, unless there's already another with that name.
    // "First map wins" is the rule other browsers seem to implement.
    m_imageMapsByName.add(name.impl(), imageMap);
}

void Document::removeImageMap(HTMLMapElement* imageMap)
{
    // Remove the image map by name.
    // But don't remove some other image map that just happens to have the same name.
    // FIXME: Use a HashCountedSet as we do for IDs to find the first remaining map
    // once a map has been removed.
    const AtomicString& name = imageMap->getName();
    if (!name.impl())
        return;

    ImageMapsByName::iterator it = m_imageMapsByName.find(name.impl());
    if (it != m_imageMapsByName.end() && it->second == imageMap)
        m_imageMapsByName.remove(it);
}

HTMLMapElement *Document::getImageMap(const String& url) const
{
    if (url.isNull())
        return 0;
    int hashPos = url.find('#');
    String name = (hashPos < 0 ? url : url.substring(hashPos + 1)).impl();
    AtomicString mapName = isHTMLDocument() ? name.lower() : name;
    return m_imageMapsByName.get(mapName.impl());
}

void Document::setDecoder(PassRefPtr<TextResourceDecoder> decoder)
{
    m_decoder = decoder;
}

KURL Document::completeURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the KURL constructor to have this behavior?
    // See also [CSS]StyleSheet::completeURL(const String&)
    if (url.isNull())
        return KURL();
    const KURL& baseURL = ((m_baseURL.isEmpty() || m_baseURL == blankURL()) && parentDocument()) ? parentDocument()->baseURL() : m_baseURL;
    if (!m_decoder)
        return KURL(baseURL, url);
    return KURL(baseURL, url, m_decoder->encoding());
}

void Document::setInPageCache(bool flag)
{
    if (m_inPageCache == flag)
        return;

    m_inPageCache = flag;
    if (flag) {
        ASSERT(m_savedRenderer == 0);
        m_savedRenderer = renderer();
        if (FrameView* v = view())
            v->resetScrollbars();
        unscheduleStyleRecalc();
    } else {
        ASSERT(renderer() == 0 || renderer() == m_savedRenderer);
        ASSERT(m_renderArena);
        setRenderer(m_savedRenderer);
        m_savedRenderer = 0;
        if (childNeedsStyleRecalc())
            scheduleStyleRecalc();
    }
}

void Document::documentWillBecomeInactive() 
{
#if USE(ACCELERATED_COMPOSITING)
    if (renderer())
        renderView()->willMoveOffscreen();
#endif

    HashSet<Element*>::iterator end = m_documentActivationCallbackElements.end();
    for (HashSet<Element*>::iterator i = m_documentActivationCallbackElements.begin(); i != end; ++i)
        (*i)->documentWillBecomeInactive();
}

void Document::documentDidBecomeActive() 
{
    HashSet<Element*>::iterator end = m_documentActivationCallbackElements.end();
    for (HashSet<Element*>::iterator i = m_documentActivationCallbackElements.begin(); i != end; ++i)
        (*i)->documentDidBecomeActive();

#if USE(ACCELERATED_COMPOSITING)
    if (renderer())
        renderView()->didMoveOnscreen();
#endif
}

void Document::registerForDocumentActivationCallbacks(Element* e)
{
    m_documentActivationCallbackElements.add(e);
}

void Document::unregisterForDocumentActivationCallbacks(Element* e)
{
    m_documentActivationCallbackElements.remove(e);
}

void Document::mediaVolumeDidChange() 
{
    HashSet<Element*>::iterator end = m_mediaVolumeCallbackElements.end();
    for (HashSet<Element*>::iterator i = m_mediaVolumeCallbackElements.begin(); i != end; ++i)
        (*i)->mediaVolumeDidChange();
}

void Document::registerForMediaVolumeCallbacks(Element* e)
{
    m_mediaVolumeCallbackElements.add(e);
}

void Document::unregisterForMediaVolumeCallbacks(Element* e)
{
    m_mediaVolumeCallbackElements.remove(e);
}

void Document::setShouldCreateRenderers(bool f)
{
    m_createRenderers = f;
}

bool Document::shouldCreateRenderers()
{
    return m_createRenderers;
}

// Support for Javascript execCommand, and related methods

static Editor::Command command(Document* document, const String& commandName, bool userInterface = false)
{
    Frame* frame = document->frame();
    if (!frame || frame->document() != document)
        return Editor::Command();
    return frame->editor()->command(commandName,
        userInterface ? CommandFromDOMWithUserInterface : CommandFromDOM);
}

bool Document::execCommand(const String& commandName, bool userInterface, const String& value)
{
    return command(this, commandName, userInterface).execute(value);
}

bool Document::queryCommandEnabled(const String& commandName)
{
    return command(this, commandName).isEnabled();
}

bool Document::queryCommandIndeterm(const String& commandName)
{
    return command(this, commandName).state() == MixedTriState;
}

bool Document::queryCommandState(const String& commandName)
{
    return command(this, commandName).state() != FalseTriState;
}

bool Document::queryCommandSupported(const String& commandName)
{
    return command(this, commandName).isSupported();
}

String Document::queryCommandValue(const String& commandName)
{
    return command(this, commandName).value();
}

static IntRect placeholderRectForMarker()
{
    return IntRect(-1, -1, -1, -1);
}

void Document::addMarker(Range *range, DocumentMarker::MarkerType type, String description)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        int exception = 0;
        DocumentMarker marker = {type, textPiece->startOffset(exception), textPiece->endOffset(exception), description, false};
        addMarker(textPiece->startContainer(exception), marker);
    }
}

void Document::removeMarkers(Range* range, DocumentMarker::MarkerType markerType)
{
    if (m_markers.isEmpty())
        return;

    ExceptionCode ec = 0;
    Node* startContainer = range->startContainer(ec);
    Node* endContainer = range->endContainer(ec);

    Node* pastLastNode = range->pastLastNode();
    for (Node* node = range->firstNode(); node != pastLastNode; node = node->traverseNextNode()) {
        int startOffset = node == startContainer ? range->startOffset(ec) : 0;
        int endOffset = node == endContainer ? range->endOffset(ec) : INT_MAX;
        int length = endOffset - startOffset;
        removeMarkers(node, startOffset, length, markerType);
    }
}

// Markers are stored in order sorted by their start offset.
// Markers of the same type do not overlap each other.

void Document::addMarker(Node* node, DocumentMarker newMarker) 
{
    ASSERT(newMarker.endOffset >= newMarker.startOffset);
    if (newMarker.endOffset == newMarker.startOffset)
        return;

    MarkerMapVectorPair* vectorPair = m_markers.get(node);

    if (!vectorPair) {
        vectorPair = new MarkerMapVectorPair;
        vectorPair->first.append(newMarker);
        vectorPair->second.append(placeholderRectForMarker());
        m_markers.set(node, vectorPair);
    } else {
        Vector<DocumentMarker>& markers = vectorPair->first;
        Vector<IntRect>& rects = vectorPair->second;
        size_t numMarkers = markers.size();
        ASSERT(numMarkers == rects.size());
        size_t i;
        // Iterate over all markers whose start offset is less than or equal to the new marker's.
        // If one of them is of the same type as the new marker and touches it or intersects with it
        // (there is at most one), remove it and adjust the new marker's start offset to encompass it.
        for (i = 0; i < numMarkers; ++i) {
            DocumentMarker marker = markers[i];
            if (marker.startOffset > newMarker.startOffset)
                break;
            if (marker.type == newMarker.type && marker.endOffset >= newMarker.startOffset) {
                newMarker.startOffset = marker.startOffset;
                markers.remove(i);
                rects.remove(i);
                numMarkers--;
                break;
            }
        }
        size_t j = i;
        // Iterate over all markers whose end offset is less than or equal to the new marker's,
        // removing markers of the same type as the new marker which touch it or intersect with it,
        // adjusting the new marker's end offset to cover them if necessary.
        while (j < numMarkers) {
            DocumentMarker marker = markers[j];
            if (marker.startOffset > newMarker.endOffset)
                break;
            if (marker.type == newMarker.type) {
                markers.remove(j);
                rects.remove(j);
                if (newMarker.endOffset <= marker.endOffset) {
                    newMarker.endOffset = marker.endOffset;
                    break;
                }
                numMarkers--;
            } else
                j++;
        }
        // At this point i points to the node before which we want to insert.
        markers.insert(i, newMarker);
        rects.insert(i, placeholderRectForMarker());
    }
    
    // repaint the affected node
    if (node->renderer())
        node->renderer()->repaint();
}

// copies markers from srcNode to dstNode, applying the specified shift delta to the copies.  The shift is
// useful if, e.g., the caller has created the dstNode from a non-prefix substring of the srcNode.
void Document::copyMarkers(Node *srcNode, unsigned startOffset, int length, Node *dstNode, int delta, DocumentMarker::MarkerType markerType)
{
    if (length <= 0)
        return;
    
    MarkerMapVectorPair* vectorPair = m_markers.get(srcNode);
    if (!vectorPair)
        return;

    ASSERT(vectorPair->first.size() == vectorPair->second.size());

    bool docDirty = false;
    unsigned endOffset = startOffset + length - 1;
    Vector<DocumentMarker>& markers = vectorPair->first;
    for (size_t i = 0; i != markers.size(); ++i) {
        DocumentMarker marker = markers[i];

        // stop if we are now past the specified range
        if (marker.startOffset > endOffset)
            break;
        
        // skip marker that is before the specified range or is the wrong type
        if (marker.endOffset < startOffset || (marker.type != markerType && markerType != DocumentMarker::AllMarkers))
            continue;

        // pin the marker to the specified range and apply the shift delta
        docDirty = true;
        if (marker.startOffset < startOffset)
            marker.startOffset = startOffset;
        if (marker.endOffset > endOffset)
            marker.endOffset = endOffset;
        marker.startOffset += delta;
        marker.endOffset += delta;
        
        addMarker(dstNode, marker);
    }
    
    // repaint the affected node
    if (docDirty && dstNode->renderer())
        dstNode->renderer()->repaint();
}

void Document::removeMarkers(Node* node, unsigned startOffset, int length, DocumentMarker::MarkerType markerType)
{
    if (length <= 0)
        return;

    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (!vectorPair)
        return;

    Vector<DocumentMarker>& markers = vectorPair->first;
    Vector<IntRect>& rects = vectorPair->second;
    ASSERT(markers.size() == rects.size());
    bool docDirty = false;
    unsigned endOffset = startOffset + length;
    for (size_t i = 0; i < markers.size();) {
        DocumentMarker marker = markers[i];

        // markers are returned in order, so stop if we are now past the specified range
        if (marker.startOffset >= endOffset)
            break;
        
        // skip marker that is wrong type or before target
        if (marker.endOffset < startOffset || (marker.type != markerType && markerType != DocumentMarker::AllMarkers)) {
            i++;
            continue;
        }

        // at this point we know that marker and target intersect in some way
        docDirty = true;

        // pitch the old marker and any associated rect
        markers.remove(i);
        rects.remove(i);
        
        // add either of the resulting slices that are left after removing target
        if (startOffset > marker.startOffset) {
            DocumentMarker newLeft = marker;
            newLeft.endOffset = startOffset;
            markers.insert(i, newLeft);
            rects.insert(i, placeholderRectForMarker());
            // i now points to the newly-inserted node, but we want to skip that one
            i++;
        }
        if (marker.endOffset > endOffset) {
            DocumentMarker newRight = marker;
            newRight.startOffset = endOffset;
            markers.insert(i, newRight);
            rects.insert(i, placeholderRectForMarker());
            // i now points to the newly-inserted node, but we want to skip that one
            i++;
        }
    }

    if (markers.isEmpty()) {
        ASSERT(rects.isEmpty());
        m_markers.remove(node);
        delete vectorPair;
    }

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

DocumentMarker* Document::markerContainingPoint(const IntPoint& point, DocumentMarker::MarkerType markerType)
{
    // outer loop: process each node that contains any markers
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator nodeIterator = m_markers.begin(); nodeIterator != end; ++nodeIterator) {
        // inner loop; process each marker in this node
        MarkerMapVectorPair* vectorPair = nodeIterator->second;
        Vector<DocumentMarker>& markers = vectorPair->first;
        Vector<IntRect>& rects = vectorPair->second;
        ASSERT(markers.size() == rects.size());
        unsigned markerCount = markers.size();
        for (unsigned markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
            DocumentMarker& marker = markers[markerIndex];
            
            // skip marker that is wrong type
            if (marker.type != markerType && markerType != DocumentMarker::AllMarkers)
                continue;
            
            IntRect& r = rects[markerIndex];
            
            // skip placeholder rects
            if (r == placeholderRectForMarker())
                continue;
            
            if (r.contains(point))
                return &marker;
        }
    }
    
    return 0;
}

Vector<DocumentMarker> Document::markersForNode(Node* node)
{
    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (vectorPair)
        return vectorPair->first;
    return Vector<DocumentMarker>();
}

Vector<IntRect> Document::renderedRectsForMarkers(DocumentMarker::MarkerType markerType)
{
    Vector<IntRect> result;
    
    // outer loop: process each node
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator nodeIterator = m_markers.begin(); nodeIterator != end; ++nodeIterator) {
        // inner loop; process each marker in this node
        MarkerMapVectorPair* vectorPair = nodeIterator->second;
        Vector<DocumentMarker>& markers = vectorPair->first;
        Vector<IntRect>& rects = vectorPair->second;
        ASSERT(markers.size() == rects.size());
        unsigned markerCount = markers.size();
        for (unsigned markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
            DocumentMarker marker = markers[markerIndex];
            
            // skip marker that is wrong type
            if (marker.type != markerType && markerType != DocumentMarker::AllMarkers)
                continue;
            
            IntRect r = rects[markerIndex];
            // skip placeholder rects
            if (r == placeholderRectForMarker())
                continue;

            result.append(r);
        }
    }
    
    return result;
}

void Document::removeMarkers(Node* node)
{
    MarkerMap::iterator i = m_markers.find(node);
    if (i != m_markers.end()) {
        delete i->second;
        m_markers.remove(i);
        if (RenderObject* renderer = node->renderer())
            renderer->repaint();
    }
}

void Document::removeMarkers(DocumentMarker::MarkerType markerType)
{
    // outer loop: process each markered node in the document
    MarkerMap markerMapCopy = m_markers;
    MarkerMap::iterator end = markerMapCopy.end();
    for (MarkerMap::iterator i = markerMapCopy.begin(); i != end; ++i) {
        Node* node = i->first.get();
        bool nodeNeedsRepaint = false;

        // inner loop: process each marker in the current node
        MarkerMapVectorPair* vectorPair = i->second;
        Vector<DocumentMarker>& markers = vectorPair->first;
        Vector<IntRect>& rects = vectorPair->second;
        ASSERT(markers.size() == rects.size());
        for (size_t i = 0; i != markers.size();) {
            DocumentMarker marker = markers[i];

            // skip nodes that are not of the specified type
            if (marker.type != markerType && markerType != DocumentMarker::AllMarkers) {
                ++i;
                continue;
            }

            // pitch the old marker
            markers.remove(i);
            rects.remove(i);
            nodeNeedsRepaint = true;
            // markerIterator now points to the next node
        }

        // Redraw the node if it changed. Do this before the node is removed from m_markers, since 
        // m_markers might contain the last reference to the node.
        if (nodeNeedsRepaint) {
            RenderObject* renderer = node->renderer();
            if (renderer)
                renderer->repaint();
        }

        // delete the node's list if it is now empty
        if (markers.isEmpty()) {
            ASSERT(rects.isEmpty());
            m_markers.remove(node);
            delete vectorPair;
        }
    }
}

void Document::repaintMarkers(DocumentMarker::MarkerType markerType)
{
    // outer loop: process each markered node in the document
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator i = m_markers.begin(); i != end; ++i) {
        Node* node = i->first.get();
        
        // inner loop: process each marker in the current node
        MarkerMapVectorPair* vectorPair = i->second;
        Vector<DocumentMarker>& markers = vectorPair->first;
        bool nodeNeedsRepaint = false;
        for (size_t i = 0; i != markers.size(); ++i) {
            DocumentMarker marker = markers[i];
            
            // skip nodes that are not of the specified type
            if (marker.type == markerType || markerType == DocumentMarker::AllMarkers) {
                nodeNeedsRepaint = true;
                break;
            }
        }
        
        if (!nodeNeedsRepaint)
            continue;
        
        // cause the node to be redrawn
        if (RenderObject* renderer = node->renderer())
            renderer->repaint();
    }
}

void Document::setRenderedRectForMarker(Node* node, DocumentMarker marker, const IntRect& r)
{
    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (!vectorPair) {
        ASSERT_NOT_REACHED(); // shouldn't be trying to set the rect for a marker we don't already know about
        return;
    }
    
    Vector<DocumentMarker>& markers = vectorPair->first;
    ASSERT(markers.size() == vectorPair->second.size());
    unsigned markerCount = markers.size();
    for (unsigned markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
        DocumentMarker m = markers[markerIndex];
        if (m == marker) {
            vectorPair->second[markerIndex] = r;
            return;
        }
    }
    
    ASSERT_NOT_REACHED(); // shouldn't be trying to set the rect for a marker we don't already know about
}

void Document::invalidateRenderedRectsForMarkersInRect(const IntRect& r)
{
    // outer loop: process each markered node in the document
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator i = m_markers.begin(); i != end; ++i) {
        
        // inner loop: process each rect in the current node
        MarkerMapVectorPair* vectorPair = i->second;
        Vector<IntRect>& rects = vectorPair->second;
        
        unsigned rectCount = rects.size();
        for (unsigned rectIndex = 0; rectIndex < rectCount; ++rectIndex)
            if (rects[rectIndex].intersects(r))
                rects[rectIndex] = placeholderRectForMarker();
    }
}

void Document::shiftMarkers(Node *node, unsigned startOffset, int delta, DocumentMarker::MarkerType markerType)
{
    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (!vectorPair)
        return;
    
    Vector<DocumentMarker>& markers = vectorPair->first;
    Vector<IntRect>& rects = vectorPair->second;
    ASSERT(markers.size() == rects.size());
    
    bool docDirty = false;
    for (size_t i = 0; i != markers.size(); ++i) {
        DocumentMarker &marker = markers[i];
        if (marker.startOffset >= startOffset && (markerType == DocumentMarker::AllMarkers || marker.type == markerType)) {
            ASSERT((int)marker.startOffset + delta >= 0);
            marker.startOffset += delta;
            marker.endOffset += delta;
            docDirty = true;
            
            // Marker moved, so previously-computed rendered rectangle is now invalid
            rects[i] = placeholderRectForMarker();
        }
    }
    
    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

void Document::setMarkersActive(Range* range, bool active)
{
    if (m_markers.isEmpty())
        return;

    ExceptionCode ec = 0;
    Node* startContainer = range->startContainer(ec);
    Node* endContainer = range->endContainer(ec);

    Node* pastLastNode = range->pastLastNode();
    for (Node* node = range->firstNode(); node != pastLastNode; node = node->traverseNextNode()) {
        int startOffset = node == startContainer ? range->startOffset(ec) : 0;
        int endOffset = node == endContainer ? range->endOffset(ec) : INT_MAX;
        setMarkersActive(node, startOffset, endOffset, active);
    }
}

void Document::setMarkersActive(Node* node, unsigned startOffset, unsigned endOffset, bool active)
{
    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (!vectorPair)
        return;

    Vector<DocumentMarker>& markers = vectorPair->first;
    ASSERT(markers.size() == vectorPair->second.size());

    bool docDirty = false;
    for (size_t i = 0; i != markers.size(); ++i) {
        DocumentMarker &marker = markers[i];

        // Markers are returned in order, so stop if we are now past the specified range.
        if (marker.startOffset >= endOffset)
            break;

        // Skip marker that is wrong type or before target.
        if (marker.endOffset < startOffset || marker.type != DocumentMarker::TextMatch)
            continue;

        marker.activeMatch = active;
        docDirty = true;
    }

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

#if ENABLE(XSLT)

void Document::applyXSLTransform(ProcessingInstruction* pi)
{
    RefPtr<XSLTProcessor> processor = XSLTProcessor::create();
    processor->setXSLStyleSheet(static_cast<XSLStyleSheet*>(pi->sheet()));
    String resultMIMEType;
    String newSource;
    String resultEncoding;
    if (!processor->transformToString(this, resultMIMEType, newSource, resultEncoding))
        return;
    // FIXME: If the transform failed we should probably report an error (like Mozilla does).
    processor->createDocumentFromSource(newSource, resultEncoding, resultMIMEType, this, frame());
}

void Document::setTransformSource(void* doc)
{
    if (doc == m_transformSource)
        return;

    xmlFreeDoc((xmlDocPtr)m_transformSource);
    m_transformSource = doc;
}

#endif

void Document::setDesignMode(InheritedBool value)
{
    m_designMode = value;
}

Document::InheritedBool Document::getDesignMode() const
{
    return m_designMode;
}

bool Document::inDesignMode() const
{
    for (const Document* d = this; d; d = d->parentDocument()) {
        if (d->m_designMode != inherit)
            return d->m_designMode;
    }
    return false;
}

Document *Document::parentDocument() const
{
    Frame *childPart = frame();
    if (!childPart)
        return 0;
    Frame *parent = childPart->tree()->parent();
    if (!parent)
        return 0;
    return parent->document();
}

Document *Document::topDocument() const
{
    Document *doc = const_cast<Document *>(this);
    Element *element;
    while ((element = doc->ownerElement()))
        doc = element->document();
    
    return doc;
}

PassRefPtr<Attr> Document::createAttribute(const String& name, ExceptionCode& ec)
{
    return createAttributeNS(String(), name, ec, true);
}

PassRefPtr<Attr> Document::createAttributeNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode& ec, bool shouldIgnoreNamespaceChecks)
{
    String prefix, localName;
    if (!parseQualifiedName(qualifiedName, prefix, localName, ec))
        return 0;

    QualifiedName qName(prefix, localName, namespaceURI);
    if (!shouldIgnoreNamespaceChecks && hasPrefixNamespaceMismatch(qName)) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    // Spec: DOM Level 2 Core: http://www.w3.org/TR/DOM-Level-2-Core/core.html#ID-DocCrAttrNS
    if (!shouldIgnoreNamespaceChecks && qName.localName() == "xmlns" && qName.namespaceURI() != "http://www.w3.org/2000/xmlns/") {
        ec = NAMESPACE_ERR;
        return 0;
    }

    // FIXME: Assume this is a mapped attribute, since createAttribute isn't namespace-aware.  There's no harm to XML
    // documents if we're wrong.
    return Attr::create(0, this, MappedAttribute::create(qName, StringImpl::empty()));
}

#if ENABLE(SVG)
const SVGDocumentExtensions* Document::svgExtensions()
{
    return m_svgExtensions.get();
}

SVGDocumentExtensions* Document::accessSVGExtensions()
{
    if (!m_svgExtensions)
        m_svgExtensions.set(new SVGDocumentExtensions(this));
    return m_svgExtensions.get();
}
#endif

PassRefPtr<HTMLCollection> Document::images()
{
    return HTMLCollection::create(this, DocImages);
}

PassRefPtr<HTMLCollection> Document::applets()
{
    return HTMLCollection::create(this, DocApplets);
}

PassRefPtr<HTMLCollection> Document::embeds()
{
    return HTMLCollection::create(this, DocEmbeds);
}

PassRefPtr<HTMLCollection> Document::plugins()
{
    // This is an alias for embeds() required for the JS DOM bindings.
    return HTMLCollection::create(this, DocEmbeds);
}

PassRefPtr<HTMLCollection> Document::objects()
{
    return HTMLCollection::create(this, DocObjects);
}

PassRefPtr<HTMLCollection> Document::scripts()
{
    return HTMLCollection::create(this, DocScripts);
}

PassRefPtr<HTMLCollection> Document::links()
{
    return HTMLCollection::create(this, DocLinks);
}

PassRefPtr<HTMLCollection> Document::forms()
{
    return HTMLCollection::create(this, DocForms);
}

PassRefPtr<HTMLCollection> Document::anchors()
{
    return HTMLCollection::create(this, DocAnchors);
}

PassRefPtr<HTMLCollection> Document::all()
{
    return HTMLCollection::create(this, DocAll);
}

PassRefPtr<HTMLCollection> Document::windowNamedItems(const String &name)
{
    return HTMLNameCollection::create(this, WindowNamedItems, name);
}

PassRefPtr<HTMLCollection> Document::documentNamedItems(const String &name)
{
    return HTMLNameCollection::create(this, DocumentNamedItems, name);
}

CollectionCache* Document::nameCollectionInfo(CollectionType type, const AtomicString& name)
{
    ASSERT(type >= FirstNamedDocumentCachedType);
    unsigned index = type - FirstNamedDocumentCachedType;
    ASSERT(index < NumNamedDocumentCachedTypes);

    NamedCollectionMap& map = m_nameCollectionInfo[index];
    NamedCollectionMap::iterator iter = map.find(name.impl());
    if (iter == map.end())
        iter = map.add(name.impl(), new CollectionCache).first;
    return iter->second;
}

void Document::finishedParsing()
{
    setParsing(false);
    dispatchEvent(Event::create(eventNames().DOMContentLoadedEvent, true, false));
    if (Frame* f = frame())
        f->loader()->finishedParsing();
}

Vector<String> Document::formElementsState() const
{
    Vector<String> stateVector;
    stateVector.reserveInitialCapacity(m_formElementsWithState.size() * 3);
    typedef ListHashSet<Element*>::const_iterator Iterator;
    Iterator end = m_formElementsWithState.end();
    for (Iterator it = m_formElementsWithState.begin(); it != end; ++it) {
        Element* e = *it;
        String value;
        if (e->saveFormControlState(value)) {
            stateVector.append(e->formControlName().string());
            stateVector.append(e->formControlType().string());
            stateVector.append(value);
        }
    }
    return stateVector;
}

#if ENABLE(XPATH)

PassRefPtr<XPathExpression> Document::createExpression(const String& expression,
                                                       XPathNSResolver* resolver,
                                                       ExceptionCode& ec)
{
    if (!m_xpathEvaluator)
        m_xpathEvaluator = XPathEvaluator::create();
    return m_xpathEvaluator->createExpression(expression, resolver, ec);
}

PassRefPtr<XPathNSResolver> Document::createNSResolver(Node* nodeResolver)
{
    if (!m_xpathEvaluator)
        m_xpathEvaluator = XPathEvaluator::create();
    return m_xpathEvaluator->createNSResolver(nodeResolver);
}

PassRefPtr<XPathResult> Document::evaluate(const String& expression,
                                           Node* contextNode,
                                           XPathNSResolver* resolver,
                                           unsigned short type,
                                           XPathResult* result,
                                           ExceptionCode& ec)
{
    if (!m_xpathEvaluator)
        m_xpathEvaluator = XPathEvaluator::create();
    return m_xpathEvaluator->evaluate(expression, contextNode, resolver, type, result, ec);
}

#endif // ENABLE(XPATH)

void Document::setStateForNewFormElements(const Vector<String>& stateVector)
{
    // Walk the state vector backwards so that the value to use for each
    // name/type pair first is the one at the end of each individual vector
    // in the FormElementStateMap. We're using them like stacks.
    typedef FormElementStateMap::iterator Iterator;
    m_formElementsWithState.clear();
    for (size_t i = stateVector.size() / 3 * 3; i; i -= 3) {
        AtomicString a = stateVector[i - 3];
        AtomicString b = stateVector[i - 2];
        const String& c = stateVector[i - 1];
        FormElementKey key(a.impl(), b.impl());
        Iterator it = m_stateForNewFormElements.find(key);
        if (it != m_stateForNewFormElements.end())
            it->second.append(c);
        else {
            Vector<String> v(1);
            v[0] = c;
            m_stateForNewFormElements.set(key, v);
        }
    }
}

bool Document::hasStateForNewFormElements() const
{
    return !m_stateForNewFormElements.isEmpty();
}

bool Document::takeStateForFormElement(AtomicStringImpl* name, AtomicStringImpl* type, String& state)
{
    typedef FormElementStateMap::iterator Iterator;
    Iterator it = m_stateForNewFormElements.find(FormElementKey(name, type));
    if (it == m_stateForNewFormElements.end())
        return false;
    ASSERT(it->second.size());
    state = it->second.last();
    if (it->second.size() > 1)
        it->second.removeLast();
    else
        m_stateForNewFormElements.remove(it);
    return true;
}

FormElementKey::FormElementKey(AtomicStringImpl* name, AtomicStringImpl* type)
    : m_name(name), m_type(type)
{
    ref();
}

FormElementKey::~FormElementKey()
{
    deref();
}

FormElementKey::FormElementKey(const FormElementKey& other)
    : m_name(other.name()), m_type(other.type())
{
    ref();
}

FormElementKey& FormElementKey::operator=(const FormElementKey& other)
{
    other.ref();
    deref();
    m_name = other.name();
    m_type = other.type();
    return *this;
}

void FormElementKey::ref() const
{
    if (name())
        name()->ref();
    if (type())
        type()->ref();
}

void FormElementKey::deref() const
{
    if (name())
        name()->deref();
    if (type())
        type()->deref();
}

unsigned FormElementKeyHash::hash(const FormElementKey& k)
{
    ASSERT(sizeof(k) % (sizeof(uint16_t) * 2) == 0);

    unsigned l = sizeof(k) / (sizeof(uint16_t) * 2);
    const uint16_t* s = reinterpret_cast<const uint16_t*>(&k);
    uint32_t hash = WTF::stringHashingStartValue;

    // Main loop
    for (; l > 0; l--) {
        hash += s[0];
        uint32_t tmp = (s[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        s += 2;
        hash += hash >> 11;
    }
        
    // Force "avalanching" of final 127 bits
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    // this avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet", using a value that is likely to be
    // effectively the same as 0 when the low bits are masked
    if (hash == 0)
        hash = 0x80000000;

    return hash;
}

void Document::setIconURL(const String& iconURL, const String& type)
{
    // FIXME - <rdar://problem/4727645> - At some point in the future, we might actually honor the "type" 
    if (m_iconURL.isEmpty())
        m_iconURL = iconURL;
    else if (!type.isEmpty())
        m_iconURL = iconURL;
}

void Document::setUseSecureKeyboardEntryWhenActive(bool usesSecureKeyboard)
{
    if (m_useSecureKeyboardEntryWhenActive == usesSecureKeyboard)
        return;
        
    m_useSecureKeyboardEntryWhenActive = usesSecureKeyboard;
    m_frame->updateSecureKeyboardEntryIfActive();
}

bool Document::useSecureKeyboardEntryWhenActive() const
{
    return m_useSecureKeyboardEntryWhenActive;
}

void Document::initSecurityContext()
{
    if (securityOrigin() && !securityOrigin()->isEmpty())
        return;  // m_securityOrigin has already been initialized.

    if (!m_frame) {
        // No source for a security context.
        // This can occur via document.implementation.createDocument().
        m_cookieURL = KURL(ParsedURLString, "");
        ScriptExecutionContext::setSecurityOrigin(SecurityOrigin::createEmpty());
        return;
    }

    // In the common case, create the security context from the currently
    // loading URL.
    const KURL& url = m_frame->loader()->url();
    m_cookieURL = url;
    ScriptExecutionContext::setSecurityOrigin(SecurityOrigin::create(url));

    if (FrameLoader::allowSubstituteDataAccessToLocal()) {
        // If this document was loaded with substituteData, then the document can
        // load local resources.  See https://bugs.webkit.org/show_bug.cgi?id=16756
        // and https://bugs.webkit.org/show_bug.cgi?id=19760 for further
        // discussion.
        DocumentLoader* documentLoader = m_frame->loader()->documentLoader();
        if (documentLoader && documentLoader->substituteData().isValid())
            securityOrigin()->grantLoadLocalResources();
    }

    if (Settings* settings = this->settings()) {
        if (!settings->isWebSecurityEnabled()) {
          // Web security is turned off.  We should let this document access every
          // other document.  This is used primary by testing harnesses for web
          // sites.
          securityOrigin()->grantUniversalAccess();

        } else if (settings->allowUniversalAccessFromFileURLs() && securityOrigin()->isLocal()) {
          // Some clients want file:// URLs to have universal access, but that
          // setting is dangerous for other clients.
          securityOrigin()->grantUniversalAccess();
        }
    }

    if (!securityOrigin()->isEmpty())
        return;

    // If we do not obtain a meaningful origin from the URL, then we try to
    // find one via the frame hierarchy.

    Frame* ownerFrame = m_frame->tree()->parent();
    if (!ownerFrame)
        ownerFrame = m_frame->loader()->opener();

    if (ownerFrame) {
        m_cookieURL = ownerFrame->document()->cookieURL();
        // We alias the SecurityOrigins to match Firefox, see Bug 15313
        // https://bugs.webkit.org/show_bug.cgi?id=15313
        ScriptExecutionContext::setSecurityOrigin(ownerFrame->document()->securityOrigin());
    }
}

void Document::setSecurityOrigin(SecurityOrigin* securityOrigin)
{
    ScriptExecutionContext::setSecurityOrigin(securityOrigin);
    // FIXME: Find a better place to enable DNS prefetch, which is a loader concept,
    // not applicable to arbitrary documents.
    initDNSPrefetch();
}

void Document::updateFocusAppearanceSoon()
{
    if (!m_updateFocusAppearanceTimer.isActive())
        m_updateFocusAppearanceTimer.startOneShot(0);
}

void Document::cancelFocusAppearanceUpdate()
{
    m_updateFocusAppearanceTimer.stop();
}

void Document::updateFocusAppearanceTimerFired(Timer<Document>*)
{
    Node* node = focusedNode();
    if (!node)
        return;
    if (!node->isElementNode())
        return;

    updateLayout();

    Element* element = static_cast<Element*>(node);
    if (element->isFocusable())
        element->updateFocusAppearance(false);
}

void Document::executeScriptSoonTimerFired(Timer<Document>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_executeScriptSoonTimer);

    Vector<pair<ScriptElementData*, CachedResourceHandle<CachedScript> > > scripts;
    scripts.swap(m_scriptsToExecuteSoon);
    size_t size = scripts.size();
    for (size_t i = 0; i < size; ++i) {
        scripts[i].first->execute(scripts[i].second.get());
        scripts[i].first->element()->deref(); // Balances ref() in executeScriptSoon().
    }
}

void Document::executeScriptSoon(ScriptElementData* data, CachedResourceHandle<CachedScript> cachedScript)
{
    ASSERT_ARG(data, data);

    Element* element = data->element();
    ASSERT(element);
    ASSERT(element->document() == this);
    ASSERT(element->inDocument());

    m_scriptsToExecuteSoon.append(make_pair(data, cachedScript));
    element->ref(); // Balanced by deref()s in executeScriptSoonTimerFired() and ~Document().
    if (!m_executeScriptSoonTimer.isActive())
        m_executeScriptSoonTimer.startOneShot(0);
}

// FF method for accessing the selection added for compatability.
DOMSelection* Document::getSelection() const
{
    return frame() ? frame()->domWindow()->getSelection() : 0;
}

#if ENABLE(DATABASE)

void Document::addOpenDatabase(Database* database)
{
    if (!m_openDatabaseSet)
        m_openDatabaseSet.set(new DatabaseSet);

    ASSERT(!m_openDatabaseSet->contains(database));
    m_openDatabaseSet->add(database);
}

void Document::removeOpenDatabase(Database* database)
{
    ASSERT(m_openDatabaseSet && m_openDatabaseSet->contains(database));
    if (!m_openDatabaseSet)
        return;
        
    m_openDatabaseSet->remove(database);
}

DatabaseThread* Document::databaseThread()
{
    if (!m_databaseThread && !m_hasOpenDatabases) {
        // Create the database thread on first request - but not if at least one database was already opened,
        // because in that case we already had a database thread and terminated it and should not create another.
        m_databaseThread = DatabaseThread::create();
        if (!m_databaseThread->start())
            m_databaseThread = 0;
    }

    return m_databaseThread.get();
}

void Document::stopDatabases()
{
    if (m_openDatabaseSet) {
        DatabaseSet::iterator i = m_openDatabaseSet->begin();
        DatabaseSet::iterator end = m_openDatabaseSet->end();
        for (; i != end; ++i) {
            (*i)->stop();
            if (m_databaseThread)
                m_databaseThread->unscheduleDatabaseTasks(*i);
        }
    }
    
    if (m_databaseThread)
        m_databaseThread->requestTermination();
}

#endif

#if ENABLE(WML)
void Document::resetWMLPageState()
{
    if (WMLPageState* pageState = wmlPageStateForDocument(this))
        pageState->reset();
}

void Document::initializeWMLPageState()
{
    if (!isWMLDocument())
        return;

    static_cast<WMLDocument*>(this)->initialize();
}
#endif

void Document::attachRange(Range* range)
{
    ASSERT(!m_ranges.contains(range));
    m_ranges.add(range);
}

void Document::detachRange(Range* range)
{
    // We don't ASSERT m_ranges.contains(range) to allow us to call this
    // unconditionally to fix: https://bugs.webkit.org/show_bug.cgi?id=26044
    m_ranges.remove(range);
}

CanvasRenderingContext* Document::getCSSCanvasContext(const String& type, const String& name, int width, int height)
{
    HTMLCanvasElement* result = getCSSCanvasElement(name);
    if (!result)
        return 0;
    result->setSize(IntSize(width, height));
    return result->getContext(type);
}

HTMLCanvasElement* Document::getCSSCanvasElement(const String& name)
{
    RefPtr<HTMLCanvasElement> result = m_cssCanvasElements.get(name).get();
    if (!result) {
        result = new HTMLCanvasElement(canvasTag, this);
        m_cssCanvasElements.set(name, result);
    }
    return result.get();
}

void Document::initDNSPrefetch()
{
    m_haveExplicitlyDisabledDNSPrefetch = false;
    m_isDNSPrefetchEnabled = securityOrigin()->protocol() == "http";

    // Inherit DNS prefetch opt-out from parent frame    
    if (Document* parent = parentDocument()) {
        if (!parent->isDNSPrefetchEnabled())
            m_isDNSPrefetchEnabled = false;
    }
}

void Document::parseDNSPrefetchControlHeader(const String& dnsPrefetchControl)
{
    if (equalIgnoringCase(dnsPrefetchControl, "on") && !m_haveExplicitlyDisabledDNSPrefetch) {
        m_isDNSPrefetchEnabled = true;
        return;
    }

    m_isDNSPrefetchEnabled = false;
    m_haveExplicitlyDisabledDNSPrefetch = true;
}

void Document::reportException(const String& errorMessage, int lineNumber, const String& sourceURL)
{
    if (DOMWindow* window = domWindow())
        window->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, errorMessage, lineNumber, sourceURL);
}

void Document::addMessage(MessageDestination destination, MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    switch (destination) {
#if ENABLE(INSPECTOR)
    case InspectorControllerDestination:
        if (page())
            page()->inspectorController()->addMessageToConsole(source, type, level, message, lineNumber, sourceURL);
        return;
#endif
    case ConsoleDestination:
        if (DOMWindow* window = domWindow())
            window->console()->addMessage(source, type, level, message, lineNumber, sourceURL);
        return;
    }
    ASSERT_NOT_REACHED();
}

void Document::resourceRetrievedByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString)
{
#if ENABLE(INSPECTOR)
    if (page())
        page()->inspectorController()->resourceRetrievedByXMLHttpRequest(identifier, sourceString);
#endif
    Frame* frame = this->frame();
    if (frame) {
        FrameLoader* frameLoader = frame->loader();
        frameLoader->didLoadResourceByXMLHttpRequest(identifier, sourceString);
    }
}

void Document::scriptImported(unsigned long identifier, const String& sourceString)
{
#if ENABLE(INSPECTOR)
    if (page())
        page()->inspectorController()->scriptImported(identifier, sourceString);
#else
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(sourceString);
#endif
}

class ScriptExecutionContextTaskTimer : public TimerBase {
public:
    ScriptExecutionContextTaskTimer(PassRefPtr<Document> context, PassRefPtr<ScriptExecutionContext::Task> task)
        : m_context(context)
        , m_task(task)
    {
    }

private:
    virtual void fired()
    {
        m_task->performTask(m_context.get());
        delete this;
    }

    RefPtr<Document> m_context;
    RefPtr<ScriptExecutionContext::Task> m_task;
};

struct PerformTaskContext {
    PerformTaskContext(ScriptExecutionContext* scriptExecutionContext, PassRefPtr<ScriptExecutionContext::Task> task)
        : scriptExecutionContext(scriptExecutionContext)
        , task(task)
    {
    }

    ScriptExecutionContext* scriptExecutionContext; // The context should exist until task execution.
    RefPtr<ScriptExecutionContext::Task> task;
};

static void performTask(void* ctx)
{
    PerformTaskContext* ptctx = reinterpret_cast<PerformTaskContext*>(ctx);
    ptctx->task->performTask(ptctx->scriptExecutionContext);
    delete ptctx;
}

void Document::postTask(PassRefPtr<Task> task)
{
    if (isMainThread()) {
        ScriptExecutionContextTaskTimer* timer = new ScriptExecutionContextTaskTimer(static_cast<Document*>(this), task);
        timer->startOneShot(0);
    } else {
        callOnMainThread(performTask, new PerformTaskContext(this, task));
    }
}

Element* Document::findAnchor(const String& name)
{
    if (name.isEmpty())
        return 0;
    if (Element* element = getElementById(name))
        return element;
    for (Node* node = this; node; node = node->traverseNextNode()) {
        if (node->hasTagName(aTag)) {
            HTMLAnchorElement* anchor = static_cast<HTMLAnchorElement*>(node);
            if (inCompatMode()) {
                // Quirks mode, case insensitive comparison of names.
                if (equalIgnoringCase(anchor->name(), name))
                    return anchor;
            } else {
                // Strict mode, names need to match exactly.
                if (anchor->name() == name)
                    return anchor;
            }
        }
    }
    return 0;
}

String Document::displayStringModifiedByEncoding(const String& str) const
{
    if (m_decoder)
        return m_decoder->encoding().displayString(str.impl());
    return str;
}

PassRefPtr<StringImpl> Document::displayStringModifiedByEncoding(PassRefPtr<StringImpl> str) const
{
    if (m_decoder)
        return m_decoder->encoding().displayString(str);
    return str;
}

void Document::displayBufferModifiedByEncoding(UChar* buffer, unsigned len) const
{
    if (m_decoder)
        m_decoder->encoding().displayBuffer(buffer, len);
}

#if ENABLE(XHTMLMP)
bool Document::isXHTMLMPDocument() const
{
    if (!frame() || !frame()->loader())
        return false;
    // As per section 7.2 of OMA-WAP-XHTMLMP-V1_1-20061020-A.pdf, a conforming user agent
    // MUST accept XHTMLMP document identified as "application/vnd.wap.xhtml+xml"
    // and SHOULD accept it identified as "application/xhtml+xml"
    return frame()->loader()->responseMIMEType() == "application/vnd.wap.xhtml+xml" || frame()->loader()->responseMIMEType() == "application/xhtml+xml";
}
#endif

} // namespace WebCore
