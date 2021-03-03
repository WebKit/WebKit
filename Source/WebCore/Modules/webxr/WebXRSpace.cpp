/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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
#include "WebXRSpace.h"

#if ENABLE(WEBXR)

#include "DOMPointReadOnly.h"
#include "Document.h"
#include "WebXRRigidTransform.h"
#include "WebXRSession.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRSpace);

WebXRSpace::WebXRSpace(Document& document, Ref<WebXRRigidTransform>&& offset)
    : ContextDestructionObserver(&document)
    , m_originOffset(WTFMove(offset))
{
}

WebXRSpace::~WebXRSpace() = default;

TransformationMatrix WebXRSpace::effectiveOrigin() const
{
    // https://immersive-web.github.io/webxr/#xrspace-effective-origin
    // The effective origin can be obtained by multiplying origin offset and the native origin.
    return m_originOffset->rawTransform() * nativeOrigin();
}


WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRViewerSpace);

WebXRViewerSpace::WebXRViewerSpace(Document& document, WebXRSession& session)
    : WebXRSpace(document, WebXRRigidTransform::create())
    , m_session(session)
{
}

WebXRViewerSpace::~WebXRViewerSpace() = default;

TransformationMatrix WebXRViewerSpace::nativeOrigin() const
{
    return WebXRFrame::matrixFromPose(m_session.frameData().origin);
}


} // namespace WebCore

#endif // ENABLE(WEBXR)
