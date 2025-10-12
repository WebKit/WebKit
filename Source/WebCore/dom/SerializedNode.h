/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class JSGlobalObject;
class JSValue;
}

namespace WebCore {

class Document;
class Node;
class QualifiedName;
class JSDOMGlobalObject;

enum class ClonedDocumentType : uint8_t;
enum class ShadowRootAvailableToElementInternals : bool;
enum class ShadowRootDelegatesFocus : bool;
enum class ShadowRootSerializable : bool;
enum class ShadowRootScopedCustomElementRegistry : bool;
enum class SlotAssignmentMode : bool;

struct SerializedNode {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED_EXPORT(SerializedNode, WEBCORE_EXPORT);
    struct QualifiedName {
        String prefix;
        String localName;
        String namespaceURI;

        QualifiedName(const WebCore::QualifiedName&);
        WEBCORE_EXPORT QualifiedName(String&&, String&&, String&&);
        QualifiedName(const QualifiedName&) = default;
        QualifiedName(QualifiedName&&) = default;
        QualifiedName& operator=(const QualifiedName&) = default;
        QualifiedName& operator=(QualifiedName&&) = default;
        WebCore::QualifiedName qualifiedName() &&;
    };
    struct Attr {
        QualifiedName name;
        String value;
    };
    struct ContainerNode {
        Vector<SerializedNode> children;
    };
    struct Document : public ContainerNode {
        ClonedDocumentType type;
        URL url;
        URL baseURL;
        URL baseURLOverride;
        Variant<String, URL> documentURI;
        String contentType;
    };
    struct DocumentFragment : public ContainerNode { };
    struct DocumentType {
        String name;
        String publicId;
        String systemId;
    };
    struct ShadowRoot : public DocumentFragment {
        bool openMode;
        SlotAssignmentMode slotAssignmentMode;
        ShadowRootDelegatesFocus delegatesFocus;
        ShadowRootSerializable serializable;
        ShadowRootAvailableToElementInternals availableToElementInternals;
        ShadowRootScopedCustomElementRegistry hasScopedCustomElementRegistry;
    };
    struct Element : public ContainerNode {
        struct Attribute {
            QualifiedName name;
            String value;
        };
        QualifiedName name;
        Vector<Attribute> attributes;
        std::optional<ShadowRoot> shadowRoot;
    };
    struct HTMLTemplateElement : public Element {
        std::optional<DocumentFragment> content;
    };
    struct CharacterData {
        String data;
    };
    struct Comment : public CharacterData { };
    struct Text : public CharacterData { };
    struct CDATASection : public Text { };
    struct ProcessingInstruction : public CharacterData {
        String target;
    };

    Variant<Attr, CDATASection, Comment, Document, DocumentFragment, DocumentType, Element, ProcessingInstruction, ShadowRoot, Text, HTMLTemplateElement> data;

    WEBCORE_EXPORT static JSC::JSValue deserialize(SerializedNode&&, JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject*, WebCore::Document&);
    static Ref<WebCore::Node> deserialize(SerializedNode&&, WebCore::Document&);
};

}
