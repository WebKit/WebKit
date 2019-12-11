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

#include "DOMException.h"
#include "DOMPatchSupport.h"
#include "Document.h"
#include "Element.h"
#include "InspectorHistory.h"
#include "Node.h"
#include "Text.h"
#include "markup.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class DOMEditor::RemoveChildAction final : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(RemoveChildAction);
public:
    RemoveChildAction(Node& parentNode, Node& node)
        : InspectorHistory::Action()
        , m_parentNode(parentNode)
        , m_node(node)
    {
    }

    ExceptionOr<void> perform() final
    {
        m_anchorNode = m_node->nextSibling();
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        return m_parentNode->insertBefore(m_node, m_anchorNode.get());
    }

    ExceptionOr<void> redo() final
    {
        return m_parentNode->removeChild(m_node);
    }

private:
    Ref<Node> m_parentNode;
    Ref<Node> m_node;
    RefPtr<Node> m_anchorNode;
};

class DOMEditor::InsertBeforeAction final : public InspectorHistory::Action {
public:
    InsertBeforeAction(Node& parentNode, Ref<Node>&& node, Node* anchorNode)
        : InspectorHistory::Action()
        , m_parentNode(parentNode)
        , m_node(WTFMove(node))
        , m_anchorNode(anchorNode)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        if (m_node->parentNode()) {
            m_removeChildAction = makeUnique<RemoveChildAction>(*m_node->parentNode(), m_node);
            auto result = m_removeChildAction->perform();
            if (result.hasException())
                return result.releaseException();
        }
        return m_parentNode->insertBefore(m_node, m_anchorNode.get());
    }

    ExceptionOr<void> undo() final
    {
        auto result = m_parentNode->removeChild(m_node);
        if (result.hasException())
            return result.releaseException();
        if (!m_removeChildAction)
            return { };
        return m_removeChildAction->undo();
    }

    ExceptionOr<void> redo() final
    {
        if (m_removeChildAction) {
            auto result = m_removeChildAction->redo();
            if (result.hasException())
                return result.releaseException();
        }
        return m_parentNode->insertBefore(m_node, m_anchorNode.get());
    }

    Ref<Node> m_parentNode;
    Ref<Node> m_node;
    RefPtr<Node> m_anchorNode;
    std::unique_ptr<RemoveChildAction> m_removeChildAction;
};

class DOMEditor::RemoveAttributeAction final : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(RemoveAttributeAction);
public:
    RemoveAttributeAction(Element& element, const String& name)
        : InspectorHistory::Action()
        , m_element(element)
        , m_name(name)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        m_value = m_element->getAttribute(m_name);
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        return m_element->setAttribute(m_name, m_value);
    }

    ExceptionOr<void> redo() final
    {
        m_element->removeAttribute(m_name);
        return { };
    }

    Ref<Element> m_element;
    String m_name;
    String m_value;
};

class DOMEditor::SetAttributeAction final : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(SetAttributeAction);
public:
    SetAttributeAction(Element& element, const AtomString& name, const AtomString& value)
        : InspectorHistory::Action()
        , m_element(element)
        , m_name(name)
        , m_value(value)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        m_oldValue = m_element->getAttribute(m_name);
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        if (m_oldValue.isNull()) {
            m_element->removeAttribute(m_name);
            return { };
        }
        return m_element->setAttribute(m_name, m_oldValue);
    }

    ExceptionOr<void> redo() final
    {
        return m_element->setAttribute(m_name, m_value);
    }

    Ref<Element> m_element;
    AtomString m_name;
    AtomString m_value;
    AtomString m_oldValue;
};

class DOMEditor::SetOuterHTMLAction final : public InspectorHistory::Action {
public:
    SetOuterHTMLAction(Node& node, const String& html)
        : InspectorHistory::Action()
        , m_node(node)
        , m_nextSibling(node.nextSibling())
        , m_html(html)
    {
    }

    Node* newNode() const
    {
        return m_newNode.get();
    }

private:
    ExceptionOr<void> perform() final
    {
        m_oldHTML = serializeFragment(m_node.get(), SerializedNodes::SubtreeIncludingNode);
        auto result = DOMPatchSupport { m_domEditor, m_node->document() }.patchNode(m_node, m_html);
        if (result.hasException())
            return result.releaseException();
        m_newNode = result.releaseReturnValue();
        return { };
    }

    ExceptionOr<void> undo() final
    {
        return m_history.undo();
    }

    ExceptionOr<void> redo() final
    {
        return m_history.redo();
    }

    Ref<Node> m_node;
    RefPtr<Node> m_nextSibling;
    String m_html;
    String m_oldHTML;
    RefPtr<Node> m_newNode { nullptr };
    InspectorHistory m_history;
    DOMEditor m_domEditor { m_history };
};

class DOMEditor::InsertAdjacentHTMLAction final : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(InsertAdjacentHTMLAction);
public:
    InsertAdjacentHTMLAction(Element& element, const String& position, const String& html)
        : InspectorHistory::Action()
        , m_element(element)
        , m_position(position)
        , m_html(html)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        for (auto& addedNode : m_addedNodes)
            addedNode->remove();
        m_addedNodes.clear();
        return { };
    }

    ExceptionOr<void> redo() final
    {
        auto result = m_element->insertAdjacentHTML(m_position, m_html, &m_addedNodes);
        if (result.hasException())
            return result.releaseException();
        return { };
    }

    Ref<Element> m_element;
    NodeVector m_addedNodes;
    String m_position;
    String m_html;
};

class DOMEditor::ReplaceWholeTextAction final : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(ReplaceWholeTextAction);
public:
    ReplaceWholeTextAction(Text& textNode, const String& text)
        : InspectorHistory::Action()
        , m_textNode(textNode)
        , m_text(text)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        m_oldText = m_textNode->wholeText();
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        m_textNode->replaceWholeText(m_oldText);
        return { };
    }

    ExceptionOr<void> redo() final
    {
        m_textNode->replaceWholeText(m_text);
        return { };
    }

    Ref<Text> m_textNode;
    String m_text;
    String m_oldText;
};

class DOMEditor::ReplaceChildNodeAction final: public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(ReplaceChildNodeAction);
public:
    ReplaceChildNodeAction(Node& parentNode, Ref<Node>&& newNode, Node& oldNode)
        : InspectorHistory::Action()
        , m_parentNode(parentNode)
        , m_newNode(WTFMove(newNode))
        , m_oldNode(oldNode)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        return m_parentNode->replaceChild(m_oldNode, m_newNode);
    }

    ExceptionOr<void> redo() final
    {
        return m_parentNode->replaceChild(m_newNode, m_oldNode);
    }

    Ref<Node> m_parentNode;
    Ref<Node> m_newNode;
    Ref<Node> m_oldNode;
};

class DOMEditor::SetNodeValueAction final : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(SetNodeValueAction);
public:
    SetNodeValueAction(Node& node, const String& value)
        : InspectorHistory::Action()
        , m_node(node)
        , m_value(value)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        m_oldValue = m_node->nodeValue();
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        return m_node->setNodeValue(m_oldValue);
    }

    ExceptionOr<void> redo() final
    {
        return m_node->setNodeValue(m_value);
    }

    Ref<Node> m_node;
    String m_value;
    String m_oldValue;
};

DOMEditor::DOMEditor(InspectorHistory& history)
    : m_history(history)
{
}

DOMEditor::~DOMEditor() = default;

ExceptionOr<void> DOMEditor::insertBefore(Node& parentNode, Ref<Node>&& node, Node* anchorNode)
{
    return m_history.perform(makeUnique<InsertBeforeAction>(parentNode, WTFMove(node), anchorNode));
}

ExceptionOr<void> DOMEditor::removeChild(Node& parentNode, Node& node)
{
    return m_history.perform(makeUnique<RemoveChildAction>(parentNode, node));
}

ExceptionOr<void> DOMEditor::setAttribute(Element& element, const String& name, const String& value)
{
    return m_history.perform(makeUnique<SetAttributeAction>(element, name, value));
}

ExceptionOr<void> DOMEditor::removeAttribute(Element& element, const String& name)
{
    return m_history.perform(makeUnique<RemoveAttributeAction>(element, name));
}

ExceptionOr<void> DOMEditor::setOuterHTML(Node& node, const String& html, Node*& newNode)
{
    auto action = makeUnique<SetOuterHTMLAction>(node, html);
    auto& rawAction = *action;
    auto result = m_history.perform(WTFMove(action));
    if (!result.hasException())
        newNode = rawAction.newNode();
    return result;
}

ExceptionOr<void> DOMEditor::insertAdjacentHTML(Element& element, const String& where, const String& html)
{
    return m_history.perform(makeUnique<InsertAdjacentHTMLAction>(element, where, html));
}

ExceptionOr<void> DOMEditor::replaceWholeText(Text& textNode, const String& text)
{
    return m_history.perform(makeUnique<ReplaceWholeTextAction>(textNode, text));
}

ExceptionOr<void> DOMEditor::replaceChild(Node& parentNode, Ref<Node>&& newNode, Node& oldNode)
{
    return m_history.perform(makeUnique<ReplaceChildNodeAction>(parentNode, WTFMove(newNode), oldNode));
}

ExceptionOr<void> DOMEditor::setNodeValue(Node& node, const String& value)
{
    return m_history.perform(makeUnique<SetNodeValueAction>(node, value));
}

static bool populateErrorString(ExceptionOr<void>&& result, ErrorString& errorString)
{
    if (!result.hasException())
        return true;
    errorString = DOMException::name(result.releaseException().code());
    return false;
}

bool DOMEditor::insertBefore(Node& parentNode, Ref<Node>&& node, Node* anchorNode, ErrorString& errorString)
{
    return populateErrorString(insertBefore(parentNode, WTFMove(node), anchorNode), errorString);
}

bool DOMEditor::removeChild(Node& parentNode, Node& node, ErrorString& errorString)
{
    return populateErrorString(removeChild(parentNode, node), errorString);
}

bool DOMEditor::setAttribute(Element& element, const String& name, const String& value, ErrorString& errorString)
{
    return populateErrorString(setAttribute(element, name, value), errorString);
}

bool DOMEditor::removeAttribute(Element& element, const String& name, ErrorString& errorString)
{
    return populateErrorString(removeAttribute(element, name), errorString);
}

bool DOMEditor::setOuterHTML(Node& node, const String& html, Node*& newNode, ErrorString& errorString)
{
    return populateErrorString(setOuterHTML(node, html, newNode), errorString);
}

bool DOMEditor::insertAdjacentHTML(Element& element, const String& where, const String& html, ErrorString& errorString)
{
    return populateErrorString(insertAdjacentHTML(element, where, html), errorString);
}

bool DOMEditor::replaceWholeText(Text& textNode, const String& text, ErrorString& errorString)
{
    return populateErrorString(replaceWholeText(textNode, text), errorString);
}

} // namespace WebCore
