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

#include "gluVarType.hpp"
#include "deStringUtil.hpp"
#include "deArrayUtil.hpp"

namespace glu
{

VarType::VarType(void) : m_type(VARTYPE_LAST)
{
}

VarType::VarType(const VarType &other) : m_type(VARTYPE_LAST)
{
    *this = other;
}

VarType::VarType(DataType basicType, Precision precision) : m_type(VARTYPE_BASIC)
{
    m_data.basic.type      = basicType;
    m_data.basic.precision = precision;
}

VarType::VarType(const VarType &elementType, int arraySize) : m_type(VARTYPE_ARRAY)
{
    DE_ASSERT(arraySize >= 0 || arraySize == UNSIZED_ARRAY);
    m_data.array.size        = arraySize;
    m_data.array.elementType = new VarType(elementType);
}

VarType::VarType(const StructType *structPtr) : m_type(VARTYPE_STRUCT)
{
    m_data.structPtr = structPtr;
}

VarType::~VarType(void)
{
    if (m_type == VARTYPE_ARRAY)
        delete m_data.array.elementType;
}

VarType &VarType::operator=(const VarType &other)
{
    if (this == &other)
        return *this; // Self-assignment.

    VarType *oldElementType = m_type == VARTYPE_ARRAY ? m_data.array.elementType : nullptr;

    m_type = other.m_type;
    m_data = Data();

    if (m_type == VARTYPE_ARRAY)
    {
        m_data.array.elementType = new VarType(*other.m_data.array.elementType);
        m_data.array.size        = other.m_data.array.size;
    }
    else
        m_data = other.m_data;

    delete oldElementType;

    return *this;
}

int VarType::getScalarSize(void) const
{
    switch (m_type)
    {
    case VARTYPE_BASIC:
        return glu::getDataTypeScalarSize(m_data.basic.type);
    case VARTYPE_ARRAY:
        return m_data.array.elementType->getScalarSize() * m_data.array.size;

    case VARTYPE_STRUCT:
    {
        int size = 0;
        for (StructType::ConstIterator iter = m_data.structPtr->begin(); iter != m_data.structPtr->end(); iter++)
            size += iter->getType().getScalarSize();
        return size;
    }

    default:
        DE_ASSERT(false);
        return 0;
    }
}

bool VarType::operator==(const VarType &other) const
{
    if (m_type != other.m_type)
        return false;

    switch (m_type)
    {
    case VARTYPE_BASIC:
        return m_data.basic.type == other.m_data.basic.type && m_data.basic.precision == other.m_data.basic.precision;

    case VARTYPE_ARRAY:
        return *m_data.array.elementType == *other.m_data.array.elementType &&
               m_data.array.size == other.m_data.array.size;

    case VARTYPE_STRUCT:
        return m_data.structPtr == other.m_data.structPtr;

    default:
        DE_ASSERT(false);
        return 0;
    }
}

bool VarType::operator!=(const VarType &other) const
{
    return !(*this == other);
}

// StructMember implementation

bool StructMember::operator==(const StructMember &other) const
{
    return (m_name == other.m_name) && (m_type == other.m_type);
}

bool StructMember::operator!=(const StructMember &other) const
{
    return !(*this == other);
}

// StructType implementation.

void StructType::addMember(const char *name, const VarType &type)
{
    m_members.push_back(StructMember(name, type));
}

bool StructType::operator==(const StructType &other) const
{
    return (m_typeName == other.m_typeName) && (m_members == other.m_members);
}

bool StructType::operator!=(const StructType &other) const
{
    return !(*this == other);
}

const char *getStorageName(Storage storage)
{
    static const char *const s_names[] = {"in", "out", "const", "uniform", "buffer", "patch in", "patch out"};

    return de::getSizedArrayElement<STORAGE_LAST>(s_names, storage);
}

const char *getInterpolationName(Interpolation interpolation)
{
    static const char *const s_names[] = {"smooth", "flat", "centroid"};

    return de::getSizedArrayElement<INTERPOLATION_LAST>(s_names, interpolation);
}

const char *getFormatLayoutName(FormatLayout layout)
{
    static const char *s_names[] = {
        "rgba32f",     // FORMATLAYOUT_RGBA32F
        "rgba16f",     // FORMATLAYOUT_RGBA16F
        "r32f",        // FORMATLAYOUT_R32F
        "rgba8",       // FORMATLAYOUT_RGBA8
        "rgba8_snorm", // FORMATLAYOUT_RGBA8_SNORM
        "rgba32i",     // FORMATLAYOUT_RGBA32I
        "rgba16i",     // FORMATLAYOUT_RGBA16I
        "rgba8i",      // FORMATLAYOUT_RGBA8I
        "r32i",        // FORMATLAYOUT_R32I
        "rgba32ui",    // FORMATLAYOUT_RGBA32UI
        "rgba16ui",    // FORMATLAYOUT_RGBA16UI
        "rgba8ui",     // FORMATLAYOUT_RGBA8UI
        "r32ui",       // FORMATLAYOUT_R32UI
    };

    return de::getSizedArrayElement<FORMATLAYOUT_LAST>(s_names, layout);
}

const char *getMemoryAccessQualifierName(MemoryAccessQualifier qualifier)
{
    switch (qualifier)
    {
    case MEMORYACCESSQUALIFIER_COHERENT_BIT:
        return "coherent";
    case MEMORYACCESSQUALIFIER_VOLATILE_BIT:
        return "volatile";
    case MEMORYACCESSQUALIFIER_RESTRICT_BIT:
        return "restrict";
    case MEMORYACCESSQUALIFIER_READONLY_BIT:
        return "readonly";
    case MEMORYACCESSQUALIFIER_WRITEONLY_BIT:
        return "writeonly";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

const char *getMatrixOrderName(MatrixOrder qualifier)
{
    static const char *s_names[] = {
        "column_major", // MATRIXORDER_COLUMN_MAJOR
        "row_major",    // MATRIXORDER_ROW_MAJOR
    };

    return de::getSizedArrayElement<MATRIXORDER_LAST>(s_names, qualifier);
}

// Layout Implementation

Layout::Layout(int location_, int binding_, int offset_, FormatLayout format_, MatrixOrder matrixOrder_)
    : location(location_)
    , binding(binding_)
    , offset(offset_)
    , format(format_)
    , matrixOrder(matrixOrder_)
{
}

bool Layout::operator==(const Layout &other) const
{
    return location == other.location && binding == other.binding && offset == other.offset && format == other.format &&
           matrixOrder == other.matrixOrder;
}

bool Layout::operator!=(const Layout &other) const
{
    return !(*this == other);
}

// VariableDeclaration Implementation

VariableDeclaration::VariableDeclaration(const VarType &varType_, const std::string &name_, Storage storage_,
                                         Interpolation interpolation_, const Layout &layout_,
                                         uint32_t memoryAccessQualifierBits_)
    : layout(layout_)
    , interpolation(interpolation_)
    , storage(storage_)
    , varType(varType_)
    , memoryAccessQualifierBits(memoryAccessQualifierBits_)
    , name(name_)
{
}

bool VariableDeclaration::operator==(const VariableDeclaration &other) const
{
    return layout == other.layout && interpolation == other.interpolation && storage == other.storage &&
           varType == other.varType && memoryAccessQualifierBits == other.memoryAccessQualifierBits &&
           name == other.name;
}

bool VariableDeclaration::operator!=(const VariableDeclaration &other) const
{
    return !(*this == other);
}

// InterfaceBlock Implementation

InterfaceBlock::InterfaceBlock(void) : layout(Layout()), storage(glu::STORAGE_LAST), memoryAccessQualifierFlags(0)
{
}

// Declaration utilties.

std::ostream &operator<<(std::ostream &str, const Layout &layout)
{
    std::vector<std::string> layoutDeclarationList;

    if (layout.location != -1)
        layoutDeclarationList.push_back("location=" + de::toString(layout.location));

    if (layout.binding != -1)
        layoutDeclarationList.push_back("binding=" + de::toString(layout.binding));

    if (layout.offset != -1)
        layoutDeclarationList.push_back("offset=" + de::toString(layout.offset));

    if (layout.format != FORMATLAYOUT_LAST)
        layoutDeclarationList.push_back(getFormatLayoutName(layout.format));

    if (layout.matrixOrder != MATRIXORDER_LAST)
        layoutDeclarationList.push_back(getMatrixOrderName(layout.matrixOrder));

    if (!layoutDeclarationList.empty())
    {
        str << "layout(" << layoutDeclarationList[0];

        for (int layoutNdx = 1; layoutNdx < (int)layoutDeclarationList.size(); ++layoutNdx)
            str << ", " << layoutDeclarationList[layoutNdx];

        str << ")";
    }

    return str;
}

std::ostream &operator<<(std::ostream &str, const VariableDeclaration &decl)
{
    if (decl.layout != Layout())
        str << decl.layout << " ";

    for (int bitNdx = 0; (1 << bitNdx) & MEMORYACCESSQUALIFIER_MASK; ++bitNdx)
        if (decl.memoryAccessQualifierBits & (1 << bitNdx))
            str << getMemoryAccessQualifierName((glu::MemoryAccessQualifier)(1 << bitNdx)) << " ";

    if (decl.interpolation != INTERPOLATION_LAST)
        str << getInterpolationName(decl.interpolation) << " ";

    if (decl.storage != STORAGE_LAST)
        str << getStorageName(decl.storage) << " ";

    str << declare(decl.varType, decl.name);

    return str;
}

namespace decl
{

std::ostream &operator<<(std::ostream &str, const Indent &indent)
{
    for (int i = 0; i < indent.level; i++)
        str << "\t";
    return str;
}

std::ostream &operator<<(std::ostream &str, const DeclareVariable &decl)
{
    const VarType &type    = decl.varType;
    const VarType *curType = &type;
    std::vector<int> arraySizes;

    // Handle arrays.
    while (curType->isArrayType())
    {
        arraySizes.push_back(curType->getArraySize());
        curType = &curType->getElementType();
    }

    if (curType->isBasicType())
    {
        if (curType->getPrecision() != PRECISION_LAST && !glu::isDataTypeFloat16OrVec(curType->getBasicType()))
            str << glu::getPrecisionName(curType->getPrecision()) << " ";
        str << glu::getDataTypeName(curType->getBasicType());
    }
    else if (curType->isStructType())
    {
        const StructType *structPtr = curType->getStructPtr();

        if (structPtr->hasTypeName())
            str << structPtr->getTypeName();
        else
            str << declare(structPtr, decl.indentLevel); // Generate inline declaration.
    }
    else
        DE_ASSERT(false);

    str << " " << decl.name;

    // Print array sizes.
    for (std::vector<int>::const_iterator sizeIter = arraySizes.begin(); sizeIter != arraySizes.end(); sizeIter++)
    {
        const int arrSize = *sizeIter;
        if (arrSize == VarType::UNSIZED_ARRAY)
            str << "[]";
        else
            str << "[" << arrSize << "]";
    }

    return str;
}

std::ostream &operator<<(std::ostream &str, const DeclareStructTypePtr &decl)
{
    str << "struct";

    // Type name is optional.
    if (decl.structPtr->hasTypeName())
        str << " " << decl.structPtr->getTypeName();

    str << "\n" << indent(decl.indentLevel) << "{\n";

    for (StructType::ConstIterator memberIter = decl.structPtr->begin(); memberIter != decl.structPtr->end();
         memberIter++)
    {
        str << indent(decl.indentLevel + 1);
        str << declare(memberIter->getType(), memberIter->getName(), decl.indentLevel + 1) << ";\n";
    }

    str << indent(decl.indentLevel) << "}";

    return str;
}

std::ostream &operator<<(std::ostream &str, const DeclareStructType &decl)
{
    return str << declare(&decl.structType, decl.indentLevel);
}

} // namespace decl
} // namespace glu
