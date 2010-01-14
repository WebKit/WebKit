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
#include "DOMData.h"

#include "ChildThreadDOMData.h"
#include "MainThreadDOMData.h"
#include "WebGLContextAttributes.h"
#include "WebGLUniformLocation.h"

namespace WebCore {

DOMData::DOMData()
    : m_delayedProcessingScheduled(false)
    , m_isMainThread(WTF::isMainThread())
    , m_owningThread(WTF::currentThread())
{
}

DOMData::~DOMData()
{
}

DOMData* DOMData::getCurrent()
{
    if (WTF::isMainThread())
        return MainThreadDOMData::getCurrent();

    DEFINE_STATIC_LOCAL(WTF::ThreadSpecific<ChildThreadDOMData>, childThreadDOMData, ());
    return childThreadDOMData;
}

void DOMData::ensureDeref(V8ClassIndex::V8WrapperType type, void* domObject)
{
    if (m_owningThread == WTF::currentThread()) {
        // No need to delay the work. We can deref right now.
        derefObject(type, domObject);
        return;
    }

    // We need to do the deref on the correct thread.
    m_delayedObjectMap.set(domObject, type);

    // Post a task to the owning thread in order to process the delayed queue.
    // FIXME: For now, we can only post to main thread due to WTF task posting limitation. We will fix this when we work on nested worker.
    if (!m_delayedProcessingScheduled) {
        m_delayedProcessingScheduled = true;
        if (isMainThread())
            WTF::callOnMainThread(&derefDelayedObjectsInCurrentThread, 0);
    }
}

void DOMData::derefObject(V8ClassIndex::V8WrapperType type, void* domObject)
{
    switch (type) {
    case V8ClassIndex::NODE:
        static_cast<Node*>(domObject)->deref();
        break;

#define MakeCase(type, name)   \
        case V8ClassIndex::type: static_cast<name*>(domObject)->deref(); break;
    DOM_OBJECT_TYPES(MakeCase)   // This includes both active and non-active.
#undef MakeCase

#if ENABLE(SVG)
#define MakeCase(type, name)     \
        case V8ClassIndex::type: static_cast<name*>(domObject)->deref(); break;
    SVG_OBJECT_TYPES(MakeCase)   // This also includes SVGElementInstance.
#undef MakeCase

#define MakeCase(type, name)     \
        case V8ClassIndex::type:    \
            static_cast<V8SVGPODTypeWrapper<name>*>(domObject)->deref(); break;
    SVG_POD_NATIVE_TYPES(MakeCase)
#undef MakeCase
#endif

    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void DOMData::derefDelayedObjects()
{
    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());

    m_delayedProcessingScheduled = false;

    for (DelayedObjectMap::iterator iter(m_delayedObjectMap.begin()); iter != m_delayedObjectMap.end(); ++iter)
        derefObject(iter->second, iter->first);

    m_delayedObjectMap.clear();
}

void DOMData::derefDelayedObjectsInCurrentThread(void*)
{
    getCurrent()->derefDelayedObjects();
}

} // namespace WebCore
