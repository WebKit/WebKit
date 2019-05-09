//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ScenicWindow.h:
//    Subclasses OSWindow for Fuchsia's Scenic compositor.
//

#ifndef UTIL_FUCHSIA_SCENIC_WINDOW_H
#define UTIL_FUCHSIA_SCENIC_WINDOW_H

#include <fuchsia/ui/policy/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <fuchsia_egl.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>
#include <zircon/types.h>
#include <string>

#include "util/OSWindow.h"
#include "util/util_export.h"

struct FuchsiaEGLWindowDeleter
{
    void operator()(fuchsia_egl_window *eglWindow) { fuchsia_egl_window_destroy(eglWindow); }
};

class ANGLE_UTIL_EXPORT ScenicWindow : public OSWindow
{
  public:
    ScenicWindow();
    ~ScenicWindow();

    // OSWindow:
    bool initialize(const std::string &name, size_t width, size_t height) override;
    void destroy() override;
    void resetNativeWindow() override;
    EGLNativeWindowType getNativeWindow() const override;
    EGLNativeDisplayType getNativeDisplay() const override;
    void messageLoop() override;
    void setMousePosition(int x, int y) override;
    bool setPosition(int x, int y) override;
    bool resize(int width, int height) override;
    void setVisible(bool isVisible) override;
    void signalTestEvent() override;

    // FIDL callbacks:
    void OnScenicEvents(std::vector<fuchsia::ui::scenic::Event> events);
    void OnScenicError(zx_status_t status);

  private:
    // Default message loop.
    async::Loop *mLoop;

    // System services.
    zx::channel mServiceRoot;
    fuchsia::ui::scenic::ScenicPtr mScenic;
    fuchsia::ui::policy::PresenterPtr mPresenter;

    // Scenic session & resources.
    scenic::Session mScenicSession;
    scenic::ShapeNode mShape;
    scenic::Material mMaterial;

    // Scenic view.
    std::unique_ptr<scenic::View> mView;

    // EGL native window.
    std::unique_ptr<fuchsia_egl_window, FuchsiaEGLWindowDeleter> mFuchsiaEGLWindow;
};

#endif  // UTIL_FUCHSIA_SCENIC_WINDOW_H
