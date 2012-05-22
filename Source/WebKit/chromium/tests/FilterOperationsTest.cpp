/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "FilterOperations.h"

#include <gtest/gtest.h>
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>

using namespace WebCore;
using namespace WebKit;

namespace {

TEST(FilterOperationsTest, getOutsetsBlur)
{
    FilterOperations ops;
    ops.operations().append(BlurFilterOperation::create(Length(20.0, WebCore::Fixed), FilterOperation::BLUR));
    EXPECT_TRUE(ops.hasOutsets());
    int top, right, bottom, left;
    top = right = bottom = left = 0;
    ops.getOutsets(top, right, bottom, left);
    EXPECT_EQ(57, top);
    EXPECT_EQ(57, right);
    EXPECT_EQ(57, bottom);
    EXPECT_EQ(57, left);
}

TEST(FilterOperationsTest, getOutsetsDropShadow)
{
    FilterOperations ops;
    ops.operations().append(DropShadowFilterOperation::create(IntPoint(3, 8), 20, Color(1, 2, 3), FilterOperation::DROP_SHADOW));
    EXPECT_TRUE(ops.hasOutsets());
    int top, right, bottom, left;
    top = right = bottom = left = 0;
    ops.getOutsets(top, right, bottom, left);
    EXPECT_EQ(49, top);
    EXPECT_EQ(60, right);
    EXPECT_EQ(65, bottom);
    EXPECT_EQ(54, left);
}

TEST(WebFilterOperationsTest, getOutsetsBlur)
{
    WebFilterOperations ops;
    ops.append(WebFilterOperation::createBlurFilter(20));
    int top, right, bottom, left;
    top = right = bottom = left = 0;
    ops.getOutsets(top, right, bottom, left);
    EXPECT_EQ(57, top);
    EXPECT_EQ(57, right);
    EXPECT_EQ(57, bottom);
    EXPECT_EQ(57, left);
}

TEST(WebFilterOperationsTest, getOutsetsDropShadow)
{
    WebFilterOperations ops;
    ops.append(WebFilterOperation::createDropShadowFilter(WebPoint(3, 8), 20, 0));
    int top, right, bottom, left;
    top = right = bottom = left = 0;
    ops.getOutsets(top, right, bottom, left);
    EXPECT_EQ(49, top);
    EXPECT_EQ(60, right);
    EXPECT_EQ(65, bottom);
    EXPECT_EQ(54, left);
}

}

