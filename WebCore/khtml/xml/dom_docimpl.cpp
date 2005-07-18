/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#include "dom_docimpl.h"

#include "dom/dom_exception.h"
#include "dom/dom2_events.h"

#include "xml/dom_textimpl.h"
#include "xml/dom_xmlimpl.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom2_viewsimpl.h"
#include "xml/xml_tokenizer.h"

#include "css/csshelper.h"
#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include "css/css_valueimpl.h"
#include "misc/helper.h"
#include "ecma/kjs_proxy.h"
#include "ecma/kjs_binding.h"

#include <qptrstack.h>
#include <qpaintdevicemetrics.h>
#include <qregexp.h>
#include <kdebug.h>
#include <kstaticdeleter.h>

#include "rendering/render_canvas.h"
#include "rendering/render_frames.h"
#include "rendering/render_image.h"
#include "rendering/render_object.h"
#include "render_arena.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include <kglobalsettings.h>
#include <kstringhandler.h>
#include "khtml_settings.h"
#include "khtmlpart_p.h"

// FIXME: We want to cut the remaining HTML dependencies so that we don't need to include these files.
#include "html/html_documentimpl.h"
#include "html/html_headimpl.h"
#include "html/html_imageimpl.h"
#include "htmlfactory.h"

#include "cssvalues.h"

#include "editing/jsediting.h"
#include "editing/visible_position.h"
#include "editing/visible_text.h"

#include <kio/job.h>

#ifdef KHTML_XSLT
#include "xsl_stylesheetimpl.h"
#include "xslt_processorimpl.h"
#endif

#ifndef KHTML_NO_XBL
#include "xbl/xbl_binding_manager.h"
using XBL::XBLBindingManager;
#endif

#if APPLE_CHANGES
#include "KWQAccObjectCache.h"
#include "KWQLogging.h"
#endif

using namespace DOM;
using namespace khtml;

// #define INSTRUMENT_LAYOUT_SCHEDULING 1

// This amount of time must have elapsed before we will even consider scheduling a layout without a delay.
// FIXME: For faster machines this value can really be lowered to 200.  250 is adequate, but a little high
// for dual G5s. :)
const int cLayoutScheduleThreshold = 250;

DOMImplementationImpl *DOMImplementationImpl::m_instance = 0;

DOMImplementationImpl::DOMImplementationImpl()
{
}

DOMImplementationImpl::~DOMImplementationImpl()
{
}

bool DOMImplementationImpl::hasFeature ( const DOMString &feature, const DOMString &version )
{
    QString lower = feature.string().lower();
    if (lower == "core" || lower == "html" || lower == "xml")
        return version.isEmpty() || version == "1.0" || version == "2.0";
    if (lower == "css"
            || lower == "css2"
            || lower == "events"
            || lower == "htmlevents"
            || lower == "mouseevents"
            || lower == "mutationevents"
            || lower == "range"
            || lower == "stylesheets"
            || lower == "traversal"
            || lower == "uievents"
            || lower == "views")
        return version.isEmpty() || version == "2.0";
    return false;
}

DocumentTypeImpl *DOMImplementationImpl::createDocumentType( const DOMString &qualifiedName, const DOMString &publicId,
                                                             const DOMString &systemId, int &exceptioncode )
{
    // Not mentioned in spec: throw NAMESPACE_ERR if no qualifiedName supplied
    if (qualifiedName.isNull()) {
        exceptioncode = DOMException::NAMESPACE_ERR;
        return 0;
    }

#if 0
    // FIXME: Add these checks (but not in a way that depends on the C++ DOM!)

    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    if (!Element::khtmlValidQualifiedName(qualifiedName)) {
        exceptioncode = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if the qualifiedName is malformed.
    if (Element::khtmlMalformedQualifiedName(qualifiedName)) {
        exceptioncode = DOMException::NAMESPACE_ERR;
        return 0;
    }
#endif

    return new DocumentTypeImpl(this,DocumentPtr::nullDocumentPtr(),qualifiedName,publicId,systemId);
}

DOMImplementationImpl* DOMImplementationImpl::getInterface(const DOMString& /*feature*/) const
{
    // ###
    return 0;
}

DocumentImpl *DOMImplementationImpl::createDocument( const DOMString &namespaceURI, const DOMString &qualifiedName,
                                                     DocumentTypeImpl *doctype, int &exceptioncode )
{
    exceptioncode = 0;

    // Not mentioned in spec: throw NAMESPACE_ERR if no qualifiedName supplied
    if (qualifiedName.isNull()) {
        exceptioncode = DOMException::NAMESPACE_ERR;
        return 0;
    }

#if 0
    // FIXME: Add this check (but not in a way that depends on the C++ DOM!)
    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    if (!Element::khtmlValidQualifiedName(qualifiedName)) {
        exceptioncode = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }
#endif

    // NAMESPACE_ERR:
    // - Raised if the qualifiedName is malformed,
    // - if the qualifiedName has a prefix and the namespaceURI is null, or
    // - if the qualifiedName has a prefix that is "xml" and the namespaceURI is different
    //   from "http://www.w3.org/XML/1998/namespace" [Namespaces].
    int colonpos = -1;
    uint i;
    DOMStringImpl *qname = qualifiedName.implementation();
    for (i = 0; i < qname->l && colonpos < 0; i++) {
        if ((*qname)[i] == ':')
            colonpos = i;
    }

    if (
#if 0
        // FIXME: Add this check (but not in a way that depends on the C++ DOM!)
        Element::khtmlMalformedQualifiedName(qualifiedName) ||
#endif
        (colonpos >= 0 && namespaceURI.isNull()) ||
        (colonpos == 3 && qualifiedName[0] == 'x' && qualifiedName[1] == 'm' && qualifiedName[2] == 'l' &&
         namespaceURI != "http://www.w3.org/XML/1998/namespace")) {

        exceptioncode = DOMException::NAMESPACE_ERR;
        return 0;
    }

    // WRONG_DOCUMENT_ERR: Raised if doctype has already been used with a different document or was
    // created from a different implementation.
    if (doctype && (doctype->getDocument() || doctype->implementation() != this)) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return 0;
    }

    // ### this is completely broken.. without a view it will not work (Dirk)
    DocumentImpl *doc = new DocumentImpl(this, 0);

    // now get the interesting parts of the doctype
    if (doctype)
        doc->realDocType()->copyFrom(*doctype);

    return doc;
}

CSSStyleSheetImpl *DOMImplementationImpl::createCSSStyleSheet(const DOMString &/*title*/, const DOMString &media, int &/*exception*/)
{
    // ### TODO : title should be set, and media could have wrong syntax, in which case we should generate an exception.
    CSSStyleSheetImpl * const nullSheet = 0;
    CSSStyleSheetImpl *sheet = new CSSStyleSheetImpl(nullSheet);
    sheet->setMedia(new MediaListImpl(sheet, media));
    return sheet;
}

DocumentImpl *DOMImplementationImpl::createDocument( KHTMLView *v )
{
    return new DocumentImpl(this, v);
}

HTMLDocumentImpl *DOMImplementationImpl::createHTMLDocument( KHTMLView *v )
{
    return new HTMLDocumentImpl(this, v);
}

DOMImplementationImpl *DOMImplementationImpl::instance()
{
    if (!m_instance) {
        m_instance = new DOMImplementationImpl();
        m_instance->ref();
    }

    return m_instance;
}

HTMLDocumentImpl *DOMImplementationImpl::createHTMLDocument(const DOMString &title)
{
    HTMLDocumentImpl *d = createHTMLDocument( 0 /* ### create a view otherwise it doesn't work */);
    d->open();
    // FIXME: Need to escape special characters in the title?
    d->write("<html><head><title>" + title.string() + "</title></head>");
    return d;
}

// ------------------------------------------------------------------------

KStaticDeleter< QPtrList<DocumentImpl> > s_changedDocumentsDeleter;
QPtrList<DocumentImpl> * DocumentImpl::changedDocuments = 0;

// KHTMLView might be 0
DocumentImpl::DocumentImpl(DOMImplementationImpl *_implementation, KHTMLView *v)
    : ContainerNodeImpl( new DocumentPtr() )
      , m_domtree_version(0)
      , m_title("")
      , m_titleSetExplicitly(false)
      , m_titleElement(0)
      , m_imageLoadEventTimer(0)
#ifndef KHTML_NO_XBL
      , m_bindingManager(new XBLBindingManager(this))
#endif
#ifdef KHTML_XSLT
    , m_transformSource(NULL)
    , m_transformSourceDocument(0)
#endif
#if APPLE_CHANGES
    , m_finishedParsing(this, SIGNAL(finishedParsing()))
    , m_inPageCache(false)
    , m_savedRenderer(0)
    , m_passwordFields(0)
    , m_secureForms(0)
    , m_decoder(0)
    , m_createRenderers(true)
    , m_designMode(inherit)
    , m_hasDashboardRegions(false)
    , m_dashboardRegionsDirty(false)
#endif
{
    document->doc = this;

    m_paintDevice = 0;
    m_paintDeviceMetrics = 0;

    m_view = v;
    m_renderArena = 0;

#if APPLE_CHANGES
    m_accCache = 0;
#endif
    
    if ( v ) {
        m_docLoader = new DocLoader(v->part(), this );
        setPaintDevice( m_view );
    }
    else
        m_docLoader = new DocLoader( 0, this );

    visuallyOrdered = false;
    m_loadingSheet = false;
    m_bParsing = false;
    m_docChanged = false;
    m_sheet = 0;
    m_elemSheet = 0;
    m_tokenizer = 0;

    // ### this should be created during parsing a <!DOCTYPE>
    // not during construction. Not sure who added that and why (Dirk)
    m_doctype = new DocumentTypeImpl(_implementation, document,
                                     DOMString() /* qualifiedName */,
                                     DOMString() /* publicId */,
                                     DOMString() /* systemId */);
    m_doctype->ref();

    m_implementation = _implementation;
    if (m_implementation)
        m_implementation->ref();
    pMode = Strict;
    hMode = XHtml;
    m_textColor = Qt::black;
    m_elementNames = 0;
    m_elementNameAlloc = 0;
    m_elementNameCount = 0;
    m_attrNames = 0;
    m_attrNameAlloc = 0;
    m_attrNameCount = 0;
    m_focusNode = 0;
    m_hoverNode = 0;
    m_defaultView = new AbstractViewImpl(this);
    m_defaultView->ref();
    m_listenerTypes = 0;
    m_styleSheets = new StyleSheetListImpl;
    m_styleSheets->ref();
    m_inDocument = true;
    m_styleSelectorDirty = false;
    m_inStyleRecalc = false;
    m_closeAfterStyleRecalc = false;
    m_usesDescendantRules = false;
    m_usesSiblingRules = false;

    m_styleSelector = new CSSStyleSelector(this, m_usersheet, m_styleSheets, !inCompatMode());
    m_windowEventListeners.setAutoDelete(true);
    m_pendingStylesheets = 0;
    m_ignorePendingStylesheets = false;

    m_cssTarget = 0;
    m_accessKeyDictValid = false;

    resetLinkColor();
    resetVisitedLinkColor();
    resetActiveLinkColor();

    m_processingLoadEvent = false;
    m_startTime.restart();
    m_overMinimumLayoutThreshold = false;
    
    m_jsEditor = 0;

    m_markers.setAutoDelete(true);
    
    static int docID = 0;
    m_docID = docID++;
}

DocumentImpl::~DocumentImpl()
{
    assert(!m_render);
#if APPLE_CHANGES
    assert(!m_inPageCache);
    assert(m_savedRenderer == 0);
#endif
    
    KJS::ScriptInterpreter::forgetAllDOMNodesForDocument(this);

    if (changedDocuments && m_docChanged)
        changedDocuments->remove(this);
    delete m_tokenizer;
    document->doc = 0;
    delete m_sheet;
    delete m_styleSelector;
    delete m_docLoader;
    if (m_elemSheet )  m_elemSheet->deref();
    if (m_doctype)
        m_doctype->deref();
    if (m_implementation)
        m_implementation->deref();
    delete m_paintDeviceMetrics;

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
    m_defaultView->deref();
    m_styleSheets->deref();

    if (m_focusNode)
        m_focusNode->deref();
    if (m_hoverNode)
        m_hoverNode->deref();

    if (m_titleElement)
        m_titleElement->deref();

    if (m_renderArena){
        delete m_renderArena;
        m_renderArena = 0;
    }

#ifdef KHTML_XSLT
    xmlFreeDoc((xmlDocPtr)m_transformSource);
    if (m_transformSourceDocument)
        m_transformSourceDocument->deref();
#endif

#ifndef KHTML_NO_XBL
    delete m_bindingManager;
#endif

#if APPLE_CHANGES
    if (m_accCache){
        delete m_accCache;
        m_accCache = 0;
    }
#endif
    
    if (m_decoder){
        m_decoder->deref();
        m_decoder = 0;
    }
    
    if (m_jsEditor) {
        delete m_jsEditor;
        m_jsEditor = 0;
    }
}

void DocumentImpl::resetLinkColor()
{
    m_linkColor = QColor(0, 0, 238);
}

void DocumentImpl::resetVisitedLinkColor()
{
    m_visitedLinkColor = QColor(85, 26, 139);    
}

void DocumentImpl::resetActiveLinkColor()
{
    m_activeLinkColor.setNamedColor(QString("red"));
}

DocumentTypeImpl *DocumentImpl::doctype() const
{
    return m_doctype;
}

DOMImplementationImpl *DocumentImpl::implementation() const
{
    return m_implementation;
}

ElementImpl *DocumentImpl::documentElement() const
{
    NodeImpl *n = firstChild();
    while (n && n->nodeType() != Node::ELEMENT_NODE)
      n = n->nextSibling();
    return static_cast<ElementImpl*>(n);
}

ElementImpl *DocumentImpl::createElement(const DOMString &name, int &exceptionCode)
{
    return createElementNS(nullAtom, name, exceptionCode);
}

DocumentFragmentImpl *DocumentImpl::createDocumentFragment(  )
{
    return new DocumentFragmentImpl( docPtr() );
}

TextImpl *DocumentImpl::createTextNode( const DOMString &data )
{
    return new TextImpl( docPtr(), data);
}

CommentImpl *DocumentImpl::createComment ( const DOMString &data )
{
    return new CommentImpl( docPtr(), data );
}

CDATASectionImpl *DocumentImpl::createCDATASection ( const DOMString &data )
{
    return new CDATASectionImpl( docPtr(), data );
}

ProcessingInstructionImpl *DocumentImpl::createProcessingInstruction ( const DOMString &target, const DOMString &data )
{
    return new ProcessingInstructionImpl( docPtr(),target,data);
}

EntityReferenceImpl *DocumentImpl::createEntityReference ( const DOMString &name )
{
    return new EntityReferenceImpl(docPtr(), name.implementation());
}

EditingTextImpl *DocumentImpl::createEditingTextNode(const DOMString &text)
{
    return new EditingTextImpl(docPtr(), text);
}

CSSStyleDeclarationImpl *DocumentImpl::createCSSStyleDeclaration()
{
    return new CSSMutableStyleDeclarationImpl;
}

NodeImpl *DocumentImpl::importNode(NodeImpl *importedNode, bool deep, int &exceptioncode)
{
    exceptioncode = 0;

    switch (importedNode->nodeType()) {
        case Node::TEXT_NODE:
            return createTextNode(importedNode->nodeValue());
        case Node::CDATA_SECTION_NODE:
            return createCDATASection(importedNode->nodeValue());
        case Node::ENTITY_REFERENCE_NODE:
            return createEntityReference(importedNode->nodeName());
        case Node::PROCESSING_INSTRUCTION_NODE:
            return createProcessingInstruction(importedNode->nodeName(), importedNode->nodeValue());
        case Node::COMMENT_NODE:
            return createComment(importedNode->nodeValue());
        case Node::ELEMENT_NODE: {
            ElementImpl *oldElement = static_cast<ElementImpl *>(importedNode);
            // FIXME: Prefix is lost.  Is that right?
            ElementImpl *newElement = createElementNS(oldElement->namespaceURI(), oldElement->localName(), exceptioncode);
            if (exceptioncode != 0)
                return 0;

            newElement->ref();

            NamedAttrMapImpl *attrs = oldElement->attributes(true);
            if (attrs) {
                unsigned length = attrs->length();
                for (unsigned i = 0; i < length; i++) {
                    AttributeImpl* attr = attrs->attributeItem(i);
                    newElement->setAttribute(attr->name(), attr->value().implementation(), exceptioncode);
                    if (exceptioncode != 0) {
                        newElement->deref();
                        return 0;
                    }
                }
            }

            if (deep) {
                for (NodeImpl *oldChild = oldElement->firstChild(); oldChild; oldChild = oldChild->nextSibling()) {
                    NodeImpl *newChild = importNode(oldChild, true, exceptioncode);
                    if (exceptioncode != 0) {
                        newElement->deref();
                        return 0;
                    }
                    newElement->appendChild(newChild, exceptioncode);
                    if (exceptioncode != 0) {
                        newElement->deref();
                        return 0;
                    }
                }
            }

            // Trick to get the result back to the floating state, with 0 refs but not destroyed.
            newElement->setParent(this);
            newElement->deref();
            newElement->setParent(0);

            return newElement;
        }
    }

    exceptioncode = DOMException::NOT_SUPPORTED_ERR;
    return 0;
}

ElementImpl *DocumentImpl::createElementNS(const DOMString &_namespaceURI, const DOMString &_qualifiedName, int &exceptioncode)
{
    // Split the name.
    int exceptionCode = 0;
    ElementImpl* e = 0;
    DOMString prefix, localName;
    int colonPos = _qualifiedName.find(':');
    if (colonPos >= 0) {
        prefix = _qualifiedName.substring(0, colonPos);
        localName = _qualifiedName.substring(colonPos+1, _qualifiedName.length() - colonPos);
    }
    else
        localName = _qualifiedName;
    
    // FIXME: Use registered namespaces and look up in a hash to find the right factory.
    if (_namespaceURI == HTMLNames::xhtmlNamespaceURI()) {
        // FIXME: Really should only be done from the public DOM API.  Internal callers know the name is valid.
        if (!isValidName(localName)) {
            exceptioncode = DOMException::INVALID_CHARACTER_ERR;
            return 0;
        }
        e = HTMLElementFactory::createHTMLElement(AtomicString(localName), this, 0, false);
        if (e && !prefix.isNull()) {
            e->setPrefix(AtomicString(prefix), exceptionCode);
            if (exceptionCode)
                return 0;
        }
    }
    
    if (!e)
        e = new ElementImpl(QualifiedName(AtomicString(prefix), AtomicString(localName), AtomicString(_namespaceURI)), document);
    
    return e;
}

ElementImpl *DocumentImpl::getElementById( const DOMString &elementId ) const
{
    ElementImpl *element;
    QString qId = elementId.string();

    if (elementId.length() == 0) {
        return 0;
    }

    element = m_elementsById.find(qId);
    
    if (element)
        return element;
        
    if (int idCount = (int)m_idCount.find(qId)) {
        for (NodeImpl *n = traverseNextNode(); n != 0; n = n->traverseNextNode()) {
            if (!n->isElementNode())
                continue;
            
            element = static_cast<ElementImpl *>(n);
            
            if (element->hasID() && element->getAttribute(HTMLAttributes::idAttr()) == elementId) {
                if (idCount == 1) 
                    m_idCount.remove(qId);
                else
                    m_idCount.insert(qId, (char *)idCount - 1);
                
                m_elementsById.insert(qId, element);
                return element;
            }
        }
    }
    return 0;
}

ElementImpl *DocumentImpl::elementFromPoint( const int _x, const int _y ) const
{
    if (!m_render) return 0;
    
    RenderObject::NodeInfo nodeInfo(true, true);
    m_render->layer()->hitTest(nodeInfo, _x, _y); 
    NodeImpl* n = nodeInfo.innerNode();

    while ( n && !n->isElementNode() ) {
        n = n->parentNode();
    }
    
    return static_cast<ElementImpl*>(n);
}

void DocumentImpl::addElementById(const DOMString &elementId, ElementImpl *element)
{
    QString qId = elementId.string();
    
    if (m_elementsById.find(qId) == NULL) {
        m_elementsById.insert(qId, element);
        m_accessKeyDictValid = false;
    } else {
        int idCount = (int)m_idCount.find(qId);
        m_idCount.insert(qId, (char *)(idCount + 1));
    }
}

void DocumentImpl::removeElementById(const DOMString &elementId, ElementImpl *element)
{
    QString qId = elementId.string();

    if (m_elementsById.find(qId) == element) {
        m_elementsById.remove(qId);
        m_accessKeyDictValid = false;
    } else {
        int idCount = (int)m_idCount.find(qId);        
        assert(idCount > 0);
        if (idCount == 1) 
            m_idCount.remove(qId);
        else
            m_idCount.insert(qId, (char *)(idCount - 1));
    }
}

ElementImpl *DocumentImpl::getElementByAccessKey( const DOMString &key )
{
    if (key.length() == 0)
	return 0;

    QString k(key.string());
    if (!m_accessKeyDictValid) {
        m_elementsByAccessKey.clear();
    
        const NodeImpl *n;
        for (n = this; n != 0; n = n->traverseNextNode()) {
            if (!n->isElementNode())
                continue;
            const ElementImpl *elementImpl = static_cast<const ElementImpl *>(n);
            DOMString accessKey(elementImpl->getAttribute(HTMLAttributes::accesskey()));;
            if (!accessKey.isEmpty()) {
                QString ak = accessKey.string().lower();
                if (m_elementsByAccessKey.find(ak) == NULL)
                    m_elementsByAccessKey.insert(ak, elementImpl);
            }
        }
        m_accessKeyDictValid = true;
    }
    return m_elementsByAccessKey.find(k);
}

void DocumentImpl::updateTitle()
{
    KHTMLPart *p = part();
    if (!p)
        return;

#if APPLE_CHANGES
    KWQ(p)->setTitle(m_title);
#else
    QString titleStr = m_title.string();
    for (int i = 0; i < titleStr.length(); ++i)
        if (titleStr[i] < ' ')
            titleStr[i] = ' ';
    titleStr = titleStr.stripWhiteSpace();
    titleStr.compose();
    if ( !p->parentPart() ) {
	if (titleStr.isNull() || titleStr.isEmpty()) {
	    // empty title... set window caption as the URL
	    KURL url = m_url;
	    url.setRef(QString::null);
	    url.setQuery(QString::null);
	    titleStr = url.url();
	}

	emit p->setWindowCaption( KStringHandler::csqueeze( titleStr, 128 ) );
    }
#endif
}

void DocumentImpl::setTitle(DOMString title, NodeImpl *titleElement)
{
    if (!titleElement) {
        // Title set by JavaScript -- overrides any title elements.
        m_titleSetExplicitly = true;
        if (m_titleElement) {
            m_titleElement->deref();
            m_titleElement = 0;
        }
    } else if (titleElement != m_titleElement) {
        if (m_titleElement) {
            // Only allow the first title element to change the title -- others have no effect.
            return;
        }
        m_titleElement = titleElement;
        titleElement->ref();
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

    titleElement->deref();

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

KHTMLPart *DocumentImpl::part() const 
{
    return m_view ? m_view->part() : 0; 
}

RangeImpl *DocumentImpl::createRange()
{
    return new RangeImpl( docPtr() );
}

NodeIteratorImpl *DocumentImpl::createNodeIterator(NodeImpl *root, unsigned long whatToShow, 
    NodeFilterImpl *filter, bool expandEntityReferences, int &exceptioncode)
{
    if (!root) {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }
    return new NodeIteratorImpl(root, whatToShow, filter, expandEntityReferences);
}

TreeWalkerImpl *DocumentImpl::createTreeWalker(NodeImpl *root, unsigned long whatToShow, 
    NodeFilterImpl *filter, bool expandEntityReferences, int &exceptioncode)
{
    if (!root) {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }
    return new TreeWalkerImpl(root, whatToShow, filter, expandEntityReferences);
}

void DocumentImpl::setDocumentChanged(bool b)
{
    if (!changedDocuments)
        changedDocuments = s_changedDocumentsDeleter.setObject(changedDocuments, new QPtrList<DocumentImpl>());

    if (b && !m_docChanged)
        changedDocuments->append(this);
    else if (!b && m_docChanged)
        changedDocuments->remove(this);
    m_docChanged = b;
    
    if (m_docChanged)
        m_accessKeyDictValid = false;
}

void DocumentImpl::recalcStyle( StyleChange change )
{
//     qDebug("recalcStyle(%p)", this);
//     QTime qt;
//     qt.start();
    if (m_inStyleRecalc)
        return; // Guard against re-entrancy. -dwh
        
    m_inStyleRecalc = true;
    
    if( !m_render ) goto bail_out;

    if ( change == Force ) {
        RenderStyle* oldStyle = m_render->style();
        if ( oldStyle ) oldStyle->ref();
        RenderStyle* _style = new (m_renderArena) RenderStyle();
        _style->ref();
        _style->setDisplay(BLOCK);
        _style->setVisuallyOrdered( visuallyOrdered );
        // ### make the font stuff _really_ work!!!!

	khtml::FontDef fontDef;
	QFont f = KGlobalSettings::generalFont();
	fontDef.family = *(f.firstFamily());
	fontDef.italic = f.italic();
	fontDef.weight = f.weight();
#if APPLE_CHANGES
        bool printing = m_paintDevice && (m_paintDevice->devType() == QInternal::Printer);
        fontDef.usePrinterFont = printing;
#endif
        if (m_view) {
            const KHTMLSettings *settings = m_view->part()->settings();
#if APPLE_CHANGES
            if (printing && !settings->shouldPrintBackgrounds()) {
                _style->setForceBackgroundsToWhite(true);
            }
#endif
            QString stdfont = settings->stdFontName();
            if ( !stdfont.isEmpty() ) {
                fontDef.family.setFamily(stdfont);
                fontDef.family.appendFamily(0);
            }
            m_styleSelector->setFontSize(fontDef, m_styleSelector->fontSizeForKeyword(CSS_VAL_MEDIUM, inCompatMode()));
        }

        //kdDebug() << "DocumentImpl::attach: setting to charset " << settings->charset() << endl;
        _style->setFontDef(fontDef);
	_style->htmlFont().update( paintDeviceMetrics() );
        if ( inCompatMode() )
            _style->setHtmlHacks(true); // enable html specific rendering tricks

        StyleChange ch = diff( _style, oldStyle );
        if(m_render && ch != NoChange)
            m_render->setStyle(_style);
        if ( change != Force )
            change = ch;

        _style->deref(m_renderArena);
        if (oldStyle)
            oldStyle->deref(m_renderArena);
    }

    NodeImpl *n;
    for (n = _first; n; n = n->nextSibling())
        if ( change>= Inherit || n->hasChangedChild() || n->changed() )
            n->recalcStyle( change );
    //kdDebug( 6020 ) << "TIME: recalcStyle() dt=" << qt.elapsed() << endl;

    if (changed() && m_view)
	m_view->layout();

bail_out:
    setChanged( false );
    setHasChangedChild( false );
    setDocumentChanged( false );
    
    m_inStyleRecalc = false;
    
    // If we wanted to emit the implicitClose() during recalcStyle, do so now that we're finished.
    if (m_closeAfterStyleRecalc) {
        m_closeAfterStyleRecalc = false;
        implicitClose();
    }
}

void DocumentImpl::updateRendering()
{
    if (!hasChangedChild()) return;

//     QTime time;
//     time.start();
//     kdDebug() << "UPDATERENDERING: "<<endl;

    StyleChange change = NoChange;
#if 0
    if ( m_styleSelectorDirty ) {
	recalcStyleSelector();
	change = Force;
    }
#endif
    recalcStyle( change );

//    kdDebug() << "UPDATERENDERING time used="<<time.elapsed()<<endl;
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
    // FIXME: Dave's pretty sure we can remove this because
    // layout calls recalcStyle as needed.
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
#if APPLE_CHANGES
    assert(!m_inPageCache);
#endif

    if ( m_view )
        setPaintDevice( m_view );

    if (!m_renderArena)
        m_renderArena = new RenderArena();
    
    // Create the rendering tree
    m_render = new (m_renderArena) RenderCanvas(this, m_view);
    recalcStyle( Force );

    RenderObject* render = m_render;
    m_render = 0;

    ContainerNodeImpl::attach();
    m_render = render;
}

void DocumentImpl::restoreRenderer(RenderObject* render)
{
    m_render = render;
}

void DocumentImpl::detach()
{
    RenderObject* render = m_render;

    // indicate destruction mode,  i.e. attached() but m_render == 0
    m_render = 0;
    
#if APPLE_CHANGES
    if (m_inPageCache) {
        if ( render )
            getAccObjectCache()->detach(render);
        return;
    }
#endif

    // Empty out these lists as a performance optimization, since detaching
    // all the individual render objects will cause all the RenderImage
    // objects to remove themselves from the lists.
    m_imageLoadEventDispatchSoonList.clear();
    m_imageLoadEventDispatchingList.clear();
    
    ContainerNodeImpl::detach();

    if ( render )
        render->detach();

    if (m_paintDevice == m_view)
        setPaintDevice(0);
    m_view = 0;
    
    if (m_renderArena){
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

void DocumentImpl::registerDisconnectedNodeWithEventListeners(NodeImpl *node)
{
    m_disconnectedNodesWithEventListeners.insert(node, node);
}

void DocumentImpl::unregisterDisconnectedNodeWithEventListeners(NodeImpl *node)
{
    m_disconnectedNodesWithEventListeners.remove(node);
}

void DocumentImpl::removeAllDisconnectedNodeEventListeners()
{
    for (QPtrDictIterator<NodeImpl> iter(m_disconnectedNodesWithEventListeners);
         iter.current();
         ++iter) {
        iter.current()->removeAllEventListeners();
    }
    m_disconnectedNodesWithEventListeners.clear();
}

#if APPLE_CHANGES
KWQAccObjectCache* DocumentImpl::getAccObjectCache()
{
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
    return m_accCache;
}
#endif

void DocumentImpl::setVisuallyOrdered()
{
    visuallyOrdered = true;
    if (m_render)
        m_render->style()->setVisuallyOrdered(true);
}

void DocumentImpl::updateSelection()
{
    if (!m_render)
        return;
    
    RenderCanvas *canvas = static_cast<RenderCanvas*>(m_render);
    Selection s = part()->selection();
    if (!s.isRange()) {
        canvas->clearSelection();
    }
    else {
        Position startPos = VisiblePosition(s.start(), s.startAffinity()).deepEquivalent();
        Position endPos = VisiblePosition(s.end(), s.endAffinity()).deepEquivalent();
        if (startPos.isNotNull() && endPos.isNotNull()) {
            RenderObject *startRenderer = startPos.node()->renderer();
            RenderObject *endRenderer = endPos.node()->renderer();
            static_cast<RenderCanvas*>(m_render)->setSelection(startRenderer, startPos.offset(), endRenderer, endPos.offset());
        }
    }
    
#if APPLE_CHANGES
    // send the AXSelectedTextChanged notification only if the new selection is non-null,
    // because null selections are only transitory (e.g. when starting an EditCommand, currently)
    if (KWQAccObjectCache::accessibilityEnabled() && s.start().isNotNull() && s.end().isNotNull()) {
        getAccObjectCache()->postNotificationToTopWebArea(renderer(), "AXSelectedTextChanged");
    }
#endif

}

Tokenizer *DocumentImpl::createTokenizer()
{
    return newXMLTokenizer(docPtr(), m_view);
}

void DocumentImpl::setPaintDevice( QPaintDevice *dev )
{
    if (m_paintDevice == dev) {
        return;
    }
    m_paintDevice = dev;
    delete m_paintDeviceMetrics;
    m_paintDeviceMetrics = dev ? new QPaintDeviceMetrics( dev ) : 0;
}

void DocumentImpl::open(  )
{
    if (parsing()) return;

    implicitOpen();

    if (part()) {
        part()->didExplicitOpen();
    }

    // This is work that we should probably do in clear(), but we can't have it
    // happen when implicitOpen() is called unless we reorganize KHTMLPart code.
    setURL(QString());
    DocumentImpl *parent = parentDocument();
    if (parent) {
        setBaseURL(parent->baseURL());
    }
}

void DocumentImpl::implicitOpen()
{
    if (m_tokenizer)
        close();

    clear();
    m_tokenizer = createTokenizer();
    connect(m_tokenizer,SIGNAL(finishedParsing()),this,SIGNAL(finishedParsing()));
    setParsing(true);

    if (m_view && m_view->part()->jScript()) {
        m_view->part()->jScript()->setSourceFile(m_url,""); //fixme
    }
}

HTMLElementImpl* DocumentImpl::body()
{
    NodeImpl *de = documentElement();
    if (!de)
        return 0;
    
    // try to prefer a FRAMESET element over BODY
    NodeImpl* body = 0;
    for (NodeImpl* i = de->firstChild(); i; i = i->nextSibling()) {
        if (i->hasTagName(HTMLNames::frameset()))
            return static_cast<HTMLElementImpl*>(i);
        
        if (i->hasTagName(HTMLNames::body()))
            body = i;
    }
    return static_cast<HTMLElementImpl *>(body);
}

void DocumentImpl::close()
{
    if (part())
        part()->endIfNotLoading();
    implicitClose();
}

void DocumentImpl::implicitClose()
{
    // If we're in the middle of recalcStyle, we need to defer the close until the style information is accurate and all elements are re-attached.
    if (m_inStyleRecalc) {
        m_closeAfterStyleRecalc = true;
        return;
    }

    // First fire the onload.
    
    bool wasLocationChangePending = part() && part()->isScheduledLocationChangePending();
    bool doload = !parsing() && m_tokenizer && !m_processingLoadEvent && !wasLocationChangePending;
    
    if (doload) {
        m_processingLoadEvent = true;

        // We have to clear the tokenizer, in case someone document.write()s from the
        // onLoad event handler, as in Radar 3206524
        delete m_tokenizer;
        m_tokenizer = 0;

        // Create a body element if we don't already have one.
        // In the case of Radar 3758785, the window.onload was set in some javascript, but never fired because there was no body.  
        // This behavior now matches Firefox and IE.
        HTMLElementImpl *body = this->body();
        if (!body && isHTMLDocument()) {
            NodeImpl *de = documentElement();
            if (de) {
                body = new HTMLBodyElementImpl(docPtr());
                int exceptionCode = 0;
                de->appendChild(body, exceptionCode);
                if (exceptionCode != 0)
                    body = 0;
            }
        }

        if (body) {
            dispatchImageLoadEventsNow();
            body->dispatchWindowEvent(EventImpl::LOAD_EVENT, false, false);

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (!ownerElement())
                printf("onload fired at %d\n", elapsedTime());
#endif
        }

        m_processingLoadEvent = false;
    }
    
    // Make sure both the initial layout and reflow happen after the onload
    // fires. This will improve onload scores, and other browsers do it.
    // If they wanna cheat, we can too. -dwh
    
    bool isLocationChangePending = part() && part()->isScheduledLocationChangePending();
    
    if (doload && isLocationChangePending && m_startTime.elapsed() < cLayoutScheduleThreshold) {
	// Just bail out. Before or during the onload we were shifted to another page.
	// The old i-Bench suite does this. When this happens don't bother painting or laying out.        
	delete m_tokenizer;
	m_tokenizer = 0;
	view()->unscheduleRelayout();
	return;
    }
    
    if (doload) {
        // on an explicit document.close(), the tokenizer might still be waiting on scripts,
        // and in that case we don't want to destroy it because that will prevent the
        // scripts from getting processed.
        // FIXME: this check may no longer be necessary, since now it should be impossible
        // for parsing to be false while stil waiting for scripts
        if (m_tokenizer && !m_tokenizer->isWaitingForScripts()) {
            delete m_tokenizer;
            m_tokenizer = 0;
        }

        if (m_view)
            m_view->part()->checkEmitLoadEvent();
    }

    // Now do our painting/layout, but only if we aren't in a subframe or if we're in a subframe
    // that has been sized already.  Otherwise, our view size would be incorrect, so doing any 
    // layout/painting now would be pointless.
    if (doload) {
        if (!ownerElement() || (ownerElement()->renderer() && !ownerElement()->renderer()->needsLayout())) {
            updateRendering();
            
            // Always do a layout after loading if needed.
            if (view() && renderer() && (!renderer()->firstChild() || renderer()->needsLayout()))
                view()->layout();
        }
#if APPLE_CHANGES
        if (renderer() && KWQAccObjectCache::accessibilityEnabled())
            getAccObjectCache()->postNotification(renderer(), "AXLoadComplete");
#endif
    }
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
            (!documentElement()->hasTagName(HTMLNames::html()) || body()));
}

int DocumentImpl::minimumLayoutDelay()
{
    if (m_overMinimumLayoutThreshold)
        return 0;
    
    int elapsed = m_startTime.elapsed();
    m_overMinimumLayoutThreshold = elapsed > cLayoutScheduleThreshold;
    
    // We'll want to schedule the timer to fire at the minimum layout threshold.
    return kMax(0, cLayoutScheduleThreshold - elapsed);
}

int DocumentImpl::elapsedTime() const
{
    return m_startTime.elapsed();
}

void DocumentImpl::write( const DOMString &text )
{
    write(text.string());
}

void DocumentImpl::write( const QString &text )
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Beginning a document.write at %d\n", elapsedTime());
#endif
    
    if (!m_tokenizer) {
        open();
        write(QString::fromLatin1("<html>"));
    }
    m_tokenizer->write(text, false);

    if (m_view && m_view->part()->jScript())
        m_view->part()->jScript()->appendSourceFile(m_url,text);
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Ending a document.write at %d\n", elapsedTime());
#endif    
}

void DocumentImpl::writeln( const DOMString &text )
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
//    kdDebug( 6030 ) << "HTMLDocument::setStyleSheet()" << endl;
    m_sheet = new CSSStyleSheetImpl(this, url);
    m_sheet->ref();
    m_sheet->parseString(sheet);
    m_loadingSheet = false;

    updateStyleSelector();
}

void DocumentImpl::setUserStyleSheet( const QString& sheet )
{
    if ( m_usersheet != sheet ) {
        m_usersheet = sheet;
	updateStyleSelector();
    }
}

CSSStyleSheetImpl* DocumentImpl::elementSheet()
{
    if (!m_elemSheet) {
        m_elemSheet = new CSSStyleSheetImpl(this, baseURL() );
        m_elemSheet->ref();
    }
    return m_elemSheet;
}

void DocumentImpl::determineParseMode( const QString &/*str*/ )
{
    // For XML documents use strict parse mode.  HTML docs will override this method to
    // determine their parse mode.
    pMode = Strict;
    hMode = XHtml;
    kdDebug(6020) << " using strict parseMode" << endl;
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

    KHTMLPart *part = this->part();

    if (strcasecmp(equiv, "default-style") == 0) {
        // The preferred style set has been overridden as per section 
        // 14.3.2 of the HTML4.0 specification.  We need to update the
        // sheet used variable and then update our style selector. 
        // For more info, see the test at:
        // http://www.hixie.ch/tests/evil/css/import/main/preferred.html
        // -dwh
        part->d->m_sheetUsed = content.string();
        m_preferredStylesheetSet = content;
        updateStyleSelector();
    }
    else if(strcasecmp(equiv, "refresh") == 0 && part->metaRefreshEnabled())
    {
        // get delay and url
        QString str = content.string().stripWhiteSpace();
        int pos = str.find(QRegExp("[;,]"));
        if ( pos == -1 )
            pos = str.find(QRegExp("[ \t]"));

        if (pos == -1) // There can be no url (David)
        {
            bool ok = false;
            int delay = 0;
	    delay = str.toInt(&ok);
#if APPLE_CHANGES
	    // We want a new history item if the refresh timeout > 1 second
	    if(ok && part) part->scheduleRedirection(delay, part->url().url(), delay <= 1);
#else
	    if(ok && part) part->scheduleRedirection(delay, part->url().url() );
#endif
        } else {
            double delay = 0;
            bool ok = false;
	    delay = str.left(pos).stripWhiteSpace().toDouble(&ok);

            pos++;
            while(pos < (int)str.length() && str[pos].isSpace()) pos++;
            str = str.mid(pos);
            if(str.find("url", 0,  false ) == 0)  str = str.mid(3);
            str = str.stripWhiteSpace();
            if ( str.length() && str[0] == '=' ) str = str.mid( 1 ).stripWhiteSpace();
            str = parseURL( DOMString(str) ).string();
            if ( ok && part )
#if APPLE_CHANGES
                // We want a new history item if the refresh timeout > 1 second
                part->scheduleRedirection(delay, completeURL( str ), delay <= 1);
#else
                part->scheduleRedirection(delay, completeURL( str ));
#endif
        }
    }
    else if(strcasecmp(equiv, "expires") == 0)
    {
        QString str = content.string().stripWhiteSpace();
        time_t expire_date = str.toLong();
        if (m_docLoader)
            m_docLoader->setExpireDate(expire_date);
    }
    else if(strcasecmp(equiv, "pragma") == 0 || strcasecmp(equiv, "cache-control") == 0 && part)
    {
        QString str = content.string().lower().stripWhiteSpace();
        KURL url = part->url();
        if ((str == "no-cache") && url.protocol().startsWith("http"))
        {
           KIO::http_update_cache(url, true, 0);
        }
    }
    else if( (strcasecmp(equiv, "set-cookie") == 0))
    {
        // ### make setCookie work on XML documents too; e.g. in case of <html:meta .....>
        HTMLDocumentImpl *d = static_cast<HTMLDocumentImpl *>(this);
        d->setCookie(content);
    }
}

bool DocumentImpl::prepareMouseEvent( bool readonly, int _x, int _y, MouseEvent *ev )
{
    if ( m_render ) {
        assert(m_render->isCanvas());
        RenderObject::NodeInfo renderInfo(readonly, ev->type == MousePress);
        bool isInside = m_render->layer()->hitTest(renderInfo, _x, _y);
        ev->innerNode.reset(renderInfo.innerNode());

        if (renderInfo.URLElement()) {
            assert(renderInfo.URLElement()->isElementNode());
            ElementImpl* e =  static_cast<ElementImpl*>(renderInfo.URLElement());
            DOMString href = khtml::parseURL(e->getAttribute(HTMLAttributes::href()));
            DOMString target = e->getAttribute(HTMLAttributes::target());

            if (!target.isNull() && !href.isNull()) {
                ev->target = target;
                ev->url = href;
            }
            else
                ev->url = href;
//            qDebug("url: *%s*", ev->url.string().latin1());
        }

        if (!readonly)
            updateRendering();

        return isInside;
    }


    return false;
}

// DOM Section 1.1.1
bool DocumentImpl::childAllowed( NodeImpl *newChild )
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

bool DocumentImpl::childTypeAllowed( unsigned short type )
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

NodeImpl *DocumentImpl::cloneNode ( bool /*deep*/ )
{
    // Spec says cloning Document nodes is "implementation dependent"
    // so we do not support it...
    return 0;
}

StyleSheetListImpl* DocumentImpl::styleSheets()
{
    return m_styleSheets;
}

DOMString DocumentImpl::preferredStylesheetSet()
{
  return m_preferredStylesheetSet;
}

DOMString DocumentImpl::selectedStylesheetSet()
{
  return view() ? view()->part()->d->m_sheetUsed : DOMString();
}

void 
DocumentImpl::setSelectedStylesheetSet(const DOMString& aString)
{
  if (view()) {
    view()->part()->d->m_sheetUsed = aString.string();
    updateStyleSelector();
    if (renderer())
      renderer()->repaint();
  }
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
#if 0

    m_styleSelectorDirty = true;
#endif

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
    if ( !m_render || !attached() ) return;

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
            if (pi->isXSL()) {
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
                ElementImpl* elem = getElementById(pi->localHref());
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
        else if (n->isHTMLElement() && (n->hasTagName(HTMLNames::link()) || n->hasTagName(HTMLNames::style()))) {
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(n);
            QString title = e->getAttribute(HTMLAttributes::title()).string();
            bool enabledViaScript = false;
            if (e->hasLocalName(HTMLNames::link())) {
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
            if (e->hasLocalName(HTMLNames::link()))
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
                    QString rel = e->getAttribute(HTMLAttributes::rel()).string();
                    if (e->hasLocalName(HTMLNames::style()) || !rel.contains("alternate"))
                        m_preferredStylesheetSet = view()->part()->d->m_sheetUsed = title;
                }
                      
                if (!m_availableSheets.contains( title ) )
                    m_availableSheets.append( title );
                
                if (title != m_preferredStylesheetSet)
                    sheet = 0;
            }
        }

        if (sheet) {
            sheet->ref();
            m_styleSheets->styleSheets.append(sheet);
        }
    
        // For HTML documents, stylesheets are not allowed within/after the <BODY> tag. So we
        // can stop searching here.
        if (isHTMLDocument() && n->hasTagName(HTMLNames::body()))
            break;
    }

    // De-reference all the stylesheets in the old list
    QPtrListIterator<StyleSheetImpl> it(oldStyleSheets);
    for (; it.current(); ++it)
	it.current()->deref();

    // Create a new style selector
    delete m_styleSelector;
    QString usersheet = m_usersheet;
    if ( m_view && m_view->mediaType() == "print" )
	usersheet += m_printSheet;
    m_styleSelector = new CSSStyleSelector(this, usersheet, m_styleSheets, !inCompatMode());
    m_styleSelector->setEncodedURL(m_url);
    m_styleSelectorDirty = false;
}

void DocumentImpl::setHoverNode(NodeImpl* newHoverNode)
{
    if (m_hoverNode != newHoverNode) {
        if (m_hoverNode)
            m_hoverNode->deref();
        m_hoverNode = newHoverNode;
        if (m_hoverNode)
            m_hoverNode->ref();
    }    
}

#if APPLE_CHANGES

bool DocumentImpl::relinquishesEditingFocus(NodeImpl *node)
{
    assert(node);
    assert(node->isContentEditable());

    NodeImpl *root = node->rootEditableElement();
    if (!part() || !root)
        return false;

    return part()->shouldEndEditing(rangeOfContents(root).get());
}

bool DocumentImpl::acceptsEditingFocus(NodeImpl *node)
{
    assert(node);
    assert(node->isContentEditable());

    NodeImpl *root = node->rootEditableElement();
    if (!part() || !root)
        return false;

    return part()->shouldBeginEditing(rangeOfContents(root).get());
}

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

static QWidget *widgetForNode(NodeImpl *focusNode)
{
    if (!focusNode)
        return 0;
    RenderObject *renderer = focusNode->renderer();
    if (!renderer || !renderer->isWidget())
        return 0;
    return static_cast<RenderWidget *>(renderer)->widget();
}

bool DocumentImpl::setFocusNode(NodeImpl *newFocusNode)
{    
    // Make sure newFocusNode is actually in this document
    if (newFocusNode && (newFocusNode->getDocument() != this))
        return true;

    if (m_focusNode == newFocusNode)
        return true;

#if APPLE_CHANGES
    if (m_focusNode && m_focusNode->isContentEditable() && !relinquishesEditingFocus(m_focusNode))
        return false;
#endif     
       
    bool focusChangeBlocked = false;
    NodeImpl *oldFocusNode = m_focusNode;
    m_focusNode = 0;

    // Remove focus from the existing focus node (if any)
    if (oldFocusNode) {
        if (oldFocusNode->active())
            oldFocusNode->setActive(false);

        oldFocusNode->setFocus(false);
        oldFocusNode->dispatchHTMLEvent(EventImpl::BLUR_EVENT, false, false);
        if (m_focusNode != 0) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusNode = 0;
        }
        oldFocusNode->dispatchUIEvent(EventImpl::DOMFOCUSOUT_EVENT);
        if (m_focusNode != 0) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusNode = 0;
        }
        if ((oldFocusNode == this) && oldFocusNode->hasOneRef()) {
            oldFocusNode->deref(); // deletes this
            return true;
        }
        else {
            oldFocusNode->deref();
        }
    }

    // Clear the selection when changing the focus node to null or to a node that is not 
    // contained by the current selection.
    if (part()) {
        NodeImpl *startContainer = part()->selection().start().node();
        if (!newFocusNode || (startContainer && startContainer != newFocusNode && !startContainer->isAncestor(newFocusNode)))
            part()->clearSelection();
    }

    if (newFocusNode) {
#if APPLE_CHANGES            
        if (newFocusNode->isContentEditable() && !acceptsEditingFocus(newFocusNode)) {
            // delegate blocks focus change
            focusChangeBlocked = true;
            goto SetFocusNodeDone;
        }
#endif
        // Set focus on the new node
        m_focusNode = newFocusNode;
        m_focusNode->ref();
        m_focusNode->dispatchHTMLEvent(EventImpl::FOCUS_EVENT, false, false);
        if (m_focusNode != newFocusNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusNodeDone;
        }
        m_focusNode->dispatchUIEvent(EventImpl::DOMFOCUSIN_EVENT);
        if (m_focusNode != newFocusNode) { 
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusNodeDone;
        }
        m_focusNode->setFocus();
        // eww, I suck. set the qt focus correctly
        // ### find a better place in the code for this
        if (view()) {
            QWidget *focusWidget = widgetForNode(m_focusNode);
            if (focusWidget) {
                // Make sure a widget has the right size before giving it focus.
                // Otherwise, we are testing edge cases of the QWidget code.
                // Specifically, in WebCore this does not work well for text fields.
                updateLayout();
                // Re-get the widget in case updating the layout changed things.
                focusWidget = widgetForNode(m_focusNode);
            }
            if (focusWidget)
                focusWidget->setFocus();
            else
                view()->setFocus();
        }
   }

#if APPLE_CHANGES
    if (!focusChangeBlocked && m_focusNode && KWQAccObjectCache::accessibilityEnabled())
        getAccObjectCache()->handleFocusedUIElementChanged();
#endif

SetFocusNodeDone:
    updateRendering();
    return !focusChangeBlocked;
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
    return m_defaultView;
}

EventImpl *DocumentImpl::createEvent(const DOMString &eventType, int &exceptioncode)
{
    if (eventType == "UIEvents" || eventType == "UIEvent")
        return new UIEventImpl();
    else if (eventType == "MouseEvents" || eventType == "MouseEvent")
        return new MouseEventImpl();
    else if (eventType == "MutationEvents" || eventType == "MutationEvent")
        return new MutationEventImpl();
    else if (eventType == "KeyboardEvents" || eventType == "KeyboardEvent")
        return new KeyboardEventImpl();
    else if (eventType == "HTMLEvents" || eventType == "Event")
        return new EventImpl();
    else {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }
}

CSSStyleDeclarationImpl *DocumentImpl::getOverrideStyle(ElementImpl */*elt*/, const DOMString &/*pseudoElt*/)
{
    return 0; // ###
}

void DocumentImpl::defaultEventHandler(EventImpl *evt)
{
    // if any html event listeners are registered on the window, then dispatch them here
    QPtrList<RegisteredEventListener> listenersCopy = m_windowEventListeners;
    QPtrListIterator<RegisteredEventListener> it(listenersCopy);
    for (; it.current(); ++it) {
        if (it.current()->id == evt->id()) {
            it.current()->listener->handleEventImpl(evt, true);
	}
    }

    // handle accesskey
    if (evt->id()==EventImpl::KEYDOWN_EVENT) {
        KeyboardEventImpl *kevt = static_cast<KeyboardEventImpl *>(evt);
        if (kevt->ctrlKey()) {
            QKeyEvent *qevt = kevt->qKeyEvent();
            QString key = (qevt ? qevt->unmodifiedText() : kevt->keyIdentifier().string()).lower();
            ElementImpl *elem = getElementByAccessKey(key);
            if (elem) {
                elem->accessKeyAction(false);
                evt->setDefaultHandled();
            }
        }
    }
}

void DocumentImpl::setHTMLWindowEventListener(int id, EventListener *listener)
{
    // If we already have it we don't want removeWindowEventListener to delete it
    if (listener)
	listener->ref();
    removeHTMLWindowEventListener(id);
    if (listener) {
	addWindowEventListener(id, listener, false);
	listener->deref();
    }
}

EventListener *DocumentImpl::getHTMLWindowEventListener(int id)
{
    QPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it) {
	if (it.current()->id == id &&
            it.current()->listener->eventListenerType() == "_khtml_HTMLEventListener") {
	    return it.current()->listener;
	}
    }

    return 0;
}

void DocumentImpl::removeHTMLWindowEventListener(int id)
{
    QPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it) {
	if (it.current()->id == id &&
            it.current()->listener->eventListenerType() == "_khtml_HTMLEventListener") {
	    m_windowEventListeners.removeRef(it.current());
	    return;
	}
    }
}

void DocumentImpl::addWindowEventListener(int id, EventListener *listener, const bool useCapture)
{
    listener->ref();

    // remove existing identical listener set with identical arguments - the DOM2
    // spec says that "duplicate instances are discarded" in this case.
    removeWindowEventListener(id,listener,useCapture);

    RegisteredEventListener *rl = new RegisteredEventListener(static_cast<EventImpl::EventId>(id), listener, useCapture);
    m_windowEventListeners.append(rl);

    listener->deref();
}

void DocumentImpl::removeWindowEventListener(int id, EventListener *listener, bool useCapture)
{
    RegisteredEventListener rl(static_cast<EventImpl::EventId>(id),listener,useCapture);

    QPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it)
        if (*(it.current()) == rl) {
            m_windowEventListeners.removeRef(it.current());
            return;
        }
}

bool DocumentImpl::hasWindowEventListener(int id)
{
    QPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it) {
	if (it.current()->id == id) {
	    return true;
	}
    }

    return false;
}

EventListener *DocumentImpl::createHTMLEventListener(QString code, NodeImpl *node)
{
    if (part()) {
	return part()->createHTMLEventListener(code, node);
    } else {
	return NULL;
    }
}

void DocumentImpl::dispatchImageLoadEventSoon(HTMLImageLoader *image)
{
    m_imageLoadEventDispatchSoonList.append(image);
    if (!m_imageLoadEventTimer) {
        m_imageLoadEventTimer = startTimer(0);
    }
}

void DocumentImpl::removeImage(HTMLImageLoader* image)
{
    // Remove instances of this image from both lists.
    // Use loops because we allow multiple instances to get into the lists.
    while (m_imageLoadEventDispatchSoonList.removeRef(image)) { }
    while (m_imageLoadEventDispatchingList.removeRef(image)) { }
    if (m_imageLoadEventDispatchSoonList.isEmpty() && m_imageLoadEventTimer) {
        killTimer(m_imageLoadEventTimer);
        m_imageLoadEventTimer = 0;
    }
}

void DocumentImpl::dispatchImageLoadEventsNow()
{
    // need to avoid re-entering this function; if new dispatches are
    // scheduled before the parent finishes processing the list, they
    // will set a timer and eventually be processed
    if (!m_imageLoadEventDispatchingList.isEmpty()) {
        return;
    }

    if (m_imageLoadEventTimer) {
        killTimer(m_imageLoadEventTimer);
        m_imageLoadEventTimer = 0;
    }
    
    m_imageLoadEventDispatchingList = m_imageLoadEventDispatchSoonList;
    m_imageLoadEventDispatchSoonList.clear();
    for (QPtrListIterator<HTMLImageLoader> it(m_imageLoadEventDispatchingList); it.current(); ) {
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

void DocumentImpl::timerEvent(QTimerEvent *)
{
    dispatchImageLoadEventsNow();
}

ElementImpl *DocumentImpl::ownerElement()
{
    KHTMLView *childView = view();
    if (!childView)
        return 0;
    KHTMLPart *childPart = childView->part();
    if (!childPart)
        return 0;
    KHTMLPart *parent = childPart->parentPart();
    if (!parent)
        return 0;
    ChildFrame *childFrame = parent->childFrame(childPart);
    if (!childFrame)
        return 0;
    RenderPart *renderPart = childFrame->m_frame;
    if (!renderPart)
        return 0;
    return static_cast<ElementImpl *>(renderPart->element());
}

DOMString DocumentImpl::domain() const
{
    if ( m_domain.isEmpty() ) // not set yet (we set it on demand to save time and space)
        m_domain = KURL(URL()).host(); // Initially set to the host
    return m_domain;
}

void DocumentImpl::setDomain(const DOMString &newDomain, bool force /*=false*/)
{
    if ( force ) {
        m_domain = newDomain;
        return;
    }
    if ( m_domain.isEmpty() ) // not set yet (we set it on demand to save time and space)
        m_domain = KURL(URL()).host(); // Initially set to the host

    // Both NS and IE specify that changing the domain is only allowed when
    // the new domain is a suffix of the old domain.
    int oldLength = m_domain.length();
    int newLength = newDomain.length();
    if ( newLength < oldLength ) // e.g. newDomain=kde.org (7) and m_domain=www.kde.org (11)
    {
        DOMString test = m_domain.copy();
        if ( test[oldLength - newLength - 1] == '.' ) // Check that it's a subdomain, not e.g. "de.org"
        {
            test.remove( 0, oldLength - newLength ); // now test is "kde.org" from m_domain
            if ( test == newDomain )                 // and we check that it's the same thing as newDomain
                m_domain = newDomain;
        }
    }
}

bool DocumentImpl::isValidName(const DOMString &name)
{
    static const char validFirstCharacter[] = "ABCDEFGHIJKLMNOPQRSTUVWXZYabcdefghijklmnopqrstuvwxyz";
    static const char validSubsequentCharacter[] = "ABCDEFGHIJKLMNOPQRSTUVWXZYabcdefghijklmnopqrstuvwxyz0123456789-_:.";
    const unsigned length = name.length();
    if (length == 0)
        return false;
    const QChar * const characters = name.unicode();
    const char fc = characters[0];
    if (!fc)
        return false;
    if (strchr(validFirstCharacter, fc) == 0)
        return false;
    for (unsigned i = 1; i < length; ++i) {
        const char sc = characters[i];
        if (!sc)
            return false;
        if (strchr(validSubsequentCharacter, sc) == 0)
            return false;
    }
    return true;
}

void DocumentImpl::addImageMap(HTMLMapElementImpl *imageMap)
{
    // Add the image map, unless there's already another with that name.
    // "First map wins" is the rule other browsers seem to implement.
    QString name = imageMap->getName().string();
    if (!m_imageMapsByName.contains(name))
        m_imageMapsByName.insert(name, imageMap);
}

void DocumentImpl::removeImageMap(HTMLMapElementImpl *imageMap)
{
    // Remove the image map by name.
    // But don't remove some other image map that just happens to have the same name.
    QString name = imageMap->getName().string();
    QMapIterator<QString, HTMLMapElementImpl *> it = m_imageMapsByName.find(name);
    if (it != m_imageMapsByName.end() && *it == imageMap)
        m_imageMapsByName.remove(it);
}

HTMLMapElementImpl *DocumentImpl::getImageMap(const DOMString &URL) const
{
    if (URL.isNull()) {
        return 0;
    }

    QString s = URL.string();
    int hashPos = s.find('#');
    if (hashPos >= 0)
        s = s.mid(hashPos + 1);

    QMapConstIterator<QString, HTMLMapElementImpl *> it = m_imageMapsByName.find(s);
    if (it == m_imageMapsByName.end())
        return 0;
    return *it;
}

#if APPLE_CHANGES

void DocumentImpl::setDecoder(Decoder *decoder)
{
    decoder->ref();
    if (m_decoder) {
        m_decoder->deref();
    }
    m_decoder = decoder;
}

QString DocumentImpl::completeURL(const QString &URL)
{
    return KURL(baseURL(), URL, m_decoder ? m_decoder->codec() : 0).url();
}

DOMString DocumentImpl::completeURL(const DOMString &URL)
{
    if (URL.isNull())
        return URL;
    return completeURL(URL.string());
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
        m_savedRenderer = m_render;
        if (m_view) {
            m_view->resetScrollBars();
        }
    } else {
        assert(m_render == 0 || m_render == m_savedRenderer);
        m_render = m_savedRenderer;
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

#endif // APPLE_CHANGES

// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------

void DocumentImpl::addMarker(RangeImpl *range, DocumentMarker::MarkerType type)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        SharedPtr<RangeImpl> textPiece = markedText.range();
        int exception = 0;
        DocumentMarker marker = {type, textPiece->startOffset(exception), textPiece->endOffset(exception)};
        addMarker(textPiece->startContainer(exception), marker);
    }
}

void DocumentImpl::removeMarker(RangeImpl *range, DocumentMarker::MarkerType type)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        SharedPtr<RangeImpl> textPiece = markedText.range();
        int exception = 0;
        DocumentMarker marker = {type, textPiece->startOffset(exception), textPiece->endOffset(exception)};
        removeMarker(textPiece->startContainer(exception), marker);
    }
}

// FIXME:  We don't deal with markers of more than one type yet

// Markers are stored in order sorted by their location.  They do not overlap each other, as currently
// required by the drawing code in render_text.cpp.

void DocumentImpl::addMarker(NodeImpl *node, DocumentMarker newMarker) 
{
    assert(newMarker.endOffset >= newMarker.startOffset);
    if (newMarker.endOffset == newMarker.startOffset) {
        return;     // zero length markers are a NOP
    }
    
    QValueList <DocumentMarker> *markers = m_markers.find(node);
    if (!markers) {
        markers = new QValueList <DocumentMarker>;
        markers->append(newMarker);
        m_markers.insert(node, markers);
    } else {
        QValueListIterator<DocumentMarker> it;
        for (it = markers->begin(); it != markers->end(); ) {
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

void DocumentImpl::removeMarker(NodeImpl *node, DocumentMarker target)
{
    assert(target.endOffset >= target.startOffset);
    if (target.endOffset == target.startOffset) {
        return;     // zero length markers are a NOP
    }

    QValueList <DocumentMarker> *markers = m_markers.find(node);
    if (!markers) {
        return;
    }
    
    bool docDirty = false;
    QValueListIterator<DocumentMarker> it;
    for (it = markers->begin(); it != markers->end(); ) {
        DocumentMarker marker = *it;

        if (target.endOffset <= marker.startOffset) {
            // This is the first marker that is completely after target.  All done.
            break;
        } else if (target.startOffset >= marker.endOffset) {
            // marker is before target.  Keep scanning.
            it++;
        } else {
            // at this point we know that marker and target intersect in some way
            docDirty = true;

            // pitch the old marker
            it = markers->remove(it);
            // it now points to the next node
            
            // add either of the resulting slices that are left after removing target
            if (target.startOffset > marker.startOffset) {
                DocumentMarker newLeft = marker;
                newLeft.endOffset = target.startOffset;
                markers->insert(it, newLeft);
            }
            if (marker.endOffset > target.endOffset) {
                DocumentMarker newRight = marker;
                newRight.startOffset = target.endOffset;
                markers->insert(it, newRight);
            }
        }
    }

    if (markers->isEmpty())
        m_markers.remove(node);

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

QValueList<DocumentMarker> DocumentImpl::markersForNode(NodeImpl *node)
{
    QValueList <DocumentMarker> *markers = m_markers.find(node);
    if (markers) {
        return *markers;
    } else {
        return QValueList <DocumentMarker> ();
    }
}

void DocumentImpl::removeAllMarkers(NodeImpl *node, ulong startOffset, long length)
{
    // FIXME - yet another cheat that relies on us only having one marker type
    DocumentMarker marker = {DocumentMarker::Spelling, startOffset, startOffset+length};
    removeMarker(node, marker);
}

void DocumentImpl::removeAllMarkers(NodeImpl *node)
{
    QValueList<DocumentMarker> *markers = m_markers.take(node);
    if (markers) {
        RenderObject *renderer = node->renderer();
        if (renderer)
            renderer->repaint();
        delete markers;
    }
}

void DocumentImpl::removeAllMarkers()
{
    QPtrDictIterator< QValueList<DocumentMarker> > it(m_markers);
    for (; NodeImpl *node = static_cast<NodeImpl *>(it.currentKey()); ++it) {
        RenderObject *renderer = node->renderer();
        if (renderer)
            renderer->repaint();
    }
    m_markers.clear();
}

void DocumentImpl::shiftMarkers(NodeImpl *node, ulong startOffset, long delta)
{
    QValueList <DocumentMarker> *markers = m_markers.find(node);
    if (!markers)
        return;

    bool docDirty = false;
    QValueListIterator<DocumentMarker> it;
    for (it = markers->begin(); it != markers->end(); ++it) {
        DocumentMarker &marker = *it;
        if (marker.startOffset >= startOffset) {
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
    // Ref ourselves to keep from being destroyed.
    XSLTProcessorImpl processor(static_cast<XSLStyleSheetImpl*>(pi->sheet()), this);
    processor.transformDocument(this);

    // FIXME: If the transform failed we should probably report an error (like Mozilla does) in this
    // case.
}

void DocumentImpl::setTransformSourceDocument(DocumentImpl* doc)
{ 
    if (m_transformSourceDocument)
        m_transformSourceDocument->deref(); 
    m_transformSourceDocument = doc;
    if (doc)
        doc->ref();
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
    KHTMLPart *childPart = part();
    if (!childPart)
        return 0;
    KHTMLPart *parent = childPart->parentPart();
    if (!parent)
        return 0;
    return parent->xmlDocImpl();
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

AttrImpl *DocumentImpl::createAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName, int &exception)
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
    return new AttrImpl(0, docPtr(), new MappedAttributeImpl(QualifiedName(prefix.implementation(), 
                                                                           localName.implementation(),
                                                                           namespaceURI.implementation()), DOMString("").implementation()));
}

SharedPtr<HTMLCollectionImpl> DocumentImpl::images()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_IMAGES));
}

SharedPtr<HTMLCollectionImpl> DocumentImpl::applets()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_APPLETS));
}

SharedPtr<HTMLCollectionImpl> DocumentImpl::embeds()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_EMBEDS));
}

SharedPtr<HTMLCollectionImpl> DocumentImpl::objects()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_OBJECTS));
}

SharedPtr<HTMLCollectionImpl> DocumentImpl::links()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_LINKS));
}

SharedPtr<HTMLCollectionImpl> DocumentImpl::forms()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_FORMS));
}

SharedPtr<HTMLCollectionImpl> DocumentImpl::anchors()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_ANCHORS));
}

SharedPtr<HTMLCollectionImpl> DocumentImpl::all()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_ALL));
}

SharedPtr<HTMLCollectionImpl> DocumentImpl::nameableItems()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_NAMEABLE_ITEMS));
}

SharedPtr<NameNodeListImpl> DocumentImpl::getElementsByName(const DOMString &elementName)
{
    return SharedPtr<NameNodeListImpl>(new NameNodeListImpl(this, elementName));
}

// ----------------------------------------------------------------------------

DocumentFragmentImpl::DocumentFragmentImpl(DocumentPtr *doc) : ContainerNodeImpl(doc)
{
}

DOMString DocumentFragmentImpl::nodeName() const
{
  return "#document-fragment";
}

unsigned short DocumentFragmentImpl::nodeType() const
{
    return Node::DOCUMENT_FRAGMENT_NODE;
}

// DOM Section 1.1.1
bool DocumentFragmentImpl::childTypeAllowed( unsigned short type )
{
    switch (type) {
        case Node::ELEMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::COMMENT_NODE:
        case Node::TEXT_NODE:
        case Node::CDATA_SECTION_NODE:
        case Node::ENTITY_REFERENCE_NODE:
            return true;
        default:
            return false;
    }
}

DOMString DocumentFragmentImpl::toString() const
{
    DOMString result;

    for (NodeImpl *child = firstChild(); child != NULL; child = child->nextSibling()) {
	result += child->toString();
    }

    return result;
}


NodeImpl *DocumentFragmentImpl::cloneNode ( bool deep )
{
    DocumentFragmentImpl *clone = new DocumentFragmentImpl( docPtr() );
    if (deep)
        cloneChildNodes(clone);
    return clone;
}


// ----------------------------------------------------------------------------

DocumentTypeImpl::DocumentTypeImpl(DOMImplementationImpl *implementation, DocumentPtr *doc,
                                   const DOMString &qualifiedName, const DOMString &publicId,
                                   const DOMString &systemId)
    : NodeImpl(doc), m_implementation(implementation),
      m_qualifiedName(qualifiedName), m_publicId(publicId), m_systemId(systemId)
{
    if (m_implementation)
        m_implementation->ref();

    m_entities = 0;
    m_notations = 0;

    // if doc is 0, it is not attached to a document and / or
    // therefore does not provide entities or notations. (DOM Level 3)
}

DocumentTypeImpl::~DocumentTypeImpl()
{
    if (m_implementation)
        m_implementation->deref();
    if (m_entities)
        m_entities->deref();
    if (m_notations)
        m_notations->deref();
}

void DocumentTypeImpl::copyFrom(const DocumentTypeImpl& other)
{
    m_qualifiedName = other.m_qualifiedName;
    m_publicId = other.m_publicId;
    m_systemId = other.m_systemId;
    m_subset = other.m_subset;
}

DOMString DocumentTypeImpl::toString() const
{
    DOMString result;
    if (m_qualifiedName.isEmpty()) {
        return "";
    } else {
        result = "<!DOCTYPE ";
        result += m_qualifiedName;
    }
    if (!m_publicId.isEmpty()) {
	result += " PUBLIC \"";
	result += m_publicId;
	result += "\" \"";
	result += m_systemId;
	result += "\"";
    } else if (!m_systemId.isEmpty()) {
	result += " SYSTEM \"";
	result += m_systemId;
	result += "\"";
    }
    if (!m_subset.isEmpty()) {
	result += " [";
	result += m_subset;
	result += "]";
    }
    result += ">";
    return result;
}

DOMString DocumentTypeImpl::nodeName() const
{
    return name();
}

unsigned short DocumentTypeImpl::nodeType() const
{
    return Node::DOCUMENT_TYPE_NODE;
}

// DOM Section 1.1.1
bool DocumentTypeImpl::childTypeAllowed( unsigned short /*type*/ )
{
    return false;
}

NodeImpl *DocumentTypeImpl::cloneNode ( bool /*deep*/ )
{
    // Spec says cloning Document nodes is "implementation dependent"
    // so we do not support it...
    return 0;
}

#include "dom_docimpl.moc"
