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

#include "config.h"

#include "cc/CCThreadTask.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WTF;
using namespace WebCore;

namespace {

class Mock {
public:
    MOCK_METHOD0(method0, void());
    MOCK_METHOD1(method1, void(int a1));
    MOCK_METHOD2(method2, void(int a1, int a2));
    MOCK_METHOD3(method3, void(int a1, int a2, int a3));
    MOCK_METHOD4(method4, void(int a1, int a2, int a3, int a4));
};

TEST(CCThreadTaskTest, runnableMethods)
{
    Mock mock;
    EXPECT_CALL(mock, method0()).Times(1);
    EXPECT_CALL(mock, method1(9)).Times(1);
    EXPECT_CALL(mock, method2(9, 8)).Times(1);
    EXPECT_CALL(mock, method3(9, 8, 7)).Times(1);
    EXPECT_CALL(mock, method4(9, 8, 7, 6)).Times(1);

    createCCThreadTask(&mock, &Mock::method0)->performTask();
    createCCThreadTask(&mock, &Mock::method1, 9)->performTask();
    createCCThreadTask(&mock, &Mock::method2, 9, 8)->performTask();
    createCCThreadTask(&mock, &Mock::method3, 9, 8, 7)->performTask();
    createCCThreadTask(&mock, &Mock::method4, 9, 8, 7, 6)->performTask();
}

} // namespace
