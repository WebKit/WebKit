/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
 *
 * $Id$
 */

#include "dom_docimpl.h"

#include "dom_node.h"
#include "dom_elementimpl.h"
#include "dom_textimpl.h"
#include "dom_exception.h"
#include "dom_xmlimpl.h"
#include "dom2_rangeimpl.h"
#include "dom2_traversalimpl.h"
#include "dom2_viewsimpl.h"
#include "dom2_eventsimpl.h"

#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include "misc/helper.h"
#include "css/csshelper.h"

#include <qstring.h>
#include <qstack.h>
#include <qlist.h>
#include <qpaintdevicemetrics.h>
#include "misc/htmlhashes.h"
#include "misc/loader.h"
#include <kdebug.h>

#include "htmltokenizer.h"
#include "xml_tokenizer.h"

#include "rendering/render_object.h"
#include "rendering/render_root.h"
#include "rendering/render_style.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include <kglobal.h>
#include <kcharsets.h>
#include <kglobalsettings.h>
#include "khtml_settings.h"

#include "html_baseimpl.h"
#include "html_blockimpl.h"
#include "html_documentimpl.h"
#include "html_elementimpl.h"
#include "html_formimpl.h"
#include "html_headimpl.h"
#include "html_imageimpl.h"
#include "html_inlineimpl.h"
#include "html_listimpl.h"
#include "html_miscimpl.h"
#include "html_tableimpl.h"
#include "html_objectimpl.h"

using namespace DOM;
using namespace khtml;

//template class QStack<DOM::NodeImpl>; // needed ?


DOMImplementationImpl::DOMImplementationImpl()
{
}

DOMImplementationImpl::~DOMImplementationImpl()
{
}

bool DOMImplementationImpl::hasFeature ( const DOMString &feature, const DOMString &version )
{
    QString lower = feature.string().lower();
    if ((lower == "html" || lower == "xml") &&
        (version == "1.0" || version == "" || version.isNull()))
        return true;
    else
        return false;
}

CSSStyleSheetImpl *DOMImplementationImpl::createCSSStyleSheet(DOMStringImpl */*title*/, DOMStringImpl */*media*/)
{
    return 0; // ###
}

// ------------------------------------------------------------------------

// KHTMLView might be 0
DocumentImpl::DocumentImpl(KHTMLView *v)
    : NodeBaseImpl( new DocumentPtr() )
{
    document->doc = this;
    m_styleSelector = 0;
    m_paintDeviceMetrics = 0;

    m_view = v;

    if ( v ) {
        m_docLoader = new DocLoader(v->part());
        setPaintDevice( m_view );
    }
    else
        m_docLoader = new DocLoader( 0 );

    visuallyOrdered = false;
    m_loadingSheet = false;
    m_sheet = 0;
    m_elemSheet = 0;
    m_tokenizer = 0;
    m_doctype = new DocumentTypeImpl( docPtr() );
    m_doctype->ref();
    m_implementation = new DOMImplementationImpl();
    m_implementation->ref();
    pMode = Strict;
    m_textColor = "#000000";
    m_elementNames = 0;
    m_elementNameAlloc = 0;
    m_elementNameCount = 0;
    m_focusNode = 0;
    m_defaultView = new AbstractViewImpl(this);
    m_defaultView->ref();
    m_listenerTypes = 0;
    m_styleSheets = new StyleSheetListImpl;
    m_styleSheets->ref();
}

DocumentImpl::~DocumentImpl()
{
    document->doc = 0;
    delete m_sheet;
    delete m_styleSelector;
    delete m_docLoader;
    if (m_elemSheet )  m_elemSheet->deref();
    delete m_tokenizer;
    m_doctype->deref();
    m_implementation->deref();
    delete m_paintDeviceMetrics;

    if (m_elementNames) {
        unsigned short id;
        for (id = 0; id < m_elementNameCount; id++) {
            m_elementNames[id]->deref();
        }
        delete [] m_elementNames;
    }
    m_defaultView->deref();
    m_styleSheets->deref();
}

const DOMString DocumentImpl::nodeName() const
{
    return "#document";
}

unsigned short DocumentImpl::nodeType() const
{
    return Node::DOCUMENT_NODE;
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

ElementImpl *DocumentImpl::createElementNS ( const DOMString &_namespaceURI, const DOMString &_qualifiedName )
{
    ElementImpl *e = 0;
    // ### somehow set the namespace for html elements to http://www.w3.org/1999/xhtml ?
    if (_namespaceURI == "http://www.w3.org/1999/xhtml") {
        QString qName = _qualifiedName.string();
        int colonPos = qName.find(':',0);
        e = createHTMLElement(colonPos ? qName.mid(colonPos+1) : qName);
    }
    if (!e)
        e = new XMLElementImpl( document, _qualifiedName.implementation(), _namespaceURI.implementation() );
    return e;
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
        n = new HTMLPreElementImpl(docPtr());
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
        n = new HTMLModElementImpl(docPtr(), id);
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
        n = new HTMLTableSectionElementImpl(docPtr(), id);
        break;

// inline elements
    case ID_BR:
        n = new HTMLBRElementImpl(docPtr());
        break;
    case ID_Q:
        n = new HTMLQuoteElementImpl(docPtr());
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

// Used to maintain list of all forms in document
QString DocumentImpl::registerElement(ElementImpl *e)
{
   m_registeredElements.append(e);
   QString state;
   if (!m_state.isEmpty())
   {
      state = m_state.first();
      m_state.remove(m_state.begin());
   }
   return state;
}

// Used to maintain list of all forms in document
void DocumentImpl::removeElement(ElementImpl *e)
{
   m_registeredElements.removeRef(e);
}

QStringList DocumentImpl::state()
{
   QStringList s;
   for( ElementImpl *e = m_registeredElements.first();
        e; e = m_registeredElements.next())
   {
       s.append(e->state());
   }
   return s;
}


RangeImpl *DocumentImpl::createRange()
{
    return new RangeImpl( docPtr() );
}

NodeIteratorImpl *DocumentImpl::createNodeIterator(NodeImpl *root, unsigned long whatToShow,
                                                   NodeFilter filter, bool entityReferenceExpansion,
                                                   int &exceptioncode)
{
    if (!root) {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return 0;
    }

    return new NodeIteratorImpl(root,whatToShow,filter,entityReferenceExpansion);
}

TreeWalkerImpl *DocumentImpl::createTreeWalker(Node /*root*/, unsigned long /*whatToShow*/, NodeFilter /*filter*/,
                                bool /*entityReferenceExpansion*/)
{
    return new TreeWalkerImpl;
}

void DocumentImpl::applyChanges(bool,bool force)
{
    // ### move the following two lines to createSelector????
    delete m_styleSelector;
    m_styleSelector = 0;

    m_styleSelector = new CSSStyleSelector(this);
    if(!m_render) return;

    recalcStyle();

    // a style change can influence the children, so we just go
    // through them and trigger an appplyChanges there too
    NodeImpl *n = _first;
    while(n) {
        n->applyChanges(false,force || changed());
        n = n->nextSibling();
    }

    // force a relayout of this part of the document
    m_render->layout();
    // force a repaint of this part.
    // ### if updateSize() changes any size, it will already force a
    // repaint, so we might do double work here...
    m_render->repaint();
    setChanged(false);
}

DocumentTypeImpl *DocumentImpl::doctype() const
{
    return m_doctype;
}

DOMImplementationImpl *DocumentImpl::implementation() const
{
    return m_implementation;
}


void DocumentImpl::setChanged(bool b)
{
    if (b)
        changedNodes.append(this);
    NodeBaseImpl::setChanged(b);
}

void DocumentImpl::recalcStyle()
{
    QTime qt;
    qt.start();
    if( !m_render ) return;
    setStyle(new RenderStyle());
    m_style->setDisplay(BLOCK);
    m_style->setVisuallyOrdered( visuallyOrdered );
    // ### make the font stuff _really_ work!!!!

    QFont f = KGlobalSettings::generalFont();
    if (m_view)
    {
        const KHTMLSettings *settings = m_view->part()->settings();
        f.setFamily(settings->stdFontName());

        QValueList<int> fs = settings->fontSizes();
        float dpiY = 72.; // fallback
        if ( !khtml::printpainter )
            dpiY = paintDeviceMetrics()->logicalDpiY();
        if ( !khtml::printpainter && dpiY < 96 )
            dpiY = 96.;
        float size = fs[3] * dpiY / 72.;
        if(size < settings->minFontSize())
            size = settings->minFontSize();

        khtml::setFontSize( f, int(size),  settings, paintDeviceMetrics() );
        KGlobal::charsets()->setQFont(f, settings->charset());
    }

    //kdDebug() << "DocumentImpl::attach: setting to charset " << settings->charset() << endl;
    m_style->setFont(f);

    if ( parseMode() != Strict )
        m_style->setHtmlHacks(true); // enable html specific rendering tricks
    if(m_render)
        m_render->setStyle(m_style);

    NodeImpl *n;
    for (n = _first; n; n = n->nextSibling())
        n->recalcStyle();
    //kdDebug( 6020 ) << "TIME: recalcStyle() dt=" << qt.elapsed() << endl;
}



// ------------------------------------------------------------------------

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

AttrImpl *DocumentImpl::createAttribute( const DOMString &name )
{
    AttrImpl *attr = new AttrImpl( docPtr(), name );
    attr->setValue("");
    return attr;
}

AttrImpl *DocumentImpl::createAttributeNS( const DOMString &/*_namespaceURI*/, const DOMString &/*_qualifiedName*/ )
{
    // ###
    return 0;
}


EntityReferenceImpl *DocumentImpl::createEntityReference ( const DOMString &name )
{
    return new EntityReferenceImpl(docPtr(), name.implementation());
}

NodeListImpl *DocumentImpl::getElementsByTagName( const DOMString &tagname )
{
    return new TagNodeListImpl( this, tagname );
}

void DocumentImpl::updateRendering()
{
//    int o=changedNodes.count();
//    int a=0;
    QListIterator<NodeImpl> it(changedNodes);
    for (; it.current(); ) {
        // applyChanges removes current from the list
	    it.current()->applyChanges( true, true );
//        a++;
    }
//    kdDebug() << "UpdateRendering orig="<<o<<" actual="<<a<<endl;
    changedNodes.clear();
}

void DocumentImpl::attach(KHTMLView *w)
{
    m_view = w;
    if ( m_view ) {
        m_docLoader->m_part = w->part();
        setPaintDevice( m_view );
    }
    if(!m_styleSelector) createSelector();
    m_render = new RenderRoot(w);
    recalcStyle();

    NodeBaseImpl::attach();
}

void DocumentImpl::detach()
{
    NodeBaseImpl::detach();

    if ( m_render )
        m_render->detach();

    m_render = 0;
    m_view = 0;
}

void DocumentImpl::setVisuallyOrdered()
{
    visuallyOrdered = true;
    if(!m_style) return;
    m_style->setVisuallyOrdered(true);
}

void DocumentImpl::setSelection(NodeImpl* s, int sp, NodeImpl* e, int ep)
{
    if ( m_render )
        static_cast<RenderRoot*>(m_render)->setSelection(s->renderer(),sp,e->renderer(),ep);
}

void DocumentImpl::clearSelection()
{
    if ( m_render )
        static_cast<RenderRoot*>(m_render)->clearSelection();
}

Tokenizer *DocumentImpl::createTokenizer()
{
    return new XMLTokenizer(docPtr(),m_view);
}

void DocumentImpl::setPaintDevice( QPaintDevice *dev )
{
    m_paintDevice = dev;
    delete m_paintDeviceMetrics;
    m_paintDeviceMetrics = new QPaintDeviceMetrics( dev );
}

void DocumentImpl::open(  )
{
    clear();
    m_tokenizer = createTokenizer();
    connect(m_tokenizer,SIGNAL(finishedParsing()),this,SIGNAL(finishedParsing()));
    m_tokenizer->begin();
}

void DocumentImpl::close(  )
{
    for ( RenderObject* o = m_render; o; o = o->lastChild() )
        o->setParsing( false );
    if ( m_render )
        m_render->close();

    delete m_tokenizer;
    m_tokenizer = 0;
}

void DocumentImpl::write( const DOMString &text )
{
    if(m_tokenizer)
        m_tokenizer->write(text.string(), false);
}

void DocumentImpl::write( const QString &text )
{
    if(m_tokenizer)
        m_tokenizer->write(text, false);
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
}

ElementImpl *DocumentImpl::getElementById( const DOMString &elementId )
{
    QStack<NodeImpl> nodeStack;
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

    return 0;
}

DOMString DocumentImpl::baseURL() const
{
    if(view() && !view()->part()->baseURL().isEmpty()) return view()->part()->baseURL().url();
    return url;
}

void DocumentImpl::setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheet)
{
    kdDebug( 6030 ) << "HTMLDocument::setStyleSheet()" << endl;
    m_sheet = new CSSStyleSheetImpl(this, url);
    m_sheet->ref();
    m_sheet->parseString(sheet);
    m_loadingSheet = false;

    createSelector();
}

void DocumentImpl::setUserStyleSheet( const QString& sheet )
{
    if ( m_usersheet != sheet ) {
        m_usersheet = sheet;
        applyChanges();
    }
}

CSSStyleSheetImpl* DocumentImpl::elementSheet()
{
    if (!m_elemSheet) {
        m_elemSheet = new CSSStyleSheetImpl(this, baseURL());
        m_elemSheet->ref();
    }
    return m_elemSheet;
}

static bool isTransitional(const QString &spec, int start)
{
    if((spec.find("TRANSITIONAL", start, false ) != -1 ) ||
       (spec.find("LOOSE", start, false ) != -1 ) ||
       (spec.find("FRAMESET", start, false ) != -1 ) ||
       (spec.find("LATIN1", start, false ) != -1 ) ||
       (spec.find("SYMBOLS", start, false ) != -1 ) ||
       (spec.find("SPECIAL", start, false ) != -1 ) ) {
        //kdDebug() << "isTransitional" << endl;
        return true;
    }
    return false;
}

enum HTMLMode {
    Html3 = 0,
    Html4 = 1,
    XHtml = 2
};

void DocumentImpl::determineParseMode( const QString &str )
{
    // determines the parse mode for HTML
    // quite some hints here are inspired by the mozilla code.

    // default parsing mode is Loose
    pMode = Compat;

    ParseMode systemId = Unknown;
    ParseMode publicId = Unknown;
    HTMLMode htmlMode = Html3;

    int pos = 0;
    int doctype = str.find("!doctype", 0, false);
    if( doctype > 2 )
        pos = doctype - 2;

    // get the first tag (or the doctype tag
    int start = str.find('<', pos);
    int stop = str.find('>', pos);
    if( start > -1 && stop > start ) {
        QString spec = str.mid( start + 1, stop - start - 1 );
        //kdDebug() << "DocumentImpl::determineParseMode dtd=" << spec<< endl;
        start = 0;
        int quote = -1;
        if( doctype != -1 ) {
            while( (quote = spec.find( "\"", start )) != -1 ) {
                int quote2 = spec.find( "\"", quote+1 );
                if(quote2 < 0) quote2 = spec.length();
                QString val = spec.mid( quote+1, quote2 - quote-1 );
                //kdDebug() << "DocumentImpl::determineParseMode val = " << val << endl;
                // find system id
                pos = val.find("http://www.w3.org/tr/", 0, false);
                if ( pos != -1 ) {
                    // loose or strict dtd?
                    if ( val.find("strict.dtd", pos, false) != -1 )
                        systemId = Strict;
                    else if (isTransitional(val, pos))
                        systemId = Transitional;
                }

                // find public id
                pos = val.find("//dtd", 0, false );
                if ( pos != -1 ) {
                    if( val.find( "xhtml", pos+6, false ) != -1 ) {
                        htmlMode = XHtml;
                        if( isTransitional( val, pos ) )
                            publicId = Transitional;
                        else
                            publicId = Strict;
                    } else if ( val.find( "15445:1999", pos+6 ) != -1 ) {
                        htmlMode = Html4;
                        publicId = Strict;
                    } else {
                        int tagPos = val.find( "html", pos+6, false );
                        if( tagPos == -1 )
                            tagPos = val.find( "hypertext markup", pos+6, false );
                        if ( tagPos != -1 ) {
                            tagPos = val.find(QRegExp("[0-9]"), tagPos );
                            int version = val.mid( tagPos, 1 ).toInt();
                            //kdDebug() << "DocumentImpl::determineParseMode tagPos = " << tagPos << " version=" << version << endl;
                            if( version > 3 ) {
                                htmlMode = Html4;
                                publicId = isTransitional( val, tagPos ) ? Transitional : Strict;
                            }
                        }
                    }
                }
                start = quote2 + 1;
            }
        }

        if( systemId == publicId )
            pMode = publicId;
        else if ( systemId == Unknown )
            pMode = htmlMode == Html4 ? Compat : publicId;
        else if ( publicId == Transitional && systemId == Strict ) {
            if ( htmlMode == Html3 )
                pMode = Compat;
            else
                pMode = Strict;
        } else
            pMode = Compat;

        if ( htmlMode == XHtml )
            pMode = Strict;
    }
    //kdDebug() << "DocumentImpl::determineParseMode: publicId =" << publicId << " systemId = " << systemId << endl;
    //kdDebug() << "DocumentImpl::determineParseMode: htmlMode = " << htmlMode<< endl;
    if( pMode == Strict )
        kdDebug(6020) << " using strict parseMode" << endl;
    else if (pMode == Compat )
        kdDebug(6020) << " using compatibility parseMode" << endl;
    else
        kdDebug(6020) << " using transitional parseMode" << endl;
}

// Please see if there`s a possibility to merge that code
// with the next function and getElementByID().
NodeImpl *DocumentImpl::findElement( int id )
{
    QStack<NodeImpl> nodeStack;
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

ElementImpl *DocumentImpl::findSelectableElement(NodeImpl *start, bool forward)
{
    if (!start)
        start = forward?_first:_last;
    if (!start)
        return 0;
    if (forward)
        while(1)
        {
            if (!start->isSelectable() && start->firstChild())
                start = start->firstChild();
            else if (start->nextSibling())
                start = start->nextSibling();
            else // find the next sibling of the first parent that has a nextSibling
            {
                NodeImpl *pa = start;
                while (pa)
                {
                    pa = pa->parentNode();
                    if (!pa)
                        return 0;
                    if (pa->nextSibling())
                    {
                        start = pa->nextSibling();
                        pa = 0;
                    }
                }
            }
            if (start->isElementNode() && start->isSelectable())
                return static_cast<ElementImpl*>(start);
        }
    else
        while (1)
        {
            if (!start->isSelectable() && start->lastChild())
                start = start->lastChild();
            else if (start->previousSibling())
                start = start->previousSibling();
            else
            {
                NodeImpl *pa = start;
                while (pa)
                {
                  // find the previous sibling of the first parent that has a prevSibling
                    pa = pa->parentNode();
                    if (!pa)
                        return 0;
                    if (pa->previousSibling())
                    {
                        start = pa->previousSibling();
                        pa = 0;
                        break;
                    }
                }
            }
            if (start->isElementNode() && start->isSelectable())
                return static_cast<ElementImpl*>(start);
        }
    kdFatal(6000) << "some error in findElement\n";
}


int DocumentImpl::findHighestTabIndex()
{
    NodeImpl *n=this;
    NodeImpl *next=0;
    ElementImpl *a;
    int retval=-1;
    int tmpval;
    while(n)
    {
        //find out tabindex of current element, if availiable
        if (n->isElementNode())
        {
            a=static_cast<ElementImpl *>(n);
            tmpval=a->tabIndex();
            if (tmpval>retval)
                retval=tmpval;
        }
        //iterate to next element.
        if (!n->isSelectable() && n->firstChild())
            n=n->firstChild();
        else if (n->nextSibling())
            n=n->nextSibling();
        else
        {
            next=0;
            while(!next)
            {
                n=n->parentNode();
                if (!n)
                    return retval;
                next=n->nextSibling();
            }
            n=next;
        }
    }
    return retval;
}

ElementImpl *DocumentImpl::findNextLink(ElementImpl *cur, bool forward)
{
    int curTabIndex = (cur?cur->tabIndex():(forward?1:-1));

    switch(curTabIndex)
    {
    case -1:
        return notabindex(cur, forward);
    case 0:
        return tabindexzero(cur, forward);
    default:
        return intabindex(cur, forward);
    }
}

ElementImpl *DocumentImpl::findLink(ElementImpl *n, bool forward, int tabIndexHint)
{
    // tabIndexHint is the tabIndex that should be found.
    // if tabIndex is -1, items containing tabIndex are skipped.

    //  kdDebug(6000)<<"DocumentImpl:findLink: Node: "<<n<<" forward: "<<(forward?"true":"false")<<" tabIndexHint: "<<tabIndexHint<<"\n";

    int maxTabIndex;

    if (forward) maxTabIndex = findHighestTabIndex();
    else maxTabIndex = -1;

    do
    {
        n = findSelectableElement(n, forward);
        // this is alright even for non-tabindex-searches,
        // because DOM::NodeImpl::tabIndex() defaults to -1.
    } while (n && (n->tabIndex()!=tabIndexHint));
    return n;
}

ElementImpl *DocumentImpl::notabindex(ElementImpl *cur, bool forward)
{
    // REQ: n must be after the current node and its tabindex must be -1
    if ((cur = findLink(cur, forward, -1)))
        return cur;

    if (forward)
        return 0;
    else
        return tabindexzero(cur, forward);
}

ElementImpl *DocumentImpl::intabindex(ElementImpl *cur, bool forward)
{
    short tmptabindex;
    short maxtabindex = findHighestTabIndex();
    short increment=(forward?1:-1);
    if (cur)
    {
        tmptabindex = cur->tabIndex();
    }
    else tmptabindex=(forward?1:maxtabindex);

    while(tmptabindex>0 && tmptabindex<=maxtabindex)
    {
        if ((cur = findLink(cur, forward, tmptabindex)))
            return cur;
        tmptabindex+=increment;
    }

    if (forward)
        return tabindexzero(cur, forward);
    else
        return 0;
}

ElementImpl *DocumentImpl::tabindexzero(ElementImpl *cur, bool forward)
{
    //REQ: tabindex of result must be 0 and it must be after the current node ;
    if ((cur = findLink(cur, forward, 0)))
        return cur;

    if (forward)
        return notabindex(cur, forward);
    else
        return intabindex(cur, forward);
}

bool DocumentImpl::prepareMouseEvent( int _x, int _y,
                                      int, int,
                                      MouseEvent *ev )
{
    NodeImpl *n = documentElement();
    if ( n )
        return n->prepareMouseEvent( _x, _y, 0, 0, ev );
    else
        return false;
}

// DOM Section 1.1.1
bool DocumentImpl::childAllowed( NodeImpl *newChild )
{
// ### maximum of one Element
// ### maximum of one DocumentType
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


NodeImpl *DocumentImpl::cloneNode ( bool /*deep*/, int &exceptioncode )
{
    exceptioncode = DOMException::NOT_SUPPORTED_ERR;
    return 0;
}

unsigned short DocumentImpl::elementId(DOMStringImpl *_name)
{
    unsigned short id = 0;
    // note: this does not take namespaces into account, as it is only really used for css at the moment

    if (_name->isLower()) // use the html id instead (if one exists)
        id = khtml::getTagID(DOMString(_name).string().ascii(), _name->l);
    if (id)
        return id;

    // first try and find the element
    for (id = 0; id < m_elementNameCount; id++)
        if (!strcmp(m_elementNames[id],_name))
            return id+1000;

    // we don't have it yet, assign it an id
    if (m_elementNameCount+1 > m_elementNameAlloc) {
        m_elementNameAlloc += 100;
        DOMStringImpl **newNames = new DOMStringImpl* [m_elementNameAlloc];
        if (m_elementNames) {
            unsigned short i;
            for (i = 0; i < m_elementNameCount; i++)
                newNames[i] = m_elementNames[i];
            delete [] m_elementNames;
        }
        m_elementNames = newNames;
    }
    id = m_elementNameCount++;
    m_elementNames[id] = _name;
    _name->ref();
    // we add 1000 to the XML element id to avoid clashes with HTML element ids
    return id+1000;
}

DOMStringImpl *DocumentImpl::elementName(unsigned short _id) const
{
    if (_id >= 1000)
        return m_elementNames[_id-1000];
    else
        return getTagName(_id).implementation()->lower();
}


StyleSheetListImpl* DocumentImpl::styleSheets()
{
    return m_styleSheets;
}

void DocumentImpl::createSelector()
{
    QList<StyleSheetImpl> oldStyleSheets = m_styleSheets->styleSheets;
    m_styleSheets->styleSheets.clear();
    NodeImpl *n;
    for (n = this; n; n = n->traverseNextNode()) {
    	StyleSheetImpl *sheet = 0;
    	if (n->nodeType() == Node::PROCESSING_INSTRUCTION_NODE)
	    sheet = static_cast<ProcessingInstructionImpl*>(n)->sheet();
    	else if (n->id() == ID_LINK)
	    sheet = static_cast<HTMLLinkElementImpl*>(n)->sheet();
    	else if (n->id() == ID_STYLE)
	    sheet = static_cast<HTMLStyleElementImpl*>(n)->sheet();
    	else if (n->id() == ID_BODY)
	    sheet = static_cast<HTMLBodyElementImpl*>(n)->sheet();

	if (sheet) {
	    sheet->ref();
	    m_styleSheets->styleSheets.append(sheet);
	}
	if (isHTMLDocument() && n->id() == ID_BODY)
	    break;
    }
    QListIterator<StyleSheetImpl> it(oldStyleSheets);
    for (; it.current(); ++it)
	it.current()->deref();
    applyChanges(true,true);
}

void DocumentImpl::setFocusNode(ElementImpl *n)
{
    // ### add check for same Document
    if (m_focusNode != n)
    {
        if (m_focusNode)
        {
            if (m_focusNode->active()) m_focusNode->setActive(false);
            m_focusNode->setFocus(false);

	    int exceptioncode;

	    UIEventImpl *ue = new UIEventImpl(EventImpl::DOMFOCUSOUT_EVENT,
	                                      true,false,defaultView(),
					      0);

	    ue->ref();
	    m_focusNode->dispatchEvent(ue,exceptioncode);
	    ue->deref();
        }
        m_focusNode = n;
        //kdDebug(6020)<<"DOM::DocumentImpl::setFocusNode("<<n<<")"<<endl;
        if (n)
	{
	    int exceptioncode;

	    UIEventImpl *ue = new UIEventImpl(EventImpl::DOMFOCUSIN_EVENT,
	                                      true,false,defaultView(),
					      0);

	    ue->ref();
	    m_focusNode->dispatchEvent(ue,exceptioncode);
	    ue->deref();

            n->setFocus();
	}
    }
}

ElementImpl *DocumentImpl::focusNode()
{
    return m_focusNode;
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
    QListIterator<NodeIteratorImpl> it(m_nodeIterators);
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
    QListIterator<RegisteredEventListener> it(m_windowEventListeners);
    Event ev = evt;
    for (; it.current(); ++it) {
        if (it.current()->id == evt->id()) {
            it.current()->listener->handleEvent(ev);
	    return;
	}
    }
}

void DocumentImpl::setWindowEventListener(int id, EventListener *listener)
{
    removeWindowEventListener(id);
    if (listener) {
	RegisteredEventListener *rl = new RegisteredEventListener(static_cast<EventImpl::EventId>(id),listener,false);
	m_windowEventListeners.append(rl);
    }
}

EventListener *DocumentImpl::getWindowEventListener(int id)
{
    QListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it) {
	if (it.current()->id == id) {
	    return it.current()->listener;
	}
    }

    return 0;
}

void DocumentImpl::removeWindowEventListener(int id)
{
    QListIterator<RegisteredEventListener> it(m_windowEventListeners);
    for (; it.current(); ++it) {
	if (it.current()->id == id) {
	    m_windowEventListeners.removeRef(it.current());
	    return;
	}
    }
}

EventListener *DocumentImpl::createHTMLEventListener(QString code)
{
    return view()->part()->createHTMLEventListener(code);
}

// ----------------------------------------------------------------------------

DocumentFragmentImpl::DocumentFragmentImpl(DocumentPtr *doc) : NodeBaseImpl(doc)
{
}

DocumentFragmentImpl::DocumentFragmentImpl(const DocumentFragmentImpl &other)
    : NodeBaseImpl(other)
{
}

const DOMString DocumentFragmentImpl::nodeName() const
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

NodeImpl *DocumentFragmentImpl::cloneNode ( bool deep, int &exceptioncode )
{
    DocumentFragmentImpl *clone = new DocumentFragmentImpl( docPtr() );
    if (deep)
        cloneChildNodes(clone,exceptioncode);
    return clone;
}


// ----------------------------------------------------------------------------

DocumentTypeImpl::DocumentTypeImpl(DocumentPtr *doc) : NodeImpl(doc)
{
    m_entities = new GenericRONamedNodeMapImpl();
    m_entities->ref();
    m_notations = new GenericRONamedNodeMapImpl();
    m_notations->ref();
}

DocumentTypeImpl::~DocumentTypeImpl()
{
    m_entities->deref();
    m_notations->deref();
}

const DOMString DocumentTypeImpl::name() const
{
    // ###
    return 0;
}

NamedNodeMapImpl *DocumentTypeImpl::entities() const
{
    return m_entities;
}

NamedNodeMapImpl *DocumentTypeImpl::notations() const
{
    return m_notations;
}

const DOMString DocumentTypeImpl::nodeName() const
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

NodeImpl *DocumentTypeImpl::cloneNode ( bool /*deep*/, int &exceptioncode )
{
    exceptioncode = DOMException::NOT_SUPPORTED_ERR;
    return 0;
}


#include "dom_docimpl.moc"


