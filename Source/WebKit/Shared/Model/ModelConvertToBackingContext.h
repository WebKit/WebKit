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

#if ENABLE(GPU_PROCESS)

#include "DDModelIdentifier.h"
#include <wtf/RefCounted.h>

namespace WebCore::DDModel {

class DDMesh;

struct ObjectDescriptorBase;
struct DDMeshDescriptor;
struct DDUpdateMeshDescriptor;
struct DDMeshPart;
struct DDReplaceVertices;
struct DDVertexAttributeFormat;
struct DDVertexLayout;

}

namespace WebKit::DDModel {

struct ObjectDescriptorBase;
struct DDMeshDescriptor;
struct DDUpdateMeshDescriptor;
struct DDMeshPart;
struct DDReplaceVertices;
struct DDVertexAttributeFormat;
struct DDVertexLayout;

class ConvertToBackingContext : public RefCounted<ConvertToBackingContext> {
public:
    virtual ~ConvertToBackingContext() = default;

    virtual DDModelIdentifier convertToBacking(const WebCore::DDModel::DDMesh&) = 0;

    std::optional<ObjectDescriptorBase> convertToBacking(const WebCore::DDModel::ObjectDescriptorBase&);
    std::optional<DDMeshDescriptor> convertToBacking(const WebCore::DDModel::DDMeshDescriptor&);
    std::optional<DDUpdateMeshDescriptor> convertToBacking(const WebCore::DDModel::DDUpdateMeshDescriptor&);
    std::optional<DDReplaceVertices> convertToBacking(const WebCore::DDModel::DDReplaceVertices&);
    std::optional<DDMeshPart> convertToBacking(const WebCore::DDModel::DDMeshPart&);
    std::optional<DDVertexAttributeFormat> convertToBacking(const WebCore::DDModel::DDVertexAttributeFormat&);
    std::optional<DDVertexLayout> convertToBacking(const WebCore::DDModel::DDVertexLayout&);
};

}

#endif // ENABLE(GPU_PROCESS)
