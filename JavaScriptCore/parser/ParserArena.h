/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef ParserArena_h
#define ParserArena_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace JSC {

    class ParserArenaDeletable;
    class ParserArenaRefCounted;

    class ParserArena {
    public:
        void swap(ParserArena& otherArena)
        {
            m_deletableObjects.swap(otherArena.m_deletableObjects);
            m_refCountedObjects.swap(otherArena.m_refCountedObjects);
        }
        ~ParserArena();

        void deleteWithArena(ParserArenaDeletable* object) { m_deletableObjects.append(object); }
        void derefWithArena(PassRefPtr<ParserArenaRefCounted> object) { m_refCountedObjects.append(object); }

        bool contains(ParserArenaRefCounted*) const;
        ParserArenaRefCounted* last() const;
        void removeLast();

        bool isEmpty() const { return m_deletableObjects.isEmpty() && m_refCountedObjects.isEmpty(); }
        void reset();

    private:
        Vector<ParserArenaDeletable*> m_deletableObjects;
        Vector<RefPtr<ParserArenaRefCounted> > m_refCountedObjects;
    };

}

#endif
