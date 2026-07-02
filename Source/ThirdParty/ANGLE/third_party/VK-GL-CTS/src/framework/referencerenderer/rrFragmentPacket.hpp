#ifndef _RRFRAGMENTPACKET_HPP
#define _RRFRAGMENTPACKET_HPP
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
 * \brief Fragment packet
 *//*--------------------------------------------------------------------*/

#include "rrDefs.hpp"
#include "rrGenericVector.hpp"

namespace rr
{

constexpr int NUM_FRAGMENTS_PER_PACKET = 4;

/*--------------------------------------------------------------------*//*!
 * \brief Fragment packet
 *
 * Fragment packet contains inputs and outputs for fragment shading.
 *
 * Fragment shading is always done in 2x2 blocks in order to easily estimate
 * derivates for mipmap-selection etc.
 *
 * Values packed in vectors (such as barycentrics) are in order:
 *  (x0,y0), (x1,y0), (x0,y1), (x1,y1)
 * OR:
 *  ndx = y*2 + x
 *//*--------------------------------------------------------------------*/
struct FragmentPacket
{
    tcu::IVec2 position;      //!< Position of (0,0) fragment.
    uint64_t coverage;        //!< Coverage mask.
    tcu::Vec4 barycentric[3]; //!< Perspective-correct barycentric values.
};

} // namespace rr

#endif // _RRFRAGMENTPACKET_HPP
