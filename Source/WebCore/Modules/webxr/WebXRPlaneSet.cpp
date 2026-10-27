/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "WebXRPlaneSet.h"

#if ENABLE(WEBXR_PLANE_DETECTION)

#include "JSDOMSetLike.h"
#include "JSWebXRPlane.h"
#include "WebXRPlane.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRPlaneSet);

Ref<WebXRPlaneSet> WebXRPlaneSet::create(Vector<Ref<WebXRPlane>>&& planes)
{
    return adoptRef(*new WebXRPlaneSet(WTF::move(planes)));
}

WebXRPlaneSet::WebXRPlaneSet(Vector<Ref<WebXRPlane>>&& planes)
    : m_planes(WTF::move(planes))
{
}

WebXRPlaneSet::~WebXRPlaneSet() = default;

void WebXRPlaneSet::initializeSetLike(DOMSetAdapter& set) const
{
    for (auto& plane : m_planes)
        set.add<IDLInterface<WebXRPlane>>(plane.get());
}

} // namespace WebCore

#endif // ENABLE(WEBXR_PLANE_DETECTION)
