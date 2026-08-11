#ifndef _RRSHADERS_HPP
#define _RRSHADERS_HPP
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
 * \brief Shader interfaces.
 *//*--------------------------------------------------------------------*/

#include "rrDefs.hpp"
#include "rrVertexAttrib.hpp"
#include "rrVertexPacket.hpp"
#include "rrFragmentPacket.hpp"
#include "rrPrimitivePacket.hpp"
#include "rrShadingContext.hpp"
#include "deString.h"

namespace rr
{

/*--------------------------------------------------------------------*//*!
 * \brief Vertex shader input information
 *//*--------------------------------------------------------------------*/
struct VertexInputInfo
{
    VertexInputInfo(void)
    {
        // sensible defaults
        type = GENERICVECTYPE_LAST;
    }

    GenericVecType type;
};

/*--------------------------------------------------------------------*//*!
 * \brief Shader varying information
 *//*--------------------------------------------------------------------*/
struct VertexVaryingInfo
{
    VertexVaryingInfo(void)
    {
        // sensible defaults
        type      = GENERICVECTYPE_LAST;
        flatshade = false;
    }

    // \note used by std::vector<T>::operator==() const
    bool operator==(const VertexVaryingInfo &other) const
    {
        return type == other.type && flatshade == other.flatshade;
    }

    GenericVecType type;
    bool flatshade;
};

typedef VertexVaryingInfo VertexOutputInfo;
typedef VertexVaryingInfo FragmentInputInfo;
typedef VertexVaryingInfo GeometryInputInfo;
typedef VertexVaryingInfo GeometryOutputInfo;

/*--------------------------------------------------------------------*//*!
 * \brief Fragment shader output information
 *//*--------------------------------------------------------------------*/
struct FragmentOutputInfo
{
    FragmentOutputInfo(void)
    {
        // sensible defaults
        type = GENERICVECTYPE_LAST;
    }

    GenericVecType type;
};

/*--------------------------------------------------------------------*//*!
 * \brief Vertex shader interface
 *
 * Vertex shaders execute shading for set of vertex packets. See VertexPacket
 * documentation for more details on shading API.
 *//*--------------------------------------------------------------------*/
class VertexShader
{
public:
    VertexShader(size_t numInputs, size_t numOutputs) : m_inputs(numInputs), m_outputs(numOutputs)
    {
    }

    virtual void shadeVertices(const VertexAttrib *inputs, VertexPacket *const *packets,
                               const int numPackets) const = 0;

    const std::vector<VertexInputInfo> &getInputs(void) const
    {
        return m_inputs;
    }
    const std::vector<VertexOutputInfo> &getOutputs(void) const
    {
        return m_outputs;
    }

protected:
    virtual ~VertexShader(void)
    {
    } // \note Renderer will not delete any objects passed in.

    std::vector<VertexInputInfo> m_inputs;
    std::vector<VertexOutputInfo> m_outputs;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Fragment shader interface
 *
 * Fragment shader executes shading for list of fragment packets. See
 * FragmentPacket documentation for more details on shading API.
 *//*--------------------------------------------------------------------*/
class FragmentShader
{
public:
    FragmentShader(size_t numInputs, size_t numOutputs) : m_inputs(numInputs), m_outputs(numOutputs)
    {
    }

    const std::vector<FragmentInputInfo> &getInputs(void) const
    {
        return m_inputs;
    }
    const std::vector<FragmentOutputInfo> &getOutputs(void) const
    {
        return m_outputs;
    }

    virtual void shadeFragments(FragmentPacket *packets, const int numPackets, const FragmentShadingContext &context)
        const = 0; // \note numPackets must be greater than zero.

protected:
    virtual ~FragmentShader(void)
    {
    } // \note Renderer will not delete any objects passed in.

    std::vector<FragmentInputInfo> m_inputs;
    std::vector<FragmentOutputInfo> m_outputs;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Geometry shader input primitive type
 *//*--------------------------------------------------------------------*/
enum GeometryShaderInputType
{
    GEOMETRYSHADERINPUTTYPE_POINTS = 0,
    GEOMETRYSHADERINPUTTYPE_LINES,
    GEOMETRYSHADERINPUTTYPE_LINES_ADJACENCY,
    GEOMETRYSHADERINPUTTYPE_TRIANGLES,
    GEOMETRYSHADERINPUTTYPE_TRIANGLES_ADJACENCY,

    GEOMETRYSHADERINPUTTYPE_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief Geometry shader output primitive type
 *//*--------------------------------------------------------------------*/
enum GeometryShaderOutputType
{
    GEOMETRYSHADEROUTPUTTYPE_POINTS = 0,
    GEOMETRYSHADEROUTPUTTYPE_LINE_STRIP,
    GEOMETRYSHADEROUTPUTTYPE_TRIANGLE_STRIP,

    GEOMETRYSHADEROUTPUTTYPE_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief Geometry shader interface
 *
 * Geometry shader executes a list of primitive packets and outputs
 * a new set of vertex packets for new primitives.
 *//*--------------------------------------------------------------------*/
class GeometryShader
{
public:
    GeometryShader(size_t numVaryingInputs, size_t numVaryingOutputs, GeometryShaderInputType inputType,
                   GeometryShaderOutputType outputType, size_t numVerticesOut, size_t numInvocations);

    virtual void shadePrimitives(GeometryEmitter &output, int verticesIn, const PrimitivePacket *packets,
                                 const int numPackets, int invocationID) const = 0;

    const std::vector<GeometryInputInfo> &getInputs(void) const
    {
        return m_inputs;
    }
    const std::vector<GeometryOutputInfo> &getOutputs(void) const
    {
        return m_outputs;
    }
    inline GeometryShaderInputType getInputType(void) const
    {
        return m_inputType;
    }
    inline GeometryShaderOutputType getOutputType(void) const
    {
        return m_outputType;
    }
    inline size_t getNumVerticesOut(void) const
    {
        return m_numVerticesOut;
    }
    inline size_t getNumInvocations(void) const
    {
        return m_numInvocations;
    }

protected:
    virtual ~GeometryShader(void)
    {
    }

    const GeometryShaderInputType m_inputType;
    const GeometryShaderOutputType m_outputType;
    const size_t m_numVerticesOut;
    const size_t m_numInvocations;

    std::vector<GeometryInputInfo> m_inputs;
    std::vector<GeometryOutputInfo> m_outputs;
} DE_WARN_UNUSED_TYPE;

// Helpers for shader implementations.

template <class Shader>
class VertexShaderLoop : public VertexShader
{
public:
    VertexShaderLoop(const Shader &shader) : m_shader(shader)
    {
    }

    void shadeVertices(const VertexAttrib *inputs, VertexPacket *packets, const int numPackets) const;

private:
    const Shader &m_shader;
};

template <class Shader>
void VertexShaderLoop<Shader>::shadeVertices(const VertexAttrib *inputs, VertexPacket *packets,
                                             const int numPackets) const
{
    for (int ndx = 0; ndx < numPackets; ndx++)
        m_shader.shadeVertex(inputs, packets[ndx]);
}

template <class Shader>
class FragmentShaderLoop : public FragmentShader
{
public:
    FragmentShaderLoop(const Shader &shader) : m_shader(shader)
    {
    }

    void shadeFragments(FragmentPacket *packets, const int numPackets) const;

private:
    const Shader &m_shader;
};

template <class Shader>
void FragmentShaderLoop<Shader>::shadeFragments(FragmentPacket *packets, const int numPackets) const
{
    for (int ndx = 0; ndx < numPackets; ndx++)
        m_shader.shadeFragment(packets[ndx]);
}

} // namespace rr

#endif // _RRSHADERS_HPP
