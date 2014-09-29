/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef OSObjectPtr_h
#define OSObjectPtr_h

#include <os/object.h>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101000

#if __has_include(<os/object_private.h>)
#include <os/object_private.h>
#endif

#if OS_OBJECT_USE_OBJC
@class OS_object;
typedef OS_object *_os_object_t;
#else
typedef struct _os_object_s *_os_object_t;
#endif

extern "C" _os_object_t _os_object_retain(_os_object_t object);
extern "C" void _os_object_release(_os_object_t object);

#endif


namespace WTF {

template<typename T> class OSObjectPtr;
template<typename T> OSObjectPtr<T> adoptOSObject(T);

template<typename T>
static inline void retainOSObject(T ptr)
{
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000 || PLATFORM(IOS)
    os_retain(ptr);
#else
#if OS_OBJECT_USE_OBJC_RETAIN_RELEASE
    [ptr retain];
#else
    _os_object_retain((_os_object_t)ptr);
#endif
#endif
}

template<typename T>
static inline void releaseOSObject(T ptr)
{
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000 || PLATFORM(IOS)
    os_release(ptr);
#else
#if OS_OBJECT_USE_OBJC_RETAIN_RELEASE
    [ptr release];
#else
    _os_object_release((_os_object_t)ptr);
#endif
#endif
}

template<typename T> class OSObjectPtr {
public:
    OSObjectPtr()
        : m_ptr(nullptr)
    {
    }

    ~OSObjectPtr()
    {
        if (m_ptr)
            releaseOSObject(m_ptr);
    }

    T get() const { return m_ptr; }

    explicit operator bool() const { return m_ptr; }
    bool operator!() const { return !m_ptr; }

    OSObjectPtr(const OSObjectPtr& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            retainOSObject(m_ptr);
    }

    OSObjectPtr(OSObjectPtr&& other)
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    OSObjectPtr& operator=(const OSObjectPtr& other)
    {
        OSObjectPtr ptr = other;
        swap(ptr);
        return *this;
    }

    OSObjectPtr& operator=(OSObjectPtr&& other)
    {
        OSObjectPtr ptr = WTF::move(other);
        swap(ptr);
        return *this;
    }

    OSObjectPtr& operator=(std::nullptr_t)
    {
        if (m_ptr)
            releaseOSObject(m_ptr);
        m_ptr = nullptr;

        return *this;
    }

    void swap(OSObjectPtr& other)
    {
        std::swap(m_ptr, other.m_ptr);
    }

    friend OSObjectPtr adoptOSObject<T>(T);

private:
    struct AdoptOSObject { };
    OSObjectPtr(AdoptOSObject, T ptr)
        : m_ptr(ptr)
    {
    }

    T m_ptr;
};

template<typename T> inline OSObjectPtr<T> adoptOSObject(T ptr)
{
    return OSObjectPtr<T>(typename OSObjectPtr<T>::AdoptOSObject { }, ptr);
}

} // namespace WTF

using WTF::OSObjectPtr;
using WTF::adoptOSObject;

#endif // OSObjectPtr_h
