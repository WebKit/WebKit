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
#include "WebAccessibilityCacheImpl.h"

#include "AccessibilityObject.h"
#include "AXObjectCache.h"
#include "Document.h"
#include "Frame.h"

#include "WebAccessibilityObject.h"
#include "WebFrameImpl.h"
#include "WebViewImpl.h"

using namespace WebCore;

namespace WebKit {

const int invalidObjectId = -1;
const int firstObjectId = 1000;

static PassRefPtr<AccessibilityObject> toAccessibilityObject(const WebAccessibilityObject& object)
{
    return object;
}

// WebView ----------------------------------------------------------------

WebAccessibilityCache* WebAccessibilityCache::create()
{
    return new WebAccessibilityCacheImpl();
}

// WeakHandle -------------------------------------------------------------

PassRefPtr<WebAccessibilityCacheImpl::WeakHandle> WebAccessibilityCacheImpl::WeakHandle::create(AccessibilityObject* object)
{
    RefPtr<WebAccessibilityCacheImpl::WeakHandle> weakHandle = adoptRef(new WebAccessibilityCacheImpl::WeakHandle(object));
    weakHandle->m_object->setWrapper(weakHandle.get());
    
    return weakHandle.release();
}

WebAccessibilityCacheImpl::WeakHandle::WeakHandle(AccessibilityObject* object)
    : AccessibilityObjectWrapper(object)
{
}

// WebAccessibilityCacheImpl ----------------------------------------

void WebAccessibilityCacheImpl::WeakHandle::detach()
{
    if (m_object)
        m_object = 0;
}

WebAccessibilityCacheImpl::WebAccessibilityCacheImpl()
    : m_nextNewId(firstObjectId)
    , m_initialized(false)
{
}

WebAccessibilityCacheImpl::~WebAccessibilityCacheImpl()
{
}

void WebAccessibilityCacheImpl::initialize(WebView* view)
{
    AXObjectCache::enableAccessibility();
    WebAccessibilityObject root = view->accessibilityObject();
    if (root.isNull())
        return;

    RefPtr<AccessibilityObject> rootObject = toAccessibilityObject(root);

    // Insert root in hashmaps.
    m_objectMap.set(m_nextNewId, WeakHandle::create(rootObject.get()));
    m_idMap.set(rootObject.get(), m_nextNewId++);

    m_initialized = true;
}

WebAccessibilityObject WebAccessibilityCacheImpl::getObjectById(int id)
{
    ObjectMap::iterator it = m_objectMap.find(id);

    if (it == m_objectMap.end() || !it->second)
        return WebAccessibilityObject();

    return WebAccessibilityObject(it->second->accessibilityObject());
}

void WebAccessibilityCacheImpl::remove(int id)
{
    ObjectMap::iterator it = m_objectMap.find(id);

    if (it == m_objectMap.end())
        return;

    if (it->second) {
        // Erase element from reverse hashmap.
        IdMap::iterator it2 = m_idMap.find(it->second->accessibilityObject());
        if (it2 != m_idMap.end())
            m_idMap.remove(it2);
    }

    m_objectMap.remove(it);
}

void WebAccessibilityCacheImpl::clear()
{
    m_objectMap.clear();
    m_idMap.clear();
}

int WebAccessibilityCacheImpl::addOrGetId(const WebAccessibilityObject& object)
{
    if (!object.isValid())
        return invalidObjectId;

    RefPtr<AccessibilityObject> o = toAccessibilityObject(object);

    IdMap::iterator it = m_idMap.find(o.get());

    if (it != m_idMap.end())
        return it->second;

    // Insert new accessibility object in hashmaps and return its newly
    // assigned accessibility object id.
    m_objectMap.set(m_nextNewId, WeakHandle::create(o.get()));
    m_idMap.set(o.get(), m_nextNewId);

    return m_nextNewId++;
}

bool WebAccessibilityCacheImpl::isCached(const WebAccessibilityObject& object)
{
    if (!object.isValid())
        return false;

    RefPtr<AccessibilityObject> o = toAccessibilityObject(object);
    IdMap::iterator it = m_idMap.find(o.get());
    if (it == m_idMap.end())
        return false;
        
    return true;
}

}
