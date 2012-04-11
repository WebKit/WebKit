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

#ifndef StaticDOMDataStore_h
#define StaticDOMDataStore_h

#include "DOMDataStore.h"
#include "IntrusiveDOMWrapperMap.h"

namespace WebCore {

// StaticDOMDataStore
//
// StaticDOMDataStore is a DOMDataStore that manages the lifetime of the store
// statically.  This encapsulates thread-specific DOM data for the main
// thread.  All the maps in it are static.  This is because we are unable to
// rely on WTF::ThreadSpecificThreadExit to do the cleanup since the place that
// tears down the main thread can not call any WTF functions.
//
class StaticDOMDataStore : public DOMDataStore {
public:
    StaticDOMDataStore();
    virtual ~StaticDOMDataStore();

private:
    IntrusiveDOMWrapperMap m_staticDomNodeMap;
    IntrusiveDOMWrapperMap m_staticActiveDomNodeMap;
    DOMWrapperMap<void> m_staticDomObjectMap;
    DOMWrapperMap<void> m_staticActiveDomObjectMap;
};

} // namespace WebCore

#endif // StaticDOMDataStore_h
