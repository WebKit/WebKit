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
 * \brief Variable class.
 *//*--------------------------------------------------------------------*/

#include "rsgVariable.hpp"
#include "rsgGeneratorState.hpp"
#include "rsgShader.hpp"

namespace rsg
{

Variable::Variable(const VariableType &type, Storage storage, const char *name)
    : m_type(type)
    , m_storage(storage)
    , m_name(name)
    , m_layoutLocation(-1)
{
}

Variable::~Variable(void)
{
}

void Variable::tokenizeDeclaration(GeneratorState &state, TokenStream &str) const
{
    Version targetVersion = state.getProgramParameters().version;

    // \todo [2011-03-10 pyry] Remove precision hacks once precision handling is implemented
    switch (m_storage)
    {
    case STORAGE_CONST:
        str << Token::CONST;
        break;
    case STORAGE_PARAMETER_IN:
        str << Token::IN;
        break;
    case STORAGE_PARAMETER_OUT:
        str << Token::OUT;
        break;
    case STORAGE_PARAMETER_INOUT:
        str << Token::INOUT;
        break;

    case STORAGE_UNIFORM:
    {
        str << Token::UNIFORM;
        if (m_type.isFloatOrVec() || m_type.isIntOrVec())
            str << Token::MEDIUM_PRECISION;
        break;
    }

    case STORAGE_SHADER_IN:
    {
        if (targetVersion >= VERSION_300)
        {
            if (hasLayoutLocation())
                str << Token::LAYOUT << Token::LEFT_PAREN << Token::LOCATION << Token::EQUAL << m_layoutLocation
                    << Token::RIGHT_PAREN;

            str << Token::IN;

            if (state.getShader().getType() == Shader::TYPE_FRAGMENT)
                str << Token::MEDIUM_PRECISION;
        }
        else
        {
            DE_ASSERT(!hasLayoutLocation());

            switch (state.getShader().getType())
            {
            case Shader::TYPE_VERTEX:
                str << Token::ATTRIBUTE;
                break;
            case Shader::TYPE_FRAGMENT:
                str << Token::VARYING << Token::MEDIUM_PRECISION;
                break;
            default:
                DE_ASSERT(false);
                break;
            }
        }
        break;
    }

    case STORAGE_SHADER_OUT:
    {
        if (targetVersion >= VERSION_300)
        {
            if (hasLayoutLocation())
                str << Token::LAYOUT << Token::LEFT_PAREN << Token::LOCATION << Token::EQUAL << m_layoutLocation
                    << Token::RIGHT_PAREN;

            str << Token::OUT << Token::MEDIUM_PRECISION;
        }
        else
        {
            DE_ASSERT(!hasLayoutLocation());

            if (state.getShader().getType() == Shader::TYPE_VERTEX)
                str << Token::VARYING << Token::MEDIUM_PRECISION;
            else
                DE_ASSERT(false);
        }
        break;
    }

    case STORAGE_LOCAL:
    default:
        /* nothing */
        break;
    }

    m_type.tokenizeShortType(str);

    DE_ASSERT(m_name != "");
    str << Token(m_name.c_str());
}

} // namespace rsg
