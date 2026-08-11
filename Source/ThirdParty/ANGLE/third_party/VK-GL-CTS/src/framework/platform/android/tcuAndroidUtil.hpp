#ifndef _TCUANDROIDUTIL_HPP
#define _TCUANDROIDUTIL_HPP
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
 * \brief Android utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuCommandLine.hpp"

#include <string>
#include <ostream>

#include <android/native_activity.h>

namespace tcu
{
namespace Android
{

enum ScreenOrientation
{
    SCREEN_ORIENTATION_UNSPECIFIED       = 0xffffffff,
    SCREEN_ORIENTATION_LANDSCAPE         = 0x00000000,
    SCREEN_ORIENTATION_PORTRAIT          = 0x00000001,
    SCREEN_ORIENTATION_REVERSE_LANDSCAPE = 0x00000008,
    SCREEN_ORIENTATION_REVERSE_PORTRAIT  = 0x00000009
};

struct CustomOrientation
{
    bool enabled = false;
    void set(bool enable)
    {
        enabled = enable;
    }
    bool isEnabled() const
    {
        return enabled;
    }

    static CustomOrientation &instance()
    {
        static CustomOrientation instance;
        return instance;
    }
};

std::string getIntentStringExtra(ANativeActivity *activity, const char *name);
void setRequestedOrientation(ANativeActivity *activity, ScreenOrientation orientation);

ScreenOrientation mapScreenRotation(ScreenRotation rotation);

void PixelCopy(ANativeActivity *activity, const char *path);

void describePlatform(ANativeActivity *activity, std::ostream &dst);

size_t getTotalAndroidSystemMemory(ANativeActivity *activity);
} // namespace Android
} // namespace tcu

#endif // _TCUANDROIDUTIL_HPP
