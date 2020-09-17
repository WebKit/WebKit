/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "MutationObserverRegistration.h"

#include "Document.h"
#include "QualifiedName.h"

namespace WebCore {

Ref<MutationObserverRegistration> MutationObserverRegistration::create(MutationObserver& observer, Node& node, MutationObserverOptions options, const HashSet<AtomString>& attributeFilter)
{
    return adoptRef(*new MutationObserverRegistration(observer, node, options, attributeFilter));
}
    
MutationObserverRegistration::MutationObserverRegistration(MutationObserver& observer, Node& node, MutationObserverOptions options, const HashSet<AtomString>& attributeFilter)
    : m_observer(observer)
    , m_node(makeWeakPtr(node))
    , m_options(options)
    , m_attributeFilter(attributeFilter)
{
    m_observer->observationStarted(*this);
}

MutationObserverRegistration::~MutationObserverRegistration()
{
    takeTransientRegistrations();
    m_observer->observationEnded(*this);
}

void MutationObserverRegistration::resetObservation(MutationObserverOptions options, const HashSet<AtomString>& attributeFilter)
{
    takeTransientRegistrations();
    m_options = options;
    m_attributeFilter = attributeFilter;
}

void MutationObserverRegistration::observedSubtreeNodeWillDetach(Node& node)
{
    if (!isSubtree())
        return;

    node.registerTransientMutationObserver(*this);
    m_observer->setHasTransientRegistration(node.document());

    if (!m_transientRegistrationNodes)
        m_transientRegistrationNodes = makeUnique<HashSet<GCReachableRef<Node>>>();
    m_transientRegistrationNodes->add(node);
}

std::unique_ptr<HashSet<GCReachableRef<Node>>> MutationObserverRegistration::takeTransientRegistrations()
{
    if (!m_transientRegistrationNodes)
        return nullptr;

    auto transientNodes = WTFMove(m_transientRegistrationNodes);
    for (auto& node : *transientNodes)
        node->unregisterTransientMutationObserver(*this);

    return transientNodes;
}

bool MutationObserverRegistration::shouldReceiveMutationFrom(Node& node, MutationObserver::MutationType type, const QualifiedName* attributeName) const
{
    ASSERT((type == MutationObserver::Attributes && attributeName) || !attributeName);
    if (!(m_options & type))
        return false;

    if (m_node.get() != &node && !isSubtree())
        return false;

    if (type != MutationObserver::Attributes || !(m_options & MutationObserver::AttributeFilter))
        return true;

    if (!attributeName->namespaceURI().isNull())
        return false;

    return m_attributeFilter.contains(attributeName->localName());
}

void MutationObserverRegistration::addRegistrationNodesToSet(HashSet<Node*>& nodes) const
{
    ASSERT(m_node.get() || m_transientRegistrationNodes);
    if (auto* observedNode = m_node.get())
        nodes.add(observedNode);
    if (!m_transientRegistrationNodes)
        return;
    for (auto& node : *m_transientRegistrationNodes)
        nodes.add(node.ptr());
}

} // namespace WebCore
