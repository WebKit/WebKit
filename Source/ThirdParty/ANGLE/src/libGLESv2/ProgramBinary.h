//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.h: Defines the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#ifndef LIBGLESV2_PROGRAM_BINARY_H_
#define LIBGLESV2_PROGRAM_BINARY_H_

#include <d3dx9.h>
#include <d3dcompiler.h>
#include <string>
#include <vector>

#include "libGLESv2/Shader.h"
#include "libGLESv2/Context.h"

namespace gl
{
class FragmentShader;
class VertexShader;

// Helper struct representing a single shader uniform
struct Uniform
{
    Uniform(GLenum type, const std::string &_name, unsigned int arraySize);

    ~Uniform();

    bool isArray();

    const GLenum type;
    const std::string _name;   // Decorated name
    const std::string name;    // Undecorated name
    const unsigned int arraySize;

    unsigned char *data;
    bool dirty;

    struct RegisterInfo
    {
        RegisterInfo()
        {
            float4Index = -1;
            samplerIndex = -1;
            boolIndex = -1;
            registerCount = 0;
        }

        void set(const D3DXCONSTANT_DESC &constantDescription)
        {
            switch(constantDescription.RegisterSet)
            {
              case D3DXRS_BOOL:    boolIndex = constantDescription.RegisterIndex;    break;
              case D3DXRS_FLOAT4:  float4Index = constantDescription.RegisterIndex;  break;
              case D3DXRS_SAMPLER: samplerIndex = constantDescription.RegisterIndex; break;
              default: UNREACHABLE();
            }
            
            ASSERT(registerCount == 0 || registerCount == (int)constantDescription.RegisterCount);
            registerCount = constantDescription.RegisterCount;
        }

        int float4Index;
        int samplerIndex;
        int boolIndex;

        int registerCount;
    };

    RegisterInfo ps;
    RegisterInfo vs;
};

// Struct used for correlating uniforms/elements of uniform arrays to handles
struct UniformLocation
{
    UniformLocation(const std::string &_name, unsigned int element, unsigned int index);

    std::string name;
    unsigned int element;
    unsigned int index;
};

// This is the result of linking a program. It is the state that would be passed to ProgramBinary.
class ProgramBinary
{
  public:
    ProgramBinary();
    ~ProgramBinary();

    IDirect3DPixelShader9 *getPixelShader();
    IDirect3DVertexShader9 *getVertexShader();

    GLuint getAttributeLocation(const char *name);
    int getSemanticIndex(int attributeIndex);

    GLint getSamplerMapping(SamplerType type, unsigned int samplerIndex);
    TextureType getSamplerTextureType(SamplerType type, unsigned int samplerIndex);
    GLint getUsedSamplerRange(SamplerType type);

    GLint getUniformLocation(std::string name);
    bool setUniform1fv(GLint location, GLsizei count, const GLfloat *v);
    bool setUniform2fv(GLint location, GLsizei count, const GLfloat *v);
    bool setUniform3fv(GLint location, GLsizei count, const GLfloat *v);
    bool setUniform4fv(GLint location, GLsizei count, const GLfloat *v);
    bool setUniformMatrix2fv(GLint location, GLsizei count, const GLfloat *value);
    bool setUniformMatrix3fv(GLint location, GLsizei count, const GLfloat *value);
    bool setUniformMatrix4fv(GLint location, GLsizei count, const GLfloat *value);
    bool setUniform1iv(GLint location, GLsizei count, const GLint *v);
    bool setUniform2iv(GLint location, GLsizei count, const GLint *v);
    bool setUniform3iv(GLint location, GLsizei count, const GLint *v);
    bool setUniform4iv(GLint location, GLsizei count, const GLint *v);

    bool getUniformfv(GLint location, GLsizei *bufSize, GLfloat *params);
    bool getUniformiv(GLint location, GLsizei *bufSize, GLint *params);

    GLint getDxDepthRangeLocation() const;
    GLint getDxDepthLocation() const;
    GLint getDxCoordLocation() const;
    GLint getDxHalfPixelSizeLocation() const;
    GLint getDxFrontCCWLocation() const;
    GLint getDxPointsOrLinesLocation() const;

    void dirtyAllUniforms();
    void applyUniforms();

    bool link(InfoLog &infoLog, const AttributeBindings &attributeBindings, FragmentShader *fragmentShader, VertexShader *vertexShader);
    void getAttachedShaders(GLsizei maxCount, GLsizei *count, GLuint *shaders);

    void getActiveAttribute(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    GLint getActiveAttributeCount();
    GLint getActiveAttributeMaxLength();

    void getActiveUniform(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    GLint getActiveUniformCount();
    GLint getActiveUniformMaxLength();

    void validate(InfoLog &infoLog);
    bool validateSamplers(InfoLog *infoLog);
    bool isValidated() const;

    static std::string decorateAttribute(const std::string &name);    // Prepend an underscore
    static std::string undecorateUniform(const std::string &_name);   // Remove leading underscore

  private:
    DISALLOW_COPY_AND_ASSIGN(ProgramBinary);

    ID3D10Blob *compileToBinary(InfoLog &infoLog, const char *hlsl, const char *profile, ID3DXConstantTable **constantTable);

    int packVaryings(InfoLog &infoLog, const Varying *packing[][4], FragmentShader *fragmentShader);
    bool linkVaryings(InfoLog &infoLog, std::string& pixelHLSL, std::string& vertexHLSL, FragmentShader *fragmentShader, VertexShader *vertexShader);

    bool linkAttributes(InfoLog &infoLog, const AttributeBindings &attributeBindings, FragmentShader *fragmentShader, VertexShader *vertexShader);

    bool linkUniforms(InfoLog &infoLog, GLenum shader, ID3DXConstantTable *constantTable);
    bool defineUniform(InfoLog &infoLog, GLenum shader, const D3DXHANDLE &constantHandle, const D3DXCONSTANT_DESC &constantDescription, std::string name = "");
    bool defineUniform(GLenum shader, const D3DXCONSTANT_DESC &constantDescription, const std::string &name);
    Uniform *createUniform(const D3DXCONSTANT_DESC &constantDescription, const std::string &name);
    bool applyUniformnfv(Uniform *targetUniform, const GLfloat *v);
    bool applyUniform1iv(Uniform *targetUniform, GLsizei count, const GLint *v);
    bool applyUniform2iv(Uniform *targetUniform, GLsizei count, const GLint *v);
    bool applyUniform3iv(Uniform *targetUniform, GLsizei count, const GLint *v);
    bool applyUniform4iv(Uniform *targetUniform, GLsizei count, const GLint *v);
    void applyUniformniv(Uniform *targetUniform, GLsizei count, const D3DXVECTOR4 *vector);
    void applyUniformnbv(Uniform *targetUniform, GLsizei count, int width, const GLboolean *v);

    IDirect3DDevice9 *mDevice;

    IDirect3DPixelShader9 *mPixelExecutable;
    IDirect3DVertexShader9 *mVertexExecutable;

    // These are only used during linking.
    ID3DXConstantTable *mConstantTablePS;
    ID3DXConstantTable *mConstantTableVS;

    Attribute mLinkedAttribute[MAX_VERTEX_ATTRIBS];
    int mSemanticIndex[MAX_VERTEX_ATTRIBS];

    struct Sampler
    {
        bool active;
        GLint logicalTextureUnit;
        TextureType textureType;
    };

    Sampler mSamplersPS[MAX_TEXTURE_IMAGE_UNITS];
    Sampler mSamplersVS[MAX_VERTEX_TEXTURE_IMAGE_UNITS_VTF];
    GLuint mUsedVertexSamplerRange;
    GLuint mUsedPixelSamplerRange;

    typedef std::vector<Uniform*> UniformArray;
    UniformArray mUniforms;
    typedef std::vector<UniformLocation> UniformIndex;
    UniformIndex mUniformIndex;

    GLint mDxDepthRangeLocation;
    GLint mDxDepthLocation;
    GLint mDxCoordLocation;
    GLint mDxHalfPixelSizeLocation;
    GLint mDxFrontCCWLocation;
    GLint mDxPointsOrLinesLocation;

    bool mValidated;
};
}

#endif   // LIBGLESV2_PROGRAM_BINARY_H_
