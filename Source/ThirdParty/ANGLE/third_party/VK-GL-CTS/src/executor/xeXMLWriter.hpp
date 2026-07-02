#ifndef _XEXMLWRITER_HPP
#define _XEXMLWRITER_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Test Executor
 * ------------------------------------------
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
 * \brief XML Writer.
 *//*--------------------------------------------------------------------*/

#include "xeDefs.hpp"

#include <ostream>
#include <vector>
#include <string>
#include <streambuf>

namespace xe
{
namespace xml
{

class EscapeStreambuf : public std::streambuf
{
public:
    EscapeStreambuf(std::ostream &dst) : m_dst(dst)
    {
    }

protected:
    std::streamsize xsputn(const char *s, std::streamsize count);
    int overflow(int ch = -1);

private:
    std::ostream &m_dst;
};

class Writer
{
public:
    struct BeginElement
    {
        std::string element;
        BeginElement(const char *element_) : element(element_)
        {
        }
    };

    struct Attribute
    {
        std::string name;
        std::string value;
        Attribute(const char *name_, const char *value_) : name(name_), value(value_)
        {
        }
        Attribute(const char *name_, const std::string &value_) : name(name_), value(value_)
        {
        }
        Attribute(const std::string &name_, const std::string &value_) : name(name_), value(value_)
        {
        }
    };

    static const struct EndElementType
    {
    } EndElement;

    Writer(std::ostream &dst);
    ~Writer(void);

    Writer &operator<<(const BeginElement &begin);
    Writer &operator<<(const Attribute &attribute);
    Writer &operator<<(const EndElementType &end);

    template <typename T>
    Writer &operator<<(const T &value); //!< Write data.

private:
    Writer(const Writer &other);
    Writer &operator=(const Writer &other);

    enum State
    {
        STATE_DATA = 0,
        STATE_ELEMENT,
        STATE_ELEMENT_END,

        STATE_LAST
    };

    std::ostream &m_rawDst;
    EscapeStreambuf m_dataBuf;
    std::ostream m_dataStr;
    State m_state;
    std::vector<std::string> m_elementStack;
};

template <typename T>
Writer &Writer::operator<<(const T &value)
{
    if (m_state == STATE_ELEMENT)
        m_rawDst << ">";

    m_dataStr << value;
    m_state = STATE_DATA;

    return *this;
}

} // namespace xml
} // namespace xe

#endif // _XEXMLWRITER_HPP
