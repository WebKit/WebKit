#ifndef _RSGVARIABLE_HPP
#define _RSGVARIABLE_HPP
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

#include "rsgDefs.hpp"
#include "rsgVariableType.hpp"
#include "rsgToken.hpp"

#include <string>

namespace rsg
{

class GeneratorState;

class Variable
{
public:
    enum Storage
    {
        STORAGE_LOCAL,
        STORAGE_SHADER_IN,
        STORAGE_SHADER_OUT,
        STORAGE_UNIFORM,
        STORAGE_CONST,
        STORAGE_PARAMETER_IN,
        STORAGE_PARAMETER_OUT,
        STORAGE_PARAMETER_INOUT,

        STORAGE_LAST
    };

    Variable(const VariableType &type, Storage storage, const char *name);
    ~Variable(void);

    const VariableType &getType(void) const
    {
        return m_type;
    }
    Storage getStorage(void) const
    {
        return m_storage;
    }
    const char *getName(void) const
    {
        return m_name.c_str();
    }
    int getLayoutLocation(void) const
    {
        return m_layoutLocation;
    }
    bool hasLayoutLocation(void) const
    {
        return m_layoutLocation >= 0;
    }

    void setStorage(Storage storage)
    {
        m_storage = storage;
    }
    void setLayoutLocation(int location)
    {
        m_layoutLocation = location;
    }

    bool isWritable(void) const;

    void tokenizeDeclaration(GeneratorState &state, TokenStream &str) const;

private:
    VariableType m_type;
    Storage m_storage;
    std::string m_name;
    int m_layoutLocation;
};

} // namespace rsg

#endif // _RSGVARIABLE_HPP
