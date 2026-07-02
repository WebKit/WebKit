#ifndef _GLUVARTYPE_HPP
#define _GLUVARTYPE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief Shader variable type.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluShaderUtil.hpp"

#include <vector>
#include <string>
#include <ostream>

namespace glu
{

class StructType;

/*--------------------------------------------------------------------*//*!
 * \brief Shader variable type.
 *
 * Variable type represents data type. No storage qualifiers are supported
 * since they are associated to a declaration, not to the variable type.
 *
 * \note Structs are handled using struct pointers since it is often desirable
 *         to maintain unique list of struct declarations.
 *//*--------------------------------------------------------------------*/
class VarType
{
public:
    VarType(void);
    VarType(const VarType &other);

    VarType(DataType basicType, Precision precision);   //!< Basic type constructor.
    VarType(const VarType &elementType, int arraySize); //!< Array type constructor.
    explicit VarType(const StructType *structPtr);      //!< Struct type constructor.
    ~VarType(void);

    bool isBasicType(void) const
    {
        return m_type == VARTYPE_BASIC;
    }
    bool isArrayType(void) const
    {
        return m_type == VARTYPE_ARRAY;
    }
    bool isStructType(void) const
    {
        return m_type == VARTYPE_STRUCT;
    }

    DataType getBasicType(void) const
    {
        DE_ASSERT(isBasicType());
        return m_data.basic.type;
    }
    Precision getPrecision(void) const
    {
        DE_ASSERT(isBasicType());
        return m_data.basic.precision;
    }

    const VarType &getElementType(void) const
    {
        DE_ASSERT(isArrayType());
        return *m_data.array.elementType;
    }
    int getArraySize(void) const
    {
        DE_ASSERT(isArrayType());
        return m_data.array.size;
    }

    const StructType *getStructPtr(void) const
    {
        DE_ASSERT(isStructType());
        return m_data.structPtr;
    }

    int getScalarSize(void) const;

    VarType &operator=(const VarType &other);

    bool operator==(const VarType &other) const;
    bool operator!=(const VarType &other) const;

    enum
    {
        UNSIZED_ARRAY = -1 //!< Array length for unsized arrays.
    };

private:
    enum Type
    {
        VARTYPE_BASIC,
        VARTYPE_ARRAY,
        VARTYPE_STRUCT,

        VARTYPE_LAST
    };

    Type m_type;
    union Data
    {
        // TYPE_BASIC
        struct
        {
            DataType type;
            Precision precision;
        } basic;

        // TYPE_ARRAY
        struct
        {
            VarType *elementType;
            int size;
        } array;

        // TYPE_STRUCT
        const StructType *structPtr;

        Data(void)
        {
            array.elementType = nullptr;
            array.size        = 0;
        }
    } m_data;
} DE_WARN_UNUSED_TYPE;

template <typename T>
inline VarType varTypeOf(Precision prec = PRECISION_LAST)
{
    return VarType(dataTypeOf<T>(), prec);
}

class StructMember
{
public:
    StructMember(const char *name, const VarType &type) : m_name(name), m_type(type)
    {
    }
    StructMember(void)
    {
    }

    const char *getName(void) const
    {
        return m_name.c_str();
    }
    const VarType &getType(void) const
    {
        return m_type;
    }

    bool operator==(const StructMember &other) const;
    bool operator!=(const StructMember &other) const;

private:
    std::string m_name;
    VarType m_type;
} DE_WARN_UNUSED_TYPE;

class StructType
{
public:
    typedef std::vector<StructMember>::iterator Iterator;
    typedef std::vector<StructMember>::const_iterator ConstIterator;

    StructType(const char *typeName) : m_typeName(typeName)
    {
    }
    ~StructType(void)
    {
    }

    bool hasTypeName(void) const
    {
        return !m_typeName.empty();
    }
    const char *getTypeName(void) const
    {
        return hasTypeName() ? m_typeName.c_str() : nullptr;
    }

    void addMember(const char *name, const VarType &type);

    int getNumMembers(void) const
    {
        return (int)m_members.size();
    }
    const StructMember &getMember(int64_t ndx) const
    {
        return m_members[(size_t)ndx];
    }

    inline Iterator begin(void)
    {
        return m_members.begin();
    }
    inline ConstIterator begin(void) const
    {
        return m_members.begin();
    }
    inline Iterator end(void)
    {
        return m_members.end();
    }
    inline ConstIterator end(void) const
    {
        return m_members.end();
    }

    bool operator==(const StructType &other) const;
    bool operator!=(const StructType &other) const;

private:
    std::string m_typeName;
    std::vector<StructMember> m_members;
} DE_WARN_UNUSED_TYPE;

enum Storage
{
    STORAGE_IN = 0,
    STORAGE_OUT,
    STORAGE_CONST,
    STORAGE_UNIFORM,
    STORAGE_BUFFER,
    STORAGE_PATCH_IN,
    STORAGE_PATCH_OUT,
    STORAGE_LAST
};

const char *getStorageName(Storage storage);

enum Interpolation
{
    INTERPOLATION_SMOOTH = 0,
    INTERPOLATION_FLAT,
    INTERPOLATION_CENTROID,
    INTERPOLATION_LAST
};

const char *getInterpolationName(Interpolation interpolation);

enum FormatLayout
{
    FORMATLAYOUT_RGBA32F = 0,
    FORMATLAYOUT_RGBA16F,
    FORMATLAYOUT_R32F,
    FORMATLAYOUT_RGBA8,
    FORMATLAYOUT_RGBA8_SNORM,

    FORMATLAYOUT_RGBA32I,
    FORMATLAYOUT_RGBA16I,
    FORMATLAYOUT_RGBA8I,
    FORMATLAYOUT_R32I,

    FORMATLAYOUT_RGBA32UI,
    FORMATLAYOUT_RGBA16UI,
    FORMATLAYOUT_RGBA8UI,
    FORMATLAYOUT_R32UI,

    FORMATLAYOUT_LAST
};

const char *getFormatLayoutName(FormatLayout layout);

enum MemoryAccessQualifier
{
    MEMORYACCESSQUALIFIER_COHERENT_BIT  = 0x01,
    MEMORYACCESSQUALIFIER_VOLATILE_BIT  = 0x02,
    MEMORYACCESSQUALIFIER_RESTRICT_BIT  = 0x04,
    MEMORYACCESSQUALIFIER_READONLY_BIT  = 0x08,
    MEMORYACCESSQUALIFIER_WRITEONLY_BIT = 0x10,

    MEMORYACCESSQUALIFIER_MASK = (MEMORYACCESSQUALIFIER_WRITEONLY_BIT << 1) - 1
};

const char *getMemoryAccessQualifierName(MemoryAccessQualifier qualifier);

enum MatrixOrder
{
    MATRIXORDER_COLUMN_MAJOR = 0,
    MATRIXORDER_ROW_MAJOR,

    MATRIXORDER_LAST
};

const char *getMatrixOrderName(MatrixOrder qualifier);

// Declaration utilities.

struct Layout
{
    Layout(int location_ = -1, int binding_ = -1, int offset_ = -1, FormatLayout format_ = FORMATLAYOUT_LAST,
           MatrixOrder matrixOrder_ = MATRIXORDER_LAST);

    bool operator==(const Layout &other) const;
    bool operator!=(const Layout &other) const;

    int location;
    int binding;
    int offset;
    FormatLayout format;
    MatrixOrder matrixOrder;
} DE_WARN_UNUSED_TYPE;

struct VariableDeclaration
{
    VariableDeclaration(const VarType &varType_, const std::string &name_, Storage storage_ = STORAGE_LAST,
                        Interpolation interpolation_ = INTERPOLATION_LAST, const Layout &layout_ = Layout(),
                        uint32_t memoryAccessQualifierBits_ = 0);

    bool operator==(const VariableDeclaration &other) const;
    bool operator!=(const VariableDeclaration &other) const;

    Layout layout;
    Interpolation interpolation;
    Storage storage;
    VarType varType;
    uint32_t memoryAccessQualifierBits;
    std::string name;
} DE_WARN_UNUSED_TYPE;

struct InterfaceBlock
{
    InterfaceBlock(void);

    glu::Layout layout;
    Storage storage;
    int memoryAccessQualifierFlags;
    std::string interfaceName;
    std::string instanceName;
    std::vector<glu::VariableDeclaration> variables;
    std::vector<int> dimensions;
} DE_WARN_UNUSED_TYPE;

//! Internals for declare() utilities.
namespace decl
{

struct Indent
{
    int level;
    Indent(int level_) : level(level_)
    {
    }
};

struct DeclareStructTypePtr
{
    DeclareStructTypePtr(const StructType *structPtr_, int indentLevel_)
        : structPtr(structPtr_)
        , indentLevel(indentLevel_)
    {
    }

    const StructType *structPtr;
    int indentLevel;
};

struct DeclareStructType
{
    DeclareStructType(const StructType &structType_, int indentLevel_)
        : structType(structType_)
        , indentLevel(indentLevel_)
    {
    }

    StructType structType;
    int indentLevel;
};

struct DeclareVariable
{
    DeclareVariable(const VarType &varType_, const std::string &name_, int indentLevel_)
        : varType(varType_)
        , name(name_)
        , indentLevel(indentLevel_)
    {
    }

    VarType varType;
    std::string name;
    int indentLevel;
};

std::ostream &operator<<(std::ostream &str, const Indent &indent);
std::ostream &operator<<(std::ostream &str, const DeclareStructTypePtr &decl);
std::ostream &operator<<(std::ostream &str, const DeclareStructType &decl);
std::ostream &operator<<(std::ostream &str, const DeclareVariable &decl);

} // namespace decl

inline decl::Indent indent(int indentLevel)
{
    return decl::Indent(indentLevel);
}
inline decl::DeclareStructTypePtr declare(const StructType *structPtr, int indentLevel = 0)
{
    return decl::DeclareStructTypePtr(structPtr, indentLevel);
}
inline decl::DeclareStructType declare(const StructType &structType, int indentLevel = 0)
{
    return decl::DeclareStructType(structType, indentLevel);
}
inline decl::DeclareVariable declare(const VarType &varType, const std::string &name, int indentLevel = 0)
{
    return decl::DeclareVariable(varType, name, indentLevel);
}

std::ostream &operator<<(std::ostream &str, const Layout &decl);
std::ostream &operator<<(std::ostream &str, const VariableDeclaration &decl);

} // namespace glu

#endif // _GLUVARTYPE_HPP
