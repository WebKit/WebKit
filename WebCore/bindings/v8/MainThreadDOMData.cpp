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

#include "config.h"
#include "MainThreadDOMData.h"

#include "V8IsolatedWorld.h"

namespace WebCore {

MainThreadDOMData::MainThreadDOMData()
    : m_defaultStore(this)
{
}
    
MainThreadDOMData* MainThreadDOMData::getCurrent()
{
    ASSERT(WTF::isMainThread());
    DEFINE_STATIC_LOCAL(MainThreadDOMData, mainThreadDOMData, ());
    return &mainThreadDOMData;
}

DOMDataStore& MainThreadDOMData::getMainThreadStore()
{
    // This is broken out as a separate non-virtual method from getStore()
    // so that it can be inlined by getCurrentMainThreadStore, which is
    // a hot spot in Dromaeo DOM tests.
    ASSERT(WTF::isMainThread());
    V8IsolatedWorld* world = V8IsolatedWorld::getEntered();
    if (UNLIKELY(world != 0))
        return *world->getDOMDataStore();
    return m_defaultStore;
}

DOMDataStore& MainThreadDOMData::getCurrentMainThreadStore()
{
    return getCurrent()->getMainThreadStore();
}


} // namespace WebCore
