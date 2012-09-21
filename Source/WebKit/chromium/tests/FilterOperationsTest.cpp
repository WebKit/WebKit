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

#define SAVE_RESTORE_AMOUNT(Type, a) \
    { \
        WebFilterOperation op = WebFilterOperation::create##Type##Filter(a); \
        EXPECT_EQ(WebFilterOperation::FilterType##Type, op.type()); \
        EXPECT_EQ(a, op.amount()); \
        \
        WebFilterOperation op2 = WebFilterOperation::createEmptyFilter(); \
        op2.setType(WebFilterOperation::FilterType##Type); \
        \
        EXPECT_NE(a, op2.amount()); \
        \
        op2.setAmount(a); \
        \
        EXPECT_EQ(WebFilterOperation::FilterType##Type, op2.type()); \
        EXPECT_EQ(a, op2.amount()); \
    }

#define SAVE_RESTORE_OFFSET_AMOUNT_COLOR(Type, a, b, c) \
    { \
        WebFilterOperation op = WebFilterOperation::create##Type##Filter(a, b, c); \
        EXPECT_EQ(WebFilterOperation::FilterType##Type, op.type()); \
        EXPECT_EQ(a, op.dropShadowOffset()); \
        EXPECT_EQ(b, op.amount()); \
        EXPECT_EQ(c, op.dropShadowColor()); \
        \
        WebFilterOperation op2 = WebFilterOperation::createEmptyFilter(); \
        op2.setType(WebFilterOperation::FilterType##Type); \
        \
        EXPECT_NE(a, op2.dropShadowOffset()); \
        EXPECT_NE(b, op2.amount()); \
        EXPECT_NE(c, op2.dropShadowColor()); \
        \
        op2.setDropShadowOffset(a); \
        op2.setAmount(b); \
        op2.setDropShadowColor(c); \
        \
        EXPECT_EQ(WebFilterOperation::FilterType##Type, op2.type()); \
        EXPECT_EQ(a, op2.dropShadowOffset()); \
        EXPECT_EQ(b, op2.amount()); \
        EXPECT_EQ(c, op2.dropShadowColor()); \
    }

#define SAVE_RESTORE_MATRIX(Type, a) \
    { \
        WebFilterOperation op = WebFilterOperation::create##Type##Filter(a); \
        EXPECT_EQ(WebFilterOperation::FilterType##Type, op.type());     \
        for (unsigned i = 0; i < 20; ++i) \
            EXPECT_EQ(a[i], op.matrix()[i]); \
        \
        WebFilterOperation op2 = WebFilterOperation::createEmptyFilter(); \
        op2.setType(WebFilterOperation::FilterType##Type); \
        \
        for (unsigned i = 0; i < 20; ++i) \
            EXPECT_NE(a[i], op2.matrix()[i]); \
        \
        op2.setMatrix(a); \
        \
        EXPECT_EQ(WebFilterOperation::FilterType##Type, op2.type()); \
        for (unsigned i = 0; i < 20; ++i) \
            EXPECT_EQ(a[i], op.matrix()[i]); \
    }

#define SAVE_RESTORE_ZOOMRECT_AMOUNT(Type, a, b) \
    { \
        WebFilterOperation op = WebFilterOperation::create##Type##Filter(a, b); \
        EXPECT_EQ(WebFilterOperation::FilterType##Type, op.type()); \
        EXPECT_EQ(a, op.zoomRect()); \
        EXPECT_EQ(b, op.amount()); \
        \
        WebFilterOperation op2 = WebFilterOperation::createEmptyFilter(); \
        op2.setType(WebFilterOperation::FilterType##Type); \
        \
        EXPECT_NE(a, op2.zoomRect()); \
        EXPECT_NE(b, op2.amount()); \
        \
        op2.setZoomRect(a); \
        op2.setAmount(b); \
        \
        EXPECT_EQ(WebFilterOperation::FilterType##Type, op2.type()); \
        EXPECT_EQ(a, op2.zoomRect()); \
        EXPECT_EQ(b, op2.amount()); \
    }


TEST(WebFilterOperationsTest, saveAndRestore)
{
    SAVE_RESTORE_AMOUNT(Grayscale, 0.6f);
    SAVE_RESTORE_AMOUNT(Sepia, 0.6f);
    SAVE_RESTORE_AMOUNT(Saturate, 0.6f);
    SAVE_RESTORE_AMOUNT(HueRotate, 0.6f);
    SAVE_RESTORE_AMOUNT(Invert, 0.6f);
    SAVE_RESTORE_AMOUNT(Brightness, 0.6f);
    SAVE_RESTORE_AMOUNT(Contrast, 0.6f);
    SAVE_RESTORE_AMOUNT(Opacity, 0.6f);
    SAVE_RESTORE_AMOUNT(Blur, 0.6f);
    SAVE_RESTORE_OFFSET_AMOUNT_COLOR(DropShadow, WebPoint(3, 4), 0.4f, 0xffffff00);

    SkScalar matrix[20] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
    SAVE_RESTORE_MATRIX(ColorMatrix, matrix);

    SAVE_RESTORE_ZOOMRECT_AMOUNT(Zoom, WebRect(20, 19, 18, 17), 32);
}

}

