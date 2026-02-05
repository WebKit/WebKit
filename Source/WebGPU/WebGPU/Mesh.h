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

#import <WebGPU/ModelTypes.h>
#import <wtf/Ref.h>
#import <wtf/RefCountedAndCanMakeWeakPtr.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/WeakHashSet.h>
#import <wtf/WeakPtr.h>

typedef struct CF_BRIDGED_TYPE(id) __CVBuffer* CVPixelBufferRef;

struct WebMeshImpl {
};

@class WebBridgeImageAsset;
@class WebBridgeReceiver;
@class WebBridgeUpdateMaterial;
@class WebBridgeUpdateMesh;
@class WebBridgeUpdateTexture;

namespace WebGPU {
class Instance;
}

namespace WebModel {

class Mesh : public ThreadSafeRefCounted<Mesh>, public WebMeshImpl {
    WTF_MAKE_TZONE_ALLOCATED(Mesh);
public:
    static Ref<Mesh> create(const WebModelCreateMeshDescriptor& descriptor, WebGPU::Instance& instance)
    {
        return adoptRef(*new Mesh(descriptor, instance));
    }

    ~Mesh();

    bool isValid() const;
    void update(WebBridgeUpdateMesh*);
    void updateTexture(WebBridgeUpdateTexture*);
    void updateMaterial(const WebModel::UpdateMaterialDescriptor&);
    void play(bool);

    id<MTLTexture> texture() const;
    void render() const;
    void setTransform(const simd_float4x4&);
    void setCameraDistance(float);
    void setEnvironmentMap(WebBridgeImageAsset *);

private:
    Mesh(const WebModelCreateMeshDescriptor&, WebGPU::Instance&);
    void processUpdates() const;

    const Ref<WebGPU::Instance> m_instance;
    WebModelCreateMeshDescriptor m_descriptor;
    NSMutableArray<id<MTLTexture>>* m_textures { nil };

#if ENABLE(GPU_PROCESS_MODEL)
    WebBridgeReceiver* m_ddReceiver;
    simd_float4x4 m_transform { matrix_identity_float4x4 };
    NSUUID* m_meshIdentifier;
    mutable uint32_t m_currentTexture { 0 };
    mutable bool m_meshDataExists { false };
    mutable NSMutableDictionary<NSString *, WebBridgeUpdateMesh *> *m_batchedUpdates;
#endif
};

}

