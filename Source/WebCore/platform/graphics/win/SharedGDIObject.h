/*
 * Copyright (C) 2013 Apple, Inc. All rights reserved.
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

#ifndef SharedGDIObject_h
#define SharedGDIObject_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

template <typename T> class SharedGDIObject : public RefCounted<SharedGDIObject<T>> {
public:
    static PassRefPtr<SharedGDIObject> create(GDIObject<T> object)
    {
        return adoptRef(new SharedGDIObject<T>(WTF::move(object)));
    }

    T get() const
    {
        return m_object.get();
    }

    unsigned hash() const
    {
        return WTF::PtrHash<T>::hash(m_object.get());
    }

private:
    explicit SharedGDIObject(GDIObject<T> object)
        : m_object(WTF::move(object))
    {
    }

    GDIObject<T> m_object;
};

} // namespace WebCore

#endif // SharedGDIObject_h
