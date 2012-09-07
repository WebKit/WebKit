/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <public/WebTransformOperations.h>

#include <algorithm>
#include <wtf/Vector.h>

using namespace std;

namespace {
const double EPSILON = 1e-4;
}

namespace WebKit {

struct WebTransformOperation {
    enum Type {
        WebTransformOperationTranslate,
        WebTransformOperationRotate,
        WebTransformOperationScale,
        WebTransformOperationSkew,
        WebTransformOperationPerspective,
        WebTransformOperationMatrix,
        WebTransformOperationIdentity
    };

    WebTransformOperation()
        : type(WebTransformOperationIdentity)
    {
    }

    Type type;
    WebTransformationMatrix matrix;

    union {
        double perspectiveDepth;

        struct {
            double x, y;
        } skew;

        struct {
             double x, y, z;
        } scale;

        struct {
            double x, y, z;
        } translate;

        struct {
            struct {
                double x, y, z;
            } axis;

            double angle;
        } rotate;
    };

    bool isIdentity() const { return matrix.isIdentity(); }
};

class WebTransformOperationsPrivate {
public:
    Vector<WebTransformOperation> operations;
};

WebTransformationMatrix WebTransformOperations::apply() const
{
    WebTransformationMatrix toReturn;
    for (size_t i = 0; i < m_private->operations.size(); ++i)
        toReturn.multiply(m_private->operations[i].matrix);
    return toReturn;
}

static bool isIdentity(const WebTransformOperation* operation)
{
    return !operation || operation->isIdentity();
}

static bool shareSameAxis(const WebTransformOperation* from, const WebTransformOperation* to, double& axisX, double& axisY, double& axisZ, double& angleFrom)
{
    if (isIdentity(from) && isIdentity(to))
        return false;

    if (isIdentity(from) && !isIdentity(to)) {
        axisX = to->rotate.axis.x;
        axisY = to->rotate.axis.y;
        axisZ = to->rotate.axis.z;
        angleFrom = 0;
        return true;
    }

    if (!isIdentity(from) && isIdentity(to)) {
        axisX = from->rotate.axis.x;
        axisY = from->rotate.axis.y;
        axisZ = from->rotate.axis.z;
        angleFrom = from->rotate.angle;
        return true;
    }

    double length2 = from->rotate.axis.x * from->rotate.axis.x + from->rotate.axis.y * from->rotate.axis.y + from->rotate.axis.z * from->rotate.axis.z;
    double otherLength2 = to->rotate.axis.x * to->rotate.axis.x + to->rotate.axis.y * to->rotate.axis.y + to->rotate.axis.z * to->rotate.axis.z;

    if (length2 <= EPSILON || otherLength2 <= EPSILON)
        return false;

    double dot = to->rotate.axis.x * from->rotate.axis.x + to->rotate.axis.y * from->rotate.axis.y + to->rotate.axis.z * from->rotate.axis.z;
    double error = fabs(1.0 - (dot * dot) / (length2 * otherLength2));
    bool result = error < EPSILON;
    if (result) {
        axisX = to->rotate.axis.x;
        axisY = to->rotate.axis.y;
        axisZ = to->rotate.axis.z;
        // If the axes are pointing in opposite directions, we need to reverse
        // the angle.
        angleFrom = dot > 0 ? from->rotate.angle : -from->rotate.angle;
    }
    return result;
}

static double blendDoubles(double from, double to, double progress)
{
    return from * (1 - progress) + to * progress;
}

static bool blendTransformOperations(const WebTransformOperation* from, const WebTransformOperation* to, double progress, WebTransformationMatrix& result)
{
    if (isIdentity(from) && isIdentity(to))
        return true;

    WebTransformOperation::Type interpolationType = WebTransformOperation::WebTransformOperationIdentity;
    if (isIdentity(to))
        interpolationType = from->type;
    else
        interpolationType = to->type;

    switch (interpolationType) {
    case WebTransformOperation::WebTransformOperationTranslate: {
        double fromX = isIdentity(from) ? 0 : from->translate.x;
        double fromY = isIdentity(from) ? 0 : from->translate.y;
        double fromZ = isIdentity(from) ? 0 : from->translate.z;
        double toX = isIdentity(to) ? 0 : to->translate.x;
        double toY = isIdentity(to) ? 0 : to->translate.y;
        double toZ = isIdentity(to) ? 0 : to->translate.z;
        result.translate3d(blendDoubles(fromX, toX, progress),
                           blendDoubles(fromY, toY, progress),
                           blendDoubles(fromZ, toZ, progress));
        break;
    }
    case WebTransformOperation::WebTransformOperationRotate: {
        double axisX = 0;
        double axisY = 0;
        double axisZ = 1;
        double fromAngle = 0;
        double toAngle = isIdentity(to) ? 0 : to->rotate.angle;
        if (shareSameAxis(from, to, axisX, axisY, axisZ, fromAngle))
            result.rotate3d(axisX, axisY, axisZ, blendDoubles(fromAngle, toAngle, progress));
        else {
            WebTransformationMatrix toMatrix;
            if (!isIdentity(to))
                toMatrix = to->matrix;
            WebTransformationMatrix fromMatrix;
            if (!isIdentity(from))
                fromMatrix = from->matrix;
            result = toMatrix;
            if (!result.blend(fromMatrix, progress))
                return false;
        }
        break;
    }
    case WebTransformOperation::WebTransformOperationScale: {
        double fromX = isIdentity(from) ? 1 : from->scale.x;
        double fromY = isIdentity(from) ? 1 : from->scale.y;
        double fromZ = isIdentity(from) ? 1 : from->scale.z;
        double toX = isIdentity(to) ? 1 : to->scale.x;
        double toY = isIdentity(to) ? 1 : to->scale.y;
        double toZ = isIdentity(to) ? 1 : to->scale.z;
        result.scale3d(blendDoubles(fromX, toX, progress),
                       blendDoubles(fromY, toY, progress),
                       blendDoubles(fromZ, toZ, progress));
        break;
    }
    case WebTransformOperation::WebTransformOperationSkew: {
        double fromX = isIdentity(from) ? 0 : from->skew.x;
        double fromY = isIdentity(from) ? 0 : from->skew.y;
        double toX = isIdentity(to) ? 0 : to->skew.x;
        double toY = isIdentity(to) ? 0 : to->skew.y;
        result.skewX(blendDoubles(fromX, toX, progress));
        result.skewY(blendDoubles(fromY, toY, progress));
        break;
    }
    case WebTransformOperation::WebTransformOperationPerspective: {
        double fromPerspectiveDepth = isIdentity(from) ? numeric_limits<double>::max() : from->perspectiveDepth;
        double toPerspectiveDepth = isIdentity(to) ? numeric_limits<double>::max() : to->perspectiveDepth;
        result.applyPerspective(blendDoubles(fromPerspectiveDepth, toPerspectiveDepth, progress));
        break;
    }
    case WebTransformOperation::WebTransformOperationMatrix: {
        WebTransformationMatrix toMatrix;
        if (!isIdentity(to))
            toMatrix = to->matrix;
        WebTransformationMatrix fromMatrix;
        if (!isIdentity(from))
            fromMatrix = from->matrix;
        result = toMatrix;
        if (!result.blend(fromMatrix, progress))
            return false;
        break;
    }
    case WebTransformOperation::WebTransformOperationIdentity:
        // Do nothing.
        break;
    }

    return true;
}

WebTransformationMatrix WebTransformOperations::blend(const WebTransformOperations& from, double progress) const
{
    WebTransformationMatrix toReturn;
    blendInternal(from, progress, toReturn);
    return toReturn;
}

bool WebTransformOperations::matchesTypes(const WebTransformOperations& other) const
{
    if (isIdentity() || other.isIdentity())
        return true;

    if (m_private->operations.size() != other.m_private->operations.size())
        return false;

    for (size_t i = 0; i < m_private->operations.size(); ++i) {
        if (m_private->operations[i].type != other.m_private->operations[i].type
            && !m_private->operations[i].isIdentity()
            && !other.m_private->operations[i].isIdentity())
            return false;
    }

    return true;
}

bool WebTransformOperations::canBlendWith(const WebTransformOperations& other) const
{
    WebTransformationMatrix dummy;
    return blendInternal(other, 0.5, dummy);
}

void WebTransformOperations::appendTranslate(double x, double y, double z)
{
    WebTransformOperation toAdd;
    toAdd.matrix.translate3d(x, y, z);
    toAdd.type = WebTransformOperation::WebTransformOperationTranslate;
    toAdd.translate.x = x;
    toAdd.translate.y = y;
    toAdd.translate.z = z;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendRotate(double x, double y, double z, double degrees)
{
    WebTransformOperation toAdd;
    toAdd.matrix.rotate3d(x, y, z, degrees);
    toAdd.type = WebTransformOperation::WebTransformOperationRotate;
    toAdd.rotate.axis.x = x;
    toAdd.rotate.axis.y = y;
    toAdd.rotate.axis.z = z;
    toAdd.rotate.angle = degrees;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendScale(double x, double y, double z)
{
    WebTransformOperation toAdd;
    toAdd.matrix.scale3d(x, y, z);
    toAdd.type = WebTransformOperation::WebTransformOperationScale;
    toAdd.scale.x = x;
    toAdd.scale.y = y;
    toAdd.scale.z = z;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendSkew(double x, double y)
{
    WebTransformOperation toAdd;
    toAdd.matrix.skewX(x);
    toAdd.matrix.skewY(y);
    toAdd.type = WebTransformOperation::WebTransformOperationSkew;
    toAdd.skew.x = x;
    toAdd.skew.y = y;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendPerspective(double depth)
{
    WebTransformOperation toAdd;
    toAdd.matrix.applyPerspective(depth);
    toAdd.type = WebTransformOperation::WebTransformOperationPerspective;
    toAdd.perspectiveDepth = depth;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendMatrix(const WebTransformationMatrix& matrix)
{
    WebTransformOperation toAdd;
    toAdd.matrix = matrix;
    toAdd.type = WebTransformOperation::WebTransformOperationMatrix;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendIdentity()
{
    m_private->operations.append(WebTransformOperation());
}

bool WebTransformOperations::isIdentity() const
{
    for (size_t i = 0; i < m_private->operations.size(); ++i) {
        if (!m_private->operations[i].isIdentity())
            return false;
    }
    return true;
}

void WebTransformOperations::reset()
{
    m_private.reset(0);
}

void WebTransformOperations::initialize()
{
    m_private.reset(new WebTransformOperationsPrivate);
}

void WebTransformOperations::initialize(const WebTransformOperations& other)
{
    if (m_private.get() != other.m_private.get())
        m_private.reset(new WebTransformOperationsPrivate(*other.m_private.get()));
    else
        initialize();
}

bool WebTransformOperations::blendInternal(const WebTransformOperations& from, double progress, WebTransformationMatrix& result) const
{
    bool fromIdentity = from.isIdentity();
    bool toIdentity = isIdentity();
    if (fromIdentity && toIdentity)
        return true;

    if (matchesTypes(from)) {
        size_t numOperations = max(fromIdentity ? 0 : from.m_private->operations.size(),
                                   toIdentity ? 0 : m_private->operations.size());
        for (size_t i = 0; i < numOperations; ++i) {
            WebTransformationMatrix blended;
            if (!blendTransformOperations(fromIdentity ? 0 : &from.m_private->operations[i],
                                          toIdentity ? 0 : &m_private->operations[i],
                                          progress,
                                          blended))
                return false;
            result.multiply(blended);
        }
        return true;
    }

    result = apply();
    WebTransformationMatrix fromTransform = from.apply();
    return result.blend(fromTransform, progress);
}

} // namespace WebKit
