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

#include "xeXMLWriter.hpp"

#include <cstring>

namespace xe
{
namespace xml
{

const Writer::EndElementType Writer::EndElement = Writer::EndElementType();

inline const char *getEscapeEntity(char ch)
{
    switch (ch)
    {
    case '<':
        return "&lt;";
    case '>':
        return "&gt;";
    case '&':
        return "&amp;";
    case '\'':
        return "&apos;";
    case '"':
        return "&quot;";

    // Non-printable characters.
    case 0:
        return "&lt;NUL&gt;";
    case 1:
        return "&lt;SOH&gt;";
    case 2:
        return "&lt;STX&gt;";
    case 3:
        return "&lt;ETX&gt;";
    case 4:
        return "&lt;EOT&gt;";
    case 5:
        return "&lt;ENQ&gt;";
    case 6:
        return "&lt;ACK&gt;";
    case 7:
        return "&lt;BEL&gt;";
    case 8:
        return "&lt;BS&gt;";
    case 11:
        return "&lt;VT&gt;";
    case 12:
        return "&lt;FF&gt;";
    case 14:
        return "&lt;SO&gt;";
    case 15:
        return "&lt;SI&gt;";
    case 16:
        return "&lt;DLE&gt;";
    case 17:
        return "&lt;DC1&gt;";
    case 18:
        return "&lt;DC2&gt;";
    case 19:
        return "&lt;DC3&gt;";
    case 20:
        return "&lt;DC4&gt;";
    case 21:
        return "&lt;NAK&gt;";
    case 22:
        return "&lt;SYN&gt;";
    case 23:
        return "&lt;ETB&gt;";
    case 24:
        return "&lt;CAN&gt;";
    case 25:
        return "&lt;EM&gt;";
    case 26:
        return "&lt;SUB&gt;";
    case 27:
        return "&lt;ESC&gt;";
    case 28:
        return "&lt;FS&gt;";
    case 29:
        return "&lt;GS&gt;";
    case 30:
        return "&lt;RS&gt;";
    case 31:
        return "&lt;US&gt;";

    default:
        return nullptr;
    }
}

std::streamsize EscapeStreambuf::xsputn(const char *s, std::streamsize count)
{
    std::streamsize numWritten = 0;

    for (std::streamsize inPos = 0; inPos < count; inPos++)
    {
        const char *entity = getEscapeEntity(s[inPos]);

        if (entity)
        {
            // Flush data prior to entity.
            if (inPos > numWritten)
            {
                m_dst.write(s + numWritten, inPos - numWritten);
                if (m_dst.fail())
                    return numWritten;
            }

            // Flush entity value
            m_dst.write(entity, (std::streamsize)strlen(entity));

            numWritten = inPos + 1;
        }
    }

    if (numWritten < count)
    {
        m_dst.write(s + numWritten, count - numWritten);
        if (m_dst.fail())
            return numWritten;
    }

    return count;
}

int EscapeStreambuf::overflow(int ch)
{
    if (ch == -1)
        return -1;
    else
    {
        DE_ASSERT((ch & 0xff) == ch);
        const char chVal = (char)(uint8_t)(ch & 0xff);
        return xsputn(&chVal, 1) == 1 ? ch : -1;
    }
}

Writer::Writer(std::ostream &dst) : m_rawDst(dst), m_dataBuf(dst), m_dataStr(&m_dataBuf), m_state(STATE_DATA)
{
}

Writer::~Writer(void)
{
}

Writer &Writer::operator<<(const BeginElement &begin)
{
    if (m_state == STATE_ELEMENT)
        m_rawDst << ">";

    if (m_state == STATE_ELEMENT || m_state == STATE_ELEMENT_END)
    {
        m_rawDst << "\n";
        for (int i = 0; i < (int)m_elementStack.size(); i++)
            m_rawDst << "  ";
    }

    m_rawDst << "<" << begin.element;

    m_elementStack.push_back(begin.element);
    m_state = STATE_ELEMENT;

    return *this;
}

Writer &Writer::operator<<(const Attribute &attribute)
{
    DE_ASSERT(m_state == STATE_ELEMENT);

    // \todo [2012-09-05 pyry] Escape?
    m_rawDst << " " << attribute.name << "=\"" << attribute.value << "\"";

    return *this;
}

Writer &Writer::operator<<(const EndElementType &)
{
    if (m_state == STATE_ELEMENT)
        m_rawDst << "/>";
    else
    {
        if (m_state == STATE_ELEMENT_END)
        {
            m_rawDst << "\n";
            for (int i = 0; i < (int)m_elementStack.size() - 1; i++)
                m_rawDst << "  ";
        }

        m_rawDst << "</" << m_elementStack.back() << ">";
    }

    m_elementStack.pop_back();
    m_state = STATE_ELEMENT_END;

    return *this;
}

} // namespace xml
} // namespace xe
