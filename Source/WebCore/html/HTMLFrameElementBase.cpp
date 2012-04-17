/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#include "Attribute.h"
#include "Document.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "KURL.h"
#include "Page.h"
#include "RenderPart.h"
#include "ScriptController.h"
#include "ScriptEventListener.h"
#include "Settings.h"

namespace WebCore {

using namespace HTMLNames;

HTMLFrameElementBase::HTMLFrameElementBase(const QualifiedName& tagName, Document* document)
    : HTMLFrameOwnerElement(tagName, document)
    , m_scrolling(ScrollbarAuto)
    , m_marginWidth(-1)
    , m_marginHeight(-1)
    , m_viewSource(false)
{
}

bool HTMLFrameElementBase::isURLAllowed() const
{
    if (m_URL.isEmpty())
        return true;

    const KURL& completeURL = document()->completeURL(m_URL);

    if (protocolIsJavaScript(completeURL)) { 
        Document* contentDoc = this->contentDocument();
        if (contentDoc && !ScriptController::canAccessFromCurrentOrigin(contentDoc->frame()))
            return false;
    }

    if (Frame* parentFrame = document()->frame()) {
        if (parentFrame->page()->frameCount() >= Page::maxNumberOfFrames)
            return false;
    }

    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    bool foundSelfReference = false;
    for (Frame* frame = document()->frame(); frame; frame = frame->tree()->parent()) {
        if (equalIgnoringFragmentIdentifier(frame->document()->url(), completeURL)) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    
    return true;
}

void HTMLFrameElementBase::openURL(bool lockHistory, bool lockBackForwardList)
{
    if (!isURLAllowed())
        return;

    if (m_URL.isEmpty())
        m_URL = blankURL().string();

    Frame* parentFrame = document()->frame();
    if (!parentFrame)
        return;

    parentFrame->loader()->subframeLoader()->requestFrame(this, m_URL, m_frameName, lockHistory, lockBackForwardList);
    if (contentFrame())
        contentFrame()->setInViewSourceMode(viewSourceMode());
}

void HTMLFrameElementBase::parseAttribute(Attribute* attr)
{
    if (attr->name() == srcdocAttr)
        setLocation("about:srcdoc");
    else if (attr->name() == srcAttr && !fastHasAttribute(srcdocAttr))
        setLocation(stripLeadingAndTrailingHTMLSpaces(attr->value()));
    else if (isIdAttributeName(attr->name())) {
        // Important to call through to base for the id attribute so the hasID bit gets set.
        HTMLFrameOwnerElement::parseAttribute(attr);
        m_frameName = attr->value();
    } else if (attr->name() == nameAttr) {
        m_frameName = attr->value();
        // FIXME: If we are already attached, this doesn't actually change the frame's name.
        // FIXME: If we are already attached, this doesn't check for frame name
        // conflicts and generate a unique frame name.
    } else if (attr->name() == marginwidthAttr) {
        m_marginWidth = attr->value().toInt();
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == marginheightAttr) {
        m_marginHeight = attr->value().toInt();
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == scrollingAttr) {
        // Auto and yes both simply mean "allow scrolling." No means "don't allow scrolling."
        if (equalIgnoringCase(attr->value(), "auto") || equalIgnoringCase(attr->value(), "yes"))
            m_scrolling = document()->frameElementsShouldIgnoreScrolling() ? ScrollbarAlwaysOff : ScrollbarAuto;
        else if (equalIgnoringCase(attr->value(), "no"))
            m_scrolling = ScrollbarAlwaysOff;
        // FIXME: If we are already attached, this has no effect.
    } else if (attr->name() == viewsourceAttr) {
        m_viewSource = !attr->isNull();
        if (contentFrame())
            contentFrame()->setInViewSourceMode(viewSourceMode());
    } else if (attr->name() == onloadAttr)
        setAttributeEventListener(eventNames().loadEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onbeforeloadAttr)
        setAttributeEventListener(eventNames().beforeloadEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onbeforeunloadAttr) {
        // FIXME: should <frame> elements have beforeunload handlers?
        setAttributeEventListener(eventNames().beforeunloadEvent, createAttributeEventListener(this, attr));
    } else
        HTMLFrameOwnerElement::parseAttribute(attr);
}

void HTMLFrameElementBase::setNameAndOpenURL()
{
    m_frameName = getNameAttribute();
    if (m_frameName.isNull())
        m_frameName = getIdAttribute();
    openURL();
}

Node::InsertionNotificationRequest HTMLFrameElementBase::insertedInto(Node* insertionPoint)
{
    HTMLFrameOwnerElement::insertedInto(insertionPoint);
    if (insertionPoint->inDocument())
        return InsertionShouldCallDidNotifyDescendantInseretions;
    return InsertionDone;
}

void HTMLFrameElementBase::didNotifyDescendantInseretions(Node* insertionPoint)
{
    ASSERT_UNUSED(insertionPoint, insertionPoint->inDocument());

    // DocumentFragments don't kick of any loads.
    if (!document()->frame())
        return;

    // JavaScript in src=javascript: and beforeonload can access the renderer
    // during attribute parsing *before* the normal parser machinery would
    // attach the element. To support this, we lazyAttach here, but only
    // if we don't already have a renderer (if we're inserted
    // as part of a DocumentFragment, insertedInto from an earlier element
    // could have forced a style resolve and already attached us).
    if (!renderer())
        lazyAttach(DoNotSetAttached);
    setNameAndOpenURL();
}

void HTMLFrameElementBase::attach()
{
    HTMLFrameOwnerElement::attach();

    if (RenderPart* part = renderPart()) {
        if (Frame* frame = contentFrame())
            part->setWidget(frame->view());
    }
}

KURL HTMLFrameElementBase::location() const
{
    if (fastHasAttribute(srcdocAttr))
        return KURL(ParsedURLString, "about:srcdoc");
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLFrameElementBase::setLocation(const String& str)
{
    Settings* settings = document()->settings();
    if (settings && settings->needsAcrobatFrameReloadingQuirk() && m_URL == str)
        return;

    m_URL = AtomicString(str);

    if (inDocument())
        openURL(false, false);
}

bool HTMLFrameElementBase::supportsFocus() const
{
    return true;
}

void HTMLFrameElementBase::setFocus(bool received)
{
    HTMLFrameOwnerElement::setFocus(received);
    if (Page* page = document()->page()) {
        if (received)
            page->focusController()->setFocusedFrame(contentFrame());
        else if (page->focusController()->focusedFrame() == contentFrame()) // Focus may have already been given to another frame, don't take it away.
            page->focusController()->setFocusedFrame(0);
    }
}

bool HTMLFrameElementBase::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr || HTMLFrameOwnerElement::isURLAttribute(attr);
}

int HTMLFrameElementBase::width()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (!renderBox())
        return 0;
    return renderBox()->width();
}

int HTMLFrameElementBase::height()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (!renderBox())
        return 0;
    return renderBox()->height();
}

#if ENABLE(FULLSCREEN_API)
bool HTMLFrameElementBase::allowFullScreen() const
{
    return hasAttribute(webkitallowfullscreenAttr);
}
#endif

} // namespace WebCore
