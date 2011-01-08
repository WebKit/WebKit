/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSDebugWrapperSet_h
#define JSDebugWrapperSet_h

#include "JSDOMWrapper.h"
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

// For debugging, keep a set of wrappers currently cached, and check that
// all are uncached before they are destroyed. This helps us catch bugs like:
//     - wrappers being deleted without being removed from the cache
//     - wrappers being cached twice

class JSDebugWrapperSet : public Noncopyable {
    friend class WTF::ThreadSpecific<JSDebugWrapperSet>;
public:
    static JSDebugWrapperSet& shared();

    void add(DOMObject* object) { m_wrapperSet.add(object); }
    void remove(DOMObject* object) { m_wrapperSet.remove(object); }
    bool contains(DOMObject* object) const { return m_wrapperSet.contains(object); }

    static void willCacheWrapper(DOMObject*);
    static void didUncacheWrapper(DOMObject*);

private:
    JSDebugWrapperSet();

    HashSet<DOMObject*> m_wrapperSet;
};

#ifdef NDEBUG

inline void JSDebugWrapperSet::willCacheWrapper(DOMObject*)
{
}

inline void JSDebugWrapperSet::didUncacheWrapper(DOMObject*)
{
}

#else

inline void JSDebugWrapperSet::willCacheWrapper(DOMObject* wrapper)
{
    ASSERT(!JSDebugWrapperSet::shared().contains(wrapper));
    JSDebugWrapperSet::shared().add(wrapper);
}

inline void JSDebugWrapperSet::didUncacheWrapper(DOMObject* wrapper)
{
    if (!wrapper)
        return;
    ASSERT(JSDebugWrapperSet::shared().contains(wrapper));
    JSDebugWrapperSet::shared().remove(wrapper);
}

#endif

} // namespace WebCore

#endif // JSDebugWrapperSet_h
