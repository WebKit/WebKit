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
#include "config.h"
#include "HTMLFrameElement.h"

#include "csshelper.h"
#include "Document.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLFrameSetElement.h"
#include "Page.h"
#include "RenderFrame.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

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
    
    FrameView* w = document()->view();
    if (!w)
        return false;

    KURL newURL(document()->completeURL(URLString.deprecatedString()));
    newURL.setRef(DeprecatedString::null);

    // Don't allow more than 1000 total frames in a set. This seems
    // like a reasonable upper bound, and otherwise mutually recursive
    // frameset pages can quickly bring the program to its knees with
    // exponential growth in the number of frames.

    // FIXME: This limit could be higher, but WebKit has some
    // algorithms that happen while loading which appear to be N^2 or
    // worse in the number of frames
    if (w->frame()->page()->frameCount() >= 200)
        return false;

    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    bool foundSelfReference = false;
    for (Frame *frame = w->frame(); frame; frame = frame->tree()->parent()) {
        KURL frameURL = frame->url();
        frameURL.setRef(DeprecatedString::null);
        if (frameURL == newURL) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    
    return true;
}

void HTMLFrameElement::openURL()
{
    FrameView *w = document()->view();
    if (!w)
        return;
    
    AtomicString relativeURL = m_URL;
    if (relativeURL.isEmpty())
        relativeURL = "about:blank";

    // Load the frame contents.
    Frame* parentFrame = w->frame();
    if (Frame* childFrame = parentFrame->tree()->child(m_name))
        childFrame->openURL(document()->completeURL(relativeURL.deprecatedString()));
    else
        parentFrame->requestFrame(static_cast<RenderFrame *>(renderer()), relativeURL, m_name);
}

void HTMLFrameElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == srcAttr)
        setLocation(WebCore::parseURL(attr->value()));
    else if (attr->name() == idAttr) {
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

    Frame* frame = document()->frame();

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
    Frame* frame = document()->frame();
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
    
    if (!isURLAllowed(m_URL))
        return;

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
    Frame* parentFrame = document()->frame();
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
    
    document()->updateLayoutIgnorePendingStylesheets();
    return renderer()->width();
}

int HTMLFrameElement::frameHeight() const
{
    if (!renderer())
        return 0;
    
    document()->updateLayoutIgnorePendingStylesheets();
    return renderer()->height();
}

}
