/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "Document.h"

#include "AccessibilityObjectCache.h"
#include "CDATASection.h"
#include "CSSValueKeywords.h"
#include "Comment.h"
#include "DOMImplementation.h"
#include "DocLoader.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "EditingText.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameTree.h"
#include "HTMLDocument.h"
#include "HTMLElementFactory.h"
#include "HTMLInputElement.h"
#include "HTMLNameCollection.h"
#include "HTMLNames.h"
#include "JSEditor.h"
#include "Logging.h"
#include "MouseEventWithHitTestResults.h"
#include "NameNodeList.h"
#include "PlatformKeyboardEvent.h"
#include "RegularExpression.h"
#include "RenderArena.h"
#include "RenderCanvas.h"
#include "SegmentedString.h"
#include "SelectionController.h"
#include "StringHash.h"
#include "SystemTime.h"
#include "TextIterator.h"
#include "css_valueimpl.h"
#include "csshelper.h"
#include "cssstyleselector.h"
#include "dom2_eventsimpl.h"
#include "dom_xmlimpl.h"
#include "html_headimpl.h"
#include "html_imageimpl.h"
#include "kjs_binding.h"
#include "kjs_proxy.h"
#include "render_frames.h"
#include "xml_tokenizer.h"
#include "xmlhttprequest.h"

#ifdef KHTML_XSLT
#include "XSLTProcessor.h"
#endif

#ifndef KHTML_NO_XBL
#include "xbl_binding_manager.h"
using XBL::XBLBindingManager;
#endif

#if SVG_SUPPORT
#include "SVGDocumentExtensions.h"
#include "SVGElementFactory.h"
#include "SVGZoomEvent.h"
#include "SVGStyleElement.h"
#include "KSVGTimeScheduler.h"
#endif

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

// #define INSTRUMENT_LAYOUT_SCHEDULING 1

// This amount of time must have elapsed before we will even consider scheduling a layout without a delay.
// FIXME: For faster machines this value can really be lowered to 200.  250 is adequate, but a little high
// for dual G5s. :)
const int cLayoutScheduleThreshold = 250;

// Use 1 to represent the document's default form.
HTMLFormElement* const defaultForm = (HTMLFormElement*) 1;

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
static const unsigned PHI = 0x9e3779b9U;

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
    const uint32_t nameStartMask = U_GC_LL_MASK | U_GC_LU_MASK | U_GC_LO_MASK | U_GC_LT_MASK | U_GC_NL_MASK;
    if (!(U_GET_GC_MASK(c) & nameStartMask))
        return false;

    // rule (c) above
    if (c >= 0xF900 && c < 0xFFFE)
        return false;

    // rule (d) above
    UDecompositionType decompType = static_cast<UDecompositionType>(u_getIntPropertyValue(c, UCHAR_DECOMPOSITION_TYPE));
    if (decompType == U_DT_FONT || decompType == U_DT_COMPAT)
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
    const uint32_t otherNamePartMask = U_GC_MC_MASK | U_GC_ME_MASK | U_GC_MN_MASK | U_GC_LM_MASK | U_GC_ND_MASK;
    if (!(U_GET_GC_MASK(c) & otherNamePartMask))
        return false;

    // rule (c) above
    if (c >= 0xF900 && c < 0xFFFE)
        return false;

    // rule (d) above
    UDecompositionType decompType = static_cast<UDecompositionType>(u_getIntPropertyValue(c, UCHAR_DECOMPOSITION_TYPE));
    if (decompType == U_DT_FONT || decompType == U_DT_COMPAT)
        return false;

    return true;
}

DeprecatedPtrList<Document> * Document::changedDocuments = 0;

// FrameView might be 0
Document::Document(DOMImplementation* impl, FrameView *v)
    : ContainerNode(0)
    , m_implementation(impl)
    , m_domtree_version(0)
    , m_styleSheets(new StyleSheetList)
    , m_title("")
    , m_titleSetExplicitly(false)
    , m_imageLoadEventTimer(this, &Document::imageLoadEventTimerFired)
#if !KHTML_NO_XBL
    , m_bindingManager(new XBLBindingManager(this))
#endif
#ifdef KHTML_XSLT
    , m_transformSource(0)
#endif
    , m_savedRenderer(0)
    , m_passwordFields(0)
    , m_secureForms(0)
    , m_designMode(inherit)
    , m_selfOnlyRefCount(0)
#if SVG_SUPPORT
    , m_svgExtensions(0)
#endif
#if __APPLE__
    , m_hasDashboardRegions(false)
    , m_dashboardRegionsDirty(false)
#endif
    , m_accessKeyMapValid(false)
    , m_createRenderers(true)
    , m_inPageCache(false)
{
    m_document.resetSkippingRef(this);

    m_printing = false;

    m_view = v;
    m_renderArena = 0;

    m_accCache = 0;
    
    m_docLoader = new DocLoader(v ? v->frame() : 0, this);

    visuallyOrdered = false;
    m_loadingSheet = false;
    m_bParsing = false;
    m_docChanged = false;
    m_tokenizer = 0;

    pMode = Strict;
    hMode = XHtml;
    m_textColor = Color::black;
    m_listenerTypes = 0;
    m_inDocument = true;
    m_styleSelectorDirty = false;
    m_inStyleRecalc = false;
    m_closeAfterStyleRecalc = false;
    m_usesDescendantRules = false;
    m_usesSiblingRules = false;

    m_styleSelector = new CSSStyleSelector(this, m_usersheet, m_styleSheets.get(), !inCompatMode());
    m_windowEventListeners.setAutoDelete(true);
    m_pendingStylesheets = 0;
    m_ignorePendingStylesheets = false;

    m_cssTarget = 0;

    resetLinkColor();
    resetVisitedLinkColor();
    resetActiveLinkColor();

    m_processingLoadEvent = false;
    m_startTime = currentTime();
    m_overMinimumLayoutThreshold = false;
    
    m_jsEditor = 0;

    static int docID = 0;
    m_docID = docID++;
}

void Document::removedLastRef()
{
    if (m_selfOnlyRefCount) {
        // if removing a child removes the last self-only ref, we don't
        // want the document to be destructed until after
        // removeAllChildren returns, so we guard ourselves with an
        // extra self-only ref

        DocPtr<Document> guard(this);

        // we must make sure not to be retaining any of our children through
        // these extra pointers or we will create a reference cycle
        m_docType = 0;
        m_focusNode = 0;
        m_hoverNode = 0;
        m_activeNode = 0;
        m_titleElement = 0;

        removeAllChildren();
    } else
        delete this;
}

Document::~Document()
{
    assert(!renderer());
    assert(!m_inPageCache);
    assert(m_savedRenderer == 0);

#if SVG_SUPPORT
    delete m_svgExtensions;
#endif

    XMLHttpRequest::detachRequests(this);
    KJS::ScriptInterpreter::forgetAllDOMNodesForDocument(this);

    if (m_docChanged && changedDocuments)
        changedDocuments->remove(this);
    delete m_tokenizer;
    m_document.resetSkippingRef(0);
    delete m_styleSelector;
    delete m_docLoader;
    
    if (m_renderArena) {
        delete m_renderArena;
        m_renderArena = 0;
    }

#ifdef KHTML_XSLT
    xmlFreeDoc((xmlDocPtr)m_transformSource);
#endif

#ifndef KHTML_NO_XBL
    delete m_bindingManager;
#endif

    deleteAllValues(m_markers);

    if (m_accCache) {
        delete m_accCache;
        m_accCache = 0;
    }
    m_decoder = 0;
    
    if (m_jsEditor) {
        delete m_jsEditor;
        m_jsEditor = 0;
    }
    
    deleteAllValues(m_selectedRadioButtons);
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
    m_docType = docType;
}

DocumentType *Document::doctype() const
{
    return m_docType.get();
}

DOMImplementation* Document::implementation() const
{
    return m_implementation.get();
}

Element* Document::documentElement() const
{
    Node* n = firstChild();
    while (n && !n->isElementNode())
      n = n->nextSibling();
    return static_cast<Element*>(n);
}

PassRefPtr<Element> Document::createElement(const String &name, ExceptionCode& ec)
{
    return createElementNS(nullAtom, name, ec);
}

PassRefPtr<DocumentFragment> Document::createDocumentFragment()
{
    return new DocumentFragment(document());
}

PassRefPtr<Text> Document::createTextNode(const String &data)
{
    return new Text(this, data);
}

PassRefPtr<Comment> Document::createComment (const String &data)
{
    return new Comment(this, data);
}

PassRefPtr<CDATASection> Document::createCDATASection(const String &data, ExceptionCode& ec)
{
    if (isHTMLDocument()) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return new CDATASection(this, data);
}

PassRefPtr<ProcessingInstruction> Document::createProcessingInstruction(const String &target, const String &data, ExceptionCode& ec)
{
    if (!isValidName(target)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }
    if (isHTMLDocument()) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return new ProcessingInstruction(this, target, data);
}

PassRefPtr<EntityReference> Document::createEntityReference(const String &name, ExceptionCode& ec)
{
    if (!isValidName(name)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }
    if (isHTMLDocument()) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return new EntityReference(this, name.impl());
}

PassRefPtr<EditingText> Document::createEditingTextNode(const String &text)
{
    return new EditingText(this, text);
}

PassRefPtr<CSSStyleDeclaration> Document::createCSSStyleDeclaration()
{
    return new CSSMutableStyleDeclaration;
}

PassRefPtr<Node> Document::importNode(Node* importedNode, bool deep, ExceptionCode& ec)
{
    ec = 0;
    
    if (!importedNode) {
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
            Element *oldElement = static_cast<Element *>(importedNode);
            RefPtr<Element> newElement = createElementNS(oldElement->namespaceURI(), oldElement->tagQName().toString(), ec);
                        
            if (ec != 0)
                return 0;

            NamedAttrMap* attrs = oldElement->attributes(true);
            if (attrs) {
                unsigned length = attrs->length();
                for (unsigned i = 0; i < length; i++) {
                    Attribute* attr = attrs->attributeItem(i);
                    newElement->setAttribute(attr->name(), attr->value().impl(), ec);
                    if (ec != 0)
                        return 0;
                }
            }

            newElement->copyNonAttributeProperties(oldElement);

            if (deep) {
                for (Node* oldChild = oldElement->firstChild(); oldChild; oldChild = oldChild->nextSibling()) {
                    RefPtr<Node> newChild = importNode(oldChild, true, ec);
                    if (ec != 0)
                        return 0;
                    newElement->appendChild(newChild.release(), ec);
                    if (ec != 0)
                        return 0;
                }
            }

            return newElement.release();
        }
        case ATTRIBUTE_NODE:
        case ENTITY_NODE:
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
        case DOCUMENT_FRAGMENT_NODE:
        case NOTATION_NODE:
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
    
    switch (source->nodeType()) {
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
            ec = NOT_SUPPORTED_ERR;
            return 0;            
        case ATTRIBUTE_NODE: {                   
            Attr* attr = static_cast<Attr*>(source.get());
            if (attr->ownerElement())
                attr->ownerElement()->removeAttributeNode(attr, ec);
            attr->m_specified = true;
            break;
        }       
        default:
            if (source->parentNode())
                source->parentNode()->removeChild(source.get(), ec);
    }
                
    for (Node* node = source.get(); node; node = node->traverseNextNode(source.get())) {
        KJS::ScriptInterpreter::updateDOMNodeDocument(node, node->document(), this);
        node->setDocument(this);
    }

    return source;
}

PassRefPtr<Element> Document::createElementNS(const String &_namespaceURI, const String &qualifiedName, ExceptionCode& ec)
{
    // FIXME: We'd like a faster code path that skips this check for calls from inside the engine where the name is known to be valid.
    String prefix, localName;
    if (!parseQualifiedName(qualifiedName, prefix, localName)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }

    RefPtr<Element> e;
    QualifiedName qName = QualifiedName(AtomicString(prefix), AtomicString(localName), AtomicString(_namespaceURI));
    
    // FIXME: Use registered namespaces and look up in a hash to find the right factory.
    if (_namespaceURI == xhtmlNamespaceURI) {
        e = HTMLElementFactory::createHTMLElement(qName.localName(), this, 0, false);
        if (e && !prefix.isNull()) {
            e->setPrefix(qName.prefix(), ec);
            if (ec)
                return 0;
        }
    }
#if SVG_SUPPORT
    else if (_namespaceURI == WebCore::SVGNames::svgNamespaceURI)
        e = WebCore::SVGElementFactory::createSVGElement(qName, this, false);
#endif
    
    if (!e)
        e = new Element(qName, document());
    
    return e.release();
}

Element *Document::getElementById(const AtomicString& elementId) const
{
    if (elementId.length() == 0)
        return 0;

    Element *element = m_elementsById.get(elementId.impl());
    if (element)
        return element;
        
    if (m_duplicateIds.contains(elementId.impl())) {
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
    }
    return 0;
}

String Document::readyState() const
{
    if (Frame* f = frame()) {
        if (f->isComplete()) 
            return "complete";
        if (parsing()) 
            return "loading";
      return "loaded";
      // FIXME: What does "interactive" mean?
      // FIXME: Missing support for "uninitialized".
    }
    return String();
}

String Document::inputEncoding() const
{
    if (Decoder* d = decoder())
        return d->encodingName();
    return String();
}

String Document::defaultCharset() const
{
    if (Frame* f = frame())
        return f->settings()->encoding();
    return String();
}

void Document::setCharset(const String& charset)
{
    if (!decoder())
        return;
    decoder()->setEncodingName(charset.deprecatedString().ascii(), Decoder::UserChosenEncoding);
}

Element* Document::elementFromPoint(int x, int y) const
{
    if (!renderer())
        return 0;

    RenderObject::NodeInfo nodeInfo(true, true);
    renderer()->layer()->hitTest(nodeInfo, IntPoint(x, y)); 

    Node* n = nodeInfo.innerNode();
    while (n && !n->isElementNode())
        n = n->parentNode();
    if (n)
        n = n->shadowAncestorNode();
    return static_cast<Element*>(n);
}

void Document::addElementById(const AtomicString& elementId, Element* element)
{
    if (!m_elementsById.contains(elementId.impl()))
        m_elementsById.set(elementId.impl(), element);
    else
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
        f->setTitle(m_title);
}

void Document::setTitle(const String& title, Node* titleElement)
{
    if (!titleElement) {
        // Title set by JavaScript -- overrides any title elements.
        m_titleSetExplicitly = true;
        m_titleElement = 0;
    } else if (titleElement != m_titleElement) {
        if (m_titleElement)
            // Only allow the first title element to change the title -- others have no effect.
            return;
        m_titleElement = titleElement;
    }

    if (m_title == title)
        return;

    m_title = title;
    updateTitle();
}

void Document::removeTitle(Node* titleElement)
{
    if (m_titleElement != titleElement)
        return;

    // FIXME: Ideally we might want this to search for the first remaining title element, and use it.
    m_titleElement = 0;

    if (!m_title.isEmpty()) {
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

Frame* Document::frame() const 
{
    return m_view ? m_view->frame() : 0; 
}

PassRefPtr<Range> Document::createRange()
{
    return new Range(this);
}

PassRefPtr<NodeIterator> Document::createNodeIterator(Node* root, unsigned whatToShow, 
    PassRefPtr<NodeFilter> filter, bool expandEntityReferences, ExceptionCode& ec)
{
    if (!root) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return new NodeIterator(root, whatToShow, filter, expandEntityReferences);
}

PassRefPtr<TreeWalker> Document::createTreeWalker(Node *root, unsigned whatToShow, 
    PassRefPtr<NodeFilter> filter, bool expandEntityReferences, ExceptionCode& ec)
{
    if (!root) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return new TreeWalker(root, whatToShow, filter, expandEntityReferences);
}

void Document::setDocumentChanged(bool b)
{
    if (b) {
        if (!m_docChanged) {
            if (!changedDocuments)
                changedDocuments = new DeprecatedPtrList<Document>;
            changedDocuments->append(this);
        }
        if (m_accessKeyMapValid) {
            m_accessKeyMapValid = false;
            m_elementsByAccessKey.clear();
        }
    } else {
        if (m_docChanged && changedDocuments)
            changedDocuments->remove(this);
    }

    m_docChanged = b;
}

void Document::recalcStyle(StyleChange change)
{
    if (m_inStyleRecalc)
        return; // Guard against re-entrancy. -dwh
        
    m_inStyleRecalc = true;
    
    ASSERT(!renderer() || renderArena());
    if (!renderer() || !renderArena())
        goto bail_out;

    if (change == Force) {
        RenderStyle* oldStyle = renderer()->style();
        if (oldStyle)
            oldStyle->ref();
        RenderStyle* _style = new (m_renderArena) RenderStyle();
        _style->ref();
        _style->setDisplay(BLOCK);
        _style->setVisuallyOrdered(visuallyOrdered);
        // ### make the font stuff _really_ work!!!!

        FontDescription fontDescription;
        fontDescription.setUsePrinterFont(printing());
        if (m_view) {
            const KHTMLSettings *settings = m_view->frame()->settings();
            if (printing() && !settings->shouldPrintBackgrounds())
                _style->setForceBackgroundsToWhite(true);
            const AtomicString& stdfont = settings->stdFontName();
            if (!stdfont.isEmpty()) {
                fontDescription.firstFamily().setFamily(stdfont);
                fontDescription.firstFamily().appendFamily(0);
            }
            m_styleSelector->setFontSize(fontDescription, m_styleSelector->fontSizeForKeyword(CSS_VAL_MEDIUM, inCompatMode()));
        }

        _style->setFontDescription(fontDescription);
        _style->font().update();
        if (inCompatMode())
            _style->setHtmlHacks(true); // enable html specific rendering tricks

        StyleChange ch = diff(_style, oldStyle);
        if (renderer() && ch != NoChange)
            renderer()->setStyle(_style);
        if (change != Force)
            change = ch;

        _style->deref(m_renderArena);
        if (oldStyle)
            oldStyle->deref(m_renderArena);
    }

    for (Node* n = fastFirstChild(); n; n = n->nextSibling())
        if (change >= Inherit || n->hasChangedChild() || n->changed())
            n->recalcStyle(change);

    if (changed() && m_view)
        m_view->layout();

bail_out:
    setChanged(false);
    setHasChangedChild(false);
    setDocumentChanged(false);
    
    m_inStyleRecalc = false;
    
    // If we wanted to emit the implicitClose() during recalcStyle, do so now that we're finished.
    if (m_closeAfterStyleRecalc) {
        m_closeAfterStyleRecalc = false;
        implicitClose();
    }
}

void Document::updateRendering()
{
    if (hasChangedChild())
        recalcStyle(NoChange);
}

void Document::updateDocumentsRendering()
{
    if (!changedDocuments)
        return;

    while (Document* doc = changedDocuments->take()) {
        doc->m_docChanged = false;
        doc->updateRendering();
    }
}

void Document::updateLayout()
{
    // FIXME: Dave Hyatt's pretty sure we can remove this because layout calls recalcStyle as needed.
    updateRendering();

    // Only do a layout if changes have occurred that make it necessary.      
    if (m_view && renderer() && renderer()->needsLayout())
        m_view->layout();
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
        updateStyleSelector();    
    }

    updateLayout();

    m_ignorePendingStylesheets = oldIgnore;
}

void Document::attach()
{
    assert(!attached());
    assert(!m_inPageCache);

    if (!m_renderArena)
        m_renderArena = new RenderArena();
    
    // Create the rendering tree
    setRenderer(new (m_renderArena) RenderCanvas(this, m_view));

    recalcStyle(Force);

    RenderObject* render = renderer();
    setRenderer(0);

    ContainerNode::attach();

    setRenderer(render);
}

void Document::detach()
{
    RenderObject* render = renderer();

    // indicate destruction mode,  i.e. attached() but renderer == 0
    setRenderer(0);
    
    if (m_inPageCache) {
        if (render)
            getAccObjectCache()->remove(render);
        return;
    }

    // Empty out these lists as a performance optimization, since detaching
    // all the individual render objects will cause all the RenderImage
    // objects to remove themselves from the lists.
    m_imageLoadEventDispatchSoonList.clear();
    m_imageLoadEventDispatchingList.clear();
    
    m_hoverNode = 0;
    m_focusNode = 0;
    m_activeNode = 0;

    ContainerNode::detach();

    if (render)
        render->destroy();

    m_view = 0;
    
    if (m_renderArena) {
        delete m_renderArena;
        m_renderArena = 0;
    }
}

void Document::removeAllEventListenersFromAllNodes()
{
    m_windowEventListeners.clear();
    removeAllDisconnectedNodeEventListeners();
    for (Node *n = this; n; n = n->traverseNextNode()) {
        if (!n->isEventTargetNode())
            continue;
        EventTargetNodeCast(n)->removeAllEventListeners();
    }
}

void Document::registerDisconnectedNodeWithEventListeners(Node* node)
{
    m_disconnectedNodesWithEventListeners.add(node);
}

void Document::unregisterDisconnectedNodeWithEventListeners(Node* node)
{
    m_disconnectedNodesWithEventListeners.remove(node);
}

void Document::removeAllDisconnectedNodeEventListeners()
{
    HashSet<Node*>::iterator end = m_disconnectedNodesWithEventListeners.end();
    for (HashSet<Node*>::iterator i = m_disconnectedNodesWithEventListeners.begin(); i != end; ++i)
        EventTargetNodeCast(*i)->removeAllEventListeners();
    m_disconnectedNodesWithEventListeners.clear();
}

AccessibilityObjectCache* Document::getAccObjectCache() const
{
    // The only document that actually has a AccessibilityObjectCache is the top-level
    // document.  This is because we need to be able to get from any WebCoreAXObject
    // to any other WebCoreAXObject on the same page.  Using a single cache allows
    // lookups across nested webareas (i.e. multiple documents).
    
    if (m_accCache) {
        // return already known top-level cache
        if (!ownerElement())
            return m_accCache;
        
        // In some pages with frames, the cache is created before the sub-webarea is
        // inserted into the tree.  Here, we catch that case and just toss the old
        // cache and start over.
        delete m_accCache;
        m_accCache = 0;
    }

    // ask the top-level document for its cache
    Document* doc = topDocument();
    if (doc != this)
        return doc->getAccObjectCache();
    
    // this is the top-level document, so install a new cache
    m_accCache = new AccessibilityObjectCache;
    return m_accCache;
}

void Document::setVisuallyOrdered()
{
    visuallyOrdered = true;
    if (renderer())
        renderer()->style()->setVisuallyOrdered(true);
}

void Document::updateSelection()
{
    if (!renderer())
        return;
    
    RenderCanvas *canvas = static_cast<RenderCanvas*>(renderer());
    SelectionController s = frame()->selection();
    if (!s.isRange()) {
        canvas->clearSelection();
    }
    else {
        Position startPos = VisiblePosition(s.start(), s.affinity()).deepEquivalent();
        Position endPos = VisiblePosition(s.end(), s.affinity()).deepEquivalent();
        if (startPos.isNotNull() && endPos.isNotNull()) {
            RenderObject *startRenderer = startPos.node()->renderer();
            RenderObject *endRenderer = endPos.node()->renderer();
            static_cast<RenderCanvas*>(renderer())->setSelection(startRenderer, startPos.offset(), endRenderer, endPos.offset());
        }
    }
    
#if __APPLE__
    // FIXME: We shouldn't post this AX notification here since updateSelection() is called far to often: every time Safari gains
    // or loses focus, and once for every low level change to the selection during an editing operation.
    // FIXME: We no longer blow away the selection before starting an editing operation, so the isNotNull checks below are no 
    // longer a correct way to check for user-level selection changes.
    if (AccessibilityObjectCache::accessibilityEnabled() && s.start().isNotNull() && s.end().isNotNull()) {
        getAccObjectCache()->postNotificationToTopWebArea(renderer(), "AXSelectedTextChanged");
    }
#endif
}

Tokenizer *Document::createTokenizer()
{
    return newXMLTokenizer(this, m_view);
}

void Document::open()
{
    if ((frame() && frame()->isLoadingMainResource()) || (tokenizer() && tokenizer()->executingScript()))
        return;

    implicitOpen();

    if (frame())
        frame()->didExplicitOpen();

    // This is work that we should probably do in clear(), but we can't have it
    // happen when implicitOpen() is called unless we reorganize Frame code.
    setURL(DeprecatedString());
    if (Document *parent = parentDocument())
        setBaseURL(parent->baseURL());
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
}

HTMLElement* Document::body()
{
    Node *de = documentElement();
    if (!de)
        return 0;
    
    // try to prefer a FRAMESET element over BODY
    Node* body = 0;
    for (Node* i = de->firstChild(); i; i = i->nextSibling()) {
        if (i->hasTagName(framesetTag))
            return static_cast<HTMLElement*>(i);
        
        if (i->hasTagName(bodyTag))
            body = i;
    }
    return static_cast<HTMLElement *>(body);
}

void Document::close()
{
    if (frame())
        frame()->endIfNotLoading();
    implicitClose();
}

void Document::implicitClose()
{
    // If we're in the middle of recalcStyle, we need to defer the close until the style information is accurate and all elements are re-attached.
    if (m_inStyleRecalc) {
        m_closeAfterStyleRecalc = true;
        return;
    }

    bool wasLocationChangePending = frame() && frame()->isScheduledLocationChangePending();
    bool doload = !parsing() && m_tokenizer && !m_processingLoadEvent && !wasLocationChangePending;
    
    if (!doload)
        return;

    m_processingLoadEvent = true;

    // We have to clear the tokenizer, in case someone document.write()s from the
    // onLoad event handler, as in Radar 3206524.
    delete m_tokenizer;
    m_tokenizer = 0;

    // Create a body element if we don't already have one.
    // In the case of Radar 3758785, the window.onload was set in some javascript, but never fired because there was no body.  
    // This behavior now matches Firefox and IE.
    HTMLElement *body = this->body();
    if (!body && isHTMLDocument()) {
        Node *de = documentElement();
        if (de) {
            body = new HTMLBodyElement(this);
            ExceptionCode ec = 0;
            de->appendChild(body, ec);
            if (ec != 0)
                body = 0;
        }
    }
    
    dispatchImageLoadEventsNow();
    this->dispatchWindowEvent(loadEvent, false, false);
    if (Frame *p = frame())
        p->handledOnloadEvents();
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("onload fired at %d\n", elapsedTime());
#endif

    m_processingLoadEvent = false;

    // Make sure both the initial layout and reflow happen after the onload
    // fires. This will improve onload scores, and other browsers do it.
    // If they wanna cheat, we can too. -dwh

    if (frame() && frame()->isScheduledLocationChangePending() && elapsedTime() < cLayoutScheduleThreshold) {
        // Just bail out. Before or during the onload we were shifted to another page.
        // The old i-Bench suite does this. When this happens don't bother painting or laying out.        
        view()->unscheduleRelayout();
        return;
    }

    if (frame())
        frame()->checkEmitLoadEvent();

    // Now do our painting/layout, but only if we aren't in a subframe or if we're in a subframe
    // that has been sized already.  Otherwise, our view size would be incorrect, so doing any 
    // layout/painting now would be pointless.
    if (!ownerElement() || (ownerElement()->renderer() && !ownerElement()->renderer()->needsLayout())) {
        updateRendering();
        
        // Always do a layout after loading if needed.
        if (view() && renderer() && (!renderer()->firstChild() || renderer()->needsLayout()))
            view()->layout();
    }
#if __APPLE__
    if (renderer() && AccessibilityObjectCache::accessibilityEnabled())
        getAccObjectCache()->postNotification(renderer(), "AXLoadComplete");
#endif

#if SVG_SUPPORT
    // FIXME: Officially, time 0 is when the outermost <svg> recieves its
    // SVGLoad event, but we don't implement those yet.  This is close enough
    // for now.  In some cases we should have fired earlier.
    if (svgExtensions())
        accessSVGExtensions()->timeScheduler()->startAnimations();
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
    // We can update layout if:
    // (a) we actually need a layout
    // (b) our stylesheets are all loaded
    // (c) we have a <body>
    return (renderer() && renderer()->needsLayout() && haveStylesheetsLoaded() &&
            documentElement() && documentElement()->renderer() &&
            (!documentElement()->hasTagName(htmlTag) || body()));
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

void Document::write(const String &text)
{
    write(text.deprecatedString());
}

void Document::write(const DeprecatedString &text)
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Beginning a document.write at %d\n", elapsedTime());
#endif
    
    if (!m_tokenizer) {
        open();
        assert(m_tokenizer);
        if (!m_tokenizer)
            return;
        write(DeprecatedString("<html>"));
    }
    m_tokenizer->write(text, false);
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Ending a document.write at %d\n", elapsedTime());
#endif    
}

void Document::writeln(const String &text)
{
    write(text);
    write(String("\n"));
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
    DeprecatedPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current();)
        m_windowEventListeners.removeRef(it.current());
}

void Document::setURL(const DeprecatedString& url)
{
    m_url = url;
    if (m_styleSelector)
        m_styleSelector->setEncodedURL(m_url);
}

void Document::setStyleSheet(const String &url, const String &sheet)
{
    m_sheet = new CSSStyleSheet(this, url);
    m_sheet->parseString(sheet);
    m_loadingSheet = false;

    updateStyleSelector();
}

void Document::setUserStyleSheet(const String& sheet)
{
    if (m_usersheet != sheet) {
        m_usersheet = sheet;
        updateStyleSelector();
    }
}

CSSStyleSheet* Document::elementSheet()
{
    if (!m_elemSheet)
        m_elemSheet = new CSSStyleSheet(this, baseURL());
    return m_elemSheet.get();
}

void Document::determineParseMode(const DeprecatedString &/*str*/)
{
    // For XML documents use strict parse mode.  HTML docs will override this method to
    // determine their parse mode.
    pMode = Strict;
    hMode = XHtml;
}

Node *Document::nextFocusNode(Node *fromNode)
{
    unsigned short fromTabIndex;

    if (!fromNode) {
        // No starting node supplied; begin with the top of the document
        Node *n;

        int lowestTabIndex = 65535;
        for (n = this; n != 0; n = n->traverseNextNode()) {
            if (n->isKeyboardFocusable()) {
                if ((n->tabIndex() > 0) && (n->tabIndex() < lowestTabIndex))
                    lowestTabIndex = n->tabIndex();
            }
        }

        if (lowestTabIndex == 65535)
            lowestTabIndex = 0;

        // Go to the first node in the document that has the desired tab index
        for (n = this; n != 0; n = n->traverseNextNode()) {
            if (n->isKeyboardFocusable() && (n->tabIndex() == lowestTabIndex))
                return n;
        }

        return 0;
    }
    else {
        fromTabIndex = fromNode->tabIndex();
    }

    if (fromTabIndex == 0) {
        // Just need to find the next selectable node after fromNode (in document order) that doesn't have a tab index
        Node *n = fromNode->traverseNextNode();
        while (n && !(n->isKeyboardFocusable() && n->tabIndex() == 0))
            n = n->traverseNextNode();
        return n;
    }
    else {
        // Find the lowest tab index out of all the nodes except fromNode, that is greater than or equal to fromNode's
        // tab index. For nodes with the same tab index as fromNode, we are only interested in those that come after
        // fromNode in document order.
        // If we don't find a suitable tab index, the next focus node will be one with a tab index of 0.
        unsigned short lowestSuitableTabIndex = 65535;
        Node *n;

        bool reachedFromNode = false;
        for (n = this; n != 0; n = n->traverseNextNode()) {
            if (n->isKeyboardFocusable() &&
                ((reachedFromNode && (n->tabIndex() >= fromTabIndex)) ||
                 (!reachedFromNode && (n->tabIndex() > fromTabIndex))) &&
                (n->tabIndex() < lowestSuitableTabIndex) &&
                (n != fromNode)) {

                // We found a selectable node with a tab index at least as high as fromNode's. Keep searching though,
                // as there may be another node which has a lower tab index but is still suitable for use.
                lowestSuitableTabIndex = n->tabIndex();
            }

            if (n == fromNode)
                reachedFromNode = true;
        }

        if (lowestSuitableTabIndex == 65535) {
            // No next node with a tab index -> just take first node with tab index of 0
            Node *n = this;
            while (n && !(n->isKeyboardFocusable() && n->tabIndex() == 0))
                n = n->traverseNextNode();
            return n;
        }

        // Search forwards from fromNode
        for (n = fromNode->traverseNextNode(); n != 0; n = n->traverseNextNode()) {
            if (n->isKeyboardFocusable() && (n->tabIndex() == lowestSuitableTabIndex))
                return n;
        }

        // The next node isn't after fromNode, start from the beginning of the document
        for (n = this; n != fromNode; n = n->traverseNextNode()) {
            if (n->isKeyboardFocusable() && (n->tabIndex() == lowestSuitableTabIndex))
                return n;
        }

        assert(false); // should never get here
        return 0;
    }
}

Node *Document::previousFocusNode(Node *fromNode)
{
    Node *lastNode = this;
    while (lastNode->lastChild())
        lastNode = lastNode->lastChild();

    if (!fromNode) {
        // No starting node supplied; begin with the very last node in the document
        Node *n;

        int highestTabIndex = 0;
        for (n = lastNode; n != 0; n = n->traversePreviousNode()) {
            if (n->isKeyboardFocusable()) {
                if (n->tabIndex() == 0)
                    return n;
                else if (n->tabIndex() > highestTabIndex)
                    highestTabIndex = n->tabIndex();
            }
        }

        // No node with a tab index of 0; just go to the last node with the highest tab index
        for (n = lastNode; n != 0; n = n->traversePreviousNode()) {
            if (n->isKeyboardFocusable() && (n->tabIndex() == highestTabIndex))
                return n;
        }

        return 0;
    }
    else {
        unsigned short fromTabIndex = fromNode->tabIndex();

        if (fromTabIndex == 0) {
            // Find the previous selectable node before fromNode (in document order) that doesn't have a tab index
            Node *n = fromNode->traversePreviousNode();
            while (n && !(n->isKeyboardFocusable() && n->tabIndex() == 0))
                n = n->traversePreviousNode();
            if (n)
                return n;

            // No previous nodes with a 0 tab index, go to the last node in the document that has the highest tab index
            int highestTabIndex = 0;
            for (n = this; n != 0; n = n->traverseNextNode()) {
                if (n->isKeyboardFocusable() && (n->tabIndex() > highestTabIndex))
                    highestTabIndex = n->tabIndex();
            }

            if (highestTabIndex == 0)
                return 0;

            for (n = lastNode; n != 0; n = n->traversePreviousNode()) {
                if (n->isKeyboardFocusable() && (n->tabIndex() == highestTabIndex))
                    return n;
            }

            assert(false); // should never get here
            return 0;
        }
        else {
            // Find the lowest tab index out of all the nodes except fromNode, that is less than or equal to fromNode's
            // tab index. For nodes with the same tab index as fromNode, we are only interested in those before
            // fromNode.
            // If we don't find a suitable tab index, then there will be no previous focus node.
            unsigned short highestSuitableTabIndex = 0;
            Node *n;

            bool reachedFromNode = false;
            for (n = this; n != 0; n = n->traverseNextNode()) {
                if (n->isKeyboardFocusable() &&
                    ((!reachedFromNode && (n->tabIndex() <= fromTabIndex)) ||
                     (reachedFromNode && (n->tabIndex() < fromTabIndex)))  &&
                    (n->tabIndex() > highestSuitableTabIndex) &&
                    (n != fromNode)) {

                    // We found a selectable node with a tab index no higher than fromNode's. Keep searching though, as
                    // there may be another node which has a higher tab index but is still suitable for use.
                    highestSuitableTabIndex = n->tabIndex();
                }

                if (n == fromNode)
                    reachedFromNode = true;
            }

            if (highestSuitableTabIndex == 0) {
                // No previous node with a tab index. Since the order specified by HTML is nodes with tab index > 0
                // first, this means that there is no previous node.
                return 0;
            }

            // Search backwards from fromNode
            for (n = fromNode->traversePreviousNode(); n != 0; n = n->traversePreviousNode()) {
                if (n->isKeyboardFocusable() && (n->tabIndex() == highestSuitableTabIndex))
                    return n;
            }
            // The previous node isn't before fromNode, start from the end of the document
            for (n = lastNode; n != fromNode; n = n->traversePreviousNode()) {
                if (n->isKeyboardFocusable() && (n->tabIndex() == highestSuitableTabIndex))
                    return n;
            }

            assert(false); // should never get here
            return 0;
        }
    }
}

int Document::nodeAbsIndex(Node *node)
{
    assert(node->document() == this);

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

void Document::processHttpEquiv(const String &equiv, const String &content)
{
    assert(!equiv.isNull() && !content.isNull());

    Frame *frame = this->frame();

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
        // get delay and url
        DeprecatedString str = content.deprecatedString().stripWhiteSpace();
        int pos = str.find(RegularExpression("[;,]"));
        if (pos == -1)
            pos = str.find(RegularExpression("[ \t]"));

        if (pos == -1) // There can be no url (David)
        {
            bool ok = false;
            int delay = 0;
            delay = str.toInt(&ok);
            // We want a new history item if the refresh timeout > 1 second
            if (ok && frame)
                frame->scheduleRedirection(delay, frame->url().url(), delay <= 1);
        } else {
            double delay = 0;
            bool ok = false;
            delay = str.left(pos).stripWhiteSpace().toDouble(&ok);

            pos++;
            while(pos < (int)str.length() && str[pos].isSpace()) pos++;
            str = str.mid(pos);
            if (str.find("url", 0,  false) == 0)
                str = str.mid(3);
            str = str.stripWhiteSpace();
            if (str.length() && str[0] == '=')
                str = str.mid(1).stripWhiteSpace();
            str = parseURL(String(str)).deprecatedString();
            if (ok && frame)
                // We want a new history item if the refresh timeout > 1 second
                frame->scheduleRedirection(delay, completeURL(str), delay <= 1);
        }
    } else if (equalIgnoringCase(equiv, "expires")) {
        DeprecatedString str = content.deprecatedString().stripWhiteSpace();
        time_t expire_date = str.toInt();
        if (m_docLoader)
            m_docLoader->setExpireDate(expire_date);
    } else if ((equalIgnoringCase(equiv, "pragma") || equalIgnoringCase(equiv, "cache-control")) && frame) {
        DeprecatedString str = content.deprecatedString().lower().stripWhiteSpace();
        KURL url = frame->url();
    } else if (equalIgnoringCase(equiv, "set-cookie")) {
        // ### FIXME: make setCookie work on XML documents too; e.g. in case of <html:meta .....>
        if (isHTMLDocument())
            static_cast<HTMLDocument *>(this)->setCookie(content);
    }
}

MouseEventWithHitTestResults Document::prepareMouseEvent(bool readonly, bool active, bool mouseMove,
                                                         const IntPoint& point, const PlatformMouseEvent& event)
{
    if (!renderer())
        return MouseEventWithHitTestResults(event, 0, false);

    assert(renderer()->isCanvas());
    RenderObject::NodeInfo renderInfo(readonly, active, mouseMove);
    renderer()->layer()->hitTest(renderInfo, point);

    if (!readonly)
        updateRendering();

    bool isOverLink = renderInfo.URLElement() && !renderInfo.URLElement()->getAttribute(hrefAttr).isNull();
    return MouseEventWithHitTestResults(event, renderInfo.innerNode(), isOverLink);
}

// DOM Section 1.1.1
bool Document::childAllowed(Node *newChild)
{
    // Documents may contain a maximum of one Element child
    if (newChild->isElementNode()) {
        Node *c;
        for (c = firstChild(); c; c = c->nextSibling()) {
            if (c->isElementNode())
                return false;
        }
    }

    // Documents may contain a maximum of one DocumentType child
    if (newChild->nodeType() == DOCUMENT_TYPE_NODE) {
        Node *c;
        for (c = firstChild(); c; c = c->nextSibling()) {
            if (c->nodeType() == DOCUMENT_TYPE_NODE)
                return false;
        }
    }

    return childTypeAllowed(newChild->nodeType());
}

bool Document::childTypeAllowed(NodeType type)
{
    switch (type) {
        case ELEMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
        case COMMENT_NODE:
        case DOCUMENT_TYPE_NODE:
            return true;
        default:
            return false;
    }
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
void Document::stylesheetLoaded()
{
  // Make sure we knew this sheet was pending, and that our count isn't out of sync.
  assert(m_pendingStylesheets > 0);

  m_pendingStylesheets--;
  
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
  if (!ownerElement())
      printf("Stylesheet loaded at time %d. %d stylesheets still remain.\n", elapsedTime(), m_pendingStylesheets);
#endif

  updateStyleSelector();    
}

void Document::updateStyleSelector()
{
    // Don't bother updating, since we haven't loaded all our style info yet.
    if (!haveStylesheetsLoaded())
        return;

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
        renderer()->setNeedsLayoutAndMinMaxRecalc();
        if (view())
            view()->scheduleRelayout();
    }
}


DeprecatedStringList Document::availableStyleSheets() const
{
    return m_availableSheets;
}

void Document::recalcStyleSelector()
{
    if (!renderer() || !attached())
        return;

    DeprecatedPtrList<StyleSheet> oldStyleSheets = m_styleSheets->styleSheets;
    m_styleSheets->styleSheets.clear();
    m_availableSheets.clear();
    Node *n;
    for (n = this; n; n = n->traverseNextNode()) {
        StyleSheet *sheet = 0;

        if (n->nodeType() == PROCESSING_INSTRUCTION_NODE)
        {
            // Processing instruction (XML documents only)
            ProcessingInstruction* pi = static_cast<ProcessingInstruction*>(n);
            sheet = pi->sheet();
#ifdef KHTML_XSLT
            // Don't apply XSL transforms to already transformed documents -- <rdar://problem/4132806>
            if (pi->isXSL() && !transformSourceDocument()) {
                // Don't apply XSL transforms until loading is finished.
                if (!parsing())
                    applyXSLTransform(pi);
                return;
            }
#endif
            if (!sheet && !pi->localHref().isEmpty())
            {
                // Processing instruction with reference to an element in this document - e.g.
                // <?xml-stylesheet href="#mystyle">, with the element
                // <foo id="mystyle">heading { color: red; }</foo> at some location in
                // the document
                Element* elem = getElementById(pi->localHref().impl());
                if (elem) {
                    String sheetText("");
                    Node *c;
                    for (c = elem->firstChild(); c; c = c->nextSibling()) {
                        if (c->nodeType() == TEXT_NODE || c->nodeType() == CDATA_SECTION_NODE)
                            sheetText += c->nodeValue();
                    }

                    CSSStyleSheet *cssSheet = new CSSStyleSheet(this);
                    cssSheet->parseString(sheetText);
                    pi->setStyleSheet(cssSheet);
                    sheet = cssSheet;
                }
            }

        }
        else if (n->isHTMLElement() && (n->hasTagName(linkTag) || n->hasTagName(styleTag))) {
            HTMLElement *e = static_cast<HTMLElement *>(n);
            DeprecatedString title = e->getAttribute(titleAttr).deprecatedString();
            bool enabledViaScript = false;
            if (e->hasLocalName(linkTag)) {
                // <LINK> element
                HTMLLinkElement* l = static_cast<HTMLLinkElement*>(n);
                if (l->isLoading() || l->isDisabled())
                    continue;
                if (!l->sheet())
                    title = DeprecatedString::null;
                enabledViaScript = l->isEnabledViaScript();
            }

            // Get the current preferred styleset.  This is the
            // set of sheets that will be enabled.
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
                    DeprecatedString rel = e->getAttribute(relAttr).deprecatedString();
                    if (e->hasLocalName(styleTag) || !rel.contains("alternate"))
                        m_preferredStylesheetSet = m_selectedStylesheetSet = title;
                }
                      
                if (!m_availableSheets.contains(title))
                    m_availableSheets.append(title);
                
                if (title != m_preferredStylesheetSet)
                    sheet = 0;
            }
        }
#if SVG_SUPPORT
        else if (n->isSVGElement() && n->hasTagName(WebCore::SVGNames::styleTag)) {
            DeprecatedString title;
            // <STYLE> element
            WebCore::SVGStyleElement *s = static_cast<WebCore::SVGStyleElement*>(n);
            if (!s->isLoading()) {
                sheet = s->sheet();
                if(sheet)
                    title = s->getAttribute(WebCore::SVGNames::titleAttr).deprecatedString();
            }

            if (!title.isEmpty() && m_preferredStylesheetSet.isEmpty())
                m_preferredStylesheetSet = m_selectedStylesheetSet = title;

            if (!title.isEmpty()) {
                if (title != m_preferredStylesheetSet)
                    sheet = 0; // don't use it

                title = title.replace('&', "&&");

                if (!m_availableSheets.contains(title))
                    m_availableSheets.append(title);
            }
       }
#endif

        if (sheet) {
            sheet->ref();
            m_styleSheets->styleSheets.append(sheet);
        }
    
        // For HTML documents, stylesheets are not allowed within/after the <BODY> tag. So we
        // can stop searching here.
        if (isHTMLDocument() && n->hasTagName(bodyTag))
            break;
    }

    // De-reference all the stylesheets in the old list
    DeprecatedPtrListIterator<StyleSheet> it(oldStyleSheets);
    for (; it.current(); ++it)
        it.current()->deref();

    // Create a new style selector
    delete m_styleSelector;
    String usersheet = m_usersheet;
    if (m_view && m_view->mediaType() == "print")
        usersheet += m_printSheet;
    m_styleSelector = new CSSStyleSelector(this, usersheet, m_styleSheets.get(), !inCompatMode());
    m_styleSelector->setEncodedURL(m_url);
    m_styleSelectorDirty = false;
}

void Document::setHoverNode(PassRefPtr<Node> newHoverNode)
{
    m_hoverNode = newHoverNode;
}

void Document::setActiveNode(PassRefPtr<Node> newActiveNode)
{
    m_activeNode = newActiveNode;
}

void Document::hoveredNodeDetached(Node* node)
{
    if (!m_hoverNode || (node != m_hoverNode && (!m_hoverNode->isTextNode() || node != m_hoverNode->parent())))
        return;

    m_hoverNode = node->parent();
    while (m_hoverNode && !m_hoverNode->renderer())
        m_hoverNode = m_hoverNode->parent();
    if (view())
        view()->scheduleHoverStateUpdate();
}

void Document::activeChainNodeDetached(Node* node)
{
    if (!m_activeNode || (node != m_activeNode && (!m_activeNode->isTextNode() || node != m_activeNode->parent())))
        return;

    m_activeNode = node->parent();
    while (m_activeNode && !m_activeNode->renderer())
        m_activeNode = m_activeNode->parent();
}

bool Document::relinquishesEditingFocus(Node *node)
{
    assert(node);
    assert(node->isContentEditable());

    Node *root = node->rootEditableElement();
    if (!frame() || !root)
        return false;

    return frame()->shouldEndEditing(rangeOfContents(root).get());
}

bool Document::acceptsEditingFocus(Node *node)
{
    assert(node);
    assert(node->isContentEditable());

    Node *root = node->rootEditableElement();
    if (!frame() || !root)
        return false;

    return frame()->shouldBeginEditing(rangeOfContents(root).get());
}

void Document::didBeginEditing()
{
    if (!frame())
        return;
    
    frame()->didBeginEditing();
}

void Document::didEndEditing()
{
    if (!frame())
        return;
    
    frame()->didEndEditing();
}

#if __APPLE__
const DeprecatedValueList<DashboardRegionValue> & Document::dashboardRegions() const
{
    return m_dashboardRegions;
}

void Document::setDashboardRegions (const DeprecatedValueList<DashboardRegionValue>& regions)
{
    m_dashboardRegions = regions;
    setDashboardRegionsDirty (false);
}
#endif

static Widget *widgetForNode(Node *focusNode)
{
    if (!focusNode)
        return 0;
    RenderObject *renderer = focusNode->renderer();
    if (!renderer || !renderer->isWidget())
        return 0;
    return static_cast<RenderWidget *>(renderer)->widget();
}

bool Document::setFocusNode(PassRefPtr<Node> newFocusNode)
{    
    // Make sure newFocusNode is actually in this document
    if (newFocusNode && (newFocusNode->document() != this))
        return true;

    if (m_focusNode == newFocusNode)
        return true;

    if (m_focusNode && m_focusNode.get() == m_focusNode->rootEditableElement() && !relinquishesEditingFocus(m_focusNode.get()))
        return false;
        
    bool focusChangeBlocked = false;
    RefPtr<Node> oldFocusNode = m_focusNode;
    m_focusNode = 0;
    clearSelectionIfNeeded(newFocusNode.get());

    // Remove focus from the existing focus node (if any)
    if (oldFocusNode && !oldFocusNode->m_inDetach) { 
        if (oldFocusNode->active())
            oldFocusNode->setActive(false);

        oldFocusNode->setFocus(false);
                
        // Dispatch a change event for text fields or textareas that have been edited
        RenderObject *r = static_cast<RenderObject*>(oldFocusNode.get()->renderer());
        if (r && (r->isTextArea() || r->isTextField()) && r->isEdited()) {
            EventTargetNodeCast(oldFocusNode.get())->dispatchHTMLEvent(changeEvent, true, false);
            if ((r = static_cast<RenderObject*>(oldFocusNode.get()->renderer())))
                r->setEdited(false);
        }

        // Dispatch the blur event and let the node do any other blur related activities (important for text fields)
        EventTargetNodeCast(oldFocusNode.get())->dispatchBlurEvent();

        if (m_focusNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusNode = 0;
        }
        clearSelectionIfNeeded(newFocusNode.get());
        EventTargetNodeCast(oldFocusNode.get())->dispatchUIEvent(DOMFocusOutEvent);
        if (m_focusNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusNode = 0;
        }
        clearSelectionIfNeeded(newFocusNode.get());
        if ((oldFocusNode.get() == this) && oldFocusNode->hasOneRef())
            return true;
            
        if (oldFocusNode.get() == oldFocusNode->rootEditableElement())
            didEndEditing();
    }

    if (newFocusNode) {
        if (newFocusNode == newFocusNode->rootEditableElement() && !acceptsEditingFocus(newFocusNode.get())) {
            // delegate blocks focus change
            focusChangeBlocked = true;
            goto SetFocusNodeDone;
        }
        // Set focus on the new node
        m_focusNode = newFocusNode.get();

        // Dispatch the focus event and let the node do any other focus related activities (important for text fields)
        EventTargetNodeCast(m_focusNode.get())->dispatchFocusEvent();

        if (m_focusNode != newFocusNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusNodeDone;
        }
        EventTargetNodeCast(m_focusNode.get())->dispatchUIEvent(DOMFocusInEvent);
        if (m_focusNode != newFocusNode) { 
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusNodeDone;
        }
        m_focusNode->setFocus();
        
        if (m_focusNode.get() == m_focusNode->rootEditableElement())
            didBeginEditing();
        
        // eww, I suck. set the qt focus correctly
        // ### find a better place in the code for this
        if (view()) {
            Widget *focusWidget = widgetForNode(m_focusNode.get());
            if (focusWidget) {
                // Make sure a widget has the right size before giving it focus.
                // Otherwise, we are testing edge cases of the Widget code.
                // Specifically, in WebCore this does not work well for text fields.
                updateLayout();
                // Re-get the widget in case updating the layout changed things.
                focusWidget = widgetForNode(m_focusNode.get());
            }
            if (focusWidget)
                focusWidget->setFocus();
            else
                view()->setFocus();
        }
   }

#if __APPLE__
    if (!focusChangeBlocked && m_focusNode && AccessibilityObjectCache::accessibilityEnabled())
        getAccObjectCache()->handleFocusedUIElementChanged();
#endif

SetFocusNodeDone:
    updateRendering();
    return !focusChangeBlocked;
}

void Document::clearSelectionIfNeeded(Node *newFocusNode)
{
    if (!frame())
        return;

    // Clear the selection when changing the focus node to null or to a node that is not 
    // contained by the current selection.
    Node *startContainer = frame()->selection().start().node();
    if (!newFocusNode || (startContainer && startContainer != newFocusNode && !(startContainer->isAncestor(newFocusNode)) && startContainer->shadowAncestorNode() != newFocusNode))
        // FIXME: 6498 Should just be able to call m_frame->selection().clear()
        frame()->setSelection(SelectionController());
}

void Document::setCSSTarget(Node* n)
{
    if (m_cssTarget)
        m_cssTarget->setChanged();
    m_cssTarget = n;
    if (n)
        n->setChanged();
}

Node* Document::getCSSTarget() const
{
    return m_cssTarget;
}

void Document::attachNodeIterator(NodeIterator *ni)
{
    m_nodeIterators.append(ni);
}

void Document::detachNodeIterator(NodeIterator *ni)
{
    m_nodeIterators.remove(ni);
}

void Document::notifyBeforeNodeRemoval(Node *n)
{
    if (Frame* f = frame()) {
        f->selection().nodeWillBeRemoved(n);
        f->dragCaret().nodeWillBeRemoved(n);
    }
    DeprecatedPtrListIterator<NodeIterator> it(m_nodeIterators);
    for (; it.current(); ++it)
        it.current()->notifyBeforeNodeRemoval(n);
}

DOMWindow* Document::defaultView() const
{
    if (!frame())
        return 0;
    
    return frame()->domWindow();
}

PassRefPtr<Event> Document::createEvent(const String &eventType, ExceptionCode& ec)
{
    if (eventType == "UIEvents" || eventType == "UIEvent")
        return new UIEvent();
    if (eventType == "MouseEvents" || eventType == "MouseEvent")
        return new MouseEvent();
    if (eventType == "MutationEvents" || eventType == "MutationEvent")
        return new MutationEvent();
    if (eventType == "KeyboardEvents" || eventType == "KeyboardEvent")
        return new KeyboardEvent();
    if (eventType == "HTMLEvents" || eventType == "Event" || eventType == "Events")
        return new Event();
#if SVG_SUPPORT
    if (eventType == "SVGEvents")
        return new Event();
    if (eventType == "SVGZoomEvents")
        return new WebCore::SVGZoomEvent();
#endif
    ec = NOT_SUPPORTED_ERR;
    return 0;
}

CSSStyleDeclaration *Document::getOverrideStyle(Element */*elt*/, const String &/*pseudoElt*/)
{
    return 0; // ###
}

void Document::handleWindowEvent(Event *evt, bool useCapture)
{
    if (m_windowEventListeners.isEmpty())
        return;
        
    // if any html event listeners are registered on the window, then dispatch them here
    DeprecatedPtrList<RegisteredEventListener> listenersCopy = m_windowEventListeners;
    DeprecatedPtrListIterator<RegisteredEventListener> it(listenersCopy);
    for (; it.current(); ++it)
        if (it.current()->eventType() == evt->type() && it.current()->useCapture() == useCapture)
            it.current()->listener()->handleEvent(evt, true);
}


void Document::defaultEventHandler(Event *evt)
{
    // handle accesskey
    if (evt->type() == keydownEvent) {
        KeyboardEvent* kevt = static_cast<KeyboardEvent *>(evt);
        if (kevt->ctrlKey()) {
            const PlatformKeyboardEvent* ev = kevt->keyEvent();
            String key = (ev ? ev->unmodifiedText() : kevt->keyIdentifier()).lower();
            Element* elem = getElementByAccessKey(key);
            if (elem) {
                elem->accessKeyAction(false);
                evt->setDefaultHandled();
            }
        }
    }
}

void Document::setHTMLWindowEventListener(const AtomicString &eventType, PassRefPtr<EventListener> listener)
{
    // If we already have it we don't want removeWindowEventListener to delete it
    removeHTMLWindowEventListener(eventType);
    if (listener)
        addWindowEventListener(eventType, listener, false);
}

EventListener *Document::getHTMLWindowEventListener(const AtomicString &eventType)
{
    DeprecatedPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it)
        if (it.current()->eventType() == eventType && it.current()->listener()->isHTMLEventListener())
            return it.current()->listener();
    return 0;
}

void Document::removeHTMLWindowEventListener(const AtomicString &eventType)
{
    DeprecatedPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it)
        if (it.current()->eventType() == eventType && it.current()->listener()->isHTMLEventListener()) {
            m_windowEventListeners.removeRef(it.current());
            return;
        }
}

void Document::addWindowEventListener(const AtomicString &eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    // Remove existing identical listener set with identical arguments.
    // The DOM 2 spec says that "duplicate instances are discarded" in this case.
    removeWindowEventListener(eventType, listener.get(), useCapture);
    m_windowEventListeners.append(new RegisteredEventListener(eventType, listener, useCapture));
}

void Document::removeWindowEventListener(const AtomicString &eventType, EventListener *listener, bool useCapture)
{
    RegisteredEventListener rl(eventType, listener, useCapture);
    DeprecatedPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it)
        if (*(it.current()) == rl) {
            m_windowEventListeners.removeRef(it.current());
            return;
        }
}

bool Document::hasWindowEventListener(const AtomicString &eventType)
{
    DeprecatedPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it) {
        if (it.current()->eventType() == eventType) {
            return true;
        }
    }

    return false;
}

PassRefPtr<EventListener> Document::createHTMLEventListener(const String& functionName, const String& code, Node *node)
{
    if (Frame* frm = frame())
        if (KJSProxy* proxy = frm->jScript())
            return proxy->createHTMLEventHandler(functionName, code, node);
    return 0;
}

void Document::setHTMLWindowEventListener(const AtomicString& eventType, Attribute* attr)
{
    setHTMLWindowEventListener(eventType,
        createHTMLEventListener(attr->localName().domString(), attr->value(), 0));
}

void Document::dispatchImageLoadEventSoon(HTMLImageLoader *image)
{
    m_imageLoadEventDispatchSoonList.append(image);
    if (!m_imageLoadEventTimer.isActive())
        m_imageLoadEventTimer.startOneShot(0);
}

void Document::removeImage(HTMLImageLoader* image)
{
    // Remove instances of this image from both lists.
    // Use loops because we allow multiple instances to get into the lists.
    while (m_imageLoadEventDispatchSoonList.removeRef(image)) { }
    while (m_imageLoadEventDispatchingList.removeRef(image)) { }
    if (m_imageLoadEventDispatchSoonList.isEmpty())
        m_imageLoadEventTimer.stop();
}

void Document::dispatchImageLoadEventsNow()
{
    // need to avoid re-entering this function; if new dispatches are
    // scheduled before the parent finishes processing the list, they
    // will set a timer and eventually be processed
    if (!m_imageLoadEventDispatchingList.isEmpty()) {
        return;
    }

    m_imageLoadEventTimer.stop();
    
    m_imageLoadEventDispatchingList = m_imageLoadEventDispatchSoonList;
    m_imageLoadEventDispatchSoonList.clear();
    for (DeprecatedPtrListIterator<HTMLImageLoader> it(m_imageLoadEventDispatchingList); it.current();) {
        HTMLImageLoader* image = it.current();
        // Must advance iterator *before* dispatching call.
        // Otherwise, it might be advanced automatically if dispatching the call had a side effect
        // of destroying the current HTMLImageLoader, and then we would advance past the *next* item,
        // missing one altogether.
        ++it;
        image->dispatchLoadEvent();
    }
    m_imageLoadEventDispatchingList.clear();
}

void Document::imageLoadEventTimerFired(Timer<Document>*)
{
    dispatchImageLoadEventsNow();
}

Element *Document::ownerElement() const
{
    if (!frame())
        return 0;
    return frame()->ownerElement();
}

String Document::referrer() const
{
    if (frame())
        return frame()->incomingReferrer();
    
    return String();
}

String Document::domain() const
{
    if (m_domain.isEmpty()) // not set yet (we set it on demand to save time and space)
        m_domain = KURL(URL()).host(); // Initially set to the host
    return m_domain;
}

void Document::setDomain(const String &newDomain, bool force /*=false*/)
{
    if (force) {
        m_domain = newDomain;
        return;
    }
    if (m_domain.isEmpty()) // not set yet (we set it on demand to save time and space)
        m_domain = KURL(URL()).host(); // Initially set to the host

    // Both NS and IE specify that changing the domain is only allowed when
    // the new domain is a suffix of the old domain.
    int oldLength = m_domain.length();
    int newLength = newDomain.length();
    if (newLength < oldLength) // e.g. newDomain=kde.org (7) and m_domain=www.kde.org (11)
    {
        String test = m_domain.copy();
        if (test[oldLength - newLength - 1] == '.') // Check that it's a subdomain, not e.g. "de.org"
        {
            test.remove(0, oldLength - newLength); // now test is "kde.org" from m_domain
            if (test == newDomain)                 // and we check that it's the same thing as newDomain
                m_domain = newDomain;
        }
    }
}

bool Document::isValidName(const String &name)
{
    const UChar *s = reinterpret_cast<const UChar *>(name.unicode());
    unsigned length = name.length();

    if (length == 0)
        return false;

    unsigned i = 0;

    UChar32 c;
    U16_NEXT(s, i, length, c)
    if (!isValidNameStart(c))
        return false;

    while (i < length) {
        U16_NEXT(s, i, length, c)
        if (!isValidNamePart(c))
            return false;
    }

    return true;
}

bool Document::parseQualifiedName(const String &qualifiedName, String &prefix, String &localName)
{
    unsigned length = qualifiedName.length();

    if (length == 0)
        return false;

    bool nameStart = true;
    bool sawColon = false;
    int colonPos = 0;

    const UChar* s = reinterpret_cast<const UChar*>(qualifiedName.unicode());
    for (unsigned i = 0; i < length;) {
        UChar32 c;
        U16_NEXT(s, i, length, c)
        if (c == ':') {
            if (sawColon)
                return false; // multiple colons: not allowed
            nameStart = true;
            sawColon = true;
            colonPos = i - 1;
        } else if (nameStart) {
            if (!isValidNameStart(c))
                return false;
            nameStart = false;
        } else {
            if (!isValidNamePart(c))
                return false;
        }
    }

    if (!sawColon) {
        prefix = String();
        localName = qualifiedName.copy();
    } else {
        prefix = qualifiedName.substring(0, colonPos);
        localName = qualifiedName.substring(colonPos + 1, length - (colonPos + 1));
    }

    return true;
}

void Document::addImageMap(HTMLMapElement* imageMap)
{
    // Add the image map, unless there's already another with that name.
    // "First map wins" is the rule other browsers seem to implement.
    m_imageMapsByName.add(imageMap->getName().impl(), imageMap);
}

void Document::removeImageMap(HTMLMapElement* imageMap)
{
    // Remove the image map by name.
    // But don't remove some other image map that just happens to have the same name.
    // FIXME: Use a HashCountedSet as we do for IDs to find the first remaining map
    // once a map has been removed.
    const AtomicString& name = imageMap->getName();
    ImageMapsByName::iterator it = m_imageMapsByName.find(name.impl());
    if (it != m_imageMapsByName.end() && it->second == imageMap)
        m_imageMapsByName.remove(it);
}

HTMLMapElement *Document::getImageMap(const String& URL) const
{
    if (URL.isNull())
        return 0;
    int hashPos = URL.find('#');
    AtomicString name = (hashPos < 0 ? URL : URL.substring(hashPos + 1)).impl();
    return m_imageMapsByName.get(name.impl());
}

void Document::setDecoder(Decoder *decoder)
{
    m_decoder = decoder;
}

QChar Document::backslashAsCurrencySymbol() const
{
    if (!m_decoder)
        return '\\';
    return m_decoder->encoding().backslashAsCurrencySymbol();
}

DeprecatedString Document::completeURL(const DeprecatedString &URL)
{
    if (!m_decoder)
        return KURL(baseURL(), URL).url();
    return KURL(baseURL(), URL, m_decoder->encoding()).url();
}

String Document::completeURL(const String &URL)
{
    if (URL.isNull())
        return URL;
    return completeURL(URL.deprecatedString());
}

bool Document::inPageCache()
{
    return m_inPageCache;
}

void Document::setInPageCache(bool flag)
{
    if (m_inPageCache == flag)
        return;

    m_inPageCache = flag;
    if (flag) {
        ASSERT(m_savedRenderer == 0);
        m_savedRenderer = renderer();
        if (m_view)
            m_view->resetScrollBars();
    } else {
        ASSERT(renderer() == 0 || renderer() == m_savedRenderer);
        ASSERT(m_renderArena);
        setRenderer(m_savedRenderer);
        m_savedRenderer = 0;
    }
}

void Document::passwordFieldAdded()
{
    m_passwordFields++;
}

void Document::passwordFieldRemoved()
{
    assert(m_passwordFields > 0);
    m_passwordFields--;
}

bool Document::hasPasswordField() const
{
    return m_passwordFields > 0;
}

void Document::secureFormAdded()
{
    m_secureForms++;
}

void Document::secureFormRemoved()
{
    assert(m_secureForms > 0);
    m_secureForms--;
}

bool Document::hasSecureForm() const
{
    return m_secureForms > 0;
}

void Document::setShouldCreateRenderers(bool f)
{
    m_createRenderers = f;
}

bool Document::shouldCreateRenderers()
{
    return m_createRenderers;
}

String Document::toString() const
{
    String result;

    for (Node *child = firstChild(); child != NULL; child = child->nextSibling()) {
        result += child->toString();
    }

    return result;
}

// Support for Javascript execCommand, and related methods

JSEditor *Document::jsEditor()
{
    if (!m_jsEditor)
        m_jsEditor = new JSEditor(this);
    return m_jsEditor;
}

bool Document::execCommand(const String &command, bool userInterface, const String &value)
{
    return jsEditor()->execCommand(command, userInterface, value);
}

bool Document::queryCommandEnabled(const String &command)
{
    return jsEditor()->queryCommandEnabled(command);
}

bool Document::queryCommandIndeterm(const String &command)
{
    return jsEditor()->queryCommandIndeterm(command);
}

bool Document::queryCommandState(const String &command)
{
    return jsEditor()->queryCommandState(command);
}

bool Document::queryCommandSupported(const String &command)
{
    return jsEditor()->queryCommandSupported(command);
}

String Document::queryCommandValue(const String &command)
{
    return jsEditor()->queryCommandValue(command);
}


void Document::addMarker(Range *range, DocumentMarker::MarkerType type)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        int exception = 0;
        DocumentMarker marker = {type, textPiece->startOffset(exception), textPiece->endOffset(exception)};
        addMarker(textPiece->startContainer(exception), marker);
    }
}

void Document::removeMarkers(Range *range, DocumentMarker::MarkerType markerType)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        int exception = 0;
        unsigned startOffset = textPiece->startOffset(exception);
        unsigned length = textPiece->endOffset(exception) - startOffset + 1;
        removeMarkers(textPiece->startContainer(exception), startOffset, length, markerType);
    }
}

// Markers are stored in order sorted by their location.  They do not overlap each other, as currently
// required by the drawing code in RenderText.cpp.

void Document::addMarker(Node *node, DocumentMarker newMarker) 
{
    assert(newMarker.endOffset >= newMarker.startOffset);
    if (newMarker.endOffset == newMarker.startOffset)
        return;
    
    DeprecatedValueList<DocumentMarker>* markers = m_markers.get(node);
    if (!markers) {
        markers = new DeprecatedValueList<DocumentMarker>;
        markers->append(newMarker);
        m_markers.set(node, markers);
    } else {
        DeprecatedValueListIterator<DocumentMarker> it;
        for (it = markers->begin(); it != markers->end();) {
            DocumentMarker marker = *it;
            
            if (newMarker.endOffset < marker.startOffset+1) {
                // This is the first marker that is completely after newMarker, and disjoint from it.
                // We found our insertion point.
                break;
            } else if (newMarker.startOffset > marker.endOffset) {
                // maker is before newMarker, and disjoint from it.  Keep scanning.
                it++;
            } else if (newMarker == marker) {
                // already have this one, NOP
                return;
            } else {
                // marker and newMarker intersect or touch - merge them into newMarker
                newMarker.startOffset = min(newMarker.startOffset, marker.startOffset);
                newMarker.endOffset = max(newMarker.endOffset, marker.endOffset);
                // remove old one, we'll add newMarker later
                it = markers->remove(it);
                // it points to the next marker to consider
            }
        }
        // at this point it points to the node before which we want to insert
        markers->insert(it, newMarker);
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
    
    DeprecatedValueList<DocumentMarker>* markers = m_markers.get(srcNode);
    if (!markers)
        return;

    bool docDirty = false;
    unsigned endOffset = startOffset + length - 1;
    DeprecatedValueListIterator<DocumentMarker> it;
    for (it = markers->begin(); it != markers->end(); ++it) {
        DocumentMarker marker = *it;

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

void Document::removeMarkers(Node *node, unsigned startOffset, int length, DocumentMarker::MarkerType markerType)
{
    if (length <= 0)
        return;

    DeprecatedValueList<DocumentMarker>* markers = m_markers.get(node);
    if (!markers)
        return;
    
    bool docDirty = false;
    unsigned endOffset = startOffset + length - 1;
    DeprecatedValueListIterator<DocumentMarker> it;
    for (it = markers->begin(); it != markers->end();) {
        DocumentMarker marker = *it;

        // markers are returned in order, so stop if we are now past the specified range
        if (marker.startOffset > endOffset)
            break;
        
        // skip marker that is wrong type or before target
        if (marker.endOffset < startOffset || (marker.type != markerType && markerType != DocumentMarker::AllMarkers)) {
            it++;
            continue;
        }

        // at this point we know that marker and target intersect in some way
        docDirty = true;

        // pitch the old marker
        it = markers->remove(it);
        // it now points to the next node
        
        // add either of the resulting slices that are left after removing target
        // NOTE: This adds to the list we are iterating!  That is OK regardless of
        // whether the iterator sees the new node, since the new node is a keeper.
        if (startOffset > marker.startOffset) {
            DocumentMarker newLeft = marker;
            newLeft.endOffset = startOffset;
            markers->insert(it, newLeft);
        }
        if (marker.endOffset > endOffset) {
            DocumentMarker newRight = marker;
            newRight.startOffset = endOffset;
            markers->insert(it, newRight);
        }
    }

    if (markers->isEmpty()) {
        m_markers.remove(node);
        delete markers;
    }

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

DeprecatedValueList<DocumentMarker> Document::markersForNode(Node* node)
{
    DeprecatedValueList<DocumentMarker>* markers = m_markers.get(node);
    if (markers)
        return *markers;
    return DeprecatedValueList<DocumentMarker>();
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
        Node* node = i->first;

        // inner loop: process each marker in the current node
        DeprecatedValueList<DocumentMarker> *markers = i->second;
        DeprecatedValueListIterator<DocumentMarker> markerIterator;
        for (markerIterator = markers->begin(); markerIterator != markers->end();) {
            DocumentMarker marker = *markerIterator;

            // skip nodes that are not of the specified type
            if (marker.type != markerType && markerType != DocumentMarker::AllMarkers) {
                ++markerIterator;
                continue;
            }

            // pitch the old marker
            markerIterator = markers->remove(markerIterator);
            // markerIterator now points to the next node
        }

        // delete the node's list if it is now empty
        if (markers->isEmpty())
            m_markers.remove(node);

        // cause the node to be redrawn
        if (RenderObject* renderer = node->renderer())
            renderer->repaint();
    }
}

void Document::shiftMarkers(Node *node, unsigned startOffset, int delta, DocumentMarker::MarkerType markerType)
{
    DeprecatedValueList<DocumentMarker>* markers = m_markers.get(node);
    if (!markers)
        return;

    bool docDirty = false;
    DeprecatedValueListIterator<DocumentMarker> it;
    for (it = markers->begin(); it != markers->end(); ++it) {
        DocumentMarker &marker = *it;
        if (marker.startOffset >= startOffset && (markerType == DocumentMarker::AllMarkers || marker.type == markerType)) {
            assert((int)marker.startOffset + delta >= 0);
            marker.startOffset += delta;
            marker.endOffset += delta;
            docDirty = true;
        }
    }
    
    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

#ifdef KHTML_XSLT

void Document::applyXSLTransform(ProcessingInstruction* pi)
{
    RefPtr<XSLTProcessor> processor = new XSLTProcessor;
    processor->setXSLStylesheet(static_cast<XSLStyleSheet*>(pi->sheet()));
    
    DeprecatedString resultMIMEType;
    DeprecatedString newSource;
    DeprecatedString resultEncoding;
    if (!processor->transformToString(this, resultMIMEType, newSource, resultEncoding))
        return;
    // FIXME: If the transform failed we should probably report an error (like Mozilla does).
    processor->createDocumentFromSource(newSource, resultEncoding, resultMIMEType, this, view());
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
    while ((element = doc->ownerElement()) != 0)
        doc = element->document();
    
    return doc;
}

PassRefPtr<Attr> Document::createAttributeNS(const String &namespaceURI, const String &qualifiedName, ExceptionCode& ec)
{
    if (qualifiedName.isNull()) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    String localName = qualifiedName;
    String prefix;
    int colonpos;
    if ((colonpos = qualifiedName.find(':')) >= 0) {
        prefix = qualifiedName.copy();
        localName = qualifiedName.copy();
        prefix.truncate(colonpos);
        localName.remove(0, colonpos+1);
    }

    if (!isValidName(localName)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }
    
    // FIXME: Assume this is a mapped attribute, since createAttribute isn't namespace-aware.  There's no harm to XML
    // documents if we're wrong.
    return new Attr(0, this, new MappedAttribute(QualifiedName(prefix.impl(), 
                                                                       localName.impl(),
                                                                       namespaceURI.impl()), String("").impl()));
}

#if SVG_SUPPORT
const SVGDocumentExtensions* Document::svgExtensions()
{
    return m_svgExtensions;
}

SVGDocumentExtensions* Document::accessSVGExtensions()
{
    if (!m_svgExtensions)
        m_svgExtensions = new SVGDocumentExtensions(this);
    return m_svgExtensions;
}
#endif

void Document::radioButtonChecked(HTMLInputElement *caller, HTMLFormElement *form)
{
    // Without a name, there is no group.
    if (caller->name().isEmpty())
        return;
    if (!form)
        form = defaultForm;
    // Uncheck the currently selected item
    NameToInputMap* formRadioButtons = m_selectedRadioButtons.get(form);
    if (!formRadioButtons) {
        formRadioButtons = new NameToInputMap;
        m_selectedRadioButtons.set(form, formRadioButtons);
    }
    
    HTMLInputElement* currentCheckedRadio = formRadioButtons->get(caller->name().impl());
    if (currentCheckedRadio && currentCheckedRadio != caller)
        currentCheckedRadio->setChecked(false);

    formRadioButtons->set(caller->name().impl(), caller);
}

HTMLInputElement* Document::checkedRadioButtonForGroup(AtomicStringImpl* name, HTMLFormElement *form)
{
    if (!form)
        form = defaultForm;
    NameToInputMap* formRadioButtons = m_selectedRadioButtons.get(form);
    if (!formRadioButtons)
        return 0;
    
    return formRadioButtons->get(name);
}

void Document::removeRadioButtonGroup(AtomicStringImpl* name, HTMLFormElement *form)
{
    if (!form)
        form = defaultForm;
    NameToInputMap* formRadioButtons = m_selectedRadioButtons.get(form);
    if (formRadioButtons) {
        formRadioButtons->remove(name);
        if (formRadioButtons->isEmpty()) {
            m_selectedRadioButtons.remove(form);
            delete formRadioButtons;
        }
    }
}

PassRefPtr<HTMLCollection> Document::images()
{
    return new HTMLCollection(this, HTMLCollection::DOC_IMAGES);
}

PassRefPtr<HTMLCollection> Document::applets()
{
    return new HTMLCollection(this, HTMLCollection::DOC_APPLETS);
}

PassRefPtr<HTMLCollection> Document::embeds()
{
    return new HTMLCollection(this, HTMLCollection::DOC_EMBEDS);
}

PassRefPtr<HTMLCollection> Document::objects()
{
    return new HTMLCollection(this, HTMLCollection::DOC_OBJECTS);
}

PassRefPtr<HTMLCollection> Document::links()
{
    return new HTMLCollection(this, HTMLCollection::DOC_LINKS);
}

PassRefPtr<HTMLCollection> Document::forms()
{
    return new HTMLCollection(this, HTMLCollection::DOC_FORMS);
}

PassRefPtr<HTMLCollection> Document::anchors()
{
    return new HTMLCollection(this, HTMLCollection::DOC_ANCHORS);
}

PassRefPtr<HTMLCollection> Document::all()
{
    return new HTMLCollection(this, HTMLCollection::DOC_ALL);
}

PassRefPtr<HTMLCollection> Document::windowNamedItems(const String &name)
{
    return new HTMLNameCollection(this, HTMLCollection::WINDOW_NAMED_ITEMS, name);
}

PassRefPtr<HTMLCollection> Document::documentNamedItems(const String &name)
{
    return new HTMLNameCollection(this, HTMLCollection::DOCUMENT_NAMED_ITEMS, name);
}

PassRefPtr<NameNodeList> Document::getElementsByName(const String &elementName)
{
    return new NameNodeList(this, elementName);
}

void Document::finishedParsing()
{
    setParsing(false);
    if (Frame* f = frame())
        f->finishedParsing();
}

Vector<String> Document::formElementsState() const
{
    Vector<String> stateVector(m_formElementsWithState.size() * 3);
    typedef HashSet<HTMLGenericFormElement*>::const_iterator Iterator;
    Iterator end = m_formElementsWithState.end();
    for (Iterator it = m_formElementsWithState.begin(); it != end; ++it) {
        HTMLGenericFormElement* e = *it;
        stateVector.append(e->name().domString());
        stateVector.append(e->type().domString());
        stateVector.append(e->stateValue());
    }
    return stateVector;
}

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
    if (name() && name() != HashTraits<AtomicStringImpl*>::deletedValue())
        name()->ref();
    if (type())
        type()->ref();
}

void FormElementKey::deref() const
{
    if (name() && name() != HashTraits<AtomicStringImpl*>::deletedValue())
        name()->deref();
    if (type())
        type()->deref();
}

unsigned FormElementKeyHash::hash(const FormElementKey& k)
{
    ASSERT(sizeof(k) % (sizeof(uint16_t) * 2) == 0);

    unsigned l = sizeof(k) / (sizeof(uint16_t) * 2);
    const uint16_t* s = reinterpret_cast<const uint16_t*>(&k);
    uint32_t hash = PHI;

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

FormElementKey FormElementKeyHashTraits::deletedValue()
{
    return HashTraits<AtomicStringImpl*>::deletedValue();
}

}
