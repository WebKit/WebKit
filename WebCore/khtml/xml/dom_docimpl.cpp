/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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

#include "dom/dom_exception.h"

#include "xml/dom_textimpl.h"
#include "xml/dom_xmlimpl.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/xml_tokenizer.h"

#include "css/csshelper.h"
#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include "misc/htmlhashes.h"
#include "misc/helper.h"
#include "ecma/kjs_proxy.h"
#include "ecma/kjs_binding.h"

#include <qptrstack.h>
#include <qpaintdevicemetrics.h>
#include <kdebug.h>
#include <kstaticdeleter.h>

#include "rendering/render_canvas.h"
#include "rendering/render_frames.h"
#include "rendering/render_image.h"
#include "render_arena.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include <kglobalsettings.h>
#include <kstringhandler.h>
#include "khtml_settings.h"
#include "khtmlpart_p.h"

#include "html/html_baseimpl.h"
#include "html/html_blockimpl.h"
#include "html/html_documentimpl.h"
#include "html/html_formimpl.h"
#include "html/html_headimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_listimpl.h"
#include "html/html_miscimpl.h"
#include "html/html_tableimpl.h"
#include "html/html_objectimpl.h"

#include <kio/job.h>

using namespace DOM;
using namespace khtml;

DOMImplementationImpl *DOMImplementationImpl::m_instance = 0;

DOMImplementationImpl::DOMImplementationImpl()
{
}

DOMImplementationImpl::~DOMImplementationImpl()
{
}

bool DOMImplementationImpl::hasFeature ( const DOMString &feature, const DOMString &version )
{
    // ### update when we (fully) support the relevant features
    QString lower = feature.string().lower();
    if ((lower == "html" || lower == "xml") &&
        (version == "1.0" || version == "null" || version == "" || version.isNull()))
        return true;
    else
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
    if (!Element::khtmlValidQualifiedName(qualifiedName)) {
        exceptioncode = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if the qualifiedName is malformed.
    if (Element::khtmlMalformedQualifiedName(qualifiedName)) {
        exceptioncode = DOMException::NAMESPACE_ERR;
        return 0;
    }

    return new DocumentTypeImpl(this,0,qualifiedName,publicId,systemId);
}

DOMImplementationImpl* DOMImplementationImpl::getInterface(const DOMString& /*feature*/) const
{
    // ###
    return 0;
}

DocumentImpl *DOMImplementationImpl::createDocument( const DOMString &namespaceURI, const DOMString &qualifiedName,
                                                     const DocumentType &doctype, int &exceptioncode )
{
    exceptioncode = 0;

    // Not mentioned in spec: throw NAMESPACE_ERR if no qualifiedName supplied
    if (qualifiedName.isNull()) {
        exceptioncode = DOMException::NAMESPACE_ERR;
        return 0;
    }

    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    if (!Element::khtmlValidQualifiedName(qualifiedName)) {
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
    DOMStringImpl *qname = qualifiedName.implementation();
    for (i = 0; i < qname->l && colonpos < 0; i++) {
        if ((*qname)[i] == ':')
            colonpos = i;
    }

    if (Element::khtmlMalformedQualifiedName(qualifiedName) ||
        (colonpos >= 0 && namespaceURI.isNull()) ||
        (colonpos == 3 && qualifiedName[0] == 'x' && qualifiedName[1] == 'm' && qualifiedName[2] == 'l' &&
         namespaceURI != "http://www.w3.org/XML/1998/namespace")) {

        exceptioncode = DOMException::NAMESPACE_ERR;
        return 0;
    }

    DocumentTypeImpl *dtype = static_cast<DocumentTypeImpl*>(doctype.handle());
    // WRONG_DOCUMENT_ERR: Raised if doctype has already been used with a different document or was
    // created from a different implementation.
    if (dtype && (dtype->getDocument() || dtype->implementation() != this)) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return 0;
    }

    // ### this is completely broken.. without a view it will not work (Dirk)
    DocumentImpl *doc = new DocumentImpl(this, 0);

    // now get the interesting parts of the doctype
    // ### create new one if not there (currently always there)
    if (doc->doctype() && dtype)
        doc->doctype()->copyFrom(*dtype);

    ElementImpl *element = doc->createElementNS(namespaceURI,qualifiedName);
    doc->appendChild(element,exceptioncode);
    if (exceptioncode) {
        delete element;
        delete doc;
        return 0;
    }

    return doc;
}

CSSStyleSheetImpl *DOMImplementationImpl::createCSSStyleSheet(DOMStringImpl */*title*/, DOMStringImpl *media,
                                                              int &/*exceptioncode*/)
{
    // ### TODO : title should be set, and media could have wrong syntax, in which case we should
	// generate an exception.
	CSSStyleSheetImpl *parent = 0L;
	CSSStyleSheetImpl *sheet = new CSSStyleSheetImpl(parent, DOMString());
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

// ------------------------------------------------------------------------

KStaticDeleter< QPtrList<DocumentImpl> > s_changedDocumentsDeleter;
QPtrList<DocumentImpl> * DocumentImpl::changedDocuments = 0;

// KHTMLView might be 0
DocumentImpl::DocumentImpl(DOMImplementationImpl *_implementation, KHTMLView *v)
    : NodeBaseImpl( new DocumentPtr() )
    , m_imageLoadEventTimer(0)
#if APPLE_CHANGES
    , m_finishedParsing(this, SIGNAL(finishedParsing()))
    , m_inPageCache(0), m_passwordFields(0), m_secureForms(0)
    , m_decoder(0), m_createRenderers(true)
#endif
{
    document->doc = this;

    m_paintDevice = 0;
    m_paintDeviceMetrics = 0;

    m_view = v;
    m_renderArena = 0;
    
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
    m_namespaceURIAlloc = 4;
    m_namespaceURICount = 1;
    QString xhtml(XHTML_NAMESPACE);
    m_namespaceURIs = new DOMStringImpl* [m_namespaceURIAlloc];
    m_namespaceURIs[0] = new DOMStringImpl(xhtml.unicode(), xhtml.length());
    m_namespaceURIs[0]->ref();
    m_focusNode = 0;
    m_defaultView = new AbstractViewImpl(this);
    m_defaultView->ref();
    m_listenerTypes = 0;
    m_styleSheets = new StyleSheetListImpl;
    m_styleSheets->ref();
    m_inDocument = true;
    m_styleSelectorDirty = false;
    m_inStyleRecalc = false;
    m_usesDescendantRules = false;
    
    m_styleSelector = new CSSStyleSelector( this, m_usersheet, m_styleSheets, m_url,
                                            !inCompatMode() );
    m_windowEventListeners.setAutoDelete(true);
    m_pendingStylesheets = 0;

    m_cssTarget = 0;
}

DocumentImpl::~DocumentImpl()
{
    assert(!m_render);
    
    KJS::ScriptInterpreter::forgetDOMObjectsForDocument(this);

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
    for (unsigned short id = 0; id < m_namespaceURICount; ++id)
        m_namespaceURIs[id]->deref();
    delete [] m_namespaceURIs;
    m_defaultView->deref();
    m_styleSheets->deref();
    if (m_focusNode)
        m_focusNode->deref();
        
    if (m_renderArena){
        delete m_renderArena;
        m_renderArena = 0;
    }
    
    if (m_decoder){
        m_decoder->deref();
        m_decoder = 0;
    }
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

ElementImpl *DocumentImpl::createElement( const DOMString &name )
{
    return new XMLElementImpl( document, name.implementation() );
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

Attr DocumentImpl::createAttribute( NodeImpl::Id id )
{
    return new AttrImpl(0, docPtr(), new AttributeImpl(id, DOMString("").implementation()));
}

EntityReferenceImpl *DocumentImpl::createEntityReference ( const DOMString &name )
{
    return new EntityReferenceImpl(docPtr(), name.implementation());
}

NodeImpl *DocumentImpl::importNode(NodeImpl *importedNode, bool deep, int &exceptioncode)
{
	NodeImpl *result = 0;

	if(importedNode->nodeType() == Node::ELEMENT_NODE)
	{
		ElementImpl *tempElementImpl = createElementNS(getDocument()->namespaceURI(id()), importedNode->nodeName());
		result = tempElementImpl;

		if(static_cast<ElementImpl *>(importedNode)->attributes(true) && static_cast<ElementImpl *>(importedNode)->attributes(true)->length())
		{
			NamedNodeMapImpl *attr = static_cast<ElementImpl *>(importedNode)->attributes();

			for(unsigned int i = 0; i < attr->length(); i++)
			{
				DOM::DOMString qualifiedName = attr->item(i)->nodeName();
				DOM::DOMString value = attr->item(i)->nodeValue();

				int colonpos = qualifiedName.find(':');
				DOMString localName = qualifiedName;
				if(colonpos >= 0)
				{
					localName.remove(0, colonpos + 1);
					// ### extract and set new prefix
				}

				NodeImpl::Id nodeId = getDocument()->attrId(getDocument()->namespaceURI(id()), localName.implementation(), false /* allocate */);
				tempElementImpl->setAttribute(nodeId, value.implementation(), exceptioncode);

				if(exceptioncode != 0)
					break;
			}
		}
	}
	else if(importedNode->nodeType() == Node::TEXT_NODE)
	{
		result = createTextNode(importedNode->nodeValue());
		deep = false;
	}
	else if(importedNode->nodeType() == Node::CDATA_SECTION_NODE)
	{
		result = createCDATASection(importedNode->nodeValue());
		deep = false;
	}
	else if(importedNode->nodeType() == Node::ENTITY_REFERENCE_NODE)
		result = createEntityReference(importedNode->nodeName());
	else if(importedNode->nodeType() == Node::PROCESSING_INSTRUCTION_NODE)
	{
		result = createProcessingInstruction(importedNode->nodeName(), importedNode->nodeValue());
		deep = false;
	}
	else if(importedNode->nodeType() == Node::COMMENT_NODE)
	{
		result = createComment(importedNode->nodeValue());
		deep = false;
	}
	else
		exceptioncode = DOMException::NOT_SUPPORTED_ERR;

	if(deep)
	{
		for(Node n = importedNode->firstChild(); !n.isNull(); n = n.nextSibling())
			result->appendChild(importNode(n.handle(), true, exceptioncode), exceptioncode);
	}

	return result;
}

ElementImpl *DocumentImpl::createElementNS( const DOMString &_namespaceURI, const DOMString &_qualifiedName )
{
    ElementImpl *e = 0;
    QString qName = _qualifiedName.string();
    int colonPos = qName.find(':',0);

    if ((_namespaceURI.isNull() && colonPos < 0) ||
        _namespaceURI == XHTML_NAMESPACE) {
        // User requested an element in the XHTML namespace - this means we create a HTML element
        // (elements not in this namespace are treated as normal XML elements)
        e = createHTMLElement(qName.mid(colonPos+1));
        int exceptioncode = 0;
        if (colonPos >= 0)
            e->setPrefix(qName.left(colonPos),  exceptioncode);
    }
    if (!e)
        e = new XMLElementImpl( document, _qualifiedName.implementation(), _namespaceURI.implementation() );

    return e;
}

ElementImpl *DocumentImpl::getElementById( const DOMString &elementId ) const
{
    if (elementId.length() == 0) {
	return 0;
    }

    QPtrStack<NodeImpl> nodeStack;
    NodeImpl *current = _first;

    while(1)
    {
        if(!current)
        {
            if(nodeStack.isEmpty()) break;
            current = nodeStack.pop();
            current = current->nextSibling();
        }
        else
        {
            if(current->isElementNode())
            {
                ElementImpl *e = static_cast<ElementImpl *>(current);
                if(e->getAttribute(ATTR_ID) == elementId)
                    return e;
            }

            NodeImpl *child = current->firstChild();
            if(child)
            {
                nodeStack.push(current);
                current = child;
            }
            else
            {
                current = current->nextSibling();
            }
        }
    }

    //kdDebug() << "WARNING: *DocumentImpl::getElementById not found " << elementId.string() << endl;
    return 0;
}

void DocumentImpl::setTitle(DOMString _title)
{
    m_title = _title;

    if (!view() || !view()->part())
        return;

#if APPLE_CHANGES
    KWQ(view()->part())->setTitle(_title);
#else
    QString titleStr = m_title.string();
    for (int i = 0; i < titleStr.length(); ++i)
        if (titleStr[i] < ' ')
            titleStr[i] = ' ';
    titleStr = titleStr.stripWhiteSpace();
    titleStr.compose();
    if ( !view()->part()->parentPart() ) {
	if (titleStr.isNull() || titleStr.isEmpty()) {
	    // empty title... set window caption as the URL
	    KURL url = m_url;
	    url.setRef(QString::null);
	    url.setQuery(QString::null);
	    titleStr = url.url();
	}

	emit view()->part()->setWindowCaption( KStringHandler::csqueeze( titleStr, 128 ) );
    }
#endif
}

DOMString DocumentImpl::nodeName() const
{
    return "#document";
}

unsigned short DocumentImpl::nodeType() const
{
    return Node::DOCUMENT_NODE;
}

ElementImpl *DocumentImpl::createHTMLElement( const DOMString &name )
{
    uint id = khtml::getTagID( name.string().lower().latin1(), name.string().length() );

    ElementImpl *n = 0;
    switch(id)
    {
    case ID_HTML:
        n = new HTMLHtmlElementImpl(docPtr());
        break;
    case ID_HEAD:
        n = new HTMLHeadElementImpl(docPtr());
        break;
    case ID_BODY:
        n = new HTMLBodyElementImpl(docPtr());
        break;

// head elements
    case ID_BASE:
        n = new HTMLBaseElementImpl(docPtr());
        break;
    case ID_LINK:
        n = new HTMLLinkElementImpl(docPtr());
        break;
    case ID_META:
        n = new HTMLMetaElementImpl(docPtr());
        break;
    case ID_STYLE:
        n = new HTMLStyleElementImpl(docPtr());
        break;
    case ID_TITLE:
        n = new HTMLTitleElementImpl(docPtr());
        break;

// frames
    case ID_FRAME:
        n = new HTMLFrameElementImpl(docPtr());
        break;
    case ID_FRAMESET:
        n = new HTMLFrameSetElementImpl(docPtr());
        break;
    case ID_IFRAME:
        n = new HTMLIFrameElementImpl(docPtr());
        break;

// form elements
// ### FIXME: we need a way to set form dependency after we have made the form elements
    case ID_FORM:
            n = new HTMLFormElementImpl(docPtr());
        break;
    case ID_BUTTON:
            n = new HTMLButtonElementImpl(docPtr());
        break;
    case ID_FIELDSET:
            n = new HTMLFieldSetElementImpl(docPtr());
        break;
    case ID_INPUT:
            n = new HTMLInputElementImpl(docPtr());
        break;
    case ID_ISINDEX:
            n = new HTMLIsIndexElementImpl(docPtr());
        break;
    case ID_LABEL:
            n = new HTMLLabelElementImpl(docPtr());
        break;
    case ID_LEGEND:
            n = new HTMLLegendElementImpl(docPtr());
        break;
    case ID_OPTGROUP:
            n = new HTMLOptGroupElementImpl(docPtr());
        break;
    case ID_OPTION:
            n = new HTMLOptionElementImpl(docPtr());
        break;
    case ID_SELECT:
            n = new HTMLSelectElementImpl(docPtr());
        break;
    case ID_TEXTAREA:
            n = new HTMLTextAreaElementImpl(docPtr());
        break;

// lists
    case ID_DL:
        n = new HTMLDListElementImpl(docPtr());
        break;
    case ID_DD:
        n = new HTMLGenericElementImpl(docPtr(), id);
        break;
    case ID_DT:
        n = new HTMLGenericElementImpl(docPtr(), id);
        break;
    case ID_UL:
        n = new HTMLUListElementImpl(docPtr());
        break;
    case ID_OL:
        n = new HTMLOListElementImpl(docPtr());
        break;
    case ID_DIR:
        n = new HTMLDirectoryElementImpl(docPtr());
        break;
    case ID_MENU:
        n = new HTMLMenuElementImpl(docPtr());
        break;
    case ID_LI:
        n = new HTMLLIElementImpl(docPtr());
        break;

// formatting elements (block)
    case ID_BLOCKQUOTE:
        n = new HTMLBlockquoteElementImpl(docPtr());
        break;
    case ID_DIV:
        n = new HTMLDivElementImpl(docPtr());
        break;
    case ID_H1:
    case ID_H2:
    case ID_H3:
    case ID_H4:
    case ID_H5:
    case ID_H6:
        n = new HTMLHeadingElementImpl(docPtr(), id);
        break;
    case ID_HR:
        n = new HTMLHRElementImpl(docPtr());
        break;
    case ID_P:
        n = new HTMLParagraphElementImpl(docPtr());
        break;
    case ID_PRE:
        n = new HTMLPreElementImpl(docPtr(), id);
        break;

// font stuff
    case ID_BASEFONT:
        n = new HTMLBaseFontElementImpl(docPtr());
        break;
    case ID_FONT:
        n = new HTMLFontElementImpl(docPtr());
        break;

// ins/del
    case ID_DEL:
    case ID_INS:
        n = new HTMLGenericElementImpl(docPtr(), id);
        break;

// anchor
    case ID_A:
        n = new HTMLAnchorElementImpl(docPtr());
        break;

// images
    case ID_IMG:
        n = new HTMLImageElementImpl(docPtr());
        break;
    case ID_MAP:
        n = new HTMLMapElementImpl(docPtr());
        /*n = map;*/
        break;
    case ID_AREA:
        n = new HTMLAreaElementImpl(docPtr());
        break;

// objects, applets and scripts
    case ID_APPLET:
        n = new HTMLAppletElementImpl(docPtr());
        break;
    case ID_OBJECT:
        n = new HTMLObjectElementImpl(docPtr());
        break;
    case ID_PARAM:
        n = new HTMLParamElementImpl(docPtr());
        break;
    case ID_SCRIPT:
        n = new HTMLScriptElementImpl(docPtr());
        break;

// tables
    case ID_TABLE:
        n = new HTMLTableElementImpl(docPtr());
        break;
    case ID_CAPTION:
        n = new HTMLTableCaptionElementImpl(docPtr());
        break;
    case ID_COLGROUP:
    case ID_COL:
        n = new HTMLTableColElementImpl(docPtr(), id);
        break;
    case ID_TR:
        n = new HTMLTableRowElementImpl(docPtr());
        break;
    case ID_TD:
    case ID_TH:
        n = new HTMLTableCellElementImpl(docPtr(), id);
        break;
    case ID_THEAD:
    case ID_TBODY:
    case ID_TFOOT:
        n = new HTMLTableSectionElementImpl(docPtr(), id, false);
        break;

// inline elements
    case ID_BR:
        n = new HTMLBRElementImpl(docPtr());
        break;
    case ID_Q:
        n = new HTMLGenericElementImpl(docPtr(), id);
        break;

// elements with no special representation in the DOM

// block:
    case ID_ADDRESS:
    case ID_CENTER:
        n = new HTMLGenericElementImpl(docPtr(), id);
        break;
// inline
        // %fontstyle
    case ID_TT:
    case ID_U:
    case ID_B:
    case ID_I:
    case ID_S:
    case ID_STRIKE:
    case ID_BIG:
    case ID_SMALL:

        // %phrase
    case ID_EM:
    case ID_STRONG:
    case ID_DFN:
    case ID_CODE:
    case ID_SAMP:
    case ID_KBD:
    case ID_VAR:
    case ID_CITE:
    case ID_ABBR:
    case ID_ACRONYM:

        // %special
    case ID_SUB:
    case ID_SUP:
    case ID_SPAN:
    case ID_NOBR:
    case ID_WBR:
        n = new HTMLGenericElementImpl(docPtr(), id);
        break;

    case ID_BDO:
        break;

// text
    case ID_TEXT:
        kdDebug( 6020 ) << "Use document->createTextNode()" << endl;
        break;

    default:
        break;
    }
    return n;
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

RangeImpl *DocumentImpl::createRange()
{
    return new RangeImpl( docPtr() );
}

NodeIteratorImpl *DocumentImpl::createNodeIterator(NodeImpl *root, unsigned long whatToShow,
                                                   NodeFilter &filter, bool entityReferenceExpansion,
                                                   int &exceptioncode)
{
    if (!root) {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }

    return new NodeIteratorImpl(root,whatToShow,filter,entityReferenceExpansion);
}

TreeWalkerImpl *DocumentImpl::createTreeWalker(Node /*root*/, unsigned long /*whatToShow*/, NodeFilter &/*filter*/,
                                bool /*entityReferenceExpansion*/)
{
    // ###
    return new TreeWalkerImpl;
}

void DocumentImpl::setDocumentChanged(bool b)
{
    if (!changedDocuments)
        changedDocuments = s_changedDocumentsDeleter.setObject( new QPtrList<DocumentImpl>() );

    if (b && !m_docChanged)
        changedDocuments->append(this);
    else if (!b && m_docChanged)
        changedDocuments->remove(this);
    m_docChanged = b;
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
        RenderStyle* _style = new RenderStyle();
        _style->setDisplay(BLOCK);
        _style->setVisuallyOrdered( visuallyOrdered );
        // ### make the font stuff _really_ work!!!!

	khtml::FontDef fontDef;
	QFont f = KGlobalSettings::generalFont();
	fontDef.family = *(f.firstFamily());
	fontDef.italic = f.italic();
	fontDef.weight = f.weight();
#if APPLE_CHANGES
        fontDef.usePrinterFont = m_paintDevice->devType() == QInternal::Printer;
#endif
        if (m_view) {
            const KHTMLSettings *settings = m_view->part()->settings();
            QString stdfont = settings->stdFontName();
            if ( !stdfont.isEmpty() ) {
                fontDef.family.setFamily(stdfont);
                fontDef.family.appendFamily(0);
            }
            m_styleSelector->setFontSize(fontDef, m_styleSelector->fontSizes()[3]);
        }

        //kdDebug() << "DocumentImpl::attach: setting to charset " << settings->charset() << endl;
        _style->setFontDef(fontDef);
	_style->htmlFont().update( paintDeviceMetrics() );
        if ( inCompatMode() )
            _style->setHtmlHacks(true); // enable html specific rendering tricks

        StyleChange ch = diff( _style, oldStyle );
        if(m_render && ch != NoChange)
            m_render->setStyle(_style);
	else
	    delete _style;
        if ( change != Force )
            change = ch;

        if (oldStyle)
            oldStyle->deref();
    }

    NodeImpl *n;
    for (n = _first; n; n = n->nextSibling())
        if ( change>= Inherit || n->hasChangedChild() || n->changed() )
            n->recalcStyle( change );
    //kdDebug( 6020 ) << "TIME: recalcStyle() dt=" << qt.elapsed() << endl;

    // ### should be done by the rendering tree itself,
    // this way is rather crude and CPU intensive
    if ( changed() ) {
        renderer()->setNeedsLayoutAndMinMaxRecalc();
	renderer()->layout();
	renderer()->repaint();
    }

bail_out:
    setChanged( false );
    setHasChangedChild( false );
    setDocumentChanged( false );
    
    m_inStyleRecalc = false;
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

    while ( !changedDocuments->isEmpty() ) {
        changedDocuments->first();
        DocumentImpl* it = changedDocuments->take();
        if (it->isDocumentChanged())
            it->updateRendering();
    }
}

void DocumentImpl::updateLayout()
{
    updateRendering();

    // Only do a layout if changes have occurred that make it necessary.      
    if (m_view && renderer() && renderer()->needsLayout())
	m_view->layout();
}

void DocumentImpl::attach()
{
    assert(!attached());

    if ( m_view )
        setPaintDevice( m_view );

    if (!m_renderArena)
        m_renderArena = new RenderArena();
    
    // Create the rendering tree
    m_render = new (m_renderArena) RenderCanvas(this, m_view);
    m_styleSelector->computeFontSizes(paintDeviceMetrics());
    recalcStyle( Force );

    RenderObject* render = m_render;
    m_render = 0;

    NodeBaseImpl::attach();
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
        return;
    }
#endif

    // Empty out these lists as a performance optimization, since detaching
    // all the individual render objects will cause all the RenderImage
    // objects to remove themselves from the lists.
    m_imageLoadEventDispatchSoonList.clear();
    m_imageLoadEventDispatchingList.clear();

    NodeBaseImpl::detach();

    if ( render )
        render->detach(m_renderArena);

    if (m_paintDevice == m_view)
        setPaintDevice(0);
    m_view = 0;
    
    if (m_renderArena){
        delete m_renderArena;
        m_renderArena = 0;
    }
}

void DocumentImpl::setVisuallyOrdered()
{
    visuallyOrdered = true;
    if (m_render)
        m_render->style()->setVisuallyOrdered(true);
}

void DocumentImpl::setSelection(NodeImpl* s, int sp, NodeImpl* e, int ep)
{
    if ( m_render )
        static_cast<RenderCanvas*>(m_render)->setSelection(s->renderer(),sp,e->renderer(),ep);
}

void DocumentImpl::clearSelection()
{
    if ( m_render )
        static_cast<RenderCanvas*>(m_render)->clearSelection();
}

Tokenizer *DocumentImpl::createTokenizer()
{
    return new XMLTokenizer(docPtr(),m_view);
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

    if (m_tokenizer)
        close();

    clear();
    m_tokenizer = createTokenizer();
    connect(m_tokenizer,SIGNAL(finishedParsing()),this,SIGNAL(finishedParsing()));
    m_tokenizer->begin();

    if (m_view && m_view->part()->jScript())
        m_view->part()->jScript()->setSourceFile(m_url,"");
}

void DocumentImpl::close(  )
{
    closeInternal(true);
}

void DocumentImpl::closeInternal( bool checkTokenizer )
{
    if (parsing() || (checkTokenizer && !m_tokenizer)) return;

    if ( m_render )
        m_render->close();

    delete m_tokenizer;
    m_tokenizer = 0;

    if (m_view)
        m_view->part()->checkEmitLoadEvent();
}

void DocumentImpl::write( const DOMString &text )
{
    write(text.string());
}

void DocumentImpl::write( const QString &text )
{
    if (!m_tokenizer) {
        open();
        write(QString::fromLatin1("<html>"));
    }
    m_tokenizer->write(text, false);

    if (m_view && m_view->part()->jScript())
        m_view->part()->jScript()->appendSourceFile(m_url,text);
}

void DocumentImpl::writeln( const DOMString &text )
{
    write(text);
    write(DOMString("\n"));
}

void DocumentImpl::finishParsing (  )
{
    if(m_tokenizer)
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

void DocumentImpl::setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheet)
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

// Please see if there`s a possibility to merge that code
// with the next function and getElementByID().
NodeImpl *DocumentImpl::findElement( Id id )
{
    QPtrStack<NodeImpl> nodeStack;
    NodeImpl *current = _first;

    while(1)
    {
        if(!current)
        {
            if(nodeStack.isEmpty()) break;
            current = nodeStack.pop();
            current = current->nextSibling();
        }
        else
        {
            if(current->id() == id)
                return current;

            NodeImpl *child = current->firstChild();
            if(child)
            {
                nodeStack.push(current);
                current = child;
            }
            else
            {
                current = current->nextSibling();
            }
        }
    }

    return 0;
}

NodeImpl *DocumentImpl::nextFocusNode(NodeImpl *fromNode)
{
    unsigned short fromTabIndex;

    if (!fromNode) {
	// No starting node supplied; begin with the top of the document
	NodeImpl *n;

	int lowestTabIndex = 65535;
	for (n = this; n != 0; n = n->traverseNextNode()) {
	    if (n->isSelectable()) {
		if ((n->tabIndex() > 0) && (n->tabIndex() < lowestTabIndex))
		    lowestTabIndex = n->tabIndex();
	    }
	}

	if (lowestTabIndex == 65535)
	    lowestTabIndex = 0;

	// Go to the first node in the document that has the desired tab index
	for (n = this; n != 0; n = n->traverseNextNode()) {
	    if (n->isSelectable() && (n->tabIndex() == lowestTabIndex))
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
	while (n && !(n->isSelectable() && n->tabIndex() == 0))
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
	    if (n->isSelectable() &&
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
	    while (n && !(n->isSelectable() && n->tabIndex() == 0))
		n = n->traverseNextNode();
	    return n;
	}

	// Search forwards from fromNode
	for (n = fromNode->traverseNextNode(); n != 0; n = n->traverseNextNode()) {
	    if (n->isSelectable() && (n->tabIndex() == lowestSuitableTabIndex))
		return n;
	}

	// The next node isn't after fromNode, start from the beginning of the document
	for (n = this; n != fromNode; n = n->traverseNextNode()) {
	    if (n->isSelectable() && (n->tabIndex() == lowestSuitableTabIndex))
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
	    if (n->isSelectable()) {
		if (n->tabIndex() == 0)
		    return n;
		else if (n->tabIndex() > highestTabIndex)
		    highestTabIndex = n->tabIndex();
	    }
	}

	// No node with a tab index of 0; just go to the last node with the highest tab index
	for (n = lastNode; n != 0; n = n->traversePreviousNode()) {
	    if (n->isSelectable() && (n->tabIndex() == highestTabIndex))
		return n;
	}

	return 0;
    }
    else {
	unsigned short fromTabIndex = fromNode->tabIndex();

	if (fromTabIndex == 0) {
	    // Find the previous selectable node before fromNode (in document order) that doesn't have a tab index
	    NodeImpl *n = fromNode->traversePreviousNode();
	    while (n && !(n->isSelectable() && n->tabIndex() == 0))
		n = n->traversePreviousNode();
	    if (n)
		return n;

	    // No previous nodes with a 0 tab index, go to the last node in the document that has the highest tab index
	    int highestTabIndex = 0;
	    for (n = this; n != 0; n = n->traverseNextNode()) {
		if (n->isSelectable() && (n->tabIndex() > highestTabIndex))
		    highestTabIndex = n->tabIndex();
	    }

	    if (highestTabIndex == 0)
		return 0;

	    for (n = lastNode; n != 0; n = n->traversePreviousNode()) {
		if (n->isSelectable() && (n->tabIndex() == highestTabIndex))
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
		if (n->isSelectable() &&
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
		if (n->isSelectable() && (n->tabIndex() == highestSuitableTabIndex))
		    return n;
	    }
	    // The previous node isn't before fromNode, start from the end of the document
	    for (n = lastNode; n != fromNode; n = n->traversePreviousNode()) {
		if (n->isSelectable() && (n->tabIndex() == highestSuitableTabIndex))
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

    KHTMLView *v = getDocument()->view();

    if (strcasecmp(equiv, "default-style") == 0) {
        // The preferred style set has been overridden as per section 
        // 14.3.2 of the HTML4.0 specification.  We need to update the
        // sheet used variable and then update our style selector. 
        // For more info, see the test at:
        // http://www.hixie.ch/tests/evil/css/import/main/preferred.html
        // -dwh
        v->part()->d->m_sheetUsed = content.string();
        m_preferredStylesheetSet = content;
        updateStyleSelector();
    }
    else if(strcasecmp(equiv, "refresh") == 0 && v->part()->metaRefreshEnabled())
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
            if(ok) v->part()->scheduleRedirection(delay, v->part()->url().url(), delay <= 1);
#else
            if(ok) v->part()->scheduleRedirection(delay, v->part()->url().url() );
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
            if ( ok )
#if APPLE_CHANGES
                // We want a new history item if the refresh timeout > 1 second
                v->part()->scheduleRedirection(delay, getDocument()->completeURL( str ), delay <= 1);
#else
                v->part()->scheduleRedirection(delay, getDocument()->completeURL( str ));
#endif
        }
    }
    else if(strcasecmp(equiv, "expires") == 0)
    {
        QString str = content.string().stripWhiteSpace();
        time_t expire_date = str.toLong();
        KURL url = v->part()->url();
        if (m_docLoader)
            m_docLoader->setExpireDate(expire_date);
    }
    else if(strcasecmp(equiv, "pragma") == 0 || strcasecmp(equiv, "cache-control") == 0)
    {
        QString str = content.string().lower().stripWhiteSpace();
        KURL url = v->part()->url();
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
        bool isInside = m_render->layer()->nodeAtPoint(renderInfo, _x, _y);
        ev->innerNode = renderInfo.innerNode();

        if (renderInfo.URLElement()) {
            assert(renderInfo.URLElement()->isElementNode());
            ElementImpl* e =  static_cast<ElementImpl*>(renderInfo.URLElement());
            DOMString href = khtml::parseURL(e->getAttribute(ATTR_HREF));
            DOMString target = e->getAttribute(ATTR_TARGET);

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
            break;
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

NodeImpl::Id DocumentImpl::attrId(DOMStringImpl* _namespaceURI, DOMStringImpl *_name, bool readonly)
{
    // Each document maintains a mapping of attrname -> id for every attr name
    // encountered in the document.
    // For attrnames without a prefix (no qualified element name) and without matching
    // namespace, the value defined in misc/htmlattrs.h is used.
    NodeImpl::Id id = 0;

    // First see if it's a HTML attribute name
    QConstString n(_name->s, _name->l);
    if (!_namespaceURI || !strcasecmp(_namespaceURI, XHTML_NAMESPACE)) {
        // we're in HTML namespace if we know the tag.
        // xhtml is lower case - case sensitive, easy to implement
        if ( htmlMode() == XHtml && (id = khtml::getAttrID(n.string().ascii(), _name->l)) )
            return id;
        // compatibility: upper case - case insensitive
        if ( htmlMode() != XHtml && (id = khtml::getAttrID(n.string().lower().ascii(), _name->l )) )
            return id;

        // ok, the fast path didn't work out, we need the full check
    }

    // now lets find out the namespace
    if (_namespaceURI) {
        DOMString nsU(_namespaceURI);
        bool found = false;
        // ### yeah, this is lame. use a dictionary / map instead
        for (unsigned short ns = 0; ns < m_namespaceURICount; ++ns)
            if (nsU == DOMString(m_namespaceURIs[ns])) {
                id |= ns << 16;
                found = true;
                break;
            }

        if (!found && !readonly) {
            // something new, add it
            if (m_namespaceURICount >= m_namespaceURIAlloc) {
                m_namespaceURIAlloc += 32;
                DOMStringImpl **newURIs = new DOMStringImpl* [m_namespaceURIAlloc];
                for (unsigned short i = 0; i < m_namespaceURICount; i++)
                    newURIs[i] = m_namespaceURIs[i];
                delete [] m_namespaceURIs;
                m_namespaceURIs = newURIs;
            }
            m_namespaceURIs[m_namespaceURICount++] = _namespaceURI;
            _namespaceURI->ref();
            id |= m_namespaceURICount << 16;
        }
    }

    // Look in the m_attrNames array for the name
    // ### yeah, this is lame. use a dictionary / map instead
    DOMString nme(n.string());
    // compatibility mode has to store upper case
    if (htmlMode() != XHtml) nme = nme.upper();
    for (id = 0; id < m_attrNameCount; id++)
        if (DOMString(m_attrNames[id]) == nme)
            return ATTR_LAST_ATTR+id;

    // unknown
    if (readonly) return 0;

    // Name not found in m_attrNames, so let's add it
    // ### yeah, this is lame. use a dictionary / map instead
    if (m_attrNameCount+1 > m_attrNameAlloc) {
        m_attrNameAlloc += 100;
        DOMStringImpl **newNames = new DOMStringImpl* [m_attrNameAlloc];
        if (m_attrNames) {
            unsigned short i;
            for (i = 0; i < m_attrNameCount; i++)
                newNames[i] = m_attrNames[i];
            delete [] m_attrNames;
        }
        m_attrNames = newNames;
    }

    id = m_attrNameCount++;
    m_attrNames[id] = nme.implementation();
    m_attrNames[id]->ref();

    return ATTR_LAST_ATTR+id;
}

DOMString DocumentImpl::attrName(NodeImpl::Id _id) const
{
    DOMString result;
    if (_id >= ATTR_LAST_ATTR)
        result = m_attrNames[_id-ATTR_LAST_ATTR];
    else
        result = getAttrName(_id);

    // Attribute names are always lowercase in the DOM for both
    // HTML and XHTML.
    if (getDocument()->isHTMLDocument() ||
        getDocument()->htmlMode() == DocumentImpl::XHtml)
        return result.lower();

    return result;
}

NodeImpl::Id DocumentImpl::tagId(DOMStringImpl* _namespaceURI, DOMStringImpl *_name, bool readonly)
{
    if (!_name) return 0;
    // Each document maintains a mapping of tag name -> id for every tag name encountered
    // in the document.
    NodeImpl::Id id = 0;

    // First see if it's a HTML element name
    QConstString n(_name->s, _name->l);
    if (!_namespaceURI || !strcasecmp(_namespaceURI, XHTML_NAMESPACE)) {
        // we're in HTML namespace if we know the tag.
        // xhtml is lower case - case sensitive, easy to implement
        if ( htmlMode() == XHtml && (id = khtml::getTagID(n.string().ascii(), _name->l)) )
            return id;
        // compatibility: upper case - case insensitive
        if ( htmlMode() != XHtml && (id = khtml::getTagID(n.string().lower().ascii(), _name->l )) )
            return id;

        // ok, the fast path didn't work out, we need the full check
    }

    // now lets find out the namespace
    if (_namespaceURI) {
        DOMString nsU(_namespaceURI);
        bool found = false;
        // ### yeah, this is lame. use a dictionary / map instead
        for (unsigned short ns = 0; ns < m_namespaceURICount; ++ns)
            if (nsU == DOMString(m_namespaceURIs[ns])) {
                id |= ns << 16;
                found = true;
                break;
            }

        if (!found && !readonly) {
            // something new, add it
            if (m_namespaceURICount >= m_namespaceURIAlloc) {
                m_namespaceURIAlloc += 32;
                DOMStringImpl **newURIs = new DOMStringImpl* [m_namespaceURIAlloc];
                for (unsigned short i = 0; i < m_namespaceURICount; i++)
                    newURIs[i] = m_namespaceURIs[i];
                delete [] m_namespaceURIs;
                m_namespaceURIs = newURIs;
            }
            m_namespaceURIs[m_namespaceURICount++] = _namespaceURI;
            _namespaceURI->ref();
            id |= m_namespaceURICount << 16;
        }
    }

    // Look in the m_elementNames array for the name
    // ### yeah, this is lame. use a dictionary / map instead
    DOMString nme(n.string());
    // compatibility mode has to store upper case
    if (htmlMode() != XHtml) nme = nme.upper();
    for (id = 0; id < m_elementNameCount; id++)
        if (DOMString(m_elementNames[id]) == nme)
            return ID_LAST_TAG+id;

    // unknown
    if (readonly) return 0;

    // Name not found in m_elementNames, so let's add it
    if (m_elementNameCount+1 > m_elementNameAlloc) {
        m_elementNameAlloc += 100;
        DOMStringImpl **newNames = new DOMStringImpl* [m_elementNameAlloc];
        // ### yeah, this is lame. use a dictionary / map instead
        if (m_elementNames) {
            unsigned short i;
            for (i = 0; i < m_elementNameCount; i++)
                newNames[i] = m_elementNames[i];
            delete [] m_elementNames;
        }
        m_elementNames = newNames;
    }

    id = m_elementNameCount++;
    m_elementNames[id] = nme.implementation();
    m_elementNames[id]->ref();

    return ID_LAST_TAG+id;
}

DOMString DocumentImpl::tagName(NodeImpl::Id _id) const
{
    if (_id >= ID_LAST_TAG)
        return m_elementNames[_id-ID_LAST_TAG];
    else {
        // ### put them in a cache
        if (getDocument()->htmlMode() == DocumentImpl::XHtml)
            return getTagName(_id).lower();
        else
            return getTagName(_id);
    }
}


DOMStringImpl* DocumentImpl::namespaceURI(NodeImpl::Id _id) const
{
    if (_id < ID_LAST_TAG)
        return htmlMode() == XHtml ? m_namespaceURIs[0] : 0;

    unsigned short ns = _id >> 16;

    if (!ns) return 0;

    return m_namespaceURIs[ns-1];
}

StyleSheetListImpl* DocumentImpl::styleSheets()
{
    return m_styleSheets;
}

DOMString 
DocumentImpl::preferredStylesheetSet()
{
  return m_preferredStylesheetSet;
}

DOMString 
DocumentImpl::selectedStylesheetSet()
{
  return view()->part()->d->m_sheetUsed;
}

void 
DocumentImpl::setSelectedStylesheetSet(const DOMString& aString)
{
  view()->part()->d->m_sheetUsed = aString.string();
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
  updateStyleSelector();    
}

void DocumentImpl::updateStyleSelector()
{
    // Don't bother updating, since we haven't loaded all our style info yet.
    if (m_pendingStylesheets > 0)
        return;

    recalcStyleSelector();
    recalcStyle(Force);
#if 0

    m_styleSelectorDirty = true;
#endif
    if (renderer())
        renderer()->setNeedsLayoutAndMinMaxRecalc();
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
        else if (n->isHTMLElement() && (n->id() == ID_LINK || n->id() == ID_STYLE)) {
            ElementImpl *e = static_cast<ElementImpl *>(n);
            QString title = e->getAttribute( ATTR_TITLE ).string();
            bool enabledViaScript = false;
            if (n->id() == ID_LINK) {
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
            if ( n->id() == ID_LINK )
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
                    QString rel = e->getAttribute( ATTR_REL ).string();
                    if (n->id() == ID_STYLE || !rel.contains("alternate"))
                        m_preferredStylesheetSet = view()->part()->d->m_sheetUsed = title;
                }
                      
                if (!m_availableSheets.contains( title ) )
                    m_availableSheets.append( title );
                
                if (title != m_preferredStylesheetSet)
                    sheet = 0;
            }
        }
        else if (n->isHTMLElement() && n->id() == ID_BODY) {
                // <BODY> element (doesn't contain styles as such but vlink="..." and friends
                // are treated as style declarations)
            sheet = static_cast<HTMLBodyElementImpl*>(n)->sheet();
        }
            
        if (sheet) {
            sheet->ref();
            m_styleSheets->styleSheets.append(sheet);
        }
    
        // For HTML documents, stylesheets are not allowed within/after the <BODY> tag. So we
        // can stop searching here.
        if (isHTMLDocument() && n->id() == ID_BODY)
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
    m_styleSelector = new CSSStyleSelector( this, usersheet, m_styleSheets, m_url,
                                            !inCompatMode() );

    m_styleSelectorDirty = false;
}

void DocumentImpl::setFocusNode(NodeImpl *newFocusNode)
{    
    // Make sure newFocusNode is actually in this document
    if (newFocusNode && (newFocusNode->getDocument() != this))
        return;

    if (m_focusNode != newFocusNode) {
        NodeImpl *oldFocusNode = m_focusNode;
        // Set focus on the new node
        m_focusNode = newFocusNode;
        // Remove focus from the existing focus node (if any)
        if (oldFocusNode) {
            // This goes hand in hand with the Qt focus setting below.
            if (!m_focusNode && getDocument()->view()) {
                getDocument()->view()->setFocus();
            }

            if (oldFocusNode->active())
                oldFocusNode->setActive(false);

            oldFocusNode->setFocus(false);
	    oldFocusNode->dispatchHTMLEvent(EventImpl::BLUR_EVENT,false,false);
	    oldFocusNode->dispatchUIEvent(EventImpl::DOMFOCUSOUT_EVENT);
            if ((oldFocusNode == this) && oldFocusNode->hasOneRef()) {
                oldFocusNode->deref(); // deletes this
                return;
            }
	    else {
                oldFocusNode->deref();
            }
        }

        if (m_focusNode) {
            m_focusNode->ref();
            m_focusNode->dispatchHTMLEvent(EventImpl::FOCUS_EVENT,false,false);
            if (m_focusNode != newFocusNode) return;
            m_focusNode->dispatchUIEvent(EventImpl::DOMFOCUSIN_EVENT);
            if (m_focusNode != newFocusNode) return;
            m_focusNode->setFocus();
            // eww, I suck. set the qt focus correctly
            // ### find a better place in the code for this
            if (getDocument()->view()) {
                if (!m_focusNode->renderer() || !m_focusNode->renderer()->isWidget())
                    getDocument()->view()->setFocus();
                else if (static_cast<RenderWidget*>(m_focusNode->renderer())->widget())
                    static_cast<RenderWidget*>(m_focusNode->renderer())->widget()->setFocus();
            }
        }

        updateRendering();
    }
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
    if (eventType == "UIEvents")
        return new UIEventImpl();
    else if (eventType == "MouseEvents")
        return new MouseEventImpl();
    else if (eventType == "MutationEvents")
        return new MutationEventImpl();
    else if (eventType == "HTMLEvents")
        return new EventImpl();
    else {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }
}

CSSStyleDeclarationImpl *DocumentImpl::getOverrideStyle(ElementImpl */*elt*/, DOMStringImpl */*pseudoElt*/)
{
    return 0; // ###
}

void DocumentImpl::defaultEventHandler(EventImpl *evt)
{
    // if any html event listeners are registered on the window, then dispatch them here
    QPtrListIterator<RegisteredEventListener> it(m_windowEventListeners);
    Event ev(evt);
    for (; it.current(); ++it) {
        if (it.current()->id == evt->id()) {
            it.current()->listener->handleEvent(ev, true);
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

EventListener *DocumentImpl::createHTMLEventListener(QString code)
{
    return view()->part()->createHTMLEventListener(code);
}

void DocumentImpl::dispatchImageLoadEventSoon(RenderImage *image)
{
    m_imageLoadEventDispatchSoonList.append(image);
    if (!m_imageLoadEventTimer) {
        m_imageLoadEventTimer = startTimer(0);
    }
}

void DocumentImpl::removeImage(RenderImage *image)
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
    if (m_imageLoadEventTimer) {
        killTimer(m_imageLoadEventTimer);
        m_imageLoadEventTimer = 0;
    }
    
    m_imageLoadEventDispatchingList = m_imageLoadEventDispatchSoonList;
    m_imageLoadEventDispatchSoonList.clear();
    for (QPtrListIterator<RenderImage> it(m_imageLoadEventDispatchingList); it.current(); ) {
        RenderImage *image = it.current();
        // Must advance iterator *before* dispatching call.
        // Otherwise, it might be advanced automatically if dispatching the call had a side effect
        // of destroying the current RenderImage, and then we would advance past the *next* item,
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
    ChildFrame *childFrame = parent->frame(childPart);
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

bool DocumentImpl::inPageCache()
{
    return m_inPageCache;
}

void DocumentImpl::setInPageCache(bool flag)
{
    m_inPageCache = flag;
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

#endif // APPLE_CHANGES

// ----------------------------------------------------------------------------

DocumentFragmentImpl::DocumentFragmentImpl(DocumentPtr *doc) : NodeBaseImpl(doc)
{
}

DocumentFragmentImpl::DocumentFragmentImpl(const DocumentFragmentImpl &other)
    : NodeBaseImpl(other)
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
            break;
        default:
            return false;
    }
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
    m_implementation->ref();

    m_entities = 0;
    m_notations = 0;

    // if doc is 0, it is not attached to a document and / or
    // therefore does not provide entities or notations. (DOM Level 3)
}

DocumentTypeImpl::~DocumentTypeImpl()
{
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
