/*
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "JSSVGTransformList.h"

#include "Document.h"
#include "Frame.h"
#include "JSSVGTransform.h"
#include "SVGDocumentExtensions.h"
#include "SVGTransformList.h"
#include "SVGStyledElement.h"

#include <wtf/Assertions.h>

using namespace KJS;

namespace WebCore {

JSValue* JSSVGTransformList::clear(ExecState* exec, const List&)
{
    ExceptionCode ec = 0;

    SVGTransformList* imp = static_cast<SVGTransformList*>(impl());
    imp->clear(ec);
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());

    return jsUndefined();
}

JSValue* JSSVGTransformList::initialize(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    SVGTransform newItem = toSVGTransform(args[0]);

    SVGTransformList* imp = static_cast<SVGTransformList*>(impl());
    SVGList<RefPtr<SVGPODListItem<SVGTransform> > >* listImp = imp;

    SVGPODListItem<SVGTransform>* listItem = listImp->initialize(new SVGPODListItem<SVGTransform>(newItem), ec).get(); 
    JSSVGPODTypeWrapperCreatorForList<SVGTransform>* obj = new JSSVGPODTypeWrapperCreatorForList<SVGTransform>(listItem, imp->associatedAttributeName());

    KJS::JSValue* result = toJS(exec, obj, m_context.get());
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());

    return result;
}

JSValue* JSSVGTransformList::getItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;

    bool indexOk;
    unsigned index = args[0]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGTransformList* imp = static_cast<SVGTransformList*>(impl());
    SVGList<RefPtr<SVGPODListItem<SVGTransform> > >* listImp = imp;

    SVGPODListItem<SVGTransform>* listItem = listImp->getItem(index, ec).get();
    JSSVGPODTypeWrapperCreatorForList<SVGTransform>* obj = new JSSVGPODTypeWrapperCreatorForList<SVGTransform>(listItem, imp->associatedAttributeName());

    KJS::JSValue* result = toJS(exec, obj, m_context.get());
    setDOMException(exec, ec);
    return result;
}

JSValue* JSSVGTransformList::insertItemBefore(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    SVGTransform newItem = toSVGTransform(args[0]);

    bool indexOk;
    unsigned index = args[1]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGTransformList* imp = static_cast<SVGTransformList*>(impl());
    SVGList<RefPtr<SVGPODListItem<SVGTransform> > >* listImp = imp;

    SVGPODListItem<SVGTransform>* listItem = listImp->insertItemBefore(new SVGPODListItem<SVGTransform>(newItem), index, ec).get();
    JSSVGPODTypeWrapperCreatorForList<SVGTransform>* obj = new JSSVGPODTypeWrapperCreatorForList<SVGTransform>(listItem, imp->associatedAttributeName());

    KJS::JSValue* result = toJS(exec, obj, m_context.get());
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());

    return result;
}

JSValue* JSSVGTransformList::replaceItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    SVGTransform newItem = toSVGTransform(args[0]);

    bool indexOk;
    unsigned index = args[1]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGTransformList* imp = static_cast<SVGTransformList*>(impl());
    SVGList<RefPtr<SVGPODListItem<SVGTransform> > >* listImp = imp;

    SVGPODListItem<SVGTransform>* listItem = listImp->replaceItem(new SVGPODListItem<SVGTransform>(newItem), index, ec).get(); 
    JSSVGPODTypeWrapperCreatorForList<SVGTransform>* obj = new JSSVGPODTypeWrapperCreatorForList<SVGTransform>(listItem, imp->associatedAttributeName());

    KJS::JSValue* result = toJS(exec, obj, m_context.get());
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());

    return result;
}

JSValue* JSSVGTransformList::removeItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    
    bool indexOk;
    unsigned index = args[0]->toInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    SVGTransformList* imp = static_cast<SVGTransformList*>(impl());
    SVGList<RefPtr<SVGPODListItem<SVGTransform> > >* listImp = imp;

    RefPtr<SVGPODListItem<SVGTransform> > listItem(listImp->removeItem(index, ec));
    JSSVGPODTypeWrapper<SVGTransform>* obj = new JSSVGPODTypeWrapperCreatorReadOnly<SVGTransform>(*listItem.get());

    KJS::JSValue* result = toJS(exec, obj, m_context.get());
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());

    return result;
}

JSValue* JSSVGTransformList::appendItem(ExecState* exec, const List& args)
{
    ExceptionCode ec = 0;
    SVGTransform newItem = toSVGTransform(args[0]);

    SVGTransformList* imp = static_cast<SVGTransformList*>(impl());
    SVGList<RefPtr<SVGPODListItem<SVGTransform> > >* listImp = imp;

    SVGPODListItem<SVGTransform>* listItem = listImp->appendItem(new SVGPODListItem<SVGTransform>(newItem), ec).get(); 
    JSSVGPODTypeWrapperCreatorForList<SVGTransform>* obj = new JSSVGPODTypeWrapperCreatorForList<SVGTransform>(listItem, imp->associatedAttributeName());

    KJS::JSValue* result = toJS(exec, obj, m_context.get());
    setDOMException(exec, ec);

    m_context->svgAttributeChanged(imp->associatedAttributeName());

    return result;
}

}

#endif // ENABLE(SVG)
