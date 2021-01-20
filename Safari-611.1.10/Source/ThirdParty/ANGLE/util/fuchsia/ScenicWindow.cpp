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
    mScenicSession.set_event_handler(fit::bind_member(this, &ScenicWindow::onScenicEvents));
    mScenicSession.set_on_frame_presented_handler(
        fit::bind_member(this, &ScenicWindow::onFramePresented));
}

ScenicWindow::~ScenicWindow()
{
    destroy();
}

bool ScenicWindow::initializeImpl(const std::string &name, int width, int height)
{
    // Set up scenic resources.
    mShape.SetEventMask(fuchsia::ui::gfx::kMetricsEventMask);
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

    // Block until initial view dimensions are known.
    while (!mHasViewMetrics && !mHasViewProperties && !mLostSession)
    {
        mLoop->ResetQuit();
        mLoop->Run(zx::time::infinite(), true /* once */);
    }

    return true;
}

void ScenicWindow::disableErrorMessageDialog() {}

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

bool ScenicWindow::setOrientation(int width, int height)
{
    UNIMPLEMENTED();
    return false;
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

    mViewSizeDirty = true;

    updateViewSize();

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
    for (const auto &event : events)
    {
        if (event.is_gfx())
        {
            if (event.gfx().is_metrics())
            {
                if (event.gfx().metrics().node_id != mShape.id())
                    continue;
                onViewMetrics(event.gfx().metrics().metrics);
            }
            else if (event.gfx().is_view_properties_changed())
            {
                if (event.gfx().view_properties_changed().view_id != mView->id())
                    continue;
                onViewProperties(event.gfx().view_properties_changed().properties);
            }
        }
    }

    if (mViewSizeDirty)
    {
        updateViewSize();
    }
}

void ScenicWindow::onScenicError(zx_status_t status)
{
    WARN() << "OnScenicError: " << zx_status_get_string(status);
    mLostSession = true;
    mLoop->Quit();
}

void ScenicWindow::onViewMetrics(const fuchsia::ui::gfx::Metrics &metrics)
{
    mDisplayScaleX = metrics.scale_x;
    mDisplayScaleY = metrics.scale_y;

    mHasViewMetrics = true;
    mViewSizeDirty  = true;
}

void ScenicWindow::onViewProperties(const fuchsia::ui::gfx::ViewProperties &properties)
{
    float width = properties.bounding_box.max.x - properties.bounding_box.min.x -
                  properties.inset_from_min.x - properties.inset_from_max.x;
    float height = properties.bounding_box.max.y - properties.bounding_box.min.y -
                   properties.inset_from_min.y - properties.inset_from_max.y;

    mDisplayWidthDips  = width;
    mDisplayHeightDips = height;

    mHasViewProperties = true;
    mViewSizeDirty     = true;
}

void ScenicWindow::updateViewSize()
{
    if (!mViewSizeDirty || !mHasViewMetrics || !mHasViewProperties)
    {
        return;
    }

    mViewSizeDirty = false;

    // Surface size in pixels is
    //   (mWidth, mHeight)
    //
    // View size in pixels is
    //   (mDisplayWidthDips * mDisplayScaleX) x (mDisplayHeightDips * mDisplayScaleY)

    float widthDips  = mWidth / mDisplayScaleX;
    float heightDips = mHeight / mDisplayScaleY;

    mShape.SetShape(scenic::Rectangle(&mScenicSession, widthDips, heightDips));
    mShape.SetTranslation(0.5f * widthDips, 0.5f * heightDips, 0.f);
    present();
}

// static
OSWindow *OSWindow::New()
{
    return new ScenicWindow;
}
