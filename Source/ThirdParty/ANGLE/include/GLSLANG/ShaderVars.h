//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderVars.h:
//  Types to represent GL variables (varyings, uniforms, etc)
//

#ifndef GLSLANG_SHADERVARS_H_
#define GLSLANG_SHADERVARS_H_

#include <algorithm>
#include <array>
#include <string>
#include <vector>

// This type is defined here to simplify ANGLE's integration with glslang for SPIRv.
using ShCompileOptions = uint64_t;

namespace sh
{
// GLenum alias
typedef unsigned int GLenum;

// Varying interpolation qualifier, see section 4.3.9 of the ESSL 3.00.4 spec
enum InterpolationType
{
    INTERPOLATION_SMOOTH,
    INTERPOLATION_CENTROID,
    INTERPOLATION_FLAT
};

// Validate link & SSO consistency of interpolation qualifiers
bool InterpolationTypesMatch(InterpolationType a, InterpolationType b);

// Uniform block layout qualifier, see section 4.3.8.3 of the ESSL 3.00.4 spec
enum BlockLayoutType
{
    BLOCKLAYOUT_STANDARD,
    BLOCKLAYOUT_STD140 = BLOCKLAYOUT_STANDARD,
    BLOCKLAYOUT_STD430,  // Shader storage block layout qualifier
    BLOCKLAYOUT_PACKED,
    BLOCKLAYOUT_SHARED
};

// Interface Blocks, see section 4.3.9 of the ESSL 3.10 spec
enum class BlockType
{
    BLOCK_UNIFORM,
    BLOCK_BUFFER,

    // Required in OpenGL ES 3.1 extension GL_OES_shader_io_blocks.
    // TODO(jiawei.shao@intel.com): add BLOCK_OUT.
    BLOCK_IN
};

// Base class for all variables defined in shaders, including Varyings, Uniforms, etc
// Note: we must override the copy constructor and assignment operator so we can
// work around excessive GCC binary bloating:
// See https://code.google.com/p/angleproject/issues/detail?id=697
struct ShaderVariable
{
    ShaderVariable();
    ShaderVariable(GLenum typeIn);
    ShaderVariable(GLenum typeIn, unsigned int arraySizeIn);
    ~ShaderVariable();
    ShaderVariable(const ShaderVariable &other);
    ShaderVariable &operator=(const ShaderVariable &other);

    bool isArrayOfArrays() const { return arraySizes.size() >= 2u; }
    bool isArray() const { return !arraySizes.empty(); }
    unsigned int getArraySizeProduct() const;
    // Return the inner array size product.
    // For example, if there's a variable declared as size 3 array of size 4 array of size 5 array
    // of int:
    //   int a[3][4][5];
    // then getInnerArraySizeProduct of a would be 4*5.
    unsigned int getInnerArraySizeProduct() const;

    // Array size 0 means not an array when passed to or returned from these functions.
    // Note that setArraySize() is deprecated and should not be used inside ANGLE.
    unsigned int getOutermostArraySize() const { return isArray() ? arraySizes.back() : 0; }
    void setArraySize(unsigned int size);

    // Turn this ShaderVariable from an array into a specific element in that array. Will update
    // flattenedOffsetInParentArrays.
    void indexIntoArray(unsigned int arrayIndex);

    // Get the nth nested array size from the top. Caller is responsible for range checking
    // arrayNestingIndex.
    unsigned int getNestedArraySize(unsigned int arrayNestingIndex) const;

    // This function should only be used with variables that are of a basic type or an array of a
    // basic type. Shader interface variables that are enumerated according to rules in GLES 3.1
    // spec section 7.3.1.1 page 77 are fine. For those variables the return value should match the
    // ARRAY_SIZE value that can be queried through the API.
    unsigned int getBasicTypeElementCount() const;

    bool isStruct() const { return !fields.empty(); }

    // All of the shader's variables are described using nested data
    // structures. This is needed in order to disambiguate similar looking
    // types, such as two structs containing the same fields, but in
    // different orders. "findInfoByMappedName" provides an easy query for
    // users to dive into the data structure and fetch the unique variable
    // instance corresponding to a dereferencing chain of the top-level
    // variable.
    // Given a mapped name like 'a[0].b.c[0]', return the ShaderVariable
    // that defines 'c' in |leafVar|, and the original name 'A[0].B.C[0]'
    // in |originalName|, based on the assumption that |this| defines 'a'.
    // If no match is found, return false.
    bool findInfoByMappedName(const std::string &mappedFullName,
                              const ShaderVariable **leafVar,
                              std::string *originalFullName) const;

    bool isBuiltIn() const;
    bool isEmulatedBuiltIn() const;

    GLenum type;
    GLenum precision;
    std::string name;
    std::string mappedName;

    // Used to make an array type. Outermost array size is stored at the end of the vector.
    std::vector<unsigned int> arraySizes;

    // Offset of this variable in parent arrays. In case the parent is an array of arrays, the
    // offset is outerArrayElement * innerArraySize + innerArrayElement.
    // For example, if there's a variable declared as size 3 array of size 4 array of int:
    //   int a[3][4];
    // then the flattenedOffsetInParentArrays of a[2] would be 2.
    // and flattenedOffsetInParentArrays of a[2][1] would be 2*4 + 1 = 9.
    int parentArrayIndex() const
    {
        return hasParentArrayIndex() ? flattenedOffsetInParentArrays : 0;
    }

    void setParentArrayIndex(int index) { flattenedOffsetInParentArrays = index; }

    bool hasParentArrayIndex() const { return flattenedOffsetInParentArrays != -1; }

    // Static use means that the variable is accessed somewhere in the shader source.
    bool staticUse;
    // A variable is active unless the compiler determined that it is not accessed by the shader.
    // All active variables are statically used, but not all statically used variables are
    // necessarily active. GLES 3.0.5 section 2.12.6. GLES 3.1 section 7.3.1.
    bool active;
    std::vector<ShaderVariable> fields;
    std::string structName;

    // Only applies to interface block fields. Kept here for simplicity.
    bool isRowMajorLayout;

  protected:
    bool isSameVariableAtLinkTime(const ShaderVariable &other,
                                  bool matchPrecision,
                                  bool matchName) const;

    bool operator==(const ShaderVariable &other) const;
    bool operator!=(const ShaderVariable &other) const { return !operator==(other); }

    int flattenedOffsetInParentArrays;
};

// A variable with an integer location to pass back to the GL API: either uniform (can have location
// in GLES3.1+), vertex shader input or fragment shader output.
struct VariableWithLocation : public ShaderVariable
{
    VariableWithLocation();
    ~VariableWithLocation();
    VariableWithLocation(const VariableWithLocation &other);
    VariableWithLocation &operator=(const VariableWithLocation &other);
    bool operator==(const VariableWithLocation &other) const;
    bool operator!=(const VariableWithLocation &other) const { return !operator==(other); }

    int location;
};

struct Uniform : public VariableWithLocation
{
    Uniform();
    ~Uniform();
    Uniform(const Uniform &other);
    Uniform &operator=(const Uniform &other);
    bool operator==(const Uniform &other) const;
    bool operator!=(const Uniform &other) const { return !operator==(other); }

    int binding;
    GLenum imageUnitFormat;
    int offset;
    bool readonly;
    bool writeonly;

    // Decide whether two uniforms are the same at shader link time,
    // assuming one from vertex shader and the other from fragment shader.
    // GLSL ES Spec 3.00.3, section 4.3.5.
    // GLSL ES Spec 3.10.4, section 4.4.5
    bool isSameUniformAtLinkTime(const Uniform &other) const;
};

struct Attribute : public VariableWithLocation
{
    Attribute();
    ~Attribute();
    Attribute(const Attribute &other);
    Attribute &operator=(const Attribute &other);
    bool operator==(const Attribute &other) const;
    bool operator!=(const Attribute &other) const { return !operator==(other); }
};

struct OutputVariable : public VariableWithLocation
{
    OutputVariable();
    ~OutputVariable();
    OutputVariable(const OutputVariable &other);
    OutputVariable &operator=(const OutputVariable &other);
    bool operator==(const OutputVariable &other) const;
    bool operator!=(const OutputVariable &other) const { return !operator==(other); }

    // From EXT_blend_func_extended.
    int index;
};

struct InterfaceBlockField : public ShaderVariable
{
    InterfaceBlockField();
    ~InterfaceBlockField();
    InterfaceBlockField(const InterfaceBlockField &other);
    InterfaceBlockField &operator=(const InterfaceBlockField &other);
    bool operator==(const InterfaceBlockField &other) const;
    bool operator!=(const InterfaceBlockField &other) const { return !operator==(other); }

    // Decide whether two InterfaceBlock fields are the same at shader
    // link time, assuming one from vertex shader and the other from
    // fragment shader.
    // See GLSL ES Spec 3.00.3, sec 4.3.7.
    bool isSameInterfaceBlockFieldAtLinkTime(const InterfaceBlockField &other) const;
};

struct Varying : public VariableWithLocation
{
    Varying();
    ~Varying();
    Varying(const Varying &other);
    Varying &operator=(const Varying &other);
    bool operator==(const Varying &other) const;
    bool operator!=(const Varying &other) const { return !operator==(other); }

    // Decide whether two varyings are the same at shader link time,
    // assuming one from vertex shader and the other from fragment shader.
    // Invariance needs to match only in ESSL1. Relevant spec sections:
    // GLSL ES 3.00.4, sections 4.6.1 and 4.3.9.
    // GLSL ES 1.00.17, section 4.6.4.
    bool isSameVaryingAtLinkTime(const Varying &other, int shaderVersion) const;

    // Deprecated version of isSameVaryingAtLinkTime, which assumes ESSL1.
    bool isSameVaryingAtLinkTime(const Varying &other) const;

    InterpolationType interpolation;
    bool isInvariant;
};

struct InterfaceBlock
{
    InterfaceBlock();
    ~InterfaceBlock();
    InterfaceBlock(const InterfaceBlock &other);
    InterfaceBlock &operator=(const InterfaceBlock &other);

    // Fields from blocks with non-empty instance names are prefixed with the block name.
    std::string fieldPrefix() const;
    std::string fieldMappedPrefix() const;

    // Decide whether two interface blocks are the same at shader link time.
    bool isSameInterfaceBlockAtLinkTime(const InterfaceBlock &other) const;

    bool isBuiltIn() const;

    bool isArray() const { return arraySize > 0; }
    unsigned int elementCount() const { return std::max(1u, arraySize); }

    std::string name;
    std::string mappedName;
    std::string instanceName;
    unsigned int arraySize;
    BlockLayoutType layout;

    // Deprecated. Matrix packing should only be queried from individual fields of the block.
    // TODO(oetuaho): Remove this once it is no longer used in Chromium.
    bool isRowMajorLayout;

    int binding;
    bool staticUse;
    bool active;
    BlockType blockType;
    std::vector<InterfaceBlockField> fields;
};

struct WorkGroupSize
{
    // Must have a trivial default constructor since it is used in YYSTYPE.
    inline WorkGroupSize() = default;
    inline explicit constexpr WorkGroupSize(int initialSize);

    void fill(int fillValue);
    void setLocalSize(int localSizeX, int localSizeY, int localSizeZ);

    int &operator[](size_t index);
    int operator[](size_t index) const;
    size_t size() const;

    // Checks whether two work group size declarations match.
    // Two work group size declarations are the same if the explicitly specified elements are the
    // same or if one of them is specified as one and the other one is not specified
    bool isWorkGroupSizeMatching(const WorkGroupSize &right) const;

    // Checks whether any of the values are set.
    bool isAnyValueSet() const;

    // Checks whether all of the values are set.
    bool isDeclared() const;

    // Checks whether either all of the values are set, or none of them are.
    bool isLocalSizeValid() const;

    int localSizeQualifiers[3];
};

inline constexpr WorkGroupSize::WorkGroupSize(int initialSize)
    : localSizeQualifiers{initialSize, initialSize, initialSize}
{}

}  // namespace sh

#endif  // GLSLANG_SHADERVARS_H_
