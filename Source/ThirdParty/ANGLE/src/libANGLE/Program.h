//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.h: Defines the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#ifndef LIBANGLE_PROGRAM_H_
#define LIBANGLE_PROGRAM_H_

#include <GLES2/gl2.h>
#include <GLSLANG/ShaderLang.h>

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "common/angleutils.h"
#include "common/mathutil.h"
#include "common/Optional.h"

#include "libANGLE/angletypes.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"

namespace rx
{
class GLImplFactory;
class ProgramImpl;
struct TranslatedAttribute;
}

namespace gl
{
struct Caps;
class ContextState;
class ResourceManager;
class Shader;
class InfoLog;
class Buffer;
class Framebuffer;
struct UniformBlock;
struct LinkedUniform;

extern const char * const g_fakepath;

class InfoLog : angle::NonCopyable
{
  public:
    InfoLog();
    ~InfoLog();

    size_t getLength() const;
    void getLog(GLsizei bufSize, GLsizei *length, char *infoLog) const;

    void appendSanitized(const char *message);
    void reset();

    // This helper class ensures we append a newline after writing a line.
    class StreamHelper : angle::NonCopyable
    {
      public:
        StreamHelper(StreamHelper &&rhs)
            : mStream(rhs.mStream)
        {
            rhs.mStream = nullptr;
        }

        StreamHelper &operator=(StreamHelper &&rhs)
        {
            std::swap(mStream, rhs.mStream);
            return *this;
        }

        ~StreamHelper()
        {
            // Write newline when destroyed on the stack
            if (mStream)
            {
                (*mStream) << std::endl;
            }
        }

        template <typename T>
        StreamHelper &operator<<(const T &value)
        {
            (*mStream) << value;
            return *this;
        }

      private:
        friend class InfoLog;

        StreamHelper(std::stringstream *stream)
            : mStream(stream)
        {
            ASSERT(stream);
        }

        std::stringstream *mStream;
    };

    template <typename T>
    StreamHelper operator<<(const T &value)
    {
        StreamHelper helper(&mStream);
        helper << value;
        return helper;
    }

    std::string str() const { return mStream.str(); }

  private:
    std::stringstream mStream;
};

// Struct used for correlating uniforms/elements of uniform arrays to handles
struct VariableLocation
{
    VariableLocation();
    VariableLocation(const std::string &name, unsigned int element, unsigned int index);

    std::string name;
    unsigned int element;
    unsigned int index;

    // If this is a valid uniform location
    bool used;

    // If this location was bound to an unreferenced uniform.  Setting data on this uniform is a
    // no-op.
    bool ignored;
};

// Information about a variable binding.
// Currently used by CHROMIUM_path_rendering
struct BindingInfo
{
    // The type of binding, for example GL_FLOAT_VEC3.
    // This can be GL_NONE if the variable is optimized away.
    GLenum type;

    // This is the name of the variable in
    // the translated shader program. Note that
    // this can be empty in the case where the
    // variable has been optimized away.
    std::string name;

    // True if the binding is valid, otherwise false.
    bool valid;
};

class ProgramState final : angle::NonCopyable
{
  public:
    ProgramState();
    ~ProgramState();

    const std::string &getLabel();

    const Shader *getAttachedVertexShader() const { return mAttachedVertexShader; }
    const Shader *getAttachedFragmentShader() const { return mAttachedFragmentShader; }
    const Shader *getAttachedComputeShader() const { return mAttachedComputeShader; }
    const std::vector<std::string> &getTransformFeedbackVaryingNames() const
    {
        return mTransformFeedbackVaryingNames;
    }
    GLint getTransformFeedbackBufferMode() const { return mTransformFeedbackBufferMode; }
    GLuint getUniformBlockBinding(GLuint uniformBlockIndex) const
    {
        ASSERT(uniformBlockIndex < IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS);
        return mUniformBlockBindings[uniformBlockIndex];
    }
    const UniformBlockBindingMask &getActiveUniformBlockBindingsMask() const
    {
        return mActiveUniformBlockBindings;
    }
    const std::vector<sh::Attribute> &getAttributes() const { return mAttributes; }
    const AttributesMask &getActiveAttribLocationsMask() const
    {
        return mActiveAttribLocationsMask;
    }
    const std::map<int, VariableLocation> &getOutputVariables() const { return mOutputVariables; }
    const std::vector<LinkedUniform> &getUniforms() const { return mUniforms; }
    const std::vector<VariableLocation> &getUniformLocations() const { return mUniformLocations; }
    const std::vector<UniformBlock> &getUniformBlocks() const { return mUniformBlocks; }

    const LinkedUniform *getUniformByName(const std::string &name) const;
    GLint getUniformLocation(const std::string &name) const;
    GLuint getUniformIndex(const std::string &name) const;

  private:
    friend class Program;

    std::string mLabel;

    sh::WorkGroupSize mComputeShaderLocalSize;

    Shader *mAttachedFragmentShader;
    Shader *mAttachedVertexShader;
    Shader *mAttachedComputeShader;

    std::vector<std::string> mTransformFeedbackVaryingNames;
    std::vector<sh::Varying> mTransformFeedbackVaryingVars;
    GLenum mTransformFeedbackBufferMode;

    GLuint mUniformBlockBindings[IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS];
    UniformBlockBindingMask mActiveUniformBlockBindings;

    std::vector<sh::Attribute> mAttributes;
    std::bitset<MAX_VERTEX_ATTRIBS> mActiveAttribLocationsMask;

    // Uniforms are sorted in order:
    //  1. Non-sampler uniforms
    //  2. Sampler uniforms
    //  3. Uniform block uniforms
    // This makes sampler validation easier, since we don't need a separate list.
    std::vector<LinkedUniform> mUniforms;
    std::vector<VariableLocation> mUniformLocations;
    std::vector<UniformBlock> mUniformBlocks;

    // TODO(jmadill): use unordered/hash map when available
    std::map<int, VariableLocation> mOutputVariables;

    bool mBinaryRetrieveableHint;
};

class Program final : angle::NonCopyable, public LabeledObject
{
  public:
    Program(rx::GLImplFactory *factory, ResourceManager *manager, GLuint handle);
    ~Program();

    GLuint id() const { return mHandle; }

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    rx::ProgramImpl *getImplementation() const { return mProgram; }

    void attachShader(Shader *shader);
    bool detachShader(Shader *shader);
    int getAttachedShadersCount() const;

    const Shader *getAttachedVertexShader() const { return mState.mAttachedVertexShader; }
    const Shader *getAttachedFragmentShader() const { return mState.mAttachedFragmentShader; }
    const Shader *getAttachedComputeShader() const { return mState.mAttachedComputeShader; }

    void bindAttributeLocation(GLuint index, const char *name);
    void bindUniformLocation(GLuint index, const char *name);

    // CHROMIUM_path_rendering
    BindingInfo getFragmentInputBindingInfo(GLint index) const;
    void bindFragmentInputLocation(GLint index, const char *name);
    void pathFragmentInputGen(GLint index,
                              GLenum genMode,
                              GLint components,
                              const GLfloat *coeffs);

    Error link(const ContextState &data);
    bool isLinked() const;

    Error loadBinary(GLenum binaryFormat, const void *binary, GLsizei length);
    Error saveBinary(GLenum *binaryFormat, void *binary, GLsizei bufSize, GLsizei *length) const;
    GLint getBinaryLength() const;
    void setBinaryRetrievableHint(bool retrievable);
    bool getBinaryRetrievableHint() const;

    int getInfoLogLength() const;
    void getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const;
    void getAttachedShaders(GLsizei maxCount, GLsizei *count, GLuint *shaders) const;

    GLuint getAttributeLocation(const std::string &name) const;
    bool isAttribLocationActive(size_t attribLocation) const;

    void getActiveAttribute(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    GLint getActiveAttributeCount() const;
    GLint getActiveAttributeMaxLength() const;
    const std::vector<sh::Attribute> &getAttributes() const { return mState.mAttributes; }

    GLint getFragDataLocation(const std::string &name) const;

    void getActiveUniform(GLuint index,
                          GLsizei bufsize,
                          GLsizei *length,
                          GLint *size,
                          GLenum *type,
                          GLchar *name) const;
    GLint getActiveUniformCount() const;
    GLint getActiveUniformMaxLength() const;
    GLint getActiveUniformi(GLuint index, GLenum pname) const;
    bool isValidUniformLocation(GLint location) const;
    bool isIgnoredUniformLocation(GLint location) const;
    const LinkedUniform &getUniformByLocation(GLint location) const;

    GLint getUniformLocation(const std::string &name) const;
    GLuint getUniformIndex(const std::string &name) const;
    void setUniform1fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform2fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform3fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform4fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform1iv(GLint location, GLsizei count, const GLint *v);
    void setUniform2iv(GLint location, GLsizei count, const GLint *v);
    void setUniform3iv(GLint location, GLsizei count, const GLint *v);
    void setUniform4iv(GLint location, GLsizei count, const GLint *v);
    void setUniform1uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform2uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform3uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform4uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

    void getUniformfv(GLint location, GLfloat *params) const;
    void getUniformiv(GLint location, GLint *params) const;
    void getUniformuiv(GLint location, GLuint *params) const;

    void getActiveUniformBlockName(GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName) const;
    GLuint getActiveUniformBlockCount() const;
    GLint getActiveUniformBlockMaxLength() const;

    GLuint getUniformBlockIndex(const std::string &name) const;

    void bindUniformBlock(GLuint uniformBlockIndex, GLuint uniformBlockBinding);
    GLuint getUniformBlockBinding(GLuint uniformBlockIndex) const;

    const UniformBlock &getUniformBlockByIndex(GLuint index) const;

    void setTransformFeedbackVaryings(GLsizei count, const GLchar *const *varyings, GLenum bufferMode);
    void getTransformFeedbackVarying(GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name) const;
    GLsizei getTransformFeedbackVaryingCount() const;
    GLsizei getTransformFeedbackVaryingMaxLength() const;
    GLenum getTransformFeedbackBufferMode() const;

    static bool linkValidateUniforms(InfoLog &infoLog, const std::string &uniformName, const sh::Uniform &vertexUniform, const sh::Uniform &fragmentUniform);
    static bool linkValidateInterfaceBlockFields(InfoLog &infoLog, const std::string &uniformName, const sh::InterfaceBlockField &vertexUniform, const sh::InterfaceBlockField &fragmentUniform);

    void addRef();
    void release();
    unsigned int getRefCount() const;
    void flagForDeletion();
    bool isFlaggedForDeletion() const;

    void validate(const Caps &caps);
    bool validateSamplers(InfoLog *infoLog, const Caps &caps);
    bool isValidated() const;

    const AttributesMask &getActiveAttribLocationsMask() const
    {
        return mState.mActiveAttribLocationsMask;
    }

  private:
    class Bindings final : angle::NonCopyable
    {
      public:
        void bindLocation(GLuint index, const std::string &name);
        int getBinding(const std::string &name) const;

        typedef std::unordered_map<std::string, GLuint>::const_iterator const_iterator;
        const_iterator begin() const;
        const_iterator end() const;

      private:
        std::unordered_map<std::string, GLuint> mBindings;
    };

    void unlink(bool destroy = false);
    void resetUniformBlockBindings();

    bool linkAttributes(const ContextState &data,
                        InfoLog &infoLog,
                        const Bindings &attributeBindings,
                        const Shader *vertexShader);
    bool validateUniformBlocksCount(GLuint maxUniformBlocks,
                                    const std::vector<sh::InterfaceBlock> &block,
                                    const std::string &errorMessage,
                                    InfoLog &infoLog) const;
    bool validateVertexAndFragmentInterfaceBlocks(
        const std::vector<sh::InterfaceBlock> &vertexInterfaceBlocks,
        const std::vector<sh::InterfaceBlock> &fragmentInterfaceBlocks,
        InfoLog &infoLog) const;
    bool linkUniformBlocks(InfoLog &infoLog, const Caps &caps);
    bool linkVaryings(InfoLog &infoLog, const Shader *vertexShader, const Shader *fragmentShader) const;
    bool validateVertexAndFragmentUniforms(InfoLog &infoLog) const;
    bool linkUniforms(gl::InfoLog &infoLog, const gl::Caps &caps, const Bindings &uniformBindings);
    bool indexUniforms(gl::InfoLog &infoLog, const gl::Caps &caps, const Bindings &uniformBindings);
    bool areMatchingInterfaceBlocks(gl::InfoLog &infoLog,
                                    const sh::InterfaceBlock &vertexInterfaceBlock,
                                    const sh::InterfaceBlock &fragmentInterfaceBlock) const;

    static bool linkValidateVariablesBase(InfoLog &infoLog,
                                          const std::string &variableName,
                                          const sh::ShaderVariable &vertexVariable,
                                          const sh::ShaderVariable &fragmentVariable,
                                          bool validatePrecision);

    static bool linkValidateVaryings(InfoLog &infoLog,
                                     const std::string &varyingName,
                                     const sh::Varying &vertexVarying,
                                     const sh::Varying &fragmentVarying,
                                     int shaderVersion);
    bool linkValidateTransformFeedback(InfoLog &infoLog,
                                       const std::vector<const sh::Varying *> &linkedVaryings,
                                       const Caps &caps) const;

    void gatherTransformFeedbackVaryings(const std::vector<const sh::Varying *> &varyings);
    bool assignUniformBlockRegister(InfoLog &infoLog, UniformBlock *uniformBlock, GLenum shader, unsigned int registerIndex, const Caps &caps);
    void defineOutputVariables(Shader *fragmentShader);

    std::vector<const sh::Varying *> getMergedVaryings() const;
    void linkOutputVariables();

    bool flattenUniformsAndCheckCapsForShader(const gl::Shader &shader,
                                              GLuint maxUniformComponents,
                                              GLuint maxTextureImageUnits,
                                              const std::string &componentsErrorMessage,
                                              const std::string &samplerErrorMessage,
                                              std::vector<LinkedUniform> &samplerUniforms,
                                              InfoLog &infoLog);
    bool flattenUniformsAndCheckCaps(const Caps &caps, InfoLog &infoLog);

    struct VectorAndSamplerCount
    {
        VectorAndSamplerCount() : vectorCount(0), samplerCount(0) {}
        VectorAndSamplerCount(const VectorAndSamplerCount &other) = default;
        VectorAndSamplerCount &operator=(const VectorAndSamplerCount &other) = default;

        VectorAndSamplerCount &operator+=(const VectorAndSamplerCount &other)
        {
            vectorCount += other.vectorCount;
            samplerCount += other.samplerCount;
            return *this;
        }

        unsigned int vectorCount;
        unsigned int samplerCount;
    };

    VectorAndSamplerCount flattenUniform(const sh::ShaderVariable &uniform,
                                         const std::string &fullName,
                                         std::vector<LinkedUniform> *samplerUniforms);

    void gatherInterfaceBlockInfo();
    template <typename VarT>
    void defineUniformBlockMembers(const std::vector<VarT> &fields,
                                   const std::string &prefix,
                                   int blockIndex);

    void defineUniformBlock(const sh::InterfaceBlock &interfaceBlock, GLenum shaderType);

    template <typename T>
    void setUniformInternal(GLint location, GLsizei count, const T *v);

    template <size_t cols, size_t rows, typename T>
    void setMatrixUniformInternal(GLint location, GLsizei count, GLboolean transpose, const T *v);

    template <typename DestT>
    void getUniformInternal(GLint location, DestT *dataOut) const;

    ProgramState mState;
    rx::ProgramImpl *mProgram;

    bool mValidated;

    Bindings mAttributeBindings;
    Bindings mUniformBindings;

    // CHROMIUM_path_rendering
    Bindings mFragmentInputBindings;

    bool mLinked;
    bool mDeleteStatus;   // Flag to indicate that the program can be deleted when no longer in use

    unsigned int mRefCount;

    ResourceManager *mResourceManager;
    const GLuint mHandle;

    InfoLog mInfoLog;

    // Cache for sampler validation
    Optional<bool> mCachedValidateSamplersResult;
    std::vector<GLenum> mTextureUnitTypesCache;
    RangeUI mSamplerUniformRange;
};
}

#endif   // LIBANGLE_PROGRAM_H_
