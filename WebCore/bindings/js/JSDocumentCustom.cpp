/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSDocument.h"

#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "JSDOMWindowCustom.h"
#include "JSHTMLDocument.h"
#include "JSLocation.h"
#include "Location.h"
#include "ScriptController.h"

#if ENABLE(SVG)
#include "JSSVGDocument.h"
#include "SVGDocument.h"
#endif

using namespace JSC;

namespace WebCore {

void JSDocument::mark()
{
    JSNode::mark();
    markDOMNodesForDocument(impl());
    markActiveObjectsForContext(*Heap::heap(this)->globalData(), impl());
}

JSValue JSDocument::location(ExecState* exec) const
{
    Frame* frame = static_cast<Document*>(impl())->frame();
    if (!frame)
        return jsNull();

    Location* location = frame->domWindow()->location();
    if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec->globalData(), location))
        return wrapper;

    JSDOMWindow* window = static_cast<JSDOMWindow*>(exec->lexicalGlobalObject());
    JSLocation* jsLocation = new (exec) JSLocation(getDOMStructure<JSLocation>(exec, window), location);
    cacheDOMObjectWrapper(exec->globalData(), location, jsLocation);
    return jsLocation;
}

void JSDocument::setLocation(ExecState* exec, JSValue value)
{
    Frame* frame = static_cast<Document*>(impl())->frame();
    if (!frame)
        return;

    String str = value.toString(exec);

    // IE and Mozilla both resolve the URL relative to the source frame,
    // not the target frame.
    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (activeFrame)
        str = activeFrame->document()->completeURL(str).string();

    bool userGesture = activeFrame->script()->processingUserGesture();
    frame->loader()->scheduleLocationChange(str, activeFrame->loader()->outgoingReferrer(), !activeFrame->script()->anyPageIsProcessingUserGesture(), false, userGesture);
}

JSValue toJS(ExecState* exec, Document* document)
{
    if (!document)
        return jsNull();

    DOMObject* wrapper = getCachedDOMObjectWrapper(exec->globalData(), document);
    if (wrapper)
        return wrapper;

    if (document->isHTMLDocument())
        wrapper = CREATE_DOM_OBJECT_WRAPPER(exec, HTMLDocument, document);
#if ENABLE(SVG)
    else if (document->isSVGDocument())
        wrapper = CREATE_DOM_OBJECT_WRAPPER(exec, SVGDocument, document);
#endif
    else
        wrapper = CREATE_DOM_OBJECT_WRAPPER(exec, Document, document);

    // Make sure the document is kept around by the window object, and works right with the
    // back/forward cache.
    if (!document->frame()) {
        size_t nodeCount = 0;
        for (Node* n = document; n; n = n->traverseNextNode())
            nodeCount++;
        
        exec->heap()->reportExtraMemoryCost(nodeCount * sizeof(Node));
    }

    return wrapper;
}

} // namespace WebCore
