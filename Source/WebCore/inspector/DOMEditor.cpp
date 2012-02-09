/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "DOMEditor.h"

#if ENABLE(INSPECTOR)

#include "DOMPatchSupport.h"
#include "Document.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "InspectorHistory.h"
#include "Node.h"
#include "Text.h"

#include "markup.h"

#include <wtf/RefPtr.h>

using namespace std;

namespace WebCore {

class DOMEditor::DOMAction : public InspectorHistory::Action {
public:
    DOMAction(const String& name) : InspectorHistory::Action(name) { }

    virtual bool perform(ErrorString* errorString)
    {
        ExceptionCode ec = 0;
        bool result = perform(ec);
        if (ec) {
            ExceptionCodeDescription description(ec);
            *errorString = description.name;
        }
        return result && !ec;
    }

    virtual bool undo(ErrorString* errorString)
    {
        ExceptionCode ec = 0;
        bool result = undo(ec);
        if (ec) {
            ExceptionCodeDescription description(ec);
            *errorString = description.name;
        }
        return result && !ec;
    }

    virtual bool perform(ExceptionCode&) = 0;

    virtual bool undo(ExceptionCode&) = 0;

private:
    RefPtr<Node> m_parentNode;
    RefPtr<Node> m_node;
    RefPtr<Node> m_anchorNode;
};

class DOMEditor::RemoveChildAction : public DOMEditor::DOMAction {
    WTF_MAKE_NONCOPYABLE(RemoveChildAction);
public:
    RemoveChildAction(Node* parentNode, Node* node)
        : DOMEditor::DOMAction("RemoveChild")
        , m_parentNode(parentNode)
        , m_node(node)
    {
    }

    virtual bool perform(ExceptionCode& ec)
    {
        m_anchorNode = m_node->nextSibling();
        return m_parentNode->removeChild(m_node.get(), ec);
    }

    virtual bool undo(ExceptionCode& ec)
    {
        return m_parentNode->insertBefore(m_node.get(), m_anchorNode.get(), ec);
    }

private:
    RefPtr<Node> m_parentNode;
    RefPtr<Node> m_node;
    RefPtr<Node> m_anchorNode;
};

class DOMEditor::InsertBeforeAction : public DOMEditor::DOMAction {
    WTF_MAKE_NONCOPYABLE(InsertBeforeAction);
public:
    InsertBeforeAction(Node* parentNode, Node* node, Node* anchorNode)
        : DOMEditor::DOMAction("InsertBefore")
        , m_parentNode(parentNode)
        , m_node(node)
        , m_anchorNode(anchorNode)
    {
    }

    virtual bool perform(ExceptionCode& ec)
    {
        if (m_node->parentNode()) {
            m_removeChildAction = adoptPtr(new RemoveChildAction(m_node->parentNode(), m_node.get()));
            if (!m_removeChildAction->perform(ec))
                return false;
        }
        return m_parentNode->insertBefore(m_node.get(), m_anchorNode.get(), ec);
    }

    virtual bool undo(ExceptionCode& ec)
    {
        if (m_removeChildAction)
            return m_removeChildAction->undo(ec);

        return m_parentNode->removeChild(m_node.get(), ec);
    }

private:
    RefPtr<Node> m_parentNode;
    RefPtr<Node> m_node;
    RefPtr<Node> m_anchorNode;
    OwnPtr<RemoveChildAction> m_removeChildAction;
};

class DOMEditor::RemoveAttributeAction : public DOMEditor::DOMAction {
    WTF_MAKE_NONCOPYABLE(RemoveAttributeAction);
public:
    RemoveAttributeAction(Element* element, const String& name)
        : DOMEditor::DOMAction("RemoveAttribute")
        , m_element(element)
        , m_name(name)
    {
    }

    virtual bool perform(ExceptionCode&)
    {
        m_value = m_element->getAttribute(m_name);
        m_element->removeAttribute(m_name);
        return true;
    }

    virtual bool undo(ExceptionCode& ec)
    {
        m_element->setAttribute(m_name, m_value, ec);
        return true;
    }

private:
    RefPtr<Element> m_element;
    String m_name;
    String m_value;
};

class DOMEditor::SetAttributeAction : public DOMEditor::DOMAction {
    WTF_MAKE_NONCOPYABLE(SetAttributeAction);
public:
    SetAttributeAction(Element* element, const String& name, const String& value)
        : DOMEditor::DOMAction("SetAttribute")
        , m_element(element)
        , m_name(name)
        , m_value(value)
        , m_hadAttribute(false)
    {
    }

    virtual bool perform(ExceptionCode& ec)
    {
        m_hadAttribute = m_element->hasAttribute(m_name);
        if (m_hadAttribute)
            m_oldValue = m_element->getAttribute(m_name);
        m_element->setAttribute(m_name, m_value, ec);
        return !ec;
    }

    virtual bool undo(ExceptionCode& ec)
    {
        if (m_hadAttribute)
            m_element->setAttribute(m_name, m_oldValue, ec);
        else
            m_element->removeAttribute(m_name);
        return true;
    }

private:
    RefPtr<Element> m_element;
    String m_name;
    String m_value;
    bool m_hadAttribute;
    String m_oldValue;
};

class DOMEditor::SetOuterHTMLAction : public DOMEditor::DOMAction {
    WTF_MAKE_NONCOPYABLE(SetOuterHTMLAction);
public:
    SetOuterHTMLAction(Node* node, const String& html)
        : DOMEditor::DOMAction("SetOuterHTML")
        , m_node(node)
        , m_nextSibling(node->nextSibling())
        , m_html(html)
        , m_newNode(0)
    {
    }

    virtual bool perform(ExceptionCode& ec)
    {
        m_oldHTML = createMarkup(m_node.get());
        DOMPatchSupport domPatchSupport(m_node->ownerDocument());
        m_newNode = domPatchSupport.patchNode(m_node.get(), m_html, ec);
        return !ec;
    }

    virtual bool undo(ExceptionCode& ec)
    {
        DOMPatchSupport domPatchSupport(m_newNode->ownerDocument());
        Node* node = domPatchSupport.patchNode(m_newNode, m_oldHTML, ec);
        if (ec || !node)
            return false;
        // HTML editing could have produced extra nodes. Remove them if necessary.
        node = node->nextSibling();

        while (!ec && node && node != m_nextSibling.get()) {
            Node* nodeToRemove = node;
            node = node->nextSibling();
            nodeToRemove->remove(ec);
        }
        return !ec;
    }

    Node* newNode()
    {
        return m_newNode;
    }

private:
    RefPtr<Node> m_node;
    RefPtr<Node> m_nextSibling;
    String m_html;
    String m_oldHTML;
    Node* m_newNode;
};

class DOMEditor::ReplaceWholeTextAction : public DOMEditor::DOMAction {
    WTF_MAKE_NONCOPYABLE(ReplaceWholeTextAction);
public:
    ReplaceWholeTextAction(Text* textNode, const String& text)
        : DOMAction("ReplaceWholeText")
        , m_textNode(textNode)
        , m_text(text)
    {
    }

    virtual bool perform(ExceptionCode& ec)
    {
        m_oldText = m_textNode->wholeText();
        m_textNode->replaceWholeText(m_text, ec);
        return true;
    }

    virtual bool undo(ExceptionCode& ec)
    {
        m_textNode->replaceWholeText(m_oldText, ec);
        return true;
    }

private:
    RefPtr<Text> m_textNode;
    String m_text;
    String m_oldText;
};

DOMEditor::DOMEditor(InspectorHistory* history) : m_history(history) { }

DOMEditor::~DOMEditor() { }

bool DOMEditor::insertBefore(Node* parentNode, Node* node, Node* anchorNode, ErrorString* errorString)
{
    return m_history->perform(adoptPtr(new InsertBeforeAction(parentNode, node, anchorNode)), errorString);
}

bool DOMEditor::removeChild(Node* parentNode, Node* node, ErrorString* errorString)
{
    return m_history->perform(adoptPtr(new RemoveChildAction(parentNode, node)), errorString);
}

bool DOMEditor::setAttribute(Element* element, const String& name, const String& value, ErrorString* errorString)
{
    return m_history->perform(adoptPtr(new SetAttributeAction(element, name, value)), errorString);
}

bool DOMEditor::removeAttribute(Element* element, const String& name, ErrorString* errorString)
{
    return m_history->perform(adoptPtr(new RemoveAttributeAction(element, name)), errorString);
}

bool DOMEditor::setOuterHTML(Node* node, const String& html, Node** newNode, ErrorString* errorString)
{
    OwnPtr<SetOuterHTMLAction> action = adoptPtr(new SetOuterHTMLAction(node, html));
    SetOuterHTMLAction* rawAction = action.get();
    bool result = m_history->perform(action.release(), errorString);
    if (result)
        *newNode = rawAction->newNode();
    return result;
}

bool DOMEditor::replaceWholeText(Text* textNode, const String& text, ErrorString* errorString)
{
    return m_history->perform(adoptPtr(new ReplaceWholeTextAction(textNode, text)), errorString);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
