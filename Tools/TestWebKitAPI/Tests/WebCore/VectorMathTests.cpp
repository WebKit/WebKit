/*
 * Copyright (C) 2026 Shubham Kapoor. All rights reserved.
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

#if ENABLE(WEB_AUDIO) && !USE(ACCELERATE)

#include "Helpers/Test.h"
#include <WebCore/VectorMath.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(VectorMath, MultiplyComplexInPlace)
{
    // An odd length so the scalar loop runs even where there is a SIMD path.
    Vector<float> real1 { 1, 2, 3, 4, 5, 6, 7 };
    Vector<float> imag1 { 2, 3, 4, 5, 6, 7, 8 };
    Vector<float> real2 { 3, 4, 5, 6, 7, 8, 9 };
    Vector<float> imag2 { 4, 5, 6, 7, 8, 9, 10 };

    Vector<float> expectedReal(real1.size());
    Vector<float> expectedImag(real1.size());
    VectorMath::multiplyComplex(real1.span(), imag1.span(), real2.span(), imag2.span(), expectedReal.mutableSpan(), expectedImag.mutableSpan());

    VectorMath::multiplyComplex(real1.span(), imag1.span(), real2.span(), imag2.span(), real1.mutableSpan(), imag1.mutableSpan());

    for (size_t i = 0; i < real1.size(); ++i) {
        EXPECT_EQ(expectedReal[i], real1[i]);
        EXPECT_EQ(expectedImag[i], imag1[i]);
    }
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUDIO) && !USE(ACCELERATE)
