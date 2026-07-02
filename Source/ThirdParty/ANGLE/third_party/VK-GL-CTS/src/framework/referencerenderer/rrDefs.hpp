#ifndef _RRDEFS_HPP
#define _RRDEFS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Reference Renderer
 * -----------------------------------------------
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
 * \brief Reference renderer base definitions.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"

/*--------------------------------------------------------------------*//*!
 * \brief Reference renderer
 *//*--------------------------------------------------------------------*/
namespace rr
{

enum FaceType
{
    FACETYPE_FRONT = 0,
    FACETYPE_BACK,

    FACETYPE_LAST
};

enum IndexType
{
    INDEXTYPE_UINT8,
    INDEXTYPE_UINT16,
    INDEXTYPE_UINT32,

    INDEXTYPE_LAST
};

enum ProvokingVertex
{
    PROVOKINGVERTEX_FIRST = 0,
    PROVOKINGVERTEX_LAST, // \note valid value, "last vertex", not last of enum
};

// \todo [pyry]
//  - subpixel bits

} // namespace rr

#endif // _RRDEFS_HPP
