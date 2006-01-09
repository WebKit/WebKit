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

#include "config.h"
#include "dom_docimpl.h"

#include "dom/dom_exception.h"
#include "dom/dom2_events.h"

#include "xml/dom_textimpl.h"
#include "xml/dom_xmlimpl.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom2_viewsimpl.h"
#include "xml/EventNames.h"
#include "xml/xml_tokenizer.h"

#include "css/csshelper.h"
#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include "css/css_valueimpl.h"
#include "misc/helper.h"
#include "DocLoader.h"
#include "ecma/kjs_proxy.h"
#include "ecma/kjs_binding.h"

#include <qptrstack.h>
#include <qpaintdevicemetrics.h>
#include <qregexp.h>
#include <kdebug.h>

#include "rendering/render_canvas.h"
#include "rendering/render_frames.h"
#include "rendering/render_image.h"
#include "rendering/render_object.h"
#include "render_arena.h"

#include "khtmlview.h"

#include <kglobalsettings.h>
#include "khtml_settings.h"
#include "khtmlpart_p.h"

// FIXME: We want to cut the remaining HTML dependencies so that we don't need to include these files.
#include "html/html_documentimpl.h"
#include "html/html_headimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_formimpl.h"
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

#include "KWQAccObjectCache.h"
#include "KWQLogging.h"

#if SVG_SUPPORT
#include "SVGNames.h"
#include "SVGElementFactory.h"
#include "SVGElementImpl.h"
#include "SVGZoomEventImpl.h"
#include "SVGStyleElementImpl.h"
#include "kcanvas/device/quartz/KRenderingDeviceQuartz.h"
#endif

using namespace DOM;
using namespace DOM::EventNames;
using namespace HTMLNames;
using namespace khtml;

// #define INSTRUMENT_LAYOUT_SCHEDULING 1

// This amount of time must have elapsed before we will even consider scheduling a layout without a delay.
// FIXME: For faster machines this value can really be lowered to 200.  250 is adequate, but a little high
// for dual G5s. :)
const int cLayoutScheduleThreshold = 250;

DOMImplementationImpl *DOMImplementationImpl::m_instance = 0;

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

// FIXME: An implementation of this is still waiting for me to understand the distinction between
// a "malformed" qualified name and one with bad characters in it. For example, is a second colon
// an illegal character or a malformed qualified name? This will determine both what parameters
// this function needs to take and exactly what it will do. Should also be exported so that
// ElementImpl can use it too.
static bool qualifiedNameIsMalformed(const DOMString &)
{
    return false;
}

DOMImplementationImpl::DOMImplementationImpl()
{
}

DOMImplementationImpl::~DOMImplementationImpl()
{
}

bool DOMImplementationImpl::hasFeature (const DOMString &feature, const DOMString &version) const
{
    DOMString lower = feature.lower();
    if (lower == "core" || lower == "html" || lower == "xml" || lower == "xhtml")
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

    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    DOMString prefix, localName;
    if (!DocumentImpl::parseQualifiedName(qualifiedName, prefix, localName)) {
        exceptioncode = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if the qualifiedName is malformed.
    if (qualifiedNameIsMalformed(qualifiedName)) {
        exceptioncode = DOMException::NAMESPACE_ERR;
        return 0;
    }

    return new DocumentTypeImpl(this, 0, qualifiedName, publicId, systemId);
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

    if (!qualifiedName.isEmpty()) {
        // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
        DOMString prefix, localName;
        if (!DocumentImpl::parseQualifiedName(qualifiedName, prefix, localName)) {
            exceptioncode = DOMException::INVALID_CHARACTER_ERR;
            return 0;
        }

        // NAMESPACE_ERR:
        // - Raised if the qualifiedName is malformed,
        // - if the qualifiedName has a prefix and the namespaceURI is null, or
        // - if the qualifiedName has a prefix that is "xml" and the namespaceURI is different
        //   from "http://www.w3.org/XML/1998/namespace" [Namespaces].
        int colonpos = -1;
        uint i;
        DOMStringImpl *qname = qualifiedName.impl();
        for (i = 0; i < qname->l && colonpos < 0; i++) {
            if ((*qname)[i] == ':')
                colonpos = i;
        }
    
        if (qualifiedNameIsMalformed(qualifiedName) ||
            (colonpos >= 0 && namespaceURI.isNull()) ||
            (colonpos == 3 && qualifiedName[0] == 'x' && qualifiedName[1] == 'm' && qualifiedName[2] == 'l' &&
             namespaceURI != "http://www.w3.org/XML/1998/namespace")) {

            exceptioncode = DOMException::NAMESPACE_ERR;
            return 0;
        }
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
        doc->setDocType(new DocumentTypeImpl(doc, *doctype));

    if (!qualifiedName.isEmpty()) {
        ElementImpl *rootElement = doc->createElementNS(namespaceURI, qualifiedName, exceptioncode);
        doc->addChild(rootElement);
    }
    
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

bool DOMImplementationImpl::isXMLMIMEType(const DOMString& mimeType)
{
    if (mimeType == "text/xml" || mimeType == "application/xml" || mimeType == "application/xhtml+xml" ||
        mimeType == "text/xsl" || mimeType == "application/rss+xml" || mimeType == "application/atom+xml"
#if SVG_SUPPORT
        || mimeType == "image/svg+xml"
#endif
        )
        return true;
    return false;
}

HTMLDocumentImpl *DOMImplementationImpl::createHTMLDocument(const DOMString &title)
{
    HTMLDocumentImpl *d = createHTMLDocument( 0 /* ### create a view otherwise it doesn't work */);
    d->open();
    // FIXME: Need to escape special characters in the title?
    d->write("<html><head><title>" + title.qstring() + "</title></head>");
    return d;
}

// ------------------------------------------------------------------------

QPtrList<DocumentImpl> * DocumentImpl::changedDocuments = 0;

// KHTMLView might be 0
DocumentImpl::DocumentImpl(DOMImplementationImpl *_implementation, KHTMLView *v)
    : ContainerNodeImpl(0)
      , m_domtree_version(0)
      , m_title("")
      , m_titleSetExplicitly(false)
      , m_imageLoadEventTimer(0)
#ifndef KHTML_NO_XBL
      , m_bindingManager(new XBLBindingManager(this))
#endif
#ifdef KHTML_XSLT
    , m_transformSource(0)
#endif
    , m_finishedParsing(this, SIGNAL(finishedParsing()))
    , m_inPageCache(false)
    , m_savedRenderer(0)
    , m_passwordFields(0)
    , m_secureForms(0)
    , m_createRenderers(true)
    , m_designMode(inherit)
    , m_hasDashboardRegions(false)
    , m_dashboardRegionsDirty(false)
    , m_selfOnlyRefCount(0)
    , m_selectedRadioButtons(0)
{
    document.resetSkippingRef(this);

    m_paintDevice = 0;
    m_paintDeviceMetrics = 0;

    m_view = v;
    m_renderArena = 0;

    m_accCache = 0;
    
    if ( v ) {
        m_docLoader = new DocLoader(v->frame(), this );
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
    assert(!m_render);
    assert(!m_inPageCache);
    assert(m_savedRenderer == 0);
    
    KJS::ScriptInterpreter::forgetAllDOMNodesForDocument(this);

    if (changedDocuments && m_docChanged)
        changedDocuments->remove(this);
    delete m_tokenizer;
    document.resetSkippingRef(0);
    delete m_sheet;
    delete m_styleSelector;
    delete m_docLoader;
    if (m_elemSheet )  m_elemSheet->deref();
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

    if (m_renderArena){
        delete m_renderArena;
        m_renderArena = 0;
    }

#ifdef KHTML_XSLT
    xmlFreeDoc((xmlDocPtr)m_transformSource);
#endif

#ifndef KHTML_NO_XBL
    delete m_bindingManager;
#endif

    if (m_accCache){
        delete m_accCache;
        m_accCache = 0;
    }
    m_decoder = 0;
    
    if (m_jsEditor) {
        delete m_jsEditor;
        m_jsEditor = 0;
    }
    
    if (m_selectedRadioButtons) {
        FormToGroupMap::iterator end = m_selectedRadioButtons->end();
        for (FormToGroupMap::iterator it = m_selectedRadioButtons->begin(); it != end; ++it) {
            NameToInputMap *n = it->second;
            if (n)
                delete n;
        }
        delete m_selectedRadioButtons;
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
    return m_docType.get();
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
    return new DocumentFragmentImpl(getDocument());
}

TextImpl *DocumentImpl::createTextNode(const DOMString &data)
{
    return new TextImpl(this, data);
}

CommentImpl *DocumentImpl::createComment (const DOMString &data)
{
    return new CommentImpl(this, data);
}

CDATASectionImpl *DocumentImpl::createCDATASection(const DOMString &data, int &exception)
{
    if (isHTMLDocument()) {
        exception = DOMException::NOT_SUPPORTED_ERR;
        return NULL;
    }
    return new CDATASectionImpl(this, data);
}

ProcessingInstructionImpl *DocumentImpl::createProcessingInstruction(const DOMString &target, const DOMString &data, int &exception)
{
    if (!isValidName(target)) {
        exception = DOMException::INVALID_CHARACTER_ERR;
        return NULL;
    }
    if (isHTMLDocument()) {
        exception = DOMException::NOT_SUPPORTED_ERR;
        return NULL;
    }
    return new ProcessingInstructionImpl(this, target, data);
}

EntityReferenceImpl *DocumentImpl::createEntityReference(const DOMString &name, int &exception)
{
    if (!isValidName(name)) {
        exception = DOMException::INVALID_CHARACTER_ERR;
        return NULL;
    }
    if (isHTMLDocument()) {
        exception = DOMException::NOT_SUPPORTED_ERR;
        return NULL;
    }
    return new EntityReferenceImpl(this, name.impl());
}

EditingTextImpl *DocumentImpl::createEditingTextNode(const DOMString &text)
{
    return new EditingTextImpl(this, text);
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
            return createCDATASection(importedNode->nodeValue(), exceptioncode);
        case Node::ENTITY_REFERENCE_NODE:
            return createEntityReference(importedNode->nodeName(), exceptioncode);
        case Node::PROCESSING_INSTRUCTION_NODE:
            return createProcessingInstruction(importedNode->nodeName(), importedNode->nodeValue(), exceptioncode);
        case Node::COMMENT_NODE:
            return createComment(importedNode->nodeValue());
        case Node::ELEMENT_NODE: {
            ElementImpl *oldElement = static_cast<ElementImpl *>(importedNode);
            ElementImpl *newElement = createElementNS(oldElement->namespaceURI(), oldElement->tagName().toString(), exceptioncode);
			
            if (exceptioncode != 0)
                return 0;

            newElement->ref();

            NamedAttrMapImpl *attrs = oldElement->attributes(true);
            if (attrs) {
                unsigned length = attrs->length();
                for (unsigned i = 0; i < length; i++) {
                    AttributeImpl* attr = attrs->attributeItem(i);
                    newElement->setAttribute(attr->name(), attr->value().impl(), exceptioncode);
                    if (exceptioncode != 0) {
                        newElement->deref();
                        return 0;
                    }
                }
            }

            newElement->copyNonAttributeProperties(oldElement);

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


NodeImpl *DocumentImpl::adoptNode(NodeImpl *source, int &exceptioncode)
{
    if (!source)
        return 0;
    
    RefPtr<NodeImpl> protect(source);

    switch (source->nodeType()) {
        case Node::ENTITY_NODE:
        case Node::NOTATION_NODE:
            return 0;
        case Node::DOCUMENT_NODE:
        case Node::DOCUMENT_TYPE_NODE:
            exceptioncode = DOMException::NOT_SUPPORTED_ERR;
            return 0;            
        case Node::ATTRIBUTE_NODE: {                   
            AttrImpl *attr = static_cast<AttrImpl *>(source);
            
            if (attr->ownerElement())
                attr->ownerElement()->removeAttributeNode(attr, exceptioncode);

            attr->m_specified = true;
            break;
        }       
        default:
            if (source->parentNode())
                source->parentNode()->removeChild(source, exceptioncode);
    }
                
    for (NodeImpl *node = source; node; node = node->traverseNextNode(source)) {
        KJS::ScriptInterpreter::updateDOMNodeDocument(node, node->getDocument(), this);
        node->setDocument(this);
    }

    return source;

}

ElementImpl *DocumentImpl::createElementNS(const DOMString &_namespaceURI, const DOMString &qualifiedName, int &exceptioncode)
{
    // FIXME: We'd like a faster code path that skips this check for calls from inside the engine where the name is known to be valid.
    DOMString prefix, localName;
    if (!parseQualifiedName(qualifiedName, prefix, localName)) {
        exceptioncode = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }

    ElementImpl *e = 0;
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
    
    return e;
}

ElementImpl *DocumentImpl::getElementById( const DOMString &elementId ) const
{
    ElementImpl *element;
    QString qId = elementId.qstring();

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
            
            if (element->hasID() && element->getAttribute(idAttr) == elementId) {
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
    QString qId = elementId.qstring();
    
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
    QString qId = elementId.qstring();

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
    if (!key.length())
	return 0;

    if (!m_accessKeyDictValid) {
        m_elementsByAccessKey.clear();
    
        const NodeImpl *n;
        for (n = this; n != 0; n = n->traverseNextNode()) {
            if (!n->isElementNode())
                continue;
            const ElementImpl *elementImpl = static_cast<const ElementImpl *>(n);
            DOMString accessKey(elementImpl->getAttribute(accesskeyAttr));;
            if (!accessKey.isEmpty()) {
                QString ak = accessKey.qstring().lower();
                if (m_elementsByAccessKey.find(ak) == NULL)
                    m_elementsByAccessKey.insert(ak, elementImpl);
            }
        }
        m_accessKeyDictValid = true;
    }
    return m_elementsByAccessKey.find(key.qstring());
}

void DocumentImpl::updateTitle()
{
    Frame *p = frame();
    if (!p)
        return;

    Mac(p)->setTitle(m_title);
}

void DocumentImpl::setTitle(DOMString title, NodeImpl *titleElement)
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

RangeImpl *DocumentImpl::createRange()
{
    return new RangeImpl(this);
}

NodeIteratorImpl *DocumentImpl::createNodeIterator(NodeImpl *root, unsigned whatToShow, 
    NodeFilterImpl *filter, bool expandEntityReferences, int &exceptioncode)
{
    if (!root) {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }
    return new NodeIteratorImpl(root, whatToShow, filter, expandEntityReferences);
}

TreeWalkerImpl *DocumentImpl::createTreeWalker(NodeImpl *root, unsigned whatToShow, 
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
        changedDocuments = new QPtrList<DocumentImpl>;

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
        bool printing = m_paintDevice && (m_paintDevice->devType() == QInternal::Printer);
        fontDef.usePrinterFont = printing;
        if (m_view) {
            const KHTMLSettings *settings = m_view->frame()->settings();
            if (printing && !settings->shouldPrintBackgrounds()) {
                _style->setForceBackgroundsToWhite(true);
            }
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
    assert(!m_inPageCache);

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
    
    if (m_inPageCache) {
        if ( render )
            getAccObjectCache()->detach(render);
        return;
    }

    // Empty out these lists as a performance optimization, since detaching
    // all the individual render objects will cause all the RenderImage
    // objects to remove themselves from the lists.
    m_imageLoadEventDispatchSoonList.clear();
    m_imageLoadEventDispatchingList.clear();
    
    ContainerNodeImpl::detach();

    if ( render )
        render->destroy();

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
    SelectionController s = frame()->selection();
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
    
    // send the AXSelectedTextChanged notification only if the new selection is non-null,
    // because null selections are only transitory (e.g. when starting an EditCommand, currently)
    if (KWQAccObjectCache::accessibilityEnabled() && s.start().isNotNull() && s.end().isNotNull()) {
        getAccObjectCache()->postNotificationToTopWebArea(renderer(), "AXSelectedTextChanged");
    }

}

Tokenizer *DocumentImpl::createTokenizer()
{
    return newXMLTokenizer(this, m_view);
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

    if (frame()) {
        frame()->didExplicitOpen();
    }

    // This is work that we should probably do in clear(), but we can't have it
    // happen when implicitOpen() is called unless we reorganize Frame code.
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
    
    NodeImpl *onloadTarget = body;
    if (!isHTMLDocument())
        onloadTarget = documentElement();

    if (onloadTarget) {
        dispatchImageLoadEventsNow();
        onloadTarget->dispatchWindowEvent(loadEvent, false, false);
        if (Frame *p = frame())
            Mac(p)->handledOnloadEvents();
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!ownerElement())
            printf("onload fired at %d\n", elapsedTime());
#endif
    }

    m_processingLoadEvent = false;

    // Make sure both the initial layout and reflow happen after the onload
    // fires. This will improve onload scores, and other browsers do it.
    // If they wanna cheat, we can too. -dwh

    if (frame() && frame()->isScheduledLocationChangePending() && m_startTime.elapsed() < cLayoutScheduleThreshold) {
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
    if (renderer() && KWQAccObjectCache::accessibilityEnabled())
        getAccObjectCache()->postNotification(renderer(), "AXLoadComplete");
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
    write(text.qstring());
}

void DocumentImpl::write( const QString &text )
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

    Frame *frame = this->frame();

    if (equalIgnoringCase(equiv, "default-style")) {
        // The preferred style set has been overridden as per section 
        // 14.3.2 of the HTML4.0 specification.  We need to update the
        // sheet used variable and then update our style selector. 
        // For more info, see the test at:
        // http://www.hixie.ch/tests/evil/css/import/main/preferred.html
        // -dwh
        frame->d->m_sheetUsed = content.qstring();
        m_preferredStylesheetSet = content;
        updateStyleSelector();
    }
    else if (equalIgnoringCase(equiv, "refresh") && frame->metaRefreshEnabled())
    {
        // get delay and url
        QString str = content.qstring().stripWhiteSpace();
        int pos = str.find(QRegExp("[;,]"));
        if ( pos == -1 )
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
            if (str.find("url", 0,  false ) == 0)
                str = str.mid(3);
            str = str.stripWhiteSpace();
            if (str.length() && str[0] == '=')
                str = str.mid(1).stripWhiteSpace();
            str = parseURL(DOMString(str)).qstring();
            if (ok && frame)
                // We want a new history item if the refresh timeout > 1 second
                frame->scheduleRedirection(delay, completeURL( str ), delay <= 1);
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

bool DocumentImpl::prepareMouseEvent(bool readonly, int x, int y, MouseEvent* ev)
{
    return prepareMouseEvent(readonly, ev->type == MousePress, x, y, ev);
}

bool DocumentImpl::prepareMouseEvent(bool readonly, bool active, int _x, int _y, MouseEvent *ev)
{
    if ( m_render ) {
        assert(m_render->isCanvas());
        RenderObject::NodeInfo renderInfo(readonly, active, ev->type == MouseMove);
        bool isInside = m_render->layer()->hitTest(renderInfo, _x, _y);
        ev->innerNode = renderInfo.innerNode();

        if (renderInfo.URLElement()) {
            assert(renderInfo.URLElement()->isElementNode());
            ElementImpl* e =  static_cast<ElementImpl*>(renderInfo.URLElement());
            DOMString href = khtml::parseURL(e->getAttribute(hrefAttr));
            DOMString target = e->getAttribute(targetAttr);

            if (!target.isNull() && !href.isNull()) {
                ev->target = target;
                ev->url = href;
            }
            else
                ev->url = href;
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
  return view() ? view()->frame()->d->m_sheetUsed : DOMString();
}

void 
DocumentImpl::setSelectedStylesheetSet(const DOMString& aString)
{
  if (view()) {
    view()->frame()->d->m_sheetUsed = aString.qstring();
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
                // Don't apply transforms to already transformed documents -- <rdar://problem/4132806>
                if (!transformSourceDocument())
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
                        m_preferredStylesheetSet = view()->frame()->d->m_sheetUsed = title;
                }
                      
                if (!m_availableSheets.contains( title ) )
                    m_availableSheets.append( title );
                
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
                m_preferredStylesheetSet = view()->frame()->d->m_sheetUsed = title;

            if (!title.isEmpty()) {
                if (title != m_preferredStylesheetSet)
                    sheet = 0; // don't use it

                title = title.replace('&', QString::fromLatin1("&&"));

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
    if ( m_view && m_view->mediaType() == "print" )
	usersheet += m_printSheet;
    m_styleSelector = new CSSStyleSelector(this, usersheet, m_styleSheets, !inCompatMode());
    m_styleSelector->setEncodedURL(m_url);
    m_styleSelectorDirty = false;
}

void DocumentImpl::setHoverNode(NodeImpl* newHoverNode)
{
    if (m_hoverNode != newHoverNode)
        m_hoverNode = newHoverNode;
}

void DocumentImpl::setActiveNode(NodeImpl* newActiveNode)
{
    if (m_activeNode != newActiveNode)
        m_activeNode = newActiveNode;
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

const QValueList<DashboardRegionValue> & DocumentImpl::dashboardRegions() const
{
    return m_dashboardRegions;
}

void DocumentImpl::setDashboardRegions (const QValueList<DashboardRegionValue>& regions)
{
    m_dashboardRegions = regions;
    setDashboardRegionsDirty (false);
}


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

    if (m_focusNode && m_focusNode->isContentEditable() && !relinquishesEditingFocus(m_focusNode.get()))
        return false;
       
    bool focusChangeBlocked = false;
    RefPtr<NodeImpl> oldFocusNode = m_focusNode;
    m_focusNode = 0;
    clearSelectionIfNeeded(newFocusNode);

    // Remove focus from the existing focus node (if any)
    if (oldFocusNode && !oldFocusNode->m_inDetach) { 
        if (oldFocusNode->active())
            oldFocusNode->setActive(false);

        oldFocusNode->setFocus(false);
        oldFocusNode->dispatchHTMLEvent(blurEvent, false, false);
        if (m_focusNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusNode = 0;
        }
        clearSelectionIfNeeded(newFocusNode);
        oldFocusNode->dispatchUIEvent(DOMFocusOutEvent);
        if (m_focusNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusNode = 0;
        }
        clearSelectionIfNeeded(newFocusNode);
        if ((oldFocusNode.get() == this) && oldFocusNode->hasOneRef())
            return true;
    }

    if (newFocusNode) {
        if (newFocusNode->isContentEditable() && !acceptsEditingFocus(newFocusNode)) {
            // delegate blocks focus change
            focusChangeBlocked = true;
            goto SetFocusNodeDone;
        }
        // Set focus on the new node
        m_focusNode = newFocusNode;
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
        // eww, I suck. set the qt focus correctly
        // ### find a better place in the code for this
        if (view()) {
            QWidget *focusWidget = widgetForNode(m_focusNode.get());
            if (focusWidget) {
                // Make sure a widget has the right size before giving it focus.
                // Otherwise, we are testing edge cases of the QWidget code.
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

    if (!focusChangeBlocked && m_focusNode && KWQAccObjectCache::accessibilityEnabled())
        getAccObjectCache()->handleFocusedUIElementChanged();

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
        frame()->clearSelection();
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
    else if (eventType == "HTMLEvents" || eventType == "Event" || eventType == "Events")
        return new EventImpl();
#if SVG_SUPPORT
    else if (eventType == "SVGEvents")
        return new EventImpl();
    else if (eventType == "SVGZoomEvents")
        return new KSVG::SVGZoomEventImpl();
#endif
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
    for (; it.current(); ++it)
        if (it.current()->eventType() == evt->type())
            it.current()->listener()->handleEventImpl(evt, true);

    // handle accesskey
    if (evt->type()==keydownEvent) {
        KeyboardEventImpl *kevt = static_cast<KeyboardEventImpl *>(evt);
        if (kevt->ctrlKey()) {
            QKeyEvent *qevt = kevt->qKeyEvent();
            DOMString key = (qevt ? qevt->unmodifiedText() : kevt->keyIdentifier()).lower();
            ElementImpl *elem = getElementByAccessKey(key);
            if (elem) {
                elem->accessKeyAction(false);
                evt->setDefaultHandled();
            }
        }
    }
}

void DocumentImpl::setHTMLWindowEventListener(const AtomicString &eventType, EventListener *listener)
{
    // If we already have it we don't want removeWindowEventListener to delete it
    if (listener)
	listener->ref();
    removeHTMLWindowEventListener(eventType);
    if (listener) {
	addWindowEventListener(eventType, listener, false);
	listener->deref();
    }
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

void DocumentImpl::addWindowEventListener(const AtomicString &eventType, EventListener *listener, bool useCapture)
{
    listener->ref();

    // Remove existing identical listener set with identical arguments.
    // The DOM 2 spec says that "duplicate instances are discarded" in this case.
    removeWindowEventListener(eventType, listener, useCapture);
    m_windowEventListeners.append(new RegisteredEventListener(eventType, listener, useCapture));

    listener->deref();
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

EventListener *DocumentImpl::createHTMLEventListener(const DOMString& code, NodeImpl *node)
{
    if (frame()) {
	return frame()->createHTMLEventListener(code, node);
    } else {
	return NULL;
    }
}

void DocumentImpl::setHTMLWindowEventListener(const AtomicString& eventType, AttributeImpl* attr)
{
    setHTMLWindowEventListener(eventType, createHTMLEventListener(attr->value(), 0));
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
    Frame *childPart = childView->frame();
    if (!childPart)
        return 0;
    Frame *parent = childPart->parentFrame();
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

DOMString DocumentImpl::referrer() const
{
    if ( frame() )
        return Mac(frame())->incomingReferrer();
    
    return DOMString();
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
    for (unsigned i = 0; i < length; ) {
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

void DocumentImpl::addImageMap(HTMLMapElementImpl *imageMap)
{
    // Add the image map, unless there's already another with that name.
    // "First map wins" is the rule other browsers seem to implement.
    QString name = imageMap->getName().qstring();
    if (!m_imageMapsByName.contains(name))
        m_imageMapsByName.insert(name, imageMap);
}

void DocumentImpl::removeImageMap(HTMLMapElementImpl *imageMap)
{
    // Remove the image map by name.
    // But don't remove some other image map that just happens to have the same name.
    QString name = imageMap->getName().qstring();
    QMapIterator<QString, HTMLMapElementImpl *> it = m_imageMapsByName.find(name);
    if (it != m_imageMapsByName.end() && *it == imageMap)
        m_imageMapsByName.remove(it);
}

HTMLMapElementImpl *DocumentImpl::getImageMap(const DOMString &URL) const
{
    if (URL.isNull()) {
        return 0;
    }

    QString s = URL.qstring();
    int hashPos = s.find('#');
    if (hashPos >= 0)
        s = s.mid(hashPos + 1);

    QMapConstIterator<QString, HTMLMapElementImpl *> it = m_imageMapsByName.find(s);
    if (it == m_imageMapsByName.end())
        return 0;
    return *it;
}


void DocumentImpl::setDecoder(Decoder *decoder)
{
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

// copies markers from srcNode to dstNode, applying the specified shift delta to the copies.  The shift is
// useful if, e.g., the caller has created the dstNode from a non-prefix substring of the srcNode.
void DocumentImpl::copyMarkers(NodeImpl *srcNode, unsigned startOffset, int length, NodeImpl *dstNode, int delta, DocumentMarker::MarkerType markerType)
{
    if (length <= 0)
        return;
    
    QValueList <DocumentMarker> *markers = m_markers.find(srcNode);
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

    QValueList <DocumentMarker> *markers = m_markers.find(node);
    if (!markers)
        return;
    
    bool docDirty = false;
    unsigned endOffset = startOffset + length - 1;
    QValueListIterator<DocumentMarker> it;
    for (it = markers->begin(); it != markers->end(); ) {
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

    if (markers->isEmpty())
        m_markers.remove(node);

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

QValueList<DocumentMarker> DocumentImpl::markersForNode(NodeImpl *node)
{
    QValueList <DocumentMarker> *markers = m_markers.find(node);
    if (markers)
        return *markers;
    return QValueList <DocumentMarker> ();
}

void DocumentImpl::removeMarkers(NodeImpl *node)
{
    QValueList<DocumentMarker> *markers = m_markers.take(node);
    if (markers) {
        RenderObject *renderer = node->renderer();
        if (renderer)
            renderer->repaint();
        delete markers;
    }
}

void DocumentImpl::removeMarkers(DocumentMarker::MarkerType markerType)
{
    // outer loop: process each markered node in the document
    QPtrDictIterator< QValueList<DocumentMarker> > dictIterator(m_markers);
    for (; NodeImpl *node = static_cast<NodeImpl *>(dictIterator.currentKey()); ) {
        // inner loop: process each marker in the current node
        QValueList <DocumentMarker> *markers = static_cast<QValueList <DocumentMarker> *>(dictIterator.current());
        QValueListIterator<DocumentMarker> markerIterator;
        for (markerIterator = markers->begin(); markerIterator != markers->end(); ) {
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
        RenderObject *renderer = node->renderer();
        if (renderer)
            renderer->repaint();
    }
}

void DocumentImpl::shiftMarkers(NodeImpl *node, unsigned startOffset, int delta, DocumentMarker::MarkerType markerType)
{
    QValueList <DocumentMarker> *markers = m_markers.find(node);
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
    Frame *parent = childPart->parentFrame();
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
    return new AttrImpl(0, this, new MappedAttributeImpl(QualifiedName(prefix.impl(), 
                                                                       localName.impl(),
                                                                       namespaceURI.impl()), DOMString("").impl()), false);
}

void DocumentImpl::radioButtonChecked(HTMLInputElementImpl *caller, HTMLFormElementImpl *form)
{
    // Without a name, there is no group.
    if (caller->name().isEmpty())
        return;
    if (!form)
        form = defaultForm;
    // Uncheck the currently selected item
    if (!m_selectedRadioButtons)
        m_selectedRadioButtons = new FormToGroupMap;
    NameToInputMap* formRadioButtons = m_selectedRadioButtons->get(form);
    if (!formRadioButtons) {
        formRadioButtons = new NameToInputMap;
        m_selectedRadioButtons->set(form, formRadioButtons);
    }
    
    HTMLInputElementImpl* currentCheckedRadio = formRadioButtons->get(caller->name().impl());
    if (currentCheckedRadio && currentCheckedRadio != caller)
        currentCheckedRadio->setChecked(false);

    formRadioButtons->set(caller->name().impl(), caller);
}

HTMLInputElementImpl* DocumentImpl::checkedRadioButtonForGroup(DOMStringImpl* name, HTMLFormElementImpl *form)
{
    if (!m_selectedRadioButtons)
        return 0;
    if (!form)
        form = defaultForm;
    NameToInputMap* formRadioButtons = m_selectedRadioButtons->get(form);
    if (!formRadioButtons)
        return 0;
    
    return formRadioButtons->get(name);
}

void DocumentImpl::removeRadioButtonGroup(DOMStringImpl* name, HTMLFormElementImpl *form)
{
    if (!form)
        form = defaultForm;
    if (m_selectedRadioButtons) {
        NameToInputMap* formRadioButtons = m_selectedRadioButtons->get(form);
        if (formRadioButtons) {
            formRadioButtons->remove(name);
            if (formRadioButtons->isEmpty()) {
                m_selectedRadioButtons->remove(form);
                delete formRadioButtons;
            }
        }
    }
}

RefPtr<HTMLCollectionImpl> DocumentImpl::images()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_IMAGES));
}

RefPtr<HTMLCollectionImpl> DocumentImpl::applets()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_APPLETS));
}

RefPtr<HTMLCollectionImpl> DocumentImpl::embeds()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_EMBEDS));
}

RefPtr<HTMLCollectionImpl> DocumentImpl::objects()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_OBJECTS));
}

RefPtr<HTMLCollectionImpl> DocumentImpl::links()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_LINKS));
}

RefPtr<HTMLCollectionImpl> DocumentImpl::forms()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_FORMS));
}

RefPtr<HTMLCollectionImpl> DocumentImpl::anchors()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_ANCHORS));
}

RefPtr<HTMLCollectionImpl> DocumentImpl::all()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::DOC_ALL));
}

RefPtr<HTMLCollectionImpl> DocumentImpl::windowNamedItems(DOMString &name)
{
    return RefPtr<HTMLCollectionImpl>(new HTMLNameCollectionImpl(this, HTMLCollectionImpl::WINDOW_NAMED_ITEMS, name));
}

RefPtr<HTMLCollectionImpl> DocumentImpl::documentNamedItems(DOMString &name)
{
    return RefPtr<HTMLCollectionImpl>(new HTMLNameCollectionImpl(this, HTMLCollectionImpl::DOCUMENT_NAMED_ITEMS, name));
}

RefPtr<NameNodeListImpl> DocumentImpl::getElementsByName(const DOMString &elementName)
{
    return RefPtr<NameNodeListImpl>(new NameNodeListImpl(this, elementName));
}

// ----------------------------------------------------------------------------

DocumentFragmentImpl::DocumentFragmentImpl(DocumentImpl *doc) : ContainerNodeImpl(doc)
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


NodeImpl *DocumentFragmentImpl::cloneNode (bool deep)
{
    DocumentFragmentImpl *clone = new DocumentFragmentImpl(getDocument());
    if (deep)
        cloneChildNodes(clone);
    return clone;
}


// ----------------------------------------------------------------------------

DocumentTypeImpl::DocumentTypeImpl(DOMImplementationImpl *i, DocumentImpl *doc, const DOMString &n, const DOMString &p, const DOMString &s)
    : NodeImpl(doc), m_implementation(i), m_name(n), m_publicId(p), m_systemId(s)
{
}

DocumentTypeImpl::DocumentTypeImpl(DocumentImpl *doc, const DOMString &n, const DOMString &p, const DOMString &s)
    : NodeImpl(doc), m_name(n), m_publicId(p), m_systemId(s)
{
}

DocumentTypeImpl::DocumentTypeImpl(DocumentImpl *doc, const DocumentTypeImpl &t)
    : NodeImpl(doc), m_implementation(t.m_implementation)
    , m_name(t.m_name), m_publicId(t.m_publicId), m_systemId(t.m_systemId), m_subset(t.m_subset)
{
}

DOMString DocumentTypeImpl::toString() const
{
    if (m_name.isEmpty())
        return "";

    DOMString result = "<!DOCTYPE ";
    result += m_name;
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

NodeImpl *DocumentTypeImpl::cloneNode(bool /*deep*/)
{
    // The DOM Level 2 specification says cloning DocumentType nodes is "implementation dependent" so for now we do not support it.
    return 0;
}
