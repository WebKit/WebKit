/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
void multiplyByScalarThenAddToOutput(std::span<const float> inputVector, float scalar, std::span<float> outputVector);

// Adds a vector inputVector2 to the product of a scalar value and a single-precision vector inputVector1 (vsma).
// for (n = 0; n < numberOfElementsToProcess; ++n)
//     outputVector[n] = inputVector1[n] * scalar + inputVector2[n];
void multiplyByScalarThenAddToVector(std::span<const float> inputVector1, float scalar, std::span<const float> inputVector2, std::span<float> outputVector);

// Multiplies the sum of two vectors by a scalar value (vasm).
void addVectorsThenMultiplyByScalar(std::span<const float> inputVector1, std::span<const float> inputVector2, float scalar, std::span<float> outputVector);

void multiplyByScalar(std::span<const float> inputVector, float scalar, std::span<float> outputVector);
void addScalar(std::span<const float> inputVector, float scalar, std::span<float> outputVector);
void substract(std::span<const float> inputVector1, std::span<const float> inputVector2, std::span<float> outputVector);

void add(std::span<const int> inputVector1, std::span<const int> inputVector2, std::span<int> outputVector);
void add(std::span<const float> inputVector1, std::span<const float> inputVector2, std::span<float> outputVector);
void add(std::span<const double> inputVector1, std::span<const double> inputVector2, std::span<double> outputVector);

// result = sum(inputVector1[n] * inputVector2[n], 0 <= n < inputVector1.size());
float dotProduct(std::span<const float> inputVector1, std::span<const float> inputVector2);

// Finds the maximum magnitude of a float vector.
float maximumMagnitude(std::span<const float> inputVector);

// Sums the squares of a float vector's elements (svesq).
float sumOfSquares(std::span<const float> inputVector);

// For an element-by-element multiply of two float vectors.
void multiply(std::span<const float> inputVector1, std::span<const float> inputVector2, std::span<float> outputVector);

// Multiplies two complex vectors (zvmul).
void multiplyComplex(std::span<const float> realVector1, std::span<const float> imagVector1, std::span<const float> realVector2, std::span<const float> imagVector2, std::span<float> realOutputVector, std::span<float> imagOutputVector);

// Copies elements while clipping values to the threshold inputs.
void clamp(std::span<const float> inputVector, float mininum, float maximum, std::span<float> outputVector);

void linearToDecibels(std::span<const float> inputVector, std::span<float> outputVector);

// Calculates the linear interpolation between the supplied single-precision vectors
// for (n = 0; n < numberOfElementsToProcess; ++n)
//     outputVector[n] = inputVector1[n] + interpolationFactor * (inputVector2[n] - inputVector1[n]);
// NOTE: Internal implementation may modify inputVector2.
void interpolate(std::span<const float> inputVector1, std::span<float> inputVector2, float interpolationFactor, std::span<float> outputVector);

} // namespace VectorMath

} // namespace WebCore
