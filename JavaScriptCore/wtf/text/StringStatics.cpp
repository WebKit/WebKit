/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#ifdef SKIP_STATIC_CONSTRUCTORS_ON_GCC
#define ATOMICSTRING_HIDE_GLOBALS 1
#endif

#include "AtomicString.h"
#include "StaticConstructors.h"
#include "StringImpl.h"

namespace WTF {

StringImpl* StringImpl::empty()
{
    // FIXME: This works around a bug in our port of PCRE, that a regular expression
    // run on the empty string may still perform a read from the first element, and
    // as such we need this to be a valid pointer. No code should ever be reading
    // from a zero length string, so this should be able to be a non-null pointer
    // into the zero-page.
    // Replace this with 'reinterpret_cast<UChar*>(static_cast<intptr_t>(1))' once
    // PCRE goes away.
    static UChar emptyUCharData = 0;
    DEFINE_STATIC_LOCAL(StringImpl, emptyString, (&emptyUCharData, 0, ConstructStaticString));
    return &emptyString;
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
