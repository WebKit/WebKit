//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// AndroidWindow.cpp: Implementation of OSWindow for Android

#include "util/android/AndroidWindow.h"

#include <pthread.h>

#include "common/debug.h"
#include "util/android/third_party/android_native_app_glue.h"

namespace
{
struct android_app *sApp = nullptr;
pthread_mutex_t sInitWindowMutex;
pthread_cond_t sInitWindowCond;
bool sInitWindowDone = false;
JNIEnv *gJni         = nullptr;

// SCREEN_ORIENTATION_LANDSCAPE and SCREEN_ORIENTATION_PORTRAIT are
// available from Android API level 1
// https://developer.android.com/reference/android/app/Activity#setRequestedOrientation(int)
const int kScreenOrientationLandscape = 0;
const int kScreenOrientationPortrait  = 1;

JNIEnv *GetJniEnv()
{
    if (gJni)
        return gJni;

    sApp->activity->vm->AttachCurrentThread(&gJni, NULL);
    return gJni;
}

int SetScreenOrientation(struct android_app *app, int orientation)
{
    // Use reverse JNI to call the Java entry point that rotates the
    // display to respect width and height
    JNIEnv *jni = GetJniEnv();
    if (!jni)
    {
        WARN() << "Failed to get JNI env for screen rotation";
        return JNI_ERR;
    }

    jclass clazz       = jni->GetObjectClass(app->activity->clazz);
    jmethodID methodID = jni->GetMethodID(clazz, "setRequestedOrientation", "(I)V");
    jni->CallVoidMethod(app->activity->clazz, methodID, orientation);

    return 0;
}

}  // namespace

AndroidWindow::AndroidWindow() {}

AndroidWindow::~AndroidWindow() {}

bool AndroidWindow::initialize(const std::string &name, int width, int height)
{
    return resize(width, height);
}
void AndroidWindow::destroy() {}

void AndroidWindow::disableErrorMessageDialog() {}

void AndroidWindow::resetNativeWindow() {}

EGLNativeWindowType AndroidWindow::getNativeWindow() const
{
    // Return the entire Activity Surface for now
    // sApp->window is valid only after sInitWindowDone, which is true after initialize()
    return sApp->window;
}

EGLNativeDisplayType AndroidWindow::getNativeDisplay() const
{
    return EGL_DEFAULT_DISPLAY;
}

void AndroidWindow::messageLoop()
{
    // TODO: accumulate events in the real message loop of android_main,
    // and process them here
}

void AndroidWindow::setMousePosition(int x, int y)
{
    UNIMPLEMENTED();
}

bool AndroidWindow::setOrientation(int width, int height)
{
    // Set tests to run in correct orientation
    int32_t err = SetScreenOrientation(
        sApp, (width > height) ? kScreenOrientationLandscape : kScreenOrientationPortrait);

    return err == 0;
}
bool AndroidWindow::setPosition(int x, int y)
{
    UNIMPLEMENTED();
    return false;
}

bool AndroidWindow::resize(int width, int height)
{
    mWidth  = width;
    mHeight = height;

    // sApp->window used below is valid only after Activity Surface is created
    pthread_mutex_lock(&sInitWindowMutex);
    while (!sInitWindowDone)
    {
        pthread_cond_wait(&sInitWindowCond, &sInitWindowMutex);
    }
    pthread_mutex_unlock(&sInitWindowMutex);

    // TODO: figure out a way to set the format as well,
    // which is available only after EGLWindow initialization
    int32_t err = ANativeWindow_setBuffersGeometry(sApp->window, mWidth, mHeight, 0);
    return err == 0;
}

void AndroidWindow::setVisible(bool isVisible) {}

void AndroidWindow::signalTestEvent()
{
    UNIMPLEMENTED();
}

static void onAppCmd(struct android_app *app, int32_t cmd)
{
    switch (cmd)
    {
        case APP_CMD_INIT_WINDOW:
            pthread_mutex_lock(&sInitWindowMutex);
            sInitWindowDone = true;
            pthread_cond_broadcast(&sInitWindowCond);
            pthread_mutex_unlock(&sInitWindowMutex);
            break;
        case APP_CMD_DESTROY:
            if (gJni)
            {
                sApp->activity->vm->DetachCurrentThread();
            }
            gJni = nullptr;
            break;

            // TODO: process other commands and pass them to AndroidWindow for handling
            // TODO: figure out how to handle APP_CMD_PAUSE,
            // which should immediately halt all the rendering,
            // since Activity Surface is no longer available.
            // Currently tests crash when paused, for example, due to device changing orientation
    }
}

static int32_t onInputEvent(struct android_app *app, AInputEvent *event)
{
    // TODO: Handle input events
    return 0;  // 0 == not handled
}

void android_main(struct android_app *app)
{
    int events;
    struct android_poll_source *source;

    sApp = app;
    pthread_mutex_init(&sInitWindowMutex, nullptr);
    pthread_cond_init(&sInitWindowCond, nullptr);

    // Event handlers, invoked from source->process()
    app->onAppCmd     = onAppCmd;
    app->onInputEvent = onInputEvent;

    // Message loop, polling for events indefinitely (due to -1 timeout)
    // Must be here in order to handle APP_CMD_INIT_WINDOW event,
    // which occurs after AndroidWindow::initialize(), but before AndroidWindow::messageLoop
    while (ALooper_pollAll(-1, nullptr, &events, reinterpret_cast<void **>(&source)) >= 0)
    {
        if (source != nullptr)
        {
            source->process(app, source);
        }
    }
}

// static
OSWindow *OSWindow::New()
{
    // There should be only one live instance of AndroidWindow at a time,
    // as there is only one Activity Surface behind it.
    // Creating a new AndroidWindow each time works for ANGLETest,
    // as it destroys an old window before creating a new one.
    // TODO: use GLSurfaceView to support multiple windows
    return new AndroidWindow();
}
