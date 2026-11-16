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
 * \brief Shading context
 *//*--------------------------------------------------------------------*/

#include "rrShadingContext.hpp"

namespace rr
{

FragmentShadingContext::FragmentShadingContext(const GenericVec4 *varying0, const GenericVec4 *varying1,
                                               const GenericVec4 *varying2, GenericVec4 *outputArray_,
                                               GenericVec4 *outputArraySrc1_, float *fragmentDepths_, int primitiveID_,
                                               int numFragmentOutputs_, int numSamples_, FaceType visibleFace_)
    : outputArray(outputArray_)
    , outputArraySrc1(outputArraySrc1_)
    , primitiveID(primitiveID_)
    , numFragmentOutputs(numFragmentOutputs_)
    , numSamples(numSamples_)
    , fragmentDepths(fragmentDepths_)
    , visibleFace(visibleFace_)
{
    varyings[0] = varying0;
    varyings[1] = varying1;
    varyings[2] = varying2;
}

} // namespace rr
