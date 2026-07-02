#ifndef _TCUANDROIDNATIVEACTIVITY_HPP
#define _TCUANDROIDNATIVEACTIVITY_HPP
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
 * \brief C++ wrapper for Android NativeActivity.
 *
 * To use this wrapper, implement your NativeActivity by extending this
 * class and create NativeActivity in ANativeActivity_onCreate().
 *
 * tcu::NativeActivity constructor will fill activity->callbacks and
 * set instance pointer.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

#include <android/native_activity.h>

namespace tcu
{
namespace Android
{

class NativeActivity
{
public:
    NativeActivity(ANativeActivity *activity);
    virtual ~NativeActivity(void);

    virtual void onStart(void);
    virtual void onResume(void);
    virtual void *onSaveInstanceState(size_t *outSize);
    virtual void onPause(void);
    virtual void onStop(void);
    virtual void onDestroy(void);

    virtual void onWindowFocusChanged(int hasFocus);
    virtual void onNativeWindowCreated(ANativeWindow *window);
    virtual void onNativeWindowResized(ANativeWindow *window);
    virtual void onNativeWindowRedrawNeeded(ANativeWindow *window);
    virtual void onNativeWindowDestroyed(ANativeWindow *window);

    virtual void onInputQueueCreated(AInputQueue *queue);
    virtual void onInputQueueDestroyed(AInputQueue *queue);

    virtual void onContentRectChanged(const ARect *rect);
    virtual void onConfigurationChanged(void);
    virtual void onLowMemory(void);

    ANativeActivity *getNativeActivity(void)
    {
        return m_activity;
    }
    void finish(void);

private:
    NativeActivity(const NativeActivity &other);
    NativeActivity &operator=(const NativeActivity &other);

    ANativeActivity *m_activity;
};

} // namespace Android
} // namespace tcu

#endif // _TCUANDROIDNATIVEACTIVITY_HPP
