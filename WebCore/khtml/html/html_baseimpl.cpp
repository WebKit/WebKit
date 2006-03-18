/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#include "config.h"
#include "html_baseimpl.h"

#include "EventNames.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "KURL.h"
#include "Page.h"
#include "PlatformString.h"
#include "css_stylesheetimpl.h"
#include "csshelper.h"
#include "cssproperties.h"
#include "cssstyleselector.h"
#include "cssvalues.h"
#include "dom2_eventsimpl.h"
#include "html_documentimpl.h"
#include "htmlnames.h"
#include "loader.h"
#include "render_frames.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLBodyElementImpl::HTMLBodyElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(bodyTag, doc)
{
}

HTMLBodyElementImpl::~HTMLBodyElementImpl()
{
    if (m_linkDecl) {
        m_linkDecl->setNode(0);
        m_linkDecl->setParent(0);
    }
}

void HTMLBodyElementImpl::createLinkDecl()
{
    m_linkDecl = new CSSMutableStyleDeclarationImpl;
    m_linkDecl->setParent(getDocument()->elementSheet());
    m_linkDecl->setNode(this);
    m_linkDecl->setStrictParsing(!getDocument()->inCompatMode());
}

bool HTMLBodyElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == backgroundAttr) {
        result = (MappedAttributeEntry)(eLastEntry + getDocument()->docID());
        return false;
    } 
    
    if (attrName == bgcolorAttr ||
        attrName == textAttr ||
        attrName == marginwidthAttr ||
        attrName == leftmarginAttr ||
        attrName == marginheightAttr ||
        attrName == topmarginAttr ||
        attrName == bgpropertiesAttr) {
        result = eUniversal;
        return false;
    }

    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLBodyElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == backgroundAttr) {
        DOMString url = khtml::parseURL(attr->value());
        if (!url.isEmpty())
            addCSSImageProperty(attr, CSS_PROP_BACKGROUND_IMAGE, getDocument()->completeURL(url));
    } else if (attr->name() == marginwidthAttr || attr->name() == leftmarginAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
    } else if (attr->name() == marginheightAttr || attr->name() == topmarginAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
    } else if (attr->name() == bgcolorAttr) {
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    } else if (attr->name() == textAttr) {
        addCSSColor(attr, CSS_PROP_COLOR, attr->value());
    } else if (attr->name() == bgpropertiesAttr) {
        if (equalIgnoringCase(attr->value(), "fixed"))
            addCSSProperty(attr, CSS_PROP_BACKGROUND_ATTACHMENT, CSS_VAL_FIXED);
    } else if (attr->name() == vlinkAttr ||
               attr->name() == alinkAttr ||
               attr->name() == linkAttr) {
        if (attr->isNull()) {
            if (attr->name() == linkAttr)
                getDocument()->resetLinkColor();
            else if (attr->name() == vlinkAttr)
                getDocument()->resetVisitedLinkColor();
            else
                getDocument()->resetActiveLinkColor();
        }
        else {
            if (!m_linkDecl)
                createLinkDecl();
            m_linkDecl->setProperty(CSS_PROP_COLOR, attr->value(), false, false);
            RefPtr<CSSValueImpl> val = m_linkDecl->getPropertyCSSValue(CSS_PROP_COLOR);
            if (val && val->isPrimitiveValue()) {
                Color col = getDocument()->styleSelector()->getColorFromPrimitiveValue(static_cast<CSSPrimitiveValueImpl*>(val.get()));
                if (attr->name() == linkAttr)
                    getDocument()->setLinkColor(col);
                else if (attr->name() == vlinkAttr)
                    getDocument()->setVisitedLinkColor(col);
                else
                    getDocument()->setActiveLinkColor(col);
            }
        }
        
        if (attached())
            getDocument()->recalcStyle(Force);
    } else if (attr->name() == onloadAttr) {
        getDocument()->setHTMLWindowEventListener(loadEvent, attr);
    } else if (attr->name() == onbeforeunloadAttr) {
        getDocument()->setHTMLWindowEventListener(beforeunloadEvent, attr);
    } else if (attr->name() == onunloadAttr) {
        getDocument()->setHTMLWindowEventListener(unloadEvent, attr);
    } else if (attr->name() == onblurAttr) {
        getDocument()->setHTMLWindowEventListener(blurEvent, attr);
    } else if (attr->name() == onfocusAttr) {
        getDocument()->setHTMLWindowEventListener(focusEvent, attr);
    } else if (attr->name() == onresizeAttr) {
        getDocument()->setHTMLWindowEventListener(resizeEvent, attr);
    } else if (attr->name() == onscrollAttr) {
        getDocument()->setHTMLWindowEventListener(scrollEvent, attr);
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

void HTMLBodyElementImpl::insertedIntoDocument()
{
    HTMLElementImpl::insertedIntoDocument();

    // FIXME: perhaps this code should be in attach() instead of here

    FrameView *w = getDocument()->view();
    if (w && w->marginWidth() != -1) {
        QString s;
        s.sprintf("%d", w->marginWidth());
        setAttribute(marginwidthAttr, s);
    }
    if (w && w->marginHeight() != -1) {
        QString s;
        s.sprintf("%d", w->marginHeight());
        setAttribute(marginheightAttr, s);
    }

    if (w)
        w->scheduleRelayout();
}

bool HTMLBodyElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == backgroundAttr;
}

DOMString HTMLBodyElementImpl::aLink() const
{
    return getAttribute(alinkAttr);
}

void HTMLBodyElementImpl::setALink(const DOMString &value)
{
    setAttribute(alinkAttr, value);
}

DOMString HTMLBodyElementImpl::background() const
{
    return getAttribute(backgroundAttr);
}

void HTMLBodyElementImpl::setBackground(const DOMString &value)
{
    setAttribute(backgroundAttr, value);
}

DOMString HTMLBodyElementImpl::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLBodyElementImpl::setBgColor(const DOMString &value)
{
    setAttribute(bgcolorAttr, value);
}

DOMString HTMLBodyElementImpl::link() const
{
    return getAttribute(linkAttr);
}

void HTMLBodyElementImpl::setLink(const DOMString &value)
{
    setAttribute(linkAttr, value);
}

DOMString HTMLBodyElementImpl::text() const
{
    return getAttribute(textAttr);
}

void HTMLBodyElementImpl::setText(const DOMString &value)
{
    setAttribute(textAttr, value);
}

DOMString HTMLBodyElementImpl::vLink() const
{
    return getAttribute(vlinkAttr);
}

void HTMLBodyElementImpl::setVLink(const DOMString &value)
{
    setAttribute(vlinkAttr, value);
}

// -------------------------------------------------------------------------

HTMLFrameElementImpl::HTMLFrameElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(frameTag, doc)
{
    init();
}

HTMLFrameElementImpl::HTMLFrameElementImpl(const QualifiedName& tagName, DocumentImpl *doc)
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
    m_scrolling = ScrollBarAuto;
    m_noResize = false;
}

HTMLFrameElementImpl::~HTMLFrameElementImpl()
{
}

bool HTMLFrameElementImpl::isURLAllowed(const AtomicString &URLString) const
{
    if (URLString.isEmpty())
        return true;
    
    FrameView *w = getDocument()->view();
    if (!w)
        return false;

    KURL newURL(getDocument()->completeURL(URLString.qstring()));
    newURL.setRef(QString::null);

    // Don't allow more than 1000 total frames in a set. This seems
    // like a reasonable upper bound, and otherwise mutually recursive
    // frameset pages can quickly bring the program to its knees with
    // exponential growth in the number of frames.

    // FIXME: This limit could be higher, but WebKit has some
    // algorithms that happen while loading which appear to be N^2 or
    // worse in the number of frames
    if (w->frame()->page()->frameCount() >= 200) {
        return false;
    }

    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    bool foundSelfReference = false;
    for (Frame *frame = w->frame(); frame; frame = frame->tree()->parent()) {
        KURL frameURL = frame->url();
        frameURL.setRef(QString::null);
        if (frameURL == newURL) {
            if (foundSelfReference) {
                return false;
            }
            foundSelfReference = true;
        }
    }
    
    return true;
}

void HTMLFrameElementImpl::openURL()
{
    FrameView *w = getDocument()->view();
    if (!w)
        return;
    
    AtomicString relativeURL = m_URL;
    if (relativeURL.isEmpty())
        relativeURL = "about:blank";

    // Load the frame contents.
    Frame* parentFrame = w->frame();
    if (Frame* childFrame = parentFrame->tree()->child(m_name))
        childFrame->openURL(getDocument()->completeURL(relativeURL.qstring()));
    else
        parentFrame->requestFrame(static_cast<RenderFrame *>(renderer()), relativeURL.qstring(), m_name.qstring());
}


void HTMLFrameElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == srcAttr) {
        setLocation(khtml::parseURL(attr->value()));
    } else if (attr->name() == idAttr) {
        // Important to call through to base for the id attribute so the hasID bit gets set.
        HTMLElementImpl::parseMappedAttribute(attr);
        m_name = attr->value();
    } else if (attr->name() == nameAttr) {
        m_name = attr->value();
        // FIXME: If we are already attached, this doesn't actually change the frame's name.
        // FIXME: If we are already attached, this doesn't check for frame name
        // conflicts and generate a unique frame name.
    } else if (attr->name() == frameborderAttr) {
        m_frameBorder = attr->value().toInt();
        m_frameBorderSet = !attr->isNull();
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == marginwidthAttr) {
        m_marginWidth = attr->value().toInt();
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == marginheightAttr) {
        m_marginHeight = attr->value().toInt();
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == noresizeAttr) {
        m_noResize = true;
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == scrollingAttr) {
        // Auto and yes both simply mean "allow scrolling." No means "don't allow scrolling."
        if (equalIgnoringCase(attr->value(), "auto") || equalIgnoringCase(attr->value(), "yes"))
            m_scrolling = ScrollBarAuto;
        else if (equalIgnoringCase(attr->value(), "no"))
            m_scrolling = ScrollBarAlwaysOff;
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == onloadAttr) {
        setHTMLEventListener(loadEvent, attr);
    } else if (attr->name() == onbeforeunloadAttr) {
        // FIXME: should <frame> elements have beforeunload handlers?
        setHTMLEventListener(beforeunloadEvent, attr);
    } else if (attr->name() == onunloadAttr) {
        setHTMLEventListener(unloadEvent, attr);
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
    m_name = getAttribute(nameAttr);
    if (m_name.isNull())
        m_name = getAttribute(idAttr);

    // inherit default settings from parent frameset
    for (NodeImpl *node = parentNode(); node; node = node->parentNode())
        if (node->hasTagName(framesetTag)) {
            HTMLFrameSetElementImpl* frameset = static_cast<HTMLFrameSetElementImpl*>(node);
            if (!m_frameBorderSet)
                m_frameBorder = frameset->frameBorder();
            if (!m_noResize)
                m_noResize = frameset->noResize();
            break;
        }

    HTMLElementImpl::attach();

    if (!renderer())
        return;

    Frame* frame = getDocument()->frame();

    if (!frame)
        return;

    frame->page()->incrementFrameCount();
    
    AtomicString relativeURL = m_URL;
    if (relativeURL.isEmpty())
        relativeURL = "about:blank";

    m_name = frame->tree()->uniqueChildName(m_name);

    // load the frame contents
    frame->requestFrame(static_cast<RenderFrame*>(renderer()), relativeURL.qstring(), m_name.qstring());
}

void HTMLFrameElementImpl::close()
{
    Frame* frame = getDocument()->frame();
    if (renderer() && frame) {
        frame->page()->decrementFrameCount();
        if (Frame* childFrame = frame->tree()->child(m_name))
            childFrame->frameDetached();
    }
}

void HTMLFrameElementImpl::willRemove()
{
    // close the frame and dissociate the renderer, but leave the
    // node attached so that frame does not get re-attached before
    // actually leaving the document.  see <rdar://problem/4132581>
    close();
    if (renderer()) {
        renderer()->destroy();
        setRenderer(0);
    }
    
    HTMLElementImpl::willRemove();
}

void HTMLFrameElementImpl::detach()
{
    close();
    HTMLElementImpl::detach();
}

void HTMLFrameElementImpl::setLocation(const DOMString& str)
{
    m_URL = AtomicString(str);

    if (!attached()) {
        return;
    }
    
    // Handle the common case where we decided not to make a frame the first time.
    // Detach and the let attach() decide again whether to make the frame for this URL.
    if (!renderer()) {
        detach();
        attach();
        return;
    }
    
    if (!isURLAllowed(m_URL)) {
        return;
    }

    openURL();
}

bool HTMLFrameElementImpl::isFocusable() const
{
    return renderer();
}

void HTMLFrameElementImpl::setFocus(bool received)
{
    HTMLElementImpl::setFocus(received);
    khtml::RenderFrame *renderFrame = static_cast<khtml::RenderFrame *>(renderer());
    if (!renderFrame || !renderFrame->widget())
        return;
    if (received)
        renderFrame->widget()->setFocus();
    else
        renderFrame->widget()->clearFocus();
}

Frame* HTMLFrameElementImpl::contentFrame() const
{
    // Start with the part that contains this element, our ownerDocument.
    Frame* parentFrame = getDocument()->frame();
    if (!parentFrame)
        return 0;

    // Find the part for the subframe that this element represents.
    return parentFrame->tree()->child(m_name);
}

DocumentImpl* HTMLFrameElementImpl::contentDocument() const
{
    Frame* frame = contentFrame();
    if (!frame)
        return 0;
    return frame->document();
}

bool HTMLFrameElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == srcAttr;
}

DOMString HTMLFrameElementImpl::frameBorder() const
{
    return getAttribute(frameborderAttr);
}

void HTMLFrameElementImpl::setFrameBorder(const DOMString &value)
{
    setAttribute(frameborderAttr, value);
}

DOMString HTMLFrameElementImpl::longDesc() const
{
    return getAttribute(longdescAttr);
}

void HTMLFrameElementImpl::setLongDesc(const DOMString &value)
{
    setAttribute(longdescAttr, value);
}

DOMString HTMLFrameElementImpl::marginHeight() const
{
    return getAttribute(marginheightAttr);
}

void HTMLFrameElementImpl::setMarginHeight(const DOMString &value)
{
    setAttribute(marginheightAttr, value);
}

DOMString HTMLFrameElementImpl::marginWidth() const
{
    return getAttribute(marginwidthAttr);
}

void HTMLFrameElementImpl::setMarginWidth(const DOMString &value)
{
    setAttribute(marginwidthAttr, value);
}

DOMString HTMLFrameElementImpl::name() const
{
    return getAttribute(nameAttr);
}

void HTMLFrameElementImpl::setName(const DOMString &value)
{
    setAttribute(nameAttr, value);
}

void HTMLFrameElementImpl::setNoResize(bool noResize)
{
    setAttribute(noresizeAttr, noResize ? "" : 0);
}

DOMString HTMLFrameElementImpl::scrolling() const
{
    return getAttribute(scrollingAttr);
}

void HTMLFrameElementImpl::setScrolling(const DOMString &value)
{
    setAttribute(scrollingAttr, value);
}

DOMString HTMLFrameElementImpl::src() const
{
    return getAttribute(srcAttr);
}

void HTMLFrameElementImpl::setSrc(const DOMString &value)
{
    setAttribute(srcAttr, value);
}

int HTMLFrameElementImpl::frameWidth() const
{
    if (!renderer())
        return 0;
    
    getDocument()->updateLayoutIgnorePendingStylesheets();
    return renderer()->width();
}

int HTMLFrameElementImpl::frameHeight() const
{
    if (!renderer())
        return 0;
    
    getDocument()->updateLayoutIgnorePendingStylesheets();
    return renderer()->height();
}

// -------------------------------------------------------------------------

HTMLFrameSetElementImpl::HTMLFrameSetElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(framesetTag, doc)
{
    // default value for rows and cols...
    m_totalRows = 1;
    m_totalCols = 1;

    m_rows = m_cols = 0;

    frameborder = true;
    frameBorderSet = false;
    m_border = 4;
    noresize = false;
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
    return newChild->hasTagName(framesetTag) || newChild->hasTagName(frameTag);
}

void HTMLFrameSetElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == rowsAttr) {
        if (!attr->isNull()) {
            if (m_rows) delete [] m_rows;
            m_rows = attr->value().toLengthArray(m_totalRows);
            setChanged();
        }
    } else if (attr->name() == colsAttr) {
        if (!attr->isNull()) {
            delete [] m_cols;
            m_cols = attr->value().toLengthArray(m_totalCols);
            setChanged();
        }
    } else if (attr->name() == frameborderAttr) {
        // false or "no" or "0"..
        if ( attr->value().toInt() == 0 ) {
            frameborder = false;
            m_border = 0;
        }
        frameBorderSet = true;
    } else if (attr->name() == noresizeAttr) {
        noresize = true;
    } else if (attr->name() == borderAttr) {
        m_border = attr->value().toInt();
        if(!m_border)
            frameborder = false;
    } else if (attr->name() == onloadAttr) {
        getDocument()->setHTMLWindowEventListener(loadEvent, attr);
    } else if (attr->name() == onbeforeunloadAttr) {
        getDocument()->setHTMLWindowEventListener(beforeunloadEvent, attr);
    } else if (attr->name() == onunloadAttr) {
        getDocument()->setHTMLWindowEventListener(unloadEvent, attr);
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
        if (node->hasTagName(framesetTag)) {
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
    if (evt->isMouseEvent() && !noresize && renderer()) {
        static_cast<khtml::RenderFrameSet *>(renderer())->userResize(static_cast<MouseEventImpl*>(evt));
        evt->setDefaultHandled();
    }

    HTMLElementImpl::defaultEventHandler(evt);
}

void HTMLFrameSetElementImpl::recalcStyle( StyleChange ch )
{
    if (changed() && renderer()) {
        renderer()->setNeedsLayout(true);
        setChanged(false);
    }
    HTMLElementImpl::recalcStyle( ch );
}

DOMString HTMLFrameSetElementImpl::cols() const
{
    return getAttribute(colsAttr);
}

void HTMLFrameSetElementImpl::setCols(const DOMString &value)
{
    setAttribute(colsAttr, value);
}

DOMString HTMLFrameSetElementImpl::rows() const
{
    return getAttribute(rowsAttr);
}

void HTMLFrameSetElementImpl::setRows(const DOMString &value)
{
    setAttribute(rowsAttr, value);
}

// -------------------------------------------------------------------------

HTMLHeadElementImpl::HTMLHeadElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(headTag, doc)
{
}

HTMLHeadElementImpl::~HTMLHeadElementImpl()
{
}

DOMString HTMLHeadElementImpl::profile() const
{
    return getAttribute(profileAttr);
}

void HTMLHeadElementImpl::setProfile(const DOMString &value)
{
    setAttribute(profileAttr, value);
}

bool HTMLHeadElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->hasTagName(titleTag) || newChild->hasTagName(isindexTag) ||
           newChild->hasTagName(baseTag) || newChild->hasTagName(scriptTag) ||
           newChild->hasTagName(styleTag) || newChild->hasTagName(metaTag) ||
           newChild->hasTagName(linkTag) || newChild->isTextNode();
}

// -------------------------------------------------------------------------

HTMLHtmlElementImpl::HTMLHtmlElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(htmlTag, doc)
{
}

HTMLHtmlElementImpl::~HTMLHtmlElementImpl()
{
}

DOMString HTMLHtmlElementImpl::version() const
{
    return getAttribute(versionAttr);
}

void HTMLHtmlElementImpl::setVersion(const DOMString &value)
{
    setAttribute(versionAttr, value);
}

bool HTMLHtmlElementImpl::checkDTD(const NodeImpl* newChild)
{
    // FIXME: Why is <script> allowed here?
    return newChild->hasTagName(headTag) || newChild->hasTagName(bodyTag) ||
           newChild->hasTagName(framesetTag) || newChild->hasTagName(noframesTag) ||
           newChild->hasTagName(scriptTag);
}

// -------------------------------------------------------------------------

HTMLIFrameElementImpl::HTMLIFrameElementImpl(DocumentImpl *doc) : HTMLFrameElementImpl(iframeTag, doc)
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
    if (attrName == widthAttr || attrName == heightAttr) {
        result = eUniversal;
        return false;
    }
    
    if (attrName == alignAttr) {
        result = eReplaced; // Share with <img> since the alignment behavior is the same.
        return false;
    }
    
    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLIFrameElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == widthAttr)
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    else if (attr->name() == heightAttr)
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    else if (attr->name() == alignAttr)
        addHTMLAlignment(attr);
    else if (attr->name() == nameAttr) {
        DOMString newNameAttr = attr->value();
        if (inDocument() && getDocument()->isHTMLDocument()) {
            HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
            document->removeDocExtraNamedItem(oldNameAttr);
            document->addDocExtraNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else
        HTMLFrameElementImpl::parseMappedAttribute(attr);
}

void HTMLIFrameElementImpl::insertedIntoDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->addDocExtraNamedItem(oldNameAttr);
    }

    HTMLElementImpl::insertedIntoDocument();
}

void HTMLIFrameElementImpl::removedFromDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->removeDocExtraNamedItem(oldNameAttr);
    }

    HTMLElementImpl::removedFromDocument();
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
    m_name = getAttribute(nameAttr);
    if (m_name.isNull())
        m_name = getAttribute(idAttr);

    HTMLElementImpl::attach();

    Frame* parentFrame = getDocument()->frame();
    if (renderer() && parentFrame) {
        parentFrame->page()->incrementFrameCount();
        m_name = parentFrame->tree()->uniqueChildName(m_name);
        static_cast<RenderPartObject*>(renderer())->updateWidget();
        needWidgetUpdate = false;
    }
}

void HTMLIFrameElementImpl::recalcStyle( StyleChange ch )
{
    if (needWidgetUpdate) {
        if (renderer())
            static_cast<RenderPartObject*>(renderer())->updateWidget();
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
    return attr->name() == srcAttr;
}

DOMString HTMLIFrameElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLIFrameElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

DOMString HTMLIFrameElementImpl::height() const
{
    return getAttribute(heightAttr);
}

void HTMLIFrameElementImpl::setHeight(const DOMString &value)
{
    setAttribute(heightAttr, value);
}

DOMString HTMLIFrameElementImpl::src() const
{
    return getDocument()->completeURL(getAttribute(srcAttr));
}

DOMString HTMLIFrameElementImpl::width() const
{
    return getAttribute(widthAttr);
}

void HTMLIFrameElementImpl::setWidth(const DOMString &value)
{
    setAttribute(widthAttr, value);
}

}
