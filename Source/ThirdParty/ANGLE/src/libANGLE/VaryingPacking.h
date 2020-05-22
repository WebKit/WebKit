//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VaryingPacking:
//   Class which describes a mapping from varyings to registers, according
//   to the spec, or using custom packing algorithms. We also keep a register
//   allocation list for the D3D renderer.
//

#ifndef LIBANGLE_VARYINGPACKING_H_
#define LIBANGLE_VARYINGPACKING_H_

#include <GLSLANG/ShaderVars.h>

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/angletypes.h"

#include <map>

namespace gl
{
class InfoLog;
struct ProgramVaryingRef;

using ProgramMergedVaryings = std::vector<ProgramVaryingRef>;

// A varying can have different names between stages if matched by the location layout qualifier.
// Additionally, same name varyings could still be of two identical struct types with different
// names.  This struct contains information on the varying in one of the two stages.  PackedVarying
// will thus contain two copies of this along with common information, such as interpolation or
// field index.
struct VaryingInShaderRef : angle::NonCopyable
{
    VaryingInShaderRef(ShaderType stageIn, const sh::ShaderVariable *varyingIn);
    VaryingInShaderRef(VaryingInShaderRef &&other);
    ~VaryingInShaderRef();

    VaryingInShaderRef &operator=(VaryingInShaderRef &&other);

    const sh::ShaderVariable *varying;

    ShaderType stage;

    // Struct name
    std::string parentStructName;
    std::string parentStructMappedName;
};

struct PackedVarying : angle::NonCopyable
{
    // Throughout this file, the "front" stage refers to the stage that outputs the varying, and the
    // "back" stage refers to the stage that takes the varying as input.  Note that this struct
    // contains linked varyings, which means both front and back stage varyings are valid, except
    // for the following which may have only one valid stage.
    //
    //  - transform-feedback-captured varyings
    //  - builtins
    //  - separable program stages,
    //
    PackedVarying(VaryingInShaderRef &&frontVaryingIn,
                  VaryingInShaderRef &&backVaryingIn,
                  sh::InterpolationType interpolationIn);
    PackedVarying(VaryingInShaderRef &&frontVaryingIn,
                  VaryingInShaderRef &&backVaryingIn,
                  sh::InterpolationType interpolationIn,
                  GLuint fieldIndexIn);
    PackedVarying(PackedVarying &&other);
    ~PackedVarying();

    PackedVarying &operator=(PackedVarying &&other);

    bool isStructField() const
    {
        return frontVarying.varying ? !frontVarying.parentStructName.empty()
                                    : !backVarying.parentStructName.empty();
    }

    bool isArrayElement() const { return arrayIndex != GL_INVALID_INDEX; }

    // Return either front or back varying, whichever is available.  Only used when the name of the
    // varying is not important, but only the type is interesting.
    const sh::ShaderVariable &varying() const
    {
        return frontVarying.varying ? *frontVarying.varying : *backVarying.varying;
    }

    std::string fullName(ShaderType stage) const
    {
        ASSERT(stage == frontVarying.stage || stage == backVarying.stage);
        const VaryingInShaderRef &varying =
            stage == frontVarying.stage ? frontVarying : backVarying;

        std::stringstream fullNameStr;
        if (isStructField())
        {
            fullNameStr << varying.parentStructName << ".";
        }

        fullNameStr << varying.varying->name;
        if (arrayIndex != GL_INVALID_INDEX)
        {
            fullNameStr << "[" << arrayIndex << "]";
        }
        return fullNameStr.str();
    }

    // Transform feedback varyings can be only referenced in the VS.
    bool vertexOnly() const
    {
        return frontVarying.stage == ShaderType::Vertex && backVarying.varying == nullptr;
    }

    VaryingInShaderRef frontVarying;
    VaryingInShaderRef backVarying;

    // Cached so we can store sh::ShaderVariable to point to varying fields.
    sh::InterpolationType interpolation;

    GLuint arrayIndex;

    // Field index in the struct.  In Vulkan, this is used to assign a
    // struct-typed varying location to the location of its first field.
    GLuint fieldIndex;
};

struct PackedVaryingRegister final
{
    PackedVaryingRegister()
        : packedVarying(nullptr),
          varyingArrayIndex(0),
          varyingRowIndex(0),
          registerRow(0),
          registerColumn(0)
    {}

    PackedVaryingRegister(const PackedVaryingRegister &) = default;
    PackedVaryingRegister &operator=(const PackedVaryingRegister &) = default;

    bool operator<(const PackedVaryingRegister &other) const
    {
        return sortOrder() < other.sortOrder();
    }

    unsigned int sortOrder() const
    {
        // TODO(jmadill): Handle interpolation types
        return registerRow * 4 + registerColumn;
    }

    std::string tfVaryingName() const
    {
        return packedVarying->fullName(packedVarying->frontVarying.stage);
    }

    // Index to the array of varyings.
    const PackedVarying *packedVarying;

    // The array element of the packed varying.
    unsigned int varyingArrayIndex;

    // The row of the array element of the packed varying.
    unsigned int varyingRowIndex;

    // The register row to which we've assigned this packed varying.
    unsigned int registerRow;

    // The column of the register row into which we've packed this varying.
    unsigned int registerColumn;
};

// Supported packing modes:
enum class PackMode
{
    // We treat mat2 arrays as taking two full rows.
    WEBGL_STRICT,

    // We allow mat2 to take a 2x2 chunk.
    ANGLE_RELAXED,

    // Each varying takes a separate register. No register sharing.
    ANGLE_NON_CONFORMANT_D3D9,
};

class VaryingPacking final : angle::NonCopyable
{
  public:
    VaryingPacking(GLuint maxVaryingVectors, PackMode packMode);
    ~VaryingPacking();

    bool packUserVaryings(gl::InfoLog &infoLog, const std::vector<PackedVarying> &packedVaryings);

    bool collectAndPackUserVaryings(gl::InfoLog &infoLog,
                                    const ProgramMergedVaryings &mergedVaryings,
                                    const std::vector<std::string> &tfVaryings,
                                    const bool isSeparableProgram);

    struct Register
    {
        Register() { data[0] = data[1] = data[2] = data[3] = false; }

        bool &operator[](unsigned int index) { return data[index]; }
        bool operator[](unsigned int index) const { return data[index]; }

        bool data[4];
    };

    Register &operator[](unsigned int index) { return mRegisterMap[index]; }
    const Register &operator[](unsigned int index) const { return mRegisterMap[index]; }

    const std::vector<PackedVaryingRegister> &getRegisterList() const { return mRegisterList; }
    unsigned int getMaxSemanticIndex() const
    {
        return static_cast<unsigned int>(mRegisterList.size());
    }

    const ShaderMap<std::vector<std::string>> &getInactiveVaryingMappedNames() const
    {
        return mInactiveVaryingMappedNames;
    }

    void reset();

  private:
    bool packVarying(const PackedVarying &packedVarying);
    bool isFree(unsigned int registerRow,
                unsigned int registerColumn,
                unsigned int varyingRows,
                unsigned int varyingColumns) const;
    void insert(unsigned int registerRow,
                unsigned int registerColumn,
                const PackedVarying &packedVarying);

    using VaryingUniqueFullNames = ShaderMap<std::set<std::string>>;
    void packUserVarying(const ProgramVaryingRef &ref, VaryingUniqueFullNames *uniqueFullNames);
    void packUserVaryingField(const ProgramVaryingRef &ref,
                              GLuint fieldIndex,
                              VaryingUniqueFullNames *uniqueFullNames);
    void packUserVaryingTF(const ProgramVaryingRef &ref, size_t subscript);
    void packUserVaryingFieldTF(const ProgramVaryingRef &ref,
                                const sh::ShaderVariable &field,
                                GLuint fieldIndex);

    void clearRegisterMap();

    std::vector<Register> mRegisterMap;
    std::vector<PackedVaryingRegister> mRegisterList;
    std::vector<PackedVarying> mPackedVaryings;
    ShaderMap<std::vector<std::string>> mInactiveVaryingMappedNames;

    PackMode mPackMode;
};

}  // namespace gl

#endif  // LIBANGLE_VARYINGPACKING_H_
