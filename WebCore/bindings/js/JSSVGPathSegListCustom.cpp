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
#include "JSSVGPathSeg.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGPathSegList.h"

#include <wtf/Assertions.h>

using namespace JSC;

namespace WebCore {

JSValue JSSVGPathSegList::clear(ExecState* exec, const ArgList&)
{
    ExceptionCode ec = 0;

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());
    imp->clear(ec);

    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());
    return jsUndefined();
}

JSValue JSSVGPathSegList::initialize(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(args.at(0));

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());

    SVGPathSeg* obj = WTF::getPtr(imp->initialize(newItem, ec));

    JSC::JSValue result = toJS(exec, obj, m_context.get());
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());    
    return result;
}

JSValue JSSVGPathSegList::getItem(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;

    bool indexOk;
    unsigned index = args.at(0).toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());
    SVGPathSeg* obj = WTF::getPtr(imp->getItem(index, ec));

    JSC::JSValue result = toJS(exec, obj, m_context.get());
    setDOMException(exec, ec);
    return result;
}

JSValue JSSVGPathSegList::insertItemBefore(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(args.at(0));

    bool indexOk;
    unsigned index = args.at(1).toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());

    JSC::JSValue result = toJS(exec, WTF::getPtr(imp->insertItemBefore(newItem, index, ec)), m_context.get());
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());    
    return result;
}

JSValue JSSVGPathSegList::replaceItem(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(args.at(0));
    
    bool indexOk;
    unsigned index = args.at(1).toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());

    JSC::JSValue result = toJS(exec, WTF::getPtr(imp->replaceItem(newItem, index, ec)), m_context.get());
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());    
    return result;
}

JSValue JSSVGPathSegList::removeItem(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    
    bool indexOk;
    unsigned index = args.at(0).toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());

    RefPtr<SVGPathSeg> obj(imp->removeItem(index, ec));

    JSC::JSValue result = toJS(exec, obj.get(), m_context.get());
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());    
    return result;
}

JSValue JSSVGPathSegList::appendItem(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    SVGPathSeg* newItem = toSVGPathSeg(args.at(0));

    SVGPathSegList* imp = static_cast<SVGPathSegList*>(impl());

    JSC::JSValue result = toJS(exec, WTF::getPtr(imp->appendItem(newItem, ec)), m_context.get());
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());    
    return result;
}

}

#endif // ENABLE(SVG)
