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
#if SVG_SUPPORT
#include <assert.h>

#include "KURL.h"

#include <kdom/Helper.h>
#include "Shared.h"
#include <kdom/Namespace.h>
#include <kdom/core/domattrs.h>
#include <kdom/cache/KDOMLoader.h>
#include "DocLoader.h"
#include <kdom/cache/KDOMCachedObject.h>
#include "cssstyleselector.h"
#include "css_stylesheetimpl.h"
#include <kdom/events/MouseEventImpl.h>
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

namespace WebCore {

SVGDocumentImpl::SVGDocumentImpl(SVGDOMImplementationImpl *i, KDOMView *view) : DocumentImpl(i, view), CachedObjectClient()
{
    m_scriptsIt = 0;
    m_cachedScript = 0;
}

SVGDocumentImpl::~SVGDocumentImpl()
{
    // Fire UNLOAD_EVENT upon destruction...
    //if(DocumentImpl::hasListenerType(UNLOAD_EVENT))
    {
        ExceptionCode ec;
        RefPtr<EventImpl> event = createEvent("SVGEvents", ec);
        event->initEvent(EventNames::unloadEvent, false, false);
        dispatchRecursiveEvent(event.get(), lastChild());
    }

    delete m_scriptsIt;
    delete m_cachedScript;
}

DOMString SVGDocumentImpl::title() const
{
    if(rootElement())
    {
        for(NodeImpl *child = rootElement()->firstChild(); child != 0; child = child->nextSibling())
            if(child->hasTagName(SVGNames::titleTag))
                return static_cast<SVGTitleElementImpl *>(child)->title();
    }

    return DOMString();
}

PassRefPtr<ElementImpl> SVGDocumentImpl::createElement(const DOMString& tagName, ExceptionCode& ec)
{
    QualifiedName qname(nullAtom, tagName.impl(), SVGNames::svgNamespaceURI);
    RefPtr<SVGElementImpl> elem = SVGElementFactory::createSVGElement(qname, this, false);
    if (!elem)
        return DocumentImpl::createElement(tagName, ec);
    return elem;
}

SVGSVGElementImpl *SVGDocumentImpl::rootElement() const
{
    ElementImpl *elem = documentElement();
    if(elem && elem->hasTagName(SVGNames::svgTag))
        return static_cast<SVGSVGElementImpl *>(elem);

    return 0;
}

void SVGDocumentImpl::notifyFinished(CachedObject *finishedObj)
{
    // This is called when a script has finished loading that was requested from
    // executeScripts().  We execute the script, and then call executeScripts()
    // again to continue iterating through the list of scripts in the document
    if(finishedObj == m_cachedScript)
    {
        DOMString scriptSource = m_cachedScript->script();
        
        m_cachedScript->deref(this);
        m_cachedScript = 0;
        
        SVGScriptElementImpl::executeScript(this, DOMString(scriptSource.qstring()).impl());
        executeScripts(true);
    }
}

FrameView *SVGDocumentImpl::svgView() const
{
    return m_view;
}

void SVGDocumentImpl::finishedParsing()
{
    // FIXME: Two problems:
    // 1) This is never called since it's a non-virtual function.
    // 2) If this was called it would need to call the base class's implementation of finishedParsing.

    addScripts(rootElement());

    m_scriptsIt = new Q3PtrListIterator<SVGScriptElementImpl>(m_scripts);
    executeScripts(false);
}

void SVGDocumentImpl::dispatchRecursiveEvent(EventImpl *event, NodeImpl *obj)
{
    // Iterate the tree, backwards, and dispatch the event to every child
    for(NodeImpl *n = obj; n != 0; n = n->previousSibling())
    {
        ExceptionCode ec;
        if(n->hasChildNodes())
        {
            // Dispatch to all children
            dispatchRecursiveEvent(event, n->lastChild());

            // Dispatch, locally
            EventTargetNodeCast(n)->dispatchEvent(event, ec);
        }
        else
            EventTargetNodeCast(n)->dispatchEvent(event, ec);
    }
}

void SVGDocumentImpl::dispatchZoomEvent(float prevScale, float newScale)
{
    // dispatch zoom event
    ExceptionCode ec;
    RefPtr<SVGZoomEventImpl> event = static_pointer_cast<SVGZoomEventImpl>(createEvent("SVGZoomEvents", ec));
    event->initEvent(EventNames::zoomEvent, true, false);
    event->setPreviousScale(prevScale);
    event->setNewScale(newScale);
    rootElement()->dispatchEvent(event.get(), ec);
}

void SVGDocumentImpl::dispatchScrollEvent()
{
    // dispatch zoom event
    ExceptionCode ec;
    RefPtr<EventImpl> event = createEvent("SVGEvents", ec);
    event->initEvent(EventNames::scrollEvent, true, false);
    rootElement()->dispatchEvent(event.get(), ec);
}

CSSStyleSelector *SVGDocumentImpl::createStyleSelector(const QString &usersheet)
{
    return new CSSStyleSelector(this, usersheet, m_styleSheets.get(), false);
}

void SVGDocumentImpl::addScripts(NodeImpl *n)
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
        DOMString hrefAttr(script->href()->baseVal());
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
        ExceptionCode ec;
        RefPtr<EventImpl> event = createEvent("SVGEvents", ec);
        event->initEvent(EventNames::loadEvent, false, false);
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
}

void SVGDocumentImpl::recalcStyle(StyleChange change)
{
    for(NodeImpl *n = firstChild(); n; n = n->nextSibling())
    {
        if(change >= Inherit || n->hasChangedChild() || n->changed())
            n->recalcStyle(change);
    }
}

void SVGDocumentImpl::dispatchUIEvent(EventTargetImpl *target, const AtomicString &type)
{
    // Setup kdom 'UIEvent'...
    ExceptionCode ec;
    RefPtr<UIEventImpl> event = static_pointer_cast<UIEventImpl>(createEvent("UIEvents", ec));
    event->initUIEvent(type, true, true, 0, 0);
    target->dispatchEvent(event.get(), ec);
}

void SVGDocumentImpl::dispatchMouseEvent(EventTargetImpl *target, const AtomicString &type)
{
    // Setup kdom 'MouseEvent'...
    ExceptionCode ec;
    RefPtr<MouseEventImpl> event = static_pointer_cast<MouseEventImpl>(createEvent("MouseEvents", ec));
    event->initEvent(type, true, true);
    target->dispatchEvent(event.get(), ec);
}

void SVGDocumentImpl::addForwardReference(const SVGElementImpl *element)
{
    m_forwardReferences.append(element);
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT
