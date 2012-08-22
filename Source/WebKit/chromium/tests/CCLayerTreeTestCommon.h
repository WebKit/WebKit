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

#include <public/WebTransformationMatrix.h>

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

// This is a function rather than a macro because when this is included as a macro
// in bulk, it causes a significant slow-down in compilation time. This problem
// exists with both gcc and clang, and bugs have been filed at
// http://llvm.org/bugs/show_bug.cgi?id=13651 and http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54337
void ExpectTransformationMatrixEq(WebKit::WebTransformationMatrix expected,
                                  WebKit::WebTransformationMatrix actual);

#define EXPECT_TRANSFORMATION_MATRIX_EQ(expected, actual)            \
    {                                                                \
        SCOPED_TRACE("");                                            \
        WebKitTests::ExpectTransformationMatrixEq(expected, actual); \
    }

} // namespace

#endif // CCLayerTreeTestCommon_h
