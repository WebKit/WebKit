//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ScenicWindow.cpp:
//    Implements methods from ScenicWindow
//

#include "util/fuchsia/ScenicWindow.h"

#include <fuchsia/element/cpp/fidl.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/interface_ptr.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/zx/channel.h>
#include <zircon/status.h>

namespace
{

async::Loop *GetDefaultLoop()
{
    static async::Loop *defaultLoop = new async::Loop(&kAsyncLoopConfigNeverAttachToThread);
    return defaultLoop;
}

template <typename Interface>
fidl::InterfacePtr<Interface> ConnectToService(async_dispatcher_t *dispatcher)
{
    fidl::InterfacePtr<Interface> result;
    fidl::InterfaceRequest<Interface> request = result.NewRequest(dispatcher);
    ASSERT(request.is_valid());
    fdio_service_connect_by_name(Interface::Name_, request.TakeChannel().release());
    return result;
}

}  // namespace

// TODO: http://anglebug.com/42050005 - Implement using fuchsia.element.GraphicalPresenter to pass a
// ViewCreationToken to Fuchsia Flatland.
ScenicWindow::ScenicWindow()
    : mLoop(GetDefaultLoop()),
      mPresenter(ConnectToService<fuchsia::element::GraphicalPresenter>(mLoop->dispatcher()))
{}

ScenicWindow::~ScenicWindow()
{
    destroy();
}

bool ScenicWindow::initializeImpl(const std::string &name, int width, int height)
{
    return true;
}

void ScenicWindow::disableErrorMessageDialog() {}

void ScenicWindow::destroy()
{
    mFuchsiaEGLWindow.reset();
}

void ScenicWindow::resetNativeWindow()
{
    UNIMPLEMENTED();
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
    fuchsia_egl_window_resize(mFuchsiaEGLWindow.get(), width, height);
    return true;
}

void ScenicWindow::setVisible(bool isVisible) {}

void ScenicWindow::signalTestEvent() {}

void ScenicWindow::present()
{
    UNIMPLEMENTED();
}

void ScenicWindow::updateViewSize()
{
    UNIMPLEMENTED();
}

// static
OSWindow *OSWindow::New()
{
    return new ScenicWindow;
}
