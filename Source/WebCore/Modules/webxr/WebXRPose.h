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
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class WebXRRigidTransform;

class WebXRPose : public RefCounted<WebXRPose> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebXRPose);
public:
    static Ref<WebXRPose> create(Ref<WebXRRigidTransform>&&, bool emulatedPosition);
    virtual ~WebXRPose();

    const WebXRRigidTransform& transform() const;
    bool emulatedPosition() const;

    virtual bool isViewerPose() const { return false; }

protected:
    WebXRPose(Ref<WebXRRigidTransform>&&, bool emulatedPosition);

    Ref<WebXRRigidTransform> m_transform;
    bool m_emulatedPosition { false };
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_WEBXRPOSE(ToValueTypeName, predicate)                    \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName)                               \
    static bool isType(const WebCore::WebXRPose& context) { return context.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBXR)
