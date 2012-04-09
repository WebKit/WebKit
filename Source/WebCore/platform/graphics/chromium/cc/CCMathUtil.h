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

#ifndef CCMathUtil_h
#define CCMathUtil_h

namespace WebCore {

class IntRect;
class FloatPoint;
class FloatRect;
class FloatQuad;
class TransformationMatrix;

// This class contains math helper functionality that does not belong in WebCore.
// It is possible that this functionality should be migrated to WebCore eventually.
class CCMathUtil {
public:

    // Background: TransformationMatrix code in WebCore does not do the right thing in
    // mapRect / mapQuad / projectQuad when there is a perspective projection that causes
    // one of the transformed vertices to go to w < 0. In those cases, it is necessary to
    // perform clipping in homogeneous coordinates, after applying the transform, before
    // dividing-by-w to convert to cartesian coordinates.
    //
    // These functions return the axis-aligned rect that encloses the correctly clipped,
    // transformed polygon.
    static IntRect mapClippedRect(const TransformationMatrix&, const IntRect&);
    static FloatRect mapClippedRect(const TransformationMatrix&, const FloatRect&);
    static FloatRect projectClippedRect(const TransformationMatrix&, const FloatRect&);

    // Returns an array of vertices that represent the clipped polygon. After returning, indexes from
    // 0 to numVerticesInClippedQuad are valid in the clippedQuad array. Note that
    // numVerticesInClippedQuad may be zero, which means the entire quad was clipped, and
    // none of the vertices in the array are valid.
    static void mapClippedQuad(const TransformationMatrix&, const FloatQuad& srcQuad, FloatPoint clippedQuad[8], int& numVerticesInClippedQuad);
    static FloatRect computeEnclosingRectOfVertices(FloatPoint vertices[], int numVertices);

    // NOTE: These functions do not do correct clipping against w = 0 plane, but they
    // correctly detect the clipped condition via the boolean clipped.
    static FloatQuad mapQuad(const TransformationMatrix&, const FloatQuad&, bool& clipped);
    static FloatQuad projectQuad(const TransformationMatrix&, const FloatQuad&, bool& clipped);
};

} // namespace WebCore

#endif // #define CCMathUtil_h
