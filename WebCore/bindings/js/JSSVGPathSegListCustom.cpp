/*
 * Copyright (C) 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "JSSVGContextCache.h"
#include "JSSVGPathSeg.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGPathSegList.h"

#include <wtf/Assertions.h>

using namespace JSC;

namespace WebCore {

JSValue JSSVGPathSegList::clear(ExecState* exec)
{
    ExceptionCode ec = 0;

    SVGPathSegList* list = impl();
    list->clear(ec);

    setDOMException(exec, ec);

    JSSVGContextCache::propagateSVGDOMChange(this, list->associatedAttributeName());
    return jsUndefined();
}

JSValue JSSVGPathSegList::initialize(ExecState* exec)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(exec->argument(0));

    SVGPathSegList* list = impl();

    SVGPathSeg* obj = WTF::getPtr(list->initialize(newItem, ec));
    SVGElement* context = JSSVGContextCache::svgContextForDOMObject(this);

    JSValue result = toJS(exec, globalObject(), obj, context);
    setDOMException(exec, ec);

    JSSVGContextCache::propagateSVGDOMChange(this, list->associatedAttributeName());
    return result;
}

JSValue JSSVGPathSegList::getItem(ExecState* exec)
{
    ExceptionCode ec = 0;

    bool indexOk;
    unsigned index = finiteInt32Value(exec->argument(0), exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* list = impl();
    SVGPathSeg* obj = WTF::getPtr(list->getItem(index, ec));
    SVGElement* context = JSSVGContextCache::svgContextForDOMObject(this);

    JSValue result = toJS(exec, globalObject(), obj, context);
    setDOMException(exec, ec);
    return result;
}

JSValue JSSVGPathSegList::insertItemBefore(ExecState* exec)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(exec->argument(0));

    bool indexOk;
    unsigned index = finiteInt32Value(exec->argument(1), exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* list = impl();
    SVGElement* context = JSSVGContextCache::svgContextForDOMObject(this);

    JSValue result = toJS(exec, globalObject(), WTF::getPtr(list->insertItemBefore(newItem, index, ec)), context);
    setDOMException(exec, ec);

    JSSVGContextCache::propagateSVGDOMChange(this, list->associatedAttributeName());
    return result;
}

JSValue JSSVGPathSegList::replaceItem(ExecState* exec)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(exec->argument(0));
    
    bool indexOk;
    unsigned index = finiteInt32Value(exec->argument(1), exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* list = impl();
    SVGElement* context = JSSVGContextCache::svgContextForDOMObject(this);

    JSValue result = toJS(exec, globalObject(), WTF::getPtr(list->replaceItem(newItem, index, ec)), context);
    setDOMException(exec, ec);

    JSSVGContextCache::propagateSVGDOMChange(this, list->associatedAttributeName());
    return result;
}

JSValue JSSVGPathSegList::removeItem(ExecState* exec)
{
    ExceptionCode ec = 0;
    
    bool indexOk;
    unsigned index = finiteInt32Value(exec->argument(0), exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* list = impl();

    RefPtr<SVGPathSeg> obj(list->removeItem(index, ec));
    SVGElement* context = JSSVGContextCache::svgContextForDOMObject(this);

    JSValue result = toJS(exec, globalObject(), obj.get(), context);
    setDOMException(exec, ec);

    JSSVGContextCache::propagateSVGDOMChange(this, list->associatedAttributeName());
    return result;
}

JSValue JSSVGPathSegList::appendItem(ExecState* exec)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(exec->argument(0));

    SVGPathSegList* list = impl();
    SVGElement* context = JSSVGContextCache::svgContextForDOMObject(this);

    JSValue result = toJS(exec, globalObject(), WTF::getPtr(list->appendItem(newItem, ec)), context);
    setDOMException(exec, ec);

    JSSVGContextCache::propagateSVGDOMChange(this, list->associatedAttributeName());
    return result;
}

}

#endif // ENABLE(SVG)
