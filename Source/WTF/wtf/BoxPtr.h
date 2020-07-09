/*
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
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

#include <memory>
#include <wtf/Box.h>

namespace WTF {

template<typename T> struct BoxPtrDeleter {
    void operator()(T*) const = delete;
};

#define WTF_DEFINE_BOXPTR_DELETER(typeName, deleterFunction) \
    template<> struct BoxPtrDeleter<typeName> \
    { \
        void operator() (typeName* ptr) const \
        { \
            deleterFunction(ptr); \
        } \
    };

template<typename T> using BoxPtr = Box<std::unique_ptr<T, BoxPtrDeleter<T>>>;

template<typename T> BoxPtr<T> createBoxPtr(T* ptr)
{
    return Box<std::unique_ptr<T, BoxPtrDeleter<T>>>::create(ptr);
}

template<typename T> bool operator==(const BoxPtr<T>& lhs, const BoxPtr<T>& rhs)
{
    if (!lhs && !rhs)
        return true;

    if (!lhs || !rhs)
        return false;

    if (!lhs->get() && !rhs->get())
        return true;

    if (!lhs->get() || !rhs->get())
        return false;

    return *lhs == *rhs;
}

template<typename T> bool operator!=(const BoxPtr<T>& lhs, const BoxPtr<T>& rhs)
{
    return !(lhs == rhs);
}

} // namespace WTF

using WTF::createBoxPtr;
using WTF::BoxPtr;
