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
#include "CSSPropertyNames.h"
#include "cssstyleselector.h"
#include "CSSValueKeywords.h"
#include "dom2_eventsimpl.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "loader.h"
#include "render_frames.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLBodyElement::HTMLBodyElement(Document *doc)
    : HTMLElement(bodyTag, doc)
{
}

HTMLBodyElement::~HTMLBodyElement()
{
    if (m_linkDecl) {
        m_linkDecl->setNode(0);
        m_linkDecl->setParent(0);
    }
}

void HTMLBodyElement::createLinkDecl()
{
    m_linkDecl = new CSSMutableStyleDeclaration;
    m_linkDecl->setParent(getDocument()->elementSheet());
    m_linkDecl->setNode(this);
    m_linkDecl->setStrictParsing(!getDocument()->inCompatMode());
}

bool HTMLBodyElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
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

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLBodyElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == backgroundAttr) {
        String url = WebCore::parseURL(attr->value());
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
            RefPtr<CSSValue> val = m_linkDecl->getPropertyCSSValue(CSS_PROP_COLOR);
            if (val && val->isPrimitiveValue()) {
                Color col = getDocument()->styleSelector()->getColorFromPrimitiveValue(static_cast<CSSPrimitiveValue*>(val.get()));
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
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLBodyElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();

    // FIXME: perhaps this code should be in attach() instead of here

    FrameView *w = getDocument()->view();
    if (w && w->marginWidth() != -1) {
        DeprecatedString s;
        s.sprintf("%d", w->marginWidth());
        setAttribute(marginwidthAttr, s);
    }
    if (w && w->marginHeight() != -1) {
        DeprecatedString s;
        s.sprintf("%d", w->marginHeight());
        setAttribute(marginheightAttr, s);
    }

    if (w)
        w->scheduleRelayout();
}

bool HTMLBodyElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == backgroundAttr;
}

String HTMLBodyElement::aLink() const
{
    return getAttribute(alinkAttr);
}

void HTMLBodyElement::setALink(const String &value)
{
    setAttribute(alinkAttr, value);
}

String HTMLBodyElement::background() const
{
    return getAttribute(backgroundAttr);
}

void HTMLBodyElement::setBackground(const String &value)
{
    setAttribute(backgroundAttr, value);
}

String HTMLBodyElement::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLBodyElement::setBgColor(const String &value)
{
    setAttribute(bgcolorAttr, value);
}

String HTMLBodyElement::link() const
{
    return getAttribute(linkAttr);
}

void HTMLBodyElement::setLink(const String &value)
{
    setAttribute(linkAttr, value);
}

String HTMLBodyElement::text() const
{
    return getAttribute(textAttr);
}

void HTMLBodyElement::setText(const String &value)
{
    setAttribute(textAttr, value);
}

String HTMLBodyElement::vLink() const
{
    return getAttribute(vlinkAttr);
}

void HTMLBodyElement::setVLink(const String &value)
{
    setAttribute(vlinkAttr, value);
}

// -------------------------------------------------------------------------

HTMLFrameElement::HTMLFrameElement(Document *doc)
    : HTMLElement(frameTag, doc)
{
    init();
}

HTMLFrameElement::HTMLFrameElement(const QualifiedName& tagName, Document *doc)
    : HTMLElement(tagName, doc)
{
    init();
}

void HTMLFrameElement::init()
{
    m_frameBorder = true;
    m_frameBorderSet = false;
    m_marginWidth = -1;
    m_marginHeight = -1;
    m_scrolling = ScrollBarAuto;
    m_noResize = false;
}

HTMLFrameElement::~HTMLFrameElement()
{
}

bool HTMLFrameElement::isURLAllowed(const AtomicString &URLString) const
{
    if (URLString.isEmpty())
        return true;
    
    FrameView *w = getDocument()->view();
    if (!w)
        return false;

    KURL newURL(getDocument()->completeURL(URLString.deprecatedString()));
    newURL.setRef(DeprecatedString::null);

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
        frameURL.setRef(DeprecatedString::null);
        if (frameURL == newURL) {
            if (foundSelfReference) {
                return false;
            }
            foundSelfReference = true;
        }
    }
    
    return true;
}

void HTMLFrameElement::openURL()
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
        childFrame->openURL(getDocument()->completeURL(relativeURL.deprecatedString()));
    else
        parentFrame->requestFrame(static_cast<RenderFrame *>(renderer()), relativeURL, m_name);
}


void HTMLFrameElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == srcAttr) {
        setLocation(WebCore::parseURL(attr->value()));
    } else if (attr->name() == idAttr) {
        // Important to call through to base for the id attribute so the hasID bit gets set.
        HTMLElement::parseMappedAttribute(attr);
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
        HTMLElement::parseMappedAttribute(attr);
}

bool HTMLFrameElement::rendererIsNeeded(RenderStyle *style)
{
    // Ignore display: none.
    return isURLAllowed(m_URL);
}

RenderObject *HTMLFrameElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderFrame(this);
}

void HTMLFrameElement::attach()
{
    m_name = getAttribute(nameAttr);
    if (m_name.isNull())
        m_name = getAttribute(idAttr);

    // inherit default settings from parent frameset
    for (Node *node = parentNode(); node; node = node->parentNode())
        if (node->hasTagName(framesetTag)) {
            HTMLFrameSetElement* frameset = static_cast<HTMLFrameSetElement*>(node);
            if (!m_frameBorderSet)
                m_frameBorder = frameset->frameBorder();
            if (!m_noResize)
                m_noResize = frameset->noResize();
            break;
        }

    HTMLElement::attach();

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
    frame->requestFrame(static_cast<RenderFrame*>(renderer()), relativeURL, m_name);
}

void HTMLFrameElement::close()
{
    Frame* frame = getDocument()->frame();
    if (renderer() && frame) {
        frame->page()->decrementFrameCount();
        if (Frame* childFrame = frame->tree()->child(m_name))
            childFrame->frameDetached();
    }
}

void HTMLFrameElement::willRemove()
{
    // close the frame and dissociate the renderer, but leave the
    // node attached so that frame does not get re-attached before
    // actually leaving the document.  see <rdar://problem/4132581>
    close();
    if (renderer()) {
        renderer()->destroy();
        setRenderer(0);
    }
    
    HTMLElement::willRemove();
}

void HTMLFrameElement::detach()
{
    close();
    HTMLElement::detach();
}

void HTMLFrameElement::setLocation(const String& str)
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

bool HTMLFrameElement::isFocusable() const
{
    return renderer();
}

void HTMLFrameElement::setFocus(bool received)
{
    HTMLElement::setFocus(received);
    WebCore::RenderFrame *renderFrame = static_cast<WebCore::RenderFrame *>(renderer());
    if (!renderFrame || !renderFrame->widget())
        return;
    if (received)
        renderFrame->widget()->setFocus();
    else
        renderFrame->widget()->clearFocus();
}

Frame* HTMLFrameElement::contentFrame() const
{
    // Start with the part that contains this element, our ownerDocument.
    Frame* parentFrame = getDocument()->frame();
    if (!parentFrame)
        return 0;

    // Find the part for the subframe that this element represents.
    return parentFrame->tree()->child(m_name);
}

Document* HTMLFrameElement::contentDocument() const
{
    Frame* frame = contentFrame();
    if (!frame)
        return 0;
    return frame->document();
}

bool HTMLFrameElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr;
}

String HTMLFrameElement::frameBorder() const
{
    return getAttribute(frameborderAttr);
}

void HTMLFrameElement::setFrameBorder(const String &value)
{
    setAttribute(frameborderAttr, value);
}

String HTMLFrameElement::longDesc() const
{
    return getAttribute(longdescAttr);
}

void HTMLFrameElement::setLongDesc(const String &value)
{
    setAttribute(longdescAttr, value);
}

String HTMLFrameElement::marginHeight() const
{
    return getAttribute(marginheightAttr);
}

void HTMLFrameElement::setMarginHeight(const String &value)
{
    setAttribute(marginheightAttr, value);
}

String HTMLFrameElement::marginWidth() const
{
    return getAttribute(marginwidthAttr);
}

void HTMLFrameElement::setMarginWidth(const String &value)
{
    setAttribute(marginwidthAttr, value);
}

String HTMLFrameElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLFrameElement::setName(const String &value)
{
    setAttribute(nameAttr, value);
}

void HTMLFrameElement::setNoResize(bool noResize)
{
    setAttribute(noresizeAttr, noResize ? "" : 0);
}

String HTMLFrameElement::scrolling() const
{
    return getAttribute(scrollingAttr);
}

void HTMLFrameElement::setScrolling(const String &value)
{
    setAttribute(scrollingAttr, value);
}

String HTMLFrameElement::src() const
{
    return getAttribute(srcAttr);
}

void HTMLFrameElement::setSrc(const String &value)
{
    setAttribute(srcAttr, value);
}

int HTMLFrameElement::frameWidth() const
{
    if (!renderer())
        return 0;
    
    getDocument()->updateLayoutIgnorePendingStylesheets();
    return renderer()->width();
}

int HTMLFrameElement::frameHeight() const
{
    if (!renderer())
        return 0;
    
    getDocument()->updateLayoutIgnorePendingStylesheets();
    return renderer()->height();
}

// -------------------------------------------------------------------------

HTMLFrameSetElement::HTMLFrameSetElement(Document *doc)
    : HTMLElement(framesetTag, doc)
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

HTMLFrameSetElement::~HTMLFrameSetElement()
{
    if (m_rows)
        delete [] m_rows;
    if (m_cols)
        delete [] m_cols;
}

bool HTMLFrameSetElement::checkDTD(const Node* newChild)
{
    // FIXME: Old code had adjacent double returns and seemed to want to do something with NOFRAMES (but didn't).
    // What is the correct behavior?
    return newChild->hasTagName(framesetTag) || newChild->hasTagName(frameTag);
}

void HTMLFrameSetElement::parseMappedAttribute(MappedAttribute *attr)
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
        HTMLElement::parseMappedAttribute(attr);
}

bool HTMLFrameSetElement::rendererIsNeeded(RenderStyle *style)
{
    // Ignore display: none but do pay attention if a stylesheet has caused us to delay our loading.
    return style->isStyleAvailable();
}

RenderObject *HTMLFrameSetElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderFrameSet(this);
}

void HTMLFrameSetElement::attach()
{
    // inherit default settings from parent frameset
    HTMLElement* node = static_cast<HTMLElement*>(parentNode());
    while(node)
    {
        if (node->hasTagName(framesetTag)) {
            HTMLFrameSetElement* frameset = static_cast<HTMLFrameSetElement*>(node);
            if(!frameBorderSet)  frameborder = frameset->frameBorder();
            if(!noresize)  noresize = frameset->noResize();
            break;
        }
        node = static_cast<HTMLElement*>(node->parentNode());
    }

    HTMLElement::attach();
}

void HTMLFrameSetElement::defaultEventHandler(Event *evt)
{
    if (evt->isMouseEvent() && !noresize && renderer()) {
        static_cast<WebCore::RenderFrameSet *>(renderer())->userResize(static_cast<MouseEvent*>(evt));
        evt->setDefaultHandled();
    }

    HTMLElement::defaultEventHandler(evt);
}

void HTMLFrameSetElement::recalcStyle( StyleChange ch )
{
    if (changed() && renderer()) {
        renderer()->setNeedsLayout(true);
        setChanged(false);
    }
    HTMLElement::recalcStyle( ch );
}

String HTMLFrameSetElement::cols() const
{
    return getAttribute(colsAttr);
}

void HTMLFrameSetElement::setCols(const String &value)
{
    setAttribute(colsAttr, value);
}

String HTMLFrameSetElement::rows() const
{
    return getAttribute(rowsAttr);
}

void HTMLFrameSetElement::setRows(const String &value)
{
    setAttribute(rowsAttr, value);
}

// -------------------------------------------------------------------------

HTMLHeadElement::HTMLHeadElement(Document *doc)
    : HTMLElement(headTag, doc)
{
}

HTMLHeadElement::~HTMLHeadElement()
{
}

String HTMLHeadElement::profile() const
{
    return getAttribute(profileAttr);
}

void HTMLHeadElement::setProfile(const String &value)
{
    setAttribute(profileAttr, value);
}

bool HTMLHeadElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(titleTag) || newChild->hasTagName(isindexTag) ||
           newChild->hasTagName(baseTag) || newChild->hasTagName(scriptTag) ||
           newChild->hasTagName(styleTag) || newChild->hasTagName(metaTag) ||
           newChild->hasTagName(linkTag) || newChild->isTextNode();
}

// -------------------------------------------------------------------------

HTMLHtmlElement::HTMLHtmlElement(Document *doc)
    : HTMLElement(htmlTag, doc)
{
}

HTMLHtmlElement::~HTMLHtmlElement()
{
}

String HTMLHtmlElement::version() const
{
    return getAttribute(versionAttr);
}

void HTMLHtmlElement::setVersion(const String &value)
{
    setAttribute(versionAttr, value);
}

bool HTMLHtmlElement::checkDTD(const Node* newChild)
{
    // FIXME: Why is <script> allowed here?
    return newChild->hasTagName(headTag) || newChild->hasTagName(bodyTag) ||
           newChild->hasTagName(framesetTag) || newChild->hasTagName(noframesTag) ||
           newChild->hasTagName(scriptTag);
}

// -------------------------------------------------------------------------

HTMLIFrameElement::HTMLIFrameElement(Document *doc) : HTMLFrameElement(iframeTag, doc)
{
    m_frameBorder = false;
    m_marginWidth = -1;
    m_marginHeight = -1;
    needWidgetUpdate = false;
}

HTMLIFrameElement::~HTMLIFrameElement()
{
}

bool HTMLIFrameElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr || attrName == heightAttr) {
        result = eUniversal;
        return false;
    }
    
    if (attrName == alignAttr) {
        result = eReplaced; // Share with <img> since the alignment behavior is the same.
        return false;
    }
    
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLIFrameElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == widthAttr)
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    else if (attr->name() == heightAttr)
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    else if (attr->name() == alignAttr)
        addHTMLAlignment(attr);
    else if (attr->name() == nameAttr) {
        String newNameAttr = attr->value();
        if (inDocument() && getDocument()->isHTMLDocument()) {
            HTMLDocument *document = static_cast<HTMLDocument *>(getDocument());
            document->removeDocExtraNamedItem(oldNameAttr);
            document->addDocExtraNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else
        HTMLFrameElement::parseMappedAttribute(attr);
}

void HTMLIFrameElement::insertedIntoDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocument *document = static_cast<HTMLDocument *>(getDocument());
        document->addDocExtraNamedItem(oldNameAttr);
    }

    HTMLElement::insertedIntoDocument();
}

void HTMLIFrameElement::removedFromDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocument *document = static_cast<HTMLDocument *>(getDocument());
        document->removeDocExtraNamedItem(oldNameAttr);
    }

    HTMLElement::removedFromDocument();
}

bool HTMLIFrameElement::rendererIsNeeded(RenderStyle *style)
{
    // Don't ignore display: none the way frame does.
    return isURLAllowed(m_URL) && style->display() != NONE;
}

RenderObject *HTMLIFrameElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderPartObject(this);
}

void HTMLIFrameElement::attach()
{
    m_name = getAttribute(nameAttr);
    if (m_name.isNull())
        m_name = getAttribute(idAttr);

    HTMLElement::attach();

    Frame* parentFrame = getDocument()->frame();
    if (renderer() && parentFrame) {
        parentFrame->page()->incrementFrameCount();
        m_name = parentFrame->tree()->uniqueChildName(m_name);
        static_cast<RenderPartObject*>(renderer())->updateWidget();
        needWidgetUpdate = false;
    }
}

void HTMLIFrameElement::recalcStyle( StyleChange ch )
{
    if (needWidgetUpdate) {
        if (renderer())
            static_cast<RenderPartObject*>(renderer())->updateWidget();
        needWidgetUpdate = false;
    }
    HTMLElement::recalcStyle( ch );
}

void HTMLIFrameElement::openURL()
{
    needWidgetUpdate = true;
    setChanged();
}

bool HTMLIFrameElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr;
}

String HTMLIFrameElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLIFrameElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

String HTMLIFrameElement::height() const
{
    return getAttribute(heightAttr);
}

void HTMLIFrameElement::setHeight(const String &value)
{
    setAttribute(heightAttr, value);
}

String HTMLIFrameElement::src() const
{
    return getDocument()->completeURL(getAttribute(srcAttr));
}

String HTMLIFrameElement::width() const
{
    return getAttribute(widthAttr);
}

void HTMLIFrameElement::setWidth(const String &value)
{
    setAttribute(widthAttr, value);
}

}
