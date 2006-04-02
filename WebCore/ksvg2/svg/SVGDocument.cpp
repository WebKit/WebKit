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
#include "SVGDocument.h"

#include "CachedScript.h"
#include "DocLoader.h"
#include "EventNames.h"
#include "KSVGTimeScheduler.h"
#include "KURL.h"
#include "SVGAnimatedString.h"
#include "SVGDOMImplementation.h"
#include "SVGElement.h"
#include "SVGElementFactory.h"
#include "SVGRenderStyle.h"
#include "SVGSVGElement.h"
#include "SVGScriptElement.h"
#include "SVGStyleElement.h"
#include "SVGTitleElement.h"
#include "SVGZoomEvent.h"
#include "Shared.h"
#include "css_stylesheetimpl.h"
#include "cssstyleselector.h"
#include "ksvg.h"
#include <assert.h>
#include <kcanvas/RenderPath.h>

namespace WebCore {

SVGDocument::SVGDocument(SVGDOMImplementation *i, KDOMView *view) : Document(i, view), CachedObjectClient()
{
    m_scriptsIt = 0;
    m_cachedScript = 0;
}

SVGDocument::~SVGDocument()
{
    // Fire UNLOAD_EVENT upon destruction...
    //if(Document::hasListenerType(UNLOAD_EVENT))
    {
        ExceptionCode ec;
        RefPtr<Event> event = createEvent("SVGEvents", ec);
        event->initEvent(EventNames::unloadEvent, false, false);
        dispatchRecursiveEvent(event.get(), lastChild());
    }

    delete m_scriptsIt;
    delete m_cachedScript;
}

String SVGDocument::title() const
{
    if(rootElement())
    {
        for(Node *child = rootElement()->firstChild(); child != 0; child = child->nextSibling())
            if(child->hasTagName(SVGNames::titleTag))
                return static_cast<SVGTitleElement *>(child)->title();
    }

    return String();
}

PassRefPtr<Element> SVGDocument::createElement(const String& tagName, ExceptionCode& ec)
{
    QualifiedName qname(nullAtom, tagName.impl(), SVGNames::svgNamespaceURI);
    RefPtr<SVGElement> elem = SVGElementFactory::createSVGElement(qname, this, false);
    if (!elem)
        return Document::createElement(tagName, ec);
    return elem;
}

SVGSVGElement *SVGDocument::rootElement() const
{
    Element *elem = documentElement();
    if(elem && elem->hasTagName(SVGNames::svgTag))
        return static_cast<SVGSVGElement *>(elem);

    return 0;
}

void SVGDocument::notifyFinished(CachedObject *finishedObj)
{
    // This is called when a script has finished loading that was requested from
    // executeScripts().  We execute the script, and then call executeScripts()
    // again to continue iterating through the list of scripts in the document
    if(finishedObj == m_cachedScript)
    {
        String scriptSource = m_cachedScript->script();
        
        m_cachedScript->deref(this);
        m_cachedScript = 0;
        
        SVGScriptElement::executeScript(this, String(scriptSource.deprecatedString()).impl());
        executeScripts(true);
    }
}

FrameView *SVGDocument::svgView() const
{
    return m_view;
}

void SVGDocument::finishedParsing()
{
    // FIXME: Two problems:
    // 1) This is never called since it's a non-virtual function.
    // 2) If this was called it would need to call the base class's implementation of finishedParsing.

    addScripts(rootElement());

    m_scriptsIt = new Q3PtrListIterator<SVGScriptElement>(m_scripts);
    executeScripts(false);
}

void SVGDocument::dispatchRecursiveEvent(Event *event, Node *obj)
{
    // Iterate the tree, backwards, and dispatch the event to every child
    for(Node *n = obj; n != 0; n = n->previousSibling())
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

void SVGDocument::dispatchZoomEvent(float prevScale, float newScale)
{
    // dispatch zoom event
    ExceptionCode ec;
    RefPtr<SVGZoomEvent> event = static_pointer_cast<SVGZoomEvent>(createEvent("SVGZoomEvents", ec));
    event->initEvent(EventNames::zoomEvent, true, false);
    event->setPreviousScale(prevScale);
    event->setNewScale(newScale);
    rootElement()->dispatchEvent(event.get(), ec);
}

void SVGDocument::dispatchScrollEvent()
{
    // dispatch zoom event
    ExceptionCode ec;
    RefPtr<Event> event = createEvent("SVGEvents", ec);
    event->initEvent(EventNames::scrollEvent, true, false);
    rootElement()->dispatchEvent(event.get(), ec);
}

CSSStyleSelector *SVGDocument::createStyleSelector(const DeprecatedString &usersheet)
{
    return new CSSStyleSelector(this, usersheet, m_styleSheets.get(), false);
}

void SVGDocument::addScripts(Node *n)
{
    if(!n)
        return;

    // Recursively go through the entire document tree, looking for svg <script> tags.
    // For each of these that is found, add it to the m_scripts list from which they will be executed
    if (n->hasTagName(SVGNames::scriptTag))
        m_scripts.append(static_cast<SVGScriptElement *>(n));

    Node *child;
    for(child = n->firstChild(); child != 0; child = child->nextSibling())
        addScripts(child);
}

void SVGDocument::executeScripts(bool needsStyleSelectorUpdate)
{
    // Iterate through all of the svg <script> tags in the document. For those that have a src attribute,
    // start loading the script and return (executeScripts() will be called again once the script is loaded
    // and continue where it left off). For scripts that don't have a src attribute, execute the code inside the tag
    SVGScriptElement *script = 0;
    while((script = m_scriptsIt->current()))
    {
        String hrefAttr(script->href()->baseVal());
        DeprecatedString charset; // TODO m_scriptsIt->current()->getAttribute(SVGNames::charsetAttr).deprecatedString();

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
            SVGScriptElement::executeScript(this, script->textContent().impl());
            ++(*m_scriptsIt);

            needsStyleSelectorUpdate = true;
        }
    }

    // Fire LOAD_EVENT after all scripts are evaluated
    if(!m_scriptsIt->current())
    {
        ExceptionCode ec;
        RefPtr<Event> event = createEvent("SVGEvents", ec);
        event->initEvent(EventNames::loadEvent, false, false);
        dispatchRecursiveEvent(event.get(), lastChild());
    }

    // All scripts have finished executing, so calculate the
    // style for the document and close the last element
    if(renderer() && needsStyleSelectorUpdate)
        updateStyleSelector();

    // close any unclosed nodes
    Q3PtrListIterator<SVGElement> it(m_forwardReferences);
    for(;it.current(); ++it)
    {
        if(!it.current()->isClosed())
            it.current()->closeRenderer();
    }
    m_forwardReferences.clear();
}

void SVGDocument::recalcStyle(StyleChange change)
{
    for(Node *n = firstChild(); n; n = n->nextSibling())
    {
        if(change >= Inherit || n->hasChangedChild() || n->changed())
            n->recalcStyle(change);
    }
}

void SVGDocument::addForwardReference(const SVGElement *element)
{
    m_forwardReferences.append(element);
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT
