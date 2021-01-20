/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "Path.h"
#include "SVGPathByteStream.h"
#include "SVGPathSeg.h"
#include "SVGPropertyList.h"

namespace WebCore {

class SVGPathSegList final : public SVGPropertyList<SVGPathSeg> {
    friend class SVGAnimatedPathSegListAnimator;
    friend class SVGPathSegListBuilder;
    friend class SVGPathSegListSource;

    using Base = SVGPropertyList<SVGPathSeg>;
    using Base::Base;

public:
    static Ref<SVGPathSegList> create(SVGPropertyOwner* owner, SVGPropertyAccess access)
    {
        return adoptRef(*new SVGPathSegList(owner, access));
    }

    static Ref<SVGPathSegList> create(const SVGPathSegList& other, SVGPropertyAccess access)
    {
        return adoptRef(*new SVGPathSegList(other, access));
    }

    static Ref<SVGPathSegList> create(Ref<SVGPathSeg>&& newItem)
    {
        return adoptRef(*new SVGPathSegList(WTFMove(newItem)));
    }

    SVGPathSegList& operator=(const SVGPathSegList& other)
    {
        pathByteStreamWillChange();
        m_pathByteStream = other.pathByteStream();
        return *this;
    }

    // Override SVGList::length() because numberOfItems() isn't virtual.
    unsigned length() const { return numberOfItems(); }

    unsigned numberOfItems() const
    {
        const_cast<SVGPathSegList*>(this)->ensureItems();
        return Base::numberOfItems();
    }

    ExceptionOr<void> clear()
    {
        itemsWillChange();
        return Base::clear();
    }

    ExceptionOr<Ref<SVGPathSeg>> getItem(unsigned index)
    {
        ensureItems();
        return Base::getItem(index);
    }

    ExceptionOr<Ref<SVGPathSeg>> initialize(Ref<SVGPathSeg>&& newItem)
    {
        itemsWillChange();
        return Base::initialize(WTFMove(newItem));
    }

    ExceptionOr<Ref<SVGPathSeg>> insertItemBefore(Ref<SVGPathSeg>&& newItem, unsigned index)
    {
        ensureItems();
        itemsWillChange();
        return Base::insertItemBefore(WTFMove(newItem), index);
    }

    ExceptionOr<Ref<SVGPathSeg>> replaceItem(Ref<SVGPathSeg>&& newItem, unsigned index)
    {
        ensureItems();
        itemsWillChange();
        return Base::replaceItem(WTFMove(newItem), index);
    }

    ExceptionOr<Ref<SVGPathSeg>> removeItem(unsigned index)
    {
        ensureItems();
        itemsWillChange();
        return Base::removeItem(index);
    }

    ExceptionOr<Ref<SVGPathSeg>> appendItem(Ref<SVGPathSeg>&& newItem)
    {
        ensureItems();
        appendPathSegToPathByteStream(newItem);
        clearPath();
        return Base::appendItem(WTFMove(newItem));
    }

    // Override SVGList::setItem() because replaceItem() isn't virtual.
    ExceptionOr<void> setItem(unsigned index, Ref<SVGPathSeg>&& newItem)
    {
        auto result = replaceItem(WTFMove(newItem), index);
        if (result.hasException())
            return result.releaseException();
        return { };
    }

    const SVGPathByteStream& pathByteStream() const { return const_cast<SVGPathSegList*>(this)->pathByteStream(); }
    SVGPathByteStream& pathByteStream()
    {
        ensurePathByteStream();
        return m_pathByteStream;
    }

    bool parse(const String& value)
    {
        pathByteStreamWillChange();
        return buildSVGPathByteStreamFromString(value, m_pathByteStream, UnalteredParsing);
    }

    Path path() const
    {
        if (!m_path)
            m_path = buildPathFromByteStream(pathByteStream());
        return *m_path;
    }

    size_t approximateMemoryCost() const
    {
        // This is an approximation for path memory cost since the path is parsed on demand.
        size_t pathMemoryCost = (m_pathByteStream.size() / 10) * sizeof(FloatPoint);
        // We need to account for the memory which is allocated by the m_path.
        return m_path ? pathMemoryCost + sizeof(*m_path) : pathMemoryCost;
    }

    String valueAsString() const override
    {
        String value;
        buildStringFromByteStream(pathByteStream(), value, UnalteredParsing);
        return value;
    }

private:
    SVGPathSegList(const SVGPathSegList& other, SVGPropertyAccess access)
        : Base(other.owner(), access)
        , m_pathByteStream(other.pathByteStream())
    {
    }

    // Used by appendPathSegToPathByteStream() to create a temporary SVGPathSegList with one item.
    SVGPathSegList(Ref<SVGPathSeg>&& newItem)
    {
        append(WTFMove(newItem));
    }

    // Called when changing an item in the list.
    void commitPropertyChange(SVGProperty* property) override
    {
        itemsWillChange();
        Base::commitPropertyChange(property);
    }

    void ensureItems()
    {
        if (!m_items.isEmpty() || m_pathByteStream.isEmpty())
            return;
        buildSVGPathSegListFromByteStream(m_pathByteStream, *this, UnalteredParsing);
    }

    void ensurePathByteStream()
    {
        if (!m_pathByteStream.isEmpty() || m_items.isEmpty())
            return;
        buildSVGPathByteStreamFromSVGPathSegList(*this, m_pathByteStream, UnalteredParsing);
    }

    // Optimize appending an SVGPathSeg to the list. Instead of creating the whole
    // byte stream, a temporary byte stream will be creating just for the new item
    // and this temporary byte stream will be appended to m_pathByteStream.
    void appendPathSegToPathByteStream(const Ref<SVGPathSeg>& item)
    {
        if (m_pathByteStream.isEmpty())
            return;

        Ref<SVGPathSegList> pathSegList = SVGPathSegList::create(item.copyRef());
        SVGPathByteStream pathSegStream;

        if (!buildSVGPathByteStreamFromSVGPathSegList(pathSegList, pathSegStream, UnalteredParsing, false))
            return;

        m_pathByteStream.append(pathSegStream);
    }

    void clearPathByteStream() { m_pathByteStream.clear(); }
    void clearPath() { m_path = WTF::nullopt; }

    void pathByteStreamWillChange()
    {
        clearItems();
        clearPath();
    }

    void itemsWillChange()
    {
        clearPathByteStream();
        clearPath();
    }

    SVGPathByteStream m_pathByteStream;
    mutable Optional<Path> m_path;
};

}
