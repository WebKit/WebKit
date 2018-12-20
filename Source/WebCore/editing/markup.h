/*
 * Copyright (C) 2004 Apple Inc.  All rights reserved.
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

#include "ExceptionOr.h"
#include "FragmentScriptingPermission.h"
#include "HTMLInterchange.h"
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>

namespace WebCore {

class ArchiveResource;
class ContainerNode;
class Document;
class DocumentFragment;
class Element;
class Frame;
class HTMLElement;
class Node;
class Page;
class QualifiedName;
class Range;
class VisibleSelection;

void replaceSubresourceURLs(Ref<DocumentFragment>&&, HashMap<AtomicString, AtomicString>&&);
void removeSubresourceURLAttributes(Ref<DocumentFragment>&&, WTF::Function<bool(const URL&)> shouldRemoveURL);

enum class MSOListQuirks { CheckIfNeeded, Disabled };
std::unique_ptr<Page> createPageForSanitizingWebContent();
String sanitizeMarkup(const String&, MSOListQuirks = MSOListQuirks::Disabled, Optional<WTF::Function<void(DocumentFragment&)>> fragmentSanitizer = WTF::nullopt);
String sanitizedMarkupForFragmentInDocument(Ref<DocumentFragment>&&, Document&, MSOListQuirks, const String& originalMarkup);

WEBCORE_EXPORT Ref<DocumentFragment> createFragmentFromText(Range& context, const String& text);
WEBCORE_EXPORT Ref<DocumentFragment> createFragmentFromMarkup(Document&, const String& markup, const String& baseURL, ParserContentPolicy = AllowScriptingContent);
ExceptionOr<Ref<DocumentFragment>> createFragmentForInnerOuterHTML(Element&, const String& markup, ParserContentPolicy);
RefPtr<DocumentFragment> createFragmentForTransformToFragment(Document&, const String& sourceString, const String& sourceMIMEType);
Ref<DocumentFragment> createFragmentForImageAndURL(Document&, const String&);
ExceptionOr<Ref<DocumentFragment>> createContextualFragment(Element&, const String& markup, ParserContentPolicy);

bool isPlainTextMarkup(Node*);

// These methods are used by HTMLElement & ShadowRoot to replace the children with respected fragment/text.
ExceptionOr<void> replaceChildrenWithFragment(ContainerNode&, Ref<DocumentFragment>&&);

enum class ResolveURLs : uint8_t { No, Yes, YesExcludingLocalFileURLsForPrivacy };
enum class ConvertBlocksToInlines : uint8_t { No, Yes };
enum class SerializeComposedTree : uint8_t { No, Yes };
WEBCORE_EXPORT String serializePreservingVisualAppearance(const Range&, Vector<Node*>* = nullptr, AnnotateForInterchange = AnnotateForInterchange::No, ConvertBlocksToInlines = ConvertBlocksToInlines::No, ResolveURLs = ResolveURLs::No);
String serializePreservingVisualAppearance(const VisibleSelection&, ResolveURLs = ResolveURLs::No, SerializeComposedTree = SerializeComposedTree::No, Vector<Node*>* = nullptr);

enum class SerializedNodes : uint8_t { SubtreeIncludingNode, SubtreesOfChildren };
enum class SerializationSyntax : uint8_t { HTML, XML };
WEBCORE_EXPORT String serializeFragment(const Node&, SerializedNodes, Vector<Node*>* = nullptr, ResolveURLs = ResolveURLs::No, Vector<QualifiedName>* tagNamesToSkip = nullptr, SerializationSyntax = SerializationSyntax::HTML);

String urlToMarkup(const URL&, const String& title);

WEBCORE_EXPORT String documentTypeString(const Document&);

}
