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

#if ENABLE(MUTATION_OBSERVERS)

#include "MutationRecord.h"

#include "Node.h"
#include "NodeList.h"
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

namespace {

class ChildListRecord : public MutationRecord {
public:
    ChildListRecord(PassRefPtr<Node> target, PassRefPtr<NodeList> added, PassRefPtr<NodeList> removed, PassRefPtr<Node> previousSibling, PassRefPtr<Node> nextSibling)
        : MutationRecord(target)
        , m_addedNodes(added)
        , m_removedNodes(removed)
        , m_previousSibling(previousSibling)
        , m_nextSibling(nextSibling)
    {
    }

private:
    virtual const AtomicString& type();
    virtual NodeList* addedNodes() { return m_addedNodes.get(); }
    virtual NodeList* removedNodes() { return m_removedNodes.get(); }
    virtual Node* previousSibling() { return m_previousSibling.get(); }
    virtual Node* nextSibling() { return m_nextSibling.get(); }

    RefPtr<NodeList> m_addedNodes;
    RefPtr<NodeList> m_removedNodes;
    RefPtr<Node> m_previousSibling;
    RefPtr<Node> m_nextSibling;
};

class AttributesRecord : public MutationRecord {
public:
    AttributesRecord(PassRefPtr<Node> target, const AtomicString& attributeName, const AtomicString& attributeNamespace)
        : MutationRecord(target)
        , m_attributeName(attributeName)
        , m_attributeNamespace(attributeNamespace)
    {
    }

private:
    virtual const AtomicString& type();
    virtual const AtomicString& attributeName() { return m_attributeName; }
    virtual const AtomicString& attributeNamespace() { return m_attributeName; }
    virtual String oldValue() { return m_oldValue; }
    virtual void setOldValue(const String& value) { m_oldValue = value; }

    AtomicString m_attributeName;
    AtomicString m_attributeNamespace;
    String m_oldValue;
};

class CharacterDataRecord : public MutationRecord {
public:
    CharacterDataRecord(PassRefPtr<Node> target)
        : MutationRecord(target)
    {
    }

private:
    virtual const AtomicString& type();
    virtual String oldValue() { return m_oldValue; }
    virtual void setOldValue(const String& value) { m_oldValue = value; }

    String m_oldValue;
};

const AtomicString& ChildListRecord::type()
{
    DEFINE_STATIC_LOCAL(AtomicString, childList, ("childList"));
    return childList;
}

const AtomicString& AttributesRecord::type()
{
    DEFINE_STATIC_LOCAL(AtomicString, attributes, ("attributes"));
    return attributes;
}

const AtomicString& CharacterDataRecord::type()
{
    DEFINE_STATIC_LOCAL(AtomicString, characterData, ("characterData"));
    return characterData;
}

} // namespace

PassRefPtr<MutationRecord> MutationRecord::createChildList(PassRefPtr<Node> target, PassRefPtr<NodeList> added, PassRefPtr<NodeList> removed, PassRefPtr<Node> previousSibling, PassRefPtr<Node> nextSibling)
{
    return adoptRef(static_cast<MutationRecord*>(new ChildListRecord(target, added, removed, previousSibling, nextSibling)));
}

PassRefPtr<MutationRecord> MutationRecord::createAttributes(PassRefPtr<Node> target, const AtomicString& attributeName, const AtomicString& attributeNamespace)
{
    return adoptRef(static_cast<MutationRecord*>(new AttributesRecord(target, attributeName, attributeNamespace)));
}

PassRefPtr<MutationRecord> MutationRecord::createCharacterData(PassRefPtr<Node> target)
{
    return adoptRef(static_cast<MutationRecord*>(new CharacterDataRecord(target)));
}

MutationRecord::MutationRecord(PassRefPtr<Node> target)
    : m_target(target)
{
}

MutationRecord::~MutationRecord()
{
}

} // namespace WebCore

#endif // ENABLE(MUTATION_OBSERVERS)
