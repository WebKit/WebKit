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
#include <vulkan/vulkan.h>
#include <zircon/status.h>

#include "common/debug.h"

namespace
{

uint32_t GetImagePipeSwapchainLayerImplementationVersion()
{
    uint32_t numInstanceLayers = 0;
    VkResult result            = vkEnumerateInstanceLayerProperties(&numInstanceLayers, nullptr);
    if (result != VK_SUCCESS)
    {
        return 0u;
    }

    std::vector<VkLayerProperties> instanceLayers(numInstanceLayers);
    result = vkEnumerateInstanceLayerProperties(&numInstanceLayers, instanceLayers.data());
    if (result != VK_SUCCESS)
    {
        return 0u;
    }

    uint32_t imagePipeSwapchainImplementationVersion = 0;
    const std::string layerName                      = "VK_LAYER_FUCHSIA_imagepipe_swapchain";
    for (const VkLayerProperties &layerProperty : instanceLayers)
    {
        if (layerName.compare(layerProperty.layerName) != 0)
            continue;
        imagePipeSwapchainImplementationVersion = layerProperty.implementationVersion;
        break;
    }

    ASSERT(imagePipeSwapchainImplementationVersion > 0u);
    return imagePipeSwapchainImplementationVersion;
}

async::Loop *GetDefaultLoop()
{
    static async::Loop *defaultLoop = new async::Loop(&kAsyncLoopConfigAttachToCurrentThread);
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
    zx_handle_t imagePipeHandle = 0;
    uint32_t imagePipeId        = mScenicSession.AllocResourceId();
    if (GetImagePipeSwapchainLayerImplementationVersion() > 1u)
    {
        fuchsia::images::ImagePipe2Ptr imagePipe;
        mScenicSession.Enqueue(scenic::NewCreateImagePipe2Cmd(imagePipeId, imagePipe.NewRequest()));
        imagePipeHandle = imagePipe.Unbind().TakeChannel().release();
    }
    else
    {
        fuchsia::images::ImagePipePtr imagePipe;
        mScenicSession.Enqueue(scenic::NewCreateImagePipeCmd(imagePipeId, imagePipe.NewRequest()));
        imagePipeHandle = imagePipe.Unbind().TakeChannel().release();
    }
    mMaterial.SetTexture(imagePipeId);
    mScenicSession.ReleaseResource(imagePipeId);
    mScenicSession.Present(0, [](fuchsia::images::PresentationInfo info) {});

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
