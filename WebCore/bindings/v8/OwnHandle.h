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

#ifndef OwnHandle_h
#define OwnHandle_h

#include <v8.h>

namespace WebCore {

    template<typename T>
    class OwnHandle {
    public:
        OwnHandle() { }
        explicit OwnHandle(v8::Handle<T> handle) : m_handle(v8::Persistent<T>::New(handle)) { }
        ~OwnHandle() { clear(); }

        v8::Handle<T> get() const { return m_handle; }
        void set(v8::Handle<T> handle) { clear(); m_handle = v8::Persistent<T>::New(handle); }

        // Note: This is clear in the OwnPtr sense, not the v8::Handle sense.
        void clear()
        {
            if (m_handle.IsEmpty())
                return;
            if (m_handle.IsWeak())
                m_handle.ClearWeak();
            m_handle.Dispose();
            m_handle.Clear();
        }

        // Make the underlying handle weak.  The client doesn't get a callback,
        // we just make the handle empty.
        void makeWeak()
        {
            if (m_handle.IsEmpty())
                return;
            m_handle.MakeWeak(this, &OwnHandle<T>::weakCallback);
        }

    private:
        static void weakCallback(v8::Persistent<v8::Value> object, void* ownHandle)
        {
            OwnHandle<T>* handle = static_cast<OwnHandle<T>*>(ownHandle);
            handle->clear();
        }

        v8::Persistent<T> m_handle;
    };

} // namespace WebCore

#endif // OwnHandle_h
