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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "config.h"
#include "HTMLFrameElementBase.h"

#include "CSSHelper.h"
#include "Document.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "Page.h"
#include "RenderFrame.h"
#include "Settings.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLFrameElementBase::HTMLFrameElementBase(const QualifiedName& tagName, Document *doc)
    : HTMLFrameOwnerElement(tagName, doc)
    , m_scrolling(ScrollbarAuto)
    , m_marginWidth(-1)
    , m_marginHeight(-1)
    , m_noResize(false)
    , m_viewSource(false)
    , m_shouldOpenURLAfterAttach(false)
{
}

bool HTMLFrameElementBase::isURLAllowed(const AtomicString& URLString) const
{
    if (URLString.isEmpty())
        return true;

    KURL completeURL(document()->completeURL(URLString.deprecatedString()));
    completeURL.setRef(DeprecatedString::null);

    // Don't allow more than 200 total frames in a set. This seems
    // like a reasonable upper bound, and otherwise mutually recursive
    // frameset pages can quickly bring the program to its knees with
    // exponential growth in the number of frames.

    // FIXME: This limit could be higher, but WebKit has some
    // algorithms that happen while loading which appear to be N^2 or
    // worse in the number of frames
    if (Frame* parentFrame = document()->frame())
        if (parentFrame->page()->frameCount() > 200)
            return false;

    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    bool foundSelfReference = false;
    for (Frame* frame = document()->frame(); frame; frame = frame->tree()->parent()) {
        KURL frameURL = frame->loader()->url();
        frameURL.setRef(DeprecatedString::null);
        if (frameURL == completeURL) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    
    return true;
}

void HTMLFrameElementBase::openURL()
{
    ASSERT(!m_name.isEmpty());

    if (!isURLAllowed(m_URL))
        return;

    if (m_URL.isEmpty())
        m_URL = "about:blank";

    Frame* parentFrame = document()->frame();
    if (!parentFrame)
        return;

    parentFrame->loader()->requestFrame(this, m_URL, m_name);
    if (contentFrame())
        contentFrame()->setInViewSourceMode(viewSourceMode());
}

void HTMLFrameElementBase::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == srcAttr)
        setLocation(parseURL(attr->value()));
    else if (attr->name() == idAttr) {
        // Important to call through to base for the id attribute so the hasID bit gets set.
        HTMLFrameOwnerElement::parseMappedAttribute(attr);
        m_name = attr->value();
    } else if (attr->name() == nameAttr) {
        m_name = attr->value();
        // FIXME: If we are already attached, this doesn't actually change the frame's name.
        // FIXME: If we are already attached, this doesn't check for frame name
        // conflicts and generate a unique frame name.
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
            m_scrolling = ScrollbarAuto;
        else if (equalIgnoringCase(attr->value(), "no"))
            m_scrolling = ScrollbarAlwaysOff;
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == viewsourceAttr) {
        m_viewSource = !attr->isNull();
        if (contentFrame())
            contentFrame()->setInViewSourceMode(viewSourceMode());
    } else if (attr->name() == onloadAttr) {
        setHTMLEventListener(loadEvent, attr);
    } else if (attr->name() == onbeforeunloadAttr) {
        // FIXME: should <frame> elements have beforeunload handlers?
        setHTMLEventListener(beforeunloadEvent, attr);
    } else if (attr->name() == onunloadAttr) {
        setHTMLEventListener(unloadEvent, attr);
    } else
        HTMLFrameOwnerElement::parseMappedAttribute(attr);
}

void HTMLFrameElementBase::setNameAndOpenURL()
{
    m_name = getAttribute(nameAttr);
    if (m_name.isNull())
        m_name = getAttribute(idAttr);
    
    if (Frame* parentFrame = document()->frame())
        m_name = parentFrame->tree()->uniqueChildName(m_name);
    
    openURL();
}

void HTMLFrameElementBase::setNameAndOpenURLCallback(Node* n)
{
    static_cast<HTMLFrameElementBase*>(n)->setNameAndOpenURL();
}

void HTMLFrameElementBase::insertedIntoDocument()
{
    HTMLFrameOwnerElement::insertedIntoDocument();
    
    // We delay frame loading until after the render tree is fully constructed.
    // Othewise, a synchronous load that executed JavaScript would see incorrect 
    // (0) values for the frame's renderer-dependent properties, like width.
    m_shouldOpenURLAfterAttach = true;
}

void HTMLFrameElementBase::removedFromDocument()
{
    m_shouldOpenURLAfterAttach = false;

    HTMLFrameOwnerElement::removedFromDocument();
}

void HTMLFrameElementBase::attach()
{
    if (m_shouldOpenURLAfterAttach) {
        m_shouldOpenURLAfterAttach = false;
        queuePostAttachCallback(&HTMLFrameElementBase::setNameAndOpenURLCallback, this);
    }

    HTMLFrameOwnerElement::attach();
    
    if (RenderPart* renderPart = static_cast<RenderPart*>(renderer()))
        if (Frame* frame = contentFrame())
            renderPart->setWidget(frame->view());
}

String HTMLFrameElementBase::location() const
{
    return src();
}

void HTMLFrameElementBase::setLocation(const String& str)
{
    Settings* settings = document()->settings();
    if (settings && settings->needsAcrobatFrameReloadingQuirk() && m_URL == str)
        return;

    m_URL = AtomicString(str);

    if (inDocument())
        openURL();
}

bool HTMLFrameElementBase::isFocusable() const
{
    return renderer();
}

void HTMLFrameElementBase::setFocus(bool received)
{
    HTMLFrameOwnerElement::setFocus(received);
    if (Page* page = document()->page())
        page->focusController()->setFocusedFrame(received ? contentFrame() : 0);
}

bool HTMLFrameElementBase::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr;
}

String HTMLFrameElementBase::frameBorder() const
{
    return getAttribute(frameborderAttr);
}

void HTMLFrameElementBase::setFrameBorder(const String &value)
{
    setAttribute(frameborderAttr, value);
}

String HTMLFrameElementBase::longDesc() const
{
    return getAttribute(longdescAttr);
}

void HTMLFrameElementBase::setLongDesc(const String &value)
{
    setAttribute(longdescAttr, value);
}

String HTMLFrameElementBase::marginHeight() const
{
    return getAttribute(marginheightAttr);
}

void HTMLFrameElementBase::setMarginHeight(const String &value)
{
    setAttribute(marginheightAttr, value);
}

String HTMLFrameElementBase::marginWidth() const
{
    return getAttribute(marginwidthAttr);
}

void HTMLFrameElementBase::setMarginWidth(const String &value)
{
    setAttribute(marginwidthAttr, value);
}

String HTMLFrameElementBase::name() const
{
    return getAttribute(nameAttr);
}

void HTMLFrameElementBase::setName(const String &value)
{
    setAttribute(nameAttr, value);
}

void HTMLFrameElementBase::setNoResize(bool noResize)
{
    setAttribute(noresizeAttr, noResize ? "" : 0);
}

String HTMLFrameElementBase::scrolling() const
{
    return getAttribute(scrollingAttr);
}

void HTMLFrameElementBase::setScrolling(const String &value)
{
    setAttribute(scrollingAttr, value);
}

String HTMLFrameElementBase::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLFrameElementBase::setSrc(const String &value)
{
    setAttribute(srcAttr, value);
}

int HTMLFrameElementBase::width() const
{
    if (!renderer())
        return 0;
    
    document()->updateLayoutIgnorePendingStylesheets();
    return renderer()->width();
}

int HTMLFrameElementBase::height() const
{
    if (!renderer())
        return 0;
    
    document()->updateLayoutIgnorePendingStylesheets();
    return renderer()->height();
}

} // namespace WebCore
