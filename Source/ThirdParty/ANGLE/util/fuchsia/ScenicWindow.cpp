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
    static async::Loop *defaultLoop = new async::Loop(&kAsyncLoopConfigAttachToThread);
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
fidl::InterfacePtr<Interface> ConnectToService(zx_handle_t serviceRoot)
{
    fidl::InterfacePtr<Interface> result;
    ConnectToService(serviceRoot, result.NewRequest());
    return result;
}

}  // namespace

ScenicWindow::ScenicWindow()
    : mLoop(GetDefaultLoop()),
      mServiceRoot(ConnectToServiceRoot()),
      mScenic(ConnectToService<fuchsia::ui::scenic::Scenic>(mServiceRoot.get())),
      mPresenter(ConnectToService<fuchsia::ui::policy::Presenter>(mServiceRoot.get())),
      mScenicSession(mScenic.get()),
      mShape(&mScenicSession),
      mMaterial(&mScenicSession)
{}

ScenicWindow::~ScenicWindow()
{
    destroy();
}

bool ScenicWindow::initialize(const std::string &name, size_t width, size_t height)
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
    mScenicSession.Present(0, [](fuchsia::images::PresentationInfo info) {});

    // Present view.
    mPresenter->PresentView(std::move(viewHolderToken), nullptr);

    mWidth  = width;
    mHeight = height;

    resetNativeWindow();

    return true;
}

void ScenicWindow::destroy()
{
    mFuchsiaEGLWindow.reset();
}

void ScenicWindow::resetNativeWindow()
{
    fuchsia::images::ImagePipePtr imagePipe;
    uint32_t imagePipeId = mScenicSession.AllocResourceId();
    mScenicSession.Enqueue(scenic::NewCreateImagePipeCmd(imagePipeId, imagePipe.NewRequest()));
    mMaterial.SetTexture(imagePipeId);
    mScenicSession.ReleaseResource(imagePipeId);
    mScenicSession.Present(0, [](fuchsia::images::PresentationInfo info) {});

    mFuchsiaEGLWindow.reset(
        fuchsia_egl_window_create(imagePipe.Unbind().TakeChannel().release(), mWidth, mHeight));
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
    mLoop->Run(zx::deadline_after({}), true /* once */);
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

void ScenicWindow::OnScenicEvents(std::vector<fuchsia::ui::scenic::Event> events)
{
    UNIMPLEMENTED();
}

void ScenicWindow::OnScenicError(zx_status_t status)
{
    WARN() << "OnScenicError: " << zx_status_get_string(status);
}

// static
OSWindow *OSWindow::New()
{
    return new ScenicWindow;
}
