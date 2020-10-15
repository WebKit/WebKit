/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2020, Apple Inc. All rights reserved.
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

#pragma once

// Defines the interface for several vector math functions whose implementation will ideally be optimized.

namespace WebCore {

namespace VectorMath {

// Multiples inputVector by scalar then adds the result to outputVector (simplified vsma).
// for (n = 0; n < numberOfElementsToProcess; ++n)
//     outputVector[n] += inputVector[n] * scalar;
void multiplyByScalarThenAddToOutput(const float* inputVector, float scalar, float* outputVector, size_t numberOfElementsToProcess);

// Adds a vector inputVector2 to the product of a scalar value and a single-precision vector inputVector1 (vsma).
// for (n = 0; n < numberOfElementsToProcess; ++n)
//     outputVector[n] = inputVector1[n] * scalar + inputVector2[n];
void multiplyByScalarThenAddToVector(const float* inputVector1, float scalar, const float* inputVector2, float* outputVector, size_t numberOfElementsToProcess);

// Multiplies the sum of two vectors by a scalar value (vasm).
void addVectorsThenMultiplyByScalar(const float* inputVector1, const float* inputVector2, float scalar, float* outputVector, size_t numberOfElementsToProcess);

void multiplyByScalar(const float* inputVector, float scalar, float* outputVector, size_t numberOfElementsToProcess);
void addScalar(const float* inputVector, float scalar, float* outputVector, size_t numberOfElementsToProcess);
void add(const float* inputVector1, const float* inputVector2, float* outputVector, size_t numberOfElementsToProcess);

// Finds the maximum magnitude of a float vector.
float maximumMagnitude(const float* inputVector, size_t numberOfElementsToProcess);

// Sums the squares of a float vector's elements (svesq).
float sumOfSquares(const float* inputVector, size_t numberOfElementsToProcess);

// For an element-by-element multiply of two float vectors.
void multiply(const float* inputVector1, const float* inputVector2, float* outputVector, size_t numberOfElementsToProcess);

// Multiplies two complex vectors (zvmul).
void multiplyComplex(const float* realVector1, const float* imagVector1, const float* realVector2, const float* imagVector2, float* realOutputVector, float* imagOutputVector, size_t numberOfElementsToProcess);

// Copies elements while clipping values to the threshold inputs.
void clamp(const float* inputVector, float mininum, float maximum, float* outputVector, size_t numberOfElementsToProcess);

void linearToDecibels(const float* inputVector, float* outputVector, size_t numberOfElementsToProcess);

} // namespace VectorMath

} // namespace WebCore
