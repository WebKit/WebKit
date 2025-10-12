/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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
#include "FractionToDouble.h"

#include "MathCommon.h"

// The calculations here are based on algorithms from two sources. The second
// one builds on the first.
//
// Shewchuk (1997). Adaptive precision floating-point arithmetic and fast robust
//   geometric predicates. Discrete & Computational Geometry 18(3), pp. 305–363.
//   https://doi.org/10.1007/PL00009321
//
// Hida, Li, Bailey (2008). Library for double-double and quad-double
//   arithmetic. Manuscript. https://www.davidhbailey.com/dhbpapers/qd.pdf
//   and the accompanying QD library https://github.com/BL-highprecision/QD,
//   which is BSD-licensed.

namespace JSC {

// Double-double precision floating point number, represented as the unevaluated
// sum of two doubles. In other words, dd[0] is the double approximation term
// and dd[1] is the error term.
//
// There are many such representations, but only one is 'normalized' meaning the
// dd[0] term is the most accurate possible double-precision approximation of
// the double-double value.
using DD = std::array<double, 2>;

// Conversion of Int128 to double-double precision floating point. The
// calculations follow from the definition of hi and lo: hi is the closest
// double-precision approximation to the exact value (which itself will be a
// safe integer) and lo is the error term.
static DD int128ToDD(const Int128& value)
{
    double hi = static_cast<double>(value);
    double lo = static_cast<double>(value - static_cast<Int128>(hi));
    return { hi, lo };
}

// Computes double-double precision a + b of two doubles a and b. This is the
// Two-Sum algorithm in theorem 7 of the Shewchuk paper.
static DD ddSum(double a, double b)
{
    // First compute the double-precision approximation of the sum by regular
    // double addition.
    double sum = a + b;

    // Compute the error term.
    double bVirtual = sum - a;
    double aVirtual = sum - bVirtual;
    double bRoundoff = b - bVirtual;
    double aRoundoff = a - aVirtual;
    double error = aRoundoff + bRoundoff;

    return { sum, error };
}

// Computes double-double precision a * b of two doubles a and b. The
// optimization using std::fma() is suggested in section 2 of the Hida-Li-Bailey
// paper.
static DD ddProduct(double a, double b)
{
    // First compute the double-precision approximation of the product by
    // regular double multiplication.
    double product = a * b;

    // On armv8, this emits the fnmsub instruction.
    // On x86_64, this emits the vfmsub213sd instruction if compiled with SSE
    // instructions. If not, it calls libm's fma(), which is comparably fast to
    // using the Two-Product algorithm in theorem 18 of the Shewchuk paper.
    double error = std::fma(a, b, -product);

    return { product, error };
}

// Computes double-double precision numerator / denominator, where divisor is a
// double, and rounds the result to double precision. This is described in
// section 3.5 of the Hida-Li-Bailey paper.
static double fractionToDoubleSlow(const Int128& numerator, double denominator)
{
    DD ddNumerator = int128ToDD(numerator);

    // Compute a first approximation of the quotient by regular double division.
    double quotient0 = ddNumerator[0] / denominator;

    // Compute remainder, ddNumerator - quotient0 * denominator.
    DD product = ddProduct(quotient0, denominator);
    DD remainder = ddSum(ddNumerator[0], -product[0]);

    // Compute the next approximation term.
    double error = remainder[1] + ddNumerator[1] - product[1];
    double quotient1 = (remainder[0] + error) / denominator;

    // The result is DD { quotient0, quotient1 }. If we wanted double-double
    // precision here, we would have to use the Fast-Two-Sum algorithm from
    // theorem 6 of the Shewchuk paper to renormalize the two terms, but since
    // we only need double precision we can discard the error term.
    return quotient0 + quotient1;
}

double fractionToDouble(const Int128& numerator, double denominator)
{
    ASSERT(denominator > 0);
    ASSERT(isSafeInteger(denominator));

    if (!numerator)
        return 0;

    // When the denominator is 1, we are just calculating the double
    // approximation of the numerator.
    if (denominator == 1)
        return static_cast<double>(numerator);

    // When the numerator can be represented exactly as a double the algorithm
    // collapses to a simple double division.
    if (isSafeInteger(static_cast<double>(numerator))) [[likely]]
        return static_cast<double>(numerator) / denominator;

    // Otherwise use double-double precision to compute the result.
    return fractionToDoubleSlow(numerator, denominator);
}

} // namespace JSC
