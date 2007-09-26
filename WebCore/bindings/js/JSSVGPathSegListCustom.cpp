/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#if ENABLE(SVG)
#include "JSSVGPathSegList.h"

#include "Document.h"
#include "Frame.h"
#include "JSSVGPathSeg.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGPathSegList.h"

#include <wtf/Assertions.h>

using namespace KJS;

namespace WebCore {

static void updatePathSegContextMap(ExecState* exec, SVGPathSegList* list, SVGPathSeg* obj)
{
    ASSERT(exec && exec->dynamicInterpreter());
    Frame* activeFrame = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter())->frame();
    if (!activeFrame)
        return;

    const SVGElement* context = list->context();
    ASSERT(context);

    // Update the SVGPathSeg* hashmap, so that the JSSVGPathSeg* wrappers, can access the context element
    SVGDocumentExtensions* extensions = (activeFrame->document() ? activeFrame->document()->accessSVGExtensions() : 0);
    if (extensions) {
        if (extensions->hasGenericContext<SVGPathSeg>(obj))
            ASSERT(extensions->genericContext<SVGPathSeg>(obj) == context);
        else
            extensions->setGenericContext<SVGPathSeg>(obj, context);
    }

    context->notifyAttributeChange();
}

static void removeFromPathSegContextMap(SVGPathSegList* list, SVGPathSeg* obj)
{
    const SVGElement* context = list->context();
    ASSERT(context);

    SVGDocumentExtensions::forgetGenericContext(obj);
    context->notifyAttributeChange();
}

JSValue* JSSVGPathSegList::clear(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());

    unsigned int nr = imp->numberOfItems();
    for (unsigned int i = 0; i < nr; i++)
        removeFromPathSegContextMap(imp, imp->getItem(i, ec).get());

    imp->clear(ec);
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue* JSSVGPathSegList::initialize(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(args[0]);

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());

    SVGPathSeg* obj = WTF::getPtr(imp->initialize(newItem, ec));
    updatePathSegContextMap(exec, imp, obj);

    KJS::JSValue* result = toJS(exec, obj);
    setDOMException(exec, ec);
    return result;
}

JSValue* JSSVGPathSegList::getItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;

    bool indexOk;
    unsigned index = args[0]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());

    SVGPathSeg* obj = WTF::getPtr(imp->getItem(index, ec));
    updatePathSegContextMap(exec, imp, obj);

    KJS::JSValue* result = toJS(exec, obj);
    setDOMException(exec, ec);
    return result;
}

JSValue* JSSVGPathSegList::insertItemBefore(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(args[0]);

    bool indexOk;
    unsigned index = args[1]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());
    updatePathSegContextMap(exec, imp, newItem);

    KJS::JSValue* result = toJS(exec, WTF::getPtr(imp->insertItemBefore(newItem, index, ec)));
    setDOMException(exec, ec);
    return result;
}

JSValue* JSSVGPathSegList::replaceItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(args[0]);
    
    bool indexOk;
    unsigned index = args[1]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());
    updatePathSegContextMap(exec, imp, newItem);

    KJS::JSValue* result = toJS(exec, WTF::getPtr(imp->replaceItem(newItem, index, ec)));
    setDOMException(exec, ec);
    return result;
}

JSValue* JSSVGPathSegList::removeItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    
    bool indexOk;
    unsigned index = args[0]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());

    RefPtr<SVGPathSeg> obj(imp->removeItem(index, ec));
    removeFromPathSegContextMap(imp, obj.get());

    KJS::JSValue* result = toJS(exec, obj.get());
    setDOMException(exec, ec);
    return result;
}

JSValue* JSSVGPathSegList::appendItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(args[0]);

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());
    updatePathSegContextMap(exec, imp, newItem);

    KJS::JSValue* result = toJS(exec, WTF::getPtr(imp->appendItem(newItem, ec)));
    setDOMException(exec, ec);
    return result;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
