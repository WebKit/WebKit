/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
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
 * \brief Texture buffer test case
 *//*--------------------------------------------------------------------*/

#include "glsTextureBufferCase.hpp"

#include "tcuFormatUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuSurface.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuResultCollector.hpp"

#include "rrRenderer.hpp"
#include "rrShaders.hpp"

#include "gluObjectWrapper.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "gluStrUtil.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include "deMemory.h"
#include "deString.h"
#include "deMath.h"

#include <sstream>
#include <string>
#include <vector>

using tcu::TestLog;

using std::map;
using std::string;
using std::vector;

using namespace deqp::gls::TextureBufferCaseUtil;

namespace deqp
{
namespace gls
{
namespace
{

enum
{
    MAX_VIEWPORT_WIDTH  = 256,
    MAX_VIEWPORT_HEIGHT = 256,
    MIN_VIEWPORT_WIDTH  = 64,
    MIN_VIEWPORT_HEIGHT = 64,
};

uint8_t extend2BitsToByte(uint8_t bits)
{
    DE_ASSERT((bits & (~0x03u)) == 0);

    return (uint8_t)(bits | (bits << 2) | (bits << 4) | (bits << 6));
}

void genRandomCoords(de::Random rng, vector<uint8_t> &coords, size_t offset, size_t size)
{
    const uint8_t bits    = 2;
    const uint8_t bitMask = uint8_t((0x1u << bits) - 1);

    coords.resize(size);

    for (int i = 0; i < (int)size; i++)
    {
        const uint8_t xBits = uint8_t(rng.getUint32() & bitMask);
        coords[i]           = extend2BitsToByte(xBits);
    }

    // Fill indices with nice quad
    {
        const uint8_t indices[] = {extend2BitsToByte(0x0u), extend2BitsToByte(0x1u), extend2BitsToByte(0x2u),
                                   extend2BitsToByte(0x3u)};

        for (int i = 0; i < DE_LENGTH_OF_ARRAY(indices); i++)
        {
            const uint8_t index = indices[i];
            const size_t posX   = (size_t(index) * 2) + 0;
            const size_t posY   = (size_t(index) * 2) + 1;

            if (posX >= offset && posX < offset + size)
                coords[posX - offset] = ((i % 2) == 0 ? extend2BitsToByte(0x0u) : extend2BitsToByte(0x3u));

            if (posY >= offset && posY < offset + size)
                coords[posY - offset] = ((i / 2) == 1 ? extend2BitsToByte(0x3u) : extend2BitsToByte(0x0u));
        }
    }

    // Fill beginning of buffer
    {
        const uint8_t indices[] = {extend2BitsToByte(0x0u), extend2BitsToByte(0x3u), extend2BitsToByte(0x1u),

                                   extend2BitsToByte(0x1u), extend2BitsToByte(0x2u), extend2BitsToByte(0x0u),

                                   extend2BitsToByte(0x0u), extend2BitsToByte(0x2u), extend2BitsToByte(0x1u),

                                   extend2BitsToByte(0x1u), extend2BitsToByte(0x3u), extend2BitsToByte(0x0u)};

        for (int i = (int)offset; i < DE_LENGTH_OF_ARRAY(indices) && i < (int)(offset + size); i++)
            coords[i - offset] = indices[i];
    }
}

class CoordVertexShader : public rr::VertexShader
{
public:
    CoordVertexShader(void) : rr::VertexShader(1, 1)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            rr::VertexPacket *const packet = packets[packetNdx];
            tcu::Vec4 position;

            readVertexAttrib(position, inputs[0], packet->instanceNdx, packet->vertexNdx);

            packet->outputs[0] = tcu::Vec4(1.0f);
            packet->position   = tcu::Vec4(2.0f * (position.x() - 0.5f), 2.0f * (position.y() - 0.5f), 0.0f, 1.0f);
        }
    }
};

class TextureVertexShader : public rr::VertexShader
{
public:
    TextureVertexShader(const tcu::ConstPixelBufferAccess &texture) : rr::VertexShader(1, 1), m_texture(texture)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            rr::VertexPacket *const packet = packets[packetNdx];
            tcu::Vec4 position;
            tcu::Vec4 texelValue;

            readVertexAttrib(position, inputs[0], packet->instanceNdx, packet->vertexNdx);

            texelValue = tcu::Vec4(m_texture.getPixel(de::clamp<int>((deRoundFloatToInt32(position.x() * 4) + 4) *
                                                                         (deRoundFloatToInt32(position.y() * 4) + 4),
                                                                     0, m_texture.getWidth() - 1),
                                                      0));

            packet->outputs[0] = texelValue;
            packet->position   = tcu::Vec4(2.0f * (position.x() - 0.5f), 2.0f * (position.y() - 0.5f), 0.0f, 1.0f);
        }
    }

private:
    const tcu::ConstPixelBufferAccess m_texture;
};

class CoordFragmentShader : public rr::FragmentShader
{
public:
    CoordFragmentShader(void) : rr::FragmentShader(1, 1)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            rr::FragmentPacket &packet = packets[packetNdx];

            const tcu::Vec4 vtxColor0 = rr::readVarying<float>(packet, context, 0, 0);
            const tcu::Vec4 vtxColor1 = rr::readVarying<float>(packet, context, 0, 1);
            const tcu::Vec4 vtxColor2 = rr::readVarying<float>(packet, context, 0, 2);
            const tcu::Vec4 vtxColor3 = rr::readVarying<float>(packet, context, 0, 3);

            const tcu::Vec4 color0 = vtxColor0;
            const tcu::Vec4 color1 = vtxColor1;
            const tcu::Vec4 color2 = vtxColor2;
            const tcu::Vec4 color3 = vtxColor3;

            rr::writeFragmentOutput(
                context, packetNdx, 0, 0,
                tcu::Vec4(color0.x() * color0.w(), color0.y() * color0.w(), color0.z() * color0.w(), 1.0f));
            rr::writeFragmentOutput(
                context, packetNdx, 1, 0,
                tcu::Vec4(color1.x() * color1.w(), color1.y() * color1.w(), color1.z() * color1.w(), 1.0f));
            rr::writeFragmentOutput(
                context, packetNdx, 2, 0,
                tcu::Vec4(color2.x() * color2.w(), color2.y() * color2.w(), color2.z() * color2.w(), 1.0f));
            rr::writeFragmentOutput(
                context, packetNdx, 3, 0,
                tcu::Vec4(color3.x() * color3.w(), color3.y() * color3.w(), color3.z() * color3.w(), 1.0f));
        }
    }
};

class TextureFragmentShader : public rr::FragmentShader
{
public:
    TextureFragmentShader(const tcu::ConstPixelBufferAccess &texture) : rr::FragmentShader(1, 1), m_texture(texture)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            rr::FragmentPacket &packet = packets[packetNdx];

            const tcu::IVec2 position0 = packet.position + tcu::IVec2(0, 0);
            const tcu::IVec2 position1 = packet.position + tcu::IVec2(1, 0);
            const tcu::IVec2 position2 = packet.position + tcu::IVec2(0, 1);
            const tcu::IVec2 position3 = packet.position + tcu::IVec2(1, 1);

            const tcu::Vec4 texColor0 =
                m_texture.getPixel(de::clamp((position0.x() * position0.y()), 0, m_texture.getWidth() - 1), 0);
            const tcu::Vec4 texColor1 =
                m_texture.getPixel(de::clamp((position1.x() * position1.y()), 0, m_texture.getWidth() - 1), 0);
            const tcu::Vec4 texColor2 =
                m_texture.getPixel(de::clamp((position2.x() * position2.y()), 0, m_texture.getWidth() - 1), 0);
            const tcu::Vec4 texColor3 =
                m_texture.getPixel(de::clamp((position3.x() * position3.y()), 0, m_texture.getWidth() - 1), 0);

            const tcu::Vec4 vtxColor0 = rr::readVarying<float>(packet, context, 0, 0);
            const tcu::Vec4 vtxColor1 = rr::readVarying<float>(packet, context, 0, 1);
            const tcu::Vec4 vtxColor2 = rr::readVarying<float>(packet, context, 0, 2);
            const tcu::Vec4 vtxColor3 = rr::readVarying<float>(packet, context, 0, 3);

            const tcu::Vec4 color0 = 0.5f * (vtxColor0 + texColor0);
            const tcu::Vec4 color1 = 0.5f * (vtxColor1 + texColor1);
            const tcu::Vec4 color2 = 0.5f * (vtxColor2 + texColor2);
            const tcu::Vec4 color3 = 0.5f * (vtxColor3 + texColor3);

            rr::writeFragmentOutput(
                context, packetNdx, 0, 0,
                tcu::Vec4(color0.x() * color0.w(), color0.y() * color0.w(), color0.z() * color0.w(), 1.0f));
            rr::writeFragmentOutput(
                context, packetNdx, 1, 0,
                tcu::Vec4(color1.x() * color1.w(), color1.y() * color1.w(), color1.z() * color1.w(), 1.0f));
            rr::writeFragmentOutput(
                context, packetNdx, 2, 0,
                tcu::Vec4(color2.x() * color2.w(), color2.y() * color2.w(), color2.z() * color2.w(), 1.0f));
            rr::writeFragmentOutput(
                context, packetNdx, 3, 0,
                tcu::Vec4(color3.x() * color3.w(), color3.y() * color3.w(), color3.z() * color3.w(), 1.0f));
        }
    }

private:
    const tcu::ConstPixelBufferAccess m_texture;
};

string generateVertexShaderTemplate(RenderBits renderBits)
{
    std::ostringstream stream;

    stream << "${VERSION_HEADER}\n";

    if (renderBits & RENDERBITS_AS_VERTEX_TEXTURE)
        stream << "${TEXTURE_BUFFER_EXT}";

    stream << "${VTX_INPUT} layout(location = 0) ${HIGHP} vec2 i_coord;\n"
              "${VTX_OUTPUT} ${HIGHP} vec4 v_color;\n";

    if (renderBits & RENDERBITS_AS_VERTEX_TEXTURE)
    {
        stream << "uniform ${HIGHP} samplerBuffer u_vtxSampler;\n";
    }

    stream << "\n"
              "void main (void)\n"
              "{\n";

    if (renderBits & RENDERBITS_AS_VERTEX_TEXTURE)
        stream << "\tv_color = texelFetch(u_vtxSampler, clamp((int(round(i_coord.x * 4.0)) + 4) * (int(round(i_coord.y "
                  "* 4.0)) + 4), 0, textureSize(u_vtxSampler)-1));\n";
    else
        stream << "\tv_color = vec4(1.0);\n";

    stream << "\tgl_Position = vec4(2.0 * (i_coord - vec2(0.5)), 0.0, 1.0);\n"
              "}\n";

    return stream.str();
}

string generateFragmentShaderTemplate(RenderBits renderBits)
{
    std::ostringstream stream;

    stream << "${VERSION_HEADER}\n";

    if (renderBits & RENDERBITS_AS_FRAGMENT_TEXTURE)
        stream << "${TEXTURE_BUFFER_EXT}";

    stream << "${FRAG_OUTPUT} layout(location = 0) ${HIGHP} vec4 dEQP_FragColor;\n"
              "${FRAG_INPUT} ${HIGHP} vec4 v_color;\n";

    if (renderBits & RENDERBITS_AS_FRAGMENT_TEXTURE)
        stream << "uniform ${HIGHP} samplerBuffer u_fragSampler;\n";

    stream << "\n"
              "void main (void)\n"
              "{\n";

    if (renderBits & RENDERBITS_AS_FRAGMENT_TEXTURE)
        stream << "\t${HIGHP} vec4 color = 0.5 * (v_color + texelFetch(u_fragSampler, clamp(int(gl_FragCoord.x) * "
                  "int(gl_FragCoord.y), 0, textureSize(u_fragSampler)-1)));\n";
    else
        stream << "\t${HIGHP} vec4 color = v_color;\n";

    stream << "\tdEQP_FragColor = vec4(color.xyz * color.w, 1.0);\n"
              "}\n";

    return stream.str();
}

string specializeShader(const string &shaderTemplateString, glu::GLSLVersion glslVersion)
{
    const tcu::StringTemplate shaderTemplate(shaderTemplateString);
    map<string, string> parameters;

    parameters["VERSION_HEADER"] = glu::getGLSLVersionDeclaration(glslVersion);
    parameters["VTX_OUTPUT"]     = "out";
    parameters["VTX_INPUT"]      = "in";
    parameters["FRAG_INPUT"]     = "in";
    parameters["FRAG_OUTPUT"]    = "out";
    parameters["HIGHP"]          = (glslVersion == glu::GLSL_VERSION_330 ? "" : "highp");
    parameters["TEXTURE_BUFFER_EXT"] =
        (glslVersion == glu::GLSL_VERSION_330 ? "" : "#extension GL_EXT_texture_buffer : enable\n");

    return shaderTemplate.specialize(parameters);
}

glu::ShaderProgram *createRenderProgram(glu::RenderContext &renderContext, RenderBits renderBits)
{
    const string vertexShaderTemplate   = generateVertexShaderTemplate(renderBits);
    const string fragmentShaderTemplate = generateFragmentShaderTemplate(renderBits);

    const glu::GLSLVersion glslVersion = glu::getContextTypeGLSLVersion(renderContext.getType());

    const string vertexShaderSource   = specializeShader(vertexShaderTemplate, glslVersion);
    const string fragmentShaderSource = specializeShader(fragmentShaderTemplate, glslVersion);

    glu::ShaderProgram *const program =
        new glu::ShaderProgram(renderContext, glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));

    return program;
}

void logModifications(TestLog &log, ModifyBits modifyBits)
{
    tcu::ScopedLogSection section(log, "Modify Operations", "Modify Operations");

    const struct
    {
        ModifyBits bit;
        const char *str;
    } bitInfos[] = {{MODIFYBITS_BUFFERDATA, "Recreate buffer data with glBufferData()."},
                    {MODIFYBITS_BUFFERSUBDATA, "Modify texture buffer with glBufferSubData()."},
                    {MODIFYBITS_MAPBUFFER_WRITE, "Map buffer write-only and rewrite data."},
                    {MODIFYBITS_MAPBUFFER_READWRITE, "Map buffer readw-write check and rewrite data."}};

    DE_ASSERT(modifyBits != 0);

    for (int infoNdx = 0; infoNdx < DE_LENGTH_OF_ARRAY(bitInfos); infoNdx++)
    {
        if (modifyBits & bitInfos[infoNdx].bit)
            log << TestLog::Message << bitInfos[infoNdx].str << TestLog::EndMessage;
    }
}

void modifyBufferData(TestLog &log, de::Random &rng, glu::TextureBuffer &texture)
{
    vector<uint8_t> data;

    genRandomCoords(rng, data, 0, texture.getBufferSize());

    log << TestLog::Message << "BufferData, Size: " << data.size() << TestLog::EndMessage;

    {
        // replace getRefBuffer with a new buffer
        de::ArrayBuffer<uint8_t> buffer(&(data[0]), data.size());
        texture.getRefBuffer().swap(buffer);
    }

    texture.upload();
}

void modifyBufferSubData(TestLog &log, de::Random &rng, const glw::Functions &gl, glu::TextureBuffer &texture)
{
    const size_t minSize = 4 * 16;
    const size_t size =
        de::max<size_t>(minSize, size_t((float)(texture.getSize() != 0 ? texture.getSize() : texture.getBufferSize()) *
                                        (0.7f + 0.3f * rng.getFloat())));
    const size_t minOffset = texture.getOffset();
    const size_t offset    = minOffset + (rng.getUint32() % (texture.getBufferSize() - (size + minOffset)));
    vector<uint8_t> data;

    genRandomCoords(rng, data, offset, size);

    log << TestLog::Message << "BufferSubData, Offset: " << offset << ", Size: " << size << TestLog::EndMessage;

    gl.bindBuffer(GL_TEXTURE_BUFFER, texture.getGLBuffer());
    gl.bufferSubData(GL_TEXTURE_BUFFER, (glw::GLsizei)offset, (glw::GLsizei)data.size(), &(data[0]));
    gl.bindBuffer(GL_TEXTURE_BUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to update data with glBufferSubData()");

    deMemcpy((uint8_t *)texture.getRefBuffer().getPtr() + offset, &(data[0]), int(data.size()));
}

void modifyMapWrite(TestLog &log, de::Random &rng, const glw::Functions &gl, glu::TextureBuffer &texture)
{
    const size_t minSize = 4 * 16;
    const size_t size =
        de::max<size_t>(minSize, size_t((float)(texture.getSize() != 0 ? texture.getSize() : texture.getBufferSize()) *
                                        (0.7f + 0.3f * rng.getFloat())));
    const size_t minOffset = texture.getOffset();
    const size_t offset    = minOffset + (rng.getUint32() % (texture.getBufferSize() - (size + minOffset)));
    vector<uint8_t> data;

    genRandomCoords(rng, data, offset, size);

    log << TestLog::Message << "glMapBufferRange, Write Only, Offset: " << offset << ", Size: " << size
        << TestLog::EndMessage;

    gl.bindBuffer(GL_TEXTURE_BUFFER, texture.getGLBuffer());
    {
        uint8_t *ptr =
            (uint8_t *)gl.mapBufferRange(GL_TEXTURE_BUFFER, (glw::GLsizei)offset, (glw::GLsizei)size, GL_MAP_WRITE_BIT);

        GLU_EXPECT_NO_ERROR(gl.getError(), "glMapBufferRange()");
        TCU_CHECK(ptr);

        for (int i = 0; i < (int)data.size(); i++)
            ptr[i] = data[i];

        TCU_CHECK(gl.unmapBuffer(GL_TEXTURE_BUFFER));
    }
    gl.bindBuffer(GL_TEXTURE_BUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to update data with glMapBufferRange()");

    deMemcpy((uint8_t *)texture.getRefBuffer().getPtr() + offset, &(data[0]), int(data.size()));
}

void modifyMapReadWrite(TestLog &log, tcu::ResultCollector &resultCollector, de::Random &rng, const glw::Functions &gl,
                        glu::TextureBuffer &texture)
{
    const size_t minSize = 4 * 16;
    const size_t size =
        de::max<size_t>(minSize, size_t((float)(texture.getSize() != 0 ? texture.getSize() : texture.getBufferSize()) *
                                        (0.7f + 0.3f * rng.getFloat())));
    const size_t minOffset = texture.getOffset();
    const size_t offset    = minOffset + (rng.getUint32() % (texture.getBufferSize() - (size + minOffset)));
    uint8_t *const refPtr  = (uint8_t *)texture.getRefBuffer().getPtr() + offset;
    vector<uint8_t> data;

    genRandomCoords(rng, data, offset, size);

    log << TestLog::Message << "glMapBufferRange, Read Write, Offset: " << offset << ", Size: " << size
        << TestLog::EndMessage;

    gl.bindBuffer(GL_TEXTURE_BUFFER, texture.getGLBuffer());
    {
        size_t invalidBytes = 0;
        uint8_t *const ptr  = (uint8_t *)gl.mapBufferRange(GL_TEXTURE_BUFFER, (glw::GLsizei)offset, (glw::GLsizei)size,
                                                           GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);

        GLU_EXPECT_NO_ERROR(gl.getError(), "glMapBufferRange()");
        TCU_CHECK(ptr);

        for (int i = 0; i < (int)data.size(); i++)
        {
            if (ptr[i] != refPtr[i])
            {
                if (invalidBytes < 24)
                    log << TestLog::Message << "Invalid byte in mapped buffer. "
                        << tcu::Format::Hex<2>(data[i]).toString() << " at " << i << ", expected "
                        << tcu::Format::Hex<2>(refPtr[i]).toString() << TestLog::EndMessage;

                invalidBytes++;
            }

            ptr[i] = data[i];
        }

        TCU_CHECK(gl.unmapBuffer(GL_TEXTURE_BUFFER));

        if (invalidBytes > 0)
        {
            log << TestLog::Message << "Total of " << invalidBytes << " invalid bytes." << TestLog::EndMessage;
            resultCollector.fail("Invalid data in mapped buffer");
        }
    }

    gl.bindBuffer(GL_TEXTURE_BUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to update data with glMapBufferRange()");

    for (int i = 0; i < (int)data.size(); i++)
        refPtr[i] = data[i];
}

void modify(TestLog &log, tcu::ResultCollector &resultCollector, glu::RenderContext &renderContext,
            ModifyBits modifyBits, de::Random &rng, glu::TextureBuffer &texture)
{
    const tcu::ScopedLogSection modifySection(log, "Modifying Texture buffer", "Modifying Texture Buffer");

    logModifications(log, modifyBits);

    if (modifyBits & MODIFYBITS_BUFFERDATA)
        modifyBufferData(log, rng, texture);

    if (modifyBits & MODIFYBITS_BUFFERSUBDATA)
        modifyBufferSubData(log, rng, renderContext.getFunctions(), texture);

    if (modifyBits & MODIFYBITS_MAPBUFFER_WRITE)
        modifyMapWrite(log, rng, renderContext.getFunctions(), texture);

    if (modifyBits & MODIFYBITS_MAPBUFFER_READWRITE)
        modifyMapReadWrite(log, resultCollector, rng, renderContext.getFunctions(), texture);
}

void renderGL(glu::RenderContext &renderContext, RenderBits renderBits, uint32_t coordSeed, int triangleCount,
              glu::ShaderProgram &program, glu::TextureBuffer &texture)
{
    const glw::Functions &gl = renderContext.getFunctions();
    const glu::VertexArray vao(renderContext);
    const glu::Buffer coordBuffer(renderContext);

    gl.useProgram(program.getProgram());
    gl.bindVertexArray(*vao);

    gl.enableVertexAttribArray(0);

    if (renderBits & RENDERBITS_AS_VERTEX_ARRAY)
    {
        gl.bindBuffer(GL_ARRAY_BUFFER, texture.getGLBuffer());
        gl.vertexAttribPointer(0, 2, GL_UNSIGNED_BYTE, true, 0, nullptr);
    }
    else
    {
        de::Random rng(coordSeed);
        vector<uint8_t> coords;

        genRandomCoords(rng, coords, 0, 256 * 2);

        gl.bindBuffer(GL_ARRAY_BUFFER, *coordBuffer);
        gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizei)coords.size(), &(coords[0]), GL_STREAM_DRAW);
        gl.vertexAttribPointer(0, 2, GL_UNSIGNED_BYTE, true, 0, nullptr);
    }

    if (renderBits & RENDERBITS_AS_VERTEX_TEXTURE)
    {
        const int32_t location = gl.getUniformLocation(program.getProgram(), "u_vtxSampler");

        gl.activeTexture(GL_TEXTURE0);
        gl.bindTexture(GL_TEXTURE_BUFFER, texture.getGLTexture());
        gl.uniform1i(location, 0);
    }

    if (renderBits & RENDERBITS_AS_FRAGMENT_TEXTURE)
    {
        const int32_t location = gl.getUniformLocation(program.getProgram(), "u_fragSampler");

        gl.activeTexture(GL_TEXTURE1);
        gl.bindTexture(GL_TEXTURE_BUFFER, texture.getGLTexture());
        gl.uniform1i(location, 1);
        gl.activeTexture(GL_TEXTURE0);
    }

    if (renderBits & RENDERBITS_AS_INDEX_ARRAY)
    {
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, texture.getGLBuffer());
        gl.drawElements(GL_TRIANGLES, triangleCount * 3, GL_UNSIGNED_BYTE, nullptr);
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    else
        gl.drawArrays(GL_TRIANGLES, 0, triangleCount * 3);

    if (renderBits & RENDERBITS_AS_FRAGMENT_TEXTURE)
    {
        gl.activeTexture(GL_TEXTURE1);
        gl.bindTexture(GL_TEXTURE_BUFFER, 0);
    }

    if (renderBits & RENDERBITS_AS_VERTEX_TEXTURE)
    {
        gl.activeTexture(GL_TEXTURE0);
        gl.bindTexture(GL_TEXTURE_BUFFER, 0);
    }

    gl.bindBuffer(GL_ARRAY_BUFFER, 0);
    gl.disableVertexAttribArray(0);

    gl.bindVertexArray(0);
    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Rendering failed");
}

void renderReference(RenderBits renderBits, uint32_t coordSeed, int triangleCount, const glu::TextureBuffer &texture,
                     int maxTextureBufferSize, const tcu::PixelBufferAccess &target, int subpixelBits)
{
    const tcu::ConstPixelBufferAccess effectiveAccess =
        glu::getTextureBufferEffectiveRefTexture(texture, maxTextureBufferSize);

    const CoordVertexShader coordVertexShader;
    const TextureVertexShader textureVertexShader(effectiveAccess);
    const rr::VertexShader *const vertexShader =
        (renderBits & RENDERBITS_AS_VERTEX_TEXTURE ? static_cast<const rr::VertexShader *>(&textureVertexShader) :
                                                     &coordVertexShader);

    const CoordFragmentShader coordFragmmentShader;
    const TextureFragmentShader textureFragmentShader(effectiveAccess);
    const rr::FragmentShader *const fragmentShader =
        (renderBits & RENDERBITS_AS_FRAGMENT_TEXTURE ? static_cast<const rr::FragmentShader *>(&textureFragmentShader) :
                                                       &coordFragmmentShader);

    const rr::Renderer renderer;
    const rr::RenderState renderState(
        rr::ViewportState(rr::WindowRectangle(0, 0, target.getWidth(), target.getHeight())), subpixelBits);
    const rr::RenderTarget renderTarget(rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(target));

    const rr::Program program(vertexShader, fragmentShader);

    rr::VertexAttrib vertexAttribs[1];
    vector<uint8_t> coords;

    if (renderBits & RENDERBITS_AS_VERTEX_ARRAY)
    {
        vertexAttribs[0].type    = rr::VERTEXATTRIBTYPE_NONPURE_UNORM8;
        vertexAttribs[0].size    = 2;
        vertexAttribs[0].pointer = texture.getRefBuffer().getPtr();
    }
    else
    {
        de::Random rng(coordSeed);

        genRandomCoords(rng, coords, 0, 256 * 2);

        vertexAttribs[0].type    = rr::VERTEXATTRIBTYPE_NONPURE_UNORM8;
        vertexAttribs[0].size    = 2;
        vertexAttribs[0].pointer = &(coords[0]);
    }

    if (renderBits & RENDERBITS_AS_INDEX_ARRAY)
    {
        const rr::PrimitiveList primitives(rr::PRIMITIVETYPE_TRIANGLES, triangleCount * 3,
                                           rr::DrawIndices(texture.getRefBuffer().getPtr(), rr::INDEXTYPE_UINT8));
        const rr::DrawCommand cmd(renderState, renderTarget, program, 1, vertexAttribs, primitives);

        renderer.draw(cmd);
    }
    else
    {
        const rr::PrimitiveList primitives(rr::PRIMITIVETYPE_TRIANGLES, triangleCount * 3, 0);
        const rr::DrawCommand cmd(renderState, renderTarget, program, 1, vertexAttribs, primitives);

        renderer.draw(cmd);
    }
}

void logRendering(TestLog &log, RenderBits renderBits)
{
    const struct
    {
        RenderBits bit;
        const char *str;
    } bitInfos[] = {{RENDERBITS_AS_VERTEX_ARRAY, "vertex array"},
                    {RENDERBITS_AS_INDEX_ARRAY, "index array"},
                    {RENDERBITS_AS_VERTEX_TEXTURE, "vertex texture"},
                    {RENDERBITS_AS_FRAGMENT_TEXTURE, "fragment texture"}};

    std::ostringstream stream;
    vector<const char *> usedAs;

    DE_ASSERT(renderBits != 0);

    for (int infoNdx = 0; infoNdx < DE_LENGTH_OF_ARRAY(bitInfos); infoNdx++)
    {
        if (renderBits & bitInfos[infoNdx].bit)
            usedAs.push_back(bitInfos[infoNdx].str);
    }

    stream << "Render using texture buffer as ";

    for (int asNdx = 0; asNdx < (int)usedAs.size(); asNdx++)
    {
        if (asNdx + 1 == (int)usedAs.size() && (int)usedAs.size() > 1)
            stream << " and ";
        else if (asNdx > 0)
            stream << ", ";

        stream << usedAs[asNdx];
    }

    stream << ".";

    log << TestLog::Message << stream.str() << TestLog::EndMessage;
}

void render(TestLog &log, glu::RenderContext &renderContext, RenderBits renderBits, de::Random &rng,
            glu::ShaderProgram &program, glu::TextureBuffer &texture, const tcu::PixelBufferAccess &target)
{
    const tcu::ScopedLogSection renderSection(log, "Render Texture buffer", "Render Texture Buffer");
    const int triangleCount  = 8;
    const uint32_t coordSeed = rng.getUint32();
    int maxTextureBufferSize = 0;

    renderContext.getFunctions().getIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTextureBufferSize);
    GLU_EXPECT_NO_ERROR(renderContext.getFunctions().getError(), "query GL_MAX_TEXTURE_BUFFER_SIZE");
    DE_ASSERT(maxTextureBufferSize > 0); // checked in init()

    logRendering(log, renderBits);

    renderGL(renderContext, renderBits, coordSeed, triangleCount, program, texture);

    int subpixelBits = 0;
    renderContext.getFunctions().getIntegerv(GL_SUBPIXEL_BITS, &subpixelBits);
    renderReference(renderBits, coordSeed, triangleCount, texture, maxTextureBufferSize, target, subpixelBits);
}

void verifyScreen(TestLog &log, tcu::ResultCollector &resultCollector, glu::RenderContext &renderContext,
                  const tcu::ConstPixelBufferAccess &referenceTarget)
{
    const tcu::ScopedLogSection verifySection(log, "Verify screen contents", "Verify screen contents");
    tcu::Surface screen(referenceTarget.getWidth(), referenceTarget.getHeight());

    glu::readPixels(renderContext, 0, 0, screen.getAccess());

    if (!tcu::fuzzyCompare(log, "Result of rendering", "Result of rendering", referenceTarget, screen.getAccess(),
                           0.05f, tcu::COMPARE_LOG_RESULT))
        resultCollector.fail("Rendering failed");
}

void logImplementationInfo(TestLog &log, glu::RenderContext &renderContext)
{
    const tcu::ScopedLogSection section(log, "Implementation Values", "Implementation Values");
    de::UniquePtr<glu::ContextInfo> info(glu::ContextInfo::create(renderContext));
    const glw::Functions &gl = renderContext.getFunctions();

    if (glu::contextSupports(renderContext.getType(), glu::ApiType(3, 3, glu::PROFILE_CORE)))
    {
        int32_t maxTextureSize = 0;

        gl.getIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTextureSize);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE)");

        log << TestLog::Message << "GL_MAX_TEXTURE_BUFFER_SIZE : " << maxTextureSize << TestLog::EndMessage;
    }
    else if (glu::contextSupports(renderContext.getType(), glu::ApiType(3, 1, glu::PROFILE_ES)) &&
             info->isExtensionSupported("GL_EXT_texture_buffer"))
    {
        {
            int32_t maxTextureSize = 0;

            gl.getIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTextureSize);
            GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE_EXT)");

            log << TestLog::Message << "GL_MAX_TEXTURE_BUFFER_SIZE_EXT : " << maxTextureSize << TestLog::EndMessage;
        }

        {
            int32_t textureBufferAlignment = 0;

            gl.getIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &textureBufferAlignment);
            GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT_EXT)");

            log << TestLog::Message << "GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT_EXT : " << textureBufferAlignment
                << TestLog::EndMessage;
        }
    }
    else
        DE_ASSERT(false);
}

void logTextureInfo(TestLog &log, uint32_t format, size_t bufferSize, size_t offset, size_t size)
{
    const tcu::ScopedLogSection section(log, "Texture Info", "Texture Info");

    log << TestLog::Message << "Texture format : " << glu::getTextureFormatStr(format) << TestLog::EndMessage;
    log << TestLog::Message << "Buffer size : " << bufferSize << TestLog::EndMessage;

    if (offset != 0 || size != 0)
    {
        log << TestLog::Message << "Buffer range offset: " << offset << TestLog::EndMessage;
        log << TestLog::Message << "Buffer range size: " << size << TestLog::EndMessage;
    }
}

void runTests(tcu::TestContext &testCtx, glu::RenderContext &renderContext, de::Random &rng, uint32_t format,
              size_t bufferSize, size_t offset, size_t size, RenderBits preRender, glu::ShaderProgram *preRenderProgram,
              ModifyBits modifyType, RenderBits postRender, glu::ShaderProgram *postRenderProgram)
{
    const tcu::RenderTarget renderTarget(renderContext.getRenderTarget());
    const glw::Functions &gl = renderContext.getFunctions();

    const int width  = de::min<int>(renderTarget.getWidth(), MAX_VIEWPORT_WIDTH);
    const int height = de::min<int>(renderTarget.getHeight(), MAX_VIEWPORT_HEIGHT);
    const tcu::Vec4 clearColor(0.25f, 0.5f, 0.75f, 1.0f);

    TestLog &log = testCtx.getLog();
    tcu::ResultCollector resultCollector(log);

    logImplementationInfo(log, renderContext);
    logTextureInfo(log, format, bufferSize, offset, size);

    {
        tcu::Surface referenceTarget(width, height);
        vector<uint8_t> bufferData;

        genRandomCoords(rng, bufferData, 0, bufferSize);

        for (uint8_t i = 0; i < 4; i++)
        {
            const uint8_t val = extend2BitsToByte(i);

            if (val >= offset && val < offset + size)
            {
                bufferData[val * 2 + 0] = (i / 2 == 0 ? extend2BitsToByte(0x2u) : extend2BitsToByte(0x01u));
                bufferData[val * 2 + 1] = (i % 2 == 0 ? extend2BitsToByte(0x2u) : extend2BitsToByte(0x01u));
            }
        }

        {
            glu::TextureBuffer texture(renderContext, format, bufferSize, offset, size, &(bufferData[0]));

            TCU_CHECK_MSG(width >= MIN_VIEWPORT_WIDTH || height >= MIN_VIEWPORT_HEIGHT, "Too small viewport");

            DE_ASSERT(preRender == 0 || preRenderProgram);
            DE_ASSERT(postRender == 0 || postRenderProgram);

            gl.viewport(0, 0, width, height);
            gl.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
            gl.clear(GL_COLOR_BUFFER_BIT);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Screen setup failed");

            tcu::clear(referenceTarget.getAccess(), clearColor);

            texture.upload();

            if (preRender != 0)
                render(log, renderContext, preRender, rng, *preRenderProgram, texture, referenceTarget.getAccess());

            if (modifyType != 0)
                modify(log, resultCollector, renderContext, modifyType, rng, texture);

            if (postRender != 0)
                render(log, renderContext, postRender, rng, *postRenderProgram, texture, referenceTarget.getAccess());
        }

        verifyScreen(log, resultCollector, renderContext, referenceTarget.getAccess());

        resultCollector.setTestContextResult(testCtx);
    }
}

} // namespace

TextureBufferCase::TextureBufferCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, uint32_t format,
                                     size_t bufferSize, size_t offset, size_t size, RenderBits preRender,
                                     ModifyBits modify, RenderBits postRender, const char *name,
                                     const char *description)
    : tcu::TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_format(format)
    , m_bufferSize(bufferSize)
    , m_offset(offset)
    , m_size(size)

    , m_preRender(preRender)
    , m_modify(modify)
    , m_postRender(postRender)

    , m_preRenderProgram(nullptr)
    , m_postRenderProgram(nullptr)
{
}

TextureBufferCase::~TextureBufferCase(void)
{
    TextureBufferCase::deinit();
}

void TextureBufferCase::init(void)
{
    de::UniquePtr<glu::ContextInfo> info(glu::ContextInfo::create(m_renderCtx));

    if (!glu::contextSupports(m_renderCtx.getType(), glu::ApiType(3, 3, glu::PROFILE_CORE)) &&
        !(glu::contextSupports(m_renderCtx.getType(), glu::ApiType(3, 1, glu::PROFILE_ES)) &&
          info->isExtensionSupported("GL_EXT_texture_buffer")))
        throw tcu::NotSupportedError("Texture buffers not supported", "", __FILE__, __LINE__);

    {
        const int maxTextureBufferSize = info->getInt(GL_MAX_TEXTURE_BUFFER_SIZE);
        if (maxTextureBufferSize <= 0)
            TCU_THROW(NotSupportedError, "GL_MAX_TEXTURE_BUFFER_SIZE > 0 required");
    }

    if (m_preRender != 0)
    {
        TestLog &log                  = m_testCtx.getLog();
        const char *const sectionName = (m_postRender != 0 ? "Primary render program" : "Render program");
        const tcu::ScopedLogSection section(log, sectionName, sectionName);

        m_preRenderProgram = createRenderProgram(m_renderCtx, m_preRender);
        m_testCtx.getLog() << (*m_preRenderProgram);

        TCU_CHECK(m_preRenderProgram->isOk());
    }

    if (m_postRender != 0)
    {
        // Reusing program
        if (m_preRender == m_postRender)
        {
            m_postRenderProgram = m_preRenderProgram;
        }
        else
        {
            TestLog &log                  = m_testCtx.getLog();
            const char *const sectionName = (m_preRender != 0 ? "Secondary render program" : "Render program");
            const tcu::ScopedLogSection section(log, sectionName, sectionName);

            m_postRenderProgram = createRenderProgram(m_renderCtx, m_postRender);
            m_testCtx.getLog() << (*m_postRenderProgram);

            TCU_CHECK(m_postRenderProgram->isOk());
        }
    }
}

void TextureBufferCase::deinit(void)
{
    if (m_preRenderProgram == m_postRenderProgram)
        m_postRenderProgram = nullptr;

    delete m_preRenderProgram;
    m_preRenderProgram = nullptr;

    delete m_postRenderProgram;
    m_postRenderProgram = nullptr;
}

tcu::TestCase::IterateResult TextureBufferCase::iterate(void)
{
    de::Random rng(deInt32Hash(deStringHash(getName())));
    size_t offset;

    if (m_offset != 0)
    {
        const glw::Functions &gl = m_renderCtx.getFunctions();
        int32_t alignment        = 0;

        gl.getIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &alignment);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT)");

        offset = m_offset * alignment;
    }
    else
        offset = 0;

    runTests(m_testCtx, m_renderCtx, rng, m_format, m_bufferSize, offset, m_size, m_preRender, m_preRenderProgram,
             m_modify, m_postRender, m_postRenderProgram);

    return STOP;
}

} // namespace gls
} // namespace deqp
