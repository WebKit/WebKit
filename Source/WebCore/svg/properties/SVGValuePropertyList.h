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

#include "SVGPropertyList.h"

namespace WebCore {

template<typename PropertyType>
class SVGValuePropertyList : public SVGPropertyList<PropertyType> {
public:
    using Base = SVGPropertyList<PropertyType>;
    using Base::clearItems;
    using Base::items;
    using Base::size;

    SVGValuePropertyList& operator=(const SVGValuePropertyList& other)
    {
        clearItems();
        for (const auto& item : other.items())
            append(PropertyType::create(item->value()));
        return *this;
    }

    // This casting operator returns a Vector of the underlying ValueType,
    // for example Vector<float> from SVGNumberList.
    operator Vector<typename PropertyType::ValueType>() const
    {
        Vector<typename PropertyType::ValueType> values;
        for (const auto& item : items())
            values.append(item->value());
        return values;
    }

    void resize(size_t newSize)
    {
        // Add new items.
        while (size() < newSize)
            append(PropertyType::create());

        // Remove existing items.
        while (size() > newSize)
            remove(size() - 1);
    }

    // Visual Studio doesn't seem to see these private constructors from subclasses.
    // FIXME: See what it takes to remove this hack.
#if !COMPILER(MSVC)
protected:
#endif
    using Base::append;
    using Base::remove;

    // Base and default constructor. Do not use "using Base::Base" because of Windows and GTK ports.
    SVGValuePropertyList(SVGPropertyOwner* owner = nullptr, SVGPropertyAccess access = SVGPropertyAccess::ReadWrite)
        : Base(owner, access)
    {
    }

    // Used by SVGAnimatedPropertyList when creating it animVal from baseVal.
    SVGValuePropertyList(const SVGValuePropertyList& other, SVGPropertyAccess access = SVGPropertyAccess::ReadWrite)
        : Base(other.owner(), access)
    {
        // Clone all items.
        for (const auto& item : other.items())
            append(PropertyType::create(item->value()));
    }
};

}
