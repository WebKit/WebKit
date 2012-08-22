/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL GOOGLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMTransactionStep_h
#define DOMTransactionStep_h

#if ENABLE(UNDO_MANAGER)

#include "PlatformString.h"
#include "QualifiedName.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CharacterData;
class Element;
class Node;

class DOMTransactionStep : public RefCounted<DOMTransactionStep> {
public:
    virtual ~DOMTransactionStep() { }

    virtual void unapply() = 0;
    virtual void reapply() = 0;
};

class NodeInsertingDOMTransactionStep : public DOMTransactionStep {
public:
    static PassRefPtr<NodeInsertingDOMTransactionStep> create(Node*, Node* child);
    virtual void unapply() OVERRIDE;
    virtual void reapply() OVERRIDE;

private:
    NodeInsertingDOMTransactionStep(Node*, Node* child);
    
    RefPtr<Node> m_node;
    RefPtr<Node> m_refChild;
    RefPtr<Node> m_child;
};

class NodeRemovingDOMTransactionStep : public DOMTransactionStep {
public:
    static PassRefPtr<NodeRemovingDOMTransactionStep> create(Node*, Node* child);
    virtual void unapply() OVERRIDE;
    virtual void reapply() OVERRIDE;
    
private:
    NodeRemovingDOMTransactionStep(Node*, Node* child);
    
    RefPtr<Node> m_node;
    RefPtr<Node> m_refChild;
    RefPtr<Node> m_child;
};

class DataReplacingDOMTransactionStep : public DOMTransactionStep {
public:
    static PassRefPtr<DataReplacingDOMTransactionStep> create(CharacterData*, unsigned offset, unsigned count, const String& data, const String& replacedData);
    virtual void unapply() OVERRIDE;
    virtual void reapply() OVERRIDE;
    
private:
    DataReplacingDOMTransactionStep(CharacterData*, unsigned offset, unsigned count, const String& data, const String& replacedData);
    
    RefPtr<CharacterData> m_node;
    unsigned m_offset;
    unsigned m_count;
    String m_data;
    String m_replacedData;
};

class AttrChangingDOMTransactionStep : public DOMTransactionStep {
public:
    static PassRefPtr<AttrChangingDOMTransactionStep> create(Element*, const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue);
    virtual void unapply() OVERRIDE;
    virtual void reapply() OVERRIDE;
    
private:
    AttrChangingDOMTransactionStep(Element*, const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue);
    
    RefPtr<Element> m_element;
    QualifiedName m_name;
    AtomicString m_oldValue;
    AtomicString m_newValue;
};

}

#endif

#endif
