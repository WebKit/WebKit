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

#pragma once

#if ENABLE(WEBXR)

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "TransformationMatrix.h"
#include "WebXRSession.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class Document;
class ScriptExecutionContext;
class WebXRRigidTransform;

class WebXRSpace : public EventTargetWithInlineData, public ContextDestructionObserver {
    WTF_MAKE_ISO_ALLOCATED(WebXRSpace);
public:
    virtual ~WebXRSpace();

    virtual WebXRSession* session() const = 0;
    virtual std::optional<TransformationMatrix> nativeOrigin() const = 0;
    std::optional<TransformationMatrix> effectiveOrigin() const;
    virtual std::optional<bool> isPositionEmulated() const;

    virtual bool isReferenceSpace() const { return false; }
    virtual bool isBoundedReferenceSpace() const { return false; }
#if ENABLE(WEBXR_HANDS)
    virtual bool isJointSpace() const { return false; }
#endif

protected:
    WebXRSpace(Document&, Ref<WebXRRigidTransform>&&);

    const WebXRRigidTransform& originOffset() const { return m_originOffset.get(); }

    // EventTarget
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

private:
    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return WebXRSpaceEventTargetInterfaceType; }

    Ref<WebXRRigidTransform> m_originOffset;
};

// https://immersive-web.github.io/webxr/#xrsession-viewer-reference-space
// This is a helper class to implement the viewer space owned by a WebXRSession.
// It avoids a circular reference between the session and the reference space.
class WebXRViewerSpace : public WebXRSpace {
    WTF_MAKE_ISO_ALLOCATED(WebXRViewerSpace);
public:
    WebXRViewerSpace(Document&, WebXRSession&);
    virtual ~WebXRViewerSpace();

private:
    WebXRSession* session() const final { return m_session.get(); }
    std::optional<TransformationMatrix> nativeOrigin() const final;

    void refEventTarget() final { RELEASE_ASSERT_NOT_REACHED(); }
    void derefEventTarget() final { RELEASE_ASSERT_NOT_REACHED(); }

    WeakPtr<WebXRSession> m_session;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_WEBXRSPACE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::WebXRSpace& context) { return context.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBXR)
