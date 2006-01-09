/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include <assert.h>

#include <q3dict.h>

#include <kurl.h>
#include <klocale.h>

#include <kdom/Helper.h>
#include <kdom/Shared.h>
#include <kdom/Namespace.h>
#include <kdom/core/domattrs.h>
#include <kdom/cache/KDOMLoader.h>
#include "DocLoader.h"
#include <kdom/cache/KDOMCachedObject.h>
#include <kdom/css/CSSStyleSelector.h>
#include <kdom/css/CSSStyleSheetImpl.h>
#include <kdom/events/MouseEventImpl.h>
#include <kdom/css/StyleSheetListImpl.h>
#include <kdom/events/KeyboardEventImpl.h>
#include <kdom/core/ProcessingInstructionImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/RenderPath.h>

#include "ksvg.h"
//#include "Ecma.h"
#include <ksvg2/KSVGView.h>
#include "SVGElementImpl.h"
#include "SVGRenderStyle.h"
#include "SVGZoomEventImpl.h"
#include "KSVGTimeScheduler.h"
#include "SVGSVGElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGScriptElementImpl.h"
#include "SVGElementFactory.h"
#include "SVGStyleElementImpl.h"
#include "SVGTitleElementImpl.h"
#include "SVGDocumentImpl.h"
#include "EventNames.h"

using namespace KSVG;
using namespace khtml;

SVGDocumentImpl::SVGDocumentImpl(SVGDOMImplementationImpl *i, KDOM::KDOMView *view) : KDOM::DocumentImpl(i, view), KDOM::CachedObjectClient()
{
    setPaintDevice(svgView()); // Assign our KSVGView as document paint device

    m_scriptsIt = 0;
    m_cachedScript = 0;
}

SVGDocumentImpl::~SVGDocumentImpl()
{
    // Fire UNLOAD_EVENT upon destruction...
    //if(KDOM::DocumentImpl::hasListenerType(KDOM::UNLOAD_EVENT))
    {
        int exceptioncode;
        RefPtr<KDOM::EventImpl> event = createEvent("SVGEvents", exceptioncode);
        event->initEvent(KDOM::EventNames::unloadEvent, false, false);
        dispatchRecursiveEvent(event.get(), lastChild());
    }

    delete m_scriptsIt;
    delete m_cachedScript;
}

KDOM::DOMString SVGDocumentImpl::title() const
{
    if(rootElement())
    {
        for(NodeImpl *child = rootElement()->firstChild(); child != 0; child = child->nextSibling())
            if(child->hasTagName(SVGNames::titleTag))
                return static_cast<SVGTitleElementImpl *>(child)->title();
    }

    return KDOM::DOMString();
}

KDOM::ElementImpl *SVGDocumentImpl::createElement(const KDOM::DOMString& tagName, int& exceptionCode)
{
    KDOM::QualifiedName qname(KDOM::nullAtom, tagName.impl(), SVGNames::svgNamespaceURI);
    SVGElementImpl *elem = SVGElementFactory::createSVGElement(qname, this, false);
    if(!elem)
        return KDOM::DocumentImpl::createElement(tagName, exceptionCode);

    return elem;
}

SVGSVGElementImpl *SVGDocumentImpl::rootElement() const
{
    KDOM::ElementImpl *elem = documentElement();
    if(elem && elem->hasTagName(SVGNames::svgTag))
        return static_cast<SVGSVGElementImpl *>(elem);

    return 0;
}

void SVGDocumentImpl::notifyFinished(KDOM::CachedObject *finishedObj)
{
    // This is called when a script has finished loading that was requested from
    // executeScripts().  We execute the script, and then call executeScripts()
    // again to continue iterating through the list of scripts in the document
    if(finishedObj == m_cachedScript)
    {
        KDOM::DOMString scriptSource = m_cachedScript->script();
        
        m_cachedScript->deref(this);
        m_cachedScript = 0;
        
        SVGScriptElementImpl::executeScript(this, KDOM::DOMString(scriptSource.qstring()).impl());
        executeScripts(true);
    }
}

KSVGView *SVGDocumentImpl::svgView() const
{
    return static_cast<KSVGView *>(m_view);
}

void SVGDocumentImpl::finishedParsing()
{
    addScripts(rootElement());

    m_scriptsIt = new Q3PtrListIterator<SVGScriptElementImpl>(m_scripts);
    executeScripts(false);
}

void SVGDocumentImpl::dispatchRecursiveEvent(KDOM::EventImpl *event, KDOM::NodeImpl *obj)
{
    // Iterate the tree, backwards, and dispatch the event to every child
    for(KDOM::NodeImpl *n = obj; n != 0; n = n->previousSibling())
    {
        int exceptioncode;
        if(n->hasChildNodes())
        {
            // Dispatch to all children
            dispatchRecursiveEvent(event, n->lastChild());

            // Dispatch, locally
            n->dispatchEvent(event, exceptioncode);
        }
        else
            n->dispatchEvent(event, exceptioncode);
    }
}

void SVGDocumentImpl::dispatchZoomEvent(float prevScale, float newScale)
{
    // dispatch zoom event
    int exceptioncode;
    RefPtr<SVGZoomEventImpl> event = static_cast<SVGZoomEventImpl *>(createEvent("SVGZoomEvents", exceptioncode));
    event->initEvent(KDOM::EventNames::zoomEvent, true, false);
    event->setPreviousScale(prevScale);
    event->setNewScale(newScale);
    rootElement()->dispatchEvent(event.get(), exceptioncode);
}

void SVGDocumentImpl::dispatchScrollEvent()
{
    // dispatch zoom event
    int exceptioncode;
    RefPtr<KDOM::EventImpl> event = createEvent("SVGEvents", exceptioncode);
    event->initEvent(KDOM::EventNames::scrollEvent, true, false);
    rootElement()->dispatchEvent(event.get(), exceptioncode);
}

bool SVGDocumentImpl::dispatchKeyEvent(KDOM::EventTargetImpl *target, QKeyEvent *key, bool keypress)
{
    // dispatch key event
    int exceptioncode;
    RefPtr<KDOM::KeyboardEventImpl> keyEventImpl = static_cast<KDOM::KeyboardEventImpl *>(createEvent("KeyboardEvents", exceptioncode));
    //keyEventImpl->initKeyboardEvent(key);
    target->dispatchEvent(keyEventImpl.get(), exceptioncode);

    return /*keyEventImpl->defaultHandled() ||*/ keyEventImpl->defaultPrevented();
}

KDOM::CSSStyleSelector *SVGDocumentImpl::createStyleSelector(const QString &usersheet)
{
    return new KDOM::CSSStyleSelector(this, usersheet, m_styleSheets, false);
}

void SVGDocumentImpl::addScripts(KDOM::NodeImpl *n)
{
    if(!n)
        return;

    // Recursively go through the entire document tree, looking for svg <script> tags.
    // For each of these that is found, add it to the m_scripts list from which they will be executed
    if (n->hasTagName(SVGNames::scriptTag))
        m_scripts.append(static_cast<SVGScriptElementImpl *>(n));

    NodeImpl *child;
    for(child = n->firstChild(); child != 0; child = child->nextSibling())
        addScripts(child);
}

void SVGDocumentImpl::executeScripts(bool needsStyleSelectorUpdate)
{
    // Iterate through all of the svg <script> tags in the document. For those that have a src attribute,
    // start loading the script and return (executeScripts() will be called again once the script is loaded
    // and continue where it left off). For scripts that don't have a src attribute, execute the code inside the tag
    SVGScriptElementImpl *script = 0;
    while((script = m_scriptsIt->current()))
    {
        KDOM::DOMString hrefAttr(script->href()->baseVal());
        QString charset; // TODO m_scriptsIt->current()->getAttribute(SVGNames::charsetAttr).qstring();

        if(!hrefAttr.isEmpty())
        {
            // we have a src attribute
            m_cachedScript = docLoader()->requestScript(hrefAttr, charset);
            ++(*m_scriptsIt);
            m_cachedScript->ref(this); // will call executeScripts() again if already cached
            return;
        }
        else
        {
            // no src attribute - execute from contents of tag
            SVGScriptElementImpl::executeScript(this, script->textContent().impl());
            ++(*m_scriptsIt);

            needsStyleSelectorUpdate = true;
        }
    }

    // Fire LOAD_EVENT after all scripts are evaluated
    if(!m_scriptsIt->current())
    {
        int exceptioncode;
        RefPtr<KDOM::EventImpl> event = createEvent("SVGEvents", exceptioncode);
        event->initEvent(KDOM::EventNames::loadEvent, false, false);
        dispatchRecursiveEvent(event.get(), lastChild());
    }

    // All scripts have finished executing, so calculate the
    // style for the document and close the last element
    if(renderer() && needsStyleSelectorUpdate)
        updateStyleSelector();

    // close any unclosed nodes
    Q3PtrListIterator<SVGElementImpl> it(m_forwardReferences);
    for(;it.current(); ++it)
    {
        if(!it.current()->isClosed())
            it.current()->closeRenderer();
    }
    m_forwardReferences.clear();

    // Start animations, as "load" scripts are executed.
    // FIXME: this won't work for CDF
    rootElement()->timeScheduler()->startAnimations();
}

void SVGDocumentImpl::recalcStyle(StyleChange change)
{
    for(NodeImpl *n = firstChild(); n; n = n->nextSibling())
    {
        if(change >= Inherit || n->hasChangedChild() || n->changed())
            n->recalcStyle(change);
    }
}

void SVGDocumentImpl::dispatchUIEvent(KDOM::EventTargetImpl *target, const KDOM::AtomicString &type)
{
    // Setup kdom 'UIEvent'...
    int exceptioncode;
    RefPtr<KDOM::UIEventImpl> event = static_cast<KDOM::UIEventImpl *>(createEvent("UIEvents", exceptioncode));
    event->initUIEvent(type, true, true, 0, 0);
    target->dispatchEvent(event.get(), exceptioncode);
}

void SVGDocumentImpl::dispatchMouseEvent(KDOM::EventTargetImpl *target, const KDOM::AtomicString &type)
{
    // Setup kdom 'MouseEvent'...
    int exceptioncode;
    RefPtr<KDOM::MouseEventImpl> event = static_cast<KDOM::MouseEventImpl *>(createEvent("MouseEvents", exceptioncode));
    event->initEvent(type, true, true);
    target->dispatchEvent(event.get(), exceptioncode);
}

void SVGDocumentImpl::addForwardReference(const SVGElementImpl *element)
{
    m_forwardReferences.append(element);
}

// vim:ts=4:noet
