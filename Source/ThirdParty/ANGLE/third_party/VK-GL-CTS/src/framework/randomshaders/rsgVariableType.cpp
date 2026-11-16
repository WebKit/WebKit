/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
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
 * \brief Variable Type class.
 *//*--------------------------------------------------------------------*/

#include "rsgVariableType.hpp"
#include "rsgToken.hpp"

using std::vector;

namespace rsg
{

VariableType &VariableType::operator=(const VariableType &other)
{
    if (this == &other)
        return *this;

    delete m_elementType;

    m_elementType = nullptr;
    m_baseType    = other.m_baseType;
    m_precision   = other.m_precision;
    m_typeName    = other.m_typeName;
    m_numElements = other.m_numElements;
    m_members     = other.m_members;
    m_elementType = nullptr;

    if (other.m_elementType)
        m_elementType = new VariableType(*other.m_elementType);

    return *this;
}

VariableType::VariableType(const VariableType &other) : m_elementType(nullptr)
{
    *this = other;
}

bool VariableType::operator!=(const VariableType &other) const
{
    if (m_baseType != other.m_baseType)
        return true;
    if (m_precision != other.m_precision)
        return true;
    if (m_numElements != other.m_numElements)
        return true;
    if (!!m_elementType != !!other.m_elementType)
        return true;
    if (m_elementType && *m_elementType != *other.m_elementType)
        return true;
    if (m_members != other.m_members)
        return true;
    return false;
}

bool VariableType::operator==(const VariableType &other) const
{
    return !(*this != other);
}

int VariableType::getScalarSize(void) const
{
    switch (m_baseType)
    {
    case TYPE_VOID:
    case TYPE_FLOAT:
    case TYPE_INT:
    case TYPE_BOOL:
    case TYPE_SAMPLER_2D:
    case TYPE_SAMPLER_CUBE:
        return m_numElements;

    case TYPE_STRUCT:
    {
        int sum = 0;
        for (vector<Member>::const_iterator i = m_members.begin(); i != m_members.end(); i++)
            sum += i->getType().getScalarSize();
        return sum;
    }

    case TYPE_ARRAY:
    {
        DE_ASSERT(m_elementType);
        return m_elementType->getScalarSize() * m_numElements;
    }

    default:
        DE_ASSERT(false);
        return 0;
    }
}

int VariableType::getMemberScalarOffset(int memberNdx) const
{
    DE_ASSERT(isStruct());

    int curOffset = 0;
    for (vector<Member>::const_iterator i = m_members.begin(); i != m_members.begin() + memberNdx; i++)
        curOffset += i->getType().getScalarSize();

    return curOffset;
}

int VariableType::getElementScalarOffset(int elementNdx) const
{
    DE_ASSERT(isArray());
    return elementNdx * getElementType().getScalarSize();
}

const VariableType &VariableType::getScalarType(Type baseType)
{
    switch (baseType)
    {
    case TYPE_FLOAT:
    {
        static const VariableType s_floatTypes[] = {
            VariableType(TYPE_FLOAT, 1)
            // \todo [pyry] Extend with different precision variants?
        };
        return s_floatTypes[0];
    }

    case TYPE_INT:
    {
        static const VariableType s_intTypes[] = {VariableType(TYPE_INT, 1)};
        return s_intTypes[0];
    }

    case TYPE_BOOL:
    {
        static const VariableType s_boolTypes[] = {VariableType(TYPE_BOOL, 1)};
        return s_boolTypes[0];
    }

    case TYPE_SAMPLER_2D:
    {
        static const VariableType sampler2DType = VariableType(TYPE_SAMPLER_2D, 1);
        return sampler2DType;
    }

    case TYPE_SAMPLER_CUBE:
    {
        static const VariableType samplerCubeType = VariableType(TYPE_SAMPLER_CUBE, 1);
        return samplerCubeType;
    }

    default:
        DE_ASSERT(false);
        throw Exception("VariableType::getScalarType(): unsupported type");
    }
}

const VariableType &VariableType::getElementType(void) const
{
    DE_ASSERT(m_precision == PRECISION_NONE); // \todo [pyry] Precision
    switch (m_baseType)
    {
    case TYPE_FLOAT:
    case TYPE_INT:
    case TYPE_BOOL:
    case TYPE_SAMPLER_2D:
    case TYPE_SAMPLER_CUBE:
        return getScalarType(m_baseType);

    case TYPE_ARRAY:
    {
        DE_ASSERT(m_elementType);
        return *m_elementType;
    }

    default:
        DE_ASSERT(false);
        throw Exception("VariableType::getElementType(): unsupported type");
    }
}

void VariableType::tokenizeShortType(TokenStream &str) const
{
    switch (m_precision)
    {
    case PRECISION_LOW:
        str << Token::LOW_PRECISION;
        break;
    case PRECISION_MEDIUM:
        str << Token::MEDIUM_PRECISION;
        break;
    case PRECISION_HIGH:
        str << Token::HIGH_PRECISION;
        break;
    default: /* nothing */
        break;
    }

    switch (m_baseType)
    {
    case TYPE_VOID:
        str << Token::VOID;
        break;

    case TYPE_FLOAT:
        switch (m_numElements)
        {
        case 1:
            str << Token::FLOAT;
            break;
        case 2:
            str << Token::VEC2;
            break;
        case 3:
            str << Token::VEC3;
            break;
        case 4:
            str << Token::VEC4;
            break;
        default:
            DE_ASSERT(false);
            break;
        }
        break;

    case TYPE_INT:
        switch (m_numElements)
        {
        case 1:
            str << Token::INT;
            break;
        case 2:
            str << Token::IVEC2;
            break;
        case 3:
            str << Token::IVEC3;
            break;
        case 4:
            str << Token::IVEC4;
            break;
        default:
            DE_ASSERT(false);
            break;
        }
        break;

    case TYPE_BOOL:
        switch (m_numElements)
        {
        case 1:
            str << Token::BOOL;
            break;
        case 2:
            str << Token::BVEC2;
            break;
        case 3:
            str << Token::BVEC3;
            break;
        case 4:
            str << Token::BVEC4;
            break;
        default:
            DE_ASSERT(false);
            break;
        }
        break;

    case TYPE_SAMPLER_2D:
        str << Token::SAMPLER2D;
        break;
    case TYPE_SAMPLER_CUBE:
        str << Token::SAMPLERCUBE;
        break;

    case TYPE_STRUCT:
        DE_ASSERT(m_typeName != "");
        str << Token(m_typeName.c_str());
        break;

    case TYPE_ARRAY:
        DE_ASSERT(m_elementType);
        m_elementType->tokenizeShortType(str);
        str << Token::LEFT_BRACKET << Token(m_numElements) << Token::RIGHT_BRACKET;
        break;

    default:
        DE_ASSERT(false);
        break;
    }
}

} // namespace rsg
