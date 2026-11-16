/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Android Native Activity.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidNativeActivity.hpp"
#include "deMemory.h"

DE_BEGIN_EXTERN_C

static void onStartCallback(ANativeActivity *activity)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onStart();
}

static void onResumeCallback(ANativeActivity *activity)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onResume();
}

static void *onSaveInstanceStateCallback(ANativeActivity *activity, size_t *outSize)
{
    return static_cast<tcu::Android::NativeActivity *>(activity->instance)->onSaveInstanceState(outSize);
}

static void onPauseCallback(ANativeActivity *activity)
{
    return static_cast<tcu::Android::NativeActivity *>(activity->instance)->onPause();
}

static void onStopCallback(ANativeActivity *activity)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onStop();
}

static void onDestroyCallback(ANativeActivity *activity)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onDestroy();
}

static void onWindowFocusChangedCallback(ANativeActivity *activity, int hasFocus)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onWindowFocusChanged(hasFocus);
}

static void onNativeWindowCreatedCallback(ANativeActivity *activity, ANativeWindow *window)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onNativeWindowCreated(window);
}

static void onNativeWindowResizedCallback(ANativeActivity *activity, ANativeWindow *window)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onNativeWindowResized(window);
}

static void onNativeWindowRedrawNeededCallback(ANativeActivity *activity, ANativeWindow *window)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onNativeWindowRedrawNeeded(window);
}

static void onNativeWindowDestroyedCallback(ANativeActivity *activity, ANativeWindow *window)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onNativeWindowDestroyed(window);
}

static void onInputQueueCreatedCallback(ANativeActivity *activity, AInputQueue *queue)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onInputQueueCreated(queue);
}

static void onInputQueueDestroyedCallback(ANativeActivity *activity, AInputQueue *queue)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onInputQueueDestroyed(queue);
}

static void onContentRectChangedCallback(ANativeActivity *activity, const ARect *rect)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onContentRectChanged(rect);
}

static void onConfigurationChangedCallback(ANativeActivity *activity)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onConfigurationChanged();
}

static void onLowMemoryCallback(ANativeActivity *activity)
{
    static_cast<tcu::Android::NativeActivity *>(activity->instance)->onLowMemory();
}

DE_END_EXTERN_C

namespace tcu
{
namespace Android
{

NativeActivity::NativeActivity(ANativeActivity *activity) : m_activity(activity)
{
    if (activity)
    {
        activity->instance                              = (void *)this;
        activity->callbacks->onStart                    = onStartCallback;
        activity->callbacks->onResume                   = onResumeCallback;
        activity->callbacks->onSaveInstanceState        = onSaveInstanceStateCallback;
        activity->callbacks->onPause                    = onPauseCallback;
        activity->callbacks->onStop                     = onStopCallback;
        activity->callbacks->onDestroy                  = onDestroyCallback;
        activity->callbacks->onWindowFocusChanged       = onWindowFocusChangedCallback;
        activity->callbacks->onNativeWindowCreated      = onNativeWindowCreatedCallback;
        activity->callbacks->onNativeWindowResized      = onNativeWindowResizedCallback;
        activity->callbacks->onNativeWindowRedrawNeeded = onNativeWindowRedrawNeededCallback;
        activity->callbacks->onNativeWindowDestroyed    = onNativeWindowDestroyedCallback;
        activity->callbacks->onInputQueueCreated        = onInputQueueCreatedCallback;
        activity->callbacks->onInputQueueDestroyed      = onInputQueueDestroyedCallback;
        activity->callbacks->onContentRectChanged       = onContentRectChangedCallback;
        activity->callbacks->onConfigurationChanged     = onConfigurationChangedCallback;
        activity->callbacks->onLowMemory                = onLowMemoryCallback;
    }
}

NativeActivity::~NativeActivity(void)
{
}

void NativeActivity::onStart(void)
{
}

void NativeActivity::onResume(void)
{
}

void *NativeActivity::onSaveInstanceState(size_t *outSize)
{
    *outSize = 0;
    return nullptr;
}

void NativeActivity::onPause(void)
{
}

void NativeActivity::onStop(void)
{
}

void NativeActivity::onDestroy(void)
{
}

void NativeActivity::onWindowFocusChanged(int hasFocus)
{
    DE_UNREF(hasFocus);
}

void NativeActivity::onNativeWindowCreated(ANativeWindow *window)
{
    DE_UNREF(window);
}

void NativeActivity::onNativeWindowResized(ANativeWindow *window)
{
    DE_UNREF(window);
}

void NativeActivity::onNativeWindowRedrawNeeded(ANativeWindow *window)
{
    DE_UNREF(window);
}

void NativeActivity::onNativeWindowDestroyed(ANativeWindow *window)
{
    DE_UNREF(window);
}

void NativeActivity::onInputQueueCreated(AInputQueue *queue)
{
    DE_UNREF(queue);
}

void NativeActivity::onInputQueueDestroyed(AInputQueue *queue)
{
    DE_UNREF(queue);
}

void NativeActivity::onContentRectChanged(const ARect *rect)
{
    DE_UNREF(rect);
}

void NativeActivity::onConfigurationChanged(void)
{
}

void NativeActivity::onLowMemory(void)
{
}

void NativeActivity::finish(void)
{
    ANativeActivity_finish(m_activity);
}

} // namespace Android
} // namespace tcu
