#ifndef _TCUCPUWARMUP_HPP
#define _TCUCPUWARMUP_HPP
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
 * \brief CPU warm-up utility, used to counteract CPU throttling.
 *//*--------------------------------------------------------------------*/

namespace tcu
{

//! Does some unused calculations to try and get the CPU working at full speed.
void warmupCPU(void);

namespace warmupCPUInternal
{

// \note Used in an attempt to prevent compiler from doing optimizations. Not supposed to be touched elsewhere.

class Unused
{
public:
    Unused(void) : m_v(new float)
    {
    }
    ~Unused(void)
    {
        delete m_v;
    }

    volatile float *volatile m_v;
};

extern volatile Unused g_unused;

} // namespace warmupCPUInternal

} // namespace tcu

#endif // _TCUCPUWARMUP_HPP
