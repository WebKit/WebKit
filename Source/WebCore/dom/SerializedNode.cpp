/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "SerializedNode.h"

#include "Attr.h"
#include "CDATASection.h"
#include "Comment.h"
#include "CustomElementRegistry.h"
#include "DocumentFragment.h"
#include "DocumentInlines.h"
#include "DocumentType.h"
#include "HTMLAttachmentElement.h"
#include "HTMLScriptElement.h"
#include "HTMLTemplateElement.h"
#include "JSNode.h"
#include "NodeDocument.h"
#include "ProcessingInstruction.h"
#include "QualifiedName.h"
#include "SVGScriptElement.h"
#include "SecurityOriginPolicy.h"
#include "ShadowRoot.h"
#include "TemplateContentDocumentFragment.h"
#include "Text.h"
#include "TextResourceDecoder.h"
#include "WebVTTElement.h"

namespace WebCore {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(SerializedNode);

static void setAttributes(Element& element, Vector<SerializedNode::Element::Attribute>&& attributes)
{
    element.parserSetAttributes(WTF::map(WTFMove(attributes), [] (auto&& attribute) {
        return Attribute(WTFMove(attribute.name).qualifiedName(), AtomString(WTFMove(attribute.value)));
    }).span());
}

static void addShadowRootIfNecessary(Element& element, std::optional<SerializedNode::ShadowRoot>&& serializedRoot)
{
    if (!serializedRoot)
        return;

    element.addShadowRoot(WebCore::ShadowRoot::create(
        Ref { element.document() }.get(),
        serializedRoot->openMode ? ShadowRootMode::Open : ShadowRootMode::Closed,
        serializedRoot->slotAssignmentMode,
        serializedRoot->delegatesFocus,
        WebCore::ShadowRoot::Clonable::Yes,
        serializedRoot->serializable,
        serializedRoot->availableToElementInternals,
        nullptr,
        serializedRoot->hasScopedCustomElementRegistry
    ));
}

Ref<Node> SerializedNode::deserialize(SerializedNode&& serializedNode, WebCore::Document& document)
{
    auto serializedChildren = WTF::switchOn(serializedNode.data, [&] (SerializedNode::ContainerNode& containerNode) {
        return std::exchange(containerNode.children, { });
    }, []<typename T>(const T&) requires (!std::derived_from<T, SerializedNode::ContainerNode>) {
        return Vector<SerializedNode> { };
    });

    Ref node = WTF::switchOn(WTFMove(serializedNode.data), [&] (SerializedNode::Text&& text) -> Ref<Node> {
        return WebCore::Text::create(document, WTFMove(text.data));
    }, [&] (SerializedNode::ProcessingInstruction&& instruction) -> Ref<Node> {
        return WebCore::ProcessingInstruction::create(document, WTFMove(instruction.target), WTFMove(instruction.data));
    }, [&] (SerializedNode::DocumentType&& type) -> Ref<Node> {
        return WebCore::DocumentType::create(document, type.name, type.publicId, type.systemId);
    }, [&] (SerializedNode::Comment&& comment) -> Ref<Node> {
        return WebCore::Comment::create(document, WTFMove(comment.data));
    }, [&] (SerializedNode::CDATASection&& section) -> Ref<Node> {
        return WebCore::CDATASection::create(document, WTFMove(section.data));
    }, [&] (SerializedNode::Attr&& attr) -> Ref<Node> {
        return WebCore::Attr::create(document, WTFMove(attr.name).qualifiedName(), AtomString(WTFMove(attr.value)));
    }, [&] (SerializedNode::Document&& serializedDocument) -> Ref<Node> {
        return WebCore::Document::createCloned(
            serializedDocument.type,
            document.settings(),
            serializedDocument.url,
            serializedDocument.baseURL,
            serializedDocument.baseURLOverride,
            serializedDocument.documentURI,
            document.compatibilityMode(),
            document,
            RefPtr { document.securityOriginPolicy() }.get(),
            serializedDocument.contentType,
            document.protectedDecoder().get()
        );
    }, [&] (SerializedNode::Element&& element) -> Ref<Node> {
        constexpr bool createdByParser { false };
        Ref result = document.createElement(WTFMove(element.name).qualifiedName(), createdByParser);
        setAttributes(result, WTFMove(element.attributes));
        addShadowRootIfNecessary(result, WTFMove(element.shadowRoot));
        return result;
    }, [&] (SerializedNode::HTMLTemplateElement&& element) -> Ref<Node> {
        Ref result = WebCore::HTMLTemplateElement::create(WTFMove(element.name).qualifiedName(), document);
        setAttributes(result, WTFMove(element.attributes));
        addShadowRootIfNecessary(result, WTFMove(element.shadowRoot));
        if (element.content) {
            Ref content = TemplateContentDocumentFragment::create(Ref { document.ensureTemplateDocument() }.get(), result);
            for (auto&& child : std::exchange(element.content->children, { })) {
                if (RefPtr childNode = deserialize(WTFMove(child), document)) {
                    childNode->setTreeScopeRecursively(content->protectedTreeScope());
                    content->appendChildCommon(*childNode);
                }
            }
            result->adoptDeserializedContent(WTFMove(content));
        }
        return result;
    }, [&] (SerializedNode::DocumentFragment&&) -> Ref<Node> {
        return WebCore::DocumentFragment::create(document);
    }, [&] (SerializedNode::ShadowRoot&&) -> Ref<Node> {
        // FIXME: Remove from variant and change the shape of the node cloning code to match.
        RELEASE_ASSERT_NOT_REACHED(); // ShadowRoot is never serialized directly on its own.
    });

    RefPtr containerNode = dynamicDowncast<WebCore::ContainerNode>(node);
    for (auto&& child : WTFMove(serializedChildren)) {
        Ref childNode = deserialize(WTFMove(child), document);
        childNode->setTreeScopeRecursively(containerNode->protectedTreeScope());
        containerNode->appendChildCommon(childNode);
    }

    return node;
}

JSC::JSValue SerializedNode::deserialize(SerializedNode&& serializedNode, JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* domGlobalObject, WebCore::Document& document)
{
    return toJSNewlyCreated(lexicalGlobalObject, domGlobalObject, deserialize(WTFMove(serializedNode), document));
}

SerializedNode::QualifiedName::QualifiedName(const WebCore::QualifiedName& name)
    : prefix(name.prefix())
    , localName(name.localName())
    , namespaceURI(name.namespaceURI())
{
}

SerializedNode::QualifiedName::QualifiedName(String&& prefix, String&& localName, String&& namespaceURI)
    : prefix(WTFMove(prefix))
    , localName(WTFMove(localName))
    , namespaceURI(WTFMove(namespaceURI))
{
}

QualifiedName SerializedNode::QualifiedName::qualifiedName() &&
{
    return WebCore::QualifiedName(AtomString(WTFMove(prefix)), AtomString(WTFMove(localName)), AtomString(WTFMove(namespaceURI)));
}

}
