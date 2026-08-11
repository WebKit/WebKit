#ifndef _TCULNX_HPP
#define _TCULNX_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2017 The Android Open Source Project
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
 * \brief Linux utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "deMutex.hpp"

namespace tcu
{
// This namespace should be named 'linux', however some compilers still
// define obsolete 'linux' macro alongside '__linux__'
namespace lnx
{
enum
{
    DEFAULT_WINDOW_WIDTH  = 400,
    DEFAULT_WINDOW_HEIGHT = 300
};

class EventState
{
public:
    EventState(void);
    virtual ~EventState(void);
    void setQuitFlag(bool quit);
    bool getQuitFlag(void);

protected:
    de::Mutex m_mutex;
    bool m_quit;

private:
    EventState(const EventState &);
    EventState &operator=(const EventState &);
};

} // namespace lnx
} // namespace tcu

#endif // _TCULNX_HPP
