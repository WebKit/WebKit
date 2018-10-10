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
#include "config.h"
#include "VRFrameData.h"

#include "VREyeParameters.h"
#include "VRPlatformDisplay.h"
#include "VRPose.h"

namespace WebCore {

VRFrameData::VRFrameData()
    : m_pose(VRPose::create())
{
}

static Ref<Float32Array> matrixToArray(const TransformationMatrix& matrix)
{
    auto columnMajorMatrix = matrix.toColumnMajorFloatArray();
    return Float32Array::create(columnMajorMatrix.data(), 16);
}

Ref<Float32Array> VRFrameData::leftProjectionMatrix() const
{
    return matrixToArray(m_leftProjectionMatrix);
}

Ref<Float32Array> VRFrameData::leftViewMatrix() const
{
    return matrixToArray(m_leftViewMatrix);
}

Ref<Float32Array> VRFrameData::rightProjectionMatrix() const
{
    return matrixToArray(m_rightProjectionMatrix);
}

Ref<Float32Array> VRFrameData::rightViewMatrix() const
{
    return matrixToArray(m_rightViewMatrix);
}

const VRPose& VRFrameData::pose() const
{
    return m_pose;
}

static TransformationMatrix projectionMatrixFromFieldOfView(const VRFieldOfView& fov, double depthNear, double depthFar)
{
    double upTan = tan(deg2rad(fov.upDegrees()));
    double downTan = tan(deg2rad(fov.downDegrees()));
    double leftTan = tan(deg2rad(fov.leftDegrees()));
    double rightTan = tan(deg2rad(fov.rightDegrees()));

    double xScale = 2 / (leftTan + rightTan);
    double yScale = 2 / (upTan + downTan);

    TransformationMatrix projectionMatrix;
    projectionMatrix.setM11(xScale);
    projectionMatrix.setM22(yScale);
    projectionMatrix.setM32((upTan - downTan) * yScale * 0.5);
    projectionMatrix.setM31(-((leftTan - rightTan) * xScale * 0.5));
    projectionMatrix.setM33((depthNear + depthFar) / (depthNear - depthFar));
    projectionMatrix.setM34(-1);
    projectionMatrix.setM43((2 * depthFar * depthNear) / (depthNear - depthFar));
    projectionMatrix.setM44(0);

    return projectionMatrix;
}

// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
static TransformationMatrix rotationMatrixFromQuaternion(const VRPlatformTrackingInfo::Quaternion& quaternion)
{
    double magnitude = (quaternion.x * quaternion.x) + (quaternion.y * quaternion.y) + (quaternion.z * quaternion.z) + (quaternion.w * quaternion.w);
    VRPlatformTrackingInfo::Quaternion normalizedQuaternion(quaternion.x / magnitude, quaternion.y / magnitude, quaternion.z / magnitude, quaternion.w / magnitude);
    double x2 = normalizedQuaternion.x * normalizedQuaternion.x;
    double y2 = normalizedQuaternion.y * normalizedQuaternion.y;
    double z2 = normalizedQuaternion.z * normalizedQuaternion.z;
    double w2 = normalizedQuaternion.w * normalizedQuaternion.w;
    double xy = normalizedQuaternion.x * normalizedQuaternion.y;
    double zw = normalizedQuaternion.z * normalizedQuaternion.w;
    double xz = normalizedQuaternion.x * normalizedQuaternion.z;
    double yw = normalizedQuaternion.y * normalizedQuaternion.w;
    double yz = normalizedQuaternion.y * normalizedQuaternion.z;
    double xw = normalizedQuaternion.x * normalizedQuaternion.w;

    return TransformationMatrix(
        x2 - y2 - z2 + w2, 2.0 * (xy - zw), 2.0 * (xz + yw), 0,
        2.0 * (xy + zw), -x2 + y2 - z2 + w2, 2.0 * (yz - xw), 0,
        2.0 * (xz - yw), 2.0 * (yz + xw), -x2 - y2 + z2 + w2, 0,
        0, 0, 0, 1);
}

static void applyHeadToEyeTransform(TransformationMatrix& matrix, const FloatPoint3D& translation)
{
    matrix.setM41(matrix.m41() - translation.x());
    matrix.setM42(matrix.m42() - translation.y());
    matrix.setM43(matrix.m43() - translation.z());
}

void VRFrameData::update(const VRPlatformTrackingInfo& trackingInfo, const VREyeParameters& leftEye, const VREyeParameters& rightEye, double depthNear, double depthFar)
{
    m_leftProjectionMatrix = projectionMatrixFromFieldOfView(leftEye.fieldOfView(), depthNear, depthFar);
    m_rightProjectionMatrix = projectionMatrixFromFieldOfView(rightEye.fieldOfView(), depthNear, depthFar);

    m_timestamp = trackingInfo.timestamp;
    m_pose->update(trackingInfo);

    auto rotationMatrix = rotationMatrixFromQuaternion(trackingInfo.orientation.value_or(VRPlatformTrackingInfo::Quaternion(0, 0, 0, 1)));
    FloatPoint3D position = trackingInfo.position.value_or(FloatPoint3D(0, 0, 0));
    rotationMatrix.translate3d(-position.x(), -position.y(), -position.z());

    m_leftViewMatrix = rotationMatrix;
    applyHeadToEyeTransform(m_leftViewMatrix, leftEye.rawOffset());

    m_rightViewMatrix = rotationMatrix;
    applyHeadToEyeTransform(m_rightViewMatrix, rightEye.rawOffset());
}

} // namespace WebCore
