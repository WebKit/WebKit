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
#include "ScriptEventListener.h"
#include "KURL.h"
#include "MappedAttribute.h"
#include "Page.h"
#include "RenderFrame.h"
#include "Settings.h"

namespace WebCore {

using namespace HTMLNames;

HTMLFrameElementBase::HTMLFrameElementBase(const QualifiedName& tagName, Document* document)
    : HTMLFrameOwnerElement(tagName, document)
    , m_scrolling(ScrollbarAuto)
    , m_marginWidth(-1)
    , m_marginHeight(-1)
    , m_checkAttachedTimer(this, &HTMLFrameElementBase::checkAttachedTimerFired)
    , m_viewSource(false)
    , m_shouldOpenURLAfterAttach(false)
    , m_remainsAliveOnRemovalFromTree(false)
{
}

bool HTMLFrameElementBase::isURLAllowed() const
{
    if (m_URL.isEmpty())
        return true;

    const KURL& completeURL = document()->completeURL(m_URL);

    // Don't allow more than 200 total frames in a set. This seems
    // like a reasonable upper bound, and otherwise mutually recursive
    // frameset pages can quickly bring the program to its knees with
    // exponential growth in the number of frames.
    // FIXME: This limit could be higher, but because WebKit has some
    // algorithms that happen while loading which appear to be N^2 or
    // worse in the number of frames, we'll keep it at 200 for now.
    if (Frame* parentFrame = document()->frame()) {
        if (parentFrame->page()->frameCount() > 200)
            return false;
    }

    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    bool foundSelfReference = false;
    for (Frame* frame = document()->frame(); frame; frame = frame->tree()->parent()) {
        if (equalIgnoringFragmentIdentifier(frame->loader()->url(), completeURL)) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    
    return true;
}

void HTMLFrameElementBase::openURL(bool lockHistory, bool lockBackForwardList)
{
    ASSERT(!m_frameName.isEmpty());

    if (!isURLAllowed())
        return;

    if (m_URL.isEmpty())
        m_URL = blankURL().string();

    Frame* parentFrame = document()->frame();
    if (!parentFrame)
        return;

    parentFrame->loader()->requestFrame(this, m_URL, m_frameName, lockHistory, lockBackForwardList);
    if (contentFrame())
        contentFrame()->setInViewSourceMode(viewSourceMode());
}

void HTMLFrameElementBase::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == srcAttr)
        setLocation(deprecatedParseURL(attr->value()));
    else if (attr->name() == idAttributeName()) {
        // Important to call through to base for the id attribute so the hasID bit gets set.
        HTMLFrameOwnerElement::parseMappedAttribute(attr);
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
        HTMLFrameOwnerElement::parseMappedAttribute(attr);
}

void HTMLFrameElementBase::setName()
{
    m_frameName = getAttribute(nameAttr);
    if (m_frameName.isNull())
        m_frameName = getAttribute(idAttributeName());
    
    if (Frame* parentFrame = document()->frame())
        m_frameName = parentFrame->tree()->uniqueChildName(m_frameName);
}

void HTMLFrameElementBase::setNameAndOpenURL()
{
    setName();
    openURL();
}

void HTMLFrameElementBase::setNameAndOpenURLCallback(Node* n)
{
    static_cast<HTMLFrameElementBase*>(n)->setNameAndOpenURL();
}

void HTMLFrameElementBase::updateOnReparenting()
{
    ASSERT(m_remainsAliveOnRemovalFromTree);

    if (Frame* frame = contentFrame())
        frame->transferChildFrameToNewDocument();
}

void HTMLFrameElementBase::insertedIntoDocument()
{
    HTMLFrameOwnerElement::insertedIntoDocument();
    
    // We delay frame loading until after the render tree is fully constructed.
    // Othewise, a synchronous load that executed JavaScript would see incorrect 
    // (0) values for the frame's renderer-dependent properties, like width.
    m_shouldOpenURLAfterAttach = true;

    if (m_remainsAliveOnRemovalFromTree)
        updateOnReparenting();
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
        if (!m_remainsAliveOnRemovalFromTree)
            queuePostAttachCallback(&HTMLFrameElementBase::setNameAndOpenURLCallback, this);
    }

    setRemainsAliveOnRemovalFromTree(false);

    HTMLFrameOwnerElement::attach();
    
    if (RenderPart* renderPart = toRenderPart(renderer())) {
        if (Frame* frame = contentFrame())
            renderPart->setWidget(frame->view());
    }
}

KURL HTMLFrameElementBase::location() const
{
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
    if (Page* page = document()->page())
        page->focusController()->setFocusedFrame(received ? contentFrame() : 0);
}

bool HTMLFrameElementBase::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr;
}

int HTMLFrameElementBase::width() const
{
    if (!renderer())
        return 0;
    
    document()->updateLayoutIgnorePendingStylesheets();
    return toRenderBox(renderer())->width();
}

int HTMLFrameElementBase::height() const
{
    if (!renderer())
        return 0;
    
    document()->updateLayoutIgnorePendingStylesheets();
    return toRenderBox(renderer())->height();
}

void HTMLFrameElementBase::setRemainsAliveOnRemovalFromTree(bool value)
{
    m_remainsAliveOnRemovalFromTree = value;

    // There is a possibility that JS will do document.adoptNode() on this element but will not insert it into the tree.
    // Start the async timer that is normally stopped by attach(). If it's not stopped and fires, it'll unload the frame.
    if (value)
        m_checkAttachedTimer.startOneShot(0);
    else
        m_checkAttachedTimer.stop();
}

void HTMLFrameElementBase::checkAttachedTimerFired(Timer<HTMLFrameElementBase>*)
{
    ASSERT(!attached());
    ASSERT(m_remainsAliveOnRemovalFromTree);

    m_remainsAliveOnRemovalFromTree = false;
    willRemove();
}

void HTMLFrameElementBase::willRemove()
{
    if (m_remainsAliveOnRemovalFromTree)
        return;

    HTMLFrameOwnerElement::willRemove();
}

} // namespace WebCore
