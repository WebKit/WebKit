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
#include "RemoteDDMeshProxy.h"

#if ENABLE(GPU_PROCESS)

#include "ModelConvertToBackingContext.h"
#include "RemoteDDMeshMessages.h"
#include <WebCore/DDFloat4x4.h>
#include <WebCore/DDMeshDescriptor.h>
#include <WebCore/DDMeshPart.h>
#include <WebCore/DDUpdateMeshDescriptor.h>
#include <WebCore/DDUpdateTextureDescriptor.h>
#include <WebCore/StageModeOperations.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::DDModel {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteDDMeshProxy);

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

static WebCore::DDModel::DDFloat4x4 makeTransformMatrix(
    const simd_float3& translation,
    const simd_float3& scale,
    const WebCore::DDModel::DDFloat3x3& rotation)
{
    WebCore::DDModel::DDFloat4x4 result;
    result.column0 = simd_make_float4(rotation.column0 * scale[0], 0.f);
    result.column1 = simd_make_float4(rotation.column1 * scale[1], 0.f);
    result.column2 = simd_make_float4(rotation.column2 * scale[2], 0.f);
    result.column3 = simd_make_float4(translation, 1.f);

    return result;
}

static std::pair<simd_float4, simd_float4> computeMinAndMaxCorners(const Vector<WebCore::DDModel::DDMeshPart>& parts, const Vector<WebCore::DDModel::DDFloat4x4>& instanceTransforms)
{
    simd_float3 minCorner = simd_make_float3(FLT_MAX, FLT_MAX, FLT_MAX);
    simd_float3 maxCorner = simd_make_float3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const WebCore::DDModel::DDMeshPart& part : parts) {
        minCorner = simd_min(part.boundsMin, minCorner);
        maxCorner = simd_max(part.boundsMax, maxCorner);
    }

    if (!instanceTransforms.size())
        return std::make_pair(simd_make_float4(minCorner), simd_make_float4(maxCorner));

    simd_float3 simdCenter = .5f * (minCorner + maxCorner);
    simd_float3 simdExtent = 2.f * (maxCorner - simdCenter);

    simd_float4 simdCenter4 = simd::make_float4(simdCenter.x, simdCenter.y, simdCenter.z, 1.f);
    simd_float4 simdExtent4 = simd::make_float4(simdExtent.x, simdExtent.y, simdExtent.z, 0.f);

    simd_float4 minCorner4 = simd::make_float4(FLT_MAX, FLT_MAX, FLT_MAX, 1.f);
    simd_float4 maxCorner4 = simd::make_float4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.f);

    for (auto& transform : instanceTransforms) {
        simd_float4 transformedCenter = simd_mul(transform, simdCenter4);
        simd_float4 transformedExtent = simd_mul(transform, simdExtent4);

        minCorner4 = simd_min(transformedCenter - transformedExtent, minCorner4);
        maxCorner4 = simd_max(transformedCenter + transformedExtent, maxCorner4);
    }

    return std::make_pair(minCorner4, maxCorner4);
}
#endif

RemoteDDMeshProxy::RemoteDDMeshProxy(Ref<RemoteGPUProxy>&& root, ConvertToBackingContext& convertToBackingContext, DDModelIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_root(WTFMove(root))
#if PLATFORM(COCOA)
    , m_minCorner(simd_make_float4(FLT_MAX, FLT_MAX, FLT_MAX, 1.f))
    , m_maxCorner(simd_make_float4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.f))
#endif
{
}

RemoteDDMeshProxy::~RemoteDDMeshProxy()
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::Destruct());
    UNUSED_VARIABLE(sendResult);
#endif
}

#if ENABLE(GPU_PROCESS_MODEL)
static WebCore::DDModel::DDFloat4x4 buildTranslation(float x, float y, float z)
{
    WebCore::DDModel::DDFloat4x4 result = matrix_identity_float4x4;
    result.column3 = simd_make_float4(x, y, z, 1.f);
    return result;
}
#endif

void RemoteDDMeshProxy::update(const WebCore::DDModel::DDUpdateMeshDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto [minCorner, maxCorner] = computeMinAndMaxCorners(descriptor.parts, descriptor.instanceTransforms);
    m_minCorner = simd_min(minCorner, m_minCorner);
    m_maxCorner = simd_max(maxCorner, m_maxCorner);

    auto [center, extents] = getCenterAndExtents();
    setCameraDistance(std::max(extents.x, extents.y) * .5f);

    auto sendResult = send(Messages::RemoteDDMesh::Update(descriptor));
    UNUSED_PARAM(sendResult);
    setEntityTransform(buildTranslation(-center.x, -center.y, -center.z));
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteDDMeshProxy::render()
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::Render());
    UNUSED_PARAM(sendResult);
#endif
}

void RemoteDDMeshProxy::setLabelInternal(const String& label)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
#else
    UNUSED_PARAM(label);
#endif
}

void RemoteDDMeshProxy::updateTexture(const WebCore::DDModel::DDUpdateTextureDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::UpdateTexture(descriptor));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteDDMeshProxy::updateMaterial(const WebCore::DDModel::DDUpdateMaterialDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::UpdateMaterial(descriptor));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

#if PLATFORM(COCOA)
std::pair<simd_float4, simd_float4> RemoteDDMeshProxy::getCenterAndExtents() const
{
    auto center = .5f * (m_minCorner + m_maxCorner);
    auto extents = 2.f * (m_maxCorner - center);
    return std::make_pair(center, extents);
}
#endif

void RemoteDDMeshProxy::setEntityTransform(const WebCore::DDModel::DDFloat4x4& transform)
{
    m_transform = transform;
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::UpdateTransform(transform));
    UNUSED_PARAM(sendResult);
#endif
}

void RemoteDDMeshProxy::play(bool playing)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::Play(playing));
    UNUSED_PARAM(sendResult);
#endif
}

std::optional<WebCore::DDModel::DDFloat4x4> RemoteDDMeshProxy::entityTransform() const
{
    return m_transform;
}

void RemoteDDMeshProxy::setCameraDistance(float distance)
{
#if ENABLE(GPU_PROCESS_MODEL)
    if (areSameSignAndAlmostEqual(distance, m_cameraDistance))
        return;

    auto sendResult = send(Messages::RemoteDDMesh::SetCameraDistance(distance));
    UNUSED_PARAM(sendResult);
    m_cameraDistance = distance;
#else
    UNUSED_PARAM(distance);
#endif
}

bool RemoteDDMeshProxy::supportsTransform(const WebCore::TransformationMatrix& transformationMatrix) const
{
#if ENABLE(GPU_PROCESS_MODEL)
    const WebCore::DDModel::DDFloat4x4 matrix = static_cast<simd_float4x4>(transformationMatrix);

    WebCore::DDModel::DDFloat3x3 upperLeft;
    upperLeft.column0 = simd_make_float3(matrix.column0);
    upperLeft.column1 = simd_make_float3(matrix.column1);
    upperLeft.column2 = simd_make_float3(matrix.column2);

    simd_float3 scale = simd_make_float3(simd_length(upperLeft.column0), simd_length(upperLeft.column1), simd_length(upperLeft.column2));

    if (!areSameSignAndAlmostEqual(simd_reduce_max(scale), simd_reduce_min(scale)))
        return false;

    WebCore::DDModel::DDFloat3x3 rotation;
    rotation.column0 = upperLeft.column0 / scale[0];
    rotation.column1 = upperLeft.column1 / scale[1];
    rotation.column2 = upperLeft.column2 / scale[2];

    simd_float3 translation = simd_make_float3(matrix.column3);
    WebCore::DDModel::DDFloat4x4 noShearMatrix = makeTransformMatrix(translation, scale, rotation);
    if (!simd_almost_equal_elements(matrix, noShearMatrix, tolerance))
        return false;

    return true;
#else
    UNUSED_PARAM(transformationMatrix);
    return false;
#endif
}

void RemoteDDMeshProxy::setScale(float scale)
{
    if (!m_transform)
        return;
#if ENABLE(GPU_PROCESS_MODEL)
    WebCore::DDModel::DDFloat4x4 transform = *m_transform;
    transform.column0 = simd_normalize(transform.column0) * scale;
    transform.column1 = simd_normalize(transform.column1) * scale;
    transform.column2 = simd_normalize(transform.column2) * scale;

    if (!simd_almost_equal_elements(transform, *m_transform, tolerance))
        setEntityTransform(transform);
#else
    UNUSED_PARAM(scale);
#endif
}

void RemoteDDMeshProxy::setStageMode(WebCore::StageModeOperation stageMode)
{
#if ENABLE(GPU_PROCESS_MODEL)
    if (stageMode == WebCore::StageModeOperation::None || !m_transform)
        return;

    m_stageMode = stageMode;
    auto [center, extents] = getCenterAndExtents();
    WebCore::DDModel::DDFloat4x4 result = matrix_identity_float4x4;
    if (auto existingTransform = entityTransform())
        result = *existingTransform;

    float scale = m_cameraDistance / (simd_length(extents) * .5f);
    result.column0 = scale * simd_normalize(result.column0);
    result.column1 = scale * simd_normalize(result.column1);
    result.column2 = scale * simd_normalize(result.column2);
    result.column3 = simd_make_float4(
        -simd_dot(center.xyz, simd_make_float3(result.column0.x, result.column1.x, result.column2.x)),
        -simd_dot(center.xyz, simd_make_float3(result.column0.y, result.column1.y, result.column2.y)),
        -simd_dot(center.xyz, simd_make_float3(result.column0.z, result.column1.z, result.column2.z)),
        1.f);

    setEntityTransform(result);
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

void RemoteDDMeshProxy::setRotation(float yaw, float pitch, float roll)
{
    if (!m_transform)
        return;

    UNUSED_PARAM(roll);
    m_transform = buildRotation(yaw, pitch);
    setStageMode(m_stageMode);
}
#endif

}

#endif // ENABLE(GPU_PROCESS)
