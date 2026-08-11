#ifndef _GLUVARTYPEUTIL_HPP
#define _GLUVARTYPEUTIL_HPP
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

#include "tcuDefs.hpp"
#include "gluVarType.hpp"

#include <vector>
#include <string>
#include <iterator>

namespace glu
{

// Variable path tokenizer

class VarTokenizer
{
public:
    enum Token
    {
        TOKEN_IDENTIFIER = 0,
        TOKEN_LEFT_BRACKET,
        TOKEN_RIGHT_BRACKET,
        TOKEN_PERIOD,
        TOKEN_NUMBER,
        TOKEN_END,

        TOKEN_LAST
    };

    VarTokenizer(const char *str);
    ~VarTokenizer(void)
    {
    }

    Token getToken(void) const
    {
        return m_token;
    }
    std::string getIdentifier(void) const
    {
        return std::string(m_str + m_tokenStart, m_str + m_tokenStart + m_tokenLen);
    }
    int getNumber(void) const;
    int getCurrentTokenStartLocation(void) const
    {
        return m_tokenStart;
    }
    int getCurrentTokenEndLocation(void) const
    {
        return m_tokenStart + m_tokenLen;
    }
    void advance(void);

private:
    const char *m_str;

    Token m_token;
    int m_tokenStart;
    int m_tokenLen;
};

// VarType subtype path utilities.

struct VarTypeComponent
{
    enum Type
    {
        STRUCT_MEMBER = 0,
        ARRAY_ELEMENT,
        MATRIX_COLUMN,
        VECTOR_COMPONENT,

        VTCTYPE_LAST
    };

    VarTypeComponent(Type type_, int64_t index_) : type(type_), index(index_)
    {
    }
    VarTypeComponent(void) : type(VTCTYPE_LAST), index(0)
    {
    }

    bool operator==(const VarTypeComponent &other) const
    {
        return type == other.type && index == other.index;
    }
    bool operator!=(const VarTypeComponent &other) const
    {
        return type != other.type || index != other.index;
    }

    Type type;
    int64_t index;
};

typedef std::vector<VarTypeComponent> TypeComponentVector;

// TypeComponentVector utilties.

template <typename Iterator>
bool isValidTypePath(const VarType &type, Iterator begin, Iterator end);

template <typename Iterator>
VarType getVarType(const VarType &type, Iterator begin, Iterator end);

inline bool isValidTypePath(const VarType &type, const TypeComponentVector &path)
{
    return isValidTypePath(type, path.begin(), path.end());
}
inline VarType getVarType(const VarType &type, const TypeComponentVector &path)
{
    return getVarType(type, path.begin(), path.end());
}

std::string parseVariableName(const char *nameWithPath);
void parseTypePath(const char *nameWithPath, const VarType &type, TypeComponentVector &path);

// Type path formatter.

struct TypeAccessFormat
{
    TypeAccessFormat(const VarType &type_, const TypeComponentVector &path_) : type(type_), path(path_)
    {
    }

    const VarType &type;
    const TypeComponentVector &path;
};

std::ostream &operator<<(std::ostream &str, const TypeAccessFormat &format);

// Subtype path builder.

class SubTypeAccess
{
public:
    SubTypeAccess(const VarType &type);

    SubTypeAccess &member(int64_t ndx)
    {
        m_path.push_back(VarTypeComponent(VarTypeComponent::STRUCT_MEMBER, ndx));
        DE_ASSERT(isValid());
        return *this;
    } //!< Access struct element.
    SubTypeAccess &element(int64_t ndx)
    {
        m_path.push_back(VarTypeComponent(VarTypeComponent::ARRAY_ELEMENT, ndx));
        DE_ASSERT(isValid());
        return *this;
    } //!< Access array element.
    SubTypeAccess &column(int64_t ndx)
    {
        m_path.push_back(VarTypeComponent(VarTypeComponent::MATRIX_COLUMN, ndx));
        DE_ASSERT(isValid());
        return *this;
    } //!< Access column.
    SubTypeAccess &component(int64_t ndx)
    {
        m_path.push_back(VarTypeComponent(VarTypeComponent::VECTOR_COMPONENT, ndx));
        DE_ASSERT(isValid());
        return *this;
    } //!< Access component.
    SubTypeAccess &parent(void)
    {
        DE_ASSERT(!m_path.empty());
        m_path.pop_back();
        return *this;
    }

    SubTypeAccess member(int64_t ndx) const
    {
        return SubTypeAccess(*this).member(ndx);
    }
    SubTypeAccess element(int64_t ndx) const
    {
        return SubTypeAccess(*this).element(ndx);
    }
    SubTypeAccess column(int64_t ndx) const
    {
        return SubTypeAccess(*this).column(ndx);
    }
    SubTypeAccess component(int64_t ndx) const
    {
        return SubTypeAccess(*this).component(ndx);
    }
    SubTypeAccess parent(void) const
    {
        return SubTypeAccess(*this).parent();
    }

    bool isValid(void) const
    {
        return isValidTypePath(m_type, m_path);
    }
    VarType getType(void) const
    {
        return getVarType(m_type, m_path);
    }
    const TypeComponentVector &getPath(void) const
    {
        return m_path;
    }

    bool empty(void) const
    {
        return m_path.empty();
    }

    bool operator==(const SubTypeAccess &other) const
    {
        return m_path == other.m_path && m_type == other.m_type;
    }
    bool operator!=(const SubTypeAccess &other) const
    {
        return m_path != other.m_path || m_type != other.m_type;
    }

private:
    VarType m_type;
    TypeComponentVector m_path;
};

// Subtype iterator.

// \note VarType must be live during iterator usage.
template <class IsExpanded>
class SubTypeIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = VarType;
    using difference_type   = std::ptrdiff_t;
    using pointer           = VarType *;
    using reference         = VarType &;

    static SubTypeIterator<IsExpanded> begin(const VarType *type)
    {
        return SubTypeIterator(type);
    }
    static SubTypeIterator<IsExpanded> end(const VarType *type)
    {
        DE_UNREF(type);
        return SubTypeIterator(nullptr);
    }

    bool operator==(const SubTypeIterator<IsExpanded> &other) const
    {
        return m_type == other.m_type && m_path == other.m_path;
    }
    bool operator!=(const SubTypeIterator<IsExpanded> &other) const
    {
        return m_type != other.m_type || m_path != other.m_path;
    }

    SubTypeIterator<IsExpanded> &operator++(void);
    SubTypeIterator<IsExpanded> operator++(int)
    {
        SubTypeIterator<IsExpanded> copy(*this);
        ++(*this);
        return copy;
    }

    void toStream(std::ostream &str) const
    {
        str << TypeAccessFormat(*m_type, m_path);
    }
    VarType getType(void) const
    {
        return getVarType(*m_type, m_path.begin(), m_path.end());
    }
    const TypeComponentVector &getPath(void) const
    {
        return m_path;
    }

    VarType operator*(void) const
    {
        return getType();
    }

private:
    SubTypeIterator(const VarType *type);

    void removeTraversed(void);
    void findNext(void);

    const VarType *m_type;
    TypeComponentVector m_path;
};

struct IsBasicType
{
    bool operator()(const VarType &type) const
    {
        return type.isBasicType();
    }
};
struct IsScalarType
{
    bool operator()(const VarType &type) const
    {
        return type.isBasicType() && isDataTypeScalar(type.getBasicType());
    }
};
struct IsVectorOrScalarType
{
    bool operator()(const VarType &type) const
    {
        return type.isBasicType() && isDataTypeScalarOrVector(type.getBasicType());
    }
};

typedef SubTypeIterator<IsBasicType> BasicTypeIterator;
typedef SubTypeIterator<IsVectorOrScalarType> VectorTypeIterator;
typedef SubTypeIterator<IsScalarType> ScalarTypeIterator;

template <class IsExpanded>
std::ostream &operator<<(std::ostream &str, const SubTypeIterator<IsExpanded> &iter)
{
    iter.toStream(str);
    return str;
}

template <class IsExpanded>
SubTypeIterator<IsExpanded>::SubTypeIterator(const VarType *type) : m_type(type)
{
    if (m_type)
        findNext();
}

template <class IsExpanded>
SubTypeIterator<IsExpanded> &SubTypeIterator<IsExpanded>::operator++(void)
{
    if (!m_path.empty())
    {
        // Remove traversed nodes.
        removeTraversed();

        if (!m_path.empty())
            findNext();
        else
            m_type = nullptr; // Unset type to signal end.
    }
    else
    {
        // First type was already expanded.
        DE_ASSERT(IsExpanded()(getVarType(*m_type, m_path)));
        m_type = nullptr;
    }

    return *this;
}

template <class IsExpanded>
void SubTypeIterator<IsExpanded>::removeTraversed(void)
{
    DE_ASSERT(m_type && !m_path.empty());

    // Pop traversed nodes.
    while (!m_path.empty())
    {
        VarTypeComponent &curComp = m_path.back();
        VarType parentType        = getVarType(*m_type, m_path.begin(), m_path.end() - 1);

        if (curComp.type == VarTypeComponent::MATRIX_COLUMN)
        {
            DE_ASSERT(isDataTypeMatrix(parentType.getBasicType()));
            if (curComp.index + 1 < getDataTypeMatrixNumColumns(parentType.getBasicType()))
                break;
        }
        else if (curComp.type == VarTypeComponent::VECTOR_COMPONENT)
        {
            DE_ASSERT(isDataTypeVector(parentType.getBasicType()));
            if (curComp.index + 1 < getDataTypeScalarSize(parentType.getBasicType()))
                break;
        }
        else if (curComp.type == VarTypeComponent::ARRAY_ELEMENT)
        {
            DE_ASSERT(parentType.isArrayType());
            if (curComp.index + 1 < parentType.getArraySize())
                break;
        }
        else if (curComp.type == VarTypeComponent::STRUCT_MEMBER)
        {
            DE_ASSERT(parentType.isStructType());
            if (curComp.index + 1 < parentType.getStructPtr()->getNumMembers())
                break;
        }

        m_path.pop_back();
    }
}

template <class IsExpanded>
void SubTypeIterator<IsExpanded>::findNext(void)
{
    if (!m_path.empty())
    {
        // Increment child counter in current level.
        VarTypeComponent &curComp = m_path.back();
        curComp.index += 1;
    }

    for (;;)
    {
        VarType curType = getVarType(*m_type, m_path);

        if (IsExpanded()(curType))
            break;

        // Recurse into child type.
        if (curType.isBasicType())
        {
            DataType basicType = curType.getBasicType();

            if (isDataTypeMatrix(basicType))
                m_path.push_back(VarTypeComponent(VarTypeComponent::MATRIX_COLUMN, 0));
            else if (isDataTypeVector(basicType))
                m_path.push_back(VarTypeComponent(VarTypeComponent::VECTOR_COMPONENT, 0));
            else
                DE_ASSERT(false); // Can't expand scalars - IsExpanded() is buggy.
        }
        else if (curType.isArrayType())
            m_path.push_back(VarTypeComponent(VarTypeComponent::ARRAY_ELEMENT, 0));
        else if (curType.isStructType())
            m_path.push_back(VarTypeComponent(VarTypeComponent::STRUCT_MEMBER, 0));
        else
            DE_ASSERT(false);
    }
}

template <typename Iterator>
bool isValidTypePath(const VarType &type, Iterator begin, Iterator end)
{
    const VarType *curType = &type;
    Iterator pathIter      = begin;

    // Process struct member and array element parts of path.
    while (pathIter != end)
    {
        if (pathIter->type == VarTypeComponent::STRUCT_MEMBER)
        {
            if (!curType->isStructType() ||
                !de::inBounds(pathIter->index, int64_t{0}, (int64_t)curType->getStructPtr()->getNumMembers()))
                return false;

            curType = &curType->getStructPtr()->getMember(pathIter->index).getType();
        }
        else if (pathIter->type == VarTypeComponent::ARRAY_ELEMENT)
        {
            if (!curType->isArrayType() ||
                (curType->getArraySize() != VarType::UNSIZED_ARRAY &&
                 !de::inBounds(pathIter->index, int64_t{0}, (int64_t)curType->getArraySize())))
                return false;

            curType = &curType->getElementType();
        }
        else
            break;

        ++pathIter;
    }

    if (pathIter != end)
    {
        DE_ASSERT(pathIter->type == VarTypeComponent::MATRIX_COLUMN ||
                  pathIter->type == VarTypeComponent::VECTOR_COMPONENT);

        // Current type should be basic type.
        if (!curType->isBasicType())
            return false;

        DataType basicType = curType->getBasicType();

        if (pathIter->type == VarTypeComponent::MATRIX_COLUMN)
        {
            if (!isDataTypeMatrix(basicType))
                return false;

            basicType = getDataTypeFloatVec(getDataTypeMatrixNumRows(basicType));
            ++pathIter;
        }

        if (pathIter != end && pathIter->type == VarTypeComponent::VECTOR_COMPONENT)
        {
            if (!isDataTypeVector(basicType))
                return false;

            basicType = getDataTypeScalarType(basicType);
            ++pathIter;
        }
    }

    return pathIter == end;
}

template <typename Iterator>
VarType getVarType(const VarType &type, Iterator begin, Iterator end)
{
    TCU_CHECK(isValidTypePath(type, begin, end));

    const VarType *curType = &type;
    Iterator pathIter      = begin;

    // Process struct member and array element parts of path.
    while (pathIter != end)
    {
        if (pathIter->type == VarTypeComponent::STRUCT_MEMBER)
            curType = &curType->getStructPtr()->getMember(pathIter->index).getType();
        else if (pathIter->type == VarTypeComponent::ARRAY_ELEMENT)
            curType = &curType->getElementType();
        else
            break;

        ++pathIter;
    }

    if (pathIter != end)
    {
        DataType basicType  = curType->getBasicType();
        Precision precision = curType->getPrecision();

        if (pathIter->type == VarTypeComponent::MATRIX_COLUMN)
        {
            basicType = getDataTypeFloatVec(getDataTypeMatrixNumRows(basicType));
            ++pathIter;
        }

        if (pathIter != end && pathIter->type == VarTypeComponent::VECTOR_COMPONENT)
        {
            basicType = getDataTypeScalarType(basicType);
            ++pathIter;
        }

        DE_ASSERT(pathIter == end);
        return VarType(basicType, precision);
    }
    else
        return VarType(*curType);
}

} // namespace glu

#endif // _GLUVARTYPEUTIL_HPP
