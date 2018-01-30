/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include <wtf/FastMalloc.h>
#include <wtf/Poisoned.h>

#include <cstddef>
#include <memory>

namespace WTF {

template<typename Poison, typename T, typename Enable = void>
class PoisonedUniquePtr : public Poisoned<Poison, T*> {
    WTF_MAKE_FAST_ALLOCATED;
    using Base = Poisoned<Poison, T*>;
public:
    static constexpr bool isPoisonedUniquePtrType = true;
    using ValueType = T;

    PoisonedUniquePtr() = default;
    PoisonedUniquePtr(std::nullptr_t) : Base() { }
    PoisonedUniquePtr(T* ptr) : Base(ptr) { }
    PoisonedUniquePtr(PoisonedUniquePtr&& ptr) : Base(WTFMove(ptr)) { ptr.clearWithoutDestroy(); }
    PoisonedUniquePtr(std::unique_ptr<T>&& unique) : PoisonedUniquePtr(unique.release()) { }

    template<typename U, typename = std::enable_if_t<std::is_base_of<T, U>::value>>
    PoisonedUniquePtr(std::unique_ptr<U>&& unique)
        : Base(unique.release())
    { }

    template<typename Other, typename = std::enable_if_t<Other::isPoisonedUniquePtrType && std::is_base_of<T, typename Other::ValueType>::value>>
    PoisonedUniquePtr(Other&& ptr)
        : Base(ptr.unpoisoned())
    {
        ptr.clearWithoutDestroy();
    }

    PoisonedUniquePtr(const PoisonedUniquePtr&) = delete;

    ~PoisonedUniquePtr() { destroy(); }

    template<typename... Arguments>
    static PoisonedUniquePtr create(Arguments&&... arguments)
    {
        return new T(std::forward<Arguments>(arguments)...);
    }

    PoisonedUniquePtr& operator=(T* ptr)
    {
        if (LIKELY(this->unpoisoned() != ptr)) {
            this->clear();
            this->Base::operator=(ptr);
        }
        return *this;
    }

    PoisonedUniquePtr& operator=(std::unique_ptr<T>&& unique)
    {
        this->clear();
        this->Base::operator=(unique.release());
        return *this;
    }

    template<typename U, typename = std::enable_if_t<std::is_base_of<T, U>::value>>
    PoisonedUniquePtr& operator=(std::unique_ptr<U>&& unique)
    {
        this->clear();
        this->Base::operator=(unique.release());
        return *this;
    }

    template<typename Other, typename = std::enable_if_t<Other::isPoisonedUniquePtrType && std::is_base_of<T, typename Other::ValueType>::value>>
    PoisonedUniquePtr& operator=(Other&& ptr)
    {
        ASSERT(this == static_cast<void*>(&ptr) || this->unpoisoned() != ptr.unpoisoned());
        if (LIKELY(this != static_cast<void*>(&ptr))) {
            this->clear();
            this->Base::operator=(WTFMove(ptr));
            ptr.clearWithoutDestroy();
        }
        return *this;
    }

    PoisonedUniquePtr& operator=(const PoisonedUniquePtr&) = delete;

    T* get() const { return this->unpoisoned(); }
    T& operator[](size_t index) const { return this->unpoisoned()[index]; }

    void clear()
    {
        destroy();
        clearWithoutDestroy();
    }

private:
    void destroy()
    {
        if (!this->bits())
            return;
        delete this->unpoisoned();
    }

    void clearWithoutDestroy() { Base::clear(); }

    template<typename, typename, typename> friend class PoisonedUniquePtr;
};

template<typename Poison, typename T>
class PoisonedUniquePtr<Poison, T[]> : public Poisoned<Poison, T*> {
    WTF_MAKE_FAST_ALLOCATED;
    using Base = Poisoned<Poison, T*>;
public:
    static constexpr bool isPoisonedUniquePtrType = true;
    using ValueType = T[];

    PoisonedUniquePtr() = default;
    PoisonedUniquePtr(std::nullptr_t) : Base() { }
    PoisonedUniquePtr(T* ptr) : Base(ptr) { }
    PoisonedUniquePtr(PoisonedUniquePtr&& ptr) : Base(WTFMove(ptr)) { ptr.clearWithoutDestroy(); }
    PoisonedUniquePtr(std::unique_ptr<T[]>&& unique) : PoisonedUniquePtr(unique.release()) { }

    template<typename Other, typename = std::enable_if_t<Other::isPoisonedUniquePtrType && std::is_same<T[], typename Other::ValueType>::value>>
    PoisonedUniquePtr(Other&& ptr)
        : Base(ptr.unpoisoned())
    {
        ptr.clearWithoutDestroy();
    }

    PoisonedUniquePtr(const PoisonedUniquePtr&) = delete;

    ~PoisonedUniquePtr() { destroy(); }

    template<typename... Arguments>
    static PoisonedUniquePtr create(size_t count, Arguments&&... arguments)
    {
        T* result = new T[count];
        while (count--)
            new (result + count) T(std::forward<Arguments>(arguments)...);
        return result;
    }

    PoisonedUniquePtr& operator=(T* ptr)
    {
        if (LIKELY(this->unpoisoned() != ptr)) {
            this->clear();
            this->Base::operator=(ptr);
        }
        return *this;
    }
    
    PoisonedUniquePtr& operator=(std::unique_ptr<T[]>&& unique)
    {
        this->clear();
        this->Base::operator=(unique.release());
        return *this;
    }

    template<typename Other, typename = std::enable_if_t<Other::isPoisonedUniquePtrType && std::is_same<T[], typename Other::ValueType>::value>>
    PoisonedUniquePtr& operator=(Other&& ptr)
    {
        ASSERT(this == static_cast<void*>(&ptr) || this->unpoisoned() != ptr.unpoisoned());
        if (LIKELY(this != static_cast<void*>(&ptr))) {
            this->clear();
            this->Base::operator=(WTFMove(ptr));
            ptr.clearWithoutDestroy();
        }
        return *this;
    }

    PoisonedUniquePtr& operator=(const PoisonedUniquePtr&) = delete;

    T* get() const { return this->unpoisoned(); }
    T& operator[](size_t index) const { return this->unpoisoned()[index]; }

    void clear()
    {
        destroy();
        clearWithoutDestroy();
    }

private:
    void destroy()
    {
        if (!this->bits())
            return;
        delete[] this->unpoisoned();
    }

    void clearWithoutDestroy() { Base::clear(); }

    template<typename, typename, typename> friend class PoisonedUniquePtr;
};

} // namespace WTF

using WTF::PoisonedUniquePtr;

