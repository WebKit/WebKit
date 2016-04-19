//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VaryingPacking:
//   Class which describes a mapping from varyings to registers in D3D
//   for linking between shader stages.
//

#ifndef LIBANGLE_RENDERER_D3D_VARYINGPACKING_H_
#define LIBANGLE_RENDERER_D3D_VARYINGPACKING_H_

#include "libANGLE/renderer/d3d/RendererD3D.h"

namespace rx
{
class ProgramD3DMetadata;

struct PackedVarying
{
    PackedVarying(const sh::ShaderVariable &varyingIn, sh::InterpolationType interpolationIn)
        : varying(&varyingIn), vertexOnly(false), interpolation(interpolationIn)
    {
    }
    PackedVarying(const sh::ShaderVariable &varyingIn,
                  sh::InterpolationType interpolationIn,
                  const std::string &parentStructNameIn)
        : varying(&varyingIn),
          vertexOnly(false),
          interpolation(interpolationIn),
          parentStructName(parentStructNameIn)
    {
    }

    bool isStructField() const { return !parentStructName.empty(); }

    const sh::ShaderVariable *varying;

    // Transform feedback varyings can be only referenced in the VS.
    bool vertexOnly;

    // Cached so we can store sh::ShaderVariable to point to varying fields.
    sh::InterpolationType interpolation;

    // Struct name
    std::string parentStructName;
};

struct PackedVaryingRegister final
{
    PackedVaryingRegister()
        : packedVarying(nullptr),
          varyingArrayIndex(0),
          varyingRowIndex(0),
          registerRow(0),
          registerColumn(0)
    {
    }

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

    bool isStructField() const { return !structFieldName.empty(); }

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

    // Assigned after packing
    unsigned int semanticIndex;

    // Struct member this varying corresponds to.
    std::string structFieldName;
};

class VaryingPacking final : angle::NonCopyable
{
  public:
    VaryingPacking(GLuint maxVaryingVectors);

    bool packVaryings(gl::InfoLog &infoLog,
                      const std::vector<PackedVarying> &packedVaryings,
                      const std::vector<std::string> &transformFeedbackVaryings);

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
    unsigned int getRegisterCount() const;

    void enableBuiltins(ShaderType shaderType, const ProgramD3DMetadata &programMetadata);

    struct BuiltinVarying final : angle::NonCopyable
    {
        BuiltinVarying();

        std::string str() const;
        void enableSystem(const std::string &systemValueSemantic);
        void enable(const std::string &semanticVal, unsigned int indexVal);

        bool enabled;
        std::string semantic;
        unsigned int index;
        bool systemValue;
    };

    struct BuiltinInfo
    {
        BuiltinVarying dxPosition;
        BuiltinVarying glPosition;
        BuiltinVarying glFragCoord;
        BuiltinVarying glPointCoord;
        BuiltinVarying glPointSize;
    };

    const BuiltinInfo &builtins(ShaderType shaderType) const { return mBuiltinInfo[shaderType]; }

    bool usesPointSize() const { return mBuiltinInfo[SHADER_VERTEX].glPointSize.enabled; }

  private:
    bool packVarying(const PackedVarying &packedVarying);
    bool isFree(unsigned int registerRow,
                unsigned int registerColumn,
                unsigned int varyingRows,
                unsigned int varyingColumns) const;
    void insert(unsigned int registerRow,
                unsigned int registerColumn,
                const PackedVarying &packedVarying);

    std::vector<Register> mRegisterMap;
    std::vector<PackedVaryingRegister> mRegisterList;

    std::vector<BuiltinInfo> mBuiltinInfo;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_VARYINGPACKING_H_
