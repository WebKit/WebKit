/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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

#include "FloatPoint3D.h"
#include "VRFieldOfView.h"
#include "VRPlatformDisplay.h"

#include <JavaScriptCore/Float32Array.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class VREyeParameters : public RefCounted<VREyeParameters> {
public:
    using RenderSize = VRPlatformDisplayInfo::RenderSize;

    static Ref<VREyeParameters> create(const FloatPoint3D& offset, const VRPlatformDisplayInfo::FieldOfView& fieldOfView, const RenderSize& renderSize)
    {
        return adoptRef(*new VREyeParameters(offset, fieldOfView, renderSize));
    }

    Ref<Float32Array> offset() const;
    const FloatPoint3D& rawOffset() const { return m_offset; }

    const VRFieldOfView& fieldOfView() const;

    unsigned renderWidth() const;
    unsigned renderHeight() const;

private:
    VREyeParameters(const FloatPoint3D& offset, const VRPlatformDisplayInfo::FieldOfView&, const RenderSize&);

    Ref<VRFieldOfView> m_fieldOfView;
    FloatPoint3D m_offset;
    RenderSize m_renderSize;
};

} // namespace WebCore
