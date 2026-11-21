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
#include <WebCore/CSSUnits.h>
#include <WebCore/StageModeOperations.h>
#include <WebCore/TransformationMatrix.h>
#include <array>
#include <wtf/HexNumber.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

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

static std::pair<simd_float4, simd_float4> computeMinAndMaxCorners(const Vector<WebModel::MeshPart>& parts, const Vector<WebModel::Float4x4>& instanceTransforms, const std::optional<WebModel::DeformationData>& deformationData)
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

        Vector<simd_float4x4> rootSkinMatrices;
        if (deformationData && deformationData->skinningData && !deformationData->skinningData->jointTransforms.isEmpty()) {
            const auto& skinning = *deformationData->skinningData;
            const simd_float4x4 geomBind = skinning.geometryBindTransform;
            const auto& rootIndices = skinning.rootJointIndices;
            const bool hasRootIndices = !rootIndices.isEmpty();
            const size_t iterCount = hasRootIndices ? rootIndices.size() : 1;
            rootSkinMatrices.reserveInitialCapacity(iterCount);
            for (size_t r = 0; r < iterCount; ++r) {
                uint32_t rootIdx = hasRootIndices ? rootIndices[r] : 0;
                if (rootIdx >= skinning.jointTransforms.size())
                    continue;
                const simd_float4x4 invBind = (rootIdx < skinning.inverseBindPoses.size())
                    ? static_cast<simd_float4x4>(skinning.inverseBindPoses[rootIdx])
                    : matrix_identity_float4x4;
                rootSkinMatrices.append(simd_mul(simd_mul(skinning.jointTransforms[rootIdx], invBind), geomBind));
            }
        } else
            rootSkinMatrices.append(matrix_identity_float4x4);

        for (auto& transform : instanceTransforms) {
            for (const auto& skinMatrix : rootSkinMatrices) {
                const simd_float4x4 worldTransform = simd_mul(transform, skinMatrix);
                for (int j = 0; j < 8; ++j) {
                    simd_float4 corner4 = simd_make_float4(corners[j].x, corners[j].y, corners[j].z, 1.f);
                    simd_float4 transformedCorner = simd_mul(worldTransform, corner4);
                    minCorner4 = simd_min(transformedCorner, minCorner4);
                    maxCorner4 = simd_max(transformedCorner, maxCorner4);
                }
            }
        }
    }

    return std::make_pair(minCorner4, maxCorner4);
}

static constexpr float kCSSPixelsPerMeter = WebCore::CSS::pixelsPerCm * 100.f;

#if ENABLE(SPATIAL_PORTAL)
enum class TranslationUnits : bool { Rendering, Reporting };

static simd_float4x4 applyContentTransform(const WebCore::UsedPortalTransform& portalTransform, const simd_float4x4& fittedTransform, TranslationUnits translationUnits)
{
    if (!portalTransform.hasContentTransform())
        return fittedTransform;

    auto beforeAuto = static_cast<simd_float4x4>(portalTransform.transformBeforeAuto);
    auto afterAuto = static_cast<simd_float4x4>(portalTransform.transformAfterAuto);

    simd_float4x4 enclosing = simd_mul(fittedTransform, beforeAuto);
    simd_float3x3 enclosingLinear = simd_matrix(simd_make_float3(enclosing.columns[0]), simd_make_float3(enclosing.columns[1]), simd_make_float3(enclosing.columns[2]));

    float cssPixelsPerUnit = translationUnits == TranslationUnits::Rendering ? kCSSPixelsPerMeter : 1;
    simd_float3 translation = simd_mul(simd_inverse(enclosingLinear), simd_make_float3(afterAuto.columns[3]) / cssPixelsPerUnit);
    if (std::isfinite(translation.x) && std::isfinite(translation.y) && std::isfinite(translation.z))
        afterAuto.columns[3] = simd_make_float4(translation, afterAuto.columns[3].w);

    return simd_mul(fittedTransform, simd_mul(beforeAuto, afterAuto));
}

static void transformBoundingBox(simd_float3 extents, simd_float3 center, const simd_float4x4& matrix, simd_float3& minimum, simd_float3& maximum)
{
    simd_float3 halfExtents = extents / 2;

    for (int corner = 0; corner < 8; ++corner) {
        simd_float3 offset = simd_make_float3(
            (corner & 1) ? halfExtents.x : -halfExtents.x,
            (corner & 2) ? halfExtents.y : -halfExtents.y,
            (corner & 4) ? halfExtents.z : -halfExtents.z
        );
        simd_float3 point = simd_make_float3(simd_mul(matrix, simd_make_float4(center + offset, 1.0f)));

        minimum = corner ? simd_min(minimum, point) : point;
        maximum = corner ? simd_max(maximum, point) : point;
    }
}

static constexpr auto portalChildQuadGraphName = WKBridgePortalChildQuadGraphName ""_s;

enum class PortalChildQuadResource : uint8_t {
    Mesh = 1,
    Material = 2,
    Texture = 3,
};

static WebModel::TypedResourceId portalChildQuadResourceID(WebCore::NodeIdentifier nodeID, PortalChildQuadResource resource)
{
    uint64_t raw = nodeID.toUInt64();
    auto resourceValue = static_cast<uint64_t>(resource);

    return WebModel::TypedResourceId {
        makeString("50524C51-"_s, hex(resourceValue, 4), "-4000-"_s, hex(raw >> 48, 4), '-', hex(raw & 0xFFFFFFFFFFFF, 12)),
        makeString('/', portalChildQuadGraphName, '/', raw, '/', resourceValue),
        static_cast<int64_t>(0x50524C5100000000ULL ^ (raw * 4 + resourceValue))
    };
}

static Vector<uint8_t> portalChildQuadVertexData()
{
    static constexpr std::array<float, 32> vertices {
        -0.5f, -0.5f, 0.f, 0.f, 0.f, 1.f, 0.f, 1.f,
        0.5f, -0.5f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f,
        0.5f, 0.5f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,
        -0.5f, 0.5f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
    };
    return Vector<uint8_t> { asByteSpan(vertices) };
}

static Vector<uint8_t> portalChildQuadIndexData()
{
    static constexpr std::array<uint16_t, 12> indices { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 };
    return Vector<uint8_t> { asByteSpan(indices) };
}

static WebModel::MeshDescriptor portalChildQuadMeshDescriptor()
{
    return WebModel::MeshDescriptor {
        .vertexBufferCount = 1,
        .vertexCapacity = 4,
        .vertexAttributes = {
            WebModel::VertexAttributeFormat { WebModel::VertexSemantic::Position, ::WebGPU::VertexFormat::Float32x3, 0, 0 },
            WebModel::VertexAttributeFormat { WebModel::VertexSemantic::Normal, ::WebGPU::VertexFormat::Float32x3, 0, 12 },
            WebModel::VertexAttributeFormat { WebModel::VertexSemantic::UV0, ::WebGPU::VertexFormat::Float32x2, 0, 24 },
        },
        .vertexLayouts = {
            WebModel::VertexLayout { 0, 0, 32, WebModel::VertexStepFunction::PerVertex, 1 },
        },
        .indexCapacity = 12,
        .indexType = WebModel::IndexType::UInt16,
    };
}

static WebModel::UpdateMeshDescriptor portalChildQuadMeshUpdate(WebCore::NodeIdentifier nodeID)
{
    return WebModel::UpdateMeshDescriptor {
        .identifier = portalChildQuadResourceID(nodeID, PortalChildQuadResource::Mesh),
        .updateType = 0,
        .descriptor = portalChildQuadMeshDescriptor(),
        .parts = {
            WebModel::MeshPart {
                .indexOffset = 0,
                .indexCount = 12,
                .topology = ::WebGPU::PrimitiveTopology::TriangleList,
                .materialIndex = 0,
                .boundsMin = WebModel::Float3(-0.5f, -0.5f, -0.01f, 0.f),
                .boundsMax = WebModel::Float3(0.5f, 0.5f, 0.01f, 0.f),
            },
        },
        .indexData = portalChildQuadIndexData(),
        .vertexData = { portalChildQuadVertexData() },
        .transform = matrix_identity_float4x4,
        .instanceTransforms = { matrix_identity_float4x4 },
        .assignedMaterials = { portalChildQuadResourceID(nodeID, PortalChildQuadResource::Material) },
        .deformationData = std::nullopt,
    };
}
#endif
#endif

RemoteMeshProxy::RemoteMeshProxy(Ref<RemoteGPUProxy>&& root, ModelConvertToBackingContext& convertToBackingContext, WebModelIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_root(WTF::move(root))
{
}

RemoteMeshProxy::~RemoteMeshProxy()
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::Destruct());
    UNUSED_VARIABLE(sendResult);
#endif
}

void RemoteMeshProxy::updateModel(WebCore::NodeIdentifier nodeID, Vector<WebModel::UpdateMeshDescriptor>&& descriptorArray)
{
#if ENABLE(GPU_PROCESS_MODEL)
    bool needBoundingBoxUpdate = false;

    auto& bounds = m_modelBounds.add(nodeID, ModelBounds { }).iterator->value;
    if (bounds.boundsAreEmpty()) {
        for (auto& descriptor : descriptorArray) {
            auto [minCorner, maxCorner] = computeMinAndMaxCorners(descriptor.parts, descriptor.instanceTransforms, descriptor.deformationData);
            bounds.minCorner = simd_min(bounds.minCorner, minCorner);
            bounds.maxCorner = simd_max(bounds.maxCorner, maxCorner);
        }
        needBoundingBoxUpdate = !bounds.boundsAreEmpty();
    }

    auto sendResult = send(Messages::RemoteMesh::UpdateModel(nodeID, WTF::move(descriptorArray)));
    UNUSED_VARIABLE(sendResult);

    if (needBoundingBoxUpdate)
        computeTransform();
#else
    UNUSED_PARAM(nodeID);
    UNUSED_PARAM(descriptorArray);
#endif
}

void RemoteMeshProxy::render(uint32_t textureIndex, Function<void(bool)>&& completionHandler)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = sendWithAsyncReply(Messages::RemoteMesh::Render(textureIndex), [completionHandler = WTF::move(completionHandler)](bool result) mutable {
        completionHandler(result);
    });
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

void RemoteMeshProxy::updateTexture(Vector<WebModel::UpdateTextureDescriptor>&& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = sendWithAsyncReply(Messages::RemoteMesh::UpdateTexture(WTF::move(descriptor)), [](auto) mutable {
    });
    UNUSED_VARIABLE(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteMeshProxy::updateMaterial(Vector<WebModel::UpdateMaterialDescriptor>&& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = sendWithAsyncReply(Messages::RemoteMesh::UpdateMaterial(WTF::move(descriptor)), [](auto) mutable {
    });
    UNUSED_VARIABLE(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

#if PLATFORM(COCOA)
static std::pair<simd_float4, simd_float4> makeCenterAndExtents(simd_float4 minCorner, simd_float4 maxCorner)
{
    auto center = .5f * (minCorner + maxCorner);
    auto extents = maxCorner - minCorner;
    return std::make_pair(center, extents);
}

std::pair<simd_float4, simd_float4> RemoteMeshProxy::mergedCenterAndExtents() const
{
    ModelBounds merged;
    for (auto& bounds : m_modelBounds.values()) {
        merged.minCorner = simd_min(merged.minCorner, bounds.minCorner);
        merged.maxCorner = simd_max(merged.maxCorner, bounds.maxCorner);
    }

    if (merged.boundsAreEmpty())
        return { simd_make_float4(0.f), simd_make_float4(0.f) };

    return makeCenterAndExtents(merged.minCorner, merged.maxCorner);
}

std::optional<std::pair<simd_float4, simd_float4>> RemoteMeshProxy::centerAndExtents(WebCore::NodeIdentifier nodeID) const
{
    auto it = m_modelBounds.find(nodeID);
    if (it == m_modelBounds.end() || it->value.boundsAreEmpty())
        return std::nullopt;

    return makeCenterAndExtents(it->value.minCorner, it->value.maxCorner);
}

void RemoteMeshProxy::removeModel(WebCore::NodeIdentifier nodeID)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::RemoveModel(nodeID));
    UNUSED_VARIABLE(sendResult);
#endif

    if (!m_modelBounds.remove(nodeID))
        return;

#if ENABLE(GPU_PROCESS_MODEL)
    computeTransform();
#endif
}
#endif

void RemoteMeshProxy::setEntityTransform(const WebModel::Float4x4& transform)
{
#if ENABLE(GPU_PROCESS_MODEL)
    m_entityTransformSetByScript = true;
    m_transform = transform;
    m_computedTransform = transform;
    setEntityTransformInternal(transform);
#else
    UNUSED_PARAM(transform);
#endif
}

void RemoteMeshProxy::setEntityTransformInternal(const WebModel::Float4x4& transform)
{
#if ENABLE(GPU_PROCESS_MODEL)
#if ENABLE(SPATIAL_PORTAL)
    m_sentEntityTransform = transform;
#endif
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

void RemoteMeshProxy::setEnvironmentMap(WebModel::UpdateTextureDescriptor&& imageAsset)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::SetEnvironmentMap(WTF::move(imageAsset)));
    UNUSED_PARAM(sendResult);
#endif
}

#if ENABLE(SPATIAL_PORTAL)
void RemoteMeshProxy::setDefaultEnvironmentMap()
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::SetDefaultEnvironmentMap());
    UNUSED_PARAM(sendResult);
#endif
}

void RemoteMeshProxy::removeEnvironmentMap()
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::RemoveEnvironmentMap());
    UNUSED_PARAM(sendResult);
#endif
}
#endif

void RemoteMeshProxy::updateContentsHeadroom(float headroom)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::UpdateContentsHeadroom(headroom));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(headroom);
#endif
}

#if PLATFORM(COCOA)
void RemoteMeshProxy::sizeDidChange(unsigned width, unsigned height, CompletionHandler<void(Vector<MachSendRight>&&)>&& callback)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = sendWithAsyncReply(Messages::RemoteMesh::UpdateRenderBuffers(width, height), WTF::move(callback));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    callback({ });
#endif
}

void RemoteMeshProxy::paintCurrentFrameToImageBuffer(WebCore::RenderingResourceIdentifier imageBufferIdentifier, uint32_t bufferIndex)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult { sendSync(Messages::RemoteMesh::PaintCurrentFrameToImageBuffer(imageBufferIdentifier, bufferIndex)) };
    UNUSED_VARIABLE(sendResult);
#else
    UNUSED_PARAM(imageBufferIdentifier);
    UNUSED_PARAM(bufferIndex);
#endif
}

std::optional<WebModel::Float4x4> RemoteMeshProxy::entityTransform() const
{
    return m_computedTransform;
}
#endif

void RemoteMeshProxy::setFOV(float fovY)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteMesh::SetFOV(fovY));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(fovY);
#endif
}

bool RemoteMeshProxy::supportsTransform(const WebCore::TransformationMatrix& transformationMatrix)
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

void RemoteMeshProxy::setViewportSize(float width, float height)
{
#if ENABLE(GPU_PROCESS_MODEL)
    m_viewportWidth = width;
    m_viewportHeight = height;
    computeTransform();
#endif
}

void RemoteMeshProxy::setStageMode(WebCore::StageModeOperation stageMode)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto previousStageMode = m_stageMode;
    m_stageMode = stageMode;
    if (m_stageMode == WebCore::StageModeOperation::Orbit)
        m_entityTransformSetByScript = false;
    bool returningToNoneFromOrbit = m_stageMode == WebCore::StageModeOperation::None && previousStageMode != WebCore::StageModeOperation::None;

#if ENABLE(SPATIAL_PORTAL)
    if (m_portalTransform && returningToNoneFromOrbit) {
        m_transform = std::nullopt;
        computeTransform();
        return;
    }
#endif

    if (!returningToNoneFromOrbit)
        computeTransform();
#else
    UNUSED_PARAM(stageMode);
#endif
}

void RemoteMeshProxy::processRemovals(Vector<WebModel::TypedResourceId>&& meshRemovals, Vector<WebModel::TypedResourceId>&& materialRemovals, Vector<WebModel::TypedResourceId>&& textureRemovals, CompletionHandler<void(bool)>&& completion)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = sendWithAsyncReply(Messages::RemoteMesh::ProcessRemovals(WTF::move(meshRemovals), WTF::move(materialRemovals), WTF::move(textureRemovals)), [completion = WTF::move(completion)](bool success) mutable {
        completion(success);
    });
    UNUSED_VARIABLE(sendResult);
#else
    UNUSED_PARAM(meshRemovals);
    UNUSED_PARAM(materialRemovals);
    UNUSED_PARAM(textureRemovals);
    completion(false);
#endif
}

#if ENABLE(GPU_PROCESS_MODEL)
void RemoteMeshProxy::computeTransform()
{
    if (m_entityTransformSetByScript)
        return;

    // Fixed camera distance matching the ModelRenderer
    static constexpr float kCameraDistance = 0.5;

#if ENABLE(SPATIAL_PORTAL)
    auto [center, extents] = m_portalTransform ? portalContentCenterAndExtents(static_cast<simd_float4x4>(m_portalTransform->transformBeforeAuto)) : mergedCenterAndExtents();
    bool hasModelGeometry = simd_length(simd_make_float3(extents.x, extents.y, extents.z)) > FLT_EPSILON;
    bool fitsContent = !m_portalTransform || (m_portalTransform->fitsContent && hasModelGeometry);
#else
    auto [center, extents] = mergedCenterAndExtents();
    constexpr bool fitsContent = true;
#endif

    float viewportWidth = m_viewportWidth / kCSSPixelsPerMeter;
    float viewportHeight = m_viewportHeight / kCSSPixelsPerMeter;

    float scale = 0;
    float depth = 0;

    if (!fitsContent) {
        scale = 1.f / kCSSPixelsPerMeter;
        depth = extents.z;
    } else if (m_stageMode == WebCore::StageModeOperation::None) {
        if (std::fmin(extents.x, extents.y) > FLT_EPSILON)
            scale = std::fmin(viewportWidth / extents.x, viewportHeight / extents.y);
        depth = extents.z;
    } else {
        float boundingDiameter = simd_length(simd_make_float3(extents.x, extents.y, extents.z));
        if (boundingDiameter > FLT_EPSILON)
            scale = std::fmin(viewportWidth, viewportHeight) / boundingDiameter;
        depth = boundingDiameter;
    }

    WebModel::Float4x4 result = matrix_identity_float4x4;
    if (auto existingTransform = m_transform)
        result = *existingTransform;

    result.column0 = scale * simd_normalize(result.column0);
    result.column1 = scale * simd_normalize(result.column1);
    result.column2 = scale * simd_normalize(result.column2);
    result.column3 = simd_make_float4(
        -simd_dot(center.xyz, simd_make_float3(result.column0.x, result.column1.x, result.column2.x)),
        -simd_dot(center.xyz, simd_make_float3(result.column0.y, result.column1.y, result.column2.y)),
        -simd_dot(center.xyz, simd_make_float3(result.column0.z, result.column1.z, result.column2.z)) - scale * depth / 2,
        1.f);

    setFOV(2 * std::atan(viewportHeight / (2 * kCameraDistance)));

    m_computedTransform = result;

#if ENABLE(SPATIAL_PORTAL)
    if (m_portalTransform) {
        m_fittedPortalTransform = result;
        result = applyContentTransform(*m_portalTransform, result, TranslationUnits::Rendering);
    }
#endif

    setEntityTransformInternal(result);

#if ENABLE(SPATIAL_PORTAL)
    updatePortalChildQuads();
#endif
}
#endif

#if ENABLE(SPATIAL_PORTAL) && PLATFORM(COCOA)
void RemoteMeshProxy::setPortalTransform(const WebCore::UsedPortalTransform& portalTransform)
{
#if ENABLE(GPU_PROCESS_MODEL)
    if (m_portalTransform == portalTransform)
        return;

    m_portalTransform = portalTransform;
    m_entityTransformSetByScript = false;
    computeTransform();
#else
    UNUSED_PARAM(portalTransform);
#endif
}

void RemoteMeshProxy::setPortalChildTransform(WebCore::NodeIdentifier nodeID, const WebModel::Float4x4& transform)
{
#if ENABLE(GPU_PROCESS_MODEL)
    simd_float4x4 matrix = transform;
    matrix.columns[3] = simd_make_float4(simd_make_float3(matrix.columns[3]) / kCSSPixelsPerMeter, 1.f);

    auto sendResult = send(Messages::RemoteMesh::UpdatePortalChildTransform(nodeID, matrix));
    UNUSED_VARIABLE(sendResult);
#else
    UNUSED_PARAM(nodeID);
    UNUSED_PARAM(transform);
#endif
}

void RemoteMeshProxy::setPortalChildQuads(Vector<PortalChildQuad>&& quads)
{
#if ENABLE(GPU_PROCESS_MODEL)
    m_portalChildQuads = WTF::move(quads);
    updatePortalChildQuads();
#else
    UNUSED_PARAM(quads);
#endif
}

#if ENABLE(GPU_PROCESS_MODEL)
std::optional<WebModel::Float4x4> RemoteMeshProxy::portalChildQuadTransform(const PortalChildQuad& quad) const
{
    if (!m_portalTransform || !m_sentEntityTransform || quad.quadRectInChildSpace.isEmpty() || m_viewportWidth <= 0 || m_viewportHeight <= 0)
        return std::nullopt;

    simd_float4x4 entityTransform = *m_sentEntityTransform;
    float determinant = simd_determinant(entityTransform);
    if (!std::isfinite(determinant) || std::abs(determinant) <= FLT_EPSILON * FLT_EPSILON * FLT_EPSILON)
        return std::nullopt;

    auto quadCenter = quad.quadRectInChildSpace.center();
    simd_float4x4 quadToChild = simd_matrix(
        simd_make_float4(quad.quadRectInChildSpace.width(), 0.f, 0.f, 0.f),
        simd_make_float4(0.f, -quad.quadRectInChildSpace.height(), 0.f, 0.f),
        simd_make_float4(0.f, 0.f, 1.f, 0.f),
        simd_make_float4(quadCenter.x(), quadCenter.y(), 0.f, 1.f));

    simd_float4x4 childTransform = static_cast<simd_float4x4>(quad.transform);

    simd_float4x4 childToPortalContent = matrix_identity_float4x4;
    childToPortalContent.columns[3] = simd_make_float4(quad.childOriginInPortal.x(), quad.childOriginInPortal.y(), 0.f, 1.f);

    constexpr float metersPerPixel = 1.f / kCSSPixelsPerMeter;
    simd_float4x4 portalContentToWorld = simd_matrix(
        simd_make_float4(metersPerPixel, 0.f, 0.f, 0.f),
        simd_make_float4(0.f, -metersPerPixel, 0.f, 0.f),
        simd_make_float4(0.f, 0.f, metersPerPixel, 0.f),
        simd_make_float4(-m_viewportWidth * metersPerPixel / 2.f, m_viewportHeight * metersPerPixel / 2.f, 0.f, 1.f));

    simd_float4x4 orbitRotation = matrix_identity_float4x4;
    if (m_transform) {
        simd_float4x4 orbit = *m_transform;
        orbitRotation = simd_matrix(
            simd_make_float4(simd_normalize(simd_make_float3(orbit.columns[0])), 0.f),
            simd_make_float4(simd_normalize(simd_make_float3(orbit.columns[1])), 0.f),
            simd_make_float4(simd_normalize(simd_make_float3(orbit.columns[2])), 0.f),
            simd_make_float4(0.f, 0.f, 0.f, 1.f));
    }

    simd_float4x4 afterAuto = static_cast<simd_float4x4>(m_portalTransform->transformAfterAuto);
    afterAuto.columns[3] = simd_make_float4(simd_make_float3(afterAuto.columns[3]) * metersPerPixel, afterAuto.columns[3].w);

    simd_float4x4 boxSpace = simd_mul(orbitRotation, afterAuto);
    simd_float4x4 quadToWorld = simd_mul(boxSpace, simd_mul(portalContentToWorld, simd_mul(childToPortalContent, simd_mul(childTransform, quadToChild))));

    return WebModel::Float4x4 { simd_mul(simd_inverse(entityTransform), quadToWorld) };
}

void RemoteMeshProxy::updatePortalChildQuads()
{
    Vector<WebModel::PortalContentSource> sources;
    Vector<WebModel::UpdateMaterialDescriptor> newMaterials;
    Vector<std::pair<WebCore::NodeIdentifier, WebModel::UpdateMeshDescriptor>> newMeshes;
    Vector<std::pair<WebCore::NodeIdentifier, WebModel::Float4x4>> changedTransforms;
    HashMap<WebCore::NodeIdentifier, WebModel::Float4x4> transforms;

    for (auto& quad : m_portalChildQuads) {
        auto transform = portalChildQuadTransform(quad);
        if (!transform)
            continue;

        auto textureID = portalChildQuadResourceID(quad.nodeID, PortalChildQuadResource::Texture);
        sources.append(WebModel::PortalContentSource { quad.bufferSet, textureID, quad.isOpaque });
        transforms.set(quad.nodeID, *transform);

        auto previous = m_sentPortalChildTransforms.find(quad.nodeID);
        bool isNew = previous == m_sentPortalChildTransforms.end();
        if (isNew) {
            newMaterials.append(WebModel::UpdateMaterialDescriptor {
                .materialGraph = WebModel::MaterialGraph { .graphName = portalChildQuadGraphName },
                .identifier = portalChildQuadResourceID(quad.nodeID, PortalChildQuadResource::Material),
                .portalContentTextureIdentifier = textureID,
            });
            newMeshes.append({ quad.nodeID, portalChildQuadMeshUpdate(quad.nodeID) });
        }

        if (isNew || !(previous->value == *transform))
            changedTransforms.append({ quad.nodeID, *transform });
    }

    Vector<WebCore::NodeIdentifier> removedNodes;
    Vector<WebModel::TypedResourceId> meshRemovals;
    Vector<WebModel::TypedResourceId> materialRemovals;
    Vector<WebModel::TypedResourceId> textureRemovals;
    for (auto nodeID : m_sentPortalChildTransforms.keys()) {
        if (transforms.contains(nodeID))
            continue;

        removedNodes.append(nodeID);
        meshRemovals.append(portalChildQuadResourceID(nodeID, PortalChildQuadResource::Mesh));
        materialRemovals.append(portalChildQuadResourceID(nodeID, PortalChildQuadResource::Material));
        textureRemovals.append(portalChildQuadResourceID(nodeID, PortalChildQuadResource::Texture));
    }

    m_sentPortalChildTransforms = WTF::move(transforms);

    if (!newMaterials.isEmpty())
        updateMaterial(WTF::move(newMaterials));

    for (auto& [nodeID, descriptor] : newMeshes) {
        auto sendResult = send(Messages::RemoteMesh::UpdateModel(nodeID, { WTF::move(descriptor) }));
        UNUSED_VARIABLE(sendResult);
    }

    for (auto& [nodeID, transform] : changedTransforms) {
        auto sendResult = send(Messages::RemoteMesh::UpdatePortalChildTransform(nodeID, transform));
        UNUSED_VARIABLE(sendResult);
    }

    if (sources != m_sentPortalContentSources) {
        m_sentPortalContentSources = sources;
        auto sendResult = sendWithAsyncReply(Messages::RemoteMesh::SetPortalContentSources(WTF::move(sources)), [](bool) { });
        UNUSED_VARIABLE(sendResult);
    }

    for (auto nodeID : removedNodes) {
        auto sendResult = send(Messages::RemoteMesh::RemoveModel(nodeID));
        UNUSED_VARIABLE(sendResult);
    }

    if (!meshRemovals.isEmpty())
        processRemovals(WTF::move(meshRemovals), WTF::move(materialRemovals), WTF::move(textureRemovals), [](bool) { });
}
#endif

std::optional<WebModel::Float4x4> RemoteMeshProxy::resolvedPortalTransform() const
{
#if ENABLE(GPU_PROCESS_MODEL)
    if (!m_portalTransform || !m_fittedPortalTransform)
        return std::nullopt;

    WebModel::Float4x4 fittedTransform = *m_fittedPortalTransform;
    if (m_portalTransform->fitsContent)
        return applyContentTransform(*m_portalTransform, fittedTransform, TranslationUnits::Reporting);

    WebModel::Float4x4 matrix = applyContentTransform(*m_portalTransform, fittedTransform, TranslationUnits::Rendering);
    matrix.column0 = simd_make_float4(simd_make_float3(matrix.column0) * kCSSPixelsPerMeter, 0.f);
    matrix.column1 = simd_make_float4(simd_make_float3(matrix.column1) * kCSSPixelsPerMeter, 0.f);
    matrix.column2 = simd_make_float4(simd_make_float3(matrix.column2) * kCSSPixelsPerMeter, 0.f);
    matrix.column3 = simd_make_float4(simd_make_float3(matrix.column3) * kCSSPixelsPerMeter, 1.f);

    return matrix;
#else
    return std::nullopt;
#endif
}
#endif

#if ENABLE(SPATIAL_PORTAL) && ENABLE(GPU_PROCESS_MODEL)
std::pair<simd_float4, simd_float4> RemoteMeshProxy::portalContentCenterAndExtents(const simd_float4x4& transformBeforeAuto) const
{
    ModelBounds merged;
    for (auto& bounds : m_modelBounds.values()) {
        if (bounds.boundsAreEmpty())
            continue;

        simd_float3 minimum;
        simd_float3 maximum;
        transformBoundingBox(simd_make_float3(bounds.maxCorner - bounds.minCorner), simd_make_float3(.5f * (bounds.minCorner + bounds.maxCorner)), transformBeforeAuto, minimum, maximum);
        merged.minCorner = simd_min(merged.minCorner, simd_make_float4(minimum, 1.f));
        merged.maxCorner = simd_max(merged.maxCorner, simd_make_float4(maximum, 1.f));
    }

    if (merged.boundsAreEmpty())
        return { simd_make_float4(0.f), simd_make_float4(0.f) };

    return makeCenterAndExtents(merged.minCorner, merged.maxCorner);
}
#endif

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
