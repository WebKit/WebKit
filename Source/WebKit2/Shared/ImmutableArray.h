/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImmutableArray_h
#define ImmutableArray_h

#include "APIObject.h"
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebKit {

// ImmutableArray - An immutable array type suitable for vending to an API.

class ImmutableArray FINAL : public API::TypedObject<API::Object::Type::Array> {
public:
    static PassRefPtr<ImmutableArray> create();
    static PassRefPtr<ImmutableArray> create(Vector<RefPtr<API::Object>> elements);

    static PassRefPtr<ImmutableArray> createStringArray(const Vector<String>&);

    virtual ~ImmutableArray();

    template<typename T>
    T* at(size_t i) const
    {
        if (m_elements[i]->type() != T::APIType)
            return nullptr;

        return static_cast<T*>(m_elements[i].get());
    }

    API::Object* at(size_t i) const { return m_elements[i].get(); }
    size_t size() const { return m_elements.size(); }

    const Vector<RefPtr<API::Object>>& elements() const { return m_elements; }
    Vector<RefPtr<API::Object>>& elements() { return m_elements; }

protected:
    ImmutableArray(Vector<RefPtr<API::Object>> elements);

    Vector<RefPtr<API::Object>> m_elements;
};

} // namespace WebKit

#endif // ImmutableArray_h
