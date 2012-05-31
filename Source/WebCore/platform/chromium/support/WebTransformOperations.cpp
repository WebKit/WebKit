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

#include <wtf/Vector.h>

namespace WebKit {

struct WebTransformOperation {
    enum Type {
        WebTransformOperationTranslate,
        WebTransformOperationRotate,
        WebTransformOperationScale,
        WebTransformOperationSkew,
        WebTransformOperationPerspective,
        WebTransformOperationMatrix
    };

    Type type;
    WebTransformationMatrix matrix;
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

WebTransformationMatrix WebTransformOperations::blend(const WebTransformOperations& from, double progress) const
{
    WebTransformationMatrix toReturn;
    if (matchesTypes(from)) {
        for (size_t i = 0; i < m_private->operations.size(); ++i) {
            WebTransformationMatrix blended = m_private->operations[i].matrix;
            blended.blend(from.m_private->operations[i].matrix, progress);
            toReturn.multiply(blended);
        }
    } else {
        toReturn = apply();
        WebTransformationMatrix fromTransform = from.apply();
        toReturn.blend(fromTransform, progress);
    }
    return toReturn;
}

bool WebTransformOperations::matchesTypes(const WebTransformOperations& other) const
{
    if (m_private->operations.size() != other.m_private->operations.size())
        return false;

    for (size_t i = 0; i < m_private->operations.size(); ++i) {
        if (m_private->operations[i].type != other.m_private->operations[i].type)
            return false;
    }

    return true;
}

void WebTransformOperations::appendTranslate(double x, double y, double z)
{
    WebTransformOperation toAdd;
    toAdd.matrix.translate3d(x, y, z);
    toAdd.type = WebTransformOperation::WebTransformOperationTranslate;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendRotate(double x, double y, double z, double degrees)
{
    WebTransformOperation toAdd;
    toAdd.matrix.rotate3d(x, y, z, degrees);
    toAdd.type = WebTransformOperation::WebTransformOperationRotate;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendScale(double x, double y, double z)
{
    WebTransformOperation toAdd;
    toAdd.matrix.scale3d(x, y, z);
    toAdd.type = WebTransformOperation::WebTransformOperationScale;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendSkew(double x, double y)
{
    WebTransformOperation toAdd;
    toAdd.matrix.skewX(x);
    toAdd.matrix.skewY(y);
    toAdd.type = WebTransformOperation::WebTransformOperationSkew;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendPerspective(double depth)
{
    WebTransformOperation toAdd;
    toAdd.matrix.applyPerspective(depth);
    toAdd.type = WebTransformOperation::WebTransformOperationPerspective;
    m_private->operations.append(toAdd);
}

void WebTransformOperations::appendMatrix(const WebTransformationMatrix& matrix)
{
    WebTransformOperation toAdd;
    toAdd.matrix = matrix;
    toAdd.type = WebTransformOperation::WebTransformOperationMatrix;
    m_private->operations.append(toAdd);
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

} // namespace WebKit
