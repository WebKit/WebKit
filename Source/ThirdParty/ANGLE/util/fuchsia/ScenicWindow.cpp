//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ScenicWindow.cpp:
//    Implements methods from ScenicWindow
//

#include "util/fuchsia/ScenicWindow.h"

#include <fuchsia/images/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/interface_ptr.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <lib/zx/channel.h>
#include <zircon/status.h>

#include "common/debug.h"

namespace
{

async::Loop *GetDefaultLoop()
{
    static async::Loop *defaultLoop = new async::Loop(&kAsyncLoopConfigNeverAttachToThread);
    return defaultLoop;
}

zx::channel ConnectToServiceRoot()
{
    zx::channel clientChannel;
    zx::channel serverChannel;
    zx_status_t result = zx::channel::create(0, &clientChannel, &serverChannel);
    ASSERT(result == ZX_OK);
    result = fdio_service_connect("/svc/.", serverChannel.release());
    ASSERT(result == ZX_OK);
    return clientChannel;
}

template <typename Interface>
zx_status_t ConnectToService(zx_handle_t serviceRoot, fidl::InterfaceRequest<Interface> request)
{
    ASSERT(request.is_valid());
    return fdio_service_connect_at(serviceRoot, Interface::Name_, request.TakeChannel().release());
}

template <typename Interface>
fidl::InterfacePtr<Interface> ConnectToService(zx_handle_t serviceRoot,
                                               async_dispatcher_t *dispatcher)
{
    fidl::InterfacePtr<Interface> result;
    ConnectToService(serviceRoot, result.NewRequest(dispatcher));
    return result;
}

}  // namespace

ScenicWindow::ScenicWindow()
    : mLoop(GetDefaultLoop()),
      mServiceRoot(ConnectToServiceRoot()),
      mScenic(
          ConnectToService<fuchsia::ui::scenic::Scenic>(mServiceRoot.get(), mLoop->dispatcher())),
      mPresenter(ConnectToService<fuchsia::ui::policy::Presenter>(mServiceRoot.get(),
                                                                  mLoop->dispatcher())),
      mScenicSession(mScenic.get(), mLoop->dispatcher()),
      mShape(&mScenicSession),
      mMaterial(&mScenicSession)
{
    mScenicSession.set_error_handler(fit::bind_member(this, &ScenicWindow::onScenicError));
    mScenicSession.set_on_frame_presented_handler(
        fit::bind_member(this, &ScenicWindow::onFramePresented));
}

ScenicWindow::~ScenicWindow()
{
    destroy();
}

bool ScenicWindow::initialize(const std::string &name, int width, int height)
{
    // Set up scenic resources.
    mShape.SetShape(scenic::Rectangle(&mScenicSession, width, height));
    mShape.SetMaterial(mMaterial);

    fuchsia::ui::views::ViewToken viewToken;
    fuchsia::ui::views::ViewHolderToken viewHolderToken;
    std::tie(viewToken, viewHolderToken) = scenic::NewViewTokenPair();

    // Create view.
    mView = std::make_unique<scenic::View>(&mScenicSession, std::move(viewToken), name);
    mView->AddChild(mShape);

    // Present view.
    mPresenter->PresentOrReplaceView(std::move(viewHolderToken), nullptr);

    mWidth  = width;
    mHeight = height;

    resetNativeWindow();

    return true;
}

void ScenicWindow::destroy()
{
    while (mInFlightPresents != 0 && !mLostSession)
    {
        mLoop->ResetQuit();
        mLoop->Run();
    }

    ASSERT(mInFlightPresents == 0 || mLostSession);

    mFuchsiaEGLWindow.reset();
}

void ScenicWindow::resetNativeWindow()
{
    fuchsia::images::ImagePipe2Ptr imagePipe;
    uint32_t imagePipeId = mScenicSession.AllocResourceId();
    mScenicSession.Enqueue(
        scenic::NewCreateImagePipe2Cmd(imagePipeId, imagePipe.NewRequest(mLoop->dispatcher())));
    zx_handle_t imagePipeHandle = imagePipe.Unbind().TakeChannel().release();

    mMaterial.SetTexture(imagePipeId);
    mScenicSession.ReleaseResource(imagePipeId);
    present();

    mFuchsiaEGLWindow.reset(fuchsia_egl_window_create(imagePipeHandle, mWidth, mHeight));
}

EGLNativeWindowType ScenicWindow::getNativeWindow() const
{
    return reinterpret_cast<EGLNativeWindowType>(mFuchsiaEGLWindow.get());
}

EGLNativeDisplayType ScenicWindow::getNativeDisplay() const
{
    return EGL_DEFAULT_DISPLAY;
}

void ScenicWindow::messageLoop()
{
    mLoop->ResetQuit();
    mLoop->RunUntilIdle();
}

void ScenicWindow::setMousePosition(int x, int y)
{
    UNIMPLEMENTED();
}

bool ScenicWindow::setPosition(int x, int y)
{
    UNIMPLEMENTED();
    return false;
}

bool ScenicWindow::resize(int width, int height)
{
    mWidth  = width;
    mHeight = height;

    fuchsia_egl_window_resize(mFuchsiaEGLWindow.get(), width, height);

    return true;
}

void ScenicWindow::setVisible(bool isVisible) {}

void ScenicWindow::signalTestEvent() {}

void ScenicWindow::present()
{
    while (mInFlightPresents >= kMaxInFlightPresents && !mLostSession)
    {
        mLoop->ResetQuit();
        mLoop->Run();
    }

    if (mLostSession)
    {
        return;
    }

    ASSERT(mInFlightPresents < kMaxInFlightPresents);

    ++mInFlightPresents;
    mScenicSession.Present2(0, 0, [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});
}

void ScenicWindow::onFramePresented(fuchsia::scenic::scheduling::FramePresentedInfo info)
{
    mInFlightPresents -= info.presentation_infos.size();
    ASSERT(mInFlightPresents >= 0);
    mLoop->Quit();
}

void ScenicWindow::onScenicEvents(std::vector<fuchsia::ui::scenic::Event> events)
{
    UNIMPLEMENTED();
}

void ScenicWindow::onScenicError(zx_status_t status)
{
    WARN() << "OnScenicError: " << zx_status_get_string(status);
    mLostSession = true;
    mLoop->Quit();
}

// static
OSWindow *OSWindow::New()
{
    return new ScenicWindow;
}
