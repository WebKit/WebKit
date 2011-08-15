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

#ifndef IsolatedWorld_h
#define IsolatedWorld_h

#include "DOMWrapperWorld.h"
#include "V8DOMMap.h"

namespace WebCore {

// An DOMWrapperWorld other than the thread's normal world.
class IsolatedWorld : public DOMWrapperWorld {
public:
    static PassRefPtr<IsolatedWorld> create(int id) { return adoptRef(new IsolatedWorld(id)); }
    static int count() { return isolatedWorldCount; }

    int id() const { return m_id; }
    DOMDataStore* domDataStore() const { return m_domDataStore.getStore(); }

protected:
    explicit IsolatedWorld(int id);
    ~IsolatedWorld();

private:
    int m_id;

    // The backing store for the isolated world's DOM wrappers.  This class
    // doesn't have visibility into the wrappers.  This handle simply helps
    // manage their lifetime.
    DOMDataStoreHandle m_domDataStore;

    static int isolatedWorldCount;
};

} // namespace WebCore

#endif // IsolatedWorld_h
