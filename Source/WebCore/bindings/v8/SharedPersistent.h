/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SharedPersistent_h
#define SharedPersistent_h

#include <v8.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

    // A shareable reference to a v8 persistent handle.  Using a shared
    // persistent any number of objects can share a reference to a v8
    // object and when it should no longer be accessible the object's
    // owner can clear it.
    template <typename T>
    class SharedPersistent : public RefCounted<SharedPersistent<T> > {
    public:
        void set(v8::Persistent<T> value)
        {
            m_value = value;
        }
        v8::Persistent<T> get()
        {
            return m_value;
        }
        void disposeHandle()
        {
            if (!m_value.IsEmpty()) {
              m_value.Dispose();
              m_value.Clear();
            }
        }
        static PassRefPtr<SharedPersistent<T> > create(v8::Persistent<T> value)
        {
            return adoptRef(new SharedPersistent<T>(value));
        }
        static PassRefPtr<SharedPersistent<T> > create()
        {
            return create(v8::Persistent<T>());
        }
    private:
        explicit SharedPersistent(v8::Persistent<T> value) : m_value(value) { }
        v8::Persistent<T> m_value;
    };

} // namespace WebCore

#endif // SharedPersistent_h
