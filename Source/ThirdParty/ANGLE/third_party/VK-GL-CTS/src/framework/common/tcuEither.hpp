#ifndef _TCUEITHER_HPP
#define _TCUEITHER_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Template class that is either type of First or Second.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Object containing Either First or Second type of object
 *
 * \note Type First and Second are always aligned to same alignment as
 *         uint64_t.
 * \note This type always uses at least sizeof(bool) + max(sizeof(First*),
 *         sizeof(Second*)) + sizeof(uint64_t) of memory.
 *//*--------------------------------------------------------------------*/
template <typename First, typename Second>
class Either
{
public:
    Either(const First &first);
    Either(const Second &second);
    ~Either(void);

    Either(const Either<First, Second> &other);
    Either &operator=(const Either<First, Second> &other);

    Either &operator=(const First &first);
    Either &operator=(const Second &second);

    bool isFirst(void) const;
    bool isSecond(void) const;

    const First &getFirst(void) const;
    const Second &getSecond(void) const;

    template <typename Type>
    const Type &get(void) const;

    template <typename Type>
    bool is(void) const;

private:
    void release(void);

    bool m_isFirst;

    union
    {
        First *m_first;
        Second *m_second;
    };

    union
    {
        uint8_t m_data[sizeof(First) > sizeof(Second) ? sizeof(First) : sizeof(Second)];
        uint64_t m_align;
    };
} DE_WARN_UNUSED_TYPE;

namespace EitherDetail
{

template <typename Type, typename First, typename Second>
struct Get;

template <typename First, typename Second>
struct Get<First, First, Second>
{
    static const First &get(const Either<First, Second> &either)
    {
        return either.getFirst();
    }
};

template <typename First, typename Second>
struct Get<Second, First, Second>
{
    static const Second &get(const Either<First, Second> &either)
    {
        return either.getSecond();
    }
};

template <typename Type, typename First, typename Second>
const Type &get(const Either<First, Second> &either)
{
    return Get<Type, First, Second>::get(either);
}

template <typename Type, typename First, typename Second>
struct Is;

template <typename First, typename Second>
struct Is<First, First, Second>
{
    static bool is(const Either<First, Second> &either)
    {
        return either.isFirst();
    }
};

template <typename First, typename Second>
struct Is<Second, First, Second>
{
    static bool is(const Either<First, Second> &either)
    {
        return either.isSecond();
    }
};

template <typename Type, typename First, typename Second>
bool is(const Either<First, Second> &either)
{
    return Is<Type, First, Second>::is(either);
}

} // namespace EitherDetail

template <typename First, typename Second>
void Either<First, Second>::release(void)
{
    if (m_isFirst)
        m_first->~First();
    else
        m_second->~Second();

    m_isFirst = true;
    m_first   = nullptr;
}

template <typename First, typename Second>
Either<First, Second>::Either(const First &first) : m_isFirst(true)
{
    m_first = new (m_data) First(first);
}

template <typename First, typename Second>
Either<First, Second>::Either(const Second &second) : m_isFirst(false)
{
    m_second = new (m_data) Second(second);
}

template <typename First, typename Second>
Either<First, Second>::~Either(void)
{
    release();
}

template <typename First, typename Second>
Either<First, Second>::Either(const Either<First, Second> &other) : m_isFirst(other.m_isFirst)
{
    if (m_isFirst)
        m_first = new (m_data) First(*other.m_first);
    else
        m_second = new (m_data) Second(*other.m_second);
}

template <typename First, typename Second>
Either<First, Second> &Either<First, Second>::operator=(const Either<First, Second> &other)
{
    if (this == &other)
        return *this;

    release();

    m_isFirst = other.m_isFirst;

    if (m_isFirst)
        m_first = new (m_data) First(*other.m_first);
    else
        m_second = new (m_data) Second(*other.m_second);

    return *this;
}

template <typename First, typename Second>
Either<First, Second> &Either<First, Second>::operator=(const First &first)
{
    release();

    m_isFirst = true;
    m_first   = new (m_data) First(first);

    return *this;
}

template <typename First, typename Second>
Either<First, Second> &Either<First, Second>::operator=(const Second &second)
{
    release();

    m_isFirst = false;
    m_second  = new (m_data) Second(second);

    return *this;
}

template <typename First, typename Second>
bool Either<First, Second>::isFirst(void) const
{
    return m_isFirst;
}

template <typename First, typename Second>
bool Either<First, Second>::isSecond(void) const
{
    return !m_isFirst;
}

template <typename First, typename Second>
const First &Either<First, Second>::getFirst(void) const
{
    DE_ASSERT(isFirst());
    return *m_first;
}

template <typename First, typename Second>
const Second &Either<First, Second>::getSecond(void) const
{
    DE_ASSERT(isSecond());
    return *m_second;
}

template <typename First, typename Second>
template <typename Type>
const Type &Either<First, Second>::get(void) const
{
    return EitherDetail::get<Type, First, Second>(*this);
}

template <typename First, typename Second>
template <typename Type>
bool Either<First, Second>::is(void) const
{
    return EitherDetail::is<Type, First, Second>(*this);
}

void Either_selfTest(void);

} // namespace tcu

#endif // _TCUEITHER_HPP
