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
 * \brief Token class.
 *//*--------------------------------------------------------------------*/

#include "rsgToken.hpp"
#include "deMemory.h"
#include "deString.h"

namespace rsg
{

Token::Token(const char *identifier) : m_type(IDENTIFIER)
{
    m_arg.identifier = deStrdup(identifier);
    if (!m_arg.identifier)
        throw std::bad_alloc();
}

Token::~Token(void)
{
    if (m_type == IDENTIFIER)
        deFree(m_arg.identifier);
}

Token &Token::operator=(const Token &other)
{
    if (m_type == IDENTIFIER)
    {
        deFree(m_arg.identifier);
        m_arg.identifier = nullptr;
    }

    m_type = other.m_type;

    if (m_type == IDENTIFIER)
    {
        m_arg.identifier = deStrdup(other.m_arg.identifier);
        if (!m_arg.identifier)
            throw std::bad_alloc();
    }
    else if (m_type == FLOAT_LITERAL)
        m_arg.floatValue = other.m_arg.floatValue;
    else if (m_type == INT_LITERAL)
        m_arg.intValue = other.m_arg.intValue;
    else if (m_type == BOOL_LITERAL)
        m_arg.boolValue = other.m_arg.boolValue;

    return *this;
}

Token::Token(const Token &other) : m_type(TYPE_LAST)
{
    *this = other;
}

bool Token::operator!=(const Token &other) const
{
    if (m_type != other.m_type)
        return false;

    if (m_type == IDENTIFIER && strcmp(m_arg.identifier, other.m_arg.identifier) != 0)
        return false;
    else if (m_type == FLOAT_LITERAL && m_arg.floatValue != other.m_arg.floatValue)
        return false;
    else if (m_type == INT_LITERAL && m_arg.intValue != other.m_arg.intValue)
        return false;
    else if (m_type == BOOL_LITERAL && m_arg.boolValue != other.m_arg.boolValue)
        return false;

    return true;
}

TokenStream::TokenStream(void) : m_tokens(ALLOC_SIZE), m_numTokens(0)
{
}

TokenStream::~TokenStream(void)
{
}

} // namespace rsg
