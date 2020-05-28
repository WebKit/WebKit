/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

namespace WebCore {

template<typename T> struct ColorComponents;

// FIXME: This should likely be replaced by a generic matrix class. Other than
// the specific matrix constuctors, there is nothing color specific about this.
// Futhermore, most these don't need to be 4x5 matrices with most only needing
// a 3x3 matrix.
class ColorMatrix {
public:
    static ColorMatrix grayscaleMatrix(float);
    static ColorMatrix saturationMatrix(float);
    static ColorMatrix hueRotateMatrix(float angleInDegrees);
    static ColorMatrix sepiaMatrix(float);

    ColorMatrix();
    ColorMatrix(const float[20]);
    
    void transformColorComponents(ColorComponents<float>&) const;
    ColorComponents<float> transformedColorComponents(const ColorComponents<float>&) const;

private:
    void makeIdentity();

    float m_matrix[4][5];
};

} // namespace WebCore
