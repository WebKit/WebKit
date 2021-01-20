/*
 * Copyright (C) 2018-2019 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SVGList.h"

namespace WebCore {

template<typename PropertyType>
class SVGPropertyList : public SVGList<Ref<PropertyType>>, public SVGPropertyOwner {
public:
    using BaseList = SVGList<Ref<PropertyType>>;
    using BaseList::isEmpty;
    using BaseList::size;
    using BaseList::append;

    // Visual Studio doesn't seem to see these private constructors from subclasses.
    // FIXME: See what it takes to remove this hack.
#if !COMPILER(MSVC)
protected:
#endif
    using SVGPropertyOwner::SVGPropertyOwner;
    using BaseList::m_items;
    using BaseList::m_access;
    using BaseList::m_owner;

    SVGPropertyList(SVGPropertyOwner* owner = nullptr, SVGPropertyAccess access = SVGPropertyAccess::ReadWrite)
        : BaseList(owner, access)
    {
    }

    ~SVGPropertyList()
    {
        // Detach the items from the list before it is deleted.
        detachItems();
    }

    void detachItems() override
    {
        for (auto& item : m_items)
            item->detach();
    }

    SVGPropertyOwner* owner() const override { return m_owner; }

    void commitPropertyChange(SVGProperty*) override
    {
        if (owner())
            owner()->commitPropertyChange(this);
    }

    Ref<PropertyType> at(unsigned index) const override
    {
        ASSERT(index < size());
        return m_items.at(index).copyRef();
    }

    Ref<PropertyType> insert(unsigned index, Ref<PropertyType>&& newItem) override
    {
        ASSERT(index <= size());

        // Spec: if newItem is not a detached object, then set newItem to be
        // a clone object of newItem.
        if (newItem->isAttached())
            newItem = newItem->clone();

        // Spec: Attach newItem to the list object.
        newItem->attach(this, m_access);
        m_items.insert(index, WTFMove(newItem));
        return at(index);
    }

    Ref<PropertyType> replace(unsigned index, Ref<PropertyType>&& newItem) override
    {
        ASSERT(index < size());
        Ref<PropertyType>& item = m_items[index];

        // Spec: Detach item.
        item->detach();

        // Spec: if newItem is not a detached object, then set newItem to be
        // a clone object of newItem.
        if (newItem->isAttached())
            item = newItem->clone();
        else
            item = WTFMove(newItem);

        // Spec: Attach newItem to the list object.
        item->attach(this, m_access);
        return at(index);
    }

    Ref<PropertyType> remove(unsigned index) override
    {
        ASSERT(index < size());
        Ref<PropertyType> item = at(index);

        // Spec: Detach item.
        item->detach();
        m_items.remove(index);
        return item;
    }

    Ref<PropertyType> append(Ref<PropertyType>&& newItem) override
    {
        // Spec: if newItem is not a detached object, then set newItem to be
        // a clone object of newItem.
        if (newItem->isAttached())
            newItem = newItem->clone();

        // Spec: Attach newItem to the list object.
        newItem->attach(this, m_access);
        m_items.append(WTFMove(newItem));
        return at(size() - 1);
    }
};

}
