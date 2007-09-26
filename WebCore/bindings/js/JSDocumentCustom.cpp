/*
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
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

#include "Document.h"
#include "HTMLDocument.h"
#include "JSHTMLDocument.h"
#include "kjs_binding.h"
#include "kjs_window.h"

#if ENABLE(SVG)
#include "JSSVGDocument.h"
#include "SVGDocument.h"
#endif

namespace WebCore {

using namespace KJS;

void JSDocument::mark()
{
    DOMObject::mark();
    ScriptInterpreter::markDOMNodesForDocument(static_cast<Document*>(impl()));
}

JSValue* toJS(ExecState* exec, Document* doc)
{
    if (!doc)
        return jsNull();

    ScriptInterpreter* interp = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter());
    JSDocument* ret =  static_cast<JSDocument*>(interp->getDOMObject(doc));
    if (ret)
        return ret;

    if (doc->isHTMLDocument())
        ret = new JSHTMLDocument(exec, static_cast<HTMLDocument*>(doc));
#if ENABLE(SVG)
    else if (doc->isSVGDocument())
        ret = new JSSVGDocument(exec, static_cast<SVGDocument*>(doc));
#endif
    else
        ret = new JSDocument(exec, doc);

    // Make sure the document is kept around by the window object, and works right with the
    // back/forward cache.
    if (doc->frame())
        Window::retrieveWindow(doc->frame())->putDirect("document", ret, DontDelete|ReadOnly);
    else {
        size_t nodeCount = 0;
        for (Node* n = doc; n; n = n->traverseNextNode())
            nodeCount++;
        
        Collector::reportExtraMemoryCost(nodeCount * sizeof(Node));
    }

    interp->putDOMObject(doc, ret);

    return ret;
}

} // namespace WebCore
