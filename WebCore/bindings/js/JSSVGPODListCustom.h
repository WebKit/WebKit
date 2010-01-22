/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef JSSVGPODListCustom_h
#define JSSVGPODListCustom_h

#include "JSSVGContextCache.h"
#include "JSSVGPODTypeWrapper.h"
#include "SVGList.h"

namespace WebCore {

namespace JSSVGPODListCustom {

// Helper structure only containing public typedefs, as C++ does not allow templatized typedefs
template<typename PODType>
struct JSSVGPODListTraits { 
    typedef SVGPODListItem<PODType> PODListItem;
    typedef SVGList<RefPtr<PODListItem> > PODList;
    typedef PODType (*ConversionCallback)(JSC::JSValue);
};

template<typename JSPODListType, typename PODType>
static JSC::JSValue finishGetter(JSC::ExecState* exec, ExceptionCode& ec, JSPODListType* wrapper,
                                 PassRefPtr<typename JSSVGPODListTraits<PODType>::PODListItem> item)
{
    if (ec) {
        setDOMException(exec, ec);
        return JSC::jsUndefined();
    }
    
    typename JSSVGPODListTraits<PODType>::PODList* listImp = wrapper->impl();

    SVGElement* context = JSSVGContextCache::svgContextForDOMObject(wrapper);
    return toJS(exec, wrapper->globalObject(),
                JSSVGPODTypeWrapperCreatorForList<PODType>::create(item.get(), listImp->associatedAttributeName()).get(), context);
}

template<typename JSPODListType, typename PODType>
static JSC::JSValue finishSetter(JSC::ExecState* exec, ExceptionCode& ec, JSPODListType* wrapper,                                
                                 PassRefPtr<typename JSSVGPODListTraits<PODType>::PODListItem> item)
{
    if (ec) {
        setDOMException(exec, ec);
        return JSC::jsUndefined();
    }

    typename JSSVGPODListTraits<PODType>::PODList* listImp = wrapper->impl();

    const QualifiedName& attributeName = listImp->associatedAttributeName();
    JSSVGContextCache::propagateSVGDOMChange(wrapper, attributeName);

    SVGElement* context = JSSVGContextCache::svgContextForDOMObject(wrapper);
    return toJS(exec, wrapper->globalObject(),
                JSSVGPODTypeWrapperCreatorForList<PODType>::create(item.get(), attributeName).get(), context);
}

template<typename JSPODListType, typename PODType>
static JSC::JSValue finishSetterReadOnlyResult(JSC::ExecState* exec, ExceptionCode& ec, JSPODListType* wrapper,                                
                                               PassRefPtr<typename JSSVGPODListTraits<PODType>::PODListItem> item)
{
    if (ec) {
        setDOMException(exec, ec);
        return JSC::jsUndefined();
    }

    typename JSSVGPODListTraits<PODType>::PODList* listImp = wrapper->impl();
    JSSVGContextCache::propagateSVGDOMChange(wrapper, listImp->associatedAttributeName());
    return toJS(exec, wrapper->globalObject(),
                JSSVGStaticPODTypeWrapper<PODType>::create(*item).get(), 0  /* no context on purpose */);
}

template<typename JSPODListType, typename PODType>
static JSC::JSValue clear(JSPODListType* wrapper, JSC::ExecState* exec, const JSC::ArgList&,
                          typename JSSVGPODListTraits<PODType>::ConversionCallback)
{
    ExceptionCode ec = 0;
    typename JSSVGPODListTraits<PODType>::PODList* listImp = wrapper->impl();
    listImp->clear(ec);

    if (ec)
        setDOMException(exec, ec);
    else
        JSSVGContextCache::propagateSVGDOMChange(wrapper, listImp->associatedAttributeName());

    return JSC::jsUndefined();
}

template<typename JSPODListType, typename PODType>
static JSC::JSValue initialize(JSPODListType* wrapper, JSC::ExecState* exec, const JSC::ArgList& args,
                               typename JSSVGPODListTraits<PODType>::ConversionCallback conversion)
{
    ExceptionCode ec = 0;
    typename JSSVGPODListTraits<PODType>::PODList* listImp = wrapper->impl();
    return finishSetter<JSPODListType, PODType>(exec, ec, wrapper,
                                                listImp->initialize(JSSVGPODListTraits<PODType>::PODListItem::copy(conversion(args.at(0))), ec));
}

template<typename JSPODListType, typename PODType>
static JSC::JSValue getItem(JSPODListType* wrapper, JSC::ExecState* exec, const JSC::ArgList& args,
                            typename JSSVGPODListTraits<PODType>::ConversionCallback)
{
    bool indexOk = false;
    unsigned index = args.at(0).toUInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return JSC::jsUndefined();
    }

    ExceptionCode ec = 0;
    typename JSSVGPODListTraits<PODType>::PODList* listImp = wrapper->impl();
    return finishGetter<JSPODListType, PODType>(exec, ec, wrapper,
                                                listImp->getItem(index, ec));
}

template<typename JSPODListType, typename PODType>
static JSC::JSValue insertItemBefore(JSPODListType* wrapper, JSC::ExecState* exec, const JSC::ArgList& args,
                                     typename JSSVGPODListTraits<PODType>::ConversionCallback conversion)
{
    bool indexOk = false;
    unsigned index = args.at(1).toUInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return JSC::jsUndefined();
    }

    ExceptionCode ec = 0;
    typename JSSVGPODListTraits<PODType>::PODList* listImp = wrapper->impl();
    return finishSetter<JSPODListType, PODType>(exec, ec, wrapper,
                                                listImp->insertItemBefore(JSSVGPODListTraits<PODType>::PODListItem::copy(conversion(args.at(0))), index, ec));
}

template<typename JSPODListType, typename PODType>
static JSC::JSValue replaceItem(JSPODListType* wrapper, JSC::ExecState* exec, const JSC::ArgList& args,
                                typename JSSVGPODListTraits<PODType>::ConversionCallback conversion)
{
    bool indexOk = false;
    unsigned index = args.at(1).toUInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return JSC::jsUndefined();
    }

    ExceptionCode ec = 0;
    typename JSSVGPODListTraits<PODType>::PODList* listImp = wrapper->impl();
    return finishSetter<JSPODListType, PODType>(exec, ec, wrapper,
                                                listImp->replaceItem(JSSVGPODListTraits<PODType>::PODListItem::copy(conversion(args.at(0))), index, ec));
}

template<typename JSPODListType, typename PODType>
static JSC::JSValue removeItem(JSPODListType* wrapper, JSC::ExecState* exec, const JSC::ArgList& args,
                               typename JSSVGPODListTraits<PODType>::ConversionCallback)
{
    bool indexOk = false;
    unsigned index = args.at(0).toUInt32(exec, indexOk);
    if (!indexOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return JSC::jsUndefined();
    }

    ExceptionCode ec = 0;
    typename JSSVGPODListTraits<PODType>::PODList* listImp = wrapper->impl();
    return finishSetterReadOnlyResult<JSPODListType, PODType>(exec, ec, wrapper,
                                                              listImp->removeItem(index, ec));
}

template<typename JSPODListType, typename PODType>
static JSC::JSValue appendItem(JSPODListType* wrapper, JSC::ExecState* exec, const JSC::ArgList& args,
                               typename JSSVGPODListTraits<PODType>::ConversionCallback conversion)
{
    ExceptionCode ec = 0;
    typename JSSVGPODListTraits<PODType>::PODList* listImp = wrapper->impl();
    return finishSetter<JSPODListType, PODType>(exec, ec, wrapper,
                                                listImp->appendItem(JSSVGPODListTraits<PODType>::PODListItem::copy(conversion(args.at(0))), ec));
}

}

}

#endif
