/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef CCLayerTreeTestCommon_h
#define CCLayerTreeTestCommon_h

namespace WebKitTests {

// These are macros instead of functions so that we get useful line numbers where a test failed.
#define EXPECT_FLOAT_RECT_EQ(expected, actual)                          \
    EXPECT_FLOAT_EQ((expected).location().x(), (actual).location().x()); \
    EXPECT_FLOAT_EQ((expected).location().y(), (actual).location().y()); \
    EXPECT_FLOAT_EQ((expected).size().width(), (actual).size().width()); \
    EXPECT_FLOAT_EQ((expected).size().height(), (actual).size().height())

#define EXPECT_INT_RECT_EQ(expected, actual)                            \
    EXPECT_EQ((expected).location().x(), (actual).location().x());      \
    EXPECT_EQ((expected).location().y(), (actual).location().y());      \
    EXPECT_EQ((expected).size().width(), (actual).size().width());      \
    EXPECT_EQ((expected).size().height(), (actual).size().height())

// This is a macro instead of a function so that we get useful line numbers where a test failed.
// Even though TransformationMatrix values are double precision, there are many other floating-point values used that affect
// the transforms, and so we only expect them to be accurate up to floating-point precision.
#define EXPECT_TRANSFORMATION_MATRIX_EQ(expected, actual)       \
    EXPECT_FLOAT_EQ((expected).m11(), (actual).m11());          \
    EXPECT_FLOAT_EQ((expected).m12(), (actual).m12());          \
    EXPECT_FLOAT_EQ((expected).m13(), (actual).m13());          \
    EXPECT_FLOAT_EQ((expected).m14(), (actual).m14());          \
    EXPECT_FLOAT_EQ((expected).m21(), (actual).m21());          \
    EXPECT_FLOAT_EQ((expected).m22(), (actual).m22());          \
    EXPECT_FLOAT_EQ((expected).m23(), (actual).m23());          \
    EXPECT_FLOAT_EQ((expected).m24(), (actual).m24());          \
    EXPECT_FLOAT_EQ((expected).m31(), (actual).m31());          \
    EXPECT_FLOAT_EQ((expected).m32(), (actual).m32());          \
    EXPECT_FLOAT_EQ((expected).m33(), (actual).m33());          \
    EXPECT_FLOAT_EQ((expected).m34(), (actual).m34());          \
    EXPECT_FLOAT_EQ((expected).m41(), (actual).m41());          \
    EXPECT_FLOAT_EQ((expected).m42(), (actual).m42());          \
    EXPECT_FLOAT_EQ((expected).m43(), (actual).m43());          \
    EXPECT_FLOAT_EQ((expected).m44(), (actual).m44())

} // namespace

#endif // CCLayerTreeTestCommon_h
