/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
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
// -------------------------------------------------------------------------

#include "html/html_baseimpl.h"
#include "html/html_documentimpl.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include "rendering/render_frames.h"
#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/csshelper.h"
#include "misc/loader.h"
#include "dom/dom_string.h"
#include "xml/dom2_eventsimpl.h"

#include <kurl.h>
#include <kdebug.h>

using namespace DOM;
using namespace khtml;

HTMLBodyElementImpl::HTMLBodyElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(HTMLTags::body(), doc), m_linkDecl(0)
{
}

HTMLBodyElementImpl::~HTMLBodyElementImpl()
{
    if (m_linkDecl) {
        m_linkDecl->setNode(0);
        m_linkDecl->setParent(0);
        m_linkDecl->deref();
    }
}

void HTMLBodyElementImpl::createLinkDecl()
{
    m_linkDecl = new CSSMutableStyleDeclarationImpl;
    m_linkDecl->ref();
    m_linkDecl->setParent(getDocument()->elementSheet());
    m_linkDecl->setNode(this);
    m_linkDecl->setStrictParsing(!getDocument()->inCompatMode());
}

bool HTMLBodyElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLAttributes::background()) {
        result = (MappedAttributeEntry)(eLastEntry + getDocument()->docID());
        return false;
    } 
    
    if (attrName == HTMLAttributes::bgcolor() ||
        attrName == HTMLAttributes::text() ||
        attrName == HTMLAttributes::marginwidth() ||
        attrName == HTMLAttributes::leftmargin() ||
        attrName == HTMLAttributes::marginheight() ||
        attrName == HTMLAttributes::topmargin() ||
        attrName == HTMLAttributes::bgproperties()) {
        result = eUniversal;
        return false;
    }

    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLBodyElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == HTMLAttributes::background()) {
        QString url = khtml::parseURL(attr->value()).string();
        if (!url.isEmpty())
            addCSSImageProperty(attr, CSS_PROP_BACKGROUND_IMAGE, getDocument()->completeURL(url));
    } else if (attr->name() == HTMLAttributes::marginwidth()) {
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
    } else if (attr->name() == HTMLAttributes::leftmargin()) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
    } else if (attr->name() == HTMLAttributes::marginheight()) {
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
    } else if (attr->name() == HTMLAttributes::topmargin()) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
    } else if (attr->name() == HTMLAttributes::bgcolor()) {
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    } else if (attr->name() == HTMLAttributes::text()) {
        addCSSColor(attr, CSS_PROP_COLOR, attr->value());
    } else if (attr->name() == HTMLAttributes::bgproperties()) {
        if (strcasecmp( attr->value(), "fixed" ) == 0)
            addCSSProperty(attr, CSS_PROP_BACKGROUND_ATTACHMENT, CSS_VAL_FIXED);
    } else if (attr->name() == HTMLAttributes::vlink() ||
               attr->name() == HTMLAttributes::alink() ||
               attr->name() == HTMLAttributes::link()) {
        if (attr->isNull()) {
            if (attr->name() == HTMLAttributes::link())
                getDocument()->resetLinkColor();
            else if (attr->name() == HTMLAttributes::vlink())
                getDocument()->resetVisitedLinkColor();
            else
                getDocument()->resetActiveLinkColor();
        }
        else {
            if (!m_linkDecl)
                createLinkDecl();
            m_linkDecl->setProperty(CSS_PROP_COLOR, attr->value(), false, false);
            CSSValueImpl* val = m_linkDecl->getPropertyCSSValue(CSS_PROP_COLOR);
            if (val) {
                val->ref();
                if (val->isPrimitiveValue()) {
                    QColor col = getDocument()->styleSelector()->getColorFromPrimitiveValue(static_cast<CSSPrimitiveValueImpl*>(val));
                    if (attr->name() == HTMLAttributes::link())
                        getDocument()->setLinkColor(col);
                    else if (attr->name() == HTMLAttributes::vlink())
                        getDocument()->setVisitedLinkColor(col);
                    else
                        getDocument()->setActiveLinkColor(col);
                }
                val->deref();
            }
        }
        
        if (attached())
            getDocument()->recalcStyle(Force);
    } else if (attr->name() == HTMLAttributes::onload()) {
        getDocument()->setHTMLWindowEventListener(EventImpl::LOAD_EVENT,	    
                                                  getDocument()->createHTMLEventListener(attr->value().string(), NULL));
    } else if (attr->name() == HTMLAttributes::onunload()) {
        getDocument()->setHTMLWindowEventListener(EventImpl::UNLOAD_EVENT,
                                                  getDocument()->createHTMLEventListener(attr->value().string(), NULL));
    } else if (attr->name() == HTMLAttributes::onblur()) {
        getDocument()->setHTMLWindowEventListener(EventImpl::BLUR_EVENT,
                                                  getDocument()->createHTMLEventListener(attr->value().string(), NULL));
    } else if (attr->name() == HTMLAttributes::onfocus()) {
        getDocument()->setHTMLWindowEventListener(EventImpl::FOCUS_EVENT,
                                                  getDocument()->createHTMLEventListener(attr->value().string(), NULL));
    } else if (attr->name() == HTMLAttributes::onresize()) {
        getDocument()->setHTMLWindowEventListener(EventImpl::RESIZE_EVENT,
                                                  getDocument()->createHTMLEventListener(attr->value().string(), NULL));
    } else if (attr->name() == HTMLAttributes::onscroll()) {
        getDocument()->setHTMLWindowEventListener(EventImpl::SCROLL_EVENT,
                                                  getDocument()->createHTMLEventListener(attr->value().string(), NULL));
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

void HTMLBodyElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();

    // FIXME: perhaps this code should be in attach() instead of here

    DocumentImpl *d = getDocument();
    KHTMLView *w = d ? d->view() : 0;
    if (w && w->marginWidth() != -1) {
        QString s;
        s.sprintf("%d", w->marginWidth());
        setAttribute(HTMLAttributes::marginwidth(), s);
    }
    if (w && w->marginHeight() != -1) {
        QString s;
        s.sprintf("%d", w->marginHeight());
        setAttribute(HTMLAttributes::marginheight(), s);
    }

    if (w)
        w->scheduleRelayout();
}

bool HTMLBodyElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == HTMLAttributes::background();
}

DOMString HTMLBodyElementImpl::aLink() const
{
    return getAttribute(HTMLAttributes::alink());
}

void HTMLBodyElementImpl::setALink(const DOMString &value)
{
    setAttribute(HTMLAttributes::alink(), value);
}

DOMString HTMLBodyElementImpl::background() const
{
    return getAttribute(HTMLAttributes::background());
}

void HTMLBodyElementImpl::setBackground(const DOMString &value)
{
    setAttribute(HTMLAttributes::background(), value);
}

DOMString HTMLBodyElementImpl::bgColor() const
{
    return getAttribute(HTMLAttributes::bgcolor());
}

void HTMLBodyElementImpl::setBgColor(const DOMString &value)
{
    setAttribute(HTMLAttributes::bgcolor(), value);
}

DOMString HTMLBodyElementImpl::link() const
{
    return getAttribute(HTMLAttributes::link());
}

void HTMLBodyElementImpl::setLink(const DOMString &value)
{
    setAttribute(HTMLAttributes::link(), value);
}

DOMString HTMLBodyElementImpl::text() const
{
    return getAttribute(HTMLAttributes::text());
}

void HTMLBodyElementImpl::setText(const DOMString &value)
{
    setAttribute(HTMLAttributes::text(), value);
}

DOMString HTMLBodyElementImpl::vLink() const
{
    return getAttribute(HTMLAttributes::vlink());
}

void HTMLBodyElementImpl::setVLink(const DOMString &value)
{
    setAttribute(HTMLAttributes::vlink(), value);
}

// -------------------------------------------------------------------------

HTMLFrameElementImpl::HTMLFrameElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(HTMLTags::frame(), doc)
{
    init();
}

HTMLFrameElementImpl::HTMLFrameElementImpl(const QualifiedName& tagName, DocumentPtr *doc)
    : HTMLElementImpl(tagName, doc)
{
    init();
}

void HTMLFrameElementImpl::init()
{
    m_frameBorder = true;
    m_frameBorderSet = false;
    m_marginWidth = -1;
    m_marginHeight = -1;
    m_scrolling = QScrollView::Auto;
    m_noResize = false;
}

HTMLFrameElementImpl::~HTMLFrameElementImpl()
{
}

bool HTMLFrameElementImpl::isURLAllowed(const AtomicString &URLString) const
{
    if (URLString.isEmpty()) {
        return true;
    }
    
    DocumentImpl *d = getDocument();
    KHTMLView *w = d ? d->view() : 0;

    if (!w) {
	return false;
    }

    KURL newURL(getDocument()->completeURL(URLString.string()));
    newURL.setRef(QString::null);

    // Don't allow more than 1000 total frames in a set. This seems
    // like a reasonable upper bound, and otherwise mutually recursive
    // frameset pages can quickly bring the program to its knees with
    // exponential growth in the number of frames.

    // FIXME: This limit could be higher, but WebKit has some
    // algorithms that happen while loading which appear to be N^2 or
    // worse in the number of frames
    if (w->part()->topLevelFrameCount() >= 200) {
	return false;
    }

    // Prohibit non-file URLs if we are asked to.
    if (w->part()->onlyLocalReferences() && newURL.protocol().lower() != "file") {
        return false;
    }

    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    bool foundSelfReference = false;
    for (KHTMLPart *part = w->part(); part; part = part->parentPart()) {
        KURL partURL = part->url();
        partURL.setRef(QString::null);
        if (partURL == newURL) {
            if (foundSelfReference) {
                return false;
            }
            foundSelfReference = true;
        }
    }
    
    return true;
}

void HTMLFrameElementImpl::updateForNewURL()
{
    if (!attached()) {
        return;
    }
    
    // Handle the common case where we decided not to make a frame the first time.
    // Detach and the let attach() decide again whether to make the frame for this URL.
    if (!m_render) {
        detach();
        attach();
        return;
    }
    
    if (!isURLAllowed(m_URL)) {
        return;
    }

    openURL();
}

void HTMLFrameElementImpl::openURL()
{
    DocumentImpl *d = getDocument();
    KHTMLView *w = d ? d->view() : 0;
    if (!w) {
        return;
    }
    
    AtomicString relativeURL = m_URL;
    if (relativeURL.isEmpty()) {
        relativeURL = "about:blank";
    }

    // Load the frame contents.
    KHTMLPart *part = w->part();
    KHTMLPart *framePart = part->findFrame(m_name.string());
    if (framePart) {
        framePart->openURL(getDocument()->completeURL(relativeURL.string()));
    } else {
        part->requestFrame(static_cast<RenderFrame *>(m_render), relativeURL.string(), m_name.string());
    }
}


void HTMLFrameElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == HTMLAttributes::src()) {
        setLocation(khtml::parseURL(attr->value()));
    } else if (attr->name() == HTMLAttributes::idAttr()) {
        // Important to call through to base for the id attribute so the hasID bit gets set.
        HTMLElementImpl::parseMappedAttribute(attr);
        m_name = attr->value();
    } else if (attr->name() == HTMLAttributes::name()) {
        m_name = attr->value();
        // FIXME: If we are already attached, this doesn't actually change the frame's name.
        // FIXME: If we are already attached, this doesn't check for frame name
        // conflicts and generate a unique frame name.
    } else if (attr->name() == HTMLAttributes::frameborder()) {
        m_frameBorder = attr->value().toInt();
        m_frameBorderSet = !attr->isNull();
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == HTMLAttributes::marginwidth()) {
        m_marginWidth = attr->value().toInt();
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == HTMLAttributes::marginheight()) {
        m_marginHeight = attr->value().toInt();
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == HTMLAttributes::noresize()) {
        m_noResize = true;
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == HTMLAttributes::scrolling()) {
        kdDebug( 6031 ) << "set scroll mode" << endl;
	// Auto and yes both simply mean "allow scrolling."  No means
	// "don't allow scrolling."
        if( strcasecmp( attr->value(), "auto" ) == 0 ||
            strcasecmp( attr->value(), "yes" ) == 0 )
            m_scrolling = QScrollView::Auto;
        else if( strcasecmp( attr->value(), "no" ) == 0 )
            m_scrolling = QScrollView::AlwaysOff;
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == HTMLAttributes::onload()) {
        setHTMLEventListener(EventImpl::LOAD_EVENT,
                                getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onunload()) {
        setHTMLEventListener(EventImpl::UNLOAD_EVENT,
                                getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

bool HTMLFrameElementImpl::rendererIsNeeded(RenderStyle *style)
{
    // Ignore display: none.
    return isURLAllowed(m_URL);
}

RenderObject *HTMLFrameElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderFrame(this);
}

void HTMLFrameElementImpl::attach()
{
    m_name = getAttribute(HTMLAttributes::name());
    if (m_name.isNull())
        m_name = getAttribute(HTMLAttributes::idAttr());

    // inherit default settings from parent frameset
    for (NodeImpl *node = parentNode(); node; node = node->parentNode())
        if (node->hasTagName(HTMLTags::frameset())) {
            HTMLFrameSetElementImpl* frameset = static_cast<HTMLFrameSetElementImpl*>(node);
            if (!m_frameBorderSet)
                m_frameBorder = frameset->frameBorder();
            if (!m_noResize)
                m_noResize = frameset->noResize();
            break;
        }

    HTMLElementImpl::attach();

    if (!m_render)
        return;

    KHTMLPart *part = getDocument()->part();

    if (!part)
	return;

    part->incrementFrameCount();
    
    AtomicString relativeURL = m_URL;
    if (relativeURL.isEmpty()) {
        relativeURL = "about:blank";
    }

    // we need a unique name for every frame in the frameset. Hope that's unique enough.
    if (m_name.isEmpty() || part->frameExists(m_name.string()))
        m_name = AtomicString(part->requestFrameName());

    // load the frame contents
    part->requestFrame(static_cast<RenderFrame*>(m_render), relativeURL.string(), m_name.string());
}

void HTMLFrameElementImpl::detach()
{
    KHTMLPart *part = getDocument()->part();

    if (m_render && part) {
	part->decrementFrameCount();
        KHTMLPart *framePart = part->findFrame(m_name.string());
        if (framePart)
            framePart->frameDetached();
    }

    HTMLElementImpl::detach();
}

void HTMLFrameElementImpl::setLocation( const DOMString& str )
{
    if (m_URL == str)
        return;
    m_URL = AtomicString(str);
    updateForNewURL();
}

bool HTMLFrameElementImpl::isFocusable() const
{
    return m_render!=0;
}

void HTMLFrameElementImpl::setFocus(bool received)
{
    HTMLElementImpl::setFocus(received);
    khtml::RenderFrame *renderFrame = static_cast<khtml::RenderFrame *>(m_render);
    if (!renderFrame || !renderFrame->widget())
	return;
    if (received)
	renderFrame->widget()->setFocus();
    else
	renderFrame->widget()->clearFocus();
}

KHTMLPart* HTMLFrameElementImpl::contentPart() const
{
    // Start with the part that contains this element, our ownerDocument.
    KHTMLPart* ownerDocumentPart = getDocument()->part();
    if (!ownerDocumentPart) {
        return 0;
    }

    // Find the part for the subframe that this element represents.
    return ownerDocumentPart->findFrame(m_name.string());
}

DocumentImpl* HTMLFrameElementImpl::contentDocument() const
{
    KHTMLPart* part = contentPart();
    if (!part) {
        return 0;
    }

    // Return the document for that part, which is our contentDocument.
    return part->xmlDocImpl();
}

bool HTMLFrameElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == HTMLAttributes::src();
}

DOMString HTMLFrameElementImpl::frameBorder() const
{
    return getAttribute(HTMLAttributes::frameborder());
}

void HTMLFrameElementImpl::setFrameBorder(const DOMString &value)
{
    setAttribute(HTMLAttributes::frameborder(), value);
}

DOMString HTMLFrameElementImpl::longDesc() const
{
    return getAttribute(HTMLAttributes::longdesc());
}

void HTMLFrameElementImpl::setLongDesc(const DOMString &value)
{
    setAttribute(HTMLAttributes::longdesc(), value);
}

DOMString HTMLFrameElementImpl::marginHeight() const
{
    return getAttribute(HTMLAttributes::marginheight());
}

void HTMLFrameElementImpl::setMarginHeight(const DOMString &value)
{
    setAttribute(HTMLAttributes::marginheight(), value);
}

DOMString HTMLFrameElementImpl::marginWidth() const
{
    return getAttribute(HTMLAttributes::marginwidth());
}

void HTMLFrameElementImpl::setMarginWidth(const DOMString &value)
{
    setAttribute(HTMLAttributes::marginwidth(), value);
}

DOMString HTMLFrameElementImpl::name() const
{
    return getAttribute(HTMLAttributes::name());
}

void HTMLFrameElementImpl::setName(const DOMString &value)
{
    setAttribute(HTMLAttributes::name(), value);
}

void HTMLFrameElementImpl::setNoResize(bool noResize)
{
    setAttribute(HTMLAttributes::noresize(), noResize ? "" : 0);
}

DOMString HTMLFrameElementImpl::scrolling() const
{
    return getAttribute(HTMLAttributes::scrolling());
}

void HTMLFrameElementImpl::setScrolling(const DOMString &value)
{
    setAttribute(HTMLAttributes::scrolling(), value);
}

DOMString HTMLFrameElementImpl::src() const
{
    return getAttribute(HTMLAttributes::src());
}

void HTMLFrameElementImpl::setSrc(const DOMString &value)
{
    setAttribute(HTMLAttributes::src(), value);
}

// -------------------------------------------------------------------------

HTMLFrameSetElementImpl::HTMLFrameSetElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(HTMLTags::frameset(), doc)
{
    // default value for rows and cols...
    m_totalRows = 1;
    m_totalCols = 1;

    m_rows = m_cols = 0;

    frameborder = true;
    frameBorderSet = false;
    m_border = 4;
    noresize = false;

    m_resizing = false;
}

HTMLFrameSetElementImpl::~HTMLFrameSetElementImpl()
{
    if (m_rows)
        delete [] m_rows;
    if (m_cols)
        delete [] m_cols;
}

bool HTMLFrameSetElementImpl::checkDTD(const NodeImpl* newChild)
{
    // FIXME: Old code had adjacent double returns and seemed to want to do something with NOFRAMES (but didn't).
    // What is the correct behavior?
    return newChild->hasTagName(HTMLTags::frameset()) || newChild->hasTagName(HTMLTags::frame());
}

void HTMLFrameSetElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == HTMLAttributes::rows()) {
        if (!attr->isNull()) {
            if (m_rows) delete [] m_rows;
            m_rows = attr->value().toLengthArray(m_totalRows);
            setChanged();
        }
    } else if (attr->name() == HTMLAttributes::cols()) {
        if (!attr->isNull()) {
            delete [] m_cols;
            m_cols = attr->value().toLengthArray(m_totalCols);
            setChanged();
        }
    } else if (attr->name() == HTMLAttributes::frameborder()) {
        // false or "no" or "0"..
        if ( attr->value().toInt() == 0 ) {
            frameborder = false;
            m_border = 0;
        }
        frameBorderSet = true;
    } else if (attr->name() == HTMLAttributes::noresize()) {
        noresize = true;
    } else if (attr->name() == HTMLAttributes::border()) {
        m_border = attr->value().toInt();
        if(!m_border)
            frameborder = false;
    } else if (attr->name() == HTMLAttributes::onload()) {
        setHTMLEventListener(EventImpl::LOAD_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else if (attr->name() == HTMLAttributes::onunload()) {
        setHTMLEventListener(EventImpl::UNLOAD_EVENT,
                             getDocument()->createHTMLEventListener(attr->value().string(), this));
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

bool HTMLFrameSetElementImpl::rendererIsNeeded(RenderStyle *style)
{
    // Ignore display: none but do pay attention if a stylesheet has caused us to delay our loading.
    return style->isStyleAvailable();
}

RenderObject *HTMLFrameSetElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderFrameSet(this);
}

void HTMLFrameSetElementImpl::attach()
{
    // inherit default settings from parent frameset
    HTMLElementImpl* node = static_cast<HTMLElementImpl*>(parentNode());
    while(node)
    {
        if (node->hasTagName(HTMLTags::frameset())) {
            HTMLFrameSetElementImpl* frameset = static_cast<HTMLFrameSetElementImpl*>(node);
            if(!frameBorderSet)  frameborder = frameset->frameBorder();
            if(!noresize)  noresize = frameset->noResize();
            break;
        }
        node = static_cast<HTMLElementImpl*>(node->parentNode());
    }

    HTMLElementImpl::attach();
}

void HTMLFrameSetElementImpl::defaultEventHandler(EventImpl *evt)
{
    if (evt->isMouseEvent() && !noresize && m_render) {
        static_cast<khtml::RenderFrameSet *>(m_render)->userResize(static_cast<MouseEventImpl*>(evt));
        evt->setDefaultHandled();
    }

    HTMLElementImpl::defaultEventHandler(evt);
}

void HTMLFrameSetElementImpl::detach()
{
    if(attached())
        // ### send the event when we actually get removed from the doc instead of here
        dispatchHTMLEvent(EventImpl::UNLOAD_EVENT,false,false);

    HTMLElementImpl::detach();
}

void HTMLFrameSetElementImpl::recalcStyle( StyleChange ch )
{
    if (changed() && m_render) {
        m_render->setNeedsLayout(true);
//         m_render->layout();
        setChanged(false);
    }
    HTMLElementImpl::recalcStyle( ch );
}

DOMString HTMLFrameSetElementImpl::cols() const
{
    return getAttribute(HTMLAttributes::cols());
}

void HTMLFrameSetElementImpl::setCols(const DOMString &value)
{
    setAttribute(HTMLAttributes::cols(), value);
}

DOMString HTMLFrameSetElementImpl::rows() const
{
    return getAttribute(HTMLAttributes::rows());
}

void HTMLFrameSetElementImpl::setRows(const DOMString &value)
{
    setAttribute(HTMLAttributes::rows(), value);
}

// -------------------------------------------------------------------------

HTMLHeadElementImpl::HTMLHeadElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(HTMLTags::head(), doc)
{
}

HTMLHeadElementImpl::~HTMLHeadElementImpl()
{
}

DOMString HTMLHeadElementImpl::profile() const
{
    return getAttribute(HTMLAttributes::profile());
}

void HTMLHeadElementImpl::setProfile(const DOMString &value)
{
    setAttribute(HTMLAttributes::profile(), value);
}

bool HTMLHeadElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->hasTagName(HTMLTags::title()) || newChild->hasTagName(HTMLTags::isindex()) ||
           newChild->hasTagName(HTMLTags::base()) || newChild->hasTagName(HTMLTags::script()) ||
           newChild->hasTagName(HTMLTags::style()) || newChild->hasTagName(HTMLTags::meta()) ||
           newChild->hasTagName(HTMLTags::link()) || newChild->isTextNode();
}

// -------------------------------------------------------------------------

HTMLHtmlElementImpl::HTMLHtmlElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(HTMLTags::html(), doc)
{
}

HTMLHtmlElementImpl::~HTMLHtmlElementImpl()
{
}

DOMString HTMLHtmlElementImpl::version() const
{
    return getAttribute(HTMLAttributes::version());
}

void HTMLHtmlElementImpl::setVersion(const DOMString &value)
{
    setAttribute(HTMLAttributes::version(), value);
}

bool HTMLHtmlElementImpl::checkDTD(const NodeImpl* newChild)
{
    // FIXME: Why is <script> allowed here?
    return newChild->hasTagName(HTMLTags::head()) || newChild->hasTagName(HTMLTags::body()) ||
           newChild->hasTagName(HTMLTags::frameset()) || newChild->hasTagName(HTMLTags::noframes()) ||
           newChild->hasTagName(HTMLTags::script());
}

// -------------------------------------------------------------------------

HTMLIFrameElementImpl::HTMLIFrameElementImpl(DocumentPtr *doc) : HTMLFrameElementImpl(HTMLTags::iframe(), doc)
{
    m_frameBorder = false;
    m_marginWidth = -1;
    m_marginHeight = -1;
    needWidgetUpdate = false;
}

HTMLIFrameElementImpl::~HTMLIFrameElementImpl()
{
}

bool HTMLIFrameElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLAttributes::width() || attrName == HTMLAttributes::height()) {
        result = eUniversal;
        return false;
    }
    
    if (attrName == HTMLAttributes::align()) {
        result = eReplaced; // Share with <img> since the alignment behavior is the same.
        return false;
    }
    
    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLIFrameElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == HTMLAttributes::width())
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    else if (attr->name() == HTMLAttributes::height())
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    else if (attr->name() == HTMLAttributes::align())
        addHTMLAlignment(attr);
    else
      HTMLFrameElementImpl::parseMappedAttribute(attr);
}

bool HTMLIFrameElementImpl::rendererIsNeeded(RenderStyle *style)
{
    // Don't ignore display: none the way frame does.
    return isURLAllowed(m_URL) && style->display() != NONE;
}

RenderObject *HTMLIFrameElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderPartObject(this);
}

void HTMLIFrameElementImpl::attach()
{
    m_name = getAttribute(HTMLAttributes::name());
    if (m_name.isNull())
        m_name = getAttribute(HTMLAttributes::idAttr());
    
    HTMLElementImpl::attach();

    KHTMLPart *part = getDocument()->part();
    if (m_render && part) {
        // we need a unique name for every frame in the frameset. Hope that's unique enough.
        part->incrementFrameCount();
        if (m_name.isEmpty() || part->frameExists(m_name.string()))
            m_name = AtomicString(part->requestFrameName());

        static_cast<RenderPartObject*>(m_render)->updateWidget();
        needWidgetUpdate = false;
    }
}

void HTMLIFrameElementImpl::recalcStyle( StyleChange ch )
{
    if (needWidgetUpdate) {
        if(m_render)  static_cast<RenderPartObject*>(m_render)->updateWidget();
        needWidgetUpdate = false;
    }
    HTMLElementImpl::recalcStyle( ch );
}

void HTMLIFrameElementImpl::openURL()
{
    needWidgetUpdate = true;
    setChanged();
}

bool HTMLIFrameElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == HTMLAttributes::src();
}

DOMString HTMLIFrameElementImpl::align() const
{
    return getAttribute(HTMLAttributes::align());
}

void HTMLIFrameElementImpl::setAlign(const DOMString &value)
{
    setAttribute(HTMLAttributes::align(), value);
}

DOMString HTMLIFrameElementImpl::height() const
{
    return getAttribute(HTMLAttributes::height());
}

void HTMLIFrameElementImpl::setHeight(const DOMString &value)
{
    setAttribute(HTMLAttributes::height(), value);
}

DOMString HTMLIFrameElementImpl::src() const
{
    return getDocument()->completeURL(getAttribute(HTMLAttributes::src()));
}

DOMString HTMLIFrameElementImpl::width() const
{
    return getAttribute(HTMLAttributes::width());
}

void HTMLIFrameElementImpl::setWidth(const DOMString &value)
{
    setAttribute(HTMLAttributes::width(), value);
}
