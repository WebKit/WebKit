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

#pragma once

#include "ExceptionOr.h"

namespace WebCore {

class ContainerNode;
class Element;
class InspectorHistory;
class Node;
class Text;

typedef String ErrorString;

class DOMEditor {
    WTF_MAKE_NONCOPYABLE(DOMEditor); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DOMEditor(InspectorHistory&);
    ~DOMEditor();

    ExceptionOr<void> insertBefore(ContainerNode& parentNode, Ref<Node>&&, Node* anchorNode);
    ExceptionOr<void> removeChild(ContainerNode& parentNode, Node&);
    ExceptionOr<void> setAttribute(Element&, const AtomString& name, const AtomString& value);
    ExceptionOr<void> removeAttribute(Element&, const AtomString& name);
    ExceptionOr<void> setOuterHTML(Node&, const String& html, Node*& newNode);
    ExceptionOr<void> replaceWholeText(Text&, const String& text);
    ExceptionOr<void> replaceChild(ContainerNode& parentNode, Ref<Node>&& newNode, Node& oldNode);
    ExceptionOr<void> setNodeValue(Node&, const String& value);
    ExceptionOr<void> insertAdjacentHTML(Element&, const String& where, const String& html);

    bool insertBefore(ContainerNode& parentNode, Ref<Node>&&, Node* anchorNode, ErrorString&);
    bool removeChild(ContainerNode& parentNode, Node&, ErrorString&);
    bool setAttribute(Element&, const AtomString& name, const AtomString& value, ErrorString&);
    bool removeAttribute(Element&, const AtomString& name, ErrorString&);
    bool setOuterHTML(Node&, const String& html, Node*& newNode, ErrorString&);
    bool replaceWholeText(Text&, const String& text, ErrorString&);
    bool insertAdjacentHTML(Element&, const String& where, const String& html, ErrorString&);

private:
    class DOMAction;
    class InsertAdjacentHTMLAction;
    class InsertBeforeAction;
    class RemoveAttributeAction;
    class RemoveChildAction;
    class ReplaceChildNodeAction;
    class ReplaceWholeTextAction;
    class SetAttributeAction;
    class SetNodeValueAction;
    class SetOuterHTMLAction;

    InspectorHistory& m_history;
};

} // namespace WebCore
