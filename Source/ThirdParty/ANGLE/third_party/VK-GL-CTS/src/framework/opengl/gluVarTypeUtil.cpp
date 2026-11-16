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
 * \brief Shader variable type utilities.
 *//*--------------------------------------------------------------------*/

#include "gluVarTypeUtil.hpp"

#include <stdlib.h>

namespace glu
{

// VarTokenizer

VarTokenizer::VarTokenizer(const char *str) : m_str(str), m_token(TOKEN_LAST), m_tokenStart(0), m_tokenLen(0)
{
    advance();
}

int VarTokenizer::getNumber(void) const
{
    return atoi(getIdentifier().c_str());
}

static inline bool isNum(char c)
{
    return de::inRange(c, '0', '9');
}
static inline bool isAlpha(char c)
{
    return de::inRange(c, 'a', 'z') || de::inRange(c, 'A', 'Z');
}
static inline bool isIdentifierChar(char c)
{
    return isAlpha(c) || isNum(c) || c == '_';
}

void VarTokenizer::advance(void)
{
    DE_ASSERT(m_token != TOKEN_END);

    m_tokenStart += m_tokenLen;
    m_token    = TOKEN_LAST;
    m_tokenLen = 1;

    if (m_str[m_tokenStart] == '[')
        m_token = TOKEN_LEFT_BRACKET;
    else if (m_str[m_tokenStart] == ']')
        m_token = TOKEN_RIGHT_BRACKET;
    else if (m_str[m_tokenStart] == 0)
        m_token = TOKEN_END;
    else if (m_str[m_tokenStart] == '.')
        m_token = TOKEN_PERIOD;
    else if (isNum(m_str[m_tokenStart]))
    {
        m_token = TOKEN_NUMBER;
        while (isNum(m_str[m_tokenStart + m_tokenLen]))
            m_tokenLen += 1;
    }
    else if (isIdentifierChar(m_str[m_tokenStart]))
    {
        m_token = TOKEN_IDENTIFIER;
        while (isIdentifierChar(m_str[m_tokenStart + m_tokenLen]))
            m_tokenLen += 1;
    }
    else
        TCU_FAIL("Unexpected character");
}

// SubTypeAccess

SubTypeAccess::SubTypeAccess(const VarType &type) : m_type(type)
{
}

std::string parseVariableName(const char *nameWithPath)
{
    VarTokenizer tokenizer(nameWithPath);
    TCU_CHECK(tokenizer.getToken() == VarTokenizer::TOKEN_IDENTIFIER);
    return tokenizer.getIdentifier();
}

void parseTypePath(const char *nameWithPath, const VarType &type, TypeComponentVector &path)
{
    VarTokenizer tokenizer(nameWithPath);

    if (tokenizer.getToken() == VarTokenizer::TOKEN_IDENTIFIER)
        tokenizer.advance();

    path.clear();
    while (tokenizer.getToken() != VarTokenizer::TOKEN_END)
    {
        VarType curType = getVarType(type, path);

        if (tokenizer.getToken() == VarTokenizer::TOKEN_PERIOD)
        {
            tokenizer.advance();
            TCU_CHECK(tokenizer.getToken() == VarTokenizer::TOKEN_IDENTIFIER);
            TCU_CHECK_MSG(curType.isStructType(), "Invalid field selector");

            // Find member.
            std::string memberName = tokenizer.getIdentifier();
            int ndx                = 0;
            for (; ndx < curType.getStructPtr()->getNumMembers(); ndx++)
            {
                if (memberName == curType.getStructPtr()->getMember(ndx).getName())
                    break;
            }
            TCU_CHECK_MSG(ndx < curType.getStructPtr()->getNumMembers(), "Member not found in type");

            path.push_back(VarTypeComponent(VarTypeComponent::STRUCT_MEMBER, ndx));
            tokenizer.advance();
        }
        else if (tokenizer.getToken() == VarTokenizer::TOKEN_LEFT_BRACKET)
        {
            tokenizer.advance();
            TCU_CHECK(tokenizer.getToken() == VarTokenizer::TOKEN_NUMBER);

            int ndx = tokenizer.getNumber();

            if (curType.isArrayType())
            {
                TCU_CHECK(de::inBounds(ndx, 0, curType.getArraySize()));
                path.push_back(VarTypeComponent(VarTypeComponent::ARRAY_ELEMENT, ndx));
            }
            else if (curType.isBasicType() && isDataTypeMatrix(curType.getBasicType()))
            {
                TCU_CHECK(de::inBounds(ndx, 0, getDataTypeMatrixNumColumns(curType.getBasicType())));
                path.push_back(VarTypeComponent(VarTypeComponent::MATRIX_COLUMN, ndx));
            }
            else if (curType.isBasicType() && isDataTypeVector(curType.getBasicType()))
            {
                TCU_CHECK(de::inBounds(ndx, 0, getDataTypeScalarSize(curType.getBasicType())));
                path.push_back(VarTypeComponent(VarTypeComponent::VECTOR_COMPONENT, ndx));
            }
            else
                TCU_FAIL("Invalid subscript");

            tokenizer.advance();
            TCU_CHECK(tokenizer.getToken() == VarTokenizer::TOKEN_RIGHT_BRACKET);
            tokenizer.advance();
        }
        else
            TCU_FAIL("Unexpected token");
    }
}

std::ostream &operator<<(std::ostream &str, const TypeAccessFormat &format)
{
    const VarType *curType = &format.type;

    for (TypeComponentVector::const_iterator iter = format.path.begin(); iter != format.path.end(); iter++)
    {
        switch (iter->type)
        {
        case VarTypeComponent::ARRAY_ELEMENT:
            curType = &curType->getElementType(); // Update current type.
            // Fall-through.

        case VarTypeComponent::MATRIX_COLUMN:
        case VarTypeComponent::VECTOR_COMPONENT:
            str << "[" << iter->index << "]";
            break;

        case VarTypeComponent::STRUCT_MEMBER:
        {
            const StructMember &member = curType->getStructPtr()->getMember(iter->index);
            str << "." << member.getName();
            curType = &member.getType();
            break;
        }

        default:
            DE_ASSERT(false);
        }
    }

    return str;
}

} // namespace glu
