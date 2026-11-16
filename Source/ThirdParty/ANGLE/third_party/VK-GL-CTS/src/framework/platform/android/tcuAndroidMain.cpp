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
 * \brief Android activity constructors.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#if (DE_OS != DE_OS_ANDROID)
#error Unsupported OS
#endif

#if (DE_ANDROID_API >= 9)
// Add NativeActivity entry point

#include "tcuAndroidTestActivity.hpp"

DE_BEGIN_EXTERN_C

JNIEXPORT void JNICALL createTestActivity(ANativeActivity *activity, void *savedState, size_t savedStateSize)
{
    DE_UNREF(savedState && savedStateSize);
    try
    {
        tcu::Android::TestActivity *obj = new tcu::Android::TestActivity(activity);
        DE_UNREF(obj);
    }
    catch (const std::exception &e)
    {
        tcu::die("Failed to create activity: %s", e.what());
    }
}

DE_END_EXTERN_C

#endif // DE_ANDROID_API >= 9
