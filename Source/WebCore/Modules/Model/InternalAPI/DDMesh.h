/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/DDFloat4x4.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <simd/simd.h>
#include <wtf/MachSendRight.h>
#endif

namespace WebCore {
class TransformationMatrix;
enum class StageModeOperation : bool;
}

namespace WebCore::DDModel {

struct DDFloat4x4;
struct DDMaterialDescriptor;
struct DDMeshDescriptor;
struct DDTextureDescriptor;
struct DDUpdateMaterialDescriptor;
struct DDUpdateMeshDescriptor;
struct DDUpdateTextureDescriptor;

class DDMesh : public RefCountedAndCanMakeWeakPtr<DDMesh> {
public:
    virtual ~DDMesh() = default;

    String label() const { return m_label; }

    void setLabel(String&& label)
    {
        m_label = WTFMove(label);
        setLabelInternal(m_label);
    }

    virtual void update(const DDUpdateMeshDescriptor&) = 0;
    virtual void updateTexture(const DDUpdateTextureDescriptor&) = 0;
    virtual void updateMaterial(const DDUpdateMaterialDescriptor&) = 0;
    virtual bool isRemoteDDMeshProxy() const { return false; }
    virtual void setEntityTransform(const DDFloat4x4&) = 0;
    virtual std::optional<DDFloat4x4> entityTransform() const = 0;
    virtual bool supportsTransform(const WebCore::TransformationMatrix&) const { return false; }
    virtual void setScale(float) { }
    virtual void setCameraDistance(float) = 0;
    virtual void setStageMode(WebCore::StageModeOperation) { }
    virtual void setRotation(float, float = 0.f, float = 0.f) { }
    virtual void play(bool) = 0;

    virtual void render() = 0;
#if PLATFORM(COCOA)
    virtual Vector<MachSendRight> ioSurfaceHandles() { return { }; }
    virtual std::pair<simd_float4, simd_float4> getCenterAndExtents() const { return std::make_pair(simd_make_float4(0.f), simd_make_float4(0.f)); }
#endif

protected:
    DDMesh() = default;

private:
    DDMesh(const DDMesh&) = delete;
    DDMesh(DDMesh&&) = delete;
    DDMesh& operator=(const DDMesh&) = delete;
    DDMesh& operator=(DDMesh&&) = delete;

    virtual void setLabelInternal(const String&) = 0;

    String m_label;
};

}
