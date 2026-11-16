/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
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
 * \brief Program Executor.
 *//*--------------------------------------------------------------------*/

#include "rsgProgramExecutor.hpp"
#include "rsgExecutionContext.hpp"
#include "rsgVariableValue.hpp"
#include "rsgUtils.hpp"
#include "tcuSurface.hpp"
#include "deMath.h"
#include "deString.h"

#include <set>
#include <string>
#include <map>

using std::map;
using std::set;
using std::string;
using std::vector;

namespace rsg
{

class VaryingStorage
{
public:
    VaryingStorage(const VariableType &type, int numVertices);
    ~VaryingStorage(void)
    {
    }

    ValueAccess getValue(const VariableType &type, int vtxNdx);
    ConstValueAccess getValue(const VariableType &type, int vtxNdx) const;

private:
    std::vector<Scalar> m_value;
};

VaryingStorage::VaryingStorage(const VariableType &type, int numVertices) : m_value(type.getScalarSize() * numVertices)
{
}

ValueAccess VaryingStorage::getValue(const VariableType &type, int vtxNdx)
{
    return ValueAccess(type, &m_value[type.getScalarSize() * vtxNdx]);
}

ConstValueAccess VaryingStorage::getValue(const VariableType &type, int vtxNdx) const
{
    return ConstValueAccess(type, &m_value[type.getScalarSize() * vtxNdx]);
}

class VaryingStore
{
public:
    VaryingStore(int numVertices);
    ~VaryingStore(void);

    VaryingStorage *getStorage(const VariableType &type, const char *name);

private:
    int m_numVertices;
    std::map<std::string, VaryingStorage *> m_values;
};

VaryingStore::VaryingStore(int numVertices) : m_numVertices(numVertices)
{
}

VaryingStore::~VaryingStore(void)
{
    for (map<string, VaryingStorage *>::iterator i = m_values.begin(); i != m_values.end(); i++)
        delete i->second;
    m_values.clear();
}

VaryingStorage *VaryingStore::getStorage(const VariableType &type, const char *name)
{
    VaryingStorage *storage = m_values[name];

    if (!storage)
    {
        storage        = new VaryingStorage(type, m_numVertices);
        m_values[name] = storage;
    }

    return storage;
}

inline float interpolateVertexQuad(const tcu::Vec4 &quad, float x, float y)
{
    float w00 = (1.0f - x) * (1.0f - y);
    float w01 = (1.0f - x) * y;
    float w10 = x * (1.0f - y);
    float w11 = x * y;
    return quad.x() * w00 + quad.y() * w10 + quad.z() * w01 + quad.w() * w11;
}

inline float interpolateVertex(float x0y0, float x1y1, float x, float y)
{
    return interpolateVertexQuad(tcu::Vec4(x0y0, (x0y0 + x1y1) * 0.5f, (x0y0 + x1y1) * 0.5f, x1y1), x, y);
}

inline float interpolateTri(float v0, float v1, float v2, float x, float y)
{
    return v0 + (v1 - v0) * x + (v2 - v0) * y;
}

inline float interpolateFragment(const tcu::Vec4 &quad, float x, float y)
{
    if (x + y < 1.0f)
        return interpolateTri(quad.x(), quad.y(), quad.z(), x, y);
    else
        return interpolateTri(quad.w(), quad.z(), quad.y(), 1.0f - x, 1.0f - y);
}

template <int Stride>
void interpolateVertexInput(StridedValueAccess<Stride> dst, int dstComp, const ConstValueRangeAccess valueRange,
                            float x, float y)
{
    TCU_CHECK(valueRange.getType().getBaseType() == VariableType::TYPE_FLOAT);
    int numElements = valueRange.getType().getNumElements();
    for (int elementNdx = 0; elementNdx < numElements; elementNdx++)
    {
        float xd, yd;
        getVertexInterpolationCoords(xd, yd, x, y, elementNdx);
        dst.component(elementNdx).asFloat(dstComp) =
            interpolateVertex(valueRange.getMin().component(elementNdx).asFloat(),
                              valueRange.getMax().component(elementNdx).asFloat(), xd, yd);
    }
}

template <int Stride>
void interpolateFragmentInput(StridedValueAccess<Stride> dst, int dstComp, ConstValueAccess vtx0, ConstValueAccess vtx1,
                              ConstValueAccess vtx2, ConstValueAccess vtx3, float x, float y)
{
    TCU_CHECK(dst.getType().getBaseType() == VariableType::TYPE_FLOAT);
    int numElements = dst.getType().getNumElements();
    for (int ndx = 0; ndx < numElements; ndx++)
        dst.component(ndx).asFloat(dstComp) =
            interpolateFragment(tcu::Vec4(vtx0.component(ndx).asFloat(), vtx1.component(ndx).asFloat(),
                                          vtx2.component(ndx).asFloat(), vtx3.component(ndx).asFloat()),
                                x, y);
}

template <int Stride>
void copyVarying(ValueAccess dst, ConstStridedValueAccess<Stride> src, int compNdx)
{
    TCU_CHECK(dst.getType().getBaseType() == VariableType::TYPE_FLOAT);
    for (int elemNdx = 0; elemNdx < dst.getType().getNumElements(); elemNdx++)
        dst.component(elemNdx).asFloat() = src.component(elemNdx).asFloat(compNdx);
}

ProgramExecutor::ProgramExecutor(const tcu::PixelBufferAccess &dst, int gridWidth, int gridHeight)
    : m_dst(dst)
    , m_gridWidth(gridWidth)
    , m_gridHeight(gridHeight)
{
}

ProgramExecutor::~ProgramExecutor(void)
{
}

void ProgramExecutor::setTexture(int samplerNdx, const tcu::Texture2D *texture, const tcu::Sampler &sampler)
{
    m_samplers2D[samplerNdx] = Sampler2D(texture, sampler);
}

void ProgramExecutor::setTexture(int samplerNdx, const tcu::TextureCube *texture, const tcu::Sampler &sampler)
{
    m_samplersCube[samplerNdx] = SamplerCube(texture, sampler);
}

inline tcu::IVec4 computeVertexIndices(float cellWidth, float cellHeight, int gridVtxWidth, int gridVtxHeight, int x,
                                       int y)
{
    DE_UNREF(gridVtxHeight);
    int x0 = (int)deFloatFloor((float)x / cellWidth);
    int y0 = (int)deFloatFloor((float)y / cellHeight);
    return tcu::IVec4(y0 * gridVtxWidth + x0, y0 * gridVtxWidth + x0 + 1, (y0 + 1) * gridVtxWidth + x0,
                      (y0 + 1) * gridVtxWidth + x0 + 1);
}

inline tcu::Vec2 computeGridCellWeights(float cellWidth, float cellHeight, int x, int y)
{
    float gx = ((float)x + 0.5f) / cellWidth;
    float gy = ((float)y + 0.5f) / cellHeight;
    return tcu::Vec2(deFloatFrac(gx), deFloatFrac(gy));
}

inline tcu::RGBA toColor(tcu::Vec4 rgba)
{
    return tcu::RGBA(
        deClamp32(deRoundFloatToInt32(rgba.x() * 255), 0, 255), deClamp32(deRoundFloatToInt32(rgba.y() * 255), 0, 255),
        deClamp32(deRoundFloatToInt32(rgba.z() * 255), 0, 255), deClamp32(deRoundFloatToInt32(rgba.w() * 255), 0, 255));
}

void ProgramExecutor::execute(const Shader &vertexShader, const Shader &fragmentShader,
                              const vector<VariableValue> &uniformValues)
{
    int gridVtxWidth  = m_gridWidth + 1;
    int gridVtxHeight = m_gridHeight + 1;
    int numVertices   = gridVtxWidth * gridVtxHeight;

    VaryingStore varyingStore(numVertices);

    // Execute vertex shader
    {
        ExecutionContext execCtx(m_samplers2D, m_samplersCube);
        int numPackets = numVertices + ((numVertices % EXEC_VEC_WIDTH) ? 1 : 0);

        const vector<ShaderInput *> &inputs = vertexShader.getInputs();
        vector<const Variable *> outputs;
        vertexShader.getOutputs(outputs);

        // Set uniform values
        for (vector<VariableValue>::const_iterator uniformIter = uniformValues.begin();
             uniformIter != uniformValues.end(); uniformIter++)
            execCtx.getValue(uniformIter->getVariable()) = uniformIter->getValue().value();

        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            int packetStart = packetNdx * EXEC_VEC_WIDTH;
            int packetEnd   = deMin32((packetNdx + 1) * EXEC_VEC_WIDTH, numVertices);

            // Compute values for vertex shader inputs
            for (vector<ShaderInput *>::const_iterator i = inputs.begin(); i != inputs.end(); i++)
            {
                const ShaderInput *input = *i;
                ExecValueAccess access   = execCtx.getValue(input->getVariable());

                for (int vtxNdx = packetStart; vtxNdx < packetEnd; vtxNdx++)
                {
                    int y    = (vtxNdx / gridVtxWidth);
                    int x    = vtxNdx - y * gridVtxWidth;
                    float xf = (float)x / (float)(gridVtxWidth - 1);
                    float yf = (float)y / (float)(gridVtxHeight - 1);

                    interpolateVertexInput(access, vtxNdx - packetStart, input->getValueRange(), xf, yf);
                }
            }

            // Execute vertex shader for packet
            vertexShader.execute(execCtx);

            // Store output values
            for (vector<const Variable *>::const_iterator i = outputs.begin(); i != outputs.end(); i++)
            {
                const Variable *output = *i;

                if (strcmp(output->getName(), "gl_Position") == 0)
                    continue; // Do not store position

                ExecConstValueAccess access = execCtx.getValue(output);
                VaryingStorage *dst         = varyingStore.getStorage(output->getType(), output->getName());

                for (int vtxNdx = packetStart; vtxNdx < packetEnd; vtxNdx++)
                {
                    ValueAccess varyingAccess = dst->getValue(output->getType(), vtxNdx);
                    copyVarying(varyingAccess, access, vtxNdx - packetStart);
                }
            }
        }
    }

    // Execute fragment shader
    {
        ExecutionContext execCtx(m_samplers2D, m_samplersCube);

        // Assign uniform values
        for (vector<VariableValue>::const_iterator i = uniformValues.begin(); i != uniformValues.end(); i++)
            execCtx.getValue(i->getVariable()) = i->getValue().value();

        const vector<ShaderInput *> &inputs = fragmentShader.getInputs();
        const Variable *fragColorVar        = nullptr;
        vector<const Variable *> outputs;

        // Find fragment shader output assigned to location 0. This is fragment color.
        fragmentShader.getOutputs(outputs);
        for (vector<const Variable *>::const_iterator i = outputs.begin(); i != outputs.end(); i++)
        {
            if ((*i)->getLayoutLocation() == 0)
            {
                fragColorVar = *i;
                break;
            }
        }
        TCU_CHECK(fragColorVar);

        int width      = m_dst.getWidth();
        int height     = m_dst.getHeight();
        int numPackets = (width * height) / EXEC_VEC_WIDTH + (((width * height) % EXEC_VEC_WIDTH) ? 1 : 0);

        float cellWidth  = (float)width / (float)m_gridWidth;
        float cellHeight = (float)height / (float)m_gridHeight;

        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            int packetStart = packetNdx * EXEC_VEC_WIDTH;
            int packetEnd   = deMin32((packetNdx + 1) * EXEC_VEC_WIDTH, width * height);

            // Interpolate varyings
            for (vector<ShaderInput *>::const_iterator i = inputs.begin(); i != inputs.end(); i++)
            {
                const ShaderInput *input  = *i;
                ExecValueAccess access    = execCtx.getValue(input->getVariable());
                const VariableType &type  = input->getVariable()->getType();
                const VaryingStorage *src = varyingStore.getStorage(type, input->getVariable()->getName());

                // \todo [2011-03-08 pyry] Part of this could be pre-computed...
                for (int fragNdx = packetStart; fragNdx < packetEnd; fragNdx++)
                {
                    int y = fragNdx / width;
                    int x = fragNdx - y * width;
                    tcu::IVec4 vtxIndices =
                        computeVertexIndices(cellWidth, cellHeight, gridVtxWidth, gridVtxHeight, x, y);
                    tcu::Vec2 weights = computeGridCellWeights(cellWidth, cellHeight, x, y);

                    interpolateFragmentInput(access, fragNdx - packetStart, src->getValue(type, vtxIndices.x()),
                                             src->getValue(type, vtxIndices.y()), src->getValue(type, vtxIndices.z()),
                                             src->getValue(type, vtxIndices.w()), weights.x(), weights.y());
                }
            }

            // Execute fragment shader
            fragmentShader.execute(execCtx);

            // Write resulting color
            ExecConstValueAccess colorValue = execCtx.getValue(fragColorVar);
            for (int fragNdx = packetStart; fragNdx < packetEnd; fragNdx++)
            {
                int y       = fragNdx / width;
                int x       = fragNdx - y * width;
                int cNdx    = fragNdx - packetStart;
                tcu::Vec4 c = tcu::Vec4(colorValue.component(0).asFloat(cNdx), colorValue.component(1).asFloat(cNdx),
                                        colorValue.component(2).asFloat(cNdx), colorValue.component(3).asFloat(cNdx));

                // \todo [2012-11-13 pyry] Reverse order.
                m_dst.setPixel(c, x, m_dst.getHeight() - y - 1);
            }
        }
    }
}

} // namespace rsg
