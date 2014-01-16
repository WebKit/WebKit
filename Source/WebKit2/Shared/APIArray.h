/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
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

#ifndef APIArray_h
#define APIArray_h

#include "APIObject.h"
#include <wtf/Forward.h>
#include <wtf/IteratorAdaptors.h>
#include <wtf/IteratorRange.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace API {

class Array final : public ObjectImpl<Object::Type::Array> {
private:
    template <class T>
    struct IsTypePredicate {
        bool operator()(const RefPtr<Object>& object) const { return object->type() == T::APIType; }
    };

    template <class T>
    struct GetObjectTransform {
        const T* operator()(const RefPtr<Object>& object) const { return static_cast<const T*>(object.get()); }
    };

    template <typename T>
    using ElementsOfTypeRange = WTF::IteratorRange<WTF::TransformIterator<GetObjectTransform<T>, WTF::FilterIterator<IsTypePredicate<T>, Vector<RefPtr<Object>>::const_iterator>>>;

public:
    static PassRefPtr<Array> create();
    static PassRefPtr<Array> create(Vector<RefPtr<Object>> elements);
    static PassRefPtr<Array> createStringArray(const Vector<WTF::String>&);
    Vector<WTF::String> toStringVector();

    virtual ~Array();

    template<typename T>
    T* at(size_t i) const
    {
        if (m_elements[i]->type() != T::APIType)
            return nullptr;

        return static_cast<T*>(m_elements[i].get());
    }

    Object* at(size_t i) const { return m_elements[i].get(); }
    size_t size() const { return m_elements.size(); }

    const Vector<RefPtr<Object>>& elements() const { return m_elements; }
    Vector<RefPtr<Object>>& elements() { return m_elements; }

    template<typename T>
    ElementsOfTypeRange<T> elementsOfType() const
    {
        return WTF::makeIteratorRange(
            WTF::makeTransformIterator(GetObjectTransform<T>(), WTF::makeFilterIterator(IsTypePredicate<T>(), m_elements.begin(), m_elements.end())),
            WTF::makeTransformIterator(GetObjectTransform<T>(), WTF::makeFilterIterator(IsTypePredicate<T>(), m_elements.end(), m_elements.end()))
        );
    }

private:
    explicit Array(Vector<RefPtr<Object>> elements);

    Vector<RefPtr<Object>> m_elements;
};

} // namespace API

#endif // APIArray_h
