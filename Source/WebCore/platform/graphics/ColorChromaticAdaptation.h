/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/ColorMatrix.h>
#include <WebCore/ColorWhitePoint.h>

namespace WebCore {

template<WhitePoint From, WhitePoint To> struct ChromaticAdaptation;

// Chromatic Adaptation allows conversion from one white point to another.
//
// The values we use are pre-calculated for the two white points we support, D50
// and D65 using the Bradford method's chromatic adaptation transform (CAT), but
// can be extended to any pair of white points.
//
// The process to compute new ones is:
//
//  1. Choose a CAT and lookup its values (these are not derivable).
//     We currently use the Bradford CAT
//
//     let toCone =
//         [  0.8951000,  0.2664000, -0.1614000 ],
//         [ -0.7502000,  1.7135000,  0.0367000 ],
//         [  0.0389000, -0.0685000,  1.0296000 ]
//
//  2. In addition, you will need the inverse
//
//     let fromCone = toCone ^ -1
//
//  3. Choose source and destination XYZ white points
//
//     let whitePoint_src = [ ... , ... , ... ]
//     let whitePoint_dst = [ ... , ... , ... ]
//
//  4. Convert the white points into the cone response domain (denoted ρ, γ, β)
//
//     let [ ρ_src, γ_src, β_src ] = toCone * whitePoint_src
//     let [ ρ_dst, γ_dst, β_dst ] = toCone * whitePoint_dst
//
//  5. Compute a scale transform
//
//     let scale =
//         [ ρ_dst / ρ_src, 0,             0             ],
//         [ 0,             γ_dst / γ_src, 0             ],
//         [ 0,             0,             β_dst / β_src ]
//
//  6. Finally, use the scale to compute the adaptation transform
//     (what is stored ChromaticAdaptation.matrix) as the concatenation
//     of the toCone, scale and fromCone transforms.
//
//     let adaptation = fromCone * scale * toCone
//
// Additional details and more CATs / white point values can be found at: http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html

template<> struct ChromaticAdaptation<WhitePoint::D65, WhitePoint::D50> {
    static constexpr ColorMatrix<3, 3> matrix {
         1.0479297925449969,    0.022946870601609652, -0.05019226628920524,
         0.02962780877005599,   0.9904344267538799,   -0.017073799063418826,
        -0.009243040646204504,  0.015055191490298152,  0.7518742814281371
    };
};

template<> struct ChromaticAdaptation<WhitePoint::D50, WhitePoint::D65> {
    static constexpr ColorMatrix<3, 3> matrix {
         0.955473421488075,    -0.02309845494876471,   0.06325924320057072,
        -0.0283697093338637,    1.0099953980813041,    0.021041441191917323,
         0.012314014864481998, -0.020507649298898964,  1.330365926242124
    };
};

} // namespace WebCore
