/*
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "JSSVGTransformList.h"

#if ENABLE(SVG)

#include "JSSVGTransform.h"
#include "SVGTransformList.h"

using namespace KJS;

namespace WebCore {

typedef SVGList<RefPtr<SVGPODListItem<SVGTransform> > > SVGTransformListBase;

static JSValue* finishGetter(ExecState* exec, ExceptionCode& ec, SVGElement* context, SVGTransformList* list, PassRefPtr<SVGPODListItem<SVGTransform> > item)
{
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, JSSVGPODTypeWrapperCreatorForList<SVGTransform>::create(item.get(), list->associatedAttributeName()).get(), context);
}

static JSValue* finishSetter(ExecState* exec, ExceptionCode& ec, SVGElement* context, SVGTransformList* list, PassRefPtr<SVGPODListItem<SVGTransform> > item)
{
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    const QualifiedName& attributeName = list->associatedAttributeName();
    context->svgAttributeChanged(attributeName);
    return toJS(exec, JSSVGPODTypeWrapperCreatorForList<SVGTransform>::create(item.get(), attributeName).get(), context);
}

static JSValue* finishSetterReadOnlyResult(ExecState* exec, ExceptionCode& ec, SVGElement* context, SVGTransformList* list, PassRefPtr<SVGPODListItem<SVGTransform> > item)
{
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    context->svgAttributeChanged(list->associatedAttributeName());
    return toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<SVGTransform>::create(*item).get(), context);
}

JSValue* JSSVGTransformList::clear(ExecState* exec, const ArgList&)
{
    ExceptionCode ec = 0;
    impl()->clear(ec);
    setDOMException(exec, ec);
    m_context->svgAttributeChanged(impl()->associatedAttributeName());
    return jsUndefined();
}

JSValue* JSSVGTransformList::initialize(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    SVGTransformListBase* listImp = impl();
    return finishSetter(exec, ec, context(), impl(),
        listImp->initialize(SVGPODListItem<SVGTransform>::copy(toSVGTransform(args[0])), ec));
}

JSValue* JSSVGTransformList::getItem(ExecState* exec, const ArgList& args)
{
    bool indexOk;
    unsigned index = args[0]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    ExceptionCode ec = 0;
    SVGTransformListBase* listImp = impl();
    return finishGetter(exec, ec, context(), impl(),
        listImp->getItem(index, ec));
}

JSValue* JSSVGTransformList::insertItemBefore(ExecState* exec, const ArgList& args)
{
    bool indexOk;
    unsigned index = args[1]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    ExceptionCode ec = 0;
    SVGTransformListBase* listImp = impl();
    return finishSetter(exec, ec, context(), impl(),
        listImp->insertItemBefore(SVGPODListItem<SVGTransform>::copy(toSVGTransform(args[0])), index, ec));
}

JSValue* JSSVGTransformList::replaceItem(ExecState* exec, const ArgList& args)
{
    bool indexOk;
    unsigned index = args[1]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    ExceptionCode ec = 0;
    SVGTransformListBase* listImp = impl();
    return finishSetter(exec, ec, context(), impl(),
        listImp->replaceItem(SVGPODListItem<SVGTransform>::copy(toSVGTransform(args[0])), index, ec));
}

JSValue* JSSVGTransformList::removeItem(ExecState* exec, const ArgList& args)
{
    bool indexOk;
    unsigned index = args[0]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    ExceptionCode ec = 0;
    SVGTransformListBase* listImp = impl();
    return finishSetterReadOnlyResult(exec, ec, context(), impl(),
        listImp->removeItem(index, ec));
}

JSValue* JSSVGTransformList::appendItem(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    SVGTransformListBase* listImp = impl();
    return finishSetter(exec, ec, context(), impl(),
        listImp->appendItem(SVGPODListItem<SVGTransform>::copy(toSVGTransform(args[0])), ec));
}

}

#endif // ENABLE(SVG)
