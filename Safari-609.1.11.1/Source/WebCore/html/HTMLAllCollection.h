/*
 * Copyright (C) 2009, 2011, 2017 Apple Inc. All rights reserved.
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

#pragma once

#include "AllDescendantsCollection.h"

namespace WebCore {

class HTMLAllCollection final : public AllDescendantsCollection {
public:
    static Ref<HTMLAllCollection> create(Document&, CollectionType);

    Optional<Variant<RefPtr<HTMLCollection>, RefPtr<Element>>> namedOrIndexedItemOrItems(const AtomString& nameOrIndex) const;
    Optional<Variant<RefPtr<HTMLCollection>, RefPtr<Element>>> namedItemOrItems(const AtomString&) const;

private:
    HTMLAllCollection(Document&, CollectionType);
};
static_assert(sizeof(HTMLAllCollection) == sizeof(AllDescendantsCollection), "");

class HTMLAllNamedSubCollection final : public CachedHTMLCollection<HTMLAllNamedSubCollection, CollectionTraversalType::Descendants> {
    WTF_MAKE_ISO_ALLOCATED(HTMLAllNamedSubCollection);
public:
    static Ref<HTMLAllNamedSubCollection> create(Document& document, CollectionType type, const AtomString& name)
    {
        return adoptRef(*new HTMLAllNamedSubCollection(document, type, name));
    }
    virtual ~HTMLAllNamedSubCollection();

    bool elementMatches(Element&) const;

private:
    HTMLAllNamedSubCollection(Document& document, CollectionType type, const AtomString& name)
        : CachedHTMLCollection<HTMLAllNamedSubCollection, CollectionTraversalType::Descendants>(document, type)
        , m_name(name)
    {
        ASSERT(type == DocumentAllNamedItems);
    }

    AtomString m_name;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_HTMLCOLLECTION(HTMLAllCollection, DocAll)
SPECIALIZE_TYPE_TRAITS_HTMLCOLLECTION(HTMLAllNamedSubCollection, DocumentAllNamedItems)
