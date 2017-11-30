//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderVars.cpp:
//  Methods for GL variable types (varyings, uniforms, etc)
//

#include <GLSLANG/ShaderLang.h>

#include "common/debug.h"
#include "common/utilities.h"

namespace sh
{

namespace
{

InterpolationType GetNonAuxiliaryInterpolationType(InterpolationType interpolation)
{
    return (interpolation == INTERPOLATION_CENTROID ? INTERPOLATION_SMOOTH : interpolation);
}
}
// The ES 3.0 spec is not clear on this point, but the ES 3.1 spec, and discussion
// on Khronos.org, clarifies that a smooth/flat mismatch produces a link error,
// but auxiliary qualifier mismatch (centroid) does not.
bool InterpolationTypesMatch(InterpolationType a, InterpolationType b)
{
    return (GetNonAuxiliaryInterpolationType(a) == GetNonAuxiliaryInterpolationType(b));
}

ShaderVariable::ShaderVariable()
    : type(0), precision(0), flattenedOffsetInParentArrays(0), staticUse(false)
{
}

ShaderVariable::ShaderVariable(GLenum typeIn)
    : type(typeIn), precision(0), flattenedOffsetInParentArrays(0), staticUse(false)
{
}

ShaderVariable::ShaderVariable(GLenum typeIn, unsigned int arraySizeIn)
    : type(typeIn), precision(0), flattenedOffsetInParentArrays(0), staticUse(false)
{
    ASSERT(arraySizeIn != 0);
    arraySizes.push_back(arraySizeIn);
}

ShaderVariable::~ShaderVariable()
{
}

ShaderVariable::ShaderVariable(const ShaderVariable &other)
    : type(other.type),
      precision(other.precision),
      name(other.name),
      mappedName(other.mappedName),
      arraySizes(other.arraySizes),
      flattenedOffsetInParentArrays(other.flattenedOffsetInParentArrays),
      staticUse(other.staticUse),
      fields(other.fields),
      structName(other.structName)
{
}

ShaderVariable &ShaderVariable::operator=(const ShaderVariable &other)
{
    type       = other.type;
    precision  = other.precision;
    name       = other.name;
    mappedName = other.mappedName;
    arraySizes                    = other.arraySizes;
    staticUse  = other.staticUse;
    flattenedOffsetInParentArrays = other.flattenedOffsetInParentArrays;
    fields     = other.fields;
    structName = other.structName;
    return *this;
}

bool ShaderVariable::operator==(const ShaderVariable &other) const
{
    if (type != other.type || precision != other.precision || name != other.name ||
        mappedName != other.mappedName || arraySizes != other.arraySizes ||
        staticUse != other.staticUse || fields.size() != other.fields.size() ||
        structName != other.structName)
    {
        return false;
    }
    for (size_t ii = 0; ii < fields.size(); ++ii)
    {
        if (fields[ii] != other.fields[ii])
            return false;
    }
    return true;
}

void ShaderVariable::setArraySize(unsigned int size)
{
    arraySizes.clear();
    if (size != 0)
    {
        arraySizes.push_back(size);
    }
}

unsigned int ShaderVariable::getArraySizeProduct() const
{
    return gl::ArraySizeProduct(arraySizes);
}

void ShaderVariable::indexIntoArray(unsigned int arrayIndex)
{
    ASSERT(isArray());
    flattenedOffsetInParentArrays =
        arrayIndex + getOutermostArraySize() * flattenedOffsetInParentArrays;
    arraySizes.pop_back();
}

unsigned int ShaderVariable::getNestedArraySize(unsigned int arrayNestingIndex) const
{
    ASSERT(arraySizes.size() > arrayNestingIndex);
    return arraySizes[arraySizes.size() - 1u - arrayNestingIndex];
}

unsigned int ShaderVariable::getBasicTypeElementCount() const
{
    // GLES 3.1 Nov 2016 section 7.3.1.1 page 77 specifies that a separate entry should be generated
    // for each array element when dealing with an array of arrays or an array of structs.
    ASSERT(!isArrayOfArrays());
    ASSERT(!isStruct() || !isArray());

    // GLES 3.1 Nov 2016 page 82.
    if (isArray())
    {
        return getOutermostArraySize();
    }
    return 1u;
}

bool ShaderVariable::findInfoByMappedName(const std::string &mappedFullName,
                                          const ShaderVariable **leafVar,
                                          std::string *originalFullName) const
{
    ASSERT(leafVar && originalFullName);
    // There are three cases:
    // 1) the top variable is of struct type;
    // 2) the top variable is an array;
    // 3) otherwise.
    size_t pos = mappedFullName.find_first_of(".[");

    if (pos == std::string::npos)
    {
        // Case 3.
        if (mappedFullName != this->mappedName)
            return false;
        *originalFullName = this->name;
        *leafVar          = this;
        return true;
    }
    else
    {
        std::string topName = mappedFullName.substr(0, pos);
        if (topName != this->mappedName)
            return false;
        std::string originalName = this->name;
        std::string remaining;
        if (mappedFullName[pos] == '[')
        {
            // Case 2.
            size_t closePos = mappedFullName.find_first_of(']');
            if (closePos < pos || closePos == std::string::npos)
                return false;
            // Append '[index]'.
            originalName += mappedFullName.substr(pos, closePos - pos + 1);
            if (closePos + 1 == mappedFullName.size())
            {
                *originalFullName = originalName;
                *leafVar          = this;
                return true;
            }
            else
            {
                // In the form of 'a[0].b', so after ']', '.' is expected.
                if (mappedFullName[closePos + 1] != '.')
                    return false;
                remaining = mappedFullName.substr(closePos + 2);  // Skip "]."
            }
        }
        else
        {
            // Case 1.
            remaining = mappedFullName.substr(pos + 1);  // Skip "."
        }
        for (size_t ii = 0; ii < this->fields.size(); ++ii)
        {
            const ShaderVariable *fieldVar = nullptr;
            std::string originalFieldName;
            bool found = fields[ii].findInfoByMappedName(remaining, &fieldVar, &originalFieldName);
            if (found)
            {
                *originalFullName = originalName + "." + originalFieldName;
                *leafVar          = fieldVar;
                return true;
            }
        }
        return false;
    }
}

bool ShaderVariable::isBuiltIn() const
{
    return (name.size() >= 4 && name[0] == 'g' && name[1] == 'l' && name[2] == '_');
}

bool ShaderVariable::isSameVariableAtLinkTime(const ShaderVariable &other,
                                              bool matchPrecision,
                                              bool matchName) const
{
    if (type != other.type)
        return false;
    if (matchPrecision && precision != other.precision)
        return false;
    if (matchName && name != other.name)
        return false;
    ASSERT(!matchName || mappedName == other.mappedName);
    if (arraySizes != other.arraySizes)
        return false;
    if (fields.size() != other.fields.size())
        return false;

    // [OpenGL ES 3.1 SPEC Chapter 7.4.1]
    // Variables declared as structures are considered to match in type if and only if structure
    // members match in name, type, qualification, and declaration order.
    for (size_t ii = 0; ii < fields.size(); ++ii)
    {
        if (!fields[ii].isSameVariableAtLinkTime(other.fields[ii], matchPrecision, true))
        {
            return false;
        }
    }
    if (structName != other.structName)
        return false;
    return true;
}

Uniform::Uniform() : binding(-1), offset(-1)
{
}

Uniform::~Uniform()
{
}

Uniform::Uniform(const Uniform &other)
    : VariableWithLocation(other), binding(other.binding), offset(other.offset)
{
}

Uniform &Uniform::operator=(const Uniform &other)
{
    VariableWithLocation::operator=(other);
    binding                 = other.binding;
    offset                        = other.offset;
    return *this;
}

bool Uniform::operator==(const Uniform &other) const
{
    return VariableWithLocation::operator==(other) && binding == other.binding &&
           offset == other.offset;
}

bool Uniform::isSameUniformAtLinkTime(const Uniform &other) const
{
    // Enforce a consistent match.
    // https://cvs.khronos.org/bugzilla/show_bug.cgi?id=16261
    if (binding != -1 && other.binding != -1 && binding != other.binding)
    {
        return false;
    }
    if (location != -1 && other.location != -1 && location != other.location)
    {
        return false;
    }
    if (offset != other.offset)
    {
        return false;
    }
    return VariableWithLocation::isSameVariableAtLinkTime(other, true, true);
}

VariableWithLocation::VariableWithLocation() : location(-1)
{
}

VariableWithLocation::~VariableWithLocation()
{
}

VariableWithLocation::VariableWithLocation(const VariableWithLocation &other)
    : ShaderVariable(other), location(other.location)
{
}

VariableWithLocation &VariableWithLocation::operator=(const VariableWithLocation &other)
{
    ShaderVariable::operator=(other);
    location                = other.location;
    return *this;
}

bool VariableWithLocation::operator==(const VariableWithLocation &other) const
{
    return (ShaderVariable::operator==(other) && location == other.location);
}

Attribute::Attribute()
{
}

Attribute::~Attribute()
{
}

Attribute::Attribute(const Attribute &other) : VariableWithLocation(other)
{
}

Attribute &Attribute::operator=(const Attribute &other)
{
    VariableWithLocation::operator=(other);
    return *this;
}

bool Attribute::operator==(const Attribute &other) const
{
    return VariableWithLocation::operator==(other);
}

OutputVariable::OutputVariable()
{
}

OutputVariable::~OutputVariable()
{
}

OutputVariable::OutputVariable(const OutputVariable &other) : VariableWithLocation(other)
{
}

OutputVariable &OutputVariable::operator=(const OutputVariable &other)
{
    VariableWithLocation::operator=(other);
    return *this;
}

bool OutputVariable::operator==(const OutputVariable &other) const
{
    return VariableWithLocation::operator==(other);
}

InterfaceBlockField::InterfaceBlockField() : isRowMajorLayout(false)
{
}

InterfaceBlockField::~InterfaceBlockField()
{
}

InterfaceBlockField::InterfaceBlockField(const InterfaceBlockField &other)
    : ShaderVariable(other), isRowMajorLayout(other.isRowMajorLayout)
{
}

InterfaceBlockField &InterfaceBlockField::operator=(const InterfaceBlockField &other)
{
    ShaderVariable::operator=(other);
    isRowMajorLayout        = other.isRowMajorLayout;
    return *this;
}

bool InterfaceBlockField::operator==(const InterfaceBlockField &other) const
{
    return (ShaderVariable::operator==(other) && isRowMajorLayout == other.isRowMajorLayout);
}

bool InterfaceBlockField::isSameInterfaceBlockFieldAtLinkTime(
    const InterfaceBlockField &other) const
{
    return (ShaderVariable::isSameVariableAtLinkTime(other, true, true) &&
            isRowMajorLayout == other.isRowMajorLayout);
}

Varying::Varying() : interpolation(INTERPOLATION_SMOOTH), isInvariant(false)
{
}

Varying::~Varying()
{
}

Varying::Varying(const Varying &other)
    : VariableWithLocation(other),
      interpolation(other.interpolation),
      isInvariant(other.isInvariant)
{
}

Varying &Varying::operator=(const Varying &other)
{
    VariableWithLocation::operator=(other);
    interpolation           = other.interpolation;
    isInvariant             = other.isInvariant;
    return *this;
}

bool Varying::operator==(const Varying &other) const
{
    return (VariableWithLocation::operator==(other) && interpolation == other.interpolation &&
            isInvariant == other.isInvariant);
}

bool Varying::isSameVaryingAtLinkTime(const Varying &other) const
{
    return isSameVaryingAtLinkTime(other, 100);
}

bool Varying::isSameVaryingAtLinkTime(const Varying &other, int shaderVersion) const
{
    return (ShaderVariable::isSameVariableAtLinkTime(other, false, false) &&
            InterpolationTypesMatch(interpolation, other.interpolation) &&
            (shaderVersion >= 300 || isInvariant == other.isInvariant) &&
            (location == other.location) &&
            (name == other.name || (shaderVersion >= 310 && location >= 0)));
}

InterfaceBlock::InterfaceBlock()
    : arraySize(0),
      layout(BLOCKLAYOUT_PACKED),
      isRowMajorLayout(false),
      binding(-1),
      staticUse(false),
      blockType(BlockType::BLOCK_UNIFORM)
{
}

InterfaceBlock::~InterfaceBlock()
{
}

InterfaceBlock::InterfaceBlock(const InterfaceBlock &other)
    : name(other.name),
      mappedName(other.mappedName),
      instanceName(other.instanceName),
      arraySize(other.arraySize),
      layout(other.layout),
      isRowMajorLayout(other.isRowMajorLayout),
      binding(other.binding),
      staticUse(other.staticUse),
      blockType(other.blockType),
      fields(other.fields)
{
}

InterfaceBlock &InterfaceBlock::operator=(const InterfaceBlock &other)
{
    name             = other.name;
    mappedName       = other.mappedName;
    instanceName     = other.instanceName;
    arraySize        = other.arraySize;
    layout           = other.layout;
    isRowMajorLayout = other.isRowMajorLayout;
    binding          = other.binding;
    staticUse        = other.staticUse;
    blockType        = other.blockType;
    fields           = other.fields;
    return *this;
}

std::string InterfaceBlock::fieldPrefix() const
{
    return instanceName.empty() ? "" : name;
}

std::string InterfaceBlock::fieldMappedPrefix() const
{
    return instanceName.empty() ? "" : mappedName;
}

bool InterfaceBlock::isSameInterfaceBlockAtLinkTime(const InterfaceBlock &other) const
{
    if (name != other.name || mappedName != other.mappedName || arraySize != other.arraySize ||
        layout != other.layout || isRowMajorLayout != other.isRowMajorLayout ||
        binding != other.binding || blockType != other.blockType ||
        fields.size() != other.fields.size())
    {
        return false;
    }

    for (size_t fieldIndex = 0; fieldIndex < fields.size(); ++fieldIndex)
    {
        if (!fields[fieldIndex].isSameInterfaceBlockFieldAtLinkTime(other.fields[fieldIndex]))
        {
            return false;
        }
    }

    return true;
}

bool InterfaceBlock::isBuiltIn() const
{
    return (name.size() >= 4 && name[0] == 'g' && name[1] == 'l' && name[2] == '_');
}

void WorkGroupSize::fill(int fillValue)
{
    localSizeQualifiers[0] = fillValue;
    localSizeQualifiers[1] = fillValue;
    localSizeQualifiers[2] = fillValue;
}

void WorkGroupSize::setLocalSize(int localSizeX, int localSizeY, int localSizeZ)
{
    localSizeQualifiers[0] = localSizeX;
    localSizeQualifiers[1] = localSizeY;
    localSizeQualifiers[2] = localSizeZ;
}

// check that if one of them is less than 1, then all of them are.
// Or if one is positive, then all of them are positive.
bool WorkGroupSize::isLocalSizeValid() const
{
    return (
        (localSizeQualifiers[0] < 1 && localSizeQualifiers[1] < 1 && localSizeQualifiers[2] < 1) ||
        (localSizeQualifiers[0] > 0 && localSizeQualifiers[1] > 0 && localSizeQualifiers[2] > 0));
}

bool WorkGroupSize::isAnyValueSet() const
{
    return localSizeQualifiers[0] > 0 || localSizeQualifiers[1] > 0 || localSizeQualifiers[2] > 0;
}

bool WorkGroupSize::isDeclared() const
{
    bool localSizeDeclared = localSizeQualifiers[0] > 0;
    ASSERT(isLocalSizeValid());
    return localSizeDeclared;
}

bool WorkGroupSize::isWorkGroupSizeMatching(const WorkGroupSize &right) const
{
    for (size_t i = 0u; i < size(); ++i)
    {
        bool result = (localSizeQualifiers[i] == right.localSizeQualifiers[i] ||
                       (localSizeQualifiers[i] == 1 && right.localSizeQualifiers[i] == -1) ||
                       (localSizeQualifiers[i] == -1 && right.localSizeQualifiers[i] == 1));
        if (!result)
        {
            return false;
        }
    }
    return true;
}

int &WorkGroupSize::operator[](size_t index)
{
    ASSERT(index < size());
    return localSizeQualifiers[index];
}

int WorkGroupSize::operator[](size_t index) const
{
    ASSERT(index < size());
    return localSizeQualifiers[index];
}

size_t WorkGroupSize::size() const
{
    return 3u;
}

}  // namespace sh
