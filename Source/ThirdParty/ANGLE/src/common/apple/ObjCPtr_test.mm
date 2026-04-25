//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
//   of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ObjCPtr_test.cpp:
//   Test for functionality in ObjCPtr.h

#import "common/apple/ObjCPtr.h"
#import <Metal/Metal.h>
#import "gtest/gtest.h"

namespace
{
using namespace angle;

// This explains why the implementation has ObjCPtr<U> &&other constructor/operator=
// as opposed to ObjCPtr &&other ones.
TEST(ObjCPtrTest, ImplementationDetailExplanation)
{
    // For Obj-C interfaces we would like the client to use ObjCPtr<T> form, similar
    // to any C++ smart pointer.
    MTLStencilDescriptor *rawDesc             = nil;
    ObjCPtr<MTLStencilDescriptor> stencilDesc = adoptObjCPtr(rawDesc);

    // For Obj-C protocols we would like the client to use ObjCPtr<id<P>> form,
    // which resembles the normal smart pointer form textually:
    id<MTLDevice> rawDevice       = nil;
    ObjCPtr<id<MTLDevice>> device = adoptObjCPtr(rawDevice);

    // adoptObjCPtr for Obj-C interface types works as expected:
    {
        auto result = adoptObjCPtr(rawDesc);
        static_assert(std::is_same_v<ObjCPtr<MTLStencilDescriptor>, decltype(result)>);
    }

    // adoptObjCPtr for protocols does not:
    {
        auto result = adoptObjCPtr(rawDevice);
        static_assert(!std::is_same_v<ObjCPtr<id<MTLDevice>>, decltype(result)>);
        static_assert(
            std::is_same_v<ObjCPtr<std::remove_pointer_t<id<MTLDevice>>>, decltype(result)>);
    }
}

TEST(ObjCPtrTest, Comparison)
{
    ObjCPtr<MTLStencilDescriptor> a = adoptObjCPtr([[MTLStencilDescriptor alloc] init]);
    ObjCPtr<MTLStencilDescriptor> b;
    EXPECT_TRUE(a == a);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a != nullptr);
    EXPECT_TRUE(b == nullptr);
    EXPECT_TRUE(nullptr != a);
    EXPECT_TRUE(nullptr == b);
    EXPECT_TRUE(a != nil);
    EXPECT_TRUE(b == nil);
    EXPECT_TRUE(nil != a);
    EXPECT_TRUE(nil == b);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!b);
}

TEST(ObjCPtrTest, Copy)
{
    ObjCPtr<MTLStencilDescriptor> a = adoptObjCPtr([[MTLStencilDescriptor alloc] init]);
    ObjCPtr<MTLStencilDescriptor> b = a;
    EXPECT_EQ(a, b);
    EXPECT_NE(a, nullptr);
    a = {};
    EXPECT_NE(a, b);
}

TEST(ObjCPtrTest, LeakObject)
{
    ObjCPtr<MTLStencilDescriptor> a = adoptObjCPtr([[MTLStencilDescriptor alloc] init]);
    EXPECT_NE(a, nullptr);
    auto rawA = a.leakObject();
    EXPECT_EQ(a, nullptr);
    EXPECT_EQ(a.leakObject(), nullptr);
    a = adoptObjCPtr(rawA);
}

TEST(ObjCPtrTest, SelfAssignment)
{
    ObjCPtr a = adoptObjCPtr([[MTLStencilDescriptor alloc] init]);
    auto rawA = a.get();
    auto &r   = a;
    a         = r;
    EXPECT_EQ(a, rawA);
    a = std::move(r);
    EXPECT_EQ(a, rawA);
}

}  // namespace
