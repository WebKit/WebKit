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

#include "glwInitFunctions.hpp"
#include "deSTLUtil.hpp"

#include <string>
#include <set>

namespace glw
{

// \todo [2014-03-19 pyry] Replace this with more generic system based on upstream XML spec desc.

void initES20(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitES20.inl"
}

void initES30(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitES30.inl"
}

void initES31(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitES31.inl"
}

void initES32(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitES32.inl"
}

void initGL30Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL30.inl"
}

void initGL31Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL31.inl"
}

void initGL32Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL32.inl"
}

void initGL33Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL33.inl"
}

void initGL40Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL40.inl"
}

void initGL41Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL41.inl"
}

void initGL42Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL42.inl"
}

void initGL43Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL43.inl"
}

void initGL44Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL44.inl"
}

void initGL45Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL45.inl"
}

void initGL46Core(Functions *gl, const FunctionLoader *loader)
{
#include "glwInitGL46.inl"
}

void initExtensionsGL(Functions *gl, const FunctionLoader *loader, int numExtensions, const char *const *extensions)
{
    using std::set;
    using std::string;

    const set<string> extSet(extensions, extensions + numExtensions);

#include "glwInitExtGL.inl"
}

void initExtensionsES(Functions *gl, const FunctionLoader *loader, int numExtensions, const char *const *extensions)
{
    using std::set;
    using std::string;

    const set<string> extSet(extensions, extensions + numExtensions);

#include "glwInitExtES.inl"
}

} // namespace glw
