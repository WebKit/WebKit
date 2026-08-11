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
#include "SVGPathUtilities.h"
#include "SVGProperty.h"

namespace WebCore {

// Owner of the SVGPathByteStream that backs the "d" attribute, used for rendering and SMIL animation.
class SVGPath final : public SVGProperty {
    friend class SVGAnimatedPathAnimator;

public:
    static Ref<SVGPath> create(SVGPropertyOwner* owner, SVGPropertyAccess access)
    {
        return adoptRef(*new SVGPath(owner, access));
    }

    static Ref<SVGPath> create(const SVGPath& other, SVGPropertyAccess access)
    {
        return adoptRef(*new SVGPath(other, access));
    }

    SVGPath& operator=(const SVGPath& other)
    {
        pathByteStreamWillChange();
        m_pathByteStream = other.pathByteStream();
        return *this;
    }

    void updateByteStreamData(DataRef<SVGPathByteStream::Data>&& byteStreamData)
    {
        pathByteStreamWillChange();
        m_pathByteStream.setData(WTF::move(byteStreamData));
    }

    void clearByteStreamData()
    {
        pathByteStreamWillChange();
        m_pathByteStream.clear();
    }

    const SVGPathByteStream& existingPathByteStream() const LIFETIME_BOUND { return m_pathByteStream; }

    const SVGPathByteStream& pathByteStream() const LIFETIME_BOUND { return m_pathByteStream; }
    SVGPathByteStream& pathByteStream() LIFETIME_BOUND { return m_pathByteStream; }

    bool parse(StringView value)
    {
        pathByteStreamWillChange();
        return buildSVGPathByteStreamFromString(value, m_pathByteStream, UnalteredParsing);
    }

    Path path() const
    {
        if (!m_path) {
            m_path = buildPathFromByteStream(pathByteStream());
            m_path->setNotTransient();
        }
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
    SVGPath(SVGPropertyOwner* owner, SVGPropertyAccess access)
        : SVGProperty(owner, access)
    {
    }

    SVGPath(const SVGPath& other, SVGPropertyAccess access)
        : SVGProperty(other.m_owner, access)
        , m_pathByteStream(other.pathByteStream())
    {
    }

    // Called by SVGAnimatedPathAnimator before writing a new animated byte stream.
    void pathByteStreamWillChange() { m_path = std::nullopt; }

    SVGPathByteStream m_pathByteStream;
    mutable std::optional<Path> m_path;
};

}
