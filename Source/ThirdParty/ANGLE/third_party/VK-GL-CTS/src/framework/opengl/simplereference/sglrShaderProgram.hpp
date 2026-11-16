#ifndef _SGLRSHADERPROGRAM_HPP
#define _SGLRSHADERPROGRAM_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief SGLR shader program.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "rrShaders.hpp"
#include "gluShaderUtil.hpp"

#include <vector>
#include <string>

namespace sglr
{

namespace rc
{
class Texture1D;
class Texture2D;
class TextureCube;
class Texture2DArray;
class Texture3D;
class TextureCubeArray;
} // namespace rc

class ShaderProgram;

namespace pdec
{

enum VaryingFlags
{
    VARYINGFLAG_NONE      = 0,
    VARYINGFLAG_FLATSHADE = (1 << 0),
};

struct VertexAttribute
{
    VertexAttribute(const std::string &name_, rr::GenericVecType type_) : name(name_), type(type_)
    {
    }

    std::string name;
    rr::GenericVecType type;
};

struct VertexToFragmentVarying
{
    VertexToFragmentVarying(rr::GenericVecType type_, int flags = VARYINGFLAG_NONE)
        : type(type_)
        , flatshade((flags & VARYINGFLAG_FLATSHADE) != 0)
    {
    }

    rr::GenericVecType type;
    bool flatshade;
};

struct VertexToGeometryVarying
{
    VertexToGeometryVarying(rr::GenericVecType type_, int flags = VARYINGFLAG_NONE)
        : type(type_)
        , flatshade((flags & VARYINGFLAG_FLATSHADE) != 0)
    {
    }

    rr::GenericVecType type;
    bool flatshade;
};

struct GeometryToFragmentVarying
{
    GeometryToFragmentVarying(rr::GenericVecType type_, int flags = VARYINGFLAG_NONE)
        : type(type_)
        , flatshade((flags & VARYINGFLAG_FLATSHADE) != 0)
    {
    }

    rr::GenericVecType type;
    bool flatshade;
};

struct FragmentOutput
{
    FragmentOutput(rr::GenericVecType type_) : type(type_)
    {
    }

    rr::GenericVecType type;
};

struct Uniform
{
    Uniform(const std::string &name_, glu::DataType type_) : name(name_), type(type_)
    {
    }

    std::string name;
    glu::DataType type;
};

struct VertexSource
{
    VertexSource(const std::string &str) : source(str)
    {
    }

    std::string source;
};

struct FragmentSource
{
    FragmentSource(const std::string &str) : source(str)
    {
    }

    std::string source;
};

struct GeometrySource
{
    GeometrySource(const std::string &str) : source(str)
    {
    }

    std::string source;
};

struct GeometryShaderDeclaration
{
    GeometryShaderDeclaration(rr::GeometryShaderInputType inputType_, rr::GeometryShaderOutputType outputType_,
                              size_t numOutputVertices_, size_t numInvocations_ = 1)
        : inputType(inputType_)
        , outputType(outputType_)
        , numOutputVertices(numOutputVertices_)
        , numInvocations(numInvocations_)
    {
    }

    rr::GeometryShaderInputType inputType;
    rr::GeometryShaderOutputType outputType;
    size_t numOutputVertices;
    size_t numInvocations;
};

class ShaderProgramDeclaration
{
public:
    ShaderProgramDeclaration(void);

    ShaderProgramDeclaration &operator<<(const VertexAttribute &);
    ShaderProgramDeclaration &operator<<(const VertexToFragmentVarying &);
    ShaderProgramDeclaration &operator<<(const VertexToGeometryVarying &);
    ShaderProgramDeclaration &operator<<(const GeometryToFragmentVarying &);
    ShaderProgramDeclaration &operator<<(const FragmentOutput &);
    ShaderProgramDeclaration &operator<<(const Uniform &);
    ShaderProgramDeclaration &operator<<(const VertexSource &);
    ShaderProgramDeclaration &operator<<(const FragmentSource &);
    ShaderProgramDeclaration &operator<<(const GeometrySource &);
    ShaderProgramDeclaration &operator<<(const GeometryShaderDeclaration &);

private:
    inline bool hasGeometryShader(void) const
    {
        return m_geometryShaderSet;
    }
    inline size_t getVertexInputCount(void) const
    {
        return m_vertexAttributes.size();
    }
    inline size_t getVertexOutputCount(void) const
    {
        return hasGeometryShader() ? m_vertexToGeometryVaryings.size() : m_vertexToFragmentVaryings.size();
    }
    inline size_t getFragmentInputCount(void) const
    {
        return hasGeometryShader() ? m_geometryToFragmentVaryings.size() : m_vertexToFragmentVaryings.size();
    }
    inline size_t getFragmentOutputCount(void) const
    {
        return m_fragmentOutputs.size();
    }
    inline size_t getGeometryInputCount(void) const
    {
        return hasGeometryShader() ? m_vertexToGeometryVaryings.size() : 0;
    }
    inline size_t getGeometryOutputCount(void) const
    {
        return hasGeometryShader() ? m_geometryToFragmentVaryings.size() : 0;
    }

    bool valid(void) const;

    std::vector<VertexAttribute> m_vertexAttributes;
    std::vector<VertexToFragmentVarying> m_vertexToFragmentVaryings;
    std::vector<VertexToGeometryVarying> m_vertexToGeometryVaryings;
    std::vector<GeometryToFragmentVarying> m_geometryToFragmentVaryings;
    std::vector<FragmentOutput> m_fragmentOutputs;
    std::vector<Uniform> m_uniforms;
    std::string m_vertexSource;
    std::string m_fragmentSource;
    std::string m_geometrySource;
    GeometryShaderDeclaration m_geometryDecl;

    bool m_vertexShaderSet;
    bool m_fragmentShaderSet;
    bool m_geometryShaderSet;

    friend class ::sglr::ShaderProgram;
};

} // namespace pdec

struct UniformSlot
{
    std::string name;
    glu::DataType type;

    union
    {
        int32_t i;
        int32_t i4[4];
        float f;
        float f4[4];
        float m3[3 * 3]; //!< row major, can be fed directly to tcu::Matrix constructor
        float m4[4 * 4]; //!< row major, can be fed directly to tcu::Matrix constructor
    } value;

    union
    {
        const void *ptr;

        const rc::Texture1D *tex1D;
        const rc::Texture2D *tex2D;
        const rc::TextureCube *texCube;
        const rc::Texture2DArray *tex2DArray;
        const rc::Texture3D *tex3D;
        const rc::TextureCubeArray *texCubeArray;
    } sampler;

    inline UniformSlot(void) : type(glu::TYPE_LAST)
    {
        value.i     = 0;
        sampler.ptr = nullptr;
    }
};

class ShaderProgram : private rr::VertexShader, private rr::GeometryShader, private rr::FragmentShader
{
public:
    ShaderProgram(const pdec::ShaderProgramDeclaration &);
    virtual ~ShaderProgram(void);

    const UniformSlot &getUniformByName(const char *name) const;

    inline const rr::VertexShader *getVertexShader(void) const
    {
        return static_cast<const rr::VertexShader *>(this);
    }
    inline const rr::FragmentShader *getFragmentShader(void) const
    {
        return static_cast<const rr::FragmentShader *>(this);
    }
    inline const rr::GeometryShader *getGeometryShader(void) const
    {
        return static_cast<const rr::GeometryShader *>(this);
    }

private:
    virtual void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                               const int numPackets) const                       = 0;
    virtual void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                const rr::FragmentShadingContext &context) const = 0;
    virtual void shadePrimitives(rr::GeometryEmitter &output, int verticesIn, const rr::PrimitivePacket *packets,
                                 const int numPackets, int invocationID) const;

    std::vector<std::string> m_attributeNames;

protected:
    std::vector<UniformSlot> m_uniforms;

private:
    const std::string m_vertSrc;
    const std::string m_fragSrc;
    const std::string m_geomSrc;
    const bool m_hasGeometryShader;

    friend class ReferenceContext; // for uniform access
    friend class GLContext;        // for source string access
} DE_WARN_UNUSED_TYPE;

} // namespace sglr

#endif // _SGLRSHADERPROGRAM_HPP
