/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef Matrix3DTransformOperation_h
#define Matrix3DTransformOperation_h

#include "TransformOperation.h"

namespace WebCore {

class Matrix3DTransformOperation final : public TransformOperation {
public:
    static PassRefPtr<Matrix3DTransformOperation> create(const TransformationMatrix& matrix)
    {
        return adoptRef(new Matrix3DTransformOperation(matrix));
    }

    TransformationMatrix matrix() const {return m_matrix; }

private:    
    virtual bool isIdentity() const override { return m_matrix.isIdentity(); }

    virtual OperationType type() const override { return MATRIX_3D; }
    virtual bool isSameType(const TransformOperation& o) const override { return o.type() == MATRIX_3D; }

    virtual bool operator==(const TransformOperation&) const override;

    virtual bool apply(TransformationMatrix& transform, const FloatSize&) const override
    {
        transform.multiply(TransformationMatrix(m_matrix));
        return false;
    }

    virtual PassRefPtr<TransformOperation> blend(const TransformOperation* from, double progress, bool blendToIdentity = false) override;
    
    Matrix3DTransformOperation(const TransformationMatrix& mat)
    {
        m_matrix = mat;
    }

    TransformationMatrix m_matrix;
};

TRANSFORMOPERATION_TYPE_CASTS(Matrix3DTransformOperation, type() == TransformOperation::MATRIX_3D);

} // namespace WebCore

#endif // Matrix3DTransformOperation_h
