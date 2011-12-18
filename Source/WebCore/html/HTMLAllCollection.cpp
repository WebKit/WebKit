/*
 * Copyright (C) 2009, 2011 Apple Inc. All rights reserved.
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
#include "HTMLAllCollection.h"

#include "CollectionCache.h"
#include "Element.h"

namespace WebCore {

PassRefPtr<HTMLAllCollection> HTMLAllCollection::create(Document* document)
{
    return adoptRef(new HTMLAllCollection(document));
}

HTMLAllCollection::HTMLAllCollection(Document* document)
    : HTMLCollection(document, DocAll)
    , m_idsDone(false)
{
}

HTMLAllCollection::~HTMLAllCollection()
{
}

Node* HTMLAllCollection::nextNamedItem(const AtomicString& name) const
{
    resetCollectionInfo();
    info()->checkConsistency();

    for (Element* e = itemAfter(info()->current); e; e = itemAfter(e)) {
        if (checkForNameMatch(e, m_idsDone, name)) {
            info()->current = e;
            return e;
        }
    }

    if (m_idsDone) {
        info()->current = 0;
        return 0;
    }
    m_idsDone = true;

    for (Element* e = itemAfter(info()->current); e; e = itemAfter(e)) {
        if (checkForNameMatch(e, m_idsDone, name)) {
            info()->current = e;
            return e;
        }
    }

    return 0;
}

} // namespace WebCore
