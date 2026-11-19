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

#include "config.h"
#include "WebXRAnchorSet.h"

#if ENABLE(WEBXR_ANCHORS)

#include "JSDOMSetLike.h"
#include "JSWebXRAnchor.h"
#include "WebXRAnchor.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRAnchorSet);

Ref<WebXRAnchorSet> WebXRAnchorSet::create(Vector<Ref<WebXRAnchor>>&& anchors)
{
    return adoptRef(*new WebXRAnchorSet(WTF::move(anchors)));
}

WebXRAnchorSet::WebXRAnchorSet(Vector<Ref<WebXRAnchor>>&& anchors)
    : m_anchors(WTF::move(anchors))
{
}

WebXRAnchorSet::~WebXRAnchorSet() = default;

void WebXRAnchorSet::initializeSetLike(DOMSetAdapter& adapter) const
{
    for (auto& anchor : m_anchors)
        adapter.add<IDLInterface<WebXRAnchor>>(anchor.get());
}

} // namespace WebCore

#endif // ENABLE(WEBXR_ANCHORS)
