/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#ifdef SKIP_STATIC_CONSTRUCTORS_ON_GCC
#define ATOMICSTRING_HIDE_GLOBALS 1
#endif

#include "AtomicString.h"

#include "StaticConstructors.h"
#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/Threading.h>
#include <wtf/WTFThreadData.h>
#include <wtf/text/AtomicStringTable.h>

namespace WebCore {

static inline AtomicStringTable& table()
{
    return wtfThreadData().atomicStringTable();
}

bool operator==(const AtomicString& a, const char* b)
{ 
    StringImpl* impl = a.impl();
    if ((!impl || !impl->characters()) && !b)
        return true;
    if ((!impl || !impl->characters()) || !b)
        return false;
    return ::equal(impl, b); 
}

PassRefPtr<StringImpl> AtomicString::add(const char* c)
{
    if (!c)
        return 0;
    if (!*c)
        return StringImpl::empty();
    return table().add(c);
}

PassRefPtr<StringImpl> AtomicString::add(const UChar* s, unsigned length)
{
    if (!s)
        return 0;

    if (length == 0)
        return StringImpl::empty();
    
    return table().add(s, length);
}

PassRefPtr<StringImpl> AtomicString::add(const UChar* s, unsigned length, unsigned existingHash)
{
    ASSERT(s);
    ASSERT(existingHash);

    if (length == 0)
        return StringImpl::empty();

    return table().add(s, length, existingHash);
}

PassRefPtr<StringImpl> AtomicString::add(const UChar* s)
{
    if (!s)
        return 0;

    unsigned length = 0;
    while (s[length] != UChar(0))
        length++;

    if (length == 0)
        return StringImpl::empty();

    return table().add(s, length);
}

PassRefPtr<StringImpl> AtomicString::add(StringImpl* r)
{
    if (!r || r->isAtomic())
        return r;

    // The singleton empty string is atomic.
    ASSERT(r->length());
    
    return table().add(r);
}

AtomicStringImpl* AtomicString::find(const UChar* s, unsigned length, unsigned existingHash)
{
    ASSERT(s);
    ASSERT(existingHash);

    if (length == 0)
        return static_cast<AtomicStringImpl*>(StringImpl::empty());
    
    return static_cast<AtomicStringImpl*>(table().find(s, length, existingHash));
}

void AtomicString::remove(StringImpl* r)
{
    table().remove(r);
}
    
AtomicString AtomicString::lower() const
{
    // Note: This is a hot function in the Dromaeo benchmark.
    StringImpl* impl = this->impl();
    RefPtr<StringImpl> newImpl = impl->lower();
    if (LIKELY(newImpl == impl))
        return *this;
    return AtomicString(newImpl);
}

JS_EXPORTDATA DEFINE_GLOBAL(AtomicString, nullAtom)
JS_EXPORTDATA DEFINE_GLOBAL(AtomicString, emptyAtom, "")
JS_EXPORTDATA DEFINE_GLOBAL(AtomicString, textAtom, "#text")
JS_EXPORTDATA DEFINE_GLOBAL(AtomicString, commentAtom, "#comment")
JS_EXPORTDATA DEFINE_GLOBAL(AtomicString, starAtom, "*")
JS_EXPORTDATA DEFINE_GLOBAL(AtomicString, xmlAtom, "xml")
JS_EXPORTDATA DEFINE_GLOBAL(AtomicString, xmlnsAtom, "xmlns")

void AtomicString::init()
{
    static bool initialized;
    if (!initialized) {
        // Initialization is not thread safe, so this function must be called from the main thread first.
        ASSERT(isMainThread());

        // Use placement new to initialize the globals.
        new ((void*)&nullAtom) AtomicString;
        new ((void*)&emptyAtom) AtomicString("");
        new ((void*)&textAtom) AtomicString("#text");
        new ((void*)&commentAtom) AtomicString("#comment");
        new ((void*)&starAtom) AtomicString("*");
        new ((void*)&xmlAtom) AtomicString("xml");
        new ((void*)&xmlnsAtom) AtomicString("xmlns");

        initialized = true;
    }
}

}
