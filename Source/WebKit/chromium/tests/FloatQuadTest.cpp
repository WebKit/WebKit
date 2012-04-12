/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"

#include "FloatQuad.h"

#include "TransformationMatrix.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

TEST(FloatQuadTest, IsRectilinearTest)
{
    const int numRectilinear = 8;
    TransformationMatrix rectilinearTrans[numRectilinear];
    rectilinearTrans[1].rotate(90);
    rectilinearTrans[2].rotate(180);
    rectilinearTrans[3].rotate(270);
    rectilinearTrans[4].skewX(0.00000000001);
    rectilinearTrans[5].skewY(0.00000000001);
    rectilinearTrans[6].scale(0.00001);
    rectilinearTrans[6].rotate(180);
    rectilinearTrans[7].scale(100000);
    rectilinearTrans[7].rotate(180);

    for (int i = 0; i < numRectilinear; ++i) {
        FloatQuad quad = rectilinearTrans[i].mapQuad(FloatRect(0.01010101f, 0.01010101f, 100.01010101f, 100.01010101f));
        EXPECT_TRUE(quad.isRectilinear());
    }

    const int numNonRectilinear = 10;
    TransformationMatrix nonRectilinearTrans[numNonRectilinear];
    nonRectilinearTrans[0].rotate(359.999);
    nonRectilinearTrans[1].rotate(0.0000001);
    nonRectilinearTrans[2].rotate(89.999999);
    nonRectilinearTrans[3].rotate(90.0000001);
    nonRectilinearTrans[4].rotate(179.999999);
    nonRectilinearTrans[5].rotate(180.0000001);
    nonRectilinearTrans[6].rotate(269.999999);
    nonRectilinearTrans[7].rotate(270.0000001);
    nonRectilinearTrans[8].skewX(0.00001);
    nonRectilinearTrans[9].skewY(0.00001);

    for (int i = 0; i < numNonRectilinear; ++i) {
        FloatQuad quad = nonRectilinearTrans[i].mapQuad(FloatRect(0.01010101f, 0.01010101f, 100.01010101f, 100.01010101f));
        EXPECT_FALSE(quad.isRectilinear());
    }
}

} // empty namespace
