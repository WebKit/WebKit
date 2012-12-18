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

#ifndef WebTransformOperations_h
#define WebTransformOperations_h

#include "WebTransformationMatrix.h"

namespace WebKit {

class WebTransformOperationsPrivate;

// Transform operations are a decomposed transformation matrix. It can be
// applied to obtain a WebTransformationMatrix at any time, and can be blended
// intelligently with other transform operations, so long as they represent the
// same decomposition. For example, if we have a transform that is made up of
// a rotation followed by skew, it can be blended intelligently with another
// transform made up of a rotation followed by a skew. Blending is possible if
// we have two dissimilar sets of transform operations, but the effect may not
// be what was intended. For more information, see the comments for the blend
// function below.
class WebTransformOperations {
public:
    ~WebTransformOperations() { reset(); }

    WebTransformOperations() { initialize(); }
    WebTransformOperations(const WebTransformOperations& other) { initialize(other); }
    WebTransformOperations& operator=(const WebTransformOperations& other)
    {
        initialize(other);
        return *this;
    }

    // Returns a transformation matrix representing these transform operations.
    WEBKIT_EXPORT WebTransformationMatrix apply() const;

    // Given another set of transform operations and a progress in the range
    // [0, 1], returns a transformation matrix representing the intermediate
    // value. If this->matchesTypes(from), then each of the operations are
    // blended separately and then combined. Otherwise, the two sets of
    // transforms are baked to matrices (using apply), and the matrices are
    // then decomposed and interpolated. For more information, see
    // http://www.w3.org/TR/2011/WD-css3-2d-transforms-20111215/#matrix-decomposition.
    WEBKIT_EXPORT WebTransformationMatrix blend(const WebTransformOperations& from, double progress) const;

    // Returns true if this operation and its descendants have the same types
    // as other and its descendants.
    WEBKIT_EXPORT bool matchesTypes(const WebTransformOperations& other) const;

    // Returns true if these operations can be blended. It will only return
    // false if we must resort to matrix interpolation, and matrix interpolation
    // fails (this can happen if either matrix cannot be decomposed).
    WEBKIT_EXPORT bool canBlendWith(const WebTransformOperations& other) const;

    WEBKIT_EXPORT void appendTranslate(double x, double y, double z);
    WEBKIT_EXPORT void appendRotate(double x, double y, double z, double degrees);
    WEBKIT_EXPORT void appendScale(double x, double y, double z);
    WEBKIT_EXPORT void appendSkew(double x, double y);
    WEBKIT_EXPORT void appendPerspective(double depth);
    WEBKIT_EXPORT void appendMatrix(const WebTransformationMatrix&);
    WEBKIT_EXPORT void appendIdentity();

    WEBKIT_EXPORT bool isIdentity() const;

private:
    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void initialize();
    WEBKIT_EXPORT void initialize(const WebTransformOperations& prototype);
    WEBKIT_EXPORT bool blendInternal(const WebTransformOperations& from, double progress, WebTransformationMatrix& result) const;

    WebPrivateOwnPtr<WebTransformOperationsPrivate> m_private;
};

} // namespace WebKit

#endif // WebTransformOperations_h

