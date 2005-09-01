/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Additional copyright (KHTML code)
              (C) 1999 Lars Knoll <knoll@kde.org>
              (C) 2003 Dirk Mueller (mueller@kde.org)

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "DOMStringImpl.h"

using namespace KDOM;

#define QT_ALLOC_QCHAR_VEC(N)    (QChar *) new char[sizeof(QChar) * (N)]
#define QT_DELETE_QCHAR_VEC(P)    delete[]((char *) (P))

DOMStringImpl::DOMStringImpl() : Shared()
{
    m_str = 0;
    m_len = 0;
}

DOMStringImpl::DOMStringImpl(const QChar *str, unsigned int len) : Shared()
{
    bool haveStr = str && len;
    m_str = QT_ALLOC_QCHAR_VEC(haveStr ? len : 1);

    if(haveStr)
    {
        memcpy(m_str, str, len * sizeof(QChar));
        m_len = len;
    }
    else
    {
        // Crash protection
        m_str[0] = 0x0;
        m_len = 0;
    }
}

DOMStringImpl::DOMStringImpl(const QString &str) : Shared()
{
    int len = str.length();
    m_str = QT_ALLOC_QCHAR_VEC(len);
    memcpy(m_str, str.unicode(), len * sizeof(QChar));
    m_len = len;
}

DOMStringImpl::DOMStringImpl(const char *str) : Shared()
{
    if(str && *str)
    {
        m_len = strlen(str);
        m_str = QT_ALLOC_QCHAR_VEC(m_len);

        int i = m_len;
        QChar *ptr = m_str;
        while(i--)
            *ptr++ = *str++;
    }
    else
    {
        // Crash protection
        m_str = QT_ALLOC_QCHAR_VEC(1);
        m_str[0] = 0x0; // == QChar::null;
        m_len = 0;
    }
}

DOMStringImpl::~DOMStringImpl()
{
    if(m_str)
        QT_DELETE_QCHAR_VEC(m_str);
}

const QChar &DOMStringImpl::operator[](int i) const
{
    return m_str[i];
}

unsigned int DOMStringImpl::length() const
{
    return m_len;
}

void DOMStringImpl::setLength(unsigned int len)
{
    m_len = len;
}

void DOMStringImpl::insert(DOMStringImpl *str, unsigned int pos)
{
    if(pos > m_len)
    {
        append(str);
        return;
    }

    if(str && str->length() != 0)
    {
        int newLen = m_len + str->length();
        QChar *c = QT_ALLOC_QCHAR_VEC(newLen);

        memcpy(c, m_str, pos * sizeof(QChar));
        memcpy(c + pos, str->unicode(), str->length() * sizeof(QChar));
        memcpy(c + pos + str->length(), m_str + pos, (m_len - pos) * sizeof(QChar));

        if(m_str)
            QT_DELETE_QCHAR_VEC(m_str);

        m_str = c;
        m_len = newLen;
    }
}

void DOMStringImpl::append(const char *str)
{
    if(str && *str)
    {
        int len = strlen(str);
        int newLen = m_len + len;
        QChar *c = QT_ALLOC_QCHAR_VEC(newLen);

        memcpy(c, m_str, m_len * sizeof(QChar));
        memcpy(c + m_len, str, len * sizeof(QChar));
        
        if(m_str)
            QT_DELETE_QCHAR_VEC(m_str);

        m_str = c;
        m_len = newLen;
    }
}

void DOMStringImpl::append(const QString &str)
{
    if(!str.isEmpty())
    {
        int len = str.length();
        int newLen = m_len + len;
        QChar *c = QT_ALLOC_QCHAR_VEC(newLen);

        memcpy(c, m_str, m_len * sizeof(QChar));
        memcpy(c + m_len, str.unicode(), len * sizeof(QChar));
        
        if(m_str)
            QT_DELETE_QCHAR_VEC(m_str);

        m_str = c;
        m_len = newLen;
    }
}

void DOMStringImpl::append(DOMStringImpl *str)
{
    if(str && str->length() != 0)
    {
        int newLen = m_len + str->length();
        QChar *c = QT_ALLOC_QCHAR_VEC(newLen);

        memcpy(c, m_str, m_len * sizeof(QChar));
        memcpy(c + m_len, str->unicode(), str->length() * sizeof(QChar));
        
        if(m_str)
            QT_DELETE_QCHAR_VEC(m_str);

        m_str = c;
        m_len = newLen;
    }
}

bool DOMStringImpl::isEmpty() const
{
    return !length();
}

// FIXME: should be a cached flag maybe.
bool DOMStringImpl::containsOnlyWhitespace() const
{
    if(!m_str)
        return true;

    for(unsigned int i = 0; i < m_len; i++)
    {
        QChar c = m_str[i];
        if(c.unicode() <= 0x7F)
        {
            if(c.unicode() > ' ')
                return false;
        }
        else
        {
            if(c.direction() != QChar::DirWS)
                return false;
        }
    }
    
    return true;
}

void DOMStringImpl::truncate(int len)
{
    if(len > (int) m_len)
        return;

    int newLen = len < 1 ? 1 : len;
    QChar *c = QT_ALLOC_QCHAR_VEC(newLen);    
    memcpy(c, m_str, newLen * sizeof(QChar));

    if(m_str)
        QT_DELETE_QCHAR_VEC(m_str);

    m_str = c;
    m_len = len;
}

void DOMStringImpl::remove(unsigned int pos, int len)
{
    if(pos >= m_len)
        return;

    if(pos + len > m_len)
        len = m_len - pos;

    unsigned int newLen = m_len - len;
    QChar *c = QT_ALLOC_QCHAR_VEC(newLen);

    memcpy(c, m_str, pos * sizeof(QChar));
    memcpy(c + pos, m_str + pos + len, (m_len - len - pos) * sizeof(QChar));

    if(m_str)
        QT_DELETE_QCHAR_VEC(m_str);

    m_str = c;
    m_len = newLen;
}

DOMStringImpl *DOMStringImpl::substring(unsigned int pos, unsigned int len)
{
    if(pos >= m_len)
        return new DOMStringImpl();

    if(pos + len > m_len)
        len = m_len - pos;

    return new DOMStringImpl(m_str + pos, len);
}

DOMStringImpl *DOMStringImpl::split(unsigned int pos)
{
    if(pos >= m_len)
         return new DOMStringImpl();

    unsigned int newLen = m_len - pos;
    DOMStringImpl *str = new DOMStringImpl(m_str + pos, newLen);
    truncate(pos - 1);
    return str;
}

// Collapses white-space according to CSS 2.1 rules
DOMStringImpl *DOMStringImpl::collapseWhiteSpace(bool preserveLF, bool preserveWS)
{
    if(preserveLF && preserveWS)
        return this;

    // Notice we are likely allocating more space than needed (worst case)
    QChar *n = QT_ALLOC_QCHAR_VEC(m_len);

    unsigned int pos = 0;
    bool collapsing = false;   // collapsing white-space
    bool collapsingLF = false; // collapsing around linefeed
    bool changedLF = false;
    for(unsigned int i = 0; i < m_len; i++)
    {
        QChar ch = m_str[i];
        if(!preserveLF && (ch == '\n' || ch == '\r'))
        {
            // ### Not strictly correct according to CSS3 text-module.
            // - In ideographic languages linefeed should be ignored
            // - and in Thai and Khmer it should be treated as a zero-width space
            ch = ' '; // Treat as space
            changedLF = true;
        }

        if(collapsing)
        {
            if(ch == ' ')
                continue;
            
            // We act on \r as we would on \n because CSS uses it to indicate new-line
            if(ch == '\n' || ch == '\r')
            {
                collapsingLF = true;
                continue;
            }

            n[pos++] = (collapsingLF) ? '\n' : ' ';
            collapsing = false;
            collapsingLF = false;
        }
        else
        {
            if (!preserveWS && ch == ' ')
            {
                collapsing = true;
                continue;
            }
            else if (!preserveWS && (ch == '\n' || ch == '\r'))
            {
                collapsing = true;
                collapsingLF = true;
                continue;
            }
        }
        
        n[pos++] = ch;
    }
    
    if(collapsing)
        n[pos++] = ((collapsingLF) ? '\n' : ' ');

    if(pos == m_len && !changedLF)
    {
        QT_DELETE_QCHAR_VEC(n);
        return this;
    }
    else
    {
        DOMStringImpl *out = new DOMStringImpl();
        out->m_str = n;
        out->m_len = pos;

        return out;
    }
}

DOMStringImpl *DOMStringImpl::lower() const
{
    DOMStringImpl *c = new DOMStringImpl();

    if(!m_len)
        return c;

    c->m_str = QT_ALLOC_QCHAR_VEC(m_len);
    c->m_len = m_len;

    for(unsigned int i = 0; i < m_len; i++)
        c->m_str[i] = m_str[i].lower();

    return c;
}

DOMStringImpl *DOMStringImpl::upper() const
{
    DOMStringImpl *c = new DOMStringImpl();

    if(!m_len)
        return c;

    c->m_str = QT_ALLOC_QCHAR_VEC(m_len);
    c->m_len = m_len;

    for(unsigned int i = 0; i < m_len; i++)
        c->m_str[i] = m_str[i].upper();

    return c;
}


DOMStringImpl *DOMStringImpl::capitalize() const
{
    DOMStringImpl *c = new DOMStringImpl();

    if(!m_len)
        return c;

    c->m_str = QT_ALLOC_QCHAR_VEC(m_len);
    c->m_len = m_len;

    if(m_len)
        c->m_str[0] = m_str[0].upper();
    
    for(unsigned int i = 1; i < m_len; i++)
        c->m_str[i] = m_str[i - 1].isLetterOrNumber() ? m_str[i] : m_str[i].upper();

    return c;
}

QChar *DOMStringImpl::unicode() const
{
    return m_str;
}

QString DOMStringImpl::string() const
{
    if(!m_str || !m_len)
        return QString();

    return QString(m_str, m_len);
}

int DOMStringImpl::toInt(bool *ok) const
{
    // match \s*[+-]?\d*
    unsigned i = 0;
    while(i < m_len && m_str[i].isSpace())
        ++i;

    if(i < m_len && (m_str[i] == '+' || m_str[i] == '-'))
        ++i;

    while(i < m_len && m_str[i].isDigit())
        ++i;

    return QConstString(m_str, i).qstring().toInt(ok);
}

DOMStringImpl *DOMStringImpl::copy() const
{
    return new DOMStringImpl(m_str, m_len);
}

// vim:ts=4:noet
