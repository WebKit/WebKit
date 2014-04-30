//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// shadervars.h:
//  Types to represent GL variables (varyings, uniforms, etc)
//

#ifndef COMMON_SHADERVARIABLE_H_
#define COMMON_SHADERVARIABLE_H_

#include <string>
#include <vector>
#include <algorithm>

#define GL_APICALL
#include <GLES3/gl3.h>
#include <GLES2/gl2.h>

namespace gl
{

enum InterpolationType
{
    INTERPOLATION_SMOOTH,
    INTERPOLATION_CENTROID,
    INTERPOLATION_FLAT
};

struct ShaderVariable
{
    GLenum type;
    GLenum precision;
    std::string name;
    unsigned int arraySize;

    ShaderVariable(GLenum type, GLenum precision, const char *name, unsigned int arraySize);
    bool isArray() const { return arraySize > 0; }
    unsigned int elementCount() const { return std::max(1u, arraySize); }
};

struct Uniform : public ShaderVariable
{
    unsigned long registerIndex;
    unsigned long elementIndex;     // For struct varyings
    std::vector<Uniform> fields;

    Uniform(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn,
            unsigned int registerIndexIn, unsigned int elementIndexIn);

    bool isStruct() const { return !fields.empty(); }
};

struct Attribute : public ShaderVariable
{
    int location;

    Attribute();
    Attribute(GLenum type, GLenum precision, const char *name, unsigned int arraySize, int location);
};

struct InterfaceBlockField : public ShaderVariable
{
    bool isRowMajorMatrix;
    std::vector<InterfaceBlockField> fields;

    InterfaceBlockField(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn, bool isRowMajorMatrix);

    bool isStruct() const { return !fields.empty(); }
};

struct Varying : public ShaderVariable
{
    InterpolationType interpolation;
    std::vector<Varying> fields;
    unsigned int registerIndex;    // Assigned during link
    unsigned int elementIndex;     // First register element for varyings, assigned during link
    std::string structName;

    Varying(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn, InterpolationType interpolationIn);

    bool isStruct() const { return !fields.empty(); }
    bool registerAssigned() const { return registerIndex != GL_INVALID_INDEX; }

    void resetRegisterAssignment();
};

struct BlockMemberInfo
{
    BlockMemberInfo(long offset, long arrayStride, long matrixStride, bool isRowMajorMatrix);

    long offset;
    long arrayStride;
    long matrixStride;
    bool isRowMajorMatrix;

    static const BlockMemberInfo defaultBlockInfo;
};

typedef std::vector<BlockMemberInfo> BlockMemberInfoArray;

enum BlockLayoutType
{
    BLOCKLAYOUT_STANDARD,
    BLOCKLAYOUT_PACKED,
    BLOCKLAYOUT_SHARED
};

struct InterfaceBlock
{
    InterfaceBlock(const char *name, unsigned int arraySize, unsigned int registerIndex);

    std::string name;
    unsigned int arraySize;
    size_t dataSize;
    BlockLayoutType layout;
    bool isRowMajorLayout;
    std::vector<InterfaceBlockField> fields;
    std::vector<BlockMemberInfo> blockInfo;

    unsigned int registerIndex;
};

}

#endif // COMMON_SHADERVARIABLE_H_
