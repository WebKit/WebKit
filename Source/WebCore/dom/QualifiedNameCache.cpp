/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "QualifiedNameCache.h"

#include "ElementName.h"
#include "Namespace.h"

namespace WebCore {

struct QNameComponentsTranslator {
    static unsigned hash(const QualifiedNameComponents& components)
    {
        return computeHash(components);
    }

    static bool equal(QualifiedName::QualifiedNameImpl* name, const QualifiedNameComponents& c)
    {
        return c.m_prefix == name->m_prefix.impl() && c.m_localName == name->m_localName.impl() && c.m_namespaceURI == name->m_namespaceURI.impl();
    }

    static void translate(QualifiedName::QualifiedNameImpl*& location, const QualifiedNameComponents& components, unsigned)
    {
        location = &QualifiedName::QualifiedNameImpl::create(components.m_prefix, components.m_localName, components.m_namespaceURI).leakRef();
    }
};

Ref<QualifiedName::QualifiedNameImpl> QualifiedNameCache::getOrCreate(const QualifiedNameComponents& components)
{
    auto addResult = m_cache.add<QNameComponentsTranslator>(components);

    if (addResult.isNewEntry) {
        auto nodeNamespace = findNamespace(components.m_namespaceURI);
        (*addResult.iterator)->m_namespace = nodeNamespace;
        (*addResult.iterator)->m_elementName = findElementName(nodeNamespace, components.m_localName);
        return adoptRef(**addResult.iterator);
    }

    return Ref { **addResult.iterator };
}

Ref<QualifiedName::QualifiedNameImpl> QualifiedNameCache::getOrCreate(const QualifiedNameComponents& components, Namespace nodeNamespace, ElementName elementName)
{
    auto addResult = m_cache.add<QNameComponentsTranslator>(components);

    if (addResult.isNewEntry) {
        (*addResult.iterator)->m_namespace = nodeNamespace;
        (*addResult.iterator)->m_elementName = elementName;
        return adoptRef(**addResult.iterator);
    }

    return Ref { **addResult.iterator };
}

void QualifiedNameCache::remove(QualifiedName::QualifiedNameImpl& impl)
{
    m_cache.remove(&impl);
}

} // namespace WebCore
