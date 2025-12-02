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

#include <WebCore/DDVertexAttributeFormat.h>
#include <WebCore/DDVertexLayout.h>

namespace WebCore::DDModel {

struct DDEdge {
    long upstreamNodeIndex;
    long downstreamNodeIndex;
    String upstreamOutputName;
    String downstreamInputName;
};

enum class DDDataType : uint8_t {
    kBool,
    kInt,
    kInt2,
    kInt3,
    kInt4,
    kFloat,
    kColor3f,
    kColor3h,
    kColor4f,
    kColor4h,
    kFloat2,
    kFloat3,
    kFloat4,
    kHalf,
    kHalf2,
    kHalf3,
    kHalf4,
    kMatrix2f,
    kMatrix3f,
    kMatrix4f,
    kSurfaceShader,
    kGeometryModifier,
    kString,
    kToken,
    kAsset
};

struct DDPrimvar {
    String name;
    String referencedGeomPropName;
    uint64_t attributeFormat;
};

struct DDInputOutput {
    DDDataType type;
    String name;
};

enum class DDConstant : uint8_t {
    kBool,
    kUchar,
    kInt,
    kUint,
    kHalf,
    kFloat,
    kTimecode,
    kString,
    kToken,
    kAsset,
    kMatrix2f,
    kMatrix3f,
    kMatrix4f,
    kQuatf,
    kQuath,
    kFloat2,
    kHalf2,
    kInt2,
    kFloat3,
    kHalf3,
    kInt3,
    kFloat4,
    kHalf4,
    kInt4,

    // semantic types
    kPoint3f,
    kPoint3h,
    kNormal3f,
    kNormal3h,
    kVector3f,
    kVector3h,
    kColor3f,
    kColor3h,
    kColor4f,
    kColor4h,
    kTexCoord2h,
    kTexCoord2f,
    kTexCoord3h,
    kTexCoord3f
};

enum class DDNodeType : uint8_t {
    Builtin,
    Constant,
    Arguments,
    Results
};

using DDNumberOrString = Variant<String, double>;

struct DDConstantContainer {
    DDConstant constant;
    Vector<DDNumberOrString> constantValues;
    String name;
};

struct DDBuiltin {
    String definition;
    String name;
};

struct DDNode {
    DDNodeType bridgeNodeType;
    DDBuiltin builtin;
    DDConstantContainer constant;
};

struct DDMaterialGraph {
    Vector<DDNode> nodes;
    Vector<DDEdge> edges;
    Vector<DDInputOutput> inputs;
    Vector<DDInputOutput> outputs;
    Vector<DDPrimvar> primvars;
    String identifier;
};

struct DDMaterialDescriptor {
    Vector<uint8_t> materialGraph;
    String identifier;
};

}
