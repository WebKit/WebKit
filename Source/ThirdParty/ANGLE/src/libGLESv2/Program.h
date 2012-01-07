//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.h: Defines the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#ifndef LIBGLESV2_PROGRAM_H_
#define LIBGLESV2_PROGRAM_H_

#include <d3dx9.h>
#include <d3dcompiler.h>
#include <string>
#include <vector>
#include <set>

#include "libGLESv2/Shader.h"
#include "libGLESv2/Context.h"

namespace gl
{
class ResourceManager;
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
        int registerSet;
        int registerIndex;
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

class Program
{
  public:
    Program(ResourceManager *manager, GLuint handle);

    ~Program();

    bool attachShader(Shader *shader);
    bool detachShader(Shader *shader);
    int getAttachedShadersCount() const;

    IDirect3DPixelShader9 *getPixelShader();
    IDirect3DVertexShader9 *getVertexShader();

    void bindAttributeLocation(GLuint index, const char *name);
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

    void link();
    bool isLinked();
    int getInfoLogLength() const;
    void getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog);
    void getAttachedShaders(GLsizei maxCount, GLsizei *count, GLuint *shaders);

    void getActiveAttribute(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    GLint getActiveAttributeCount();
    GLint getActiveAttributeMaxLength();

    void getActiveUniform(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    GLint getActiveUniformCount();
    GLint getActiveUniformMaxLength();

    void addRef();
    void release();
    unsigned int getRefCount() const;
    void flagForDeletion();
    bool isFlaggedForDeletion() const;

    void validate();
    bool validateSamplers(bool logErrors);
    bool isValidated() const;

    unsigned int getSerial() const;

    static std::string decorateAttribute(const std::string &name);    // Prepend an underscore
    static std::string undecorateUniform(const std::string &_name);   // Remove leading underscore

  private:
    DISALLOW_COPY_AND_ASSIGN(Program);

    ID3D10Blob *compileToBinary(const char *hlsl, const char *profile, ID3DXConstantTable **constantTable);
    void unlink(bool destroy = false);

    int packVaryings(const Varying *packing[][4]);
    bool linkVaryings();

    bool linkAttributes();
    int getAttributeBinding(const std::string &name);

    bool linkUniforms(ID3DXConstantTable *constantTable);
    bool defineUniform(const D3DXHANDLE &constantHandle, const D3DXCONSTANT_DESC &constantDescription, std::string name = "");
    bool defineUniform(const D3DXCONSTANT_DESC &constantDescription, std::string &name);
    Uniform *createUniform(const D3DXCONSTANT_DESC &constantDescription, std::string &name);
    bool applyUniformnfv(Uniform *targetUniform, const GLfloat *v);
    bool applyUniform1iv(Uniform *targetUniform, GLsizei count, const GLint *v);
    bool applyUniform2iv(Uniform *targetUniform, GLsizei count, const GLint *v);
    bool applyUniform3iv(Uniform *targetUniform, GLsizei count, const GLint *v);
    bool applyUniform4iv(Uniform *targetUniform, GLsizei count, const GLint *v);
    void applyUniformniv(Uniform *targetUniform, GLsizei count, const D3DXVECTOR4 *vector);
    void applyUniformnbv(Uniform *targetUniform, GLsizei count, int width, const GLboolean *v);

    void initializeConstantHandles(Uniform *targetUniform, Uniform::RegisterInfo *rs, ID3DXConstantTable *constantTable);

    void appendToInfoLogSanitized(const char *message);
    void appendToInfoLog(const char *info, ...);
    void resetInfoLog();

    static unsigned int issueSerial();

    IDirect3DDevice9 *mDevice;
    FragmentShader *mFragmentShader;
    VertexShader *mVertexShader;

    std::string mPixelHLSL;
    std::string mVertexHLSL;

    IDirect3DPixelShader9 *mPixelExecutable;
    IDirect3DVertexShader9 *mVertexExecutable;
    ID3DXConstantTable *mConstantTablePS;
    ID3DXConstantTable *mConstantTableVS;

    std::set<std::string> mAttributeBinding[MAX_VERTEX_ATTRIBS];
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

    bool mLinked;
    bool mDeleteStatus;   // Flag to indicate that the program can be deleted when no longer in use
    char *mInfoLog;
    bool mValidated;

    unsigned int mRefCount;

    const unsigned int mSerial;

    static unsigned int mCurrentSerial;

    ResourceManager *mResourceManager;
    const GLuint mHandle;
};
}

#endif   // LIBGLESV2_PROGRAM_H_
