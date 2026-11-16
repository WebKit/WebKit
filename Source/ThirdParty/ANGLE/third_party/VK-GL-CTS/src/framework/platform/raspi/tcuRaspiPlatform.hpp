#ifndef _TCURASPIPLATFORM_HPP
#define _TCURASPIPLATFORM_HPP
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
 * \brief Raspberry PI platform.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuPlatform.hpp"
#include "egluPlatform.hpp"
#include "gluPlatform.hpp"

#include <bcm_host.h>

namespace tcu
{
namespace rpi
{

class Platform : public tcu::Platform, private eglu::Platform, private glu::Platform
{
public:
    Platform(void);
    virtual ~Platform(void);

    virtual const glu::Platform &getGLPlatform(void) const
    {
        return static_cast<const glu::Platform &>(*this);
    }
    virtual const eglu::Platform &getEGLPlatform(void) const
    {
        return static_cast<const eglu::Platform &>(*this);
    }
};

} // namespace rpi
} // namespace tcu

#endif // _TCURASPIPLATFORM_HPP
