/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import <simd/simd.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMalloc.h>

OBJC_CLASS NSMutableArray;
OBJC_CLASS NSUUID;
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS WKBridgeReceiver;
OBJC_CLASS WKBridgeUpdateMesh;
OBJC_CLASS WKBridgeUpdateTexture;
OBJC_CLASS WKBridgeImageAsset;
OBJC_PROTOCOL(MTLTexture);

struct WebModelCreateMeshDescriptor;

namespace WebModel {
struct ImageAsset;
struct ResizeMeshDescriptor;
struct TypedResourceId;
struct UpdateMaterialDescriptor;
struct UpdateMeshDescriptor;
struct UpdateTextureDescriptor;
}

namespace WebKit {

class WebMesh : public RefCounted<WebMesh> {
    WTF_MAKE_TZONE_ALLOCATED(WebMesh);
public:
    static Ref<WebMesh> create(const WebModelCreateMeshDescriptor& descriptor)
    {
        return adoptRef(*new WebMesh(descriptor));
    }

    ~WebMesh();

    bool NODELETE isValid() const;
    void NODELETE render(uint32_t textureIndex, Function<void(bool)>&&) const;
    void NODELETE update(Vector<WebModel::UpdateMeshDescriptor>&&);
    void NODELETE updateTexture(Vector<WebModel::UpdateTextureDescriptor>&&);
    void NODELETE updateMaterial(Vector<WebModel::UpdateMaterialDescriptor>&&);
    void NODELETE setTransform(const simd_float4x4&);
    void NODELETE setFOV(float);
    void NODELETE setBackgroundColor(const simd_float3&);
    void NODELETE setEnvironmentMap(const WebModel::UpdateTextureDescriptor&);
    void NODELETE play(bool);
    void NODELETE updateRenderBuffers(const WebModel::ResizeMeshDescriptor&);
    void processRemovals(Vector<WebModel::TypedResourceId>&& meshRemovals, Vector<WebModel::TypedResourceId>&& materialRemovals, Vector<WebModel::TypedResourceId>&& textureRemovals, CompletionHandler<void(bool)>&&);

private:
    WebMesh(const WebModelCreateMeshDescriptor&);

    RetainPtr<NSMutableArray> m_textures;

#if ENABLE(GPU_PROCESS_MODEL)
    RetainPtr<WKBridgeReceiver> m_receiver;
    RetainPtr<NSUUID> m_meshIdentifier;
    mutable bool m_meshDataExists { false };
    std::optional<simd_float4x4> m_transform;
#endif
};

} // namespace WebKit
