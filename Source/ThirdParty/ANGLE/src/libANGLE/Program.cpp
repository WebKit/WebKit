//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.cpp: Implements the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#include "libANGLE/Program.h"

#include <algorithm>
#include <utility>

#include "common/angle_version_info.h"
#include "common/bitset_utils.h"
#include "common/debug.h"
#include "common/platform.h"
#include "common/platform_helpers.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "compiler/translator/blocklayout.h"
#include "libANGLE/Context.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/MemoryProgramCache.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/VaryingPacking.h"
#include "libANGLE/Version.h"
#include "libANGLE/capture/FrameCapture.h"
#include "libANGLE/features.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "platform/PlatformMethods.h"
#include "platform/autogen/FrontendFeatures_autogen.h"

namespace gl
{

namespace
{

// This simplified cast function doesn't need to worry about advanced concepts like
// depth range values, or casting to bool.
template <typename DestT, typename SrcT>
DestT UniformStateQueryCast(SrcT value);

// From-Float-To-Integer Casts
template <>
GLint UniformStateQueryCast(GLfloat value)
{
    return clampCast<GLint>(roundf(value));
}

template <>
GLuint UniformStateQueryCast(GLfloat value)
{
    return clampCast<GLuint>(roundf(value));
}

// From-Integer-to-Integer Casts
template <>
GLint UniformStateQueryCast(GLuint value)
{
    return clampCast<GLint>(value);
}

template <>
GLuint UniformStateQueryCast(GLint value)
{
    return clampCast<GLuint>(value);
}

// From-Boolean-to-Anything Casts
template <>
GLfloat UniformStateQueryCast(GLboolean value)
{
    return (ConvertToBool(value) ? 1.0f : 0.0f);
}

template <>
GLint UniformStateQueryCast(GLboolean value)
{
    return (ConvertToBool(value) ? 1 : 0);
}

template <>
GLuint UniformStateQueryCast(GLboolean value)
{
    return (ConvertToBool(value) ? 1u : 0u);
}

// Default to static_cast
template <typename DestT, typename SrcT>
DestT UniformStateQueryCast(SrcT value)
{
    return static_cast<DestT>(value);
}

template <typename SrcT, typename DestT>
void UniformStateQueryCastLoop(DestT *dataOut, const uint8_t *srcPointer, int components)
{
    for (int comp = 0; comp < components; ++comp)
    {
        // We only work with strides of 4 bytes for uniform components. (GLfloat/GLint)
        // Don't use SrcT stride directly since GLboolean has a stride of 1 byte.
        size_t offset               = comp * 4;
        const SrcT *typedSrcPointer = reinterpret_cast<const SrcT *>(&srcPointer[offset]);
        dataOut[comp]               = UniformStateQueryCast<DestT>(*typedSrcPointer);
    }
}

template <typename VarT>
GLuint GetResourceIndexFromName(const std::vector<VarT> &list, const std::string &name)
{
    std::string nameAsArrayName = name + "[0]";
    for (size_t index = 0; index < list.size(); index++)
    {
        const VarT &resource = list[index];
        if (resource.name == name || (resource.isArray() && resource.name == nameAsArrayName))
        {
            return static_cast<GLuint>(index);
        }
    }

    return GL_INVALID_INDEX;
}

GLuint GetUniformIndexFromName(const std::vector<LinkedUniform> &uniformList,
                               const std::vector<std::string> &nameList,
                               const std::string &name)
{
    std::string nameAsArrayName = name + "[0]";
    for (size_t index = 0; index < nameList.size(); index++)
    {
        const std::string &uniformName = nameList[index];
        if (uniformName == name || (uniformList[index].isArray() && uniformName == nameAsArrayName))
        {
            return static_cast<GLuint>(index);
        }
    }

    return GL_INVALID_INDEX;
}

GLint GetVariableLocation(const std::vector<sh::ShaderVariable> &list,
                          const std::vector<VariableLocation> &locationList,
                          const std::string &name)
{
    size_t nameLengthWithoutArrayIndex;
    unsigned int arrayIndex = ParseArrayIndex(name, &nameLengthWithoutArrayIndex);

    for (size_t location = 0u; location < locationList.size(); ++location)
    {
        const VariableLocation &variableLocation = locationList[location];
        if (!variableLocation.used())
        {
            continue;
        }

        const sh::ShaderVariable &variable = list[variableLocation.index];

        // Array output variables may be bound out of order, so we need to ensure we only pick the
        // first element if given the base name.
        if ((variable.name == name) && (variableLocation.arrayIndex == 0))
        {
            return static_cast<GLint>(location);
        }
        if (variable.isArray() && variableLocation.arrayIndex == arrayIndex &&
            angle::BeginsWith(variable.name, name, nameLengthWithoutArrayIndex))
        {
            return static_cast<GLint>(location);
        }
    }

    return -1;
}

GLint GetUniformLocation(const std::vector<LinkedUniform> &uniformList,
                         const std::vector<std::string> &nameList,
                         const std::vector<VariableLocation> &locationList,
                         const std::string &name)
{
    size_t nameLengthWithoutArrayIndex;
    unsigned int arrayIndex = ParseArrayIndex(name, &nameLengthWithoutArrayIndex);

    for (size_t location = 0u; location < locationList.size(); ++location)
    {
        const VariableLocation &variableLocation = locationList[location];
        if (!variableLocation.used())
        {
            continue;
        }

        const LinkedUniform &variable  = uniformList[variableLocation.index];
        const std::string &uniformName = nameList[variableLocation.index];

        // Array output variables may be bound out of order, so we need to ensure we only pick the
        // first element if given the base name. Uniforms don't allow this behavior and some code
        // seemingly depends on the opposite behavior, so only enable it for output variables.
        if (angle::BeginsWith(uniformName, name) && (variableLocation.arrayIndex == 0))
        {
            if (name.length() == uniformName.length())
            {
                ASSERT(name == uniformName);
                // GLES 3.1 November 2016 page 87.
                // The string exactly matches the name of the active variable.
                return static_cast<GLint>(location);
            }
            if (name.length() + 3u == uniformName.length() && variable.isArray())
            {
                ASSERT(name + "[0]" == uniformName);
                // The string identifies the base name of an active array, where the string would
                // exactly match the name of the variable if the suffix "[0]" were appended to the
                // string.
                return static_cast<GLint>(location);
            }
        }
        if (variable.isArray() && variableLocation.arrayIndex == arrayIndex &&
            nameLengthWithoutArrayIndex + 3u == uniformName.length() &&
            angle::BeginsWith(uniformName, name, nameLengthWithoutArrayIndex))
        {
            ASSERT(name.substr(0u, nameLengthWithoutArrayIndex) + "[0]" == uniformName);
            // The string identifies an active element of the array, where the string ends with the
            // concatenation of the "[" character, an integer (with no "+" sign, extra leading
            // zeroes, or whitespace) identifying an array element, and the "]" character, the
            // integer is less than the number of active elements of the array variable, and where
            // the string would exactly match the enumerated name of the array if the decimal
            // integer were replaced with zero.
            return static_cast<GLint>(location);
        }
    }

    return -1;
}

void CopyStringToBuffer(GLchar *buffer,
                        const std::string &string,
                        GLsizei bufSize,
                        GLsizei *lengthOut)
{
    ASSERT(bufSize > 0);
    size_t length = std::min<size_t>(bufSize - 1, string.length());
    memcpy(buffer, string.c_str(), length);
    buffer[length] = '\0';

    if (lengthOut)
    {
        *lengthOut = static_cast<GLsizei>(length);
    }
}

GLuint GetInterfaceBlockIndex(const std::vector<InterfaceBlock> &list, const std::string &name)
{
    std::vector<unsigned int> subscripts;
    std::string baseName = ParseResourceName(name, &subscripts);

    unsigned int numBlocks = static_cast<unsigned int>(list.size());
    for (unsigned int blockIndex = 0; blockIndex < numBlocks; blockIndex++)
    {
        const auto &block = list[blockIndex];
        if (block.name == baseName)
        {
            const bool arrayElementZero =
                (subscripts.empty() && (!block.isArray || block.arrayElement == 0));
            const bool arrayElementMatches =
                (subscripts.size() == 1 && subscripts[0] == block.arrayElement);
            if (arrayElementMatches || arrayElementZero)
            {
                return blockIndex;
            }
        }
    }

    return GL_INVALID_INDEX;
}

void GetInterfaceBlockName(const UniformBlockIndex index,
                           const std::vector<InterfaceBlock> &list,
                           GLsizei bufSize,
                           GLsizei *length,
                           GLchar *name)
{
    ASSERT(index.value < list.size());

    const auto &block = list[index.value];

    if (bufSize > 0)
    {
        std::string blockName = block.name;

        if (block.isArray)
        {
            blockName += ArrayString(block.arrayElement);
        }
        CopyStringToBuffer(name, blockName, bufSize, length);
    }
}

void InitUniformBlockLinker(const ProgramState &state, UniformBlockLinker *blockLinker)
{
    for (ShaderType shaderType : AllShaderTypes())
    {
        const SharedCompiledShaderState &shader = state.getAttachedShader(shaderType);
        if (shader)
        {
            blockLinker->addShaderBlocks(shaderType, &shader->uniformBlocks);
        }
    }
}

void InitShaderStorageBlockLinker(const ProgramState &state, ShaderStorageBlockLinker *blockLinker)
{
    for (ShaderType shaderType : AllShaderTypes())
    {
        const SharedCompiledShaderState &shader = state.getAttachedShader(shaderType);
        if (shader)
        {
            blockLinker->addShaderBlocks(shaderType, &shader->shaderStorageBlocks);
        }
    }
}

template <typename T>
GLuint GetResourceMaxNameSize(const T &resource, GLint max)
{
    if (resource.isArray())
    {
        return std::max(max, clampCast<GLint>((resource.name + "[0]").size()));
    }
    else
    {
        return std::max(max, clampCast<GLint>((resource.name).size()));
    }
}

template <typename T>
GLuint GetResourceLocation(const GLchar *name, const T &variable, GLint location)
{
    if (variable.isBuiltIn())
    {
        return GL_INVALID_INDEX;
    }

    if (variable.isArray())
    {
        size_t nameLengthWithoutArrayIndexOut;
        size_t arrayIndex = ParseArrayIndex(name, &nameLengthWithoutArrayIndexOut);
        // The 'name' string may not contain the array notation "[0]"
        if (arrayIndex != GL_INVALID_INDEX)
        {
            location += arrayIndex;
        }
    }

    return location;
}

template <typename T>
const std::string GetResourceName(const T &resource)
{
    std::string resourceName = resource.name;

    if (resource.isArray())
    {
        resourceName += "[0]";
    }

    return resourceName;
}
}  // anonymous namespace

const char *GetLinkMismatchErrorString(LinkMismatchError linkError)
{
    switch (linkError)
    {
        case LinkMismatchError::TYPE_MISMATCH:
            return "Type";
        case LinkMismatchError::ARRAYNESS_MISMATCH:
            return "Array-ness";
        case LinkMismatchError::ARRAY_SIZE_MISMATCH:
            return "Array size";
        case LinkMismatchError::PRECISION_MISMATCH:
            return "Precision";
        case LinkMismatchError::STRUCT_NAME_MISMATCH:
            return "Structure name";
        case LinkMismatchError::FIELD_NUMBER_MISMATCH:
            return "Field number";
        case LinkMismatchError::FIELD_NAME_MISMATCH:
            return "Field name";

        case LinkMismatchError::INTERPOLATION_TYPE_MISMATCH:
            return "Interpolation type";
        case LinkMismatchError::INVARIANCE_MISMATCH:
            return "Invariance";

        case LinkMismatchError::BINDING_MISMATCH:
            return "Binding layout qualifier";
        case LinkMismatchError::LOCATION_MISMATCH:
            return "Location layout qualifier";
        case LinkMismatchError::OFFSET_MISMATCH:
            return "Offset layout qualifier";
        case LinkMismatchError::INSTANCE_NAME_MISMATCH:
            return "Instance name qualifier";
        case LinkMismatchError::FORMAT_MISMATCH:
            return "Format qualifier";

        case LinkMismatchError::LAYOUT_QUALIFIER_MISMATCH:
            return "Layout qualifier";
        case LinkMismatchError::MATRIX_PACKING_MISMATCH:
            return "Matrix Packing";

        case LinkMismatchError::FIELD_LOCATION_MISMATCH:
            return "Field location";
        case LinkMismatchError::FIELD_STRUCT_NAME_MISMATCH:
            return "Field structure name";
        default:
            UNREACHABLE();
            return "";
    }
}

template <typename T>
void UpdateInterfaceVariable(std::vector<T> *block, const sh::ShaderVariable &var)
{
    if (!var.isStruct())
    {
        block->emplace_back(var);
        block->back().resetEffectiveLocation();
    }

    for (const sh::ShaderVariable &field : var.fields)
    {
        ASSERT(!var.name.empty() || var.isShaderIOBlock);

        // Shader I/O block naming is similar to UBOs and SSBOs:
        //
        //     in Block
        //     {
        //         type field;  // produces "field"
        //     };
        //
        //     in Block2
        //     {
        //         type field;  // produces "Block2.field"
        //     } block2;
        //
        const std::string &baseName = var.isShaderIOBlock ? var.structOrBlockName : var.name;
        const std::string prefix    = var.name.empty() ? "" : baseName + ".";

        if (!field.isStruct())
        {
            sh::ShaderVariable fieldCopy = field;
            fieldCopy.updateEffectiveLocation(var);
            fieldCopy.name = prefix + field.name;
            block->emplace_back(fieldCopy);
        }

        for (const sh::ShaderVariable &nested : field.fields)
        {
            sh::ShaderVariable nestedCopy = nested;
            nestedCopy.updateEffectiveLocation(field);
            nestedCopy.name = prefix + field.name + "." + nested.name;
            block->emplace_back(nestedCopy);
        }
    }
}

void WriteActiveVariable(BinaryOutputStream *stream, const ActiveVariable &var)
{
    for (ShaderType shaderType : AllShaderTypes())
    {
        stream->writeBool(var.isActive(shaderType));
        stream->writeInt(var.isActive(shaderType) ? var.getIds()[shaderType] : 0);
    }
}

void LoadActiveVariable(BinaryInputStream *stream, ActiveVariable *var)
{
    for (ShaderType shaderType : AllShaderTypes())
    {
        const bool isActive = stream->readBool();
        const uint32_t id   = stream->readInt<uint32_t>();
        var->setActive(shaderType, isActive, id);
    }
}

void WriteBufferVariable(BinaryOutputStream *stream, const BufferVariable &var)
{
    WriteShaderVar(stream, var);
    WriteActiveVariable(stream, var.activeVariable);

    stream->writeInt(var.bufferIndex);
    WriteBlockMemberInfo(stream, var.blockInfo);
    stream->writeInt(var.topLevelArraySize);
}

void LoadBufferVariable(BinaryInputStream *stream, BufferVariable *var)
{
    LoadShaderVar(stream, var);
    LoadActiveVariable(stream, &(var->activeVariable));

    var->bufferIndex = stream->readInt<int>();
    LoadBlockMemberInfo(stream, &var->blockInfo);
    var->topLevelArraySize = stream->readInt<int>();
}

// Saves the linking context for later use in resolveLink().
struct Program::LinkingState
{
    LinkingVariables linkingVariables;
    ProgramLinkedResources resources;
    egl::BlobCache::Key programHash;
    std::unique_ptr<rx::LinkEvent> linkEvent;
    bool linkingFromBinary;
};

const char *const g_fakepath = "C:\\fakepath";

// InfoLog implementation.
InfoLog::InfoLog() : mLazyStream(nullptr) {}

InfoLog::~InfoLog() {}

size_t InfoLog::getLength() const
{
    if (!mLazyStream)
    {
        return 0;
    }

    const std::string &logString = mLazyStream->str();
    return logString.empty() ? 0 : logString.length() + 1;
}

void InfoLog::getLog(GLsizei bufSize, GLsizei *length, char *infoLog) const
{
    size_t index = 0;

    if (bufSize > 0)
    {
        const std::string logString(str());

        if (!logString.empty())
        {
            index = std::min(static_cast<size_t>(bufSize) - 1, logString.length());
            memcpy(infoLog, logString.c_str(), index);
        }

        infoLog[index] = '\0';
    }

    if (length)
    {
        *length = static_cast<GLsizei>(index);
    }
}

// append a sanitized message to the program info log.
// The D3D compiler includes a fake file path in some of the warning or error
// messages, so lets remove all occurrences of this fake file path from the log.
void InfoLog::appendSanitized(const char *message)
{
    ensureInitialized();

    std::string msg(message);

    size_t found;
    do
    {
        found = msg.find(g_fakepath);
        if (found != std::string::npos)
        {
            msg.erase(found, strlen(g_fakepath));
        }
    } while (found != std::string::npos);

    if (!msg.empty())
    {
        *mLazyStream << message << std::endl;
    }
}

void InfoLog::reset()
{
    if (mLazyStream)
    {
        mLazyStream.reset(nullptr);
    }
}

bool InfoLog::empty() const
{
    if (!mLazyStream)
    {
        return true;
    }

    return mLazyStream->rdbuf()->in_avail() == 0;
}

void LogLinkMismatch(InfoLog &infoLog,
                     const std::string &variableName,
                     const char *variableType,
                     LinkMismatchError linkError,
                     const std::string &mismatchedStructOrBlockFieldName,
                     ShaderType shaderType1,
                     ShaderType shaderType2)
{
    std::ostringstream stream;
    stream << GetLinkMismatchErrorString(linkError) << "s of " << variableType << " '"
           << variableName;

    if (!mismatchedStructOrBlockFieldName.empty())
    {
        stream << "' member '" << variableName << "." << mismatchedStructOrBlockFieldName;
    }

    stream << "' differ between " << GetShaderTypeString(shaderType1) << " and "
           << GetShaderTypeString(shaderType2) << " shaders.";

    infoLog << stream.str();
}

bool IsActiveInterfaceBlock(const sh::InterfaceBlock &interfaceBlock)
{
    // Only 'packed' blocks are allowed to be considered inactive.
    return interfaceBlock.active || interfaceBlock.layout != sh::BLOCKLAYOUT_PACKED;
}

void WriteBlockMemberInfo(BinaryOutputStream *stream, const sh::BlockMemberInfo &var)
{
    stream->writeInt<GLenum>(var.type);
    stream->writeInt(var.arrayStride);
    stream->writeBool(var.isRowMajorMatrix);
    stream->writeInt(var.matrixStride);
    stream->writeInt(var.arraySize);
    stream->writeInt(var.offset);
    stream->writeInt(var.topLevelArrayStride);
}

void LoadBlockMemberInfo(BinaryInputStream *stream, sh::BlockMemberInfo *var)
{
    var->type                = stream->readInt<GLenum>();
    var->arrayStride         = stream->readInt<int>();
    var->isRowMajorMatrix    = stream->readBool();
    var->matrixStride        = stream->readInt<int>();
    var->arraySize           = stream->readInt<int>();
    var->offset              = stream->readInt<int>();
    var->topLevelArrayStride = stream->readInt<int>();
}

// ProgramInput implementation.
ProgramInput::ProgramInput() {}

ProgramInput::ProgramInput(const sh::ShaderVariable &var)
{
    ASSERT(!var.isStruct());

    name       = var.name;
    mappedName = var.mappedName;

    SetBitField(basicDataTypeStruct.type, var.type);
    basicDataTypeStruct.location = var.hasImplicitLocation ? -1 : var.location;
    SetBitField(basicDataTypeStruct.interpolation, var.interpolation);
    basicDataTypeStruct.flagBitsAsUByte              = 0;
    basicDataTypeStruct.flagBits.active              = var.active;
    basicDataTypeStruct.flagBits.isPatch             = var.isPatch;
    basicDataTypeStruct.flagBits.hasImplicitLocation = var.hasImplicitLocation;
    basicDataTypeStruct.flagBits.isArray             = var.isArray();
    basicDataTypeStruct.flagBits.isBuiltIn           = IsBuiltInName(var.name);
    SetBitField(basicDataTypeStruct.basicTypeElementCount, var.getBasicTypeElementCount());
    basicDataTypeStruct.id = var.id;
    SetBitField(basicDataTypeStruct.arraySizeProduct, var.getArraySizeProduct());
}

ProgramInput::ProgramInput(const ProgramInput &other)
{
    *this = other;
}

ProgramInput &ProgramInput::operator=(const ProgramInput &rhs)
{
    if (this != &rhs)
    {
        name                = rhs.name;
        mappedName          = rhs.mappedName;
        basicDataTypeStruct = rhs.basicDataTypeStruct;
    }
    return *this;
}

// VariableLocation implementation.
VariableLocation::VariableLocation() : arrayIndex(0), index(kUnused), ignored(false) {}

VariableLocation::VariableLocation(unsigned int arrayIndex, unsigned int index)
    : arrayIndex(arrayIndex), index(index), ignored(false)
{
    ASSERT(arrayIndex != GL_INVALID_INDEX);
}

// ProgramBindings implementation.
ProgramBindings::ProgramBindings() {}

ProgramBindings::~ProgramBindings() {}

void ProgramBindings::bindLocation(GLuint index, const std::string &name)
{
    mBindings[name] = index;
}

int ProgramBindings::getBindingByName(const std::string &name) const
{
    auto iter = mBindings.find(name);
    return (iter != mBindings.end()) ? iter->second : -1;
}

template <typename T>
int ProgramBindings::getBinding(const T &variable) const
{
    return getBindingByName(variable.name);
}

ProgramBindings::const_iterator ProgramBindings::begin() const
{
    return mBindings.begin();
}

ProgramBindings::const_iterator ProgramBindings::end() const
{
    return mBindings.end();
}

std::map<std::string, GLuint> ProgramBindings::getStableIterationMap() const
{
    return std::map<std::string, GLuint>(mBindings.begin(), mBindings.end());
}

// ProgramAliasedBindings implementation.
ProgramAliasedBindings::ProgramAliasedBindings() {}

ProgramAliasedBindings::~ProgramAliasedBindings() {}

void ProgramAliasedBindings::bindLocation(GLuint index, const std::string &name)
{
    mBindings[name] = ProgramBinding(index);

    // EXT_blend_func_extended spec: "If it specifies the base name of an array,
    // it identifies the resources associated with the first element of the array."
    //
    // Normalize array bindings so that "name" and "name[0]" map to the same entry.
    // If this binding is of the form "name[0]", then mark the "name" binding as
    // aliased but do not update it yet in case "name" is not actually an array.
    size_t nameLengthWithoutArrayIndex;
    unsigned int arrayIndex = ParseArrayIndex(name, &nameLengthWithoutArrayIndex);
    if (arrayIndex == 0)
    {
        std::string baseName = name.substr(0u, nameLengthWithoutArrayIndex);
        auto iter            = mBindings.find(baseName);
        if (iter != mBindings.end())
        {
            iter->second.aliased = true;
        }
    }
}

int ProgramAliasedBindings::getBindingByName(const std::string &name) const
{
    auto iter = mBindings.find(name);
    return (iter != mBindings.end()) ? iter->second.location : -1;
}

int ProgramAliasedBindings::getBindingByLocation(GLuint location) const
{
    for (const auto &iter : mBindings)
    {
        if (iter.second.location == location)
        {
            return iter.second.location;
        }
    }
    return -1;
}

template <typename T>
int ProgramAliasedBindings::getBinding(const T &variable) const
{
    const std::string &name = variable.name;

    // Check with the normalized array name if applicable.
    if (variable.isArray())
    {
        size_t nameLengthWithoutArrayIndex;
        unsigned int arrayIndex = ParseArrayIndex(name, &nameLengthWithoutArrayIndex);
        if (arrayIndex == 0)
        {
            std::string baseName = name.substr(0u, nameLengthWithoutArrayIndex);
            auto iter            = mBindings.find(baseName);
            // If "name" exists and is not aliased, that means it was modified more
            // recently than its "name[0]" form and should be used instead of that.
            if (iter != mBindings.end() && !iter->second.aliased)
            {
                return iter->second.location;
            }
        }
        else if (arrayIndex == GL_INVALID_INDEX)
        {
            auto iter = mBindings.find(variable.name);
            // If "name" exists and is not aliased, that means it was modified more
            // recently than its "name[0]" form and should be used instead of that.
            if (iter != mBindings.end() && !iter->second.aliased)
            {
                return iter->second.location;
            }
            // The base name was aliased, so use the name with the array notation.
            return getBindingByName(name + "[0]");
        }
    }

    return getBindingByName(name);
}
template int ProgramAliasedBindings::getBinding<gl::UsedUniform>(
    const gl::UsedUniform &variable) const;
template int ProgramAliasedBindings::getBinding<sh::ShaderVariable>(
    const sh::ShaderVariable &variable) const;

ProgramAliasedBindings::const_iterator ProgramAliasedBindings::begin() const
{
    return mBindings.begin();
}

ProgramAliasedBindings::const_iterator ProgramAliasedBindings::end() const
{
    return mBindings.end();
}

std::map<std::string, ProgramBinding> ProgramAliasedBindings::getStableIterationMap() const
{
    return std::map<std::string, ProgramBinding>(mBindings.begin(), mBindings.end());
}

// ImageBinding implementation.
ImageBinding::ImageBinding(size_t count, TextureType textureTypeIn)
    : textureType(textureTypeIn), boundImageUnits(count, 0)
{}
ImageBinding::ImageBinding(GLuint imageUnit, size_t count, TextureType textureTypeIn)
    : textureType(textureTypeIn)
{
    for (size_t index = 0; index < count; ++index)
    {
        boundImageUnits.push_back(imageUnit + static_cast<GLuint>(index));
    }
}

ImageBinding::ImageBinding() = default;

ImageBinding::ImageBinding(const ImageBinding &other) = default;

ImageBinding::~ImageBinding() = default;

// ProgramState implementation.
ProgramState::ProgramState(rx::GLImplFactory *factory)
    : mLabel(),
      mAttachedShaders{},
      mTransformFeedbackBufferMode(GL_INTERLEAVED_ATTRIBS),
      mBinaryRetrieveableHint(false),
      mSeparable(false),
      mCachedBaseVertex(0),
      mCachedBaseInstance(0),
      mExecutable(new ProgramExecutable(factory, &mInfoLog))
{}

ProgramState::~ProgramState()
{
    ASSERT(!hasAnyAttachedShader());
}

const std::string &ProgramState::getLabel()
{
    return mLabel;
}

SharedCompiledShaderState ProgramState::getAttachedShader(ShaderType shaderType) const
{
    ASSERT(shaderType != ShaderType::InvalidEnum);
    return mAttachedShaders[shaderType];
}

GLuint ProgramState::getUniformIndexFromName(const std::string &name) const
{
    return GetUniformIndexFromName(mExecutable->mUniforms, mExecutable->mUniformNames, name);
}

GLuint ProgramState::getBufferVariableIndexFromName(const std::string &name) const
{
    return GetResourceIndexFromName(mExecutable->mBufferVariables, name);
}

GLuint ProgramState::getUniformIndexFromLocation(UniformLocation location) const
{
    ASSERT(location.value >= 0 &&
           static_cast<size_t>(location.value) < mExecutable->mUniformLocations.size());
    return mExecutable->mUniformLocations[location.value].index;
}

Optional<GLuint> ProgramState::getSamplerIndex(UniformLocation location) const
{
    GLuint index = getUniformIndexFromLocation(location);
    if (!isSamplerUniformIndex(index))
    {
        return Optional<GLuint>::Invalid();
    }

    return getSamplerIndexFromUniformIndex(index);
}

bool ProgramState::isSamplerUniformIndex(GLuint index) const
{
    return mExecutable->mPODStruct.samplerUniformRange.contains(index);
}

GLuint ProgramState::getSamplerIndexFromUniformIndex(GLuint uniformIndex) const
{
    ASSERT(isSamplerUniformIndex(uniformIndex));
    return uniformIndex - mExecutable->mPODStruct.samplerUniformRange.low();
}

GLuint ProgramState::getUniformIndexFromSamplerIndex(GLuint samplerIndex) const
{
    return mExecutable->getUniformIndexFromSamplerIndex(samplerIndex);
}

bool ProgramState::isImageUniformIndex(GLuint index) const
{
    return mExecutable->mPODStruct.imageUniformRange.contains(index);
}

GLuint ProgramState::getImageIndexFromUniformIndex(GLuint uniformIndex) const
{
    ASSERT(isImageUniformIndex(uniformIndex));
    return uniformIndex - mExecutable->mPODStruct.imageUniformRange.low();
}

bool ProgramState::hasAnyAttachedShader() const
{
    for (const SharedCompiledShaderState &shader : mAttachedShaders)
    {
        if (shader)
        {
            return true;
        }
    }
    return false;
}

ShaderType ProgramState::getFirstAttachedShaderStageType() const
{
    const ShaderBitSet linkedStages = mExecutable->getLinkedShaderStages();
    if (linkedStages.none())
    {
        return ShaderType::InvalidEnum;
    }

    return linkedStages.first();
}

ShaderType ProgramState::getLastAttachedShaderStageType() const
{
    const ShaderBitSet linkedStages = mExecutable->getLinkedShaderStages();
    if (linkedStages.none())
    {
        return ShaderType::InvalidEnum;
    }

    return linkedStages.last();
}

ShaderType ProgramState::getAttachedTransformFeedbackStage() const
{
    if (mAttachedShaders[ShaderType::Geometry])
    {
        return ShaderType::Geometry;
    }
    if (mAttachedShaders[ShaderType::TessEvaluation])
    {
        return ShaderType::TessEvaluation;
    }
    return ShaderType::Vertex;
}

Program::Program(rx::GLImplFactory *factory, ShaderProgramManager *manager, ShaderProgramID handle)
    : mSerial(factory->generateSerial()),
      mState(factory),
      mProgram(factory->createProgram(mState)),
      mValidated(false),
      mLinked(false),
      mDeleteStatus(false),
      mRefCount(0),
      mResourceManager(manager),
      mHandle(handle),
      mAttachedShaders{}
{
    ASSERT(mProgram);

    unlink();
}

Program::~Program()
{
    ASSERT(!mProgram);
}

void Program::onDestroy(const Context *context)
{
    resolveLink(context);
    for (ShaderType shaderType : AllShaderTypes())
    {
        Shader *shader = getAttachedShader(shaderType);
        if (shader != nullptr)
        {
            shader->release(context);
        }
        mState.mAttachedShaders[shaderType].reset();
        mAttachedShaders[shaderType] = nullptr;
    }

    mProgram->destroy(context);
    mState.mExecutable->destroy(context);

    ASSERT(!mState.hasAnyAttachedShader());
    SafeDelete(mProgram);

    delete this;
}
ShaderProgramID Program::id() const
{
    ASSERT(!mLinkingState);
    return mHandle;
}

angle::Result Program::setLabel(const Context *context, const std::string &label)
{
    ASSERT(!mLinkingState);
    mState.mLabel = label;

    if (mProgram)
    {
        return mProgram->onLabelUpdate(context);
    }
    return angle::Result::Continue;
}

const std::string &Program::getLabel() const
{
    ASSERT(!mLinkingState);
    return mState.mLabel;
}

void Program::attachShader(const Context *context, Shader *shader)
{
    resolveLink(context);
    ShaderType shaderType = shader->getType();
    ASSERT(shaderType != ShaderType::InvalidEnum);

    shader->addRef();
    mAttachedShaders[shaderType] = shader;
}

void Program::detachShader(const Context *context, Shader *shader)
{
    resolveLink(context);
    ShaderType shaderType = shader->getType();
    ASSERT(shaderType != ShaderType::InvalidEnum);

    ASSERT(mAttachedShaders[shaderType] == shader);
    shader->release(context);
    mAttachedShaders[shaderType] = nullptr;
    mState.mAttachedShaders[shaderType].reset();
}

int Program::getAttachedShadersCount() const
{
    ASSERT(!mLinkingState);
    int numAttachedShaders = 0;
    for (const Shader *shader : mAttachedShaders)
    {
        if (shader != nullptr)
        {
            ++numAttachedShaders;
        }
    }

    return numAttachedShaders;
}

Shader *Program::getAttachedShader(ShaderType shaderType) const
{
    ASSERT(!mLinkingState);
    return mAttachedShaders[shaderType];
}

void Program::bindAttributeLocation(GLuint index, const char *name)
{
    ASSERT(!mLinkingState);
    mState.mAttributeBindings.bindLocation(index, name);
}

void Program::bindUniformLocation(UniformLocation location, const char *name)
{
    ASSERT(!mLinkingState);
    mState.mUniformLocationBindings.bindLocation(location.value, name);
}

void Program::bindFragmentOutputLocation(GLuint index, const char *name)
{
    ASSERT(!mLinkingState);
    mState.mFragmentOutputLocations.bindLocation(index, name);
}

void Program::bindFragmentOutputIndex(GLuint index, const char *name)
{
    ASSERT(!mLinkingState);
    mState.mFragmentOutputIndexes.bindLocation(index, name);
}

angle::Result Program::link(const Context *context)
{
    // Make sure no compile jobs are pending.
    //
    // For every attached shader, get the compiled state.  This is done at link time (instead of
    // earlier, such as attachShader time), because the shader could get recompiled between attach
    // and link.
    //
    // Additionally, make sure the backend is also able to cache the compiled state of its own
    // ShaderImpl objects.
    ShaderMap<rx::ShaderImpl *> shaderImpls = {};
    for (ShaderType shaderType : AllShaderTypes())
    {
        Shader *shader = mAttachedShaders[shaderType];
        SharedCompiledShaderState shaderCompiledState;
        if (shader != nullptr)
        {
            shader->resolveCompile(context);
            shaderCompiledState     = shader->getCompiledState();
            shaderImpls[shaderType] = shader->getImplementation();
        }
        mState.mAttachedShaders[shaderType] = std::move(shaderCompiledState);
    }
    mProgram->prepareForLink(shaderImpls);

    const angle::FrontendFeatures &frontendFeatures = context->getFrontendFeatures();
    if (frontendFeatures.dumpShaderSource.enabled)
    {
        dumpProgramInfo(context);
    }

    angle::Result result = linkImpl(context);

    return result;
}

// The attached shaders are checked for linking errors by matching up their variables.
// Uniform, input and output variables get collected.
// The code gets compiled into binaries.
angle::Result Program::linkImpl(const Context *context)
{
    ASSERT(!mLinkingState);
    // Don't make any local variables pointing to anything within the ProgramExecutable, since
    // unlink() resets the ProgramExecutable making any references/pointers invalid.
    auto *platform   = ANGLEPlatformCurrent();
    double startTime = platform->currentTime(platform);

    // Make sure the executable state is in sync with the program.  Only the transform feedback
    // buffer mode is duplicated in the executable as is the only link-input that is also needed at
    // draw time.
    mState.mExecutable->mPODStruct.transformFeedbackBufferMode =
        mState.mTransformFeedbackBufferMode;

    // Unlink the program, but do not clear the validation-related caching yet, since we can still
    // use the previously linked program if linking the shaders fails.
    mLinked = false;

    mState.mInfoLog.reset();

    // Validate we have properly attached shaders before checking the cache.
    if (!linkValidateShaders(context))
    {
        return angle::Result::Continue;
    }
    linkShaders();

    egl::BlobCache::Key programHash = {0};
    MemoryProgramCache *cache       = context->getMemoryProgramCache();

    // TODO: http://anglebug.com/4530: Enable program caching for separable programs
    if (cache && !isSeparable())
    {
        std::lock_guard<std::mutex> cacheLock(context->getProgramCacheMutex());
        angle::Result cacheResult = cache->getProgram(context, this, &programHash);
        ANGLE_TRY(cacheResult);

        // Check explicitly for Continue, Incomplete means a cache miss
        if (cacheResult == angle::Result::Continue)
        {
            std::scoped_lock lock(mHistogramMutex);
            // Succeeded in loading the binaries in the front-end, back end may still be loading
            // asynchronously
            double delta = platform->currentTime(platform) - startTime;
            int us       = static_cast<int>(delta * 1000000.0);
            ANGLE_HISTOGRAM_COUNTS("GPU.ANGLE.ProgramCache.ProgramCacheHitTimeUS", us);
            return angle::Result::Continue;
        }
    }

    const Caps &caps               = context->getCaps();
    const Limitations &limitations = context->getLimitations();
    const Version &clientVersion   = context->getClientVersion();
    const bool isWebGL             = context->isWebGL();

    // Cache load failed, fall through to normal linking.
    unlink();

    // Re-link shaders after the unlink call.
    linkShaders();

    std::unique_ptr<LinkingState> linkingState(new LinkingState());
    ProgramMergedVaryings mergedVaryings;
    LinkingVariables &linkingVariables = linkingState->linkingVariables;
    ProgramLinkedResources &resources  = linkingState->resources;

    linkingVariables.initForProgram(mState);
    resources.init(&mState.mExecutable->mUniformBlocks, &mState.mExecutable->mUniforms,
                   &mState.mExecutable->mUniformNames, &mState.mExecutable->mUniformMappedNames,
                   &mState.mExecutable->mShaderStorageBlocks, &mState.mExecutable->mBufferVariables,
                   &mState.mExecutable->mAtomicCounterBuffers);

    // TODO: Fix incomplete linking. http://anglebug.com/6358
    updateLinkedShaderStages();

    InitUniformBlockLinker(mState, &resources.uniformBlockLinker);
    InitShaderStorageBlockLinker(mState, &resources.shaderStorageBlockLinker);

    if (mState.mAttachedShaders[ShaderType::Compute])
    {
        GLuint combinedImageUniforms = 0;
        if (!linkUniforms(caps, clientVersion, &resources.unusedUniforms, &combinedImageUniforms))
        {
            return angle::Result::Continue;
        }

        GLuint combinedShaderStorageBlocks = 0u;
        if (!LinkValidateProgramInterfaceBlocks(
                caps, clientVersion, isWebGL, mState.mExecutable->getLinkedShaderStages(),
                resources, mState.mInfoLog, &combinedShaderStorageBlocks))
        {
            return angle::Result::Continue;
        }

        // [OpenGL ES 3.1] Chapter 8.22 Page 203:
        // A link error will be generated if the sum of the number of active image uniforms used in
        // all shaders, the number of active shader storage blocks, and the number of active
        // fragment shader outputs exceeds the implementation-dependent value of
        // MAX_COMBINED_SHADER_OUTPUT_RESOURCES.
        if (combinedImageUniforms + combinedShaderStorageBlocks >
            static_cast<GLuint>(caps.maxCombinedShaderOutputResources))
        {
            mState.mInfoLog
                << "The sum of the number of active image uniforms, active shader storage blocks "
                   "and active fragment shader outputs exceeds "
                   "MAX_COMBINED_SHADER_OUTPUT_RESOURCES ("
                << caps.maxCombinedShaderOutputResources << ")";
            return angle::Result::Continue;
        }
    }
    else
    {
        if (!linkAttributes(caps, limitations, isWebGL))
        {
            return angle::Result::Continue;
        }

        if (!linkVaryings())
        {
            return angle::Result::Continue;
        }

        GLuint combinedImageUniforms = 0;
        if (!linkUniforms(caps, clientVersion, &resources.unusedUniforms, &combinedImageUniforms))
        {
            return angle::Result::Continue;
        }

        GLuint combinedShaderStorageBlocks = 0u;
        if (!LinkValidateProgramInterfaceBlocks(
                caps, clientVersion, isWebGL, mState.mExecutable->getLinkedShaderStages(),
                resources, mState.mInfoLog, &combinedShaderStorageBlocks))
        {
            return angle::Result::Continue;
        }

        if (!LinkValidateProgramGlobalNames(mState.mInfoLog, getExecutable(), linkingVariables))
        {
            return angle::Result::Continue;
        }

        const SharedCompiledShaderState &vertexShader = mState.mAttachedShaders[ShaderType::Vertex];
        if (vertexShader)
        {
            mState.mExecutable->mPODStruct.numViews        = vertexShader->numViews;
            mState.mExecutable->mPODStruct.hasClipDistance = vertexShader->hasClipDistance;
            mState.mExecutable->mPODStruct.specConstUsageBits |= vertexShader->specConstUsageBits;
        }

        const SharedCompiledShaderState &fragmentShader =
            mState.mAttachedShaders[ShaderType::Fragment];
        if (fragmentShader)
        {
            if (!mState.mExecutable->linkValidateOutputVariables(
                    caps, clientVersion, combinedImageUniforms, combinedShaderStorageBlocks,
                    fragmentShader->activeOutputVariables, fragmentShader->shaderVersion,
                    mState.mFragmentOutputLocations, mState.mFragmentOutputIndexes))
            {
                return angle::Result::Continue;
            }

            mState.mExecutable->mPODStruct.hasDiscard = fragmentShader->hasDiscard;
            mState.mExecutable->mPODStruct.enablesPerSampleShading =
                fragmentShader->enablesPerSampleShading;
            mState.mExecutable->mPODStruct.advancedBlendEquations =
                fragmentShader->advancedBlendEquations;
            mState.mExecutable->mPODStruct.specConstUsageBits |= fragmentShader->specConstUsageBits;
        }

        mergedVaryings = GetMergedVaryingsFromLinkingVariables(linkingVariables);
        if (!mState.mExecutable->linkMergedVaryings(
                caps, limitations, clientVersion, isWebGL, mergedVaryings,
                mState.mTransformFeedbackVaryingNames, linkingVariables, isSeparable(),
                &resources.varyingPacking))
        {
            return angle::Result::Continue;
        }
    }

    mState.mExecutable->saveLinkedStateInfo(mState);

    mLinkingState                    = std::move(linkingState);
    mLinkingState->linkingFromBinary = false;
    mLinkingState->programHash       = programHash;
    mLinkingState->linkEvent = mProgram->link(context, resources, std::move(mergedVaryings));

    // Must be after mProgram->link() to avoid misleading the linker about output variables.
    mState.updateProgramInterfaceInputs();
    mState.updateProgramInterfaceOutputs();

    return angle::Result::Continue;
}

bool Program::isLinking() const
{
    return (mLinkingState.get() && mLinkingState->linkEvent &&
            mLinkingState->linkEvent->isLinking());
}

void Program::resolveLinkImpl(const Context *context)
{
    ASSERT(mLinkingState.get());

    angle::Result result = mLinkingState->linkEvent->wait(context);

    mLinked                                    = result == angle::Result::Continue;
    std::unique_ptr<LinkingState> linkingState = std::move(mLinkingState);
    if (!mLinked)
    {
        mState.mExecutable->reset();
        return;
    }

    // According to GLES 3.0/3.1 spec for LinkProgram and UseProgram,
    // Only successfully linked program can replace the executables.
    ASSERT(mLinked);

    // Mark implementation-specific unreferenced uniforms as ignored.
    std::vector<ImageBinding> *imageBindings = getExecutable().getImageBindings();
    mProgram->markUnusedUniformLocations(&mState.mExecutable->mUniformLocations,
                                         &mState.mExecutable->mSamplerBindings, imageBindings);

    // Must be called after markUnusedUniformLocations.
    postResolveLink(context);

    if (linkingState->linkingFromBinary)
    {
        // All internal Program state is already loaded from the binary.
        return;
    }

    // Save to the program cache.
    std::lock_guard<std::mutex> cacheLock(context->getProgramCacheMutex());
    MemoryProgramCache *cache = context->getMemoryProgramCache();
    // TODO: http://anglebug.com/4530: Enable program caching for separable programs
    if (cache && !isSeparable() && !context->getFrontendFeatures().disableProgramCaching.enabled &&
        (mState.mExecutable->mLinkedTransformFeedbackVaryings.empty() ||
         !context->getFrontendFeatures().disableProgramCachingForTransformFeedback.enabled))
    {
        if (cache->putProgram(linkingState->programHash, context, this) == angle::Result::Stop)
        {
            // Don't fail linking if putting the program binary into the cache fails, the program is
            // still usable.
            ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                               "Failed to save linked program to memory program cache.");
        }
    }
}

void Program::updateLinkedShaderStages()
{
    mState.mExecutable->resetLinkedShaderStages();

    for (ShaderType shaderType : AllShaderTypes())
    {
        if (mState.mAttachedShaders[shaderType])
        {
            mState.mExecutable->setLinkedShaderStages(shaderType);
        }
    }
}

void ProgramState::updateActiveSamplers()
{
    mExecutable->mActiveSamplerRefCounts.fill(0);
    mExecutable->updateActiveSamplers(*this);
}

void ProgramState::updateProgramInterfaceInputs()
{
    const ShaderType firstAttachedShaderType = getFirstAttachedShaderStageType();

    if (firstAttachedShaderType == ShaderType::Vertex)
    {
        // Vertex attributes are already what we need, so nothing to do
        return;
    }

    const SharedCompiledShaderState &shader = getAttachedShader(firstAttachedShaderType);
    ASSERT(shader);

    // Copy over each input varying, since the Shader could go away
    if (shader->shaderType == ShaderType::Compute)
    {
        for (const sh::ShaderVariable &attribute : shader->allAttributes)
        {
            // Compute Shaders have the following built-in input variables.
            //
            // in uvec3 gl_NumWorkGroups;
            // in uvec3 gl_WorkGroupID;
            // in uvec3 gl_LocalInvocationID;
            // in uvec3 gl_GlobalInvocationID;
            // in uint  gl_LocalInvocationIndex;
            // They are all vecs or uints, so no special handling is required.
            mExecutable->mProgramInputs.emplace_back(attribute);
        }
    }
    else
    {
        for (const sh::ShaderVariable &varying : shader->inputVaryings)
        {
            UpdateInterfaceVariable(&mExecutable->mProgramInputs, varying);
        }
    }
}

void ProgramState::updateProgramInterfaceOutputs()
{
    const ShaderType lastAttachedShaderType = getLastAttachedShaderStageType();

    if (lastAttachedShaderType == ShaderType::Fragment)
    {
        // Fragment outputs are already what we need, so nothing to do
        return;
    }
    if (lastAttachedShaderType == ShaderType::Compute)
    {
        // If the program only contains a Compute Shader, then there are no user-defined outputs.
        return;
    }

    const SharedCompiledShaderState &shader = getAttachedShader(lastAttachedShaderType);
    ASSERT(shader);

    // Copy over each output varying, since the Shader could go away
    for (const sh::ShaderVariable &varying : shader->outputVaryings)
    {
        UpdateInterfaceVariable(&mExecutable->mOutputVariables, varying);
    }
}

// Returns the program object to an unlinked state, before re-linking, or at destruction
void Program::unlink()
{
    mState.mExecutable->reset();

    mState.mCachedBaseVertex   = 0;
    mState.mCachedBaseInstance = 0;

    mValidated = false;

    mLinked = false;
}

angle::Result Program::loadBinary(const Context *context,
                                  GLenum binaryFormat,
                                  const void *binary,
                                  GLsizei length)
{
    mState.mInfoLog.reset();

    ASSERT(!mLinkingState);
    unlink();

    ASSERT(binaryFormat == GL_PROGRAM_BINARY_ANGLE);
    if (binaryFormat != GL_PROGRAM_BINARY_ANGLE)
    {
        mState.mInfoLog << "Invalid program binary format.";
        return angle::Result::Incomplete;
    }

    BinaryInputStream stream(binary, length);
    ANGLE_TRY(deserialize(context, stream));
    // Currently we require the full shader text to compute the program hash.
    // We could also store the binary in the internal program cache.

    for (size_t uniformBlockIndex = 0;
         uniformBlockIndex < mState.mExecutable->getActiveUniformBlockCount(); ++uniformBlockIndex)
    {
        mDirtyBits.set(uniformBlockIndex);
    }

    // The rx::LinkEvent returned from ProgramImpl::load is a base class with multiple
    // implementations. In some implementations, a background thread is used to compile the
    // shaders. Any calls to the LinkEvent object, therefore, are racy and may interfere with
    // the operation.

    // We do not want to call LinkEvent::wait because that will cause the background thread
    // to finish its task before returning, thus defeating the purpose of background compilation.
    // We need to defer waiting on background compilation until the very last minute when we
    // absolutely need the results, such as when the developer binds the program or queries
    // for the completion status.

    // If load returns nullptr, we know for sure that the binary is not compatible with the backend.
    // The loaded binary could have been read from the on-disk shader cache and be corrupted or
    // serialized with different revision and subsystem id than the currently loaded backend.
    // Returning 'Incomplete' to the caller results in link happening using the original shader
    // sources.
    angle::Result result;
    std::unique_ptr<LinkingState> linkingState;
    std::unique_ptr<rx::LinkEvent> linkEvent = mProgram->load(context, &stream);
    if (linkEvent)
    {
        linkingState                    = std::make_unique<LinkingState>();
        linkingState->linkingFromBinary = true;
        linkingState->linkEvent         = std::move(linkEvent);
        result                          = angle::Result::Continue;
    }
    else
    {
        result = angle::Result::Incomplete;
    }
    mLinkingState = std::move(linkingState);

    return result;
}

angle::Result Program::saveBinary(Context *context,
                                  GLenum *binaryFormat,
                                  void *binary,
                                  GLsizei bufSize,
                                  GLsizei *length) const
{
    ASSERT(!mLinkingState);
    if (binaryFormat)
    {
        *binaryFormat = GL_PROGRAM_BINARY_ANGLE;
    }

    angle::MemoryBuffer memoryBuf;
    ANGLE_TRY(serialize(context, &memoryBuf));

    GLsizei streamLength       = static_cast<GLsizei>(memoryBuf.size());
    const uint8_t *streamState = memoryBuf.data();

    if (streamLength > bufSize)
    {
        if (length)
        {
            *length = 0;
        }

        // TODO: This should be moved to the validation layer but computing the size of the binary
        // before saving it causes the save to happen twice.  It may be possible to write the binary
        // to a separate buffer, validate sizes and then copy it.
        ANGLE_CHECK(context, false, "Insufficient buffer size", GL_INVALID_OPERATION);
    }

    if (binary)
    {
        char *ptr = reinterpret_cast<char *>(binary);

        memcpy(ptr, streamState, streamLength);
        ptr += streamLength;

        ASSERT(ptr - streamLength == binary);
    }

    if (length)
    {
        *length = streamLength;
    }

    return angle::Result::Continue;
}

GLint Program::getBinaryLength(Context *context) const
{
    ASSERT(!mLinkingState);
    if (!mLinked)
    {
        return 0;
    }

    GLint length;
    angle::Result result =
        saveBinary(context, nullptr, nullptr, std::numeric_limits<GLint>::max(), &length);
    if (result != angle::Result::Continue)
    {
        return 0;
    }

    return length;
}

void Program::setBinaryRetrievableHint(bool retrievable)
{
    ASSERT(!mLinkingState);
    // TODO(jmadill) : replace with dirty bits
    mProgram->setBinaryRetrievableHint(retrievable);
    mState.mBinaryRetrieveableHint = retrievable;
}

bool Program::getBinaryRetrievableHint() const
{
    ASSERT(!mLinkingState);
    return mState.mBinaryRetrieveableHint;
}

int Program::getInfoLogLength() const
{
    return static_cast<int>(mState.mInfoLog.getLength());
}

void Program::getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const
{
    return mState.mInfoLog.getLog(bufSize, length, infoLog);
}

void Program::setSeparable(bool separable)
{
    ASSERT(!mLinkingState);
    if (isSeparable() != separable)
    {
        mProgram->setSeparable(separable);
        mState.mSeparable = separable;
    }
}

void Program::deleteSelf(const Context *context)
{
    ASSERT(mRefCount == 0 && mDeleteStatus);
    mResourceManager->deleteProgram(context, mHandle);
}

unsigned int Program::getRefCount() const
{
    return mRefCount;
}

void Program::getAttachedShaders(GLsizei maxCount, GLsizei *count, ShaderProgramID *shaders) const
{
    ASSERT(!mLinkingState);
    int total = 0;

    for (const Shader *shader : mAttachedShaders)
    {
        if (shader != nullptr && total < maxCount)
        {
            shaders[total] = shader->getHandle();
            ++total;
        }
    }

    if (count)
    {
        *count = total;
    }
}

GLuint Program::getAttributeLocation(const std::string &name) const
{
    ASSERT(!mLinkingState);
    return mState.getAttributeLocation(name);
}

void Program::getActiveAttribute(GLuint index,
                                 GLsizei bufsize,
                                 GLsizei *length,
                                 GLint *size,
                                 GLenum *type,
                                 GLchar *name) const
{
    ASSERT(!mLinkingState);
    if (!mLinked)
    {
        if (bufsize > 0)
        {
            name[0] = '\0';
        }

        if (length)
        {
            *length = 0;
        }

        *type = GL_NONE;
        *size = 1;
        return;
    }

    ASSERT(index < mState.mExecutable->getProgramInputs().size());
    const ProgramInput &attrib = mState.mExecutable->getProgramInputs()[index];

    if (bufsize > 0)
    {
        CopyStringToBuffer(name, attrib.name, bufsize, length);
    }

    // Always a single 'type' instance
    *size = 1;
    *type = attrib.getType();
}

GLint Program::getActiveAttributeCount() const
{
    ASSERT(!mLinkingState);
    if (!mLinked)
    {
        return 0;
    }

    return static_cast<GLint>(mState.mExecutable->getProgramInputs().size());
}

GLint Program::getActiveAttributeMaxLength() const
{
    ASSERT(!mLinkingState);
    if (!mLinked)
    {
        return 0;
    }

    size_t maxLength = 0;

    for (const ProgramInput &attrib : mState.mExecutable->getProgramInputs())
    {
        maxLength = std::max(attrib.name.length() + 1, maxLength);
    }

    return static_cast<GLint>(maxLength);
}

const sh::WorkGroupSize &Program::getComputeShaderLocalSize() const
{
    ASSERT(!mLinkingState);
    return mState.getComputeShaderLocalSize();
}

PrimitiveMode Program::getGeometryShaderInputPrimitiveType() const
{
    ASSERT(!mLinkingState && mState.mExecutable);
    return mState.mExecutable->getGeometryShaderInputPrimitiveType();
}
PrimitiveMode Program::getGeometryShaderOutputPrimitiveType() const
{
    ASSERT(!mLinkingState && mState.mExecutable);
    return mState.mExecutable->getGeometryShaderOutputPrimitiveType();
}
GLint Program::getGeometryShaderInvocations() const
{
    ASSERT(!mLinkingState && mState.mExecutable);
    return mState.mExecutable->getGeometryShaderInvocations();
}
GLint Program::getGeometryShaderMaxVertices() const
{
    ASSERT(!mLinkingState && mState.mExecutable);
    return mState.mExecutable->getGeometryShaderMaxVertices();
}

GLint Program::getTessControlShaderVertices() const
{
    ASSERT(!mLinkingState && mState.mExecutable);
    return mState.mExecutable->mPODStruct.tessControlShaderVertices;
}

GLenum Program::getTessGenMode() const
{
    ASSERT(!mLinkingState && mState.mExecutable);
    return mState.mExecutable->mPODStruct.tessGenMode;
}

GLenum Program::getTessGenPointMode() const
{
    ASSERT(!mLinkingState && mState.mExecutable);
    return mState.mExecutable->mPODStruct.tessGenPointMode;
}

GLenum Program::getTessGenSpacing() const
{
    ASSERT(!mLinkingState && mState.mExecutable);
    return mState.mExecutable->mPODStruct.tessGenSpacing;
}

GLenum Program::getTessGenVertexOrder() const
{
    ASSERT(!mLinkingState && mState.mExecutable);
    return mState.mExecutable->mPODStruct.tessGenVertexOrder;
}

const ProgramInput &Program::getInputResource(size_t index) const
{
    ASSERT(!mLinkingState);
    ASSERT(index < mState.mExecutable->getProgramInputs().size());
    return mState.mExecutable->getProgramInputs()[index];
}

GLuint Program::getInputResourceIndex(const GLchar *name) const
{
    ASSERT(!mLinkingState);
    const std::string nameString = StripLastArrayIndex(name);

    for (size_t index = 0; index < mState.mExecutable->getProgramInputs().size(); index++)
    {
        ProgramInput resource = getInputResource(index);
        if (resource.name == nameString)
        {
            return static_cast<GLuint>(index);
        }
    }

    return GL_INVALID_INDEX;
}

GLuint Program::getInputResourceMaxNameSize() const
{
    GLint max = 0;

    for (const ProgramInput &resource : mState.mExecutable->getProgramInputs())
    {
        max = GetResourceMaxNameSize(resource, max);
    }

    return max;
}

GLuint Program::getOutputResourceMaxNameSize() const
{
    GLint max = 0;

    for (const sh::ShaderVariable &resource : mState.mExecutable->getOutputVariables())
    {
        max = GetResourceMaxNameSize(resource, max);
    }

    return max;
}

GLuint Program::getInputResourceLocation(const GLchar *name) const
{
    const GLuint index = getInputResourceIndex(name);
    if (index == GL_INVALID_INDEX)
    {
        return index;
    }

    const ProgramInput &variable = getInputResource(index);

    return GetResourceLocation(name, variable, variable.getLocation());
}

GLuint Program::getOutputResourceLocation(const GLchar *name) const
{
    const GLuint index = getOutputResourceIndex(name);
    if (index == GL_INVALID_INDEX)
    {
        return index;
    }

    const sh::ShaderVariable &variable = getOutputResource(index);

    return GetResourceLocation(name, variable, variable.location);
}

GLuint Program::getOutputResourceIndex(const GLchar *name) const
{
    ASSERT(!mLinkingState);
    const std::string nameString = StripLastArrayIndex(name);

    for (size_t index = 0; index < mState.mExecutable->getOutputVariables().size(); index++)
    {
        sh::ShaderVariable resource = getOutputResource(index);
        if (resource.name == nameString)
        {
            return static_cast<GLuint>(index);
        }
    }

    return GL_INVALID_INDEX;
}

size_t Program::getOutputResourceCount() const
{
    ASSERT(!mLinkingState);
    return (mLinked ? mState.mExecutable->getOutputVariables().size() : 0);
}

void Program::getResourceName(const std::string name,
                              GLsizei bufSize,
                              GLsizei *length,
                              GLchar *dest) const
{
    if (length)
    {
        *length = 0;
    }

    if (!mLinked)
    {
        if (bufSize > 0)
        {
            dest[0] = '\0';
        }
        return;
    }

    if (bufSize > 0)
    {
        CopyStringToBuffer(dest, name, bufSize, length);
    }
}

void Program::getInputResourceName(GLuint index,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLchar *name) const
{
    ASSERT(!mLinkingState);
    getResourceName(getInputResourceName(index), bufSize, length, name);
}

void Program::getOutputResourceName(GLuint index,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLchar *name) const
{
    ASSERT(!mLinkingState);
    getResourceName(getOutputResourceName(index), bufSize, length, name);
}

void Program::getUniformResourceName(GLuint index,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *name) const
{
    ASSERT(!mLinkingState);
    ASSERT(index < mState.mExecutable->getUniforms().size());
    getResourceName(mState.mExecutable->getUniformNameByIndex(index), bufSize, length, name);
}

void Program::getBufferVariableResourceName(GLuint index,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLchar *name) const
{
    ASSERT(!mLinkingState);
    ASSERT(index < mState.mExecutable->mBufferVariables.size());
    getResourceName(mState.mExecutable->mBufferVariables[index].name, bufSize, length, name);
}

const std::string Program::getInputResourceName(GLuint index) const
{
    ASSERT(!mLinkingState);
    const ProgramInput &resource = getInputResource(index);

    return GetResourceName(resource);
}

const std::string Program::getOutputResourceName(GLuint index) const
{
    ASSERT(!mLinkingState);
    const sh::ShaderVariable &resource = getOutputResource(index);

    return GetResourceName(resource);
}

const sh::ShaderVariable &Program::getOutputResource(size_t index) const
{
    ASSERT(!mLinkingState);
    ASSERT(index < mState.mExecutable->getOutputVariables().size());
    return mState.mExecutable->getOutputVariables()[index];
}

const std::vector<GLsizei> &Program::getTransformFeedbackStrides() const
{
    ASSERT(!mLinkingState);
    return mState.mExecutable->getTransformFeedbackStrides();
}

GLint Program::getFragDataLocation(const std::string &name) const
{
    ASSERT(!mLinkingState);
    GLint primaryLocation = GetVariableLocation(mState.mExecutable->getOutputVariables(),
                                                mState.mExecutable->getOutputLocations(), name);
    if (primaryLocation != -1)
    {
        return primaryLocation;
    }
    return GetVariableLocation(mState.mExecutable->getOutputVariables(),
                               mState.mExecutable->getSecondaryOutputLocations(), name);
}

GLint Program::getFragDataIndex(const std::string &name) const
{
    ASSERT(!mLinkingState);
    if (GetVariableLocation(mState.mExecutable->getOutputVariables(),
                            mState.mExecutable->getOutputLocations(), name) != -1)
    {
        return 0;
    }
    if (GetVariableLocation(mState.mExecutable->getOutputVariables(),
                            mState.mExecutable->getSecondaryOutputLocations(), name) != -1)
    {
        return 1;
    }
    return -1;
}

void Program::getActiveUniform(GLuint index,
                               GLsizei bufsize,
                               GLsizei *length,
                               GLint *size,
                               GLenum *type,
                               GLchar *name) const
{
    ASSERT(!mLinkingState);
    if (mLinked)
    {
        // index must be smaller than getActiveUniformCount()
        ASSERT(index < mState.mExecutable->getUniforms().size());
        const LinkedUniform &uniform = mState.mExecutable->getUniforms()[index];

        if (bufsize > 0)
        {
            std::string string = mState.mExecutable->getUniformNameByIndex(index);
            CopyStringToBuffer(name, string, bufsize, length);
        }

        *size = clampCast<GLint>(uniform.getBasicTypeElementCount());
        *type = uniform.getType();
    }
    else
    {
        if (bufsize > 0)
        {
            name[0] = '\0';
        }

        if (length)
        {
            *length = 0;
        }

        *size = 0;
        *type = GL_NONE;
    }
}

GLint Program::getActiveUniformCount() const
{
    ASSERT(!mLinkingState);
    if (mLinked)
    {
        return static_cast<GLint>(mState.mExecutable->getUniforms().size());
    }
    else
    {
        return 0;
    }
}

size_t Program::getActiveBufferVariableCount() const
{
    ASSERT(!mLinkingState);
    return mLinked ? mState.mExecutable->mBufferVariables.size() : 0;
}

GLint Program::getActiveUniformMaxLength() const
{
    ASSERT(!mLinkingState);
    size_t maxLength = 0;

    if (mLinked)
    {
        for (GLuint index = 0;
             index < static_cast<size_t>(mState.mExecutable->getUniformNames().size()); index++)
        {
            const std::string &uniformName = mState.mExecutable->getUniformNameByIndex(index);
            if (!uniformName.empty())
            {
                size_t length = uniformName.length() + 1u;
                if (mState.mExecutable->getUniformByIndex(index).isArray())
                {
                    length += 3;  // Counting in "[0]".
                }
                maxLength = std::max(length, maxLength);
            }
        }
    }

    return static_cast<GLint>(maxLength);
}

bool Program::isValidUniformLocation(UniformLocation location) const
{
    ASSERT(!mLinkingState);
    ASSERT(
        angle::IsValueInRangeForNumericType<GLint>(mState.mExecutable->mUniformLocations.size()));
    return (location.value >= 0 &&
            static_cast<size_t>(location.value) < mState.mExecutable->mUniformLocations.size() &&
            mState.mExecutable->mUniformLocations[static_cast<size_t>(location.value)].used());
}

const LinkedUniform &Program::getUniformByLocation(UniformLocation location) const
{
    ASSERT(!mLinkingState);
    ASSERT(location.value >= 0 &&
           static_cast<size_t>(location.value) < mState.mExecutable->mUniformLocations.size());
    return mState.mExecutable->getUniforms()[mState.getUniformIndexFromLocation(location)];
}

const VariableLocation &Program::getUniformLocation(UniformLocation location) const
{
    ASSERT(!mLinkingState);
    ASSERT(location.value >= 0 &&
           static_cast<size_t>(location.value) < mState.mExecutable->mUniformLocations.size());
    return mState.mExecutable->mUniformLocations[location.value];
}

const BufferVariable &Program::getBufferVariableByIndex(GLuint index) const
{
    ASSERT(!mLinkingState);
    ASSERT(index < static_cast<size_t>(mState.mExecutable->mBufferVariables.size()));
    return mState.mExecutable->mBufferVariables[index];
}

UniformLocation Program::getUniformLocation(const std::string &name) const
{
    ASSERT(!mLinkingState);
    return {GetUniformLocation(mState.mExecutable->getUniforms(),
                               mState.mExecutable->getUniformNames(),
                               mState.mExecutable->mUniformLocations, name)};
}

GLuint Program::getUniformIndex(const std::string &name) const
{
    ASSERT(!mLinkingState);
    return mState.getUniformIndexFromName(name);
}

bool Program::shouldIgnoreUniform(UniformLocation location) const
{
    if (location.value == -1)
    {
        return true;
    }

    if (mState.mExecutable->mUniformLocations[static_cast<size_t>(location.value)].ignored)
    {
        return true;
    }

    return false;
}

template <typename UniformT,
          GLint UniformSize,
          void (rx::ProgramImpl::*SetUniformFunc)(GLint, GLsizei, const UniformT *)>
void Program::setUniformGeneric(UniformLocation location, GLsizei count, const UniformT *v)
{
    ASSERT(!mLinkingState);
    if (shouldIgnoreUniform(location))
    {
        return;
    }

    const VariableLocation &locationInfo = mState.mExecutable->mUniformLocations[location.value];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, UniformSize, v);
    (mProgram->*SetUniformFunc)(location.value, clampedCount, v);
    onStateChange(angle::SubjectMessage::ProgramUniformUpdated);
}

void Program::setUniform1fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    setUniformGeneric<GLfloat, 1, &rx::ProgramImpl::setUniform1fv>(location, count, v);
}

void Program::setUniform2fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    setUniformGeneric<GLfloat, 2, &rx::ProgramImpl::setUniform2fv>(location, count, v);
}

void Program::setUniform3fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    setUniformGeneric<GLfloat, 3, &rx::ProgramImpl::setUniform3fv>(location, count, v);
}

void Program::setUniform4fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    setUniformGeneric<GLfloat, 4, &rx::ProgramImpl::setUniform4fv>(location, count, v);
}

void Program::setUniform1iv(Context *context,
                            UniformLocation location,
                            GLsizei count,
                            const GLint *v)
{
    ASSERT(!mLinkingState);
    if (shouldIgnoreUniform(location))
    {
        return;
    }

    const VariableLocation &locationInfo = mState.mExecutable->mUniformLocations[location.value];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 1, v);

    mProgram->setUniform1iv(location.value, clampedCount, v);

    if (mState.isSamplerUniformIndex(locationInfo.index))
    {
        updateSamplerUniform(context, locationInfo, clampedCount, v);
    }
    else
    {
        onStateChange(angle::SubjectMessage::ProgramUniformUpdated);
    }
}

void Program::setUniform2iv(UniformLocation location, GLsizei count, const GLint *v)
{
    setUniformGeneric<GLint, 2, &rx::ProgramImpl::setUniform2iv>(location, count, v);
}

void Program::setUniform3iv(UniformLocation location, GLsizei count, const GLint *v)
{
    setUniformGeneric<GLint, 3, &rx::ProgramImpl::setUniform3iv>(location, count, v);
}

void Program::setUniform4iv(UniformLocation location, GLsizei count, const GLint *v)
{
    setUniformGeneric<GLint, 4, &rx::ProgramImpl::setUniform4iv>(location, count, v);
}

void Program::setUniform1uiv(UniformLocation location, GLsizei count, const GLuint *v)
{
    setUniformGeneric<GLuint, 1, &rx::ProgramImpl::setUniform1uiv>(location, count, v);
}

void Program::setUniform2uiv(UniformLocation location, GLsizei count, const GLuint *v)
{
    setUniformGeneric<GLuint, 2, &rx::ProgramImpl::setUniform2uiv>(location, count, v);
}

void Program::setUniform3uiv(UniformLocation location, GLsizei count, const GLuint *v)
{
    setUniformGeneric<GLuint, 3, &rx::ProgramImpl::setUniform3uiv>(location, count, v);
}

void Program::setUniform4uiv(UniformLocation location, GLsizei count, const GLuint *v)
{
    setUniformGeneric<GLuint, 4, &rx::ProgramImpl::setUniform4uiv>(location, count, v);
}

template <
    typename UniformT,
    GLint MatrixC,
    GLint MatrixR,
    void (rx::ProgramImpl::*SetUniformMatrixFunc)(GLint, GLsizei, GLboolean, const UniformT *)>
void Program::setUniformMatrixGeneric(UniformLocation location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const UniformT *v)
{
    ASSERT(!mLinkingState);
    if (shouldIgnoreUniform(location))
    {
        return;
    }

    GLsizei clampedCount = clampMatrixUniformCount<MatrixC, MatrixR>(location, count, transpose, v);
    (mProgram->*SetUniformMatrixFunc)(location.value, clampedCount, transpose, v);
    onStateChange(angle::SubjectMessage::ProgramUniformUpdated);
}

void Program::setUniformMatrix2fv(UniformLocation location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const GLfloat *v)
{
    setUniformMatrixGeneric<GLfloat, 2, 2, &rx::ProgramImpl::setUniformMatrix2fv>(location, count,
                                                                                  transpose, v);
}

void Program::setUniformMatrix3fv(UniformLocation location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const GLfloat *v)
{
    setUniformMatrixGeneric<GLfloat, 3, 3, &rx::ProgramImpl::setUniformMatrix3fv>(location, count,
                                                                                  transpose, v);
}

void Program::setUniformMatrix4fv(UniformLocation location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const GLfloat *v)
{
    setUniformMatrixGeneric<GLfloat, 4, 4, &rx::ProgramImpl::setUniformMatrix4fv>(location, count,
                                                                                  transpose, v);
}

void Program::setUniformMatrix2x3fv(UniformLocation location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    setUniformMatrixGeneric<GLfloat, 2, 3, &rx::ProgramImpl::setUniformMatrix2x3fv>(location, count,
                                                                                    transpose, v);
}

void Program::setUniformMatrix2x4fv(UniformLocation location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    setUniformMatrixGeneric<GLfloat, 2, 4, &rx::ProgramImpl::setUniformMatrix2x4fv>(location, count,
                                                                                    transpose, v);
}

void Program::setUniformMatrix3x2fv(UniformLocation location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    setUniformMatrixGeneric<GLfloat, 3, 2, &rx::ProgramImpl::setUniformMatrix3x2fv>(location, count,
                                                                                    transpose, v);
}

void Program::setUniformMatrix3x4fv(UniformLocation location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    setUniformMatrixGeneric<GLfloat, 3, 4, &rx::ProgramImpl::setUniformMatrix3x4fv>(location, count,
                                                                                    transpose, v);
}

void Program::setUniformMatrix4x2fv(UniformLocation location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    setUniformMatrixGeneric<GLfloat, 4, 2, &rx::ProgramImpl::setUniformMatrix4x2fv>(location, count,
                                                                                    transpose, v);
}

void Program::setUniformMatrix4x3fv(UniformLocation location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    setUniformMatrixGeneric<GLfloat, 4, 3, &rx::ProgramImpl::setUniformMatrix4x3fv>(location, count,
                                                                                    transpose, v);
}

GLuint Program::getSamplerUniformBinding(const VariableLocation &uniformLocation) const
{
    ASSERT(!mLinkingState);
    GLuint samplerIndex = mState.getSamplerIndexFromUniformIndex(uniformLocation.index);
    const SamplerBinding &samplerBinding = mState.mExecutable->mSamplerBindings[samplerIndex];
    if (uniformLocation.arrayIndex >= samplerBinding.textureUnitsCount)
    {
        return 0;
    }

    const std::vector<GLuint> &boundTextureUnits = mState.mExecutable->mSamplerBoundTextureUnits;
    return samplerBinding.getTextureUnit(boundTextureUnits, uniformLocation.arrayIndex);
}

GLuint Program::getImageUniformBinding(const VariableLocation &uniformLocation) const
{
    ASSERT(!mLinkingState);
    GLuint imageIndex = mState.getImageIndexFromUniformIndex(uniformLocation.index);

    const std::vector<ImageBinding> &imageBindings = getExecutable().getImageBindings();
    const std::vector<GLuint> &boundImageUnits     = imageBindings[imageIndex].boundImageUnits;
    return boundImageUnits[uniformLocation.arrayIndex];
}

void Program::getUniformfv(const Context *context, UniformLocation location, GLfloat *v) const
{
    ASSERT(!mLinkingState);
    const VariableLocation &uniformLocation = mState.getUniformLocations()[location.value];
    const LinkedUniform &uniform            = mState.getUniforms()[uniformLocation.index];

    if (uniform.isSampler())
    {
        *v = static_cast<GLfloat>(getSamplerUniformBinding(uniformLocation));
        return;
    }
    else if (uniform.isImage())
    {
        *v = static_cast<GLfloat>(getImageUniformBinding(uniformLocation));
        return;
    }

    const GLenum nativeType = gl::VariableComponentType(uniform.getType());
    if (nativeType == GL_FLOAT)
    {
        mProgram->getUniformfv(context, location.value, v);
    }
    else
    {
        getUniformInternal(context, v, location, nativeType,
                           VariableComponentCount(uniform.getType()));
    }
}

void Program::getUniformiv(const Context *context, UniformLocation location, GLint *v) const
{
    ASSERT(!mLinkingState);
    const VariableLocation &uniformLocation = mState.getUniformLocations()[location.value];
    const LinkedUniform &uniform            = mState.getUniforms()[uniformLocation.index];

    if (uniform.isSampler())
    {
        *v = static_cast<GLint>(getSamplerUniformBinding(uniformLocation));
        return;
    }
    else if (uniform.isImage())
    {
        *v = static_cast<GLint>(getImageUniformBinding(uniformLocation));
        return;
    }

    const GLenum nativeType = gl::VariableComponentType(uniform.getType());
    if (nativeType == GL_INT || nativeType == GL_BOOL)
    {
        mProgram->getUniformiv(context, location.value, v);
    }
    else
    {
        getUniformInternal(context, v, location, nativeType,
                           VariableComponentCount(uniform.getType()));
    }
}

void Program::getUniformuiv(const Context *context, UniformLocation location, GLuint *v) const
{
    ASSERT(!mLinkingState);
    const VariableLocation &uniformLocation = mState.getUniformLocations()[location.value];
    const LinkedUniform &uniform            = mState.getUniforms()[uniformLocation.index];

    if (uniform.isSampler())
    {
        *v = getSamplerUniformBinding(uniformLocation);
        return;
    }
    else if (uniform.isImage())
    {
        *v = getImageUniformBinding(uniformLocation);
        return;
    }

    const GLenum nativeType = VariableComponentType(uniform.getType());
    if (nativeType == GL_UNSIGNED_INT)
    {
        mProgram->getUniformuiv(context, location.value, v);
    }
    else
    {
        getUniformInternal(context, v, location, nativeType,
                           VariableComponentCount(uniform.getType()));
    }
}

void Program::flagForDeletion()
{
    ASSERT(!mLinkingState);
    mDeleteStatus = true;
}

bool Program::isFlaggedForDeletion() const
{
    ASSERT(!mLinkingState);
    return mDeleteStatus;
}

void Program::validate(const Caps &caps)
{
    ASSERT(!mLinkingState);
    mState.mInfoLog.reset();

    if (mLinked)
    {
        mValidated = ConvertToBool(mProgram->validate(caps));
    }
    else
    {
        mState.mInfoLog << "Program has not been successfully linked.";
    }
}

bool Program::isValidated() const
{
    ASSERT(!mLinkingState);
    return mValidated;
}

void Program::getActiveUniformBlockName(const Context *context,
                                        const UniformBlockIndex blockIndex,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *blockName) const
{
    ASSERT(!mLinkingState);
    GetInterfaceBlockName(blockIndex, mState.mExecutable->getUniformBlocks(), bufSize, length,
                          blockName);
}

void Program::getActiveShaderStorageBlockName(const GLuint blockIndex,
                                              GLsizei bufSize,
                                              GLsizei *length,
                                              GLchar *blockName) const
{
    ASSERT(!mLinkingState);
    GetInterfaceBlockName({blockIndex}, mState.mExecutable->getShaderStorageBlocks(), bufSize,
                          length, blockName);
}

template <typename T>
GLint Program::getActiveInterfaceBlockMaxNameLength(const std::vector<T> &resources) const
{
    int maxLength = 0;

    if (mLinked)
    {
        for (const T &resource : resources)
        {
            if (!resource.name.empty())
            {
                int length = static_cast<int>(resource.nameWithArrayIndex().length());
                maxLength  = std::max(length + 1, maxLength);
            }
        }
    }

    return maxLength;
}

GLint Program::getActiveUniformBlockMaxNameLength() const
{
    ASSERT(!mLinkingState);
    return getActiveInterfaceBlockMaxNameLength(mState.mExecutable->getUniformBlocks());
}

GLint Program::getActiveShaderStorageBlockMaxNameLength() const
{
    ASSERT(!mLinkingState);
    return getActiveInterfaceBlockMaxNameLength(mState.mExecutable->getShaderStorageBlocks());
}

GLuint Program::getUniformBlockIndex(const std::string &name) const
{
    ASSERT(!mLinkingState);
    return GetInterfaceBlockIndex(mState.mExecutable->getUniformBlocks(), name);
}

GLuint Program::getShaderStorageBlockIndex(const std::string &name) const
{
    ASSERT(!mLinkingState);
    return GetInterfaceBlockIndex(mState.mExecutable->getShaderStorageBlocks(), name);
}

const InterfaceBlock &Program::getUniformBlockByIndex(GLuint index) const
{
    ASSERT(!mLinkingState);
    ASSERT(index < static_cast<GLuint>(mState.mExecutable->getActiveUniformBlockCount()));
    return mState.mExecutable->getUniformBlocks()[index];
}

const InterfaceBlock &Program::getShaderStorageBlockByIndex(GLuint index) const
{
    ASSERT(!mLinkingState);
    ASSERT(index < static_cast<GLuint>(mState.mExecutable->getActiveShaderStorageBlockCount()));
    return mState.mExecutable->getShaderStorageBlocks()[index];
}

void Program::bindUniformBlock(UniformBlockIndex uniformBlockIndex, GLuint uniformBlockBinding)
{
    ASSERT(!mLinkingState);

    if (mState.mExecutable->mPODStruct.activeUniformBlockBindings[uniformBlockIndex.value])
    {
        GLuint previousBinding =
            mState.mExecutable->mUniformBlocks[uniformBlockIndex.value].binding;
        if (previousBinding >= mUniformBlockBindingMasks.size())
        {
            mUniformBlockBindingMasks.resize(previousBinding + 1, UniformBlockBindingMask());
        }
        mUniformBlockBindingMasks[previousBinding].reset(uniformBlockIndex.value);
    }

    mState.mExecutable->mUniformBlocks[uniformBlockIndex.value].binding = uniformBlockBinding;
    if (uniformBlockBinding >= mUniformBlockBindingMasks.size())
    {
        mUniformBlockBindingMasks.resize(uniformBlockBinding + 1, UniformBlockBindingMask());
    }
    mUniformBlockBindingMasks[uniformBlockBinding].set(uniformBlockIndex.value);
    mState.mExecutable->mPODStruct.activeUniformBlockBindings.set(uniformBlockIndex.value,
                                                                  uniformBlockBinding != 0);

    mDirtyBits.set(DIRTY_BIT_UNIFORM_BLOCK_BINDING_0 + uniformBlockIndex.value);
}

GLuint Program::getUniformBlockBinding(GLuint uniformBlockIndex) const
{
    ASSERT(!mLinkingState);
    return mState.getUniformBlockBinding(uniformBlockIndex);
}

GLuint Program::getShaderStorageBlockBinding(GLuint shaderStorageBlockIndex) const
{
    ASSERT(!mLinkingState);
    return mState.getShaderStorageBlockBinding(shaderStorageBlockIndex);
}

void Program::setTransformFeedbackVaryings(GLsizei count,
                                           const GLchar *const *varyings,
                                           GLenum bufferMode)
{
    ASSERT(!mLinkingState);
    mState.mTransformFeedbackVaryingNames.resize(count);
    for (GLsizei i = 0; i < count; i++)
    {
        mState.mTransformFeedbackVaryingNames[i] = varyings[i];
    }

    mState.mTransformFeedbackBufferMode = bufferMode;
}

void Program::getTransformFeedbackVarying(GLuint index,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLsizei *size,
                                          GLenum *type,
                                          GLchar *name) const
{
    ASSERT(!mLinkingState);
    if (mLinked)
    {
        ASSERT(index < mState.mExecutable->mLinkedTransformFeedbackVaryings.size());
        const auto &var     = mState.mExecutable->mLinkedTransformFeedbackVaryings[index];
        std::string varName = var.nameWithArrayIndex();
        GLsizei lastNameIdx = std::min(bufSize - 1, static_cast<GLsizei>(varName.length()));
        if (length)
        {
            *length = lastNameIdx;
        }
        if (size)
        {
            *size = var.size();
        }
        if (type)
        {
            *type = var.type;
        }
        if (name)
        {
            memcpy(name, varName.c_str(), lastNameIdx);
            name[lastNameIdx] = '\0';
        }
    }
}

GLsizei Program::getTransformFeedbackVaryingCount() const
{
    ASSERT(!mLinkingState);
    if (mLinked)
    {
        return static_cast<GLsizei>(mState.mExecutable->mLinkedTransformFeedbackVaryings.size());
    }
    else
    {
        return 0;
    }
}

GLsizei Program::getTransformFeedbackVaryingMaxLength() const
{
    ASSERT(!mLinkingState);
    if (mLinked)
    {
        GLsizei maxSize = 0;
        for (const auto &var : mState.mExecutable->mLinkedTransformFeedbackVaryings)
        {
            maxSize =
                std::max(maxSize, static_cast<GLsizei>(var.nameWithArrayIndex().length() + 1));
        }

        return maxSize;
    }
    else
    {
        return 0;
    }
}

bool Program::linkValidateShaders(const Context *context)
{
    const ShaderMap<SharedCompiledShaderState> &shaders = mState.mAttachedShaders;

    bool isComputeShaderAttached  = shaders[ShaderType::Compute].get() != nullptr;
    bool isGraphicsShaderAttached = shaders[ShaderType::Vertex].get() != nullptr ||
                                    shaders[ShaderType::TessControl].get() != nullptr ||
                                    shaders[ShaderType::TessEvaluation].get() != nullptr ||
                                    shaders[ShaderType::Geometry].get() != nullptr ||
                                    shaders[ShaderType::Fragment].get() != nullptr;
    // Check whether we both have a compute and non-compute shaders attached.
    // If there are of both types attached, then linking should fail.
    // OpenGL ES 3.10, 7.3 Program Objects, under LinkProgram
    if (isComputeShaderAttached && isGraphicsShaderAttached)
    {
        mState.mInfoLog << "Both compute and graphics shaders are attached to the same program.";
        return false;
    }

    Optional<int> version;
    for (ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        Shader *shaderObj = getAttachedShader(shaderType);
        ASSERT(!shaderObj || shaderObj->getType() == shaderType);

        const SharedCompiledShaderState &shader = shaders[shaderType];
        ASSERT(!shader || shader->shaderType == shaderType);

        if (!shaderObj)
        {
            ASSERT(!shader);
            continue;
        }

        if (!shaderObj->isCompiled(context))
        {
            mState.mInfoLog << ShaderTypeToString(shaderType) << " shader is not compiled.";
            return false;
        }

        if (!version.valid())
        {
            version = shader->shaderVersion;
        }
        else if (version != shader->shaderVersion)
        {
            mState.mInfoLog << ShaderTypeToString(shaderType)
                            << " shader version does not match other shader versions.";
            return false;
        }
    }

    if (isComputeShaderAttached)
    {
        ASSERT(shaders[ShaderType::Compute]->shaderType == ShaderType::Compute);

        // GLSL ES 3.10, 4.4.1.1 Compute Shader Inputs
        // If the work group size is not specified, a link time error should occur.
        if (!shaders[ShaderType::Compute]->localSize.isDeclared())
        {
            mState.mInfoLog << "Work group size is not specified.";
            return false;
        }
    }
    else
    {
        if (!isGraphicsShaderAttached)
        {
            mState.mInfoLog << "No compiled shaders.";
            return false;
        }

        bool hasVertex   = shaders[ShaderType::Vertex].get() != nullptr;
        bool hasFragment = shaders[ShaderType::Fragment].get() != nullptr;
        if (!isSeparable() && (!hasVertex || !hasFragment))
        {
            mState.mInfoLog
                << "The program must contain objects to form both a vertex and fragment shader.";
            return false;
        }

        bool hasTessControl    = shaders[ShaderType::TessControl].get() != nullptr;
        bool hasTessEvaluation = shaders[ShaderType::TessEvaluation].get() != nullptr;
        if (!isSeparable() && (hasTessControl != hasTessEvaluation))
        {
            mState.mInfoLog
                << "Tessellation control and evaluation shaders must be specified together.";
            return false;
        }

        const SharedCompiledShaderState &geometryShader = shaders[ShaderType::Geometry];
        if (geometryShader)
        {
            // [GL_EXT_geometry_shader] Chapter 7
            // Linking can fail for a variety of reasons as specified in the OpenGL ES Shading
            // Language Specification, as well as any of the following reasons:
            // * One or more of the shader objects attached to <program> are not compiled
            //   successfully.
            // * The shaders do not use the same shader language version.
            // * <program> contains objects to form a geometry shader, and
            //   - <program> is not separable and contains no objects to form a vertex shader; or
            //   - the input primitive type, output primitive type, or maximum output vertex count
            //     is not specified in the compiled geometry shader object.
            Optional<PrimitiveMode> inputPrimitive =
                geometryShader->geometryShaderInputPrimitiveType;
            if (!inputPrimitive.valid())
            {
                mState.mInfoLog << "Input primitive type is not specified in the geometry shader.";
                return false;
            }

            Optional<PrimitiveMode> outputPrimitive =
                geometryShader->geometryShaderOutputPrimitiveType;
            if (!outputPrimitive.valid())
            {
                mState.mInfoLog << "Output primitive type is not specified in the geometry shader.";
                return false;
            }

            Optional<GLint> maxVertices = geometryShader->geometryShaderMaxVertices;
            if (!maxVertices.valid())
            {
                mState.mInfoLog << "'max_vertices' is not specified in the geometry shader.";
                return false;
            }
        }

        const SharedCompiledShaderState &tessControlShader = shaders[ShaderType::TessControl];
        if (tessControlShader)
        {
            int tcsShaderVertices = tessControlShader->tessControlShaderVertices;
            if (tcsShaderVertices == 0)
            {
                // In tessellation control shader, output vertices should be specified at least
                // once.
                // > GLSL ES Version 3.20.6 spec:
                // > 4.4.2. Output Layout Qualifiers
                // > Tessellation Control Outputs
                // > ...
                // > There must be at least one layout qualifier specifying an output patch vertex
                // > count in any program containing a tessellation control shader.
                mState.mInfoLog << "In Tessellation Control Shader, at least one layout qualifier "
                                   "specifying an output patch vertex count must exist.";
                return false;
            }
        }

        const SharedCompiledShaderState &tessEvaluationShader = shaders[ShaderType::TessEvaluation];
        if (tessEvaluationShader)
        {
            GLenum tesPrimitiveMode = tessEvaluationShader->tessGenMode;
            if (tesPrimitiveMode == 0)
            {
                // In tessellation evaluation shader, a primitive mode should be specified at least
                // once.
                // > GLSL ES Version 3.20.6 spec:
                // > 4.4.1. Input Layout Qualifiers
                // > Tessellation Evaluation Inputs
                // > ...
                // > The tessellation evaluation shader object in a program must declare a primitive
                // > mode in its input layout. Declaring vertex spacing, ordering, or point mode
                // > identifiers is optional.
                mState.mInfoLog
                    << "The Tessellation Evaluation Shader object in a program must declare a "
                       "primitive mode in its input layout.";
                return false;
            }
        }
    }

    return true;
}

// Assumes linkValidateShaders() has validated the shaders and caches some values from the shaders.
void Program::linkShaders()
{
    const ShaderMap<SharedCompiledShaderState> &shaders = mState.mAttachedShaders;

    const bool isComputeShaderAttached = shaders[ShaderType::Compute].get() != nullptr;

    if (isComputeShaderAttached)
    {
        mState.mExecutable->mPODStruct.computeShaderLocalSize =
            shaders[ShaderType::Compute]->localSize;
    }
    else
    {
        const SharedCompiledShaderState &geometryShader = shaders[ShaderType::Geometry];
        if (geometryShader)
        {
            Optional<PrimitiveMode> inputPrimitive =
                geometryShader->geometryShaderInputPrimitiveType;
            Optional<PrimitiveMode> outputPrimitive =
                geometryShader->geometryShaderOutputPrimitiveType;
            Optional<GLint> maxVertices = geometryShader->geometryShaderMaxVertices;

            mState.mExecutable->mPODStruct.geometryShaderInputPrimitiveType =
                inputPrimitive.value();
            mState.mExecutable->mPODStruct.geometryShaderOutputPrimitiveType =
                outputPrimitive.value();
            mState.mExecutable->mPODStruct.geometryShaderMaxVertices = maxVertices.value();
            mState.mExecutable->mPODStruct.geometryShaderInvocations =
                geometryShader->geometryShaderInvocations;
        }

        const SharedCompiledShaderState &tessControlShader = shaders[ShaderType::TessControl];
        if (tessControlShader)
        {
            int tcsShaderVertices = tessControlShader->tessControlShaderVertices;
            mState.mExecutable->mPODStruct.tessControlShaderVertices = tcsShaderVertices;
        }

        const SharedCompiledShaderState &tessEvaluationShader = shaders[ShaderType::TessEvaluation];
        if (tessEvaluationShader)
        {
            GLenum tesPrimitiveMode = tessEvaluationShader->tessGenMode;

            mState.mExecutable->mPODStruct.tessGenMode    = tesPrimitiveMode;
            mState.mExecutable->mPODStruct.tessGenSpacing = tessEvaluationShader->tessGenSpacing;
            mState.mExecutable->mPODStruct.tessGenVertexOrder =
                tessEvaluationShader->tessGenVertexOrder;
            mState.mExecutable->mPODStruct.tessGenPointMode =
                tessEvaluationShader->tessGenPointMode;
        }
    }
}

GLuint Program::getTransformFeedbackVaryingResourceIndex(const GLchar *name) const
{
    ASSERT(!mLinkingState);
    for (GLuint tfIndex = 0; tfIndex < mState.mExecutable->mLinkedTransformFeedbackVaryings.size();
         ++tfIndex)
    {
        const auto &tf = mState.mExecutable->mLinkedTransformFeedbackVaryings[tfIndex];
        if (tf.nameWithArrayIndex() == name)
        {
            return tfIndex;
        }
    }
    return GL_INVALID_INDEX;
}

const TransformFeedbackVarying &Program::getTransformFeedbackVaryingResource(GLuint index) const
{
    ASSERT(!mLinkingState);
    ASSERT(index < mState.mExecutable->mLinkedTransformFeedbackVaryings.size());
    return mState.mExecutable->mLinkedTransformFeedbackVaryings[index];
}

bool Program::hasDrawIDUniform() const
{
    ASSERT(!mLinkingState);
    return mState.getDrawIDLocation() >= 0;
}

void Program::setDrawIDUniform(GLint drawid)
{
    ASSERT(!mLinkingState);
    ASSERT(mState.getDrawIDLocation() >= 0);
    mProgram->setUniform1iv(mState.getDrawIDLocation(), 1, &drawid);
}

bool Program::hasBaseVertexUniform() const
{
    ASSERT(!mLinkingState);
    return mState.getBaseVertexLocation() >= 0;
}

void Program::setBaseVertexUniform(GLint baseVertex)
{
    ASSERT(!mLinkingState);
    ASSERT(mState.getBaseVertexLocation() >= 0);
    if (baseVertex == mState.mCachedBaseVertex)
    {
        return;
    }
    mState.mCachedBaseVertex = baseVertex;
    mProgram->setUniform1iv(mState.getBaseVertexLocation(), 1, &baseVertex);
}

bool Program::hasBaseInstanceUniform() const
{
    ASSERT(!mLinkingState);
    return mState.getBaseInstanceLocation() >= 0;
}

void Program::setBaseInstanceUniform(GLuint baseInstance)
{
    ASSERT(!mLinkingState);
    ASSERT(mState.getBaseInstanceLocation() >= 0);
    if (baseInstance == mState.mCachedBaseInstance)
    {
        return;
    }
    mState.mCachedBaseInstance = baseInstance;
    GLint baseInstanceInt      = baseInstance;
    mProgram->setUniform1iv(mState.getBaseInstanceLocation(), 1, &baseInstanceInt);
}

bool Program::linkVaryings()
{
    ShaderType previousShaderType = ShaderType::InvalidEnum;
    for (ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        const SharedCompiledShaderState &currentShader = mState.mAttachedShaders[shaderType];
        if (!currentShader)
        {
            continue;
        }

        if (previousShaderType != ShaderType::InvalidEnum)
        {
            const SharedCompiledShaderState &previousShader =
                mState.mAttachedShaders[previousShaderType];
            const std::vector<sh::ShaderVariable> &outputVaryings = previousShader->outputVaryings;

            if (!LinkValidateShaderInterfaceMatching(
                    outputVaryings, currentShader->inputVaryings, previousShaderType,
                    currentShader->shaderType, previousShader->shaderVersion,
                    currentShader->shaderVersion, isSeparable(), mState.mInfoLog))
            {
                return false;
            }
        }
        previousShaderType = currentShader->shaderType;
    }

    // TODO: http://anglebug.com/3571 and http://anglebug.com/3572
    // Need to move logic of validating builtin varyings inside the for-loop above.
    // This is because the built-in symbols `gl_ClipDistance` and `gl_CullDistance`
    // can be redeclared in Geometry or Tessellation shaders as well.
    const SharedCompiledShaderState &vertexShader   = mState.mAttachedShaders[ShaderType::Vertex];
    const SharedCompiledShaderState &fragmentShader = mState.mAttachedShaders[ShaderType::Fragment];
    if (vertexShader && fragmentShader &&
        !LinkValidateBuiltInVaryings(vertexShader->outputVaryings, fragmentShader->inputVaryings,
                                     vertexShader->shaderType, fragmentShader->shaderType,
                                     vertexShader->shaderVersion, fragmentShader->shaderVersion,
                                     mState.mInfoLog))
    {
        return false;
    }

    return true;
}

bool Program::linkUniforms(const Caps &caps,
                           const Version &clientVersion,
                           std::vector<UnusedUniform> *unusedUniformsOutOrNull,
                           GLuint *combinedImageUniformsOut)
{
    // Initialize executable shader map.
    ShaderMap<std::vector<sh::ShaderVariable>> shaderUniforms;
    for (const SharedCompiledShaderState &shader : mState.mAttachedShaders)
    {
        if (shader)
        {
            shaderUniforms[shader->shaderType] = shader->uniforms;
        }
    }

    if (!mState.mExecutable->linkUniforms(caps, shaderUniforms, mState.mUniformLocationBindings,
                                          combinedImageUniformsOut, unusedUniformsOutOrNull))
    {
        return false;
    }

    if (clientVersion >= Version(3, 1))
    {
        GLint locationSize = static_cast<GLint>(mState.getUniformLocations().size());

        if (locationSize > caps.maxUniformLocations)
        {
            mState.mInfoLog << "Exceeded maximum uniform location size";
            return false;
        }
    }

    return true;
}

// Assigns locations to all attributes (except built-ins) from the bindings and program locations.
bool Program::linkAttributes(const Caps &caps,
                             const Limitations &limitations,
                             bool webglCompatibility)
{
    int shaderVersion          = -1;
    unsigned int usedLocations = 0;

    const SharedCompiledShaderState &vertexShader =
        mState.getAttachedShader(gl::ShaderType::Vertex);

    if (!vertexShader)
    {
        // No vertex shader, so no attributes, so nothing to do
        return true;
    }

    // In GLSL ES 3.00.6, aliasing checks should be done with all declared attributes -
    // see GLSL ES 3.00.6 section 12.46. Inactive attributes will be pruned after
    // aliasing checks.
    // In GLSL ES 1.00.17 we only do aliasing checks for active attributes.
    shaderVersion = vertexShader->shaderVersion;
    const std::vector<sh::ShaderVariable> &shaderAttributes =
        shaderVersion >= 300 ? vertexShader->allAttributes : vertexShader->activeAttributes;

    ASSERT(mState.mExecutable->mProgramInputs.empty());
    mState.mExecutable->mProgramInputs.reserve(shaderAttributes.size());

    GLuint maxAttribs = static_cast<GLuint>(caps.maxVertexAttributes);
    std::vector<ProgramInput *> usedAttribMap(maxAttribs, nullptr);

    for (const sh::ShaderVariable &shaderAttribute : shaderAttributes)
    {
        // GLSL ES 3.10 January 2016 section 4.3.4: Vertex shader inputs can't be arrays or
        // structures, so we don't need to worry about adjusting their names or generating entries
        // for each member/element (unlike uniforms for example).
        ASSERT(!shaderAttribute.isArray() && !shaderAttribute.isStruct());

        mState.mExecutable->mProgramInputs.emplace_back(shaderAttribute);

        // Assign locations to attributes that have a binding location and check for attribute
        // aliasing.
        ProgramInput &attribute = mState.mExecutable->mProgramInputs.back();
        int bindingLocation     = mState.mAttributeBindings.getBinding(attribute);
        if (attribute.getLocation() == -1 && bindingLocation != -1)
        {
            attribute.setLocation(bindingLocation);
        }

        if (attribute.getLocation() != -1)
        {
            // Location is set by glBindAttribLocation or by location layout qualifier
            const int regs = VariableRegisterCount(attribute.getType());

            if (static_cast<GLuint>(regs + attribute.getLocation()) > maxAttribs)
            {
                mState.mInfoLog << "Attribute (" << attribute.name << ") at location "
                                << attribute.getLocation() << " is too big to fit";

                return false;
            }

            for (int reg = 0; reg < regs; reg++)
            {
                const int regLocation         = attribute.getLocation() + reg;
                ProgramInput *linkedAttribute = usedAttribMap[regLocation];

                // In GLSL ES 3.00.6 and in WebGL, attribute aliasing produces a link error.
                // In non-WebGL GLSL ES 1.00.17, attribute aliasing is allowed with some
                // restrictions - see GLSL ES 1.00.17 section 2.10.4, but ANGLE currently has a bug.
                // In D3D 9 and 11, aliasing is not supported, so check a limitation.
                if (linkedAttribute)
                {
                    if (shaderVersion >= 300 || webglCompatibility ||
                        limitations.noVertexAttributeAliasing)
                    {
                        mState.mInfoLog << "Attribute '" << attribute.name
                                        << "' aliases attribute '" << linkedAttribute->name
                                        << "' at location " << regLocation;
                        return false;
                    }
                }
                else
                {
                    usedAttribMap[regLocation] = &attribute;
                }

                usedLocations |= 1 << regLocation;
            }
        }
    }

    // Assign locations to attributes that don't have a binding location.
    for (ProgramInput &attribute : mState.mExecutable->mProgramInputs)
    {
        // Not set by glBindAttribLocation or by location layout qualifier
        if (attribute.getLocation() == -1)
        {
            int regs           = VariableRegisterCount(attribute.getType());
            int availableIndex = AllocateFirstFreeBits(&usedLocations, regs, maxAttribs);

            if (availableIndex == -1 || static_cast<GLuint>(availableIndex + regs) > maxAttribs)
            {
                mState.mInfoLog << "Too many attributes (" << attribute.name << ")";
                return false;
            }

            attribute.setLocation(availableIndex);
        }
    }

    ASSERT(mState.mExecutable->mPODStruct.attributesTypeMask.none());
    ASSERT(mState.mExecutable->mPODStruct.attributesMask.none());

    // Prune inactive attributes. This step is only needed on shaderVersion >= 300 since on earlier
    // shader versions we're only processing active attributes to begin with.
    if (shaderVersion >= 300)
    {
        for (auto attributeIter = mState.mExecutable->getProgramInputs().begin();
             attributeIter != mState.mExecutable->getProgramInputs().end();)
        {
            if (attributeIter->isActive())
            {
                ++attributeIter;
            }
            else
            {
                attributeIter = mState.mExecutable->mProgramInputs.erase(attributeIter);
            }
        }
    }

    for (const ProgramInput &attribute : mState.mExecutable->getProgramInputs())
    {
        ASSERT(attribute.isActive());
        ASSERT(attribute.getLocation() != -1);
        unsigned int regs = static_cast<unsigned int>(VariableRegisterCount(attribute.getType()));

        unsigned int location = static_cast<unsigned int>(attribute.getLocation());
        for (unsigned int r = 0; r < regs; r++)
        {
            // Built-in active program inputs don't have a bound attribute.
            if (!attribute.isBuiltIn())
            {
                mState.mExecutable->mPODStruct.activeAttribLocationsMask.set(location);
                mState.mExecutable->mPODStruct.maxActiveAttribLocation =
                    std::max(mState.mExecutable->mPODStruct.maxActiveAttribLocation, location + 1);

                ComponentType componentType =
                    GLenumToComponentType(VariableComponentType(attribute.getType()));

                SetComponentTypeMask(componentType, location,
                                     &mState.mExecutable->mPODStruct.attributesTypeMask);
                mState.mExecutable->mPODStruct.attributesMask.set(location);

                location++;
            }
        }
    }

    return true;
}

void Program::setUniformValuesFromBindingQualifiers()
{
    for (unsigned int samplerIndex : mState.mExecutable->getSamplerUniformRange())
    {
        const auto &samplerUniform = mState.mExecutable->getUniforms()[samplerIndex];
        if (samplerUniform.getBinding() != -1)
        {
            const std::string &uniformName =
                mState.mExecutable->getUniformNameByIndex(samplerIndex);
            UniformLocation location = getUniformLocation(uniformName);
            ASSERT(location.value != -1);
            std::vector<GLint> boundTextureUnits;
            for (unsigned int elementIndex = 0;
                 elementIndex < samplerUniform.getBasicTypeElementCount(); ++elementIndex)
            {
                boundTextureUnits.push_back(samplerUniform.getBinding() + elementIndex);
            }

            // Here we pass nullptr to avoid a large chain of calls that need a non-const Context.
            // We know it's safe not to notify the Context because this is only called after link.
            setUniform1iv(nullptr, location, static_cast<GLsizei>(boundTextureUnits.size()),
                          boundTextureUnits.data());
        }
    }
}

void Program::initInterfaceBlockBindings()
{
    // Set initial bindings from shader.
    for (unsigned int blockIndex = 0; blockIndex < mState.mExecutable->getActiveUniformBlockCount();
         blockIndex++)
    {
        InterfaceBlock &uniformBlock = mState.mExecutable->mUniformBlocks[blockIndex];
        bindUniformBlock({blockIndex}, uniformBlock.binding);
    }
}

void Program::updateSamplerUniform(Context *context,
                                   const VariableLocation &locationInfo,
                                   GLsizei clampedCount,
                                   const GLint *v)
{
    ASSERT(mState.isSamplerUniformIndex(locationInfo.index));
    GLuint samplerIndex            = mState.getSamplerIndexFromUniformIndex(locationInfo.index);
    SamplerBinding &samplerBinding = mState.mExecutable->mSamplerBindings[samplerIndex];
    std::vector<GLuint> &boundTextureUnits = mState.mExecutable->mSamplerBoundTextureUnits;

    if (locationInfo.arrayIndex >= samplerBinding.textureUnitsCount)
    {
        return;
    }
    GLsizei safeUniformCount =
        std::min(clampedCount,
                 static_cast<GLsizei>(samplerBinding.textureUnitsCount - locationInfo.arrayIndex));

    // Update the sampler uniforms.
    for (uint16_t arrayIndex = 0; arrayIndex < safeUniformCount; ++arrayIndex)
    {
        GLint oldTextureUnit =
            samplerBinding.getTextureUnit(boundTextureUnits, arrayIndex + locationInfo.arrayIndex);
        GLint newTextureUnit = v[arrayIndex];

        if (oldTextureUnit == newTextureUnit)
        {
            continue;
        }

        // Update sampler's bound textureUnit
        boundTextureUnits[samplerBinding.textureUnitsStartIndex + arrayIndex +
                          locationInfo.arrayIndex] = newTextureUnit;

        // Update the reference counts.
        uint32_t &oldRefCount = mState.mExecutable->mActiveSamplerRefCounts[oldTextureUnit];
        uint32_t &newRefCount = mState.mExecutable->mActiveSamplerRefCounts[newTextureUnit];
        ASSERT(oldRefCount > 0);
        ASSERT(newRefCount < std::numeric_limits<uint32_t>::max());
        oldRefCount--;
        newRefCount++;

        // Check for binding type change.
        TextureType newSamplerType     = mState.mExecutable->mActiveSamplerTypes[newTextureUnit];
        TextureType oldSamplerType     = mState.mExecutable->mActiveSamplerTypes[oldTextureUnit];
        SamplerFormat newSamplerFormat = mState.mExecutable->mActiveSamplerFormats[newTextureUnit];
        SamplerFormat oldSamplerFormat = mState.mExecutable->mActiveSamplerFormats[oldTextureUnit];
        bool newSamplerYUV             = mState.mExecutable->mActiveSamplerYUV.test(newTextureUnit);

        if (newRefCount == 1)
        {
            mState.mExecutable->setActive(newTextureUnit, samplerBinding,
                                          mState.mExecutable->getUniforms()[locationInfo.index]);
        }
        else
        {
            if (newSamplerType != samplerBinding.textureType ||
                newSamplerYUV != IsSamplerYUVType(samplerBinding.samplerType))
            {
                mState.mExecutable->hasSamplerTypeConflict(newTextureUnit);
            }

            if (newSamplerFormat != samplerBinding.format)
            {
                mState.mExecutable->hasSamplerFormatConflict(newTextureUnit);
            }
        }

        // Unset previously active sampler.
        if (oldRefCount == 0)
        {
            mState.mExecutable->setInactive(oldTextureUnit);
        }
        else
        {
            if (oldSamplerType == TextureType::InvalidEnum ||
                oldSamplerFormat == SamplerFormat::InvalidEnum)
            {
                // Previous conflict. Check if this new change fixed the conflict.
                mState.setSamplerUniformTextureTypeAndFormat(oldTextureUnit);
            }
        }

        // Update the observing PPO's executable, if any.
        // Do this before any of the Context work, since that uses the current ProgramExecutable,
        // which will be the PPO's if this Program is bound to it, rather than this Program's.
        if (isSeparable())
        {
            onStateChange(angle::SubjectMessage::ProgramTextureOrImageBindingChanged);
        }

        // Notify context.
        if (context)
        {
            context->onSamplerUniformChange(newTextureUnit);
            context->onSamplerUniformChange(oldTextureUnit);
        }
    }

    // Invalidate the validation cache.
    getExecutable().resetCachedValidateSamplersResult();
    // Inform any PPOs this Program may be bound to.
    onStateChange(angle::SubjectMessage::SamplerUniformsUpdated);
}

void ProgramState::setSamplerUniformTextureTypeAndFormat(size_t textureUnitIndex)
{
    mExecutable->setSamplerUniformTextureTypeAndFormat(
        textureUnitIndex, mExecutable->mSamplerBindings, mExecutable->mSamplerBoundTextureUnits);
}

template <typename T>
GLsizei Program::clampUniformCount(const VariableLocation &locationInfo,
                                   GLsizei count,
                                   int vectorSize,
                                   const T *v)
{
    if (count == 1)
        return 1;

    const LinkedUniform &linkedUniform = mState.mExecutable->getUniforms()[locationInfo.index];

    // OpenGL ES 3.0.4 spec pg 67: "Values for any array element that exceeds the highest array
    // element index used, as reported by GetActiveUniform, will be ignored by the GL."
    unsigned int remainingElements =
        linkedUniform.getBasicTypeElementCount() - locationInfo.arrayIndex;
    GLsizei maxElementCount =
        static_cast<GLsizei>(remainingElements * linkedUniform.getElementComponents());

    if (count * vectorSize > maxElementCount)
    {
        return maxElementCount / vectorSize;
    }

    return count;
}

template <size_t cols, size_t rows, typename T>
GLsizei Program::clampMatrixUniformCount(UniformLocation location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const T *v)
{
    const VariableLocation &locationInfo = mState.mExecutable->mUniformLocations[location.value];

    if (!transpose)
    {
        return clampUniformCount(locationInfo, count, cols * rows, v);
    }

    const LinkedUniform &linkedUniform = mState.mExecutable->getUniforms()[locationInfo.index];

    // OpenGL ES 3.0.4 spec pg 67: "Values for any array element that exceeds the highest array
    // element index used, as reported by GetActiveUniform, will be ignored by the GL."
    unsigned int remainingElements =
        linkedUniform.getBasicTypeElementCount() - locationInfo.arrayIndex;
    return std::min(count, static_cast<GLsizei>(remainingElements));
}

// Driver differences mean that doing the uniform value cast ourselves gives consistent results.
// EG: on NVIDIA drivers, it was observed that getUniformi for MAX_INT+1 returned MIN_INT.
template <typename DestT>
void Program::getUniformInternal(const Context *context,
                                 DestT *dataOut,
                                 UniformLocation location,
                                 GLenum nativeType,
                                 int components) const
{
    switch (nativeType)
    {
        case GL_BOOL:
        {
            GLint tempValue[16] = {0};
            mProgram->getUniformiv(context, location.value, tempValue);
            UniformStateQueryCastLoop<GLboolean>(
                dataOut, reinterpret_cast<const uint8_t *>(tempValue), components);
            break;
        }
        case GL_INT:
        {
            GLint tempValue[16] = {0};
            mProgram->getUniformiv(context, location.value, tempValue);
            UniformStateQueryCastLoop<GLint>(dataOut, reinterpret_cast<const uint8_t *>(tempValue),
                                             components);
            break;
        }
        case GL_UNSIGNED_INT:
        {
            GLuint tempValue[16] = {0};
            mProgram->getUniformuiv(context, location.value, tempValue);
            UniformStateQueryCastLoop<GLuint>(dataOut, reinterpret_cast<const uint8_t *>(tempValue),
                                              components);
            break;
        }
        case GL_FLOAT:
        {
            GLfloat tempValue[16] = {0};
            mProgram->getUniformfv(context, location.value, tempValue);
            UniformStateQueryCastLoop<GLfloat>(
                dataOut, reinterpret_cast<const uint8_t *>(tempValue), components);
            break;
        }
        default:
            UNREACHABLE();
            break;
    }
}

angle::Result Program::syncState(const Context *context)
{
    if (mDirtyBits.any())
    {
        ASSERT(!mLinkingState);
        ANGLE_TRY(mProgram->syncState(context, mDirtyBits));
        mDirtyBits.reset();
    }

    return angle::Result::Continue;
}

angle::Result Program::serialize(const Context *context, angle::MemoryBuffer *binaryOut) const
{
    BinaryOutputStream stream;

    stream.writeBytes(
        reinterpret_cast<const unsigned char *>(angle::GetANGLEShaderProgramVersion()),
        angle::GetANGLEShaderProgramVersionHashSize());

    stream.writeBool(angle::Is64Bit());

    stream.writeInt(angle::GetANGLESHVersion());

    stream.writeString(context->getRendererString());

    // nullptr context is supported when computing binary length.
    if (context)
    {
        stream.writeInt(context->getClientVersion().major);
        stream.writeInt(context->getClientVersion().minor);
    }
    else
    {
        stream.writeInt(2);
        stream.writeInt(0);
    }

    // mSeparable must be before mExecutable->save(), since it uses the value.
    stream.writeBool(mState.mSeparable);
    stream.writeInt(mState.mTransformFeedbackBufferMode);

    mState.mExecutable->save(mState.mSeparable, &stream);

    // Warn the app layer if saving a binary with unsupported transform feedback.
    if (!mState.getLinkedTransformFeedbackVaryings().empty() &&
        context->getFrontendFeatures().disableProgramCachingForTransformFeedback.enabled)
    {
        ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "Saving program binary with transform feedback, which is not supported "
                           "on this driver.");
    }

    if (context->getShareGroup()->getFrameCaptureShared()->enabled())
    {
        // Serialize the source for each stage for re-use during capture
        for (ShaderType shaderType : mState.mExecutable->getLinkedShaderStages())
        {
            gl::Shader *shader = getAttachedShader(shaderType);
            if (shader)
            {
                stream.writeString(shader->getSourceString());
            }
            else
            {
                // If we don't have an attached shader, which would occur if this program was
                // created via glProgramBinary, pull from our cached copy
                const angle::ProgramSources &cachedLinkedSources =
                    context->getShareGroup()->getFrameCaptureShared()->getProgramSources(id());
                const std::string &cachedSourceString = cachedLinkedSources[shaderType];
                ASSERT(!cachedSourceString.empty());
                stream.writeString(cachedSourceString.c_str());
            }
        }
    }

    mProgram->save(context, &stream);

    ASSERT(binaryOut);
    if (!binaryOut->resize(stream.length()))
    {
        std::stringstream sstream;
        sstream << "Failed to allocate enough memory to serialize a program. (" << stream.length()
                << " bytes )";
        ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                           sstream.str().c_str());
        return angle::Result::Incomplete;
    }
    memcpy(binaryOut->data(), stream.data(), stream.length());
    return angle::Result::Continue;
}

angle::Result Program::deserialize(const Context *context, BinaryInputStream &stream)
{
    std::vector<uint8_t> angleShaderProgramVersionString(
        angle::GetANGLEShaderProgramVersionHashSize(), 0);
    stream.readBytes(angleShaderProgramVersionString.data(),
                     angleShaderProgramVersionString.size());
    if (memcmp(angleShaderProgramVersionString.data(), angle::GetANGLEShaderProgramVersion(),
               angleShaderProgramVersionString.size()) != 0)
    {
        mState.mInfoLog << "Invalid program binary version.";
        return angle::Result::Stop;
    }

    bool binaryIs64Bit = stream.readBool();
    if (binaryIs64Bit != angle::Is64Bit())
    {
        mState.mInfoLog << "cannot load program binaries across CPU architectures.";
        return angle::Result::Stop;
    }

    int angleSHVersion = stream.readInt<int>();
    if (angleSHVersion != angle::GetANGLESHVersion())
    {
        mState.mInfoLog << "cannot load program binaries across different angle sh version.";
        return angle::Result::Stop;
    }

    std::string rendererString = stream.readString();
    if (rendererString != context->getRendererString())
    {
        mState.mInfoLog << "Cannot load program binary due to changed renderer string.";
        return angle::Result::Stop;
    }

    int majorVersion = stream.readInt<int>();
    int minorVersion = stream.readInt<int>();
    if (majorVersion != context->getClientMajorVersion() ||
        minorVersion != context->getClientMinorVersion())
    {
        mState.mInfoLog << "Cannot load program binaries across different ES context versions.";
        return angle::Result::Stop;
    }

    // mSeparable must be before mExecutable->load(), since it uses the value.
    mState.mSeparable                   = stream.readBool();
    mState.mTransformFeedbackBufferMode = stream.readInt<GLenum>();

    mState.mExecutable->load(mState.mSeparable, &stream);

    static_assert(static_cast<unsigned long>(ShaderType::EnumCount) <= sizeof(unsigned long) * 8,
                  "Too many shader types");

    // Reject programs that use transform feedback varyings if the hardware cannot support them.
    if (mState.mExecutable->getLinkedTransformFeedbackVaryings().size() > 0 &&
        context->getFrontendFeatures().disableProgramCachingForTransformFeedback.enabled)
    {
        mState.mInfoLog << "Current driver does not support transform feedback in binary programs.";
        return angle::Result::Stop;
    }

    if (!mState.mAttachedShaders[ShaderType::Compute])
    {
        mState.mExecutable->updateTransformFeedbackStrides();
    }

    if (context->getShareGroup()->getFrameCaptureShared()->enabled())
    {
        // Extract the source for each stage from the program binary
        angle::ProgramSources sources;

        for (ShaderType shaderType : mState.mExecutable->getLinkedShaderStages())
        {
            std::string shaderSource = stream.readString();
            ASSERT(shaderSource.length() > 0);
            sources[shaderType] = std::move(shaderSource);
        }

        // Store it for use during mid-execution capture
        context->getShareGroup()->getFrameCaptureShared()->setProgramSources(id(),
                                                                             std::move(sources));
    }

    return angle::Result::Continue;
}

void Program::postResolveLink(const gl::Context *context)
{
    initInterfaceBlockBindings();

    mState.updateActiveSamplers();
    mState.mExecutable->mActiveImageShaderBits.fill({});
    mState.mExecutable->updateActiveImages(getExecutable());

    setUniformValuesFromBindingQualifiers();

    if (context->getExtensions().multiDrawANGLE)
    {
        mState.mExecutable->mPODStruct.drawIDLocation = getUniformLocation("gl_DrawID").value;
    }

    if (context->getExtensions().baseVertexBaseInstanceShaderBuiltinANGLE)
    {
        mState.mExecutable->mPODStruct.baseVertexLocation =
            getUniformLocation("gl_BaseVertex").value;
        mState.mExecutable->mPODStruct.baseInstanceLocation =
            getUniformLocation("gl_BaseInstance").value;
    }
}

void Program::dumpProgramInfo(const Context *context) const
{
    std::stringstream dumpStream;
    for (ShaderType shaderType : angle::AllEnums<ShaderType>())
    {
        gl::Shader *shader = getAttachedShader(shaderType);
        if (shader)
        {
            dumpStream << shader->getType() << ": "
                       << GetShaderDumpFileName(shader->getSourceHash()) << std::endl;
        }
    }

    std::string dump = dumpStream.str();
    size_t dumpHash  = std::hash<std::string>{}(dump);

    std::stringstream pathStream;
    std::string shaderDumpDir = GetShaderDumpFileDirectory();
    if (!shaderDumpDir.empty())
    {
        pathStream << shaderDumpDir << "/";
    }
    pathStream << dumpHash << ".program";
    std::string path = pathStream.str();

    writeFile(path.c_str(), dump.c_str(), dump.length());
    INFO() << "Dumped program: " << path;
}

}  // namespace gl
