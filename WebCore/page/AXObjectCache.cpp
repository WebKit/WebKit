/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "config.h"
#include "AXObjectCache.h"

#include "AccessibilityObject.h"
#include "RenderObject.h"

#include <wtf/PassRefPtr.h>

namespace WebCore {

bool AXObjectCache::gAccessibilityEnabled = false;

AXObjectCache::~AXObjectCache()
{
    HashMap<RenderObject*, RefPtr<AccessibilityObject> >::iterator end = m_objects.end();
    for (HashMap<RenderObject*, RefPtr<AccessibilityObject> >::iterator it = m_objects.begin(); it != end; ++it) {
        AccessibilityObject* obj = (*it).second.get();
        detachWrapper(obj);
        obj->detach();
    }
}

AccessibilityObject* AXObjectCache::get(RenderObject* renderer)
{
    RefPtr<AccessibilityObject> obj = m_objects.get(renderer).get();
    if (obj)
        return obj.get();
    obj = AccessibilityObject::create(renderer);
    m_objects.set(renderer, obj);    
    attachWrapper(obj.get());
    return obj.get();
}

void AXObjectCache::remove(RenderObject* renderer)
{
    // first fetch object to operate some cleanup functions on it 
    AccessibilityObject* obj = m_objects.get(renderer).get();
    if (obj) {
        detachWrapper(obj);
        obj->detach();
        
        // finally remove the object
        if (!m_objects.take(renderer)) {
            ASSERT(!renderer->hasAXObject());
            return;
        }
    }
#if PLATFORM(MAC)
    ASSERT(m_objects.size() >= m_idsInUse.size());
#endif
}

void AXObjectCache::childrenChanged(RenderObject* renderer)
{
    AccessibilityObject* obj = m_objects.get(renderer).get();
    if (obj)
        obj->childrenChanged();
}

} // namespace WebCore
