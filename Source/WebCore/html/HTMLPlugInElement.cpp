/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "HTMLPlugInElement.h"

#include "Attribute.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLNames.h"
#include "Page.h"
#include "RenderEmbeddedObject.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "Widget.h"

#if ENABLE(NETSCAPE_PLUGIN_API)
#include "npruntime_impl.h"
#endif

namespace WebCore {

using namespace HTMLNames;

HTMLPlugInElement::HTMLPlugInElement(const QualifiedName& tagName, Document* doc)
    : HTMLFrameOwnerElement(tagName, doc)
    , m_inBeforeLoadEventHandler(false)
#if ENABLE(NETSCAPE_PLUGIN_API)
    , m_NPObject(0)
#endif
    , m_isCapturingMouseEvents(false)
{
}

HTMLPlugInElement::~HTMLPlugInElement()
{
    ASSERT(!m_instance); // cleared in detach()

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (m_NPObject) {
        _NPN_ReleaseObject(m_NPObject);
        m_NPObject = 0;
    }
#endif
}

void HTMLPlugInElement::detach()
{
    m_instance.clear();

    if (m_isCapturingMouseEvents) {
        if (Frame* frame = document()->frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);
        m_isCapturingMouseEvents = false;
    }

    HTMLFrameOwnerElement::detach();
}

void HTMLPlugInElement::removedFromDocument()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (m_NPObject) {
        _NPN_ReleaseObject(m_NPObject);
        m_NPObject = 0;
    }
#endif

    HTMLFrameOwnerElement::removedFromDocument();
}

PassScriptInstance HTMLPlugInElement::getInstance()
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;

    // If the host dynamically turns off JavaScript (or Java) we will still return
    // the cached allocated Bindings::Instance.  Not supporting this edge-case is OK.
    if (m_instance)
        return m_instance;

    if (Widget* widget = pluginWidget())
        m_instance = frame->script()->createScriptInstanceForWidget(widget);

    return m_instance;
}

bool HTMLPlugInElement::guardedDispatchBeforeLoadEvent(const String& sourceURL)
{
    ASSERT(!m_inBeforeLoadEventHandler);
    m_inBeforeLoadEventHandler = true;
    // static_cast is used to avoid a compile error since dispatchBeforeLoadEvent
    // is intentionally undefined on this class.
    bool beforeLoadAllowedLoad = static_cast<HTMLFrameOwnerElement*>(this)->dispatchBeforeLoadEvent(sourceURL);
    m_inBeforeLoadEventHandler = false;
    return beforeLoadAllowedLoad;
}

Widget* HTMLPlugInElement::pluginWidget()
{
    if (m_inBeforeLoadEventHandler) {
        // The plug-in hasn't loaded yet, and it makes no sense to try to load if beforeload handler happened to touch the plug-in element.
        // That would recursively call beforeload for the same element.
        return 0;
    }

    RenderWidget* renderWidget = renderWidgetForJSBindings();
    if (!renderWidget)
        return 0;

    return renderWidget->widget();
}

void HTMLPlugInElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == widthAttr)
        if (attr->isNull())
            removeCSSProperty(CSSPropertyWidth);
        else
            addCSSLength(CSSPropertyWidth, attr->value());
    else if (attr->name() == heightAttr)
        if (attr->isNull())
            removeCSSProperty(CSSPropertyHeight);
        else
            addCSSLength(CSSPropertyHeight, attr->value());
    else if (attr->name() == vspaceAttr) {
        if (attr->isNull())
            removeCSSProperties(CSSPropertyMarginTop, CSSPropertyMarginBottom);
        else {
            addCSSLength(CSSPropertyMarginTop, attr->value());
            addCSSLength(CSSPropertyMarginBottom, attr->value());
        }
    } else if (attr->name() == hspaceAttr) {
        if (attr->isNull())
            removeCSSProperties(CSSPropertyMarginLeft, CSSPropertyMarginRight);
        else {
            addCSSLength(CSSPropertyMarginLeft, attr->value());
            addCSSLength(CSSPropertyMarginRight, attr->value());
        }
    } else if (attr->name() == alignAttr)
        addHTMLAlignment(attr);
    else
        HTMLFrameOwnerElement::parseMappedAttribute(attr);
}

void HTMLPlugInElement::defaultEventHandler(Event* event)
{
    // Firefox seems to use a fake event listener to dispatch events to plug-in (tested with mouse events only).
    // This is observable via different order of events - in Firefox, event listeners specified in HTML attributes fires first, then an event
    // gets dispatched to plug-in, and only then other event listeners fire. Hopefully, this difference does not matter in practice.

    // FIXME: Mouse down and scroll events are passed down to plug-in via custom code in EventHandler; these code paths should be united.

    RenderObject* r = renderer();
    if (r && r->isEmbeddedObject() && toRenderEmbeddedObject(r)->showsMissingPluginIndicator()) {
        toRenderEmbeddedObject(r)->handleMissingPluginIndicatorEvent(event);
        return;
    }

    if (!r || !r->isWidget())
        return;
    RefPtr<Widget> widget = toRenderWidget(r)->widget();
    if (!widget)
        return;
    widget->handleEvent(event);
    if (event->defaultHandled())
        return;
    HTMLFrameOwnerElement::defaultEventHandler(event);
}

#if ENABLE(NETSCAPE_PLUGIN_API)

NPObject* HTMLPlugInElement::getNPObject()
{
    ASSERT(document()->frame());
    if (!m_NPObject)
        m_NPObject = document()->frame()->script()->createScriptObjectForPluginElement(this);
    return m_NPObject;
}

#endif /* ENABLE(NETSCAPE_PLUGIN_API) */

}
