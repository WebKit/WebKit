/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Uniform block case.
 *//*--------------------------------------------------------------------*/

#include "glsUniformBlockCase.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"
#include "gluDrawUtil.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuRenderTarget.hpp"
#include "deInt32.h"
#include "deMemory.h"
#include "deRandom.hpp"
#include "deString.h"
#include "deStringUtil.hpp"

#include <algorithm>
#include <map>

using std::map;
using std::string;
using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace gls
{
namespace ub
{

static bool isSupportedGLSLVersion(glu::GLSLVersion version)
{
    return version >= (glslVersionIsES(version) ? glu::GLSL_VERSION_300_ES : glu::GLSL_VERSION_330);
}

struct PrecisionFlagsFmt
{
    uint32_t flags;
    PrecisionFlagsFmt(uint32_t flags_) : flags(flags_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const PrecisionFlagsFmt &fmt)
{
    // Precision.
    DE_ASSERT(dePop32(fmt.flags & (PRECISION_LOW | PRECISION_MEDIUM | PRECISION_HIGH)) <= 1);
    str << (fmt.flags & PRECISION_LOW    ? "lowp" :
            fmt.flags & PRECISION_MEDIUM ? "mediump" :
            fmt.flags & PRECISION_HIGH   ? "highp" :
                                           "");
    return str;
}

struct LayoutFlagsFmt
{
    uint32_t flags;
    LayoutFlagsFmt(uint32_t flags_) : flags(flags_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const LayoutFlagsFmt &fmt)
{
    static const struct
    {
        uint32_t bit;
        const char *token;
    } bitDesc[] = {{LAYOUT_SHARED, "shared"},
                   {LAYOUT_PACKED, "packed"},
                   {LAYOUT_STD140, "std140"},
                   {LAYOUT_ROW_MAJOR, "row_major"},
                   {LAYOUT_COLUMN_MAJOR, "column_major"}};

    uint32_t remBits = fmt.flags;
    for (int descNdx = 0; descNdx < DE_LENGTH_OF_ARRAY(bitDesc); descNdx++)
    {
        if (remBits & bitDesc[descNdx].bit)
        {
            if (remBits != fmt.flags)
                str << ", ";
            str << bitDesc[descNdx].token;
            remBits &= ~bitDesc[descNdx].bit;
        }
    }
    DE_ASSERT(remBits == 0);
    return str;
}

// VarType implementation.

VarType::VarType(void) : m_type(TYPE_LAST), m_flags(0)
{
}

VarType::VarType(const VarType &other) : m_type(TYPE_LAST), m_flags(0)
{
    *this = other;
}

VarType::VarType(glu::DataType basicType, uint32_t flags) : m_type(TYPE_BASIC), m_flags(flags)
{
    m_data.basicType = basicType;
}

VarType::VarType(const VarType &elementType, int arraySize) : m_type(TYPE_ARRAY), m_flags(0)
{
    m_data.array.size        = arraySize;
    m_data.array.elementType = new VarType(elementType);
}

VarType::VarType(const StructType *structPtr, uint32_t flags) : m_type(TYPE_STRUCT), m_flags(flags)
{
    m_data.structPtr = structPtr;
}

VarType::~VarType(void)
{
    if (m_type == TYPE_ARRAY)
        delete m_data.array.elementType;
}

VarType &VarType::operator=(const VarType &other)
{
    if (this == &other)
        return *this; // Self-assignment.

    if (m_type == TYPE_ARRAY)
        delete m_data.array.elementType;

    m_type  = other.m_type;
    m_flags = other.m_flags;
    m_data  = Data();

    if (m_type == TYPE_ARRAY)
    {
        m_data.array.elementType = new VarType(*other.m_data.array.elementType);
        m_data.array.size        = other.m_data.array.size;
    }
    else
        m_data = other.m_data;

    return *this;
}

// StructType implementation.

void StructType::addMember(const char *name, const VarType &type, uint32_t flags)
{
    m_members.push_back(StructMember(name, type, flags));
}

// Uniform implementation.

Uniform::Uniform(const char *name, const VarType &type, uint32_t flags) : m_name(name), m_type(type), m_flags(flags)
{
}

// UniformBlock implementation.

UniformBlock::UniformBlock(const char *blockName) : m_blockName(blockName), m_arraySize(0), m_flags(0)
{
}

struct BlockLayoutEntry
{
    BlockLayoutEntry(void) : size(0)
    {
    }

    std::string name;
    int size;
    std::vector<int> activeUniformIndices;
};

std::ostream &operator<<(std::ostream &stream, const BlockLayoutEntry &entry)
{
    stream << entry.name << " { name = " << entry.name << ", size = " << entry.size << ", activeUniformIndices = [";

    for (vector<int>::const_iterator i = entry.activeUniformIndices.begin(); i != entry.activeUniformIndices.end(); i++)
    {
        if (i != entry.activeUniformIndices.begin())
            stream << ", ";
        stream << *i;
    }

    stream << "] }";
    return stream;
}

struct UniformLayoutEntry
{
    UniformLayoutEntry(void)
        : type(glu::TYPE_LAST)
        , size(0)
        , blockNdx(-1)
        , offset(-1)
        , arrayStride(-1)
        , matrixStride(-1)
        , isRowMajor(false)
    {
    }

    std::string name;
    glu::DataType type;
    int size;
    int blockNdx;
    int offset;
    int arrayStride;
    int matrixStride;
    bool isRowMajor;
};

std::ostream &operator<<(std::ostream &stream, const UniformLayoutEntry &entry)
{
    stream << entry.name << " { type = " << glu::getDataTypeName(entry.type) << ", size = " << entry.size
           << ", blockNdx = " << entry.blockNdx << ", offset = " << entry.offset
           << ", arrayStride = " << entry.arrayStride << ", matrixStride = " << entry.matrixStride
           << ", isRowMajor = " << (entry.isRowMajor ? "true" : "false") << " }";
    return stream;
}

class UniformLayout
{
public:
    std::vector<BlockLayoutEntry> blocks;
    std::vector<UniformLayoutEntry> uniforms;

    int getUniformIndex(const char *name) const;
    int getBlockIndex(const char *name) const;
};

// \todo [2012-01-24 pyry] Speed up lookups using hash.

int UniformLayout::getUniformIndex(const char *name) const
{
    for (int ndx = 0; ndx < (int)uniforms.size(); ndx++)
    {
        if (uniforms[ndx].name == name)
            return ndx;
    }
    return -1;
}

int UniformLayout::getBlockIndex(const char *name) const
{
    for (int ndx = 0; ndx < (int)blocks.size(); ndx++)
    {
        if (blocks[ndx].name == name)
            return ndx;
    }
    return -1;
}

// ShaderInterface implementation.

ShaderInterface::ShaderInterface(void)
{
}

ShaderInterface::~ShaderInterface(void)
{
    for (std::vector<StructType *>::iterator i = m_structs.begin(); i != m_structs.end(); i++)
        delete *i;

    for (std::vector<UniformBlock *>::iterator i = m_uniformBlocks.begin(); i != m_uniformBlocks.end(); i++)
        delete *i;
}

StructType &ShaderInterface::allocStruct(const char *name)
{
    m_structs.reserve(m_structs.size() + 1);
    m_structs.push_back(new StructType(name));
    return *m_structs.back();
}

struct StructNameEquals
{
    std::string name;

    StructNameEquals(const char *name_) : name(name_)
    {
    }

    bool operator()(const StructType *type) const
    {
        return type->getTypeName() && name == type->getTypeName();
    }
};

const StructType *ShaderInterface::findStruct(const char *name) const
{
    std::vector<StructType *>::const_iterator pos =
        std::find_if(m_structs.begin(), m_structs.end(), StructNameEquals(name));
    return pos != m_structs.end() ? *pos : nullptr;
}

void ShaderInterface::getNamedStructs(std::vector<const StructType *> &structs) const
{
    for (std::vector<StructType *>::const_iterator i = m_structs.begin(); i != m_structs.end(); i++)
    {
        if ((*i)->getTypeName() != nullptr)
            structs.push_back(*i);
    }
}

UniformBlock &ShaderInterface::allocBlock(const char *name)
{
    m_uniformBlocks.reserve(m_uniformBlocks.size() + 1);
    m_uniformBlocks.push_back(new UniformBlock(name));
    return *m_uniformBlocks.back();
}

namespace // Utilities
{

// Layout computation.

int getDataTypeByteSize(glu::DataType type)
{
    return glu::getDataTypeScalarSize(type) * (int)sizeof(uint32_t);
}

int getDataTypeByteAlignment(glu::DataType type)
{
    switch (type)
    {
    case glu::TYPE_FLOAT:
    case glu::TYPE_INT:
    case glu::TYPE_UINT:
    case glu::TYPE_BOOL:
        return 1 * (int)sizeof(uint32_t);

    case glu::TYPE_FLOAT_VEC2:
    case glu::TYPE_INT_VEC2:
    case glu::TYPE_UINT_VEC2:
    case glu::TYPE_BOOL_VEC2:
        return 2 * (int)sizeof(uint32_t);

    case glu::TYPE_FLOAT_VEC3:
    case glu::TYPE_INT_VEC3:
    case glu::TYPE_UINT_VEC3:
    case glu::TYPE_BOOL_VEC3: // Fall-through to vec4

    case glu::TYPE_FLOAT_VEC4:
    case glu::TYPE_INT_VEC4:
    case glu::TYPE_UINT_VEC4:
    case glu::TYPE_BOOL_VEC4:
        return 4 * (int)sizeof(uint32_t);

    default:
        DE_ASSERT(false);
        return 0;
    }
}

int getDataTypeArrayStride(glu::DataType type)
{
    DE_ASSERT(!glu::isDataTypeMatrix(type));

    const int baseStride    = getDataTypeByteSize(type);
    const int vec4Alignment = (int)sizeof(uint32_t) * 4;

    DE_ASSERT(baseStride <= vec4Alignment);
    return de::max(baseStride, vec4Alignment); // Really? See rule 4.
}

int computeStd140BaseAlignment(const VarType &type)
{
    const int vec4Alignment = (int)sizeof(uint32_t) * 4;

    if (type.isBasicType())
    {
        glu::DataType basicType = type.getBasicType();

        if (glu::isDataTypeMatrix(basicType))
        {
            bool isRowMajor = !!(type.getFlags() & LAYOUT_ROW_MAJOR);
            int vecSize =
                isRowMajor ? glu::getDataTypeMatrixNumColumns(basicType) : glu::getDataTypeMatrixNumRows(basicType);

            return getDataTypeArrayStride(glu::getDataTypeFloatVec(vecSize));
        }
        else
            return getDataTypeByteAlignment(basicType);
    }
    else if (type.isArrayType())
    {
        int elemAlignment = computeStd140BaseAlignment(type.getElementType());

        // Round up to alignment of vec4
        return deRoundUp32(elemAlignment, vec4Alignment);
    }
    else
    {
        DE_ASSERT(type.isStructType());

        int maxBaseAlignment = 0;

        for (StructType::ConstIterator memberIter = type.getStruct().begin(); memberIter != type.getStruct().end();
             memberIter++)
            maxBaseAlignment = de::max(maxBaseAlignment, computeStd140BaseAlignment(memberIter->getType()));

        return deRoundUp32(maxBaseAlignment, vec4Alignment);
    }
}

inline uint32_t mergeLayoutFlags(uint32_t prevFlags, uint32_t newFlags)
{
    const uint32_t packingMask = LAYOUT_PACKED | LAYOUT_SHARED | LAYOUT_STD140;
    const uint32_t matrixMask  = LAYOUT_ROW_MAJOR | LAYOUT_COLUMN_MAJOR;

    uint32_t mergedFlags = 0;

    mergedFlags |= ((newFlags & packingMask) ? newFlags : prevFlags) & packingMask;
    mergedFlags |= ((newFlags & matrixMask) ? newFlags : prevFlags) & matrixMask;

    return mergedFlags;
}

void computeStd140Layout(UniformLayout &layout, int &curOffset, int curBlockNdx, const std::string &curPrefix,
                         const VarType &type, uint32_t layoutFlags)
{
    int baseAlignment = computeStd140BaseAlignment(type);

    curOffset = deAlign32(curOffset, baseAlignment);

    if (type.isBasicType())
    {
        glu::DataType basicType = type.getBasicType();
        UniformLayoutEntry entry;

        entry.name         = curPrefix;
        entry.type         = basicType;
        entry.size         = 1;
        entry.arrayStride  = 0;
        entry.matrixStride = 0;
        entry.blockNdx     = curBlockNdx;

        if (glu::isDataTypeMatrix(basicType))
        {
            // Array of vectors as specified in rules 5 & 7.
            bool isRowMajor =
                !!(((type.getFlags() & (LAYOUT_ROW_MAJOR | LAYOUT_COLUMN_MAJOR) ? type.getFlags() : layoutFlags) &
                    LAYOUT_ROW_MAJOR));

            int vecSize =
                isRowMajor ? glu::getDataTypeMatrixNumColumns(basicType) : glu::getDataTypeMatrixNumRows(basicType);
            int numVecs =
                isRowMajor ? glu::getDataTypeMatrixNumRows(basicType) : glu::getDataTypeMatrixNumColumns(basicType);
            int stride = getDataTypeArrayStride(glu::getDataTypeFloatVec(vecSize));

            entry.offset       = curOffset;
            entry.matrixStride = stride;
            entry.isRowMajor   = isRowMajor;

            curOffset += numVecs * stride;
        }
        else
        {
            // Scalar or vector.
            entry.offset = curOffset;

            curOffset += getDataTypeByteSize(basicType);
        }

        layout.uniforms.push_back(entry);
    }
    else if (type.isArrayType())
    {
        const VarType &elemType = type.getElementType();

        if (elemType.isBasicType() && !glu::isDataTypeMatrix(elemType.getBasicType()))
        {
            // Array of scalars or vectors.
            glu::DataType elemBasicType = elemType.getBasicType();
            UniformLayoutEntry entry;
            int stride = getDataTypeArrayStride(elemBasicType);

            entry.name         = curPrefix + "[0]"; // Array uniforms are always postfixed with [0]
            entry.type         = elemBasicType;
            entry.blockNdx     = curBlockNdx;
            entry.offset       = curOffset;
            entry.size         = type.getArraySize();
            entry.arrayStride  = stride;
            entry.matrixStride = 0;

            curOffset += stride * type.getArraySize();

            layout.uniforms.push_back(entry);
        }
        else if (elemType.isBasicType() && glu::isDataTypeMatrix(elemType.getBasicType()))
        {
            // Array of matrices.
            glu::DataType elemBasicType = elemType.getBasicType();
            bool isRowMajor             = !!(
                ((elemType.getFlags() & (LAYOUT_ROW_MAJOR | LAYOUT_COLUMN_MAJOR) ? elemType.getFlags() : layoutFlags) &
                 LAYOUT_ROW_MAJOR));
            int vecSize = isRowMajor ? glu::getDataTypeMatrixNumColumns(elemBasicType) :
                                       glu::getDataTypeMatrixNumRows(elemBasicType);
            int numVecs = isRowMajor ? glu::getDataTypeMatrixNumRows(elemBasicType) :
                                       glu::getDataTypeMatrixNumColumns(elemBasicType);
            int stride  = getDataTypeArrayStride(glu::getDataTypeFloatVec(vecSize));
            UniformLayoutEntry entry;

            entry.name         = curPrefix + "[0]"; // Array uniforms are always postfixed with [0]
            entry.type         = elemBasicType;
            entry.blockNdx     = curBlockNdx;
            entry.offset       = curOffset;
            entry.size         = type.getArraySize();
            entry.arrayStride  = stride * numVecs;
            entry.matrixStride = stride;
            entry.isRowMajor   = isRowMajor;

            curOffset += numVecs * type.getArraySize() * stride;

            layout.uniforms.push_back(entry);
        }
        else
        {
            DE_ASSERT(elemType.isStructType() || elemType.isArrayType());

            for (int elemNdx = 0; elemNdx < type.getArraySize(); elemNdx++)
                computeStd140Layout(layout, curOffset, curBlockNdx, curPrefix + "[" + de::toString(elemNdx) + "]",
                                    type.getElementType(), layoutFlags);
        }
    }
    else
    {
        DE_ASSERT(type.isStructType());

        // Override matrix packing layout flags in case the structure has them defined.
        const uint32_t matrixLayoutMask = LAYOUT_ROW_MAJOR | LAYOUT_COLUMN_MAJOR;
        if (type.getFlags() & matrixLayoutMask)
            layoutFlags = (layoutFlags & (~matrixLayoutMask)) | (type.getFlags() & matrixLayoutMask);

        for (StructType::ConstIterator memberIter = type.getStruct().begin(); memberIter != type.getStruct().end();
             memberIter++)
            computeStd140Layout(layout, curOffset, curBlockNdx, curPrefix + "." + memberIter->getName(),
                                memberIter->getType(), layoutFlags);

        curOffset = deAlign32(curOffset, baseAlignment);
    }
}

void computeStd140Layout(UniformLayout &layout, const ShaderInterface &interface)
{
    // \todo [2012-01-23 pyry] Uniforms in default block.

    int numUniformBlocks = interface.getNumUniformBlocks();

    for (int blockNdx = 0; blockNdx < numUniformBlocks; blockNdx++)
    {
        const UniformBlock &block = interface.getUniformBlock(blockNdx);
        bool hasInstanceName      = block.getInstanceName() != nullptr;
        std::string blockPrefix   = hasInstanceName ? (std::string(block.getBlockName()) + ".") : std::string("");
        int curOffset             = 0;
        int activeBlockNdx        = (int)layout.blocks.size();
        int firstUniformNdx       = (int)layout.uniforms.size();

        for (UniformBlock::ConstIterator uniformIter = block.begin(); uniformIter != block.end(); uniformIter++)
        {
            const Uniform &uniform = *uniformIter;
            computeStd140Layout(layout, curOffset, activeBlockNdx, blockPrefix + uniform.getName(), uniform.getType(),
                                mergeLayoutFlags(block.getFlags(), uniform.getFlags()));
        }

        int uniformIndicesEnd = (int)layout.uniforms.size();
        int blockSize         = curOffset;
        int numInstances      = block.isArray() ? block.getArraySize() : 1;

        // Create block layout entries for each instance.
        for (int instanceNdx = 0; instanceNdx < numInstances; instanceNdx++)
        {
            // Allocate entry for instance.
            layout.blocks.push_back(BlockLayoutEntry());
            BlockLayoutEntry &blockEntry = layout.blocks.back();

            blockEntry.name = block.getBlockName();
            blockEntry.size = blockSize;

            // Compute active uniform set for block.
            for (int uniformNdx = firstUniformNdx; uniformNdx < uniformIndicesEnd; uniformNdx++)
                blockEntry.activeUniformIndices.push_back(uniformNdx);

            if (block.isArray())
                blockEntry.name += "[" + de::toString(instanceNdx) + "]";
        }
    }
}

// Value generator.

void generateValue(const UniformLayoutEntry &entry, void *basePtr, de::Random &rnd)
{
    glu::DataType scalarType = glu::getDataTypeScalarType(entry.type);
    int scalarSize           = glu::getDataTypeScalarSize(entry.type);
    bool isMatrix            = glu::isDataTypeMatrix(entry.type);
    int numVecs              = isMatrix ? (entry.isRowMajor ? glu::getDataTypeMatrixNumRows(entry.type) :
                                                              glu::getDataTypeMatrixNumColumns(entry.type)) :
                                          1;
    int vecSize              = scalarSize / numVecs;
    bool isArray             = entry.size > 1;
    const int compSize       = sizeof(uint32_t);

    DE_ASSERT(scalarSize % numVecs == 0);

    for (int elemNdx = 0; elemNdx < entry.size; elemNdx++)
    {
        uint8_t *elemPtr = (uint8_t *)basePtr + entry.offset + (isArray ? elemNdx * entry.arrayStride : 0);

        for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
        {
            uint8_t *vecPtr = elemPtr + (isMatrix ? vecNdx * entry.matrixStride : 0);

            for (int compNdx = 0; compNdx < vecSize; compNdx++)
            {
                uint8_t *compPtr = vecPtr + compSize * compNdx;

                switch (scalarType)
                {
                case glu::TYPE_FLOAT:
                    *((float *)compPtr) = (float)rnd.getInt(-9, 9);
                    break;
                case glu::TYPE_INT:
                    *((int *)compPtr) = rnd.getInt(-9, 9);
                    break;
                case glu::TYPE_UINT:
                    *((uint32_t *)compPtr) = (uint32_t)rnd.getInt(0, 9);
                    break;
                // \note Random bit pattern is used for true values. Spec states that all non-zero values are
                //       interpreted as true but some implementations fail this.
                case glu::TYPE_BOOL:
                    *((uint32_t *)compPtr) = rnd.getBool() ? rnd.getUint32() | 1u : 0u;
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
        }
    }
}

void generateValues(const UniformLayout &layout, const std::map<int, void *> &blockPointers, uint32_t seed)
{
    de::Random rnd(seed);
    int numBlocks = (int)layout.blocks.size();

    for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
    {
        void *basePtr  = blockPointers.find(blockNdx)->second;
        int numEntries = (int)layout.blocks[blockNdx].activeUniformIndices.size();

        for (int entryNdx = 0; entryNdx < numEntries; entryNdx++)
        {
            const UniformLayoutEntry &entry = layout.uniforms[layout.blocks[blockNdx].activeUniformIndices[entryNdx]];
            generateValue(entry, basePtr, rnd);
        }
    }
}

// Shader generator.

const char *getCompareFuncForType(glu::DataType type)
{
    switch (type)
    {
    case glu::TYPE_FLOAT:
        return "mediump float compare_float    (highp float a, highp float b)  { return abs(a - b) < 0.05 ? 1.0 : 0.0; "
               "}\n";
    case glu::TYPE_FLOAT_VEC2:
        return "mediump float compare_vec2     (highp vec2 a, highp vec2 b)    { return compare_float(a.x, "
               "b.x)*compare_float(a.y, b.y); }\n";
    case glu::TYPE_FLOAT_VEC3:
        return "mediump float compare_vec3     (highp vec3 a, highp vec3 b)    { return compare_float(a.x, "
               "b.x)*compare_float(a.y, b.y)*compare_float(a.z, b.z); }\n";
    case glu::TYPE_FLOAT_VEC4:
        return "mediump float compare_vec4     (highp vec4 a, highp vec4 b)    { return compare_float(a.x, "
               "b.x)*compare_float(a.y, b.y)*compare_float(a.z, b.z)*compare_float(a.w, b.w); }\n";
    case glu::TYPE_FLOAT_MAT2:
        return "mediump float compare_mat2     (highp mat2 a, highp mat2 b)    { return compare_vec2(a[0], "
               "b[0])*compare_vec2(a[1], b[1]); }\n";
    case glu::TYPE_FLOAT_MAT2X3:
        return "mediump float compare_mat2x3   (highp mat2x3 a, highp mat2x3 b){ return compare_vec3(a[0], "
               "b[0])*compare_vec3(a[1], b[1]); }\n";
    case glu::TYPE_FLOAT_MAT2X4:
        return "mediump float compare_mat2x4   (highp mat2x4 a, highp mat2x4 b){ return compare_vec4(a[0], "
               "b[0])*compare_vec4(a[1], b[1]); }\n";
    case glu::TYPE_FLOAT_MAT3X2:
        return "mediump float compare_mat3x2   (highp mat3x2 a, highp mat3x2 b){ return compare_vec2(a[0], "
               "b[0])*compare_vec2(a[1], b[1])*compare_vec2(a[2], b[2]); }\n";
    case glu::TYPE_FLOAT_MAT3:
        return "mediump float compare_mat3     (highp mat3 a, highp mat3 b)    { return compare_vec3(a[0], "
               "b[0])*compare_vec3(a[1], b[1])*compare_vec3(a[2], b[2]); }\n";
    case glu::TYPE_FLOAT_MAT3X4:
        return "mediump float compare_mat3x4   (highp mat3x4 a, highp mat3x4 b){ return compare_vec4(a[0], "
               "b[0])*compare_vec4(a[1], b[1])*compare_vec4(a[2], b[2]); }\n";
    case glu::TYPE_FLOAT_MAT4X2:
        return "mediump float compare_mat4x2   (highp mat4x2 a, highp mat4x2 b){ return compare_vec2(a[0], "
               "b[0])*compare_vec2(a[1], b[1])*compare_vec2(a[2], b[2])*compare_vec2(a[3], b[3]); }\n";
    case glu::TYPE_FLOAT_MAT4X3:
        return "mediump float compare_mat4x3   (highp mat4x3 a, highp mat4x3 b){ return compare_vec3(a[0], "
               "b[0])*compare_vec3(a[1], b[1])*compare_vec3(a[2], b[2])*compare_vec3(a[3], b[3]); }\n";
    case glu::TYPE_FLOAT_MAT4:
        return "mediump float compare_mat4     (highp mat4 a, highp mat4 b)    { return compare_vec4(a[0], "
               "b[0])*compare_vec4(a[1], b[1])*compare_vec4(a[2], b[2])*compare_vec4(a[3], b[3]); }\n";
    case glu::TYPE_INT:
        return "mediump float compare_int      (highp int a, highp int b)      { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_INT_VEC2:
        return "mediump float compare_ivec2    (highp ivec2 a, highp ivec2 b)  { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_INT_VEC3:
        return "mediump float compare_ivec3    (highp ivec3 a, highp ivec3 b)  { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_INT_VEC4:
        return "mediump float compare_ivec4    (highp ivec4 a, highp ivec4 b)  { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_UINT:
        return "mediump float compare_uint     (highp uint a, highp uint b)    { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_UINT_VEC2:
        return "mediump float compare_uvec2    (highp uvec2 a, highp uvec2 b)  { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_UINT_VEC3:
        return "mediump float compare_uvec3    (highp uvec3 a, highp uvec3 b)  { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_UINT_VEC4:
        return "mediump float compare_uvec4    (highp uvec4 a, highp uvec4 b)  { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_BOOL:
        return "mediump float compare_bool     (bool a, bool b)                { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_BOOL_VEC2:
        return "mediump float compare_bvec2    (bvec2 a, bvec2 b)              { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_BOOL_VEC3:
        return "mediump float compare_bvec3    (bvec3 a, bvec3 b)              { return a == b ? 1.0 : 0.0; }\n";
    case glu::TYPE_BOOL_VEC4:
        return "mediump float compare_bvec4    (bvec4 a, bvec4 b)              { return a == b ? 1.0 : 0.0; }\n";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

void getCompareDependencies(std::set<glu::DataType> &compareFuncs, glu::DataType basicType)
{
    switch (basicType)
    {
    case glu::TYPE_FLOAT_VEC2:
    case glu::TYPE_FLOAT_VEC3:
    case glu::TYPE_FLOAT_VEC4:
        compareFuncs.insert(glu::TYPE_FLOAT);
        compareFuncs.insert(basicType);
        break;

    case glu::TYPE_FLOAT_MAT2:
    case glu::TYPE_FLOAT_MAT2X3:
    case glu::TYPE_FLOAT_MAT2X4:
    case glu::TYPE_FLOAT_MAT3X2:
    case glu::TYPE_FLOAT_MAT3:
    case glu::TYPE_FLOAT_MAT3X4:
    case glu::TYPE_FLOAT_MAT4X2:
    case glu::TYPE_FLOAT_MAT4X3:
    case glu::TYPE_FLOAT_MAT4:
        compareFuncs.insert(glu::TYPE_FLOAT);
        compareFuncs.insert(glu::getDataTypeFloatVec(glu::getDataTypeMatrixNumRows(basicType)));
        compareFuncs.insert(basicType);
        break;

    default:
        compareFuncs.insert(basicType);
        break;
    }
}

void collectUniqueBasicTypes(std::set<glu::DataType> &basicTypes, const VarType &type)
{
    if (type.isStructType())
    {
        for (StructType::ConstIterator iter = type.getStruct().begin(); iter != type.getStruct().end(); ++iter)
            collectUniqueBasicTypes(basicTypes, iter->getType());
    }
    else if (type.isArrayType())
        collectUniqueBasicTypes(basicTypes, type.getElementType());
    else
    {
        DE_ASSERT(type.isBasicType());
        basicTypes.insert(type.getBasicType());
    }
}

void collectUniqueBasicTypes(std::set<glu::DataType> &basicTypes, const UniformBlock &uniformBlock)
{
    for (UniformBlock::ConstIterator iter = uniformBlock.begin(); iter != uniformBlock.end(); ++iter)
        collectUniqueBasicTypes(basicTypes, iter->getType());
}

void collectUniqueBasicTypes(std::set<glu::DataType> &basicTypes, const ShaderInterface &interface)
{
    for (int ndx = 0; ndx < interface.getNumUniformBlocks(); ++ndx)
        collectUniqueBasicTypes(basicTypes, interface.getUniformBlock(ndx));
}

void generateCompareFuncs(std::ostream &str, const ShaderInterface &interface)
{
    std::set<glu::DataType> types;
    std::set<glu::DataType> compareFuncs;

    // Collect unique basic types
    collectUniqueBasicTypes(types, interface);

    // Set of compare functions required
    for (std::set<glu::DataType>::const_iterator iter = types.begin(); iter != types.end(); ++iter)
    {
        getCompareDependencies(compareFuncs, *iter);
    }

    for (int type = 0; type < glu::TYPE_LAST; ++type)
    {
        if (compareFuncs.find(glu::DataType(type)) != compareFuncs.end())
            str << getCompareFuncForType(glu::DataType(type));
    }
}

struct Indent
{
    int level;
    Indent(int level_) : level(level_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const Indent &indent)
{
    for (int i = 0; i < indent.level; i++)
        str << "\t";
    return str;
}

void generateDeclaration(std::ostringstream &src, const VarType &type, const char *name, int indentLevel,
                         uint32_t unusedHints);
void generateDeclaration(std::ostringstream &src, const Uniform &uniform, int indentLevel);
void generateDeclaration(std::ostringstream &src, const StructType &structType, int indentLevel);

void generateLocalDeclaration(std::ostringstream &src, const StructType &structType, int indentLevel);
void generateFullDeclaration(std::ostringstream &src, const StructType &structType, int indentLevel);

void generateDeclaration(std::ostringstream &src, const StructType &structType, int indentLevel)
{
    DE_ASSERT(structType.getTypeName() != nullptr);
    generateFullDeclaration(src, structType, indentLevel);
    src << ";\n";
}

void generateFullDeclaration(std::ostringstream &src, const StructType &structType, int indentLevel)
{
    src << "struct";
    if (structType.getTypeName())
        src << " " << structType.getTypeName();
    src << "\n" << Indent(indentLevel) << "{\n";

    for (StructType::ConstIterator memberIter = structType.begin(); memberIter != structType.end(); memberIter++)
    {
        src << Indent(indentLevel + 1);
        generateDeclaration(src, memberIter->getType(), memberIter->getName(), indentLevel + 1,
                            memberIter->getFlags() & UNUSED_BOTH);
    }

    src << Indent(indentLevel) << "}";
}

void generateLocalDeclaration(std::ostringstream &src, const StructType &structType, int indentLevel)
{
    if (structType.getTypeName() == nullptr)
        generateFullDeclaration(src, structType, indentLevel);
    else
        src << structType.getTypeName();
}

void generateDeclaration(std::ostringstream &src, const VarType &type, const char *name, int indentLevel,
                         uint32_t unusedHints)
{
    uint32_t flags = type.getFlags();

    if ((flags & LAYOUT_MASK) != 0)
        src << "layout(" << LayoutFlagsFmt(flags & LAYOUT_MASK) << ") ";

    if ((flags & PRECISION_MASK) != 0)
        src << PrecisionFlagsFmt(flags & PRECISION_MASK) << " ";

    if (type.isBasicType())
        src << glu::getDataTypeName(type.getBasicType()) << " " << name;
    else if (type.isArrayType())
    {
        std::vector<int> arraySizes;
        const VarType *curType = &type;
        while (curType->isArrayType())
        {
            arraySizes.push_back(curType->getArraySize());
            curType = &curType->getElementType();
        }

        if (curType->isBasicType())
        {
            if ((curType->getFlags() & LAYOUT_MASK) != 0)
                src << "layout(" << LayoutFlagsFmt(curType->getFlags() & LAYOUT_MASK) << ") ";
            if ((curType->getFlags() & PRECISION_MASK) != 0)
                src << PrecisionFlagsFmt(curType->getFlags() & PRECISION_MASK) << " ";
            src << glu::getDataTypeName(curType->getBasicType());
        }
        else
        {
            DE_ASSERT(curType->isStructType());
            generateLocalDeclaration(src, curType->getStruct(), indentLevel + 1);
        }

        src << " " << name;

        for (std::vector<int>::const_iterator sizeIter = arraySizes.begin(); sizeIter != arraySizes.end(); sizeIter++)
            src << "[" << *sizeIter << "]";
    }
    else
    {
        generateLocalDeclaration(src, type.getStruct(), indentLevel + 1);
        src << " " << name;
    }

    src << ";";

    // Print out unused hints.
    if (unusedHints != 0)
        src << " // unused in "
            << (unusedHints == UNUSED_BOTH     ? "both shaders" :
                unusedHints == UNUSED_VERTEX   ? "vertex shader" :
                unusedHints == UNUSED_FRAGMENT ? "fragment shader" :
                                                 "???");

    src << "\n";
}

void generateDeclaration(std::ostringstream &src, const Uniform &uniform, int indentLevel)
{
    if ((uniform.getFlags() & LAYOUT_MASK) != 0)
        src << "layout(" << LayoutFlagsFmt(uniform.getFlags() & LAYOUT_MASK) << ") ";

    generateDeclaration(src, uniform.getType(), uniform.getName(), indentLevel, uniform.getFlags() & UNUSED_BOTH);
}

void generateDeclaration(std::ostringstream &src, const UniformBlock &block)
{
    if ((block.getFlags() & LAYOUT_MASK) != 0)
        src << "layout(" << LayoutFlagsFmt(block.getFlags() & LAYOUT_MASK) << ") ";

    src << "uniform " << block.getBlockName();
    src << "\n{\n";

    for (UniformBlock::ConstIterator uniformIter = block.begin(); uniformIter != block.end(); uniformIter++)
    {
        src << Indent(1);
        generateDeclaration(src, *uniformIter, 1 /* indent level */);
    }

    src << "}";

    if (block.getInstanceName() != nullptr)
    {
        src << " " << block.getInstanceName();
        if (block.isArray())
            src << "[" << block.getArraySize() << "]";
    }
    else
        DE_ASSERT(!block.isArray());

    src << ";\n";
}

void generateValueSrc(std::ostringstream &src, const UniformLayoutEntry &entry, const void *basePtr, int elementNdx)
{
    glu::DataType scalarType = glu::getDataTypeScalarType(entry.type);
    int scalarSize           = glu::getDataTypeScalarSize(entry.type);
    bool isArray             = entry.size > 1;
    const uint8_t *elemPtr   = (const uint8_t *)basePtr + entry.offset + (isArray ? elementNdx * entry.arrayStride : 0);
    const int compSize       = sizeof(uint32_t);

    if (scalarSize > 1)
        src << glu::getDataTypeName(entry.type) << "(";

    if (glu::isDataTypeMatrix(entry.type))
    {
        int numRows = glu::getDataTypeMatrixNumRows(entry.type);
        int numCols = glu::getDataTypeMatrixNumColumns(entry.type);

        DE_ASSERT(scalarType == glu::TYPE_FLOAT);

        // Constructed in column-wise order.
        for (int colNdx = 0; colNdx < numCols; colNdx++)
        {
            for (int rowNdx = 0; rowNdx < numRows; rowNdx++)
            {
                const uint8_t *compPtr = elemPtr + (entry.isRowMajor ? rowNdx * entry.matrixStride + colNdx * compSize :
                                                                       colNdx * entry.matrixStride + rowNdx * compSize);

                if (colNdx > 0 || rowNdx > 0)
                    src << ", ";

                src << de::floatToString(*((const float *)compPtr), 1);
            }
        }
    }
    else
    {
        for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
        {
            const uint8_t *compPtr = elemPtr + scalarNdx * compSize;

            if (scalarNdx > 0)
                src << ", ";

            switch (scalarType)
            {
            case glu::TYPE_FLOAT:
                src << de::floatToString(*((const float *)compPtr), 1);
                break;
            case glu::TYPE_INT:
                src << *((const int *)compPtr);
                break;
            case glu::TYPE_UINT:
                src << *((const uint32_t *)compPtr) << "u";
                break;
            case glu::TYPE_BOOL:
                src << (*((const uint32_t *)compPtr) != 0u ? "true" : "false");
                break;
            default:
                DE_ASSERT(false);
            }
        }
    }

    if (scalarSize > 1)
        src << ")";
}

void generateCompareSrc(std::ostringstream &src, const char *resultVar, const VarType &type, const char *srcName,
                        const char *apiName, const UniformLayout &layout, const void *basePtr, uint32_t unusedMask)
{
    if (type.isBasicType() || (type.isArrayType() && type.getElementType().isBasicType()))
    {
        // Basic type or array of basic types.
        bool isArray              = type.isArrayType();
        glu::DataType elementType = isArray ? type.getElementType().getBasicType() : type.getBasicType();
        const char *typeName      = glu::getDataTypeName(elementType);
        std::string fullApiName   = string(apiName) + (isArray ? "[0]" : ""); // Arrays are always postfixed with [0]
        int uniformNdx            = layout.getUniformIndex(fullApiName.c_str());
        const UniformLayoutEntry &entry = layout.uniforms[uniformNdx];

        if (isArray)
        {
            for (int elemNdx = 0; elemNdx < type.getArraySize(); elemNdx++)
            {
                src << "\tresult *= compare_" << typeName << "(" << srcName << "[" << elemNdx << "], ";
                generateValueSrc(src, entry, basePtr, elemNdx);
                src << ");\n";
            }
        }
        else
        {
            src << "\tresult *= compare_" << typeName << "(" << srcName << ", ";
            generateValueSrc(src, entry, basePtr, 0);
            src << ");\n";
        }
    }
    else if (type.isArrayType())
    {
        const VarType &elementType = type.getElementType();

        for (int elementNdx = 0; elementNdx < type.getArraySize(); elementNdx++)
        {
            std::string op = string("[") + de::toString(elementNdx) + "]";
            generateCompareSrc(src, resultVar, elementType, (string(srcName) + op).c_str(),
                               (string(apiName) + op).c_str(), layout, basePtr, unusedMask);
        }
    }
    else
    {
        DE_ASSERT(type.isStructType());

        for (StructType::ConstIterator memberIter = type.getStruct().begin(); memberIter != type.getStruct().end();
             memberIter++)
        {
            if (memberIter->getFlags() & unusedMask)
                continue; // Skip member.

            string op = string(".") + memberIter->getName();
            generateCompareSrc(src, resultVar, memberIter->getType(), (string(srcName) + op).c_str(),
                               (string(apiName) + op).c_str(), layout, basePtr, unusedMask);
        }
    }
}

void generateCompareSrc(std::ostringstream &src, const char *resultVar, const ShaderInterface &interface,
                        const UniformLayout &layout, const std::map<int, void *> &blockPointers, bool isVertex)
{
    uint32_t unusedMask = isVertex ? UNUSED_VERTEX : UNUSED_FRAGMENT;

    for (int blockNdx = 0; blockNdx < interface.getNumUniformBlocks(); blockNdx++)
    {
        const UniformBlock &block = interface.getUniformBlock(blockNdx);

        if ((block.getFlags() & (isVertex ? DECLARE_VERTEX : DECLARE_FRAGMENT)) == 0)
            continue; // Skip.

        bool hasInstanceName  = block.getInstanceName() != nullptr;
        bool isArray          = block.isArray();
        int numInstances      = isArray ? block.getArraySize() : 1;
        std::string apiPrefix = hasInstanceName ? string(block.getBlockName()) + "." : string("");

        DE_ASSERT(!isArray || hasInstanceName);

        for (int instanceNdx = 0; instanceNdx < numInstances; instanceNdx++)
        {
            std::string instancePostfix   = isArray ? string("[") + de::toString(instanceNdx) + "]" : string("");
            std::string blockInstanceName = block.getBlockName() + instancePostfix;
            std::string srcPrefix =
                hasInstanceName ? string(block.getInstanceName()) + instancePostfix + "." : string("");
            int activeBlockNdx = layout.getBlockIndex(blockInstanceName.c_str());
            void *basePtr      = blockPointers.find(activeBlockNdx)->second;

            for (UniformBlock::ConstIterator uniformIter = block.begin(); uniformIter != block.end(); uniformIter++)
            {
                const Uniform &uniform = *uniformIter;

                if (uniform.getFlags() & unusedMask)
                    continue; // Don't read from that uniform.

                generateCompareSrc(src, resultVar, uniform.getType(), (srcPrefix + uniform.getName()).c_str(),
                                   (apiPrefix + uniform.getName()).c_str(), layout, basePtr, unusedMask);
            }
        }
    }
}

void generateVertexShader(std::ostringstream &src, glu::GLSLVersion glslVersion, const ShaderInterface &interface,
                          const UniformLayout &layout, const std::map<int, void *> &blockPointers)
{
    DE_ASSERT(isSupportedGLSLVersion(glslVersion));

    src << glu::getGLSLVersionDeclaration(glslVersion) << "\n";
    src << "in highp vec4 a_position;\n";
    src << "out mediump float v_vtxResult;\n";
    src << "\n";

    std::vector<const StructType *> namedStructs;
    interface.getNamedStructs(namedStructs);
    for (std::vector<const StructType *>::const_iterator structIter = namedStructs.begin();
         structIter != namedStructs.end(); structIter++)
        generateDeclaration(src, **structIter, 0);

    for (int blockNdx = 0; blockNdx < interface.getNumUniformBlocks(); blockNdx++)
    {
        const UniformBlock &block = interface.getUniformBlock(blockNdx);
        if (block.getFlags() & DECLARE_VERTEX)
            generateDeclaration(src, block);
    }

    // Comparison utilities.
    src << "\n";
    generateCompareFuncs(src, interface);

    src << "\n"
           "void main (void)\n"
           "{\n"
           "    gl_Position = a_position;\n"
           "    mediump float result = 1.0;\n";

    // Value compare.
    generateCompareSrc(src, "result", interface, layout, blockPointers, true);

    src << "    v_vtxResult = result;\n"
           "}\n";
}

void generateFragmentShader(std::ostringstream &src, glu::GLSLVersion glslVersion, const ShaderInterface &interface,
                            const UniformLayout &layout, const std::map<int, void *> &blockPointers)
{
    DE_ASSERT(isSupportedGLSLVersion(glslVersion));

    src << glu::getGLSLVersionDeclaration(glslVersion) << "\n";
    src << "in mediump float v_vtxResult;\n";
    src << "layout(location = 0) out mediump vec4 dEQP_FragColor;\n";
    src << "\n";

    std::vector<const StructType *> namedStructs;
    interface.getNamedStructs(namedStructs);
    for (std::vector<const StructType *>::const_iterator structIter = namedStructs.begin();
         structIter != namedStructs.end(); structIter++)
        generateDeclaration(src, **structIter, 0);

    for (int blockNdx = 0; blockNdx < interface.getNumUniformBlocks(); blockNdx++)
    {
        const UniformBlock &block = interface.getUniformBlock(blockNdx);
        if (block.getFlags() & DECLARE_FRAGMENT)
            generateDeclaration(src, block);
    }

    // Comparison utilities.
    src << "\n";
    generateCompareFuncs(src, interface);

    src << "\n"
           "void main (void)\n"
           "{\n"
           "    mediump float result = 1.0;\n";

    // Value compare.
    generateCompareSrc(src, "result", interface, layout, blockPointers, false);

    src << "    dEQP_FragColor = vec4(1.0, v_vtxResult, result, 1.0);\n"
           "}\n";
}

void getGLUniformLayout(const glw::Functions &gl, UniformLayout &layout, uint32_t program)
{
    int numActiveUniforms = 0;
    int numActiveBlocks   = 0;

    gl.getProgramiv(program, GL_ACTIVE_UNIFORMS, &numActiveUniforms);
    gl.getProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &numActiveBlocks);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to get number of uniforms and uniform blocks");

    // Block entries.
    layout.blocks.resize(numActiveBlocks);
    for (int blockNdx = 0; blockNdx < numActiveBlocks; blockNdx++)
    {
        BlockLayoutEntry &entry = layout.blocks[blockNdx];
        int size;
        int nameLen;
        int numBlockUniforms;

        gl.getActiveUniformBlockiv(program, (uint32_t)blockNdx, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
        gl.getActiveUniformBlockiv(program, (uint32_t)blockNdx, GL_UNIFORM_BLOCK_NAME_LENGTH, &nameLen);
        gl.getActiveUniformBlockiv(program, (uint32_t)blockNdx, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &numBlockUniforms);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Uniform block query failed");

        // \note Some implementations incorrectly return 0 as name length even though the length should include null terminator.
        std::vector<char> nameBuf(nameLen > 0 ? nameLen : 1);
        gl.getActiveUniformBlockName(program, (uint32_t)blockNdx, (glw::GLsizei)nameBuf.size(), nullptr, &nameBuf[0]);

        entry.name = std::string(&nameBuf[0]);
        entry.size = size;
        entry.activeUniformIndices.resize(numBlockUniforms);

        if (numBlockUniforms > 0)
            gl.getActiveUniformBlockiv(program, (uint32_t)blockNdx, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
                                       &entry.activeUniformIndices[0]);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Uniform block query failed");
    }

    if (numActiveUniforms > 0)
    {
        // Uniform entries.
        std::vector<uint32_t> uniformIndices(numActiveUniforms);
        for (int i = 0; i < numActiveUniforms; i++)
            uniformIndices[i] = (uint32_t)i;

        std::vector<int> types(numActiveUniforms);
        std::vector<int> sizes(numActiveUniforms);
        std::vector<int> nameLengths(numActiveUniforms);
        std::vector<int> blockIndices(numActiveUniforms);
        std::vector<int> offsets(numActiveUniforms);
        std::vector<int> arrayStrides(numActiveUniforms);
        std::vector<int> matrixStrides(numActiveUniforms);
        std::vector<int> rowMajorFlags(numActiveUniforms);

        // Execute queries.
        gl.getActiveUniformsiv(program, (glw::GLsizei)uniformIndices.size(), &uniformIndices[0], GL_UNIFORM_TYPE,
                               &types[0]);
        gl.getActiveUniformsiv(program, (glw::GLsizei)uniformIndices.size(), &uniformIndices[0], GL_UNIFORM_SIZE,
                               &sizes[0]);
        gl.getActiveUniformsiv(program, (glw::GLsizei)uniformIndices.size(), &uniformIndices[0], GL_UNIFORM_NAME_LENGTH,
                               &nameLengths[0]);
        gl.getActiveUniformsiv(program, (glw::GLsizei)uniformIndices.size(), &uniformIndices[0], GL_UNIFORM_BLOCK_INDEX,
                               &blockIndices[0]);
        gl.getActiveUniformsiv(program, (glw::GLsizei)uniformIndices.size(), &uniformIndices[0], GL_UNIFORM_OFFSET,
                               &offsets[0]);
        gl.getActiveUniformsiv(program, (glw::GLsizei)uniformIndices.size(), &uniformIndices[0],
                               GL_UNIFORM_ARRAY_STRIDE, &arrayStrides[0]);
        gl.getActiveUniformsiv(program, (glw::GLsizei)uniformIndices.size(), &uniformIndices[0],
                               GL_UNIFORM_MATRIX_STRIDE, &matrixStrides[0]);
        gl.getActiveUniformsiv(program, (glw::GLsizei)uniformIndices.size(), &uniformIndices[0],
                               GL_UNIFORM_IS_ROW_MAJOR, &rowMajorFlags[0]);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Active uniform query failed");

        // Translate to LayoutEntries
        layout.uniforms.resize(numActiveUniforms);
        for (int uniformNdx = 0; uniformNdx < numActiveUniforms; uniformNdx++)
        {
            UniformLayoutEntry &entry = layout.uniforms[uniformNdx];
            std::vector<char> nameBuf(nameLengths[uniformNdx]);
            glw::GLsizei nameLen = 0;
            int size             = 0;
            uint32_t type        = GL_NONE;

            gl.getActiveUniform(program, (uint32_t)uniformNdx, (glw::GLsizei)nameBuf.size(), &nameLen, &size, &type,
                                &nameBuf[0]);

            GLU_EXPECT_NO_ERROR(gl.getError(), "Uniform name query failed");

            // \note glGetActiveUniform() returns length without \0 and glGetActiveUniformsiv() with \0
            if (nameLen + 1 != nameLengths[uniformNdx] || size != sizes[uniformNdx] ||
                type != (uint32_t)types[uniformNdx])
                TCU_FAIL("Values returned by glGetActiveUniform() don't match with values queried with "
                         "glGetActiveUniformsiv().");

            entry.name         = std::string(&nameBuf[0]);
            entry.type         = glu::getDataTypeFromGLType(types[uniformNdx]);
            entry.size         = sizes[uniformNdx];
            entry.blockNdx     = blockIndices[uniformNdx];
            entry.offset       = offsets[uniformNdx];
            entry.arrayStride  = arrayStrides[uniformNdx];
            entry.matrixStride = matrixStrides[uniformNdx];
            entry.isRowMajor   = rowMajorFlags[uniformNdx] != GL_FALSE;
        }
    }
}

void copyUniformData(const UniformLayoutEntry &dstEntry, void *dstBlockPtr, const UniformLayoutEntry &srcEntry,
                     const void *srcBlockPtr)
{
    uint8_t *dstBasePtr       = (uint8_t *)dstBlockPtr + dstEntry.offset;
    const uint8_t *srcBasePtr = (const uint8_t *)srcBlockPtr + srcEntry.offset;

    DE_ASSERT(dstEntry.size <= srcEntry.size);
    DE_ASSERT(dstEntry.type == srcEntry.type);

    int scalarSize     = glu::getDataTypeScalarSize(dstEntry.type);
    bool isMatrix      = glu::isDataTypeMatrix(dstEntry.type);
    const int compSize = sizeof(uint32_t);

    for (int elementNdx = 0; elementNdx < dstEntry.size; elementNdx++)
    {
        uint8_t *dstElemPtr       = dstBasePtr + elementNdx * dstEntry.arrayStride;
        const uint8_t *srcElemPtr = srcBasePtr + elementNdx * srcEntry.arrayStride;

        if (isMatrix)
        {
            int numRows = glu::getDataTypeMatrixNumRows(dstEntry.type);
            int numCols = glu::getDataTypeMatrixNumColumns(dstEntry.type);

            for (int colNdx = 0; colNdx < numCols; colNdx++)
            {
                for (int rowNdx = 0; rowNdx < numRows; rowNdx++)
                {
                    uint8_t *dstCompPtr =
                        dstElemPtr + (dstEntry.isRowMajor ? rowNdx * dstEntry.matrixStride + colNdx * compSize :
                                                            colNdx * dstEntry.matrixStride + rowNdx * compSize);
                    const uint8_t *srcCompPtr =
                        srcElemPtr + (srcEntry.isRowMajor ? rowNdx * srcEntry.matrixStride + colNdx * compSize :
                                                            colNdx * srcEntry.matrixStride + rowNdx * compSize);
                    deMemcpy(dstCompPtr, srcCompPtr, compSize);
                }
            }
        }
        else
            deMemcpy(dstElemPtr, srcElemPtr, scalarSize * compSize);
    }
}

void copyUniformData(const UniformLayout &dstLayout, const std::map<int, void *> &dstBlockPointers,
                     const UniformLayout &srcLayout, const std::map<int, void *> &srcBlockPointers)
{
    // \note Src layout is used as reference in case of activeUniforms happens to be incorrect in dstLayout blocks.
    int numBlocks = (int)srcLayout.blocks.size();

    for (int srcBlockNdx = 0; srcBlockNdx < numBlocks; srcBlockNdx++)
    {
        const BlockLayoutEntry &srcBlock = srcLayout.blocks[srcBlockNdx];
        const void *srcBlockPtr          = srcBlockPointers.find(srcBlockNdx)->second;
        int dstBlockNdx                  = dstLayout.getBlockIndex(srcBlock.name.c_str());
        void *dstBlockPtr                = dstBlockNdx >= 0 ? dstBlockPointers.find(dstBlockNdx)->second : nullptr;

        if (dstBlockNdx < 0)
            continue;

        for (vector<int>::const_iterator srcUniformNdxIter = srcBlock.activeUniformIndices.begin();
             srcUniformNdxIter != srcBlock.activeUniformIndices.end(); srcUniformNdxIter++)
        {
            const UniformLayoutEntry &srcEntry = srcLayout.uniforms[*srcUniformNdxIter];
            int dstUniformNdx                  = dstLayout.getUniformIndex(srcEntry.name.c_str());

            if (dstUniformNdx < 0)
                continue;

            copyUniformData(dstLayout.uniforms[dstUniformNdx], dstBlockPtr, srcEntry, srcBlockPtr);
        }
    }
}

} // namespace

class UniformBufferManager
{
public:
    UniformBufferManager(const glu::RenderContext &renderCtx);
    ~UniformBufferManager(void);

    uint32_t allocBuffer(void);

private:
    UniformBufferManager(const UniformBufferManager &other);
    UniformBufferManager &operator=(const UniformBufferManager &other);

    const glu::RenderContext &m_renderCtx;
    std::vector<uint32_t> m_buffers;
};

UniformBufferManager::UniformBufferManager(const glu::RenderContext &renderCtx) : m_renderCtx(renderCtx)
{
}

UniformBufferManager::~UniformBufferManager(void)
{
    if (!m_buffers.empty())
        m_renderCtx.getFunctions().deleteBuffers((glw::GLsizei)m_buffers.size(), &m_buffers[0]);
}

uint32_t UniformBufferManager::allocBuffer(void)
{
    uint32_t buf = 0;

    m_buffers.reserve(m_buffers.size() + 1);
    m_renderCtx.getFunctions().genBuffers(1, &buf);
    GLU_EXPECT_NO_ERROR(m_renderCtx.getFunctions().getError(), "Failed to allocate uniform buffer");
    m_buffers.push_back(buf);

    return buf;
}

} // namespace ub

using namespace ub;

// UniformBlockCase.

UniformBlockCase::UniformBlockCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                   const char *description, glu::GLSLVersion glslVersion, BufferMode bufferMode)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_glslVersion(glslVersion)
    , m_bufferMode(bufferMode)
{
    TCU_CHECK_INTERNAL(isSupportedGLSLVersion(glslVersion));
}

UniformBlockCase::~UniformBlockCase(void)
{
}

UniformBlockCase::IterateResult UniformBlockCase::iterate(void)
{
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_renderCtx.getFunctions();
    UniformLayout refLayout;        //!< std140 layout.
    vector<uint8_t> data;           //!< Data.
    map<int, void *> blockPointers; //!< Reference block pointers.

    // Initialize result to pass.
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    // Compute reference layout.
    computeStd140Layout(refLayout, m_interface);

    // Assign storage for reference values.
    {
        int totalSize = 0;
        for (vector<BlockLayoutEntry>::const_iterator blockIter = refLayout.blocks.begin();
             blockIter != refLayout.blocks.end(); blockIter++)
            totalSize += blockIter->size;
        data.resize(totalSize);

        // Pointers for each block.
        int curOffset = 0;
        for (int blockNdx = 0; blockNdx < (int)refLayout.blocks.size(); blockNdx++)
        {
            blockPointers[blockNdx] = &data[0] + curOffset;
            curOffset += refLayout.blocks[blockNdx].size;
        }
    }

    // Generate values.
    generateValues(refLayout, blockPointers, 1 /* seed */);

    // Generate shaders and build program.
    std::ostringstream vtxSrc;
    std::ostringstream fragSrc;

    generateVertexShader(vtxSrc, m_glslVersion, m_interface, refLayout, blockPointers);
    generateFragmentShader(fragSrc, m_glslVersion, m_interface, refLayout, blockPointers);

    glu::ShaderProgram program(m_renderCtx, glu::makeVtxFragSources(vtxSrc.str(), fragSrc.str()));
    log << program;

    if (!program.isOk())
    {
        // Compile failed.
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Compile failed");
        return STOP;
    }

    // Query layout from GL.
    UniformLayout glLayout;
    getGLUniformLayout(gl, glLayout, program.getProgram());

    // Print layout to log.
    log << TestLog::Section("ActiveUniformBlocks", "Active Uniform Blocks");
    for (int blockNdx = 0; blockNdx < (int)glLayout.blocks.size(); blockNdx++)
        log << TestLog::Message << blockNdx << ": " << glLayout.blocks[blockNdx] << TestLog::EndMessage;
    log << TestLog::EndSection;

    log << TestLog::Section("ActiveUniforms", "Active Uniforms");
    for (int uniformNdx = 0; uniformNdx < (int)glLayout.uniforms.size(); uniformNdx++)
        log << TestLog::Message << uniformNdx << ": " << glLayout.uniforms[uniformNdx] << TestLog::EndMessage;
    log << TestLog::EndSection;

    // Check that we can even try rendering with given layout.
    if (!checkLayoutIndices(glLayout) || !checkLayoutBounds(glLayout) || !compareTypes(refLayout, glLayout))
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid layout");
        return STOP; // It is not safe to use the given layout.
    }

    // Verify all std140 blocks.
    if (!compareStd140Blocks(refLayout, glLayout))
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid std140 layout");

    // Verify all shared blocks - all uniforms should be active, and certain properties match.
    if (!compareSharedBlocks(refLayout, glLayout))
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid shared layout");

    // Check consistency with index queries
    if (!checkIndexQueries(program.getProgram(), glLayout))
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Inconsintent block index query results");

    // Use program.
    gl.useProgram(program.getProgram());

    // Assign binding points to all active uniform blocks.
    for (int blockNdx = 0; blockNdx < (int)glLayout.blocks.size(); blockNdx++)
    {
        uint32_t binding = (uint32_t)blockNdx; // \todo [2012-01-25 pyry] Randomize order?
        gl.uniformBlockBinding(program.getProgram(), (uint32_t)blockNdx, binding);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to set uniform block bindings");

    // Allocate buffers, write data and bind to targets.
    UniformBufferManager bufferManager(m_renderCtx);
    if (m_bufferMode == BUFFERMODE_PER_BLOCK)
    {
        int numBlocks = (int)glLayout.blocks.size();
        vector<vector<uint8_t>> glData(numBlocks);
        map<int, void *> glBlockPointers;

        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            glData[blockNdx].resize(glLayout.blocks[blockNdx].size);
            glBlockPointers[blockNdx] = &glData[blockNdx][0];
        }

        copyUniformData(glLayout, glBlockPointers, refLayout, blockPointers);

        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            uint32_t buffer  = bufferManager.allocBuffer();
            uint32_t binding = (uint32_t)blockNdx;

            gl.bindBuffer(GL_UNIFORM_BUFFER, buffer);
            gl.bufferData(GL_UNIFORM_BUFFER, (glw::GLsizeiptr)glData[blockNdx].size(), &glData[blockNdx][0],
                          GL_STATIC_DRAW);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to upload uniform buffer data");

            gl.bindBufferBase(GL_UNIFORM_BUFFER, binding, buffer);
            GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBufferBase(GL_UNIFORM_BUFFER) failed");
        }
    }
    else
    {
        DE_ASSERT(m_bufferMode == BUFFERMODE_SINGLE);

        int totalSize        = 0;
        int curOffset        = 0;
        int numBlocks        = (int)glLayout.blocks.size();
        int bindingAlignment = 0;
        map<int, int> glBlockOffsets;

        gl.getIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &bindingAlignment);

        // Compute total size and offsets.
        curOffset = 0;
        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            if (bindingAlignment > 0)
                curOffset = deRoundUp32(curOffset, bindingAlignment);
            glBlockOffsets[blockNdx] = curOffset;
            curOffset += glLayout.blocks[blockNdx].size;
        }
        totalSize = curOffset;

        // Assign block pointers.
        vector<uint8_t> glData(totalSize);
        map<int, void *> glBlockPointers;

        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
            glBlockPointers[blockNdx] = &glData[glBlockOffsets[blockNdx]];

        // Copy to gl format.
        copyUniformData(glLayout, glBlockPointers, refLayout, blockPointers);

        // Allocate buffer and upload data.
        uint32_t buffer = bufferManager.allocBuffer();
        gl.bindBuffer(GL_UNIFORM_BUFFER, buffer);
        if (!glData.empty())
            gl.bufferData(GL_UNIFORM_BUFFER, (glw::GLsizeiptr)glData.size(), &glData[0], GL_STATIC_DRAW);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to upload uniform buffer data");

        // Bind ranges to binding points.
        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            uint32_t binding = (uint32_t)blockNdx;
            gl.bindBufferRange(GL_UNIFORM_BUFFER, binding, buffer, (glw::GLintptr)glBlockOffsets[blockNdx],
                               (glw::GLsizeiptr)glLayout.blocks[blockNdx].size);
            GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBufferRange(GL_UNIFORM_BUFFER) failed");
        }
    }

    bool renderOk = render(program.getProgram());
    if (!renderOk)
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image compare failed");

    return STOP;
}

bool UniformBlockCase::compareStd140Blocks(const UniformLayout &refLayout, const UniformLayout &cmpLayout) const
{
    TestLog &log  = m_testCtx.getLog();
    bool isOk     = true;
    int numBlocks = m_interface.getNumUniformBlocks();

    for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
    {
        const UniformBlock &block = m_interface.getUniformBlock(blockNdx);
        bool isArray              = block.isArray();
        std::string instanceName  = string(block.getBlockName()) + (isArray ? "[0]" : "");
        int refBlockNdx           = refLayout.getBlockIndex(instanceName.c_str());
        int cmpBlockNdx           = cmpLayout.getBlockIndex(instanceName.c_str());
        bool isUsed               = (block.getFlags() & (DECLARE_VERTEX | DECLARE_FRAGMENT)) != 0;

        if ((block.getFlags() & LAYOUT_STD140) == 0)
            continue; // Not std140 layout.

        DE_ASSERT(refBlockNdx >= 0);

        if (cmpBlockNdx < 0)
        {
            // Not found, should it?
            if (isUsed)
            {
                log << TestLog::Message << "Error: Uniform block '" << instanceName << "' not found"
                    << TestLog::EndMessage;
                isOk = false;
            }

            continue; // Skip block.
        }

        const BlockLayoutEntry &refBlockLayout = refLayout.blocks[refBlockNdx];
        const BlockLayoutEntry &cmpBlockLayout = cmpLayout.blocks[cmpBlockNdx];

        // \todo [2012-01-24 pyry] Verify that activeUniformIndices is correct.
        // \todo [2012-01-24 pyry] Verify all instances.
        if (refBlockLayout.activeUniformIndices.size() != cmpBlockLayout.activeUniformIndices.size())
        {
            log << TestLog::Message << "Error: Number of active uniforms differ in block '" << instanceName
                << "' (expected " << refBlockLayout.activeUniformIndices.size() << ", got "
                << cmpBlockLayout.activeUniformIndices.size() << ")" << TestLog::EndMessage;
            isOk = false;
        }

        for (vector<int>::const_iterator ndxIter = refBlockLayout.activeUniformIndices.begin();
             ndxIter != refBlockLayout.activeUniformIndices.end(); ndxIter++)
        {
            const UniformLayoutEntry &refEntry = refLayout.uniforms[*ndxIter];
            int cmpEntryNdx                    = cmpLayout.getUniformIndex(refEntry.name.c_str());

            if (cmpEntryNdx < 0)
            {
                log << TestLog::Message << "Error: Uniform '" << refEntry.name << "' not found" << TestLog::EndMessage;
                isOk = false;
                continue;
            }

            const UniformLayoutEntry &cmpEntry = cmpLayout.uniforms[cmpEntryNdx];

            if (refEntry.type != cmpEntry.type || refEntry.size != cmpEntry.size ||
                refEntry.offset != cmpEntry.offset || refEntry.arrayStride != cmpEntry.arrayStride ||
                refEntry.matrixStride != cmpEntry.matrixStride || refEntry.isRowMajor != cmpEntry.isRowMajor)
            {
                log << TestLog::Message << "Error: Layout mismatch in '" << refEntry.name << "':\n"
                    << "  expected: type = " << glu::getDataTypeName(refEntry.type) << ", size = " << refEntry.size
                    << ", offset = " << refEntry.offset << ", array stride = " << refEntry.arrayStride
                    << ", matrix stride = " << refEntry.matrixStride
                    << ", row major = " << (refEntry.isRowMajor ? "true" : "false") << "\n"
                    << "  got: type = " << glu::getDataTypeName(cmpEntry.type) << ", size = " << cmpEntry.size
                    << ", offset = " << cmpEntry.offset << ", array stride = " << cmpEntry.arrayStride
                    << ", matrix stride = " << cmpEntry.matrixStride
                    << ", row major = " << (cmpEntry.isRowMajor ? "true" : "false") << TestLog::EndMessage;
                isOk = false;
            }
        }
    }

    return isOk;
}

bool UniformBlockCase::compareSharedBlocks(const UniformLayout &refLayout, const UniformLayout &cmpLayout) const
{
    TestLog &log  = m_testCtx.getLog();
    bool isOk     = true;
    int numBlocks = m_interface.getNumUniformBlocks();

    for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
    {
        const UniformBlock &block = m_interface.getUniformBlock(blockNdx);
        bool isArray              = block.isArray();
        std::string instanceName  = string(block.getBlockName()) + (isArray ? "[0]" : "");
        int refBlockNdx           = refLayout.getBlockIndex(instanceName.c_str());
        int cmpBlockNdx           = cmpLayout.getBlockIndex(instanceName.c_str());
        bool isUsed               = (block.getFlags() & (DECLARE_VERTEX | DECLARE_FRAGMENT)) != 0;

        if ((block.getFlags() & LAYOUT_SHARED) == 0)
            continue; // Not shared layout.

        DE_ASSERT(refBlockNdx >= 0);

        if (cmpBlockNdx < 0)
        {
            // Not found, should it?
            if (isUsed)
            {
                log << TestLog::Message << "Error: Uniform block '" << instanceName << "' not found"
                    << TestLog::EndMessage;
                isOk = false;
            }

            continue; // Skip block.
        }

        const BlockLayoutEntry &refBlockLayout = refLayout.blocks[refBlockNdx];
        const BlockLayoutEntry &cmpBlockLayout = cmpLayout.blocks[cmpBlockNdx];

        if (refBlockLayout.activeUniformIndices.size() != cmpBlockLayout.activeUniformIndices.size())
        {
            log << TestLog::Message << "Error: Number of active uniforms differ in block '" << instanceName
                << "' (expected " << refBlockLayout.activeUniformIndices.size() << ", got "
                << cmpBlockLayout.activeUniformIndices.size() << ")" << TestLog::EndMessage;
            isOk = false;
        }

        for (vector<int>::const_iterator ndxIter = refBlockLayout.activeUniformIndices.begin();
             ndxIter != refBlockLayout.activeUniformIndices.end(); ndxIter++)
        {
            const UniformLayoutEntry &refEntry = refLayout.uniforms[*ndxIter];
            int cmpEntryNdx                    = cmpLayout.getUniformIndex(refEntry.name.c_str());

            if (cmpEntryNdx < 0)
            {
                log << TestLog::Message << "Error: Uniform '" << refEntry.name << "' not found" << TestLog::EndMessage;
                isOk = false;
                continue;
            }

            const UniformLayoutEntry &cmpEntry = cmpLayout.uniforms[cmpEntryNdx];

            if (refEntry.type != cmpEntry.type || refEntry.size != cmpEntry.size ||
                refEntry.isRowMajor != cmpEntry.isRowMajor)
            {
                log << TestLog::Message << "Error: Layout mismatch in '" << refEntry.name << "':\n"
                    << "  expected: type = " << glu::getDataTypeName(refEntry.type) << ", size = " << refEntry.size
                    << ", row major = " << (refEntry.isRowMajor ? "true" : "false") << "\n"
                    << "  got: type = " << glu::getDataTypeName(cmpEntry.type) << ", size = " << cmpEntry.size
                    << ", row major = " << (cmpEntry.isRowMajor ? "true" : "false") << TestLog::EndMessage;
                isOk = false;
            }
        }
    }

    return isOk;
}

bool UniformBlockCase::compareTypes(const UniformLayout &refLayout, const UniformLayout &cmpLayout) const
{
    TestLog &log  = m_testCtx.getLog();
    bool isOk     = true;
    int numBlocks = m_interface.getNumUniformBlocks();

    for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
    {
        const UniformBlock &block = m_interface.getUniformBlock(blockNdx);
        bool isArray              = block.isArray();
        int numInstances          = isArray ? block.getArraySize() : 1;

        for (int instanceNdx = 0; instanceNdx < numInstances; instanceNdx++)
        {
            std::ostringstream instanceName;

            instanceName << block.getBlockName();
            if (isArray)
                instanceName << "[" << instanceNdx << "]";

            int cmpBlockNdx = cmpLayout.getBlockIndex(instanceName.str().c_str());

            if (cmpBlockNdx < 0)
                continue;

            const BlockLayoutEntry &cmpBlockLayout = cmpLayout.blocks[cmpBlockNdx];

            for (vector<int>::const_iterator ndxIter = cmpBlockLayout.activeUniformIndices.begin();
                 ndxIter != cmpBlockLayout.activeUniformIndices.end(); ndxIter++)
            {
                const UniformLayoutEntry &cmpEntry = cmpLayout.uniforms[*ndxIter];
                int refEntryNdx                    = refLayout.getUniformIndex(cmpEntry.name.c_str());

                if (refEntryNdx < 0)
                {
                    log << TestLog::Message << "Error: Uniform '" << cmpEntry.name << "' not found in reference layout"
                        << TestLog::EndMessage;
                    isOk = false;
                    continue;
                }

                const UniformLayoutEntry &refEntry = refLayout.uniforms[refEntryNdx];

                // \todo [2012-11-26 pyry] Should we check other properties as well?
                if (refEntry.type != cmpEntry.type)
                {
                    log << TestLog::Message << "Error: Uniform type mismatch in '" << refEntry.name << "':\n"
                        << "  expected: " << glu::getDataTypeName(refEntry.type) << "\n"
                        << "  got: " << glu::getDataTypeName(cmpEntry.type) << TestLog::EndMessage;
                    isOk = false;
                }
            }
        }
    }

    return isOk;
}

bool UniformBlockCase::checkLayoutIndices(const UniformLayout &layout) const
{
    TestLog &log    = m_testCtx.getLog();
    int numUniforms = (int)layout.uniforms.size();
    int numBlocks   = (int)layout.blocks.size();
    bool isOk       = true;

    // Check uniform block indices.
    for (int uniformNdx = 0; uniformNdx < numUniforms; uniformNdx++)
    {
        const UniformLayoutEntry &uniform = layout.uniforms[uniformNdx];

        if (uniform.blockNdx < 0 || !deInBounds32(uniform.blockNdx, 0, numBlocks))
        {
            log << TestLog::Message << "Error: Invalid block index in uniform '" << uniform.name << "'"
                << TestLog::EndMessage;
            isOk = false;
        }
    }

    // Check active uniforms.
    for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
    {
        const BlockLayoutEntry &block = layout.blocks[blockNdx];

        for (vector<int>::const_iterator uniformIter = block.activeUniformIndices.begin();
             uniformIter != block.activeUniformIndices.end(); uniformIter++)
        {
            if (!deInBounds32(*uniformIter, 0, numUniforms))
            {
                log << TestLog::Message << "Error: Invalid active uniform index " << *uniformIter << " in block '"
                    << block.name << "'" << TestLog::EndMessage;
                isOk = false;
            }
        }
    }

    return isOk;
}

bool UniformBlockCase::checkLayoutBounds(const UniformLayout &layout) const
{
    TestLog &log    = m_testCtx.getLog();
    int numUniforms = (int)layout.uniforms.size();
    bool isOk       = true;

    for (int uniformNdx = 0; uniformNdx < numUniforms; uniformNdx++)
    {
        const UniformLayoutEntry &uniform = layout.uniforms[uniformNdx];

        if (uniform.blockNdx < 0)
            continue;

        const BlockLayoutEntry &block = layout.blocks[uniform.blockNdx];
        bool isMatrix                 = glu::isDataTypeMatrix(uniform.type);
        int numVecs                   = isMatrix ? (uniform.isRowMajor ? glu::getDataTypeMatrixNumRows(uniform.type) :
                                                                         glu::getDataTypeMatrixNumColumns(uniform.type)) :
                                                   1;
        int numComps       = isMatrix ? (uniform.isRowMajor ? glu::getDataTypeMatrixNumColumns(uniform.type) :
                                                              glu::getDataTypeMatrixNumRows(uniform.type)) :
                                        glu::getDataTypeScalarSize(uniform.type);
        int numElements    = uniform.size;
        const int compSize = sizeof(uint32_t);
        int vecSize        = numComps * compSize;

        int minOffset = 0;
        int maxOffset = 0;

        // For negative strides.
        minOffset = de::min(minOffset, (numVecs - 1) * uniform.matrixStride);
        minOffset = de::min(minOffset, (numElements - 1) * uniform.arrayStride);
        minOffset = de::min(minOffset, (numElements - 1) * uniform.arrayStride + (numVecs - 1) * uniform.matrixStride);

        maxOffset = de::max(maxOffset, vecSize);
        maxOffset = de::max(maxOffset, (numVecs - 1) * uniform.matrixStride + vecSize);
        maxOffset = de::max(maxOffset, (numElements - 1) * uniform.arrayStride + vecSize);
        maxOffset = de::max(maxOffset,
                            (numElements - 1) * uniform.arrayStride + (numVecs - 1) * uniform.matrixStride + vecSize);

        if (uniform.offset + minOffset < 0 || uniform.offset + maxOffset > block.size)
        {
            log << TestLog::Message << "Error: Uniform '" << uniform.name << "' out of block bounds"
                << TestLog::EndMessage;
            isOk = false;
        }
    }

    return isOk;
}

bool UniformBlockCase::checkIndexQueries(uint32_t program, const UniformLayout &layout) const
{
    tcu::TestLog &log        = m_testCtx.getLog();
    const glw::Functions &gl = m_renderCtx.getFunctions();
    bool allOk               = true;

    // \note Spec mandates that uniform blocks are assigned consecutive locations from 0
    //         to ACTIVE_UNIFORM_BLOCKS. BlockLayoutEntries are stored in that order in UniformLayout.
    for (int blockNdx = 0; blockNdx < (int)layout.blocks.size(); blockNdx++)
    {
        const BlockLayoutEntry &block = layout.blocks[blockNdx];
        const int queriedNdx          = gl.getUniformBlockIndex(program, block.name.c_str());

        if (queriedNdx != blockNdx)
        {
            log << TestLog::Message << "ERROR: glGetUniformBlockIndex(" << block.name << ") returned " << queriedNdx
                << ", expected " << blockNdx << "!" << TestLog::EndMessage;
            allOk = false;
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformBlockIndex()");
    }

    return allOk;
}

bool UniformBlockCase::render(uint32_t program) const
{
    tcu::TestLog &log        = m_testCtx.getLog();
    const glw::Functions &gl = m_renderCtx.getFunctions();
    de::Random rnd(deStringHash(getName()));
    const tcu::RenderTarget &renderTarget = m_renderCtx.getRenderTarget();
    const int viewportW                   = de::min(renderTarget.getWidth(), 128);
    const int viewportH                   = de::min(renderTarget.getHeight(), 128);
    const int viewportX                   = rnd.getInt(0, renderTarget.getWidth() - viewportW);
    const int viewportY                   = rnd.getInt(0, renderTarget.getHeight() - viewportH);

    gl.clearColor(0.125f, 0.25f, 0.5f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Draw
    {
        const float position[]   = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, +1.0f, 0.0f, 1.0f,
                                    +1.0f, -1.0f, 0.0f, 1.0f, +1.0f, +1.0f, 0.0f, 1.0f};
        const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

        gl.viewport(viewportX, viewportY, viewportW, viewportH);

        glu::VertexArrayBinding posArray = glu::va::Float("a_position", 4, 4, 0, &position[0]);
        glu::draw(m_renderCtx, program, 1, &posArray, glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));
        GLU_EXPECT_NO_ERROR(gl.getError(), "Draw failed");
    }

    // Verify that all pixels are white.
    {
        tcu::Surface pixels(viewportW, viewportH);
        int numFailedPixels = 0;

        glu::readPixels(m_renderCtx, viewportX, viewportY, pixels.getAccess());
        GLU_EXPECT_NO_ERROR(gl.getError(), "Reading pixels failed");

        for (int y = 0; y < pixels.getHeight(); y++)
        {
            for (int x = 0; x < pixels.getWidth(); x++)
            {
                if (pixels.getPixel(x, y) != tcu::RGBA::white())
                    numFailedPixels += 1;
            }
        }

        if (numFailedPixels > 0)
        {
            log << TestLog::Image("Image", "Rendered image", pixels);
            log << TestLog::Message << "Image comparison failed, got " << numFailedPixels << " non-white pixels"
                << TestLog::EndMessage;
        }

        return numFailedPixels == 0;
    }
}

} // namespace gls
} // namespace deqp
