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
 * \brief Surface class.
 *//*--------------------------------------------------------------------*/

#include "tcuSurface.hpp"

using namespace tcu;

Surface::Surface(void) : m_width(0), m_height(0)
{
}

Surface::Surface(int width, int height) : m_width(width), m_height(height), m_pixels(width * height)
{
}

Surface::~Surface()
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Resize surface.
 *
 * Old contents are not preserved.
 * \param width        New width.
 * \param height    New height.
 *//*--------------------------------------------------------------------*/
void Surface::setSize(int width, int height)
{
    m_width  = width;
    m_height = height;
    m_pixels.setStorage(width * height);
}
