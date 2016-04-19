//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/Config.h"
#include "libANGLE/Data.h"
#include "libANGLE/State.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/FramebufferImpl_mock.h"
#include "libANGLE/renderer/SurfaceImpl.h"

using namespace rx;

namespace
{

class MockSurfaceImpl : public rx::SurfaceImpl
{
  public:
    virtual ~MockSurfaceImpl() { destroy(); }

    MOCK_METHOD0(initialize, egl::Error());
    MOCK_METHOD1(createDefaultFramebuffer,
                 rx::FramebufferImpl *(const gl::Framebuffer::Data &data));
    MOCK_METHOD0(swap, egl::Error());
    MOCK_METHOD4(postSubBuffer, egl::Error(EGLint, EGLint, EGLint, EGLint));
    MOCK_METHOD2(querySurfacePointerANGLE, egl::Error(EGLint, void**));
    MOCK_METHOD2(bindTexImage, egl::Error(gl::Texture*, EGLint));
    MOCK_METHOD1(releaseTexImage, egl::Error(EGLint));
    MOCK_METHOD1(setSwapInterval, void(EGLint));
    MOCK_CONST_METHOD0(getWidth, EGLint());
    MOCK_CONST_METHOD0(getHeight, EGLint());
    MOCK_CONST_METHOD0(isPostSubBufferSupported, EGLint(void));
    MOCK_CONST_METHOD0(getSwapBehavior, EGLint(void));
    MOCK_METHOD2(getAttachmentRenderTarget, gl::Error(const gl::FramebufferAttachment::Target &, rx::FramebufferAttachmentRenderTarget **));

    MOCK_METHOD0(destroy, void());
};

TEST(SurfaceTest, DestructionDeletesImpl)
{
    MockFramebufferImpl *framebuffer = new MockFramebufferImpl;

    MockSurfaceImpl *impl = new MockSurfaceImpl;
    EXPECT_CALL(*impl, getSwapBehavior());
    EXPECT_CALL(*impl, createDefaultFramebuffer(testing::_)).WillOnce(testing::Return(framebuffer));

    egl::Config config;
    egl::Surface *surface = new egl::Surface(impl, EGL_WINDOW_BIT, &config, egl::AttributeMap());

    EXPECT_CALL(*framebuffer, destroy()).Times(1).RetiresOnSaturation();
    EXPECT_CALL(*impl, destroy()).Times(1).RetiresOnSaturation();

    surface->onDestroy();

    // Only needed because the mock is leaked if bugs are present,
    // which logs an error, but does not cause the test to fail.
    // Ordinarily mocks are verified when destroyed.
    testing::Mock::VerifyAndClear(impl);
}

} // namespace
