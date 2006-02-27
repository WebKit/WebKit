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
#include "DocumentImpl.h"

#include "CDATASectionImpl.h"
#include "CommentImpl.h"
#include "DOMImplementationImpl.h"
#include "DocLoader.h"
#include "DocumentFragmentImpl.h"
#include "DocumentTypeImpl.h"
#include "EditingTextImpl.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLNameCollectionImpl.h"
#include "KWQAccObjectCache.h"
#include "KWQLogging.h"
#include "KeyEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "NameNodeListImpl.h"
#include "SegmentedString.h"
#include "SelectionController.h"
#include "SystemTime.h"
#include "VisiblePosition.h"
#include "css_stylesheetimpl.h"
#include "css_valueimpl.h"
#include "csshelper.h"
#include "cssstyleselector.h"
#include "cssvalues.h"
#include "dom2_events.h"
#include "dom2_eventsimpl.h"
#include "dom2_rangeimpl.h"
#include "dom2_viewsimpl.h"
#include "dom_exception.h"
#include "dom_xmlimpl.h"
#include "ecma/kjs_binding.h"
#include "ecma/kjs_proxy.h"
#include "helper.h"
#include "jsediting.h"
#include "khtml_settings.h"
#include "render_arena.h"
#include "render_canvas.h"
#include "render_frames.h"
#include "visible_text.h"
#include "xml_tokenizer.h"
#include <qregexp.h>

// FIXME: We want to cut the remaining HTML dependencies so that we don't need to include these files.
#include "HTMLInputElementImpl.h"
#include "html/html_baseimpl.h"
#include "html/html_documentimpl.h"
#include "html/html_headimpl.h"
#include "html/html_imageimpl.h"
#include "htmlfactory.h"
#include "htmlnames.h"

#ifdef KHTML_XSLT
#include "xsl_stylesheetimpl.h"
#include "xslt_processorimpl.h"
#endif

#ifndef KHTML_NO_XBL
#include "xbl/xbl_binding_manager.h"
using XBL::XBLBindingManager;
#endif

#if SVG_SUPPORT
#include "SVGNames.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementFactory.h"
#include "SVGZoomEventImpl.h"
#include "SVGStyleElementImpl.h"
#include "KSVGTimeScheduler.h"
#endif

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

// #define INSTRUMENT_LAYOUT_SCHEDULING 1

// This amount of time must have elapsed before we will even consider scheduling a layout without a delay.
// FIXME: For faster machines this value can really be lowered to 200.  250 is adequate, but a little high
// for dual G5s. :)
const int cLayoutScheduleThreshold = 250;

// Use 1 to represent the document's default form.
HTMLFormElementImpl* const defaultForm = (HTMLFormElementImpl*) 1;

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

QPtrList<DocumentImpl> * DocumentImpl::changedDocuments = 0;

// FrameView might be 0
DocumentImpl::DocumentImpl(DOMImplementationImpl* impl, FrameView *v)
    : ContainerNodeImpl(0)
    , m_implementation(impl)
    , m_domtree_version(0)
    , m_styleSheets(new StyleSheetListImpl)
    , m_title("")
    , m_titleSetExplicitly(false)
    , m_imageLoadEventTimer(this, &DocumentImpl::imageLoadEventTimerFired)
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
    document.resetSkippingRef(this);

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
    m_elementNames = 0;
    m_elementNameAlloc = 0;
    m_elementNameCount = 0;
    m_attrNames = 0;
    m_attrNameAlloc = 0;
    m_attrNameCount = 0;
    m_defaultView = new AbstractViewImpl(this);
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

void DocumentImpl::removedLastRef()
{
    if (m_selfOnlyRefCount) {
        // if removing a child removes the last self-only ref, we don't
        // want the document to be destructed until after
        // removeAllChildren returns, so we guard ourselves with an
        // extra self-only ref

        DocPtr<DocumentImpl> guard(this);

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

DocumentImpl::~DocumentImpl()
{
    assert(!renderer());
    assert(!m_inPageCache);
    assert(m_savedRenderer == 0);

#if SVG_SUPPORT
    delete m_svgExtensions;
#endif

    KJS::ScriptInterpreter::forgetAllDOMNodesForDocument(this);

    if (m_docChanged && changedDocuments)
        changedDocuments->remove(this);
    delete m_tokenizer;
    document.resetSkippingRef(0);
    delete m_styleSelector;
    delete m_docLoader;
    
    if (m_elementNames) {
        for (unsigned short id = 0; id < m_elementNameCount; id++)
            m_elementNames[id]->deref();
        delete [] m_elementNames;
    }
    if (m_attrNames) {
        for (unsigned short id = 0; id < m_attrNameCount; id++)
            m_attrNames[id]->deref();
        delete [] m_attrNames;
    }

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

    if (m_accCache){
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

void DocumentImpl::resetLinkColor()
{
    m_linkColor = Color(0, 0, 238);
}

void DocumentImpl::resetVisitedLinkColor()
{
    m_visitedLinkColor = Color(85, 26, 139);    
}

void DocumentImpl::resetActiveLinkColor()
{
    m_activeLinkColor.setNamedColor(QString("red"));
}

void DocumentImpl::setDocType(PassRefPtr<DocumentTypeImpl> docType)
{
    m_docType = docType;
}

DocumentTypeImpl *DocumentImpl::doctype() const
{
    return m_docType.get();
}

DOMImplementationImpl* DocumentImpl::implementation() const
{
    return m_implementation.get();
}

ElementImpl *DocumentImpl::documentElement() const
{
    NodeImpl *n = firstChild();
    while (n && n->nodeType() != Node::ELEMENT_NODE)
      n = n->nextSibling();
    return static_cast<ElementImpl*>(n);
}

PassRefPtr<ElementImpl> DocumentImpl::createElement(const DOMString &name, int &exceptionCode)
{
    return createElementNS(nullAtom, name, exceptionCode);
}

PassRefPtr<DocumentFragmentImpl> DocumentImpl::createDocumentFragment()
{
    return new DocumentFragmentImpl(getDocument());
}

PassRefPtr<TextImpl> DocumentImpl::createTextNode(const DOMString &data)
{
    return new TextImpl(this, data);
}

PassRefPtr<CommentImpl> DocumentImpl::createComment (const DOMString &data)
{
    return new CommentImpl(this, data);
}

PassRefPtr<CDATASectionImpl> DocumentImpl::createCDATASection(const DOMString &data, int &exception)
{
    if (isHTMLDocument()) {
        exception = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }
    return new CDATASectionImpl(this, data);
}

PassRefPtr<ProcessingInstructionImpl> DocumentImpl::createProcessingInstruction(const DOMString &target, const DOMString &data, int &exception)
{
    if (!isValidName(target)) {
        exception = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }
    if (isHTMLDocument()) {
        exception = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }
    return new ProcessingInstructionImpl(this, target, data);
}

PassRefPtr<EntityReferenceImpl> DocumentImpl::createEntityReference(const DOMString &name, int &exception)
{
    if (!isValidName(name)) {
        exception = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }
    if (isHTMLDocument()) {
        exception = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }
    return new EntityReferenceImpl(this, name.impl());
}

PassRefPtr<EditingTextImpl> DocumentImpl::createEditingTextNode(const DOMString &text)
{
    return new EditingTextImpl(this, text);
}

PassRefPtr<CSSStyleDeclarationImpl> DocumentImpl::createCSSStyleDeclaration()
{
    return new CSSMutableStyleDeclarationImpl;
}

PassRefPtr<NodeImpl> DocumentImpl::importNode(NodeImpl* importedNode, bool deep, int &exceptioncode)
{
    exceptioncode = 0;

    switch (importedNode->nodeType()) {
        case Node::TEXT_NODE:
            return createTextNode(importedNode->nodeValue());
        case Node::CDATA_SECTION_NODE:
            return createCDATASection(importedNode->nodeValue(), exceptioncode);
        case Node::ENTITY_REFERENCE_NODE:
            return createEntityReference(importedNode->nodeName(), exceptioncode);
        case Node::PROCESSING_INSTRUCTION_NODE:
            return createProcessingInstruction(importedNode->nodeName(), importedNode->nodeValue(), exceptioncode);
        case Node::COMMENT_NODE:
            return createComment(importedNode->nodeValue());
        case Node::ELEMENT_NODE: {
            ElementImpl *oldElement = static_cast<ElementImpl *>(importedNode);
            RefPtr<ElementImpl> newElement = createElementNS(oldElement->namespaceURI(), oldElement->tagName().toString(), exceptioncode);
                        
            if (exceptioncode != 0)
                return 0;

            NamedAttrMapImpl* attrs = oldElement->attributes(true);
            if (attrs) {
                unsigned length = attrs->length();
                for (unsigned i = 0; i < length; i++) {
                    AttributeImpl* attr = attrs->attributeItem(i);
                    newElement->setAttribute(attr->name(), attr->value().impl(), exceptioncode);
                    if (exceptioncode != 0)
                        return 0;
                }
            }

            newElement->copyNonAttributeProperties(oldElement);

            if (deep) {
                for (NodeImpl* oldChild = oldElement->firstChild(); oldChild; oldChild = oldChild->nextSibling()) {
                    RefPtr<NodeImpl> newChild = importNode(oldChild, true, exceptioncode);
                    if (exceptioncode != 0)
                        return 0;
                    newElement->appendChild(newChild.release(), exceptioncode);
                    if (exceptioncode != 0)
                        return 0;
                }
            }

            return newElement.release();
        }
    }

    exceptioncode = DOMException::NOT_SUPPORTED_ERR;
    return 0;
}


PassRefPtr<NodeImpl> DocumentImpl::adoptNode(PassRefPtr<NodeImpl> source, int &exceptioncode)
{
    if (!source)
        return 0;
    
    switch (source->nodeType()) {
        case Node::ENTITY_NODE:
        case Node::NOTATION_NODE:
            return 0;
        case Node::DOCUMENT_NODE:
        case Node::DOCUMENT_TYPE_NODE:
            exceptioncode = DOMException::NOT_SUPPORTED_ERR;
            return 0;            
        case Node::ATTRIBUTE_NODE: {                   
            AttrImpl* attr = static_cast<AttrImpl*>(source.get());
            if (attr->ownerElement())
                attr->ownerElement()->removeAttributeNode(attr, exceptioncode);
            attr->m_specified = true;
            break;
        }       
        default:
            if (source->parentNode())
                source->parentNode()->removeChild(source.get(), exceptioncode);
    }
                
    for (NodeImpl* node = source.get(); node; node = node->traverseNextNode(source.get())) {
        KJS::ScriptInterpreter::updateDOMNodeDocument(node, node->getDocument(), this);
        node->setDocument(this);
    }

    return source;
}

PassRefPtr<ElementImpl> DocumentImpl::createElementNS(const DOMString &_namespaceURI, const DOMString &qualifiedName, int &exceptioncode)
{
    // FIXME: We'd like a faster code path that skips this check for calls from inside the engine where the name is known to be valid.
    DOMString prefix, localName;
    if (!parseQualifiedName(qualifiedName, prefix, localName)) {
        exceptioncode = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }

    RefPtr<ElementImpl> e;
    QualifiedName qName = QualifiedName(AtomicString(prefix), AtomicString(localName), AtomicString(_namespaceURI));
    
    // FIXME: Use registered namespaces and look up in a hash to find the right factory.
    if (_namespaceURI == xhtmlNamespaceURI) {
        e = HTMLElementFactory::createHTMLElement(qName.localName(), this, 0, false);
        if (e && !prefix.isNull()) {
            e->setPrefix(qName.prefix(), exceptioncode);
            if (exceptioncode)
                return 0;
        }
    }
#if SVG_SUPPORT
    else if (_namespaceURI == KSVG::SVGNames::svgNamespaceURI)
        e = KSVG::SVGElementFactory::createSVGElement(qName, this, false);
#endif
    
    if (!e)
        e = new ElementImpl(qName, getDocument());
    
    return e.release();
}

ElementImpl *DocumentImpl::getElementById(const AtomicString& elementId) const
{
    if (elementId.length() == 0)
        return 0;

    ElementImpl *element = m_elementsById.get(elementId.impl());
    if (element)
        return element;
        
    if (m_duplicateIds.contains(elementId.impl())) {
        for (NodeImpl *n = traverseNextNode(); n != 0; n = n->traverseNextNode()) {
            if (n->isElementNode()) {
                element = static_cast<ElementImpl*>(n);
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

ElementImpl* DocumentImpl::elementFromPoint(int x, int y) const
{
    if (!renderer())
        return 0;

    RenderObject::NodeInfo nodeInfo(true, true);
    renderer()->layer()->hitTest(nodeInfo, x, y); 

    NodeImpl* n = nodeInfo.innerNode();
    while (n && !n->isElementNode())
        n = n->parentNode();
    return static_cast<ElementImpl*>(n);
}

void DocumentImpl::addElementById(const AtomicString& elementId, ElementImpl* element)
{
    if (!m_elementsById.contains(elementId.impl()))
        m_elementsById.set(elementId.impl(), element);
    else
        m_duplicateIds.add(elementId.impl());
}

void DocumentImpl::removeElementById(const AtomicString& elementId, ElementImpl* element)
{
    if (m_elementsById.get(elementId.impl()) == element)
        m_elementsById.remove(elementId.impl());
    else
        m_duplicateIds.remove(elementId.impl());
}

ElementImpl* DocumentImpl::getElementByAccessKey(const DOMString& key)
{
    if (!key.length())
        return 0;
    if (!m_accessKeyMapValid) {
        for (NodeImpl* n = this; n; n = n->traverseNextNode()) {
            if (!n->isElementNode())
                continue;
            ElementImpl* element = static_cast<ElementImpl *>(n);
            const AtomicString& accessKey = element->getAttribute(accesskeyAttr);
            if (!accessKey.isEmpty())
                m_elementsByAccessKey.set(accessKey.impl(), element);
        }
        m_accessKeyMapValid = true;
    }
    return m_elementsByAccessKey.get(key.impl());
}

void DocumentImpl::updateTitle()
{
    Frame *p = frame();
    if (!p)
        return;

    p->setTitle(m_title);
}

void DocumentImpl::setTitle(const String& title, NodeImpl* titleElement)
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

void DocumentImpl::removeTitle(NodeImpl *titleElement)
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

DOMString DocumentImpl::nodeName() const
{
    return "#document";
}

unsigned short DocumentImpl::nodeType() const
{
    return Node::DOCUMENT_NODE;
}

QString DocumentImpl::nextState()
{
   QString state;
   if (!m_state.isEmpty())
   {
      state = m_state.first();
      m_state.remove(m_state.begin());
   }
   return state;
}

QStringList DocumentImpl::docState()
{
    QStringList s;
    for (QPtrListIterator<NodeImpl> it(m_maintainsState); it.current(); ++it)
        s.append(it.current()->state());

    return s;
}

Frame *DocumentImpl::frame() const 
{
    return m_view ? m_view->frame() : 0; 
}

PassRefPtr<RangeImpl> DocumentImpl::createRange()
{
    return new RangeImpl(this);
}

PassRefPtr<NodeIteratorImpl> DocumentImpl::createNodeIterator(NodeImpl* root, unsigned whatToShow, 
    PassRefPtr<NodeFilterImpl> filter, bool expandEntityReferences, int& exceptioncode)
{
    if (!root) {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }
    return new NodeIteratorImpl(root, whatToShow, filter, expandEntityReferences);
}

PassRefPtr<TreeWalkerImpl> DocumentImpl::createTreeWalker(NodeImpl *root, unsigned whatToShow, 
    PassRefPtr<NodeFilterImpl> filter, bool expandEntityReferences, int &exceptioncode)
{
    if (!root) {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }
    return new TreeWalkerImpl(root, whatToShow, filter, expandEntityReferences);
}

void DocumentImpl::setDocumentChanged(bool b)
{
    if (b) {
        if (!m_docChanged) {
            if (!changedDocuments)
                changedDocuments = new QPtrList<DocumentImpl>;
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

void DocumentImpl::recalcStyle(StyleChange change)
{
    if (m_inStyleRecalc)
        return; // Guard against re-entrancy. -dwh
        
    m_inStyleRecalc = true;
    
    if (!renderer())
        goto bail_out;

    if (change == Force) {
        RenderStyle* oldStyle = renderer()->style();
        if (oldStyle) oldStyle->ref();
        RenderStyle* _style = new (m_renderArena) RenderStyle();
        _style->ref();
        _style->setDisplay(BLOCK);
        _style->setVisuallyOrdered(visuallyOrdered);
        // ### make the font stuff _really_ work!!!!

        FontDescription fontDescription;
        QFont f;
        fontDescription.setFamily(*(f.firstFamily()));
        fontDescription.setItalic(f.italic());
        fontDescription.setWeight(f.weight());
        fontDescription.setUsePrinterFont(printing());
        if (m_view) {
            const KHTMLSettings *settings = m_view->frame()->settings();
            if (printing() && !settings->shouldPrintBackgrounds())
                _style->setForceBackgroundsToWhite(true);
            QString stdfont = settings->stdFontName();
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

    for (NodeImpl* n = fastFirstChild(); n; n = n->nextSibling())
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

void DocumentImpl::updateRendering()
{
    if (hasChangedChild())
        recalcStyle(NoChange);
}

void DocumentImpl::updateDocumentsRendering()
{
    if (!changedDocuments)
        return;

    while (DocumentImpl* doc = changedDocuments->take()) {
        doc->m_docChanged = false;
        doc->updateRendering();
    }
}

void DocumentImpl::updateLayout()
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
void DocumentImpl::updateLayoutIgnorePendingStylesheets()
{
    bool oldIgnore = m_ignorePendingStylesheets;
    
    if (!haveStylesheetsLoaded()) {
        m_ignorePendingStylesheets = true;
        updateStyleSelector();    
    }

    updateLayout();

    m_ignorePendingStylesheets = oldIgnore;
}

void DocumentImpl::attach()
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

    ContainerNodeImpl::attach();

    setRenderer(render);
}

void DocumentImpl::restoreRenderer(RenderObject* render)
{
    setRenderer(render);
}

void DocumentImpl::detach()
{
    RenderObject* render = renderer();

    // indicate destruction mode,  i.e. attached() but renderer == 0
    setRenderer(0);
    
    if (m_inPageCache) {
#if __APPLE__
        if (render)
            getAccObjectCache()->detach(render);
#endif
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

    ContainerNodeImpl::detach();

    if (render)
        render->destroy();

    m_view = 0;
    
    if (m_renderArena) {
        delete m_renderArena;
        m_renderArena = 0;
    }
}

void DocumentImpl::removeAllEventListenersFromAllNodes()
{
    m_windowEventListeners.clear();
    removeAllDisconnectedNodeEventListeners();
    for (NodeImpl *n = this; n; n = n->traverseNextNode()) {
        n->removeAllEventListeners();
    }
}

void DocumentImpl::registerDisconnectedNodeWithEventListeners(NodeImpl* node)
{
    m_disconnectedNodesWithEventListeners.add(node);
}

void DocumentImpl::unregisterDisconnectedNodeWithEventListeners(NodeImpl* node)
{
    m_disconnectedNodesWithEventListeners.remove(node);
}

void DocumentImpl::removeAllDisconnectedNodeEventListeners()
{
    NodeSet::iterator end = m_disconnectedNodesWithEventListeners.end();
    for (NodeSet::iterator i = m_disconnectedNodesWithEventListeners.begin(); i != end; ++i)
        (*i)->removeAllEventListeners();
    m_disconnectedNodesWithEventListeners.clear();
}

KWQAccObjectCache* DocumentImpl::getAccObjectCache()
{
#if __APPLE__
    // The only document that actually has a KWQAccObjectCache is the top-level
    // document.  This is because we need to be able to get from any KWQAccObject
    // to any other KWQAccObject on the same page.  Using a single cache allows
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
    
    // look for top-level document
    ElementImpl *element = ownerElement();
    if (element) {
        DocumentImpl *doc;
        while (element) {
            doc = element->getDocument();
            element = doc->ownerElement();
        }
        
        // ask the top-level document for its cache
        return doc->getAccObjectCache();
    }
    
    // this is the top-level document, so install a new cache
    m_accCache = new KWQAccObjectCache;
#endif
    return m_accCache;
}

void DocumentImpl::setVisuallyOrdered()
{
    visuallyOrdered = true;
    if (renderer())
        renderer()->style()->setVisuallyOrdered(true);
}

void DocumentImpl::updateSelection()
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
    // send the AXSelectedTextChanged notification only if the new selection is non-null,
    // because null selections are only transitory (e.g. when starting an EditCommand, currently)
    if (KWQAccObjectCache::accessibilityEnabled() && s.start().isNotNull() && s.end().isNotNull()) {
        getAccObjectCache()->postNotificationToTopWebArea(renderer(), "AXSelectedTextChanged");
    }
#endif
}

Tokenizer *DocumentImpl::createTokenizer()
{
    return newXMLTokenizer(this, m_view);
}

void DocumentImpl::open()
{
    if (parsing())
        return;

    implicitOpen();

    if (frame())
        frame()->didExplicitOpen();

    // This is work that we should probably do in clear(), but we can't have it
    // happen when implicitOpen() is called unless we reorganize Frame code.
    setURL(QString());
    if (DocumentImpl *parent = parentDocument())
        setBaseURL(parent->baseURL());
}

void DocumentImpl::cancelParsing()
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

void DocumentImpl::implicitOpen()
{
    cancelParsing();

    clear();
    m_tokenizer = createTokenizer();
    setParsing(true);
}

HTMLElementImpl* DocumentImpl::body()
{
    NodeImpl *de = documentElement();
    if (!de)
        return 0;
    
    // try to prefer a FRAMESET element over BODY
    NodeImpl* body = 0;
    for (NodeImpl* i = de->firstChild(); i; i = i->nextSibling()) {
        if (i->hasTagName(framesetTag))
            return static_cast<HTMLElementImpl*>(i);
        
        if (i->hasTagName(bodyTag))
            body = i;
    }
    return static_cast<HTMLElementImpl *>(body);
}

void DocumentImpl::close()
{
    if (frame())
        frame()->endIfNotLoading();
    implicitClose();
}

void DocumentImpl::implicitClose()
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
    HTMLElementImpl *body = this->body();
    if (!body && isHTMLDocument()) {
        NodeImpl *de = documentElement();
        if (de) {
            body = new HTMLBodyElementImpl(this);
            int exceptionCode = 0;
            de->appendChild(body, exceptionCode);
            if (exceptionCode != 0)
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
    if (renderer() && KWQAccObjectCache::accessibilityEnabled())
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

void DocumentImpl::setParsing(bool b)
{
    m_bParsing = b;
    if (!m_bParsing && view())
        view()->scheduleRelayout();

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement() && !m_bParsing)
        printf("Parsing finished at %d\n", elapsedTime());
#endif
}

bool DocumentImpl::shouldScheduleLayout()
{
    // We can update layout if:
    // (a) we actually need a layout
    // (b) our stylesheets are all loaded
    // (c) we have a <body>
    return (renderer() && renderer()->needsLayout() && haveStylesheetsLoaded() &&
            documentElement() && documentElement()->renderer() &&
            (!documentElement()->hasTagName(htmlTag) || body()));
}

int DocumentImpl::minimumLayoutDelay()
{
    if (m_overMinimumLayoutThreshold)
        return 0;
    
    int elapsed = elapsedTime();
    m_overMinimumLayoutThreshold = elapsed > cLayoutScheduleThreshold;
    
    // We'll want to schedule the timer to fire at the minimum layout threshold.
    return kMax(0, cLayoutScheduleThreshold - elapsed);
}

int DocumentImpl::elapsedTime() const
{
    return static_cast<int>((currentTime() - m_startTime) * 1000);
}

void DocumentImpl::write(const DOMString &text)
{
    write(text.qstring());
}

void DocumentImpl::write(const QString &text)
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Beginning a document.write at %d\n", elapsedTime());
#endif
    
    if (!m_tokenizer) {
        open();
        assert(m_tokenizer);
        write(QString("<html>"));
    }
    m_tokenizer->write(text, false);
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Ending a document.write at %d\n", elapsedTime());
#endif    
}

void DocumentImpl::writeln(const DOMString &text)
{
    write(text);
    write(DOMString("\n"));
}

void DocumentImpl::finishParsing()
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

void DocumentImpl::clear()
{
    delete m_tokenizer;
    m_tokenizer = 0;

    removeChildren();
    QPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current();)
        m_windowEventListeners.removeRef(it.current());
}

void DocumentImpl::setURL(const QString& url)
{
    m_url = url;
    if (m_styleSelector)
        m_styleSelector->setEncodedURL(m_url);
}

void DocumentImpl::setStyleSheet(const DOMString &url, const DOMString &sheet)
{
    m_sheet = new CSSStyleSheetImpl(this, url);
    m_sheet->parseString(sheet);
    m_loadingSheet = false;

    updateStyleSelector();
}

void DocumentImpl::setUserStyleSheet(const QString& sheet)
{
    if (m_usersheet != sheet) {
        m_usersheet = sheet;
        updateStyleSelector();
    }
}

CSSStyleSheetImpl* DocumentImpl::elementSheet()
{
    if (!m_elemSheet)
        m_elemSheet = new CSSStyleSheetImpl(this, baseURL());
    return m_elemSheet.get();
}

void DocumentImpl::determineParseMode(const QString &/*str*/)
{
    // For XML documents use strict parse mode.  HTML docs will override this method to
    // determine their parse mode.
    pMode = Strict;
    hMode = XHtml;
}

NodeImpl *DocumentImpl::nextFocusNode(NodeImpl *fromNode)
{
    unsigned short fromTabIndex;

    if (!fromNode) {
        // No starting node supplied; begin with the top of the document
        NodeImpl *n;

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
        NodeImpl *n = fromNode->traverseNextNode();
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
        NodeImpl *n;

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
            NodeImpl *n = this;
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

NodeImpl *DocumentImpl::previousFocusNode(NodeImpl *fromNode)
{
    NodeImpl *lastNode = this;
    while (lastNode->lastChild())
        lastNode = lastNode->lastChild();

    if (!fromNode) {
        // No starting node supplied; begin with the very last node in the document
        NodeImpl *n;

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
            NodeImpl *n = fromNode->traversePreviousNode();
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
            NodeImpl *n;

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

int DocumentImpl::nodeAbsIndex(NodeImpl *node)
{
    assert(node->getDocument() == this);

    int absIndex = 0;
    for (NodeImpl *n = node; n && n != this; n = n->traversePreviousNode())
        absIndex++;
    return absIndex;
}

NodeImpl *DocumentImpl::nodeWithAbsIndex(int absIndex)
{
    NodeImpl *n = this;
    for (int i = 0; n && (i < absIndex); i++) {
        n = n->traverseNextNode();
    }
    return n;
}

void DocumentImpl::processHttpEquiv(const DOMString &equiv, const DOMString &content)
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
    }
    else if (equalIgnoringCase(equiv, "refresh") && frame->metaRefreshEnabled())
    {
        // get delay and url
        QString str = content.qstring().stripWhiteSpace();
        int pos = str.find(QRegExp("[;,]"));
        if (pos == -1)
            pos = str.find(QRegExp("[ \t]"));

        if (pos == -1) // There can be no url (David)
        {
            bool ok = false;
            int delay = 0;
            delay = str.toInt(&ok);
            // We want a new history item if the refresh timeout > 1 second
            if(ok && frame) frame->scheduleRedirection(delay, frame->url().url(), delay <= 1);
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
            str = parseURL(DOMString(str)).qstring();
            if (ok && frame)
                // We want a new history item if the refresh timeout > 1 second
                frame->scheduleRedirection(delay, completeURL(str), delay <= 1);
        }
    }
    else if (equalIgnoringCase(equiv, "expires"))
    {
        QString str = content.qstring().stripWhiteSpace();
        time_t expire_date = str.toInt();
        if (m_docLoader)
            m_docLoader->setExpireDate(expire_date);
    }
    else if ((equalIgnoringCase(equiv, "pragma") || equalIgnoringCase(equiv, "cache-control")) && frame)
    {
        QString str = content.qstring().lower().stripWhiteSpace();
        KURL url = frame->url();
    }
    else if (equalIgnoringCase(equiv, "set-cookie"))
    {
        // ### make setCookie work on XML documents too; e.g. in case of <html:meta .....>
        if (isHTMLDocument())
            static_cast<HTMLDocumentImpl *>(this)->setCookie(content);
    }
}

MouseEventWithHitTestResults DocumentImpl::prepareMouseEvent(bool readonly, bool active, bool mouseMove,
    int x, int y, MouseEvent* event)
{
    if (!renderer())
        return MouseEventWithHitTestResults();

    assert(renderer()->isCanvas());
    RenderObject::NodeInfo renderInfo(readonly, active, mouseMove);
    renderer()->layer()->hitTest(renderInfo, x, y);

    String href;
    String target;
    if (renderInfo.URLElement()) {
        assert(renderInfo.URLElement()->isElementNode());
        ElementImpl* e = static_cast<ElementImpl*>(renderInfo.URLElement());
        href = parseURL(e->getAttribute(hrefAttr));
        if (!href.isNull())
            target = e->getAttribute(targetAttr);
    }

    if (!readonly)
        updateRendering();

    return MouseEventWithHitTestResults(event, href, target, renderInfo.innerNode());
}

// DOM Section 1.1.1
bool DocumentImpl::childAllowed(NodeImpl *newChild)
{
    // Documents may contain a maximum of one Element child
    if (newChild->nodeType() == Node::ELEMENT_NODE) {
        NodeImpl *c;
        for (c = firstChild(); c; c = c->nextSibling()) {
            if (c->nodeType() == Node::ELEMENT_NODE)
                return false;
        }
    }

    // Documents may contain a maximum of one DocumentType child
    if (newChild->nodeType() == Node::DOCUMENT_TYPE_NODE) {
        NodeImpl *c;
        for (c = firstChild(); c; c = c->nextSibling()) {
            if (c->nodeType() == Node::DOCUMENT_TYPE_NODE)
                return false;
        }
    }

    return childTypeAllowed(newChild->nodeType());
}

bool DocumentImpl::childTypeAllowed(unsigned short type)
{
    switch (type) {
        case Node::ELEMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::COMMENT_NODE:
        case Node::DOCUMENT_TYPE_NODE:
            return true;
        default:
            return false;
    }
}

PassRefPtr<NodeImpl> DocumentImpl::cloneNode(bool /*deep*/)
{
    // Spec says cloning Document nodes is "implementation dependent"
    // so we do not support it...
    return 0;
}

StyleSheetListImpl* DocumentImpl::styleSheets()
{
    return m_styleSheets.get();
}

DOMString DocumentImpl::preferredStylesheetSet()
{
  return m_preferredStylesheetSet;
}

DOMString DocumentImpl::selectedStylesheetSet()
{
  return m_selectedStylesheetSet;
}

void DocumentImpl::setSelectedStylesheetSet(const DOMString& aString)
{
  m_selectedStylesheetSet = aString;
  updateStyleSelector();
  if (renderer())
    renderer()->repaint();
}

// This method is called whenever a top-level stylesheet has finished loading.
void DocumentImpl::stylesheetLoaded()
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

void DocumentImpl::updateStyleSelector()
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


QStringList DocumentImpl::availableStyleSheets() const
{
    return m_availableSheets;
}

void DocumentImpl::recalcStyleSelector()
{
    if (!renderer() || !attached())
        return;

    QPtrList<StyleSheetImpl> oldStyleSheets = m_styleSheets->styleSheets;
    m_styleSheets->styleSheets.clear();
    m_availableSheets.clear();
    NodeImpl *n;
    for (n = this; n; n = n->traverseNextNode()) {
        StyleSheetImpl *sheet = 0;

        if (n->nodeType() == Node::PROCESSING_INSTRUCTION_NODE)
        {
            // Processing instruction (XML documents only)
            ProcessingInstructionImpl* pi = static_cast<ProcessingInstructionImpl*>(n);
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
                ElementImpl* elem = getElementById(pi->localHref().impl());
                if (elem) {
                    DOMString sheetText("");
                    NodeImpl *c;
                    for (c = elem->firstChild(); c; c = c->nextSibling()) {
                        if (c->nodeType() == Node::TEXT_NODE || c->nodeType() == Node::CDATA_SECTION_NODE)
                            sheetText += c->nodeValue();
                    }

                    CSSStyleSheetImpl *cssSheet = new CSSStyleSheetImpl(this);
                    cssSheet->parseString(sheetText);
                    pi->setStyleSheet(cssSheet);
                    sheet = cssSheet;
                }
            }

        }
        else if (n->isHTMLElement() && (n->hasTagName(linkTag) || n->hasTagName(styleTag))) {
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(n);
            QString title = e->getAttribute(titleAttr).qstring();
            bool enabledViaScript = false;
            if (e->hasLocalName(linkTag)) {
                // <LINK> element
                HTMLLinkElementImpl* l = static_cast<HTMLLinkElementImpl*>(n);
                if (l->isLoading() || l->isDisabled())
                    continue;
                if (!l->sheet())
                    title = QString::null;
                enabledViaScript = l->isEnabledViaScript();
            }

            // Get the current preferred styleset.  This is the
            // set of sheets that will be enabled.
            if (e->hasLocalName(linkTag))
                sheet = static_cast<HTMLLinkElementImpl*>(n)->sheet();
            else
                // <STYLE> element
                sheet = static_cast<HTMLStyleElementImpl*>(n)->sheet();

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
                    QString rel = e->getAttribute(relAttr).qstring();
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
        else if (n->isSVGElement() && n->hasTagName(KSVG::SVGNames::styleTag)) {
            QString title;
            // <STYLE> element
            KSVG::SVGStyleElementImpl *s = static_cast<KSVG::SVGStyleElementImpl*>(n);
            if (!s->isLoading()) {
                sheet = s->sheet();
                if(sheet)
                    title = s->getAttribute(KSVG::SVGNames::titleAttr).qstring();
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
    QPtrListIterator<StyleSheetImpl> it(oldStyleSheets);
    for (; it.current(); ++it)
        it.current()->deref();

    // Create a new style selector
    delete m_styleSelector;
    QString usersheet = m_usersheet;
    if (m_view && m_view->mediaType() == "print")
        usersheet += m_printSheet;
    m_styleSelector = new CSSStyleSelector(this, usersheet, m_styleSheets.get(), !inCompatMode());
    m_styleSelector->setEncodedURL(m_url);
    m_styleSelectorDirty = false;
}

void DocumentImpl::setHoverNode(PassRefPtr<NodeImpl> newHoverNode)
{
    m_hoverNode = newHoverNode;
}

void DocumentImpl::setActiveNode(PassRefPtr<NodeImpl> newActiveNode)
{
    m_activeNode = newActiveNode;
}

void DocumentImpl::hoveredNodeDetached(NodeImpl* node)
{
    if (!m_hoverNode || (node != m_hoverNode && (!m_hoverNode->isTextNode() || node != m_hoverNode->parent())))
        return;

    m_hoverNode = node->parent();
    while (m_hoverNode && !m_hoverNode->renderer())
        m_hoverNode = m_hoverNode->parent();
    if (view())
        view()->scheduleHoverStateUpdate();
}

void DocumentImpl::activeChainNodeDetached(NodeImpl* node)
{
    if (!m_activeNode || (node != m_activeNode && (!m_activeNode->isTextNode() || node != m_activeNode->parent())))
        return;

    m_activeNode = node->parent();
    while (m_activeNode && !m_activeNode->renderer())
        m_activeNode = m_activeNode->parent();
}

bool DocumentImpl::relinquishesEditingFocus(NodeImpl *node)
{
    assert(node);
    assert(node->isContentEditable());

    NodeImpl *root = node->rootEditableElement();
    if (!frame() || !root)
        return false;

    return frame()->shouldEndEditing(rangeOfContents(root).get());
}

bool DocumentImpl::acceptsEditingFocus(NodeImpl *node)
{
    assert(node);
    assert(node->isContentEditable());

    NodeImpl *root = node->rootEditableElement();
    if (!frame() || !root)
        return false;

    return frame()->shouldBeginEditing(rangeOfContents(root).get());
}

void DocumentImpl::didBeginEditing()
{
    if (!frame())
        return;
    
    frame()->didBeginEditing();
}

void DocumentImpl::didEndEditing()
{
    if (!frame())
        return;
    
    frame()->didEndEditing();
}

#if __APPLE__
const QValueList<DashboardRegionValue> & DocumentImpl::dashboardRegions() const
{
    return m_dashboardRegions;
}

void DocumentImpl::setDashboardRegions (const QValueList<DashboardRegionValue>& regions)
{
    m_dashboardRegions = regions;
    setDashboardRegionsDirty (false);
}
#endif

static Widget *widgetForNode(NodeImpl *focusNode)
{
    if (!focusNode)
        return 0;
    RenderObject *renderer = focusNode->renderer();
    if (!renderer || !renderer->isWidget())
        return 0;
    return static_cast<RenderWidget *>(renderer)->widget();
}

bool DocumentImpl::setFocusNode(PassRefPtr<NodeImpl> newFocusNode)
{    
    // Make sure newFocusNode is actually in this document
    if (newFocusNode && (newFocusNode->getDocument() != this))
        return true;

    if (m_focusNode == newFocusNode)
        return true;

    if (m_focusNode && m_focusNode.get() == m_focusNode->rootEditableElement() && !relinquishesEditingFocus(m_focusNode.get()))
        return false;
        
    bool focusChangeBlocked = false;
    RefPtr<NodeImpl> oldFocusNode = m_focusNode;
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
            oldFocusNode->dispatchHTMLEvent(changeEvent, true, false);
            if ((r = static_cast<RenderObject*>(oldFocusNode.get()->renderer())))
                r->setEdited(false);
        }

        oldFocusNode->dispatchHTMLEvent(blurEvent, false, false);

        if (m_focusNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusNode = 0;
        }
        clearSelectionIfNeeded(newFocusNode.get());
        oldFocusNode->dispatchUIEvent(DOMFocusOutEvent);
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
        m_focusNode->dispatchHTMLEvent(focusEvent, false, false);
        if (m_focusNode != newFocusNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusNodeDone;
        }
        m_focusNode->dispatchUIEvent(DOMFocusInEvent);
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
    if (!focusChangeBlocked && m_focusNode && KWQAccObjectCache::accessibilityEnabled())
        getAccObjectCache()->handleFocusedUIElementChanged();
#endif

SetFocusNodeDone:
    updateRendering();
    return !focusChangeBlocked;
}

void DocumentImpl::clearSelectionIfNeeded(NodeImpl *newFocusNode)
{
    if (!frame())
        return;

    // Clear the selection when changing the focus node to null or to a node that is not 
    // contained by the current selection.
    NodeImpl *startContainer = frame()->selection().start().node();
    if (!newFocusNode || (startContainer && startContainer != newFocusNode && !startContainer->isAncestor(newFocusNode)))
        // FIXME: 6498 Should just be able to call m_frame->selection().clear()
        frame()->setSelection(SelectionController());
}

void DocumentImpl::setCSSTarget(NodeImpl* n)
{
    if (m_cssTarget)
        m_cssTarget->setChanged();
    m_cssTarget = n;
    if (n)
        n->setChanged();
}

NodeImpl* DocumentImpl::getCSSTarget()
{
    return m_cssTarget;
}

void DocumentImpl::attachNodeIterator(NodeIteratorImpl *ni)
{
    m_nodeIterators.append(ni);
}

void DocumentImpl::detachNodeIterator(NodeIteratorImpl *ni)
{
    m_nodeIterators.remove(ni);
}

void DocumentImpl::notifyBeforeNodeRemoval(NodeImpl *n)
{
    QPtrListIterator<NodeIteratorImpl> it(m_nodeIterators);
    for (; it.current(); ++it)
        it.current()->notifyBeforeNodeRemoval(n);
}

AbstractViewImpl *DocumentImpl::defaultView() const
{
    return m_defaultView.get();
}

PassRefPtr<EventImpl> DocumentImpl::createEvent(const DOMString &eventType, int &exceptioncode)
{
    if (eventType == "UIEvents" || eventType == "UIEvent")
        return new UIEventImpl();
    if (eventType == "MouseEvents" || eventType == "MouseEvent")
        return new MouseEventImpl();
    if (eventType == "MutationEvents" || eventType == "MutationEvent")
        return new MutationEventImpl();
    if (eventType == "KeyboardEvents" || eventType == "KeyboardEvent")
        return new KeyboardEventImpl();
    if (eventType == "HTMLEvents" || eventType == "Event" || eventType == "Events")
        return new EventImpl();
#if SVG_SUPPORT
    if (eventType == "SVGEvents")
        return new EventImpl();
    if (eventType == "SVGZoomEvents")
        return new KSVG::SVGZoomEventImpl();
#endif
    exceptioncode = DOMException::NOT_SUPPORTED_ERR;
    return 0;
}

CSSStyleDeclarationImpl *DocumentImpl::getOverrideStyle(ElementImpl */*elt*/, const DOMString &/*pseudoElt*/)
{
    return 0; // ###
}

void DocumentImpl::handleWindowEvent(EventImpl *evt, bool useCapture)
{
    if (m_windowEventListeners.isEmpty())
        return;
        
    // if any html event listeners are registered on the window, then dispatch them here
    QPtrList<RegisteredEventListener> listenersCopy = m_windowEventListeners;
    QPtrListIterator<RegisteredEventListener> it(listenersCopy);
    for (; it.current(); ++it)
        if (it.current()->eventType() == evt->type() && it.current()->useCapture() == useCapture)
            it.current()->listener()->handleEventImpl(evt, true);
}


void DocumentImpl::defaultEventHandler(EventImpl *evt)
{
    // handle accesskey
    if (evt->type() == keydownEvent) {
        KeyboardEventImpl* kevt = static_cast<KeyboardEventImpl *>(evt);
        if (kevt->ctrlKey()) {
            KeyEvent* ev = kevt->keyEvent();
            String key = (ev ? ev->unmodifiedText() : kevt->keyIdentifier()).lower();
            ElementImpl* elem = getElementByAccessKey(key);
            if (elem) {
                elem->accessKeyAction(false);
                evt->setDefaultHandled();
            }
        }
    }
}

void DocumentImpl::setHTMLWindowEventListener(const AtomicString &eventType, PassRefPtr<EventListener> listener)
{
    // If we already have it we don't want removeWindowEventListener to delete it
    removeHTMLWindowEventListener(eventType);
    if (listener)
        addWindowEventListener(eventType, listener, false);
}

EventListener *DocumentImpl::getHTMLWindowEventListener(const AtomicString &eventType)
{
    QPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it)
        if (it.current()->eventType() == eventType && it.current()->listener()->eventListenerType() == "_khtml_HTMLEventListener")
            return it.current()->listener();
    return 0;
}

void DocumentImpl::removeHTMLWindowEventListener(const AtomicString &eventType)
{
    QPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it)
        if (it.current()->eventType() == eventType && it.current()->listener()->eventListenerType() == "_khtml_HTMLEventListener") {
            m_windowEventListeners.removeRef(it.current());
            return;
        }
}

void DocumentImpl::addWindowEventListener(const AtomicString &eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    // Remove existing identical listener set with identical arguments.
    // The DOM 2 spec says that "duplicate instances are discarded" in this case.
    removeWindowEventListener(eventType, listener.get(), useCapture);
    m_windowEventListeners.append(new RegisteredEventListener(eventType, listener, useCapture));
}

void DocumentImpl::removeWindowEventListener(const AtomicString &eventType, EventListener *listener, bool useCapture)
{
    RegisteredEventListener rl(eventType, listener, useCapture);
    QPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it)
        if (*(it.current()) == rl) {
            m_windowEventListeners.removeRef(it.current());
            return;
        }
}

bool DocumentImpl::hasWindowEventListener(const AtomicString &eventType)
{
    QPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it) {
        if (it.current()->eventType() == eventType) {
            return true;
        }
    }

    return false;
}

PassRefPtr<EventListener> DocumentImpl::createHTMLEventListener(const DOMString& code, NodeImpl *node)
{
    if (Frame *frm = frame()) {
        if (KJSProxyImpl *proxy = frm->jScript())
            return proxy->createHTMLEventHandler(code, node);
    }
    return 0;
}

void DocumentImpl::setHTMLWindowEventListener(const AtomicString& eventType, AttributeImpl* attr)
{
    setHTMLWindowEventListener(eventType, createHTMLEventListener(attr->value(), 0));
}

void DocumentImpl::dispatchImageLoadEventSoon(HTMLImageLoader *image)
{
    m_imageLoadEventDispatchSoonList.append(image);
    if (!m_imageLoadEventTimer.isActive())
        m_imageLoadEventTimer.startOneShot(0);
}

void DocumentImpl::removeImage(HTMLImageLoader* image)
{
    // Remove instances of this image from both lists.
    // Use loops because we allow multiple instances to get into the lists.
    while (m_imageLoadEventDispatchSoonList.removeRef(image)) { }
    while (m_imageLoadEventDispatchingList.removeRef(image)) { }
    if (m_imageLoadEventDispatchSoonList.isEmpty())
        m_imageLoadEventTimer.stop();
}

void DocumentImpl::dispatchImageLoadEventsNow()
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
    for (QPtrListIterator<HTMLImageLoader> it(m_imageLoadEventDispatchingList); it.current();) {
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

void DocumentImpl::imageLoadEventTimerFired(Timer<DocumentImpl>*)
{
    dispatchImageLoadEventsNow();
}

ElementImpl *DocumentImpl::ownerElement()
{
    if (!frame())
        return 0;
    return frame()->ownerElement();
}

DOMString DocumentImpl::referrer() const
{
    if (frame())
        return frame()->incomingReferrer();
    
    return DOMString();
}

DOMString DocumentImpl::domain() const
{
    if (m_domain.isEmpty()) // not set yet (we set it on demand to save time and space)
        m_domain = KURL(URL()).host(); // Initially set to the host
    return m_domain;
}

void DocumentImpl::setDomain(const DOMString &newDomain, bool force /*=false*/)
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
        DOMString test = m_domain.copy();
        if (test[oldLength - newLength - 1] == '.') // Check that it's a subdomain, not e.g. "de.org"
        {
            test.remove(0, oldLength - newLength); // now test is "kde.org" from m_domain
            if (test == newDomain)                 // and we check that it's the same thing as newDomain
                m_domain = newDomain;
        }
    }
}

bool DocumentImpl::isValidName(const DOMString &name)
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

bool DocumentImpl::parseQualifiedName(const DOMString &qualifiedName, DOMString &prefix, DOMString &localName)
{
    unsigned length = qualifiedName.length();

    if (length == 0)
        return false;

    bool nameStart = true;
    bool sawColon = false;
    int colonPos = 0;

    const QChar *s = qualifiedName.unicode();
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
        prefix = DOMString();
        localName = qualifiedName.copy();
    } else {
        prefix = qualifiedName.substring(0, colonPos);
        localName = qualifiedName.substring(colonPos + 1, length - (colonPos + 1));
    }

    return true;
}

void DocumentImpl::addImageMap(HTMLMapElementImpl* imageMap)
{
    // Add the image map, unless there's already another with that name.
    // "First map wins" is the rule other browsers seem to implement.
    m_imageMapsByName.add(imageMap->getName().impl(), imageMap);
}

void DocumentImpl::removeImageMap(HTMLMapElementImpl* imageMap)
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

HTMLMapElementImpl *DocumentImpl::getImageMap(const DOMString& URL) const
{
    if (URL.isNull())
        return 0;
    int hashPos = URL.find('#');
    AtomicString name = (hashPos < 0 ? URL : URL.substring(hashPos + 1)).impl();
    return m_imageMapsByName.get(name.impl());
}

void DocumentImpl::setDecoder(Decoder *decoder)
{
    m_decoder = decoder;
}

QString DocumentImpl::completeURL(const QString &URL)
{
    if (!m_decoder)
        return KURL(baseURL(), URL).url();
    return KURL(baseURL(), URL, m_decoder->encoding()).url();
}

DOMString DocumentImpl::completeURL(const DOMString &URL)
{
    if (URL.isNull())
        return URL;
    return completeURL(URL.qstring());
}

bool DocumentImpl::inPageCache()
{
    return m_inPageCache;
}

void DocumentImpl::setInPageCache(bool flag)
{
    if (m_inPageCache == flag)
        return;

    m_inPageCache = flag;
    if (flag) {
        assert(m_savedRenderer == 0);
        m_savedRenderer = renderer();
        if (m_view) {
            m_view->resetScrollBars();
        }
    } else {
        assert(renderer() == 0 || renderer() == m_savedRenderer);
        setRenderer(m_savedRenderer);
        m_savedRenderer = 0;
    }
}

void DocumentImpl::passwordFieldAdded()
{
    m_passwordFields++;
}

void DocumentImpl::passwordFieldRemoved()
{
    assert(m_passwordFields > 0);
    m_passwordFields--;
}

bool DocumentImpl::hasPasswordField() const
{
    return m_passwordFields > 0;
}

void DocumentImpl::secureFormAdded()
{
    m_secureForms++;
}

void DocumentImpl::secureFormRemoved()
{
    assert(m_secureForms > 0);
    m_secureForms--;
}

bool DocumentImpl::hasSecureForm() const
{
    return m_secureForms > 0;
}

void DocumentImpl::setShouldCreateRenderers(bool f)
{
    m_createRenderers = f;
}

bool DocumentImpl::shouldCreateRenderers()
{
    return m_createRenderers;
}

DOMString DocumentImpl::toString() const
{
    DOMString result;

    for (NodeImpl *child = firstChild(); child != NULL; child = child->nextSibling()) {
        result += child->toString();
    }

    return result;
}

// Support for Javascript execCommand, and related methods

JSEditor *DocumentImpl::jsEditor()
{
    if (!m_jsEditor)
        m_jsEditor = new JSEditor(this);
    return m_jsEditor;
}

bool DocumentImpl::execCommand(const DOMString &command, bool userInterface, const DOMString &value)
{
    return jsEditor()->execCommand(command, userInterface, value);
}

bool DocumentImpl::queryCommandEnabled(const DOMString &command)
{
    return jsEditor()->queryCommandEnabled(command);
}

bool DocumentImpl::queryCommandIndeterm(const DOMString &command)
{
    return jsEditor()->queryCommandIndeterm(command);
}

bool DocumentImpl::queryCommandState(const DOMString &command)
{
    return jsEditor()->queryCommandState(command);
}

bool DocumentImpl::queryCommandSupported(const DOMString &command)
{
    return jsEditor()->queryCommandSupported(command);
}

DOMString DocumentImpl::queryCommandValue(const DOMString &command)
{
    return jsEditor()->queryCommandValue(command);
}


void DocumentImpl::addMarker(RangeImpl *range, DocumentMarker::MarkerType type)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<RangeImpl> textPiece = markedText.range();
        int exception = 0;
        DocumentMarker marker = {type, textPiece->startOffset(exception), textPiece->endOffset(exception)};
        addMarker(textPiece->startContainer(exception), marker);
    }
}

void DocumentImpl::removeMarkers(RangeImpl *range, DocumentMarker::MarkerType markerType)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        RefPtr<RangeImpl> textPiece = markedText.range();
        int exception = 0;
        unsigned startOffset = textPiece->startOffset(exception);
        unsigned length = textPiece->endOffset(exception) - startOffset + 1;
        removeMarkers(textPiece->startContainer(exception), startOffset, length, markerType);
    }
}

// Markers are stored in order sorted by their location.  They do not overlap each other, as currently
// required by the drawing code in RenderText.cpp.

void DocumentImpl::addMarker(NodeImpl *node, DocumentMarker newMarker) 
{
    assert(newMarker.endOffset >= newMarker.startOffset);
    if (newMarker.endOffset == newMarker.startOffset)
        return;
    
    QValueList<DocumentMarker>* markers = m_markers.get(node);
    if (!markers) {
        markers = new QValueList<DocumentMarker>;
        markers->append(newMarker);
        m_markers.set(node, markers);
    } else {
        QValueListIterator<DocumentMarker> it;
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
                newMarker.startOffset = kMin(newMarker.startOffset, marker.startOffset);
                newMarker.endOffset = kMax(newMarker.endOffset, marker.endOffset);
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
void DocumentImpl::copyMarkers(NodeImpl *srcNode, unsigned startOffset, int length, NodeImpl *dstNode, int delta, DocumentMarker::MarkerType markerType)
{
    if (length <= 0)
        return;
    
    QValueList<DocumentMarker>* markers = m_markers.get(srcNode);
    if (!markers)
        return;

    bool docDirty = false;
    unsigned endOffset = startOffset + length - 1;
    QValueListIterator<DocumentMarker> it;
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

void DocumentImpl::removeMarkers(NodeImpl *node, unsigned startOffset, int length, DocumentMarker::MarkerType markerType)
{
    if (length <= 0)
        return;

    QValueList<DocumentMarker>* markers = m_markers.get(node);
    if (!markers)
        return;
    
    bool docDirty = false;
    unsigned endOffset = startOffset + length - 1;
    QValueListIterator<DocumentMarker> it;
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

QValueList<DocumentMarker> DocumentImpl::markersForNode(NodeImpl* node)
{
    QValueList<DocumentMarker>* markers = m_markers.get(node);
    if (markers)
        return *markers;
    return QValueList<DocumentMarker>();
}

void DocumentImpl::removeMarkers(NodeImpl* node)
{
    MarkerMap::iterator i = m_markers.find(node);
    if (i != m_markers.end()) {
        delete i->second;
        m_markers.remove(i);
        if (RenderObject* renderer = node->renderer())
            renderer->repaint();
    }
}

void DocumentImpl::removeMarkers(DocumentMarker::MarkerType markerType)
{
    // outer loop: process each markered node in the document
    MarkerMap markerMapCopy = m_markers;
    MarkerMap::iterator end = markerMapCopy.end();
    for (MarkerMap::iterator i = markerMapCopy.begin(); i != end; ++i) {
        NodeImpl* node = i->first;

        // inner loop: process each marker in the current node
        QValueList<DocumentMarker> *markers = i->second;
        QValueListIterator<DocumentMarker> markerIterator;
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

void DocumentImpl::shiftMarkers(NodeImpl *node, unsigned startOffset, int delta, DocumentMarker::MarkerType markerType)
{
    QValueList<DocumentMarker>* markers = m_markers.get(node);
    if (!markers)
        return;

    bool docDirty = false;
    QValueListIterator<DocumentMarker> it;
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

void DocumentImpl::applyXSLTransform(ProcessingInstructionImpl* pi)
{
    RefPtr<XSLTProcessorImpl> processor = new XSLTProcessorImpl;
    processor->setXSLStylesheet(static_cast<XSLStyleSheetImpl*>(pi->sheet()));
    
    QString resultMIMEType;
    QString newSource;
    QString resultEncoding;
    if (!processor->transformToString(this, resultMIMEType, newSource, resultEncoding))
        return;
    // FIXME: If the transform failed we should probably report an error (like Mozilla does).
    processor->createDocumentFromSource(newSource, resultEncoding, resultMIMEType, this, view());
}

#endif

void DocumentImpl::setDesignMode(InheritedBool value)
{
    m_designMode = value;
}

DocumentImpl::InheritedBool DocumentImpl::getDesignMode() const
{
    return m_designMode;
}

bool DocumentImpl::inDesignMode() const
{
    for (const DocumentImpl* d = this; d; d = d->parentDocument()) {
        if (d->m_designMode != inherit)
            return d->m_designMode;      
    }
    return false;
}

DocumentImpl *DocumentImpl::parentDocument() const
{
    Frame *childPart = frame();
    if (!childPart)
        return 0;
    Frame *parent = childPart->tree()->parent();
    if (!parent)
        return 0;
    return parent->document();
}

DocumentImpl *DocumentImpl::topDocument() const
{
    DocumentImpl *doc = const_cast<DocumentImpl *>(this);
    ElementImpl *element;
    while ((element = doc->ownerElement()) != 0) {
        doc = element->getDocument();
        element = doc ? doc->ownerElement() : 0;
    }
    
    return doc;
}

PassRefPtr<AttrImpl> DocumentImpl::createAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName, int &exception)
{
    if (qualifiedName.isNull()) {
        exception = DOMException::NAMESPACE_ERR;
        return 0;
    }

    DOMString localName = qualifiedName;
    DOMString prefix;
    int colonpos;
    if ((colonpos = qualifiedName.find(':')) >= 0) {
        prefix = qualifiedName.copy();
        localName = qualifiedName.copy();
        prefix.truncate(colonpos);
        localName.remove(0, colonpos+1);
    }

    if (!isValidName(localName)) {
        exception = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }
    
    // FIXME: Assume this is a mapped attribute, since createAttribute isn't namespace-aware.  There's no harm to XML
    // documents if we're wrong.
    return new AttrImpl(0, this, new MappedAttributeImpl(QualifiedName(prefix.impl(), 
                                                                       localName.impl(),
                                                                       namespaceURI.impl()), DOMString("").impl()));
}

#if SVG_SUPPORT
const SVGDocumentExtensions* DocumentImpl::svgExtensions()
{
    return m_svgExtensions;
}

SVGDocumentExtensions* DocumentImpl::accessSVGExtensions()
{
    if (!m_svgExtensions)
        m_svgExtensions = new SVGDocumentExtensions(this);
    return m_svgExtensions;
}
#endif

void DocumentImpl::radioButtonChecked(HTMLInputElementImpl *caller, HTMLFormElementImpl *form)
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
    
    HTMLInputElementImpl* currentCheckedRadio = formRadioButtons->get(caller->name().impl());
    if (currentCheckedRadio && currentCheckedRadio != caller)
        currentCheckedRadio->setChecked(false);

    formRadioButtons->set(caller->name().impl(), caller);
}

HTMLInputElementImpl* DocumentImpl::checkedRadioButtonForGroup(AtomicStringImpl* name, HTMLFormElementImpl *form)
{
    if (!form)
        form = defaultForm;
    NameToInputMap* formRadioButtons = m_selectedRadioButtons.get(form);
    if (!formRadioButtons)
        return 0;
    
    return formRadioButtons->get(name);
}

void DocumentImpl::removeRadioButtonGroup(AtomicStringImpl* name, HTMLFormElementImpl *form)
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

PassRefPtr<HTMLCollectionImpl> DocumentImpl::images()
{
    return new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_IMAGES);
}

PassRefPtr<HTMLCollectionImpl> DocumentImpl::applets()
{
    return new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_APPLETS);
}

PassRefPtr<HTMLCollectionImpl> DocumentImpl::embeds()
{
    return new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_EMBEDS);
}

PassRefPtr<HTMLCollectionImpl> DocumentImpl::objects()
{
    return new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_OBJECTS);
}

PassRefPtr<HTMLCollectionImpl> DocumentImpl::links()
{
    return new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_LINKS);
}

PassRefPtr<HTMLCollectionImpl> DocumentImpl::forms()
{
    return new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_FORMS);
}

PassRefPtr<HTMLCollectionImpl> DocumentImpl::anchors()
{
    return new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_ANCHORS);
}

PassRefPtr<HTMLCollectionImpl> DocumentImpl::all()
{
    return new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_ALL);
}

PassRefPtr<HTMLCollectionImpl> DocumentImpl::windowNamedItems(const String &name)
{
    return new HTMLNameCollectionImpl(this, HTMLCollectionImpl::WINDOW_NAMED_ITEMS, name);
}

PassRefPtr<HTMLCollectionImpl> DocumentImpl::documentNamedItems(const String &name)
{
    return new HTMLNameCollectionImpl(this, HTMLCollectionImpl::DOCUMENT_NAMED_ITEMS, name);
}

PassRefPtr<NameNodeListImpl> DocumentImpl::getElementsByName(const String &elementName)
{
    return new NameNodeListImpl(this, elementName);
}

void DocumentImpl::finishedParsing()
{
    setParsing(false);
    if (Frame* f = frame())
        f->finishedParsing();
}

}
