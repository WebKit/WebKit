/*
 * Copyright (C) 2004-2024 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/CSSStyleSheet.h>
#include <WebCore/Element.h>
#include <WebCore/FloatSize.h>
#include <WebCore/HTMLInterchange.h>
#include <WebCore/MarkupExclusionRule.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/ParserContentPolicy.h>
#include <WebCore/ShadowRoot.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>

namespace WebCore {

class ArchiveResource;
class ContainerNode;
class CustomElementRegistry;
class Document;
class DocumentFragment;
class Element;
class HTMLElement;
class LocalFrame;
class Node;
class Page;
class QualifiedName;
class VisibleSelection;

struct PresentationSize;
struct SimpleRange;

template<typename> class ExceptionOr;

void replaceSubresourceURLs(Ref<DocumentFragment>&&, HashMap<RefPtr<AtomStringImpl>, RefPtr<AtomStringImpl>>&&);
void removeSubresourceURLAttributes(Ref<DocumentFragment>&&, Function<bool(const URL&)> shouldRemoveURL);

Ref<Page> createPageForSanitizingWebContent(Document* destinationDocument, std::optional<PageConfiguration>&& = { });
enum class MSOListQuirks : bool { CheckIfNeeded, Disabled };
RefPtr<StringImpl> sanitizeMarkup(StringImpl*, Document* destinationDocument, MSOListQuirks = MSOListQuirks::Disabled, NOESCAPE const Function<void(DocumentFragment&)>& fragmentSanitizer = nullptr, NOESCAPE const Function<void(Element& stagingBody)>& postLayoutSanitizer = nullptr);
WEBCORE_EXPORT RefPtr<StringImpl> sanitizeSVG(StringImpl* svg, Document* destinationDocument);
RefPtr<StringImpl> sanitizedMarkupForFragmentInDocument(Ref<DocumentFragment>&&, Document&, MSOListQuirks, StringImpl* originalMarkup, NOESCAPE const Function<void(Element& stagingBody)>& postLayoutSanitizer = nullptr);

class UserSelectNoneStateCache {
public:
    explicit UserSelectNoneStateCache(TreeType);

    bool nodeOnlyContainsUserSelectNone(Node& node) { return computeState(node) == State::OnlyUserSelectNone; }

private:
    NODELETE ContainerNode* parentNode(Node&);
    Node* firstChild(Node&);
    Node* nextSibling(Node&);

    enum class State : uint8_t { NotUserSelectNone, Mixed, OnlyUserSelectNone };
    State computeState(Node&);

    HashMap<Ref<Node>, State> m_cache;
    bool m_useComposedTree;
};

WEBCORE_EXPORT Ref<DocumentFragment> createFragmentFromText(const SimpleRange& context, StringImpl* text);
WEBCORE_EXPORT Ref<DocumentFragment> createFragmentFromMarkup(Document&, StringImpl* markup, StringImpl* baseURL, OptionSet<ParserContentPolicy> = { ParserContentPolicy::AllowScriptingContent });
ExceptionOr<Ref<DocumentFragment>> createFragmentForInnerOuterHTML(Element&, StringImpl* markup, OptionSet<ParserContentPolicy>, CustomElementRegistry*, Element::CustomElementRegistryKind = Element::CustomElementRegistryKind::Window);
RefPtr<DocumentFragment> createFragmentForTransformToFragment(Document&, RefPtr<StringImpl>&& sourceString, StringImpl* sourceMIMEType);
Ref<DocumentFragment> createFragmentForImageAndURL(Document&, StringImpl*, PresentationSize preferredSize);
ExceptionOr<Ref<DocumentFragment>> createContextualFragment(Element&, StringImpl* markup, OptionSet<ParserContentPolicy>);

bool isPlainTextMarkup(Node*);

// These methods are used by HTMLElement & ShadowRoot to replace the children with respected fragment/text.
ExceptionOr<void> replaceChildrenWithFragment(ContainerNode&, Ref<DocumentFragment>&&);

enum class ConvertBlocksToInlines : bool { No, Yes };
enum class SerializeComposedTree : bool { No, Yes };
enum class IgnoreUserSelectNone : bool { No, Yes };
enum class PreserveBaseElement : bool { No, Yes };
enum class PreserveDirectionForInlineText : bool { No, Yes };
WEBCORE_EXPORT RefPtr<StringImpl> serializePreservingVisualAppearance(const SimpleRange&, Vector<Ref<Node>>* = nullptr, AnnotateForInterchange = AnnotateForInterchange::No, ConvertBlocksToInlines = ConvertBlocksToInlines::No, ResolveURLs = ResolveURLs::NoExcludingURLsForPrivacy);
RefPtr<StringImpl> serializePreservingVisualAppearance(const VisibleSelection&, ResolveURLs = ResolveURLs::NoExcludingURLsForPrivacy, SerializeComposedTree = SerializeComposedTree::No,
    IgnoreUserSelectNone = IgnoreUserSelectNone::Yes, PreserveBaseElement = PreserveBaseElement::No, PreserveDirectionForInlineText = PreserveDirectionForInlineText::No, Vector<Ref<Node>>* = nullptr);

enum class SerializedNodes : uint8_t { SubtreeIncludingNode, SubtreesOfChildren };
enum class SerializationSyntax : bool { HTML, XML };
enum class SerializeShadowRoots : uint8_t { Explicit, Serializable, AllForInterchange };
WEBCORE_EXPORT RefPtr<StringImpl> serializeFragment(const Node&, SerializedNodes, Vector<Ref<Node>>* = nullptr, ResolveURLs = ResolveURLs::NoExcludingURLsForPrivacy, std::optional<SerializationSyntax> = std::nullopt, SerializeShadowRoots = SerializeShadowRoots::Explicit, Vector<Ref<ShadowRoot>>&& explicitShadowRoots = { }, const Vector<MarkupExclusionRule>& exclusionRules = { });
WEBCORE_EXPORT RefPtr<StringImpl> serializeFragmentWithURLReplacement(const Node&, SerializedNodes, Vector<Ref<Node>>*, ResolveURLs, std::optional<SerializationSyntax>, HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>>&& replacementURLStrings, HashMap<Ref<CSSStyleSheet>, RefPtr<StringImpl>>&& replacementURLStringsForCSSStyleSheet, SerializeShadowRoots = SerializeShadowRoots::Explicit, Vector<Ref<ShadowRoot>>&& explicitShadowRoots = { }, const Vector<MarkupExclusionRule>& exclusionRules = { });

RefPtr<StringImpl> urlToMarkup(const URL&, StringImpl* title);

WEBCORE_EXPORT RefPtr<StringImpl> documentTypeString(const Document&);

}
