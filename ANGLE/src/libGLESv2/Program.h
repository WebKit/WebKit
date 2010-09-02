//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.h: Defines the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#ifndef LIBGLESV2_PROGRAM_H_
#define LIBGLESV2_PROGRAM_H_

#include <d3dx9.h>
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
    Uniform(GLenum type, const std::string &name, unsigned int arraySize);

    ~Uniform();

    const GLenum type;
    const std::string name;
    const unsigned int arraySize;

    unsigned char *data;
    bool dirty;

    D3DXHANDLE vsHandle;
    D3DXHANDLE psHandle;
    bool handlesSet;
};

// Struct used for correlating uniforms/elements of uniform arrays to handles
struct UniformLocation
{
    UniformLocation(const std::string &name, unsigned int element, unsigned int index);

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

    void dirtyAllSamplers();

    GLint getSamplerMapping(unsigned int samplerIndex);
    SamplerType getSamplerType(unsigned int samplerIndex);
    bool isSamplerDirty(unsigned int samplerIndex) const;
    void setSamplerDirty(unsigned int samplerIndex, bool dirty);

    GLint getUniformLocation(const char *name, bool decorated);
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

    bool getUniformfv(GLint location, GLfloat *params);
    bool getUniformiv(GLint location, GLint *params);

    GLint getDepthRangeDiffLocation() const;
    GLint getDepthRangeNearLocation() const;
    GLint getDepthRangeFarLocation() const;
    GLint getDxDepthLocation() const;
    GLint getDxViewportLocation() const;
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
    bool validateSamplers() const;
    bool isValidated() const;

    unsigned int getSerial() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Program);

    ID3DXBuffer *compileToBinary(const char *hlsl, const char *profile, ID3DXConstantTable **constantTable);
    void unlink(bool destroy = false);

    int packVaryings(const Varying *packing[][4]);
    bool linkVaryings();

    bool linkAttributes();
    int getAttributeBinding(const std::string &name);

    bool linkUniforms(ID3DXConstantTable *constantTable);
    bool defineUniform(const D3DXHANDLE &constantHandle, const D3DXCONSTANT_DESC &constantDescription, std::string name = "");
    bool defineUniform(const D3DXCONSTANT_DESC &constantDescription, std::string &name);
    Uniform *createUniform(const D3DXCONSTANT_DESC &constantDescription, std::string &name);
    bool applyUniform1bv(GLint location, GLsizei count, const GLboolean *v);
    bool applyUniform2bv(GLint location, GLsizei count, const GLboolean *v);
    bool applyUniform3bv(GLint location, GLsizei count, const GLboolean *v);
    bool applyUniform4bv(GLint location, GLsizei count, const GLboolean *v);
    bool applyUniform1fv(GLint location, GLsizei count, const GLfloat *v);
    bool applyUniform2fv(GLint location, GLsizei count, const GLfloat *v);
    bool applyUniform3fv(GLint location, GLsizei count, const GLfloat *v);
    bool applyUniform4fv(GLint location, GLsizei count, const GLfloat *v);
    bool applyUniformMatrix2fv(GLint location, GLsizei count, const GLfloat *value);
    bool applyUniformMatrix3fv(GLint location, GLsizei count, const GLfloat *value);
    bool applyUniformMatrix4fv(GLint location, GLsizei count, const GLfloat *value);
    bool applyUniform1iv(GLint location, GLsizei count, const GLint *v);
    bool applyUniform2iv(GLint location, GLsizei count, const GLint *v);
    bool applyUniform3iv(GLint location, GLsizei count, const GLint *v);
    bool applyUniform4iv(GLint location, GLsizei count, const GLint *v);

    void getConstantHandles(Uniform *targetUniform, D3DXHANDLE *constantPS, D3DXHANDLE *constantVS);

    void appendToInfoLog(const char *info, ...);
    void resetInfoLog();

    static std::string decorate(const std::string &string);     // Prepend an underscore
    static std::string undecorate(const std::string &string);   // Remove leading underscore

    static unsigned int issueSerial();

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
        SamplerType type;
        bool dirty;
    };

    Sampler mSamplers[MAX_TEXTURE_IMAGE_UNITS];

    typedef std::vector<Uniform*> UniformArray;
    UniformArray mUniforms;
    typedef std::vector<UniformLocation> UniformIndex;
    UniformIndex mUniformIndex;

    GLint mDepthRangeDiffLocation;
    GLint mDepthRangeNearLocation;
    GLint mDepthRangeFarLocation;
    GLint mDxDepthLocation;
    GLint mDxViewportLocation;
    GLint mDxHalfPixelSizeLocation;
    GLint mDxFrontCCWLocation;
    GLint mDxPointsOrLinesLocation;

    bool mLinked;
    bool mDeleteStatus;   // Flag to indicate that the program can be deleted when no longer in use
    char *mInfoLog;
    bool mValidated;

    unsigned int mRefCount;

    unsigned int mSerial;

    static unsigned int mCurrentSerial;

    ResourceManager *mResourceManager;
    const GLuint mHandle;
};
}

#endif   // LIBGLESV2_PROGRAM_H_
