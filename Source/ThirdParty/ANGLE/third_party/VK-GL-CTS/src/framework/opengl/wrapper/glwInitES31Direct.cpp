/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL Utilities
 * ---------------------------------------------
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
 * \brief Function table initialization.
 *//*--------------------------------------------------------------------*/

#include "glwInitES31Direct.hpp"

#include <stdexcept>

#if defined(DEQP_GLES31_DIRECT_LINK)
#if (DE_OS == DE_OS_IOS)
#include <OpenGLES/ES3/gl.h>
#else
#include <GLES3/gl31.h>
#endif
#endif

namespace glw
{

void initES31Direct(Functions *gl)
{
#if defined(DEQP_GLES31_DIRECT_LINK)
#include "glwInitES31Direct.inl"
#else
    DE_UNREF(gl);
    throw std::runtime_error("Binaries were compiled without ES31 direct loading support");
#endif
}

} // namespace glw
