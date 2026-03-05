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
    element.parserSetAttributes(WTF::map(WTF::move(attributes), [] (auto&& attribute) {
        return Attribute(WTF::move(attribute.name).qualifiedName(), AtomString(WTF::move(attribute.value)));
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

    Ref node = WTF::switchOn(WTF::move(serializedNode.data), [&] (SerializedNode::Text&& text) -> Ref<Node> {
        return WebCore::Text::create(document, WTF::move(text.data));
    }, [&] (SerializedNode::ProcessingInstruction&& instruction) -> Ref<Node> {
        return WebCore::ProcessingInstruction::create(document, WTF::move(instruction.target), WTF::move(instruction.data));
    }, [&] (SerializedNode::DocumentType&& type) -> Ref<Node> {
        return WebCore::DocumentType::create(document, type.name, type.publicId, type.systemId);
    }, [&] (SerializedNode::Comment&& comment) -> Ref<Node> {
        return WebCore::Comment::create(document, WTF::move(comment.data));
    }, [&] (SerializedNode::CDATASection&& section) -> Ref<Node> {
        return WebCore::CDATASection::create(document, WTF::move(section.data));
    }, [&] (SerializedNode::Attr&& attr) -> Ref<Node> {
        return WebCore::Attr::create(document, WTF::move(attr.name).qualifiedName(), AtomString(WTF::move(attr.value)));
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
            protect(document.decoder()).get()
        );
    }, [&] (SerializedNode::Element&& element) -> Ref<Node> {
        constexpr bool createdByParser { false };
        Ref result = document.createElement(WTF::move(element.name).qualifiedName(), createdByParser);
        setAttributes(result, WTF::move(element.attributes));
        addShadowRootIfNecessary(result, WTF::move(element.shadowRoot));
        return result;
    }, [&] (SerializedNode::HTMLTemplateElement&& element) -> Ref<Node> {
        ASSERT(!element.shadowRoot);
        Ref result = WebCore::HTMLTemplateElement::create(WTF::move(element.name).qualifiedName(), document);
        setAttributes(result, WTF::move(element.attributes));
        if (element.content) {
            Ref content = TemplateContentDocumentFragment::create(Ref { document.ensureTemplateDocument() }.get(), result);
            for (auto&& child : std::exchange(element.content->children, { })) {
                if (RefPtr childNode = deserialize(WTF::move(child), document)) {
                    childNode->setTreeScopeRecursively(protect(content->treeScope()));
                    content->appendChildCommon(*childNode);
                }
            }
            result->adoptDeserializedContent(WTF::move(content));
        }
        return result;
    }, [&] (SerializedNode::DocumentFragment&&) -> Ref<Node> {
        return WebCore::DocumentFragment::create(document);
    }, [&] (SerializedNode::ShadowRoot&&) -> Ref<Node> {
        // FIXME: Remove from variant and change the shape of the node cloning code to match.
        RELEASE_ASSERT_NOT_REACHED(); // ShadowRoot is never serialized directly on its own.
    });

    RefPtr containerNode = dynamicDowncast<WebCore::ContainerNode>(node);
    for (auto&& child : WTF::move(serializedChildren)) {
        Ref childNode = deserialize(WTF::move(child), document);
        childNode->setTreeScopeRecursively(protect(containerNode->treeScope()));
        containerNode->appendChildCommon(childNode);
    }

    return node;
}

JSC::JSValue SerializedNode::deserialize(SerializedNode&& serializedNode, JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* domGlobalObject, WebCore::Document& document)
{
    return toJSNewlyCreated(lexicalGlobalObject, domGlobalObject, deserialize(WTF::move(serializedNode), document));
}

SerializedNode::QualifiedName::QualifiedName(const WebCore::QualifiedName& name)
    : prefix(name.prefix())
    , localName(name.localName())
    , namespaceURI(name.namespaceURI())
{
}

SerializedNode::QualifiedName::QualifiedName(String&& prefix, String&& localName, String&& namespaceURI)
    : prefix(WTF::move(prefix))
    , localName(WTF::move(localName))
    , namespaceURI(WTF::move(namespaceURI))
{
}

QualifiedName SerializedNode::QualifiedName::qualifiedName() &&
{
    return WebCore::QualifiedName(AtomString(WTF::move(prefix)), AtomString(WTF::move(localName)), AtomString(WTF::move(namespaceURI)));
}

}
