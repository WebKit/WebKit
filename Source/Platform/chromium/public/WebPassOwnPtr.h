/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPassOwnPtr_h
#define WebPassOwnPtr_h

#include "WebCommon.h"

#if WEBKIT_IMPLEMENTATION
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#else
#include "base/memory/scoped_ptr.h"
#endif

namespace WebKit {

// Use a return value of type WebPassOwnPtr<T> when the caller takes ownership of the returned value, for instance
// in a createFoo() factory function. Example:
//
// To implement the class WebPassOwnPtr<Foo> createFoo() in chromium code, write:
//
// WebPassOwnPtr<Foo> createFoo() {
//   scoped_ptr<Foo> foo = ...;
//   return foo.Pass();
// }
//
// To use on the WebKit side:
//
//   OwnPtr<Foo> foo = ...->createFoo();
//
// To implement WebPassOwnPtr<Foo> createFoo() in WebKit and use it from chromium, write:
//
// WebPassOwnPtr<Foo> createFoo()
// {
//     OwnPtr<Foo> foo = ...;
//     return foo.release();
// }
//
// and
//
//   scoped_ptr<Foo> = ...->createFoo();

template <typename T>
class WebPassOwnPtr : public WebNonCopyable {
public:
    WebPassOwnPtr()
        : m_ptr(0)
    {
    }

    ~WebPassOwnPtr()
    {
        WEBKIT_ASSERT(!m_ptr);
    }


#if WEBKIT_IMPLEMENTATION
    WebPassOwnPtr(PassOwnPtr<T> ptr)
        : m_ptr(ptr.leakPtr())
    {
    }

    operator PassOwnPtr<T>()
    {
        OwnPtr<T> ret = adoptPtr(m_ptr);
        m_ptr = 0;
        return ret.release();
    }
#else
    WebPassOwnPtr(scoped_ptr<T> ptr)
        : m_ptr(ptr.release())
    {
    }

    operator scoped_ptr<T>()
    {
        scoped_ptr<T> ret(m_ptr);
        m_ptr = 0;
        return ret.Pass();
    }
#endif

private:
    T* m_ptr;

    // This constructor has to be declared but not defined to trigger move emulation.
    WebPassOwnPtr(WebPassOwnPtr&);
};

}

#endif // WebPassOwnPtr_h
