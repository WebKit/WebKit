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

#include "config.h"
#include "RemoteMeshProxy.h"

#if ENABLE(GPU_PROCESS)

#include "ModelConvertToBackingContext.h"
#include "RemoteMeshMessages.h"
#include <WebCore/StageModeOperations.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(GPU_PROCESS_MODEL)
#include "Float3.h"
#include "Float4x4.h"
#include "ModelTypes.h"
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMeshProxy);

#if ENABLE(GPU_PROCESS_MODEL)
constexpr float tolerance = 1e-5f;
static bool areSameSignAndAlmostEqual(float a, float b)
{
    if (a * b < 0)
        return false;

    float absA = std::abs(a);
    float absB = std::abs(b);
    return std::abs(absA - absB) < tolerance * std::min(absA, absB);
}

static WebModel::Float4x4 makeTransformMatrix(
    const simd_float3& translation,
    const simd_float3& scale,
    const WebModel::Float3x3& rotation)
{
    WebModel::Float4x4 result;
    result.column0 = simd_make_float4(rotation.column0 * scale[0], 0.f);
    result.column1 = simd_make_float4(rotation.column1 * scale[1], 0.f);
    result.column2 = simd_make_float4(rotation.column2 * scale[2], 0.f);
    result.column3 = simd_make_float4(translation, 1.f);

    return result;
}

static std::pair<simd_float4, simd_float4> computeMinAndMaxCorners(const Vector<WebModel::MeshPart>& parts, const Vector<WebModel::Float4x4>& instanceTransforms)
{
    simd_float4 minCorner4 = simd_make_float4(FLT_MAX, FLT_MAX, FLT_MAX, 1.f);
    simd_float4 maxCorner4 = simd_make_float4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.f);

    for (auto& part : parts) {
        // Get the 8 corners of the axis-aligned bounding box
        std::array<simd_float3, 8> corners;
        corners[0] = simd_make_float3(part.boundsMin.x, part.boundsMin.y, part.boundsMin.z);
        corners[1] = simd_make_float3(part.boundsMax.x, part.boundsMin.y, part.boundsMin.z);
        corners[2] = simd_make_float3(part.boundsMin.x, part.boundsMax.y, part.boundsMin.z);
        corners[3] = simd_make_float3(part.boundsMax.x, part.boundsMax.y, part.boundsMin.z);
        corners[4] = simd_make_float3(part.boundsMin.x, part.boundsMin.y, part.boundsMax.z);
        corners[5] = simd_make_float3(part.boundsMax.x, part.boundsMin.y, part.boundsMax.z);
        corners[6] = simd_make_float3(part.boundsMin.x, part.boundsMax.y, part.boundsMax.z);
        corners[7] = simd_make_float3(part.boundsMax.x, part.boundsMax.y, part.boundsMax.z);

        for (auto& transform : instanceTransforms) {
            for (int j = 0; j < 8; ++j) {
                simd_float4 corner4 = simd_make_float4(corners[j].x, corners[j].y, corners[j].z, 1.f);
                simd_float4 transformedCorner = simd_mul(transform, corner4);

                minCorner4 = simd_min(transformedCorner, minCorner4);
                maxCorner4 = simd_max(transformedCorner, maxCorner4);
            }
        }
    }

    return std::make_pair(minCorner4, maxCorner4);
}
#endif

RemoteMeshProxy::RemoteMeshProxy(Ref<RemoteGPUProxy>&& root, ModelConvertToBackingContext& convertToBackingContext, WebModelIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_root(WTF::move(root))
#if PLATFORM(COCOA)
    , m_minCorner(simd_make_float4(FLT_MAX, FLT_MAX, FLT_MAX, 1.f))
    , m_maxCorner(simd_make_float4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.f))
#endif
{
}

RemoteMeshProxy::~RemoteMeshProxy()
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::Destruct());
    UNUSED_VARIABLE(sendResult);
#endif
}

#if ENABLE(GPU_PROCESS_MODEL)
static WebModel::Float4x4 buildTranslation(float x, float y, float z)
{
    WebModel::Float4x4 result = matrix_identity_float4x4;
    result.column3 = simd_make_float4(x, y, z, 1.f);
    return result;
}
#endif

void RemoteMeshProxy::update(const WebModel::UpdateMeshDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto [minCorner, maxCorner] = computeMinAndMaxCorners(descriptor.parts, descriptor.instanceTransforms);
    auto boundingBoxChanged = minCorner.x <= maxCorner.x && minCorner.y <= maxCorner.y && minCorner.z <= maxCorner.z;
    boundingBoxChanged = boundingBoxChanged && (!simd_equal(m_minCorner, minCorner) || !simd_equal(m_maxCorner, maxCorner));
    if (boundingBoxChanged) {
        m_minCorner = simd_min(m_minCorner, minCorner);
        m_maxCorner = simd_max(m_maxCorner, maxCorner);
    }

    auto [center, extents] = getCenterAndExtents();
    if (boundingBoxChanged)
        setCameraDistance(std::max(extents.x, extents.y) * .5f);

    auto sendResult = sendWithAsyncReply(Messages::RemoteMesh::Update(descriptor), [](auto) mutable {
    });
    UNUSED_VARIABLE(sendResult);
    if (boundingBoxChanged)
        setStageMode(m_stageMode);

#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteMeshProxy::render()
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::Render());
    UNUSED_PARAM(sendResult);
#endif
}

void RemoteMeshProxy::setLabelInternal(const String& label)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
#else
    UNUSED_PARAM(label);
#endif
}

void RemoteMeshProxy::updateTexture(const WebModel::UpdateTextureDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = sendWithAsyncReply(Messages::RemoteMesh::UpdateTexture(descriptor), [](auto) mutable {
    });
    UNUSED_VARIABLE(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteMeshProxy::updateMaterial(const WebModel::UpdateMaterialDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = sendWithAsyncReply(Messages::RemoteMesh::UpdateMaterial(descriptor), [](auto) mutable {
    });
    UNUSED_VARIABLE(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

#if PLATFORM(COCOA)
std::pair<simd_float4, simd_float4> RemoteMeshProxy::getCenterAndExtents() const
{
    auto center = .5f * (m_minCorner + m_maxCorner);
    auto extents = m_maxCorner - m_minCorner;
    return std::make_pair(center, extents);
}
#endif

void RemoteMeshProxy::setEntityTransform(const WebModel::Float4x4& transform)
{
#if ENABLE(GPU_PROCESS_MODEL)
    m_transform = transform;
    setStageMode(m_stageMode);
#else
    UNUSED_PARAM(transform);
#endif
}

void RemoteMeshProxy::setEntityTransformInternal(const WebModel::Float4x4& transform)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::UpdateTransform(transform));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(transform);
#endif
}

void RemoteMeshProxy::play(bool playing)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::Play(playing));
    UNUSED_PARAM(sendResult);
#endif
}

void RemoteMeshProxy::setEnvironmentMap(const WebModel::ImageAsset& imageAsset)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::SetEnvironmentMap(imageAsset));
    UNUSED_PARAM(sendResult);
#endif
}

#if PLATFORM(COCOA)
std::optional<WebModel::Float4x4> RemoteMeshProxy::entityTransform() const
{
    return m_transform;
}
#endif

void RemoteMeshProxy::setCameraDistance(float distance)
{
#if ENABLE(GPU_PROCESS_MODEL)
    if (areSameSignAndAlmostEqual(distance, m_cameraDistance))
        return;

    auto sendResult = send(Messages::RemoteMesh::SetCameraDistance(distance));
    UNUSED_PARAM(sendResult);
    m_cameraDistance = distance;
#else
    UNUSED_PARAM(distance);
#endif
}

bool RemoteMeshProxy::supportsTransform(const WebCore::TransformationMatrix& transformationMatrix) const
{
#if ENABLE(GPU_PROCESS_MODEL)
    const WebModel::Float4x4 matrix = static_cast<simd_float4x4>(transformationMatrix);

    WebModel::Float3x3 upperLeft;
    upperLeft.column0 = simd_make_float3(matrix.column0);
    upperLeft.column1 = simd_make_float3(matrix.column1);
    upperLeft.column2 = simd_make_float3(matrix.column2);

    simd_float3 scale = simd_make_float3(simd_length(upperLeft.column0), simd_length(upperLeft.column1), simd_length(upperLeft.column2));

    if (!areSameSignAndAlmostEqual(simd_reduce_max(scale), simd_reduce_min(scale)))
        return false;

    WebModel::Float3x3 rotation;
    rotation.column0 = upperLeft.column0 / scale[0];
    rotation.column1 = upperLeft.column1 / scale[1];
    rotation.column2 = upperLeft.column2 / scale[2];

    simd_float3 translation = simd_make_float3(matrix.column3);
    WebModel::Float4x4 noShearMatrix = makeTransformMatrix(translation, scale, rotation);
    if (!simd_almost_equal_elements(matrix, noShearMatrix, tolerance))
        return false;

    return true;
#else
    UNUSED_PARAM(transformationMatrix);
    return false;
#endif
}

void RemoteMeshProxy::setScale(float scale)
{
#if ENABLE(GPU_PROCESS_MODEL)
    if (!m_transform)
        m_transform = matrix_identity_float4x4;
    WebModel::Float4x4 transform = *m_transform;
    transform.column0 = simd_normalize(transform.column0) * scale;
    transform.column1 = simd_normalize(transform.column1) * scale;
    transform.column2 = simd_normalize(transform.column2) * scale;

    setEntityTransform(transform);
#else
    UNUSED_PARAM(scale);
#endif
}

void RemoteMeshProxy::setStageMode(WebCore::StageModeOperation stageMode)
{
#if ENABLE(GPU_PROCESS_MODEL)
    m_stageMode = stageMode;
    auto [center, extents] = getCenterAndExtents();
    if (stageMode == WebCore::StageModeOperation::None) {
        setEntityTransformInternal(buildTranslation(-center.x, -center.y, -center.z - .5f * extents.z));
        return;
    }

    WebModel::Float4x4 result = matrix_identity_float4x4;
    if (auto existingTransform = entityTransform())
        result = *existingTransform;

    float maxExtent = simd_reduce_max(extents.xyz);
    float scale = m_cameraDistance / maxExtent;

    result.column0 = scale * simd_normalize(result.column0);
    result.column1 = scale * simd_normalize(result.column1);
    result.column2 = scale * simd_normalize(result.column2);
    result.column3 = simd_make_float4(
        -simd_dot(center.xyz, simd_make_float3(result.column0.x, result.column1.x, result.column2.x)),
        -simd_dot(center.xyz, simd_make_float3(result.column0.y, result.column1.y, result.column2.y)),
        -simd_dot(center.xyz, simd_make_float3(result.column0.z, result.column1.z, result.column2.z)),
        1.f);

    setEntityTransformInternal(result);
#else
    UNUSED_PARAM(stageMode);
#endif
}

#if ENABLE(GPU_PROCESS_MODEL)
static simd_float4x4 buildRotation(float azimuth, float elevation)
{
    float cosAz = std::cos(azimuth);
    float sinAz = std::sin(azimuth);
    float cosEl = std::cos(elevation);
    float sinEl = std::sin(elevation);

    simd_float4x4 matrix;
    matrix.columns[0] = simd_make_float4(cosAz,     sinAz * sinEl,  sinAz * cosEl, 0.0f);
    matrix.columns[1] = simd_make_float4(0.0f,      cosEl,         -sinEl,         0.0f);
    matrix.columns[2] = simd_make_float4(-sinAz,    cosAz * sinEl,  cosAz * cosEl, 0.0f);
    matrix.columns[3] = simd_make_float4(0.0f,      0.0f,           0.0f,          1.0f);

    return matrix;
}

void RemoteMeshProxy::setRotation(float yaw, float pitch, float roll)
{
    UNUSED_PARAM(roll);
    m_transform = buildRotation(yaw, pitch);
    setStageMode(m_stageMode);
}
#endif

}

#endif // ENABLE(GPU_PROCESS)
