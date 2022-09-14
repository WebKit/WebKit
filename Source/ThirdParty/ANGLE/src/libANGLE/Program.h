//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.h: Defines the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#ifndef LIBANGLE_PROGRAM_H_
#define LIBANGLE_PROGRAM_H_

#include <GLES2/gl2.h>
#include <GLSLANG/ShaderVars.h>

#include <array>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "common/Optional.h"
#include "common/angleutils.h"
#include "common/mathutil.h"
#include "common/utilities.h"

#include "libANGLE/Constants.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/InfoLog.h"
#include "libANGLE/ProgramExecutable.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/angletypes.h"

namespace rx
{
class GLImplFactory;
class ProgramImpl;
struct TranslatedAttribute;
}  // namespace rx

namespace gl
{
class Buffer;
class BinaryInputStream;
class BinaryOutputStream;
struct Caps;
class Context;
struct Extensions;
class Framebuffer;
class ProgramExecutable;
class Shader;
class ShaderProgramManager;
class State;
struct UnusedUniform;
struct Version;

extern const char *const g_fakepath;

enum class LinkMismatchError
{
    // Shared
    NO_MISMATCH,
    TYPE_MISMATCH,
    ARRAYNESS_MISMATCH,
    ARRAY_SIZE_MISMATCH,
    PRECISION_MISMATCH,
    STRUCT_NAME_MISMATCH,
    FIELD_NUMBER_MISMATCH,
    FIELD_NAME_MISMATCH,

    // Varying specific
    INTERPOLATION_TYPE_MISMATCH,
    INVARIANCE_MISMATCH,

    // Uniform specific
    BINDING_MISMATCH,
    LOCATION_MISMATCH,
    OFFSET_MISMATCH,
    INSTANCE_NAME_MISMATCH,
    FORMAT_MISMATCH,

    // Interface block specific
    LAYOUT_QUALIFIER_MISMATCH,
    MATRIX_PACKING_MISMATCH,

    // I/O block specific
    FIELD_LOCATION_MISMATCH,
    FIELD_STRUCT_NAME_MISMATCH,
};

void LogLinkMismatch(InfoLog &infoLog,
                     const std::string &variableName,
                     const char *variableType,
                     LinkMismatchError linkError,
                     const std::string &mismatchedStructOrBlockFieldName,
                     ShaderType shaderType1,
                     ShaderType shaderType2);

bool IsActiveInterfaceBlock(const sh::InterfaceBlock &interfaceBlock);

void WriteBlockMemberInfo(BinaryOutputStream *stream, const sh::BlockMemberInfo &var);
void LoadBlockMemberInfo(BinaryInputStream *stream, sh::BlockMemberInfo *var);

void WriteShaderVar(BinaryOutputStream *stream, const sh::ShaderVariable &var);
void LoadShaderVar(BinaryInputStream *stream, sh::ShaderVariable *var);

void WriteInterfaceBlock(BinaryOutputStream *stream, const InterfaceBlock &block);
void LoadInterfaceBlock(BinaryInputStream *stream, InterfaceBlock *block);

void WriteShInterfaceBlock(BinaryOutputStream *stream, const sh::InterfaceBlock &block);
void LoadShInterfaceBlock(BinaryInputStream *stream, sh::InterfaceBlock *block);

void WriteShaderVariableBuffer(BinaryOutputStream *stream, const ShaderVariableBuffer &var);
void LoadShaderVariableBuffer(BinaryInputStream *stream, ShaderVariableBuffer *var);

// Struct used for correlating uniforms/elements of uniform arrays to handles
struct VariableLocation
{
    static constexpr unsigned int kUnused = GL_INVALID_INDEX;

    VariableLocation();
    VariableLocation(unsigned int arrayIndex, unsigned int index);

    // If used is false, it means this location is only used to fill an empty space in an array,
    // and there is no corresponding uniform variable for this location. It can also mean the
    // uniform was optimized out by the implementation.
    bool used() const { return (index != kUnused); }
    void markUnused() { index = kUnused; }
    void markIgnored() { ignored = true; }

    bool operator==(const VariableLocation &other) const
    {
        return arrayIndex == other.arrayIndex && index == other.index;
    }

    // "arrayIndex" stores the index of the innermost GLSL array. It's zero for non-arrays.
    unsigned int arrayIndex;
    // "index" is an index of the variable. The variable contains the indices for other than the
    // innermost GLSL arrays.
    unsigned int index;

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

struct ProgramBinding
{
    ProgramBinding() : location(GL_INVALID_INDEX), aliased(false) {}
    ProgramBinding(GLuint index) : location(index), aliased(false) {}

    GLuint location;
    // Whether another binding was set that may potentially alias this.
    bool aliased;
};

class ProgramBindings final : angle::NonCopyable
{
  public:
    ProgramBindings();
    ~ProgramBindings();

    void bindLocation(GLuint index, const std::string &name);
    int getBindingByName(const std::string &name) const;
    int getBinding(const sh::ShaderVariable &variable) const;

    using const_iterator = angle::HashMap<std::string, GLuint>::const_iterator;
    const_iterator begin() const;
    const_iterator end() const;

    std::map<std::string, GLuint> getStableIterationMap() const;

  private:
    angle::HashMap<std::string, GLuint> mBindings;
};

// Uniforms and Fragment Outputs require special treatment due to array notation (e.g., "[0]")
class ProgramAliasedBindings final : angle::NonCopyable
{
  public:
    ProgramAliasedBindings();
    ~ProgramAliasedBindings();

    void bindLocation(GLuint index, const std::string &name);
    int getBindingByName(const std::string &name) const;
    int getBindingByLocation(GLuint location) const;
    int getBinding(const sh::ShaderVariable &variable) const;

    using const_iterator = angle::HashMap<std::string, ProgramBinding>::const_iterator;
    const_iterator begin() const;
    const_iterator end() const;

    std::map<std::string, ProgramBinding> getStableIterationMap() const;

  private:
    angle::HashMap<std::string, ProgramBinding> mBindings;
};

class ProgramState final : angle::NonCopyable
{
  public:
    ProgramState();
    ~ProgramState();

    const std::string &getLabel();

    Shader *getAttachedShader(ShaderType shaderType) const;
    const gl::ShaderMap<Shader *> &getAttachedShaders() const { return mAttachedShaders; }
    const std::vector<std::string> &getTransformFeedbackVaryingNames() const
    {
        return mTransformFeedbackVaryingNames;
    }
    GLint getTransformFeedbackBufferMode() const
    {
        return mExecutable->getTransformFeedbackBufferMode();
    }
    GLuint getUniformBlockBinding(GLuint uniformBlockIndex) const
    {
        return mExecutable->getUniformBlockBinding(uniformBlockIndex);
    }
    GLuint getShaderStorageBlockBinding(GLuint blockIndex) const
    {
        return mExecutable->getShaderStorageBlockBinding(blockIndex);
    }
    const UniformBlockBindingMask &getActiveUniformBlockBindingsMask() const
    {
        return mExecutable->getActiveUniformBlockBindings();
    }
    const std::vector<sh::ShaderVariable> &getProgramInputs() const
    {
        return mExecutable->getProgramInputs();
    }
    const std::vector<sh::ShaderVariable> &getOutputVariables() const
    {
        return mExecutable->getOutputVariables();
    }
    const std::vector<VariableLocation> &getOutputLocations() const
    {
        return mExecutable->getOutputLocations();
    }
    const std::vector<VariableLocation> &getSecondaryOutputLocations() const
    {
        return mExecutable->getSecondaryOutputLocations();
    }
    const std::vector<LinkedUniform> &getUniforms() const { return mExecutable->getUniforms(); }
    const std::vector<VariableLocation> &getUniformLocations() const { return mUniformLocations; }
    const std::vector<InterfaceBlock> &getUniformBlocks() const
    {
        return mExecutable->getUniformBlocks();
    }
    const std::vector<InterfaceBlock> &getShaderStorageBlocks() const
    {
        return mExecutable->getShaderStorageBlocks();
    }
    const std::vector<BufferVariable> &getBufferVariables() const { return mBufferVariables; }
    const std::vector<SamplerBinding> &getSamplerBindings() const
    {
        return mExecutable->getSamplerBindings();
    }
    const std::vector<ImageBinding> &getImageBindings() const
    {
        return getExecutable().getImageBindings();
    }
    const sh::WorkGroupSize &getComputeShaderLocalSize() const { return mComputeShaderLocalSize; }
    const RangeUI &getDefaultUniformRange() const { return mExecutable->getDefaultUniformRange(); }
    const RangeUI &getSamplerUniformRange() const { return mExecutable->getSamplerUniformRange(); }
    const RangeUI &getImageUniformRange() const { return mExecutable->getImageUniformRange(); }
    const RangeUI &getAtomicCounterUniformRange() const
    {
        return mExecutable->getAtomicCounterUniformRange();
    }
    const RangeUI &getFragmentInoutRange() const { return mExecutable->getFragmentInoutRange(); }

    const std::vector<TransformFeedbackVarying> &getLinkedTransformFeedbackVaryings() const
    {
        return mExecutable->getLinkedTransformFeedbackVaryings();
    }
    const std::vector<GLsizei> &getTransformFeedbackStrides() const
    {
        return mExecutable->getTransformFeedbackStrides();
    }
    const std::vector<AtomicCounterBuffer> &getAtomicCounterBuffers() const
    {
        return mExecutable->getAtomicCounterBuffers();
    }

    GLuint getUniformIndexFromName(const std::string &name) const;
    GLuint getUniformIndexFromLocation(UniformLocation location) const;
    Optional<GLuint> getSamplerIndex(UniformLocation location) const;
    bool isSamplerUniformIndex(GLuint index) const;
    GLuint getSamplerIndexFromUniformIndex(GLuint uniformIndex) const;
    GLuint getUniformIndexFromSamplerIndex(GLuint samplerIndex) const;
    bool isImageUniformIndex(GLuint index) const;
    GLuint getImageIndexFromUniformIndex(GLuint uniformIndex) const;
    GLuint getAttributeLocation(const std::string &name) const;

    GLuint getBufferVariableIndexFromName(const std::string &name) const;

    int getNumViews() const { return mNumViews; }
    bool usesMultiview() const { return mNumViews != -1; }

    bool hasAttachedShader() const;

    ShaderType getFirstAttachedShaderStageType() const;
    ShaderType getLastAttachedShaderStageType() const;

    const ProgramAliasedBindings &getUniformLocationBindings() const
    {
        return mUniformLocationBindings;
    }

    const ProgramExecutable &getExecutable() const
    {
        ASSERT(mExecutable);
        return *mExecutable;
    }
    ProgramExecutable &getExecutable()
    {
        ASSERT(mExecutable);
        return *mExecutable;
    }

    bool hasImages() const { return !getImageBindings().empty(); }
    rx::SpecConstUsageBits getSpecConstUsageBits() const { return mSpecConstUsageBits; }

    // A Program can only either be graphics or compute, but never both, so it
    // can answer isCompute() based on which shaders it has.
    bool isCompute() const { return mExecutable->hasLinkedShaderStage(ShaderType::Compute); }

    const std::string &getLabel() const { return mLabel; }

    uint32_t getLocationsUsedForXfbExtension() const { return mLocationsUsedForXfbExtension; }

    bool hasBinaryRetrieveableHint() const { return mBinaryRetrieveableHint; }

    bool isSeparable() const { return mSeparable; }

    int getDrawIDLocation() const { return mDrawIDLocation; }

    int getBaseVertexLocation() const { return mBaseVertexLocation; }

    int getBaseInstanceLocation() const { return mBaseInstanceLocation; }

    ShaderType getAttachedTransformFeedbackStage() const;

  private:
    friend class MemoryProgramCache;
    friend class Program;

    void updateActiveSamplers();
    void updateProgramInterfaceInputs(const Context *context);
    void updateProgramInterfaceOutputs(const Context *context);

    // Scans the sampler bindings for type conflicts with sampler 'textureUnitIndex'.
    void setSamplerUniformTextureTypeAndFormat(size_t textureUnitIndex);

    std::string mLabel;

    sh::WorkGroupSize mComputeShaderLocalSize;

    ShaderMap<Shader *> mAttachedShaders;

    uint32_t mLocationsUsedForXfbExtension;
    std::vector<std::string> mTransformFeedbackVaryingNames;

    std::vector<VariableLocation> mUniformLocations;
    std::vector<BufferVariable> mBufferVariables;

    bool mBinaryRetrieveableHint;
    bool mSeparable;
    rx::SpecConstUsageBits mSpecConstUsageBits;

    // ANGLE_multiview.
    int mNumViews;

    // GL_ANGLE_multi_draw
    int mDrawIDLocation;

    // GL_ANGLE_base_vertex_base_instance_shader_builtin
    int mBaseVertexLocation;
    int mBaseInstanceLocation;
    // Cached value of base vertex and base instance
    // need to reset them to zero if using non base vertex or base instance draw calls.
    GLint mCachedBaseVertex;
    GLuint mCachedBaseInstance;

    // Note that this has nothing to do with binding layout qualifiers that can be set for some
    // uniforms in GLES3.1+. It is used to pre-set the location of uniforms.
    ProgramAliasedBindings mUniformLocationBindings;

    std::shared_ptr<ProgramExecutable> mExecutable;
};

struct ProgramVaryingRef
{
    const sh::ShaderVariable *get(ShaderType stage) const
    {
        ASSERT(stage == frontShaderStage || stage == backShaderStage);
        const sh::ShaderVariable *ref = stage == frontShaderStage ? frontShader : backShader;
        ASSERT(ref);
        return ref;
    }

    const sh::ShaderVariable *frontShader = nullptr;
    const sh::ShaderVariable *backShader  = nullptr;
    ShaderType frontShaderStage           = ShaderType::InvalidEnum;
    ShaderType backShaderStage            = ShaderType::InvalidEnum;
};

using ProgramMergedVaryings = std::vector<ProgramVaryingRef>;

class Program final : public LabeledObject, public angle::Subject
{
  public:
    Program(rx::GLImplFactory *factory, ShaderProgramManager *manager, ShaderProgramID handle);
    void onDestroy(const Context *context);

    ShaderProgramID id() const;

    angle::Result setLabel(const Context *context, const std::string &label) override;
    const std::string &getLabel() const override;

    ANGLE_INLINE rx::ProgramImpl *getImplementation() const
    {
        ASSERT(!mLinkingState);
        return mProgram;
    }

    void attachShader(Shader *shader);
    void detachShader(const Context *context, Shader *shader);
    int getAttachedShadersCount() const;

    Shader *getAttachedShader(ShaderType shaderType) const;

    void bindAttributeLocation(GLuint index, const char *name);
    void bindUniformLocation(UniformLocation location, const char *name);

    // EXT_blend_func_extended
    void bindFragmentOutputLocation(GLuint index, const char *name);
    void bindFragmentOutputIndex(GLuint index, const char *name);

    // KHR_parallel_shader_compile
    // Try to link the program asynchronously. As a result, background threads may be launched to
    // execute the linking tasks concurrently.
    angle::Result link(const Context *context);

    // Peek whether there is any running linking tasks.
    bool isLinking() const;
    bool hasLinkingState() const { return mLinkingState != nullptr; }

    bool isLinked() const
    {
        ASSERT(!mLinkingState);
        return mLinked;
    }

    angle::Result loadBinary(const Context *context,
                             GLenum binaryFormat,
                             const void *binary,
                             GLsizei length);
    angle::Result saveBinary(Context *context,
                             GLenum *binaryFormat,
                             void *binary,
                             GLsizei bufSize,
                             GLsizei *length) const;
    GLint getBinaryLength(Context *context) const;
    void setBinaryRetrievableHint(bool retrievable);
    bool getBinaryRetrievableHint() const;

    void setSeparable(bool separable);
    bool isSeparable() const;

    void getAttachedShaders(GLsizei maxCount, GLsizei *count, ShaderProgramID *shaders) const;

    GLuint getAttributeLocation(const std::string &name) const;

    void getActiveAttribute(GLuint index,
                            GLsizei bufsize,
                            GLsizei *length,
                            GLint *size,
                            GLenum *type,
                            GLchar *name) const;
    GLint getActiveAttributeCount() const;
    GLint getActiveAttributeMaxLength() const;
    const std::vector<sh::ShaderVariable> &getAttributes() const;

    GLint getFragDataLocation(const std::string &name) const;
    size_t getOutputResourceCount() const;

    // EXT_blend_func_extended
    GLint getFragDataIndex(const std::string &name) const;

    void getActiveUniform(GLuint index,
                          GLsizei bufsize,
                          GLsizei *length,
                          GLint *size,
                          GLenum *type,
                          GLchar *name) const;
    GLint getActiveUniformCount() const;
    size_t getActiveBufferVariableCount() const;
    GLint getActiveUniformMaxLength() const;
    bool isValidUniformLocation(UniformLocation location) const;
    const LinkedUniform &getUniformByLocation(UniformLocation location) const;
    const VariableLocation &getUniformLocation(UniformLocation location) const;

    const std::vector<VariableLocation> &getUniformLocations() const
    {
        ASSERT(!mLinkingState);
        return mState.mUniformLocations;
    }

    const LinkedUniform &getUniformByIndex(GLuint index) const
    {
        ASSERT(!mLinkingState);
        return mState.mExecutable->getUniformByIndex(index);
    }

    const BufferVariable &getBufferVariableByIndex(GLuint index) const;

    enum SetUniformResult
    {
        SamplerChanged,
        NoSamplerChange,
    };

    UniformLocation getUniformLocation(const std::string &name) const;
    GLuint getUniformIndex(const std::string &name) const;
    void setUniform1fv(UniformLocation location, GLsizei count, const GLfloat *v);
    void setUniform2fv(UniformLocation location, GLsizei count, const GLfloat *v);
    void setUniform3fv(UniformLocation location, GLsizei count, const GLfloat *v);
    void setUniform4fv(UniformLocation location, GLsizei count, const GLfloat *v);
    void setUniform1iv(Context *context, UniformLocation location, GLsizei count, const GLint *v);
    void setUniform2iv(UniformLocation location, GLsizei count, const GLint *v);
    void setUniform3iv(UniformLocation location, GLsizei count, const GLint *v);
    void setUniform4iv(UniformLocation location, GLsizei count, const GLint *v);
    void setUniform1uiv(UniformLocation location, GLsizei count, const GLuint *v);
    void setUniform2uiv(UniformLocation location, GLsizei count, const GLuint *v);
    void setUniform3uiv(UniformLocation location, GLsizei count, const GLuint *v);
    void setUniform4uiv(UniformLocation location, GLsizei count, const GLuint *v);
    void setUniformMatrix2fv(UniformLocation location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value);
    void setUniformMatrix3fv(UniformLocation location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value);
    void setUniformMatrix4fv(UniformLocation location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value);
    void setUniformMatrix2x3fv(UniformLocation location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);
    void setUniformMatrix3x2fv(UniformLocation location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);
    void setUniformMatrix2x4fv(UniformLocation location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);
    void setUniformMatrix4x2fv(UniformLocation location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);
    void setUniformMatrix3x4fv(UniformLocation location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);
    void setUniformMatrix4x3fv(UniformLocation location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value);

    void getUniformfv(const Context *context, UniformLocation location, GLfloat *params) const;
    void getUniformiv(const Context *context, UniformLocation location, GLint *params) const;
    void getUniformuiv(const Context *context, UniformLocation location, GLuint *params) const;

    void getActiveUniformBlockName(const Context *context,
                                   const UniformBlockIndex blockIndex,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLchar *blockName) const;
    void getActiveShaderStorageBlockName(const GLuint blockIndex,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLchar *blockName) const;

    ANGLE_INLINE GLuint getActiveUniformBlockCount() const
    {
        ASSERT(!mLinkingState);
        return static_cast<GLuint>(mState.mExecutable->getActiveUniformBlockCount());
    }

    ANGLE_INLINE GLuint getActiveAtomicCounterBufferCount() const
    {
        ASSERT(!mLinkingState);
        return static_cast<GLuint>(mState.mExecutable->getActiveAtomicCounterBufferCount());
    }

    ANGLE_INLINE GLuint getActiveShaderStorageBlockCount() const
    {
        ASSERT(!mLinkingState);
        return static_cast<GLuint>(mState.mExecutable->getActiveShaderStorageBlockCount());
    }

    GLint getActiveUniformBlockMaxNameLength() const;
    GLint getActiveShaderStorageBlockMaxNameLength() const;

    const std::vector<LinkedUniform> &getUniforms() const { return mState.getUniforms(); }
    GLuint getUniformBlockIndex(const std::string &name) const;
    GLuint getShaderStorageBlockIndex(const std::string &name) const;

    void bindUniformBlock(UniformBlockIndex uniformBlockIndex, GLuint uniformBlockBinding);
    GLuint getUniformBlockBinding(GLuint uniformBlockIndex) const;
    GLuint getShaderStorageBlockBinding(GLuint shaderStorageBlockIndex) const;

    const InterfaceBlock &getUniformBlockByIndex(GLuint index) const;
    const InterfaceBlock &getShaderStorageBlockByIndex(GLuint index) const;

    void setTransformFeedbackVaryings(GLsizei count,
                                      const GLchar *const *varyings,
                                      GLenum bufferMode);
    void getTransformFeedbackVarying(GLuint index,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLsizei *size,
                                     GLenum *type,
                                     GLchar *name) const;
    GLsizei getTransformFeedbackVaryingCount() const;
    GLsizei getTransformFeedbackVaryingMaxLength() const;
    GLenum getTransformFeedbackBufferMode() const;
    GLuint getTransformFeedbackVaryingResourceIndex(const GLchar *name) const;
    const TransformFeedbackVarying &getTransformFeedbackVaryingResource(GLuint index) const;

    bool hasDrawIDUniform() const;
    void setDrawIDUniform(GLint drawid);

    bool hasBaseVertexUniform() const;
    void setBaseVertexUniform(GLint baseVertex);
    bool hasBaseInstanceUniform() const;
    void setBaseInstanceUniform(GLuint baseInstance);

    ANGLE_INLINE void addRef()
    {
        ASSERT(!mLinkingState);
        mRefCount++;
    }

    ANGLE_INLINE void release(const Context *context)
    {
        ASSERT(!mLinkingState);
        mRefCount--;

        if (mRefCount == 0 && mDeleteStatus)
        {
            deleteSelf(context);
        }
    }

    unsigned int getRefCount() const;
    bool isInUse() const { return getRefCount() != 0; }
    void flagForDeletion();
    bool isFlaggedForDeletion() const;

    void validate(const Caps &caps);
    bool isValidated() const;

    const std::vector<ImageBinding> &getImageBindings() const
    {
        ASSERT(!mLinkingState);
        return getExecutable().getImageBindings();
    }
    const sh::WorkGroupSize &getComputeShaderLocalSize() const;
    PrimitiveMode getGeometryShaderInputPrimitiveType() const;
    PrimitiveMode getGeometryShaderOutputPrimitiveType() const;
    GLint getGeometryShaderInvocations() const;
    GLint getGeometryShaderMaxVertices() const;

    GLint getTessControlShaderVertices() const;
    GLenum getTessGenMode() const;
    GLenum getTessGenPointMode() const;
    GLenum getTessGenSpacing() const;
    GLenum getTessGenVertexOrder() const;

    const ProgramState &getState() const
    {
        ASSERT(!mLinkingState);
        return mState;
    }

    GLuint getInputResourceIndex(const GLchar *name) const;
    GLuint getOutputResourceIndex(const GLchar *name) const;
    void getInputResourceName(GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) const;
    void getOutputResourceName(GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) const;
    void getUniformResourceName(GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) const;
    void getBufferVariableResourceName(GLuint index,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLchar *name) const;
    const sh::ShaderVariable &getInputResource(size_t index) const;
    GLuint getResourceMaxNameSize(const sh::ShaderVariable &resource, GLint max) const;
    GLuint getInputResourceMaxNameSize() const;
    GLuint getOutputResourceMaxNameSize() const;
    GLuint getResourceLocation(const GLchar *name, const sh::ShaderVariable &variable) const;
    GLuint getInputResourceLocation(const GLchar *name) const;
    GLuint getOutputResourceLocation(const GLchar *name) const;
    const std::string getResourceName(const sh::ShaderVariable &resource) const;
    const std::string getInputResourceName(GLuint index) const;
    const std::string getOutputResourceName(GLuint index) const;
    const sh::ShaderVariable &getOutputResource(size_t index) const;

    const ProgramBindings &getAttributeBindings() const;
    const ProgramAliasedBindings &getUniformLocationBindings() const;
    const ProgramAliasedBindings &getFragmentOutputLocations() const;
    const ProgramAliasedBindings &getFragmentOutputIndexes() const;

    int getNumViews() const
    {
        ASSERT(!mLinkingState);
        return mState.getNumViews();
    }

    bool usesMultiview() const { return mState.usesMultiview(); }

    const std::vector<GLsizei> &getTransformFeedbackStrides() const;

    // Program dirty bits.
    enum DirtyBitType
    {
        DIRTY_BIT_UNIFORM_BLOCK_BINDING_0,
        DIRTY_BIT_UNIFORM_BLOCK_BINDING_MAX =
            DIRTY_BIT_UNIFORM_BLOCK_BINDING_0 + IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS,

        DIRTY_BIT_COUNT = DIRTY_BIT_UNIFORM_BLOCK_BINDING_MAX,
    };

    using DirtyBits = angle::BitSet<DIRTY_BIT_COUNT>;

    angle::Result syncState(const Context *context);

    // Try to resolve linking. Inlined to make sure its overhead is as low as possible.
    void resolveLink(const Context *context)
    {
        if (mLinkingState)
        {
            resolveLinkImpl(context);
        }
    }

    ANGLE_INLINE bool hasAnyDirtyBit() const { return mDirtyBits.any(); }

    // Writes a program's binary to the output memory buffer.
    angle::Result serialize(const Context *context, angle::MemoryBuffer *binaryOut) const;

    rx::Serial serial() const { return mSerial; }

    const ProgramExecutable &getExecutable() const { return mState.getExecutable(); }
    ProgramExecutable &getExecutable() { return mState.getExecutable(); }

  private:
    struct LinkingState;

    ~Program() override;

    // Loads program state according to the specified binary blob.
    angle::Result deserialize(const Context *context, BinaryInputStream &stream, InfoLog &infoLog);

    void unlink();
    void deleteSelf(const Context *context);

    angle::Result linkImpl(const Context *context);

    bool linkValidateShaders(const Context *context, InfoLog &infoLog);
    bool linkAttributes(const Context *context, InfoLog &infoLog);
    bool linkVaryings(const Context *context, InfoLog &infoLog) const;

    bool linkUniforms(const Context *context,
                      std::vector<UnusedUniform> *unusedUniformsOutOrNull,
                      GLuint *combinedImageUniformsOut,
                      InfoLog &infoLog);

    void updateLinkedShaderStages();

    void setUniformValuesFromBindingQualifiers();
    bool shouldIgnoreUniform(UniformLocation location) const;

    void initInterfaceBlockBindings();

    // Both these function update the cached uniform values and return a modified "count"
    // so that the uniform update doesn't overflow the uniform.
    template <typename T>
    GLsizei clampUniformCount(const VariableLocation &locationInfo,
                              GLsizei count,
                              int vectorSize,
                              const T *v);
    template <size_t cols, size_t rows, typename T>
    GLsizei clampMatrixUniformCount(UniformLocation location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const T *v);

    void updateSamplerUniform(Context *context,
                              const VariableLocation &locationInfo,
                              GLsizei clampedCount,
                              const GLint *v);

    template <typename DestT>
    void getUniformInternal(const Context *context,
                            DestT *dataOut,
                            UniformLocation location,
                            GLenum nativeType,
                            int components) const;

    void getResourceName(const std::string name,
                         GLsizei bufSize,
                         GLsizei *length,
                         GLchar *dest) const;

    template <typename T>
    GLint getActiveInterfaceBlockMaxNameLength(const std::vector<T> &resources) const;

    GLuint getSamplerUniformBinding(const VariableLocation &uniformLocation) const;
    GLuint getImageUniformBinding(const VariableLocation &uniformLocation) const;

    // Block until linking is finished and resolve it.
    void resolveLinkImpl(const gl::Context *context);

    void postResolveLink(const gl::Context *context);

    template <typename UniformT,
              GLint UniformSize,
              void (rx::ProgramImpl::*SetUniformFunc)(GLint, GLsizei, const UniformT *)>
    void setUniformGeneric(UniformLocation location, GLsizei count, const UniformT *v);

    template <
        typename UniformT,
        GLint MatrixC,
        GLint MatrixR,
        void (rx::ProgramImpl::*SetUniformMatrixFunc)(GLint, GLsizei, GLboolean, const UniformT *)>
    void setUniformMatrixGeneric(UniformLocation location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const UniformT *v);

    rx::Serial mSerial;
    ProgramState mState;
    rx::ProgramImpl *mProgram;

    bool mValidated;

    ProgramBindings mAttributeBindings;

    // EXT_blend_func_extended
    ProgramAliasedBindings mFragmentOutputLocations;
    ProgramAliasedBindings mFragmentOutputIndexes;

    bool mLinked;
    std::unique_ptr<LinkingState> mLinkingState;
    bool mDeleteStatus;  // Flag to indicate that the program can be deleted when no longer in use

    unsigned int mRefCount;

    ShaderProgramManager *mResourceManager;
    const ShaderProgramID mHandle;

    DirtyBits mDirtyBits;

    std::mutex mHistogramMutex;
};
}  // namespace gl

#endif  // LIBANGLE_PROGRAM_H_
