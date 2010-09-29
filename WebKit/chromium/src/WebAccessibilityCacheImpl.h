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

#ifndef WebAccessibilityCacheImpl_h
#define WebAccessibilityCacheImpl_h

#include "AccessibilityObjectWrapper.h"
#include "WebAccessibilityCache.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>

namespace WebKit {

// FIXME: Should be eliminated to use AXObjectCache instead.
class WebAccessibilityCacheImpl : public WebKit::WebAccessibilityCache {
public:
    virtual void initialize(WebView* view);
    virtual bool isInitialized() const { return m_initialized; }

    virtual WebAccessibilityObject getObjectById(int);
    virtual bool isValidId(int) const;
    virtual int addOrGetId(const WebKit::WebAccessibilityObject&);
    virtual bool isCached(const WebAccessibilityObject&);

    virtual void remove(int);
    virtual void clear();

protected:
    friend class WebKit::WebAccessibilityCache;

    WebAccessibilityCacheImpl();
    ~WebAccessibilityCacheImpl();

private:
    // FIXME: This can be just part of Chromium's AccessibilityObjectWrapper.
    class WeakHandle : public WebCore::AccessibilityObjectWrapper {
    public:
        static PassRefPtr<WeakHandle> create(WebCore::AccessibilityObject*);
        virtual void detach();
    private:
        WeakHandle(WebCore::AccessibilityObject*);
    };

    typedef HashMap<int, RefPtr<WeakHandle> > ObjectMap;
    typedef HashMap<WebCore::AccessibilityObject*, int> IdMap;

    // Hashmap for caching of elements in use by the AT, mapping id (int) to
    // WebAccessibilityObject.
    ObjectMap m_objectMap;
    // Hashmap for caching of elements in use by the AT, mapping a
    // AccessibilityObject pointer to its id (int). Needed for reverse lookup,
    // to ensure unnecessary duplicate entries are not created in the
    // ObjectMap and for focus changes in WebKit.
    IdMap m_idMap;

    // Unique identifier for retrieving an accessibility object from the page's
    // hashmaps. Id is always 0 for the root of the accessibility object
    // hierarchy (on a per-renderer process basis).
    int m_nextNewId;

    bool m_initialized;
};

}

#endif
