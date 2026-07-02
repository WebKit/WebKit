#ifndef _RSGVARIABLETYPE_HPP
#define _RSGVARIABLETYPE_HPP
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

#include "rsgDefs.hpp"

#include <vector>
#include <string>

namespace rsg
{

class TokenStream;

class VariableType
{
public:
    enum Type
    {
        TYPE_VOID = 0,
        TYPE_FLOAT,
        TYPE_INT,
        TYPE_BOOL,
        TYPE_STRUCT,
        TYPE_ARRAY,
        TYPE_SAMPLER_2D,
        TYPE_SAMPLER_CUBE,

        TYPE_LAST
    };

    enum Precision
    {
        PRECISION_NONE = 0,
        PRECISION_LOW,
        PRECISION_MEDIUM,
        PRECISION_HIGH,

        PRECISION_LAST
    };

    class Member
    {
    public:
        Member(void) : m_type(nullptr), m_name()
        {
        }

        Member(const VariableType &type, const char *name) : m_type(new VariableType(type)), m_name(name)
        {
        }

        ~Member(void)
        {
            delete m_type;
        }

        Member(const Member &other) : m_type(nullptr), m_name(other.m_name)
        {
            if (other.m_type)
                m_type = new VariableType(*other.m_type);
        }

        Member &operator=(const Member &other)
        {
            if (this == &other)
                return *this;

            delete m_type;

            m_type = nullptr;
            m_name = other.m_name;

            if (other.m_type)
                m_type = new VariableType(*other.m_type);

            return *this;
        }

        bool operator!=(const Member &other) const
        {
            if (m_name != other.m_name)
                return true;
            if (!!m_type != !!other.m_type)
                return true;
            if (m_type && *m_type != *other.m_type)
                return true;
            return false;
        }

        bool operator==(const Member &other) const
        {
            return !(*this != other);
        }

        const VariableType &getType(void) const
        {
            return *m_type;
        }
        const char *getName(void) const
        {
            return m_name.c_str();
        }

    private:
        VariableType *m_type;
        std::string m_name;
    };

    VariableType(void);
    VariableType(Type baseType, int numElements = 0);
    VariableType(Type baseType, const VariableType &elementType, int numElements);
    VariableType(Type baseType, const char *typeName);
    ~VariableType(void);

    Type getBaseType(void) const
    {
        return m_baseType;
    }
    Precision getPrecision(void) const
    {
        return m_precision;
    }
    const char *getTypeName(void) const
    {
        return m_typeName.c_str();
    }
    int getNumElements(void) const
    {
        return m_numElements;
    }
    const VariableType &getElementType(void) const;

    const std::vector<Member> &getMembers(void) const
    {
        return m_members;
    }
    std::vector<Member> &getMembers(void)
    {
        return m_members;
    }

    int getScalarSize(void) const;
    int getElementScalarOffset(int elementNdx) const;
    int getMemberScalarOffset(int memberNdx) const;

    bool operator!=(const VariableType &other) const;
    bool operator==(const VariableType &other) const;

    VariableType &operator=(const VariableType &other);
    VariableType(const VariableType &other);

    void tokenizeShortType(TokenStream &str) const;

    bool isStruct(void) const
    {
        return m_baseType == TYPE_STRUCT;
    }
    bool isArray(void) const
    {
        return m_baseType == TYPE_ARRAY;
    }
    bool isFloatOrVec(void) const
    {
        return m_baseType == TYPE_FLOAT;
    }
    bool isIntOrVec(void) const
    {
        return m_baseType == TYPE_INT;
    }
    bool isBoolOrVec(void) const
    {
        return m_baseType == TYPE_BOOL;
    }
    bool isSampler(void) const
    {
        return m_baseType == TYPE_SAMPLER_2D || m_baseType == TYPE_SAMPLER_CUBE;
    }
    bool isVoid(void) const
    {
        return m_baseType == TYPE_VOID;
    }

    static const VariableType &getScalarType(Type baseType);

private:
    Type m_baseType;
    Precision m_precision;
    std::string m_typeName;
    int m_numElements;
    VariableType *m_elementType;
    std::vector<Member> m_members;
};

inline VariableType::VariableType(void)
    : m_baseType(TYPE_VOID)
    , m_precision(PRECISION_NONE)
    , m_typeName()
    , m_numElements(0)
    , m_elementType(nullptr)
{
}

inline VariableType::VariableType(Type baseType, int numElements)
    : m_baseType(baseType)
    , m_precision(PRECISION_NONE)
    , m_typeName()
    , m_numElements(numElements)
    , m_elementType(nullptr)
{
    DE_ASSERT(baseType != TYPE_ARRAY && baseType != TYPE_STRUCT);
}

inline VariableType::VariableType(Type baseType, const VariableType &elementType, int numElements)
    : m_baseType(baseType)
    , m_precision(PRECISION_NONE)
    , m_typeName()
    , m_numElements(numElements)
    , m_elementType(new VariableType(elementType))
{
    DE_ASSERT(baseType == TYPE_ARRAY);
}

inline VariableType::VariableType(Type baseType, const char *typeName)
    : m_baseType(baseType)
    , m_precision(PRECISION_NONE)
    , m_typeName(typeName)
    , m_numElements(0)
    , m_elementType(nullptr)
{
    DE_ASSERT(baseType == TYPE_STRUCT);
}

inline VariableType::~VariableType(void)
{
    delete m_elementType;
}

} // namespace rsg

#endif // _RSGVARIABLETYPE_HPP
