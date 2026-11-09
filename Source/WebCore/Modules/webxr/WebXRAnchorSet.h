/*
 * Copyright (C) 2026 Igalia, S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in source code must reproduce the above copyright
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

#pragma once

#if ENABLE(WEBXR_ANCHORS)

#include "ScriptWrappable.h"
#include "WebXRAnchor.h"
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class DOMSetAdapter;

class WebXRAnchorSet : public ScriptWrappable, public RefCounted<WebXRAnchorSet> {
    WTF_MAKE_TZONE_ALLOCATED(WebXRAnchorSet);
public:
    static Ref<WebXRAnchorSet> create()
    {
        return adoptRef(*new WebXRAnchorSet());
    }

    ~WebXRAnchorSet();

    void initializeSetLike(DOMSetAdapter&);
    bool has(WebXRAnchor&) const;

    void add(Ref<WebXRAnchor>&&);
    void remove(WebXRAnchor&);
    void clear();

    const Vector<Ref<WebXRAnchor>>& anchors() const { return m_anchors; }

private:
    WebXRAnchorSet();

    Vector<Ref<WebXRAnchor>> m_anchors;
};

} // namespace WebCore

#endif // ENABLE(WEBXR_ANCHORS)
