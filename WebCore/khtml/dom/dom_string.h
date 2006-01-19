/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef DOM_DOMString_h_
#define DOM_DOMString_h_

#include "dom_stringimpl.h"

namespace WebCore {

/**
 * This class implements the basic string we use in the DOM. We do not use
 * QString for 2 reasons: Memory overhead, and the missing explicit sharing
 * of strings we need for the DOM.
 *
 * All DOMStrings are explicitly shared (they behave like pointers), meaning
 * that modifications to one instance will also modify all others. If you
 * wish to get a DOMString that is independent, use @ref copy().
 */
class DOMString
{
public:
    /**
     * default constructor. Gives an empty DOMString
     */
    DOMString() { }

    DOMString(const QChar *str, uint len);
    DOMString(const QString &);
    DOMString(const char *str);
    DOMString(DOMStringImpl *i) : m_impl(i) { }

    /**
     * append str to this string
     */
    DOMString &operator += (const DOMString &str);

    void insert(DOMString str, uint pos);

    /**
     * The character at position i of the DOMString. If i >= length(), the
     * character returned will be 0.
     */
    const QChar &operator [](unsigned int i) const;
    
    // NOTE: contains returns bool, eventually we'll add count() which returns an int.
    bool contains(QChar c) const { return (find(c) != -1); }
    bool contains(const char *str, bool caseSensitive = true) const { return (find(str, 0, caseSensitive) != -1); }

    int find(const QChar c, int start = 0) const
        { if (m_impl) return m_impl->find(c, start); return -1; }
    int find(const char *str, int start = 0, bool caseSensitive = true) const
        { if (m_impl) return m_impl->find(str, start, caseSensitive); return -1; }
    int find(const DOMString& str, int start = 0, bool caseSensitive = true) const
        { if (m_impl) return m_impl->find(str.impl(), start, caseSensitive); return -1; }
    
    bool startsWith(const DOMString &s, bool caseSensitive = true) const
        { if (m_impl) return m_impl->startsWith(s.impl(), caseSensitive); return false; }
    bool endsWith(const DOMString &s, bool caseSensitive = true) const
        { if (m_impl) return m_impl->endsWith(s.impl(), caseSensitive); return false; }

    DOMString &replace(QChar a, QChar b) { if (m_impl) m_impl = m_impl->replace(a, b); return *this; }

    uint length() const;
    void truncate( unsigned int len );
    void remove(unsigned int pos, int len=1);

    DOMString substring(unsigned pos, unsigned len = UINT_MAX) const;

    /**
     * Splits the string into two. The original string gets truncated to pos, and the rest is returned.
     */
    DOMString split(unsigned int pos);

    /**
     * Returns a lowercase/uppercase version of the string
     */
    DOMString lower() const;
    DOMString upper() const;

    QChar *unicode() const;
    QString qstring() const;

    int toInt() const;
    khtml::Length* toLengthArray(int& len) const;
    khtml::Length* toCoordsArray(int& len) const;
    bool percentage(int &_percentage) const;

    DOMString copy() const;

    bool isNull()  const { return !m_impl; }
    bool isEmpty()  const;

    DOMStringImpl *impl() const { return m_impl.get(); }

#if __APPLE__
    DOMString(CFStringRef);
    CFStringRef createCFString() const { return m_impl ? m_impl->createCFString() : CFSTR(""); }
#endif
#if __OBJC__
    DOMString(NSString*);
    operator NSString*() const { if (!m_impl) return @""; return *m_impl; }
#endif

#ifndef NDEBUG
    // For debugging only, leaks memory.
    const char *ascii() const;
#endif

private:
    RefPtr<DOMStringImpl> m_impl;
};

DOMString operator+(const DOMString&, const DOMString&);

inline bool operator==(const DOMString& a, const DOMString& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const DOMString& a, const char* b) { return equal(a.impl(), b); }
inline bool operator==(const char* a, const DOMString& b) { return equal(a, b.impl()); }

inline bool operator!=(const DOMString& a, const DOMString& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const DOMString& a, const char* b) { return !equal(a.impl(), b); }
inline bool operator!=(const char* a, const DOMString& b) { return !equal(a, b.impl()); }

inline bool equalIgnoringCase(const DOMString& a, const DOMString& b) { return equalIgnoringCase(a.impl(), b.impl()); }
inline bool equalIgnoringCase(const DOMString& a, const char* b) { return equalIgnoringCase(a.impl(), b); }
inline bool equalIgnoringCase(const char* a, const DOMString& b) { return equalIgnoringCase(a, b.impl()); }

bool operator==(const DOMString& a, const QString& b);
inline bool operator==( const QString& b, const DOMString& a) { return a == b; }
inline bool operator!=(const DOMString& a, const QString& b) { return !(a == b); }
inline bool operator!=(const QString& b, const DOMString& a ) { return !(a == b); }

}

namespace KXMLCore {

    template<> struct DefaultHash<WebCore::DOMString> {
        static unsigned hash(const WebCore::DOMString& key) { return key.impl()->hash(); }
        static bool equal(const WebCore::DOMString& a, const WebCore::DOMString& b)
            { return DefaultHash<WebCore::DOMStringImpl*>::equal(a.impl(), b.impl()); }
    };
    
    template<> struct HashTraits<WebCore::DOMString> {
        typedef WebCore::DOMString TraitType;
        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = true;
        static TraitType emptyValue() { return TraitType(); }
        static TraitType deletedValue() { return HashTraits<RefPtr<WebCore::DOMStringImpl> >::_deleted.get(); }
    };
}

#endif
