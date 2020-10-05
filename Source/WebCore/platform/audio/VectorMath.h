/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef VectorMath_h
#define VectorMath_h

// Defines the interface for several vector math functions whose implementation will ideally be optimized.

namespace WebCore {

namespace VectorMath {

// Vector scalar multiply and then add.
void vsma(const float* inputVector, int inputStride, const float* scale, float* outputVector, int outputStride, size_t numberOfElementsToProcess);

void vsmul(const float* inputVector, int inputStride, const float* scale, float* outputVector, int outputStride, size_t numberOfElementsToProcess);
void vsadd(const float* inputVector, int inputStride, const float* scalar, float* outputVector, int outputStride, size_t numberOfElementsToProcess);
void vadd(const float* inputVector1, int inputStride1, const float* inputVector2, int inputStride2, float* outputVector, int outputStride, size_t numberOfElementsToProcess);

// Finds the maximum magnitude of a float vector.
void vmaxmgv(const float* inputVector, int inputStride, float* maximumValue, size_t numberOfElementsToProcess);

// Sums the squares of a float vector's elements.
void vsvesq(const float* inputVector, int inputStride, float* sum, size_t numberOfElementsToProcess);

// For an element-by-element multiply of two float vectors.
void vmul(const float* inputVector1, int inputStride1, const float* inputVector2, int inputStride2, float* outputVector, int outputStride, size_t numberOfElementsToProcess);

// Multiplies two complex vectors.
void zvmul(const float* realVector1, const float* imagVector1, const float* realVector2, const float* imagVector2, float* realOutputVector, float* imagOutputVector, size_t numberOfElementsToProcess);

// Copies elements while clipping values to the threshold inputs.
void vclip(const float* inputVector, int inputStride, const float* lowThresholdP, const float* highThresholdP, float* outputVector, int outputStride, size_t numberOfElementsToProcess);

void linearToDecibels(const float* inputVector, int inputStride, float* outputVector, int outputStride, size_t numberOfElementsToProcess);

} // namespace VectorMath

} // namespace WebCore

#endif // VectorMath_h
