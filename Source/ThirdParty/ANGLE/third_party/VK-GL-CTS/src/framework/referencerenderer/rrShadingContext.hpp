#ifndef _RRSHADINGCONTEXT_HPP
#define _RRSHADINGCONTEXT_HPP
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

#include "rrDefs.hpp"
#include "rrGenericVector.hpp"
#include "rrFragmentPacket.hpp"

namespace rr
{

/*--------------------------------------------------------------------*//*!
 * \brief Fragment shading context
 *
 * Contains per-primitive information used in fragment shading
 *//*--------------------------------------------------------------------*/
struct FragmentShadingContext
{
    FragmentShadingContext(const GenericVec4 *varying0, const GenericVec4 *varying1, const GenericVec4 *varying2,
                           GenericVec4 *outputArray, GenericVec4 *outputArraySrc1, float *fragmentDepths,
                           int primitiveID, int numFragmentOutputs, int numSamples, FaceType visibleFace_);

    const GenericVec4 *varyings[3];     //!< Vertex shader outputs. Pointer will be NULL if there is no such vertex.
    GenericVec4 *const outputArray;     //!< Fragment output array
    GenericVec4 *const outputArraySrc1; //!< Fragment output array for source 1.
    const int primitiveID;              //!< Geometry shader output
    const int numFragmentOutputs;       //!< Fragment output count
    const int numSamples;               //!< Number of samples
    float *
        fragmentDepths; //!< Fragment packet depths. Pointer will be NULL if there is no depth buffer. Each sample has per-sample depth values
    FaceType visibleFace; //!< Which face (front or back) is visible
};

// Write output

template <typename T>
void writeFragmentOutput(const FragmentShadingContext &context, int packetNdx, int fragNdx, int outputNdx,
                         const T &value)
{
    DE_ASSERT(packetNdx >= 0);
    DE_ASSERT(fragNdx >= 0 && fragNdx < 4);
    DE_ASSERT(outputNdx >= 0 && outputNdx < context.numFragmentOutputs);

    context.outputArray[outputNdx + context.numFragmentOutputs * (fragNdx + packetNdx * 4)] = value;
}

template <typename T>
void writeFragmentOutputDualSource(const FragmentShadingContext &context, int packetNdx, int fragNdx, int outputNdx,
                                   const T &value, const T &value1)
{
    DE_ASSERT(packetNdx >= 0);
    DE_ASSERT(fragNdx >= 0 && fragNdx < 4);
    DE_ASSERT(outputNdx >= 0 && outputNdx < context.numFragmentOutputs);

    context.outputArray[outputNdx + context.numFragmentOutputs * (fragNdx + packetNdx * 4)]     = value;
    context.outputArraySrc1[outputNdx + context.numFragmentOutputs * (fragNdx + packetNdx * 4)] = value1;
}

// Read Varying

template <typename T>
tcu::Vector<T, 4> readPointVarying(const FragmentPacket &packet, const FragmentShadingContext &context, int varyingLoc,
                                   int fragNdx)
{
    DE_UNREF(fragNdx);
    DE_UNREF(packet);

    return context.varyings[0][varyingLoc].get<T>();
}

template <typename T>
tcu::Vector<T, 4> readLineVarying(const FragmentPacket &packet, const FragmentShadingContext &context, int varyingLoc,
                                  int fragNdx)
{
    return packet.barycentric[0][fragNdx] * context.varyings[0][varyingLoc].get<T>() +
           packet.barycentric[1][fragNdx] * context.varyings[1][varyingLoc].get<T>();
}

template <typename T>
tcu::Vector<T, 4> readTriangleVarying(const FragmentPacket &packet, const FragmentShadingContext &context,
                                      int varyingLoc, int fragNdx)
{
    return packet.barycentric[0][fragNdx] * context.varyings[0][varyingLoc].get<T>() +
           packet.barycentric[1][fragNdx] * context.varyings[1][varyingLoc].get<T>() +
           packet.barycentric[2][fragNdx] * context.varyings[2][varyingLoc].get<T>();
}

template <typename T>
tcu::Vector<T, 4> readVarying(const FragmentPacket &packet, const FragmentShadingContext &context, int varyingLoc,
                              int fragNdx)
{
    if (context.varyings[1] == nullptr)
        return readPointVarying<T>(packet, context, varyingLoc, fragNdx);
    if (context.varyings[2] == nullptr)
        return readLineVarying<T>(packet, context, varyingLoc, fragNdx);
    return readTriangleVarying<T>(packet, context, varyingLoc, fragNdx);
}

// Derivative

template <typename T, int Size>
void dFdxLocal(tcu::Vector<T, Size> outFragmentdFdx[4], const tcu::Vector<T, Size> func[4])
{
    const tcu::Vector<T, Size> dFdx[2] = {func[1] - func[0], func[3] - func[2]};

    outFragmentdFdx[0] = dFdx[0];
    outFragmentdFdx[1] = dFdx[0];
    outFragmentdFdx[2] = dFdx[1];
    outFragmentdFdx[3] = dFdx[1];
}

template <typename T, int Size>
void dFdyLocal(tcu::Vector<T, Size> outFragmentdFdy[4], const tcu::Vector<T, Size> func[4])
{
    const tcu::Vector<T, Size> dFdy[2] = {func[2] - func[0], func[3] - func[1]};

    outFragmentdFdy[0] = dFdy[0];
    outFragmentdFdy[1] = dFdy[1];
    outFragmentdFdy[2] = dFdy[0];
    outFragmentdFdy[3] = dFdy[1];
}

template <typename T>
inline void dFdxVarying(tcu::Vector<T, 4> outFragmentdFdx[4], const FragmentPacket &packet,
                        const FragmentShadingContext &context, int varyingLoc)
{
    const tcu::Vector<T, 4> func[4] = {
        readVarying<T>(packet, context, varyingLoc, 0),
        readVarying<T>(packet, context, varyingLoc, 1),
        readVarying<T>(packet, context, varyingLoc, 2),
        readVarying<T>(packet, context, varyingLoc, 3),
    };

    dFdxLocal(outFragmentdFdx, func);
}

template <typename T>
inline void dFdyVarying(tcu::Vector<T, 4> outFragmentdFdy[4], const FragmentPacket &packet,
                        const FragmentShadingContext &context, int varyingLoc)
{
    const tcu::Vector<T, 4> func[4] = {
        readVarying<T>(packet, context, varyingLoc, 0),
        readVarying<T>(packet, context, varyingLoc, 1),
        readVarying<T>(packet, context, varyingLoc, 2),
        readVarying<T>(packet, context, varyingLoc, 3),
    };

    dFdyLocal(outFragmentdFdy, func);
}

// Fragent depth

inline float readFragmentDepth(const FragmentShadingContext &context, int packetNdx, int fragNdx, int sampleNdx)
{
    // Reading or writing to fragment depth values while there is no depth buffer is legal but not supported by rr
    DE_ASSERT(context.fragmentDepths);
    return context.fragmentDepths[(packetNdx * 4 + fragNdx) * context.numSamples + sampleNdx];
}

inline void writeFragmentDepth(const FragmentShadingContext &context, int packetNdx, int fragNdx, int sampleNdx,
                               float depthValue)
{
    // Reading or writing to fragment depth values while there is no depth buffer is legal but not supported by rr
    DE_ASSERT(context.fragmentDepths);
    context.fragmentDepths[(packetNdx * 4 + fragNdx) * context.numSamples + sampleNdx] = depthValue;
}

} // namespace rr

#endif // _RRSHADINGCONTEXT_HPP
