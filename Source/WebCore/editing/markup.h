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
#include <wtf/Vector.h>

namespace WebCore {

class ContainerNode;
class Document;
class DocumentFragment;
class Element;
class HTMLElement;
class URL;
class Node;
class QualifiedName;
class Range;

enum EChildrenOnly { IncludeNode, ChildrenOnly };
enum EAbsoluteURLs { DoNotResolveURLs, ResolveAllURLs, ResolveNonLocalURLs };
enum EFragmentSerialization { HTMLFragmentSerialization, XMLFragmentSerialization };

WEBCORE_EXPORT Ref<DocumentFragment> createFragmentFromText(Range& context, const String& text);
WEBCORE_EXPORT Ref<DocumentFragment> createFragmentFromMarkup(Document&, const String& markup, const String& baseURL, ParserContentPolicy = AllowScriptingContent);
ExceptionOr<Ref<DocumentFragment>> createFragmentForInnerOuterHTML(Element&, const String& markup, ParserContentPolicy);
RefPtr<DocumentFragment> createFragmentForTransformToFragment(Document&, const String& sourceString, const String& sourceMIMEType);
ExceptionOr<Ref<DocumentFragment>> createContextualFragment(Element&, const String& markup, ParserContentPolicy);

bool isPlainTextMarkup(Node*);

// These methods are used by HTMLElement & ShadowRoot to replace the children with respected fragment/text.
ExceptionOr<void> replaceChildrenWithFragment(ContainerNode&, Ref<DocumentFragment>&&);

String createMarkup(const Range&, Vector<Node*>* = nullptr, EAnnotateForInterchange = DoNotAnnotateForInterchange, bool convertBlocksToInlines = false, EAbsoluteURLs = DoNotResolveURLs);
String createMarkup(const Node&, EChildrenOnly = IncludeNode, Vector<Node*>* = nullptr, EAbsoluteURLs = DoNotResolveURLs, Vector<QualifiedName>* tagNamesToSkip = nullptr, EFragmentSerialization = HTMLFragmentSerialization);

WEBCORE_EXPORT String createFullMarkup(const Node&);
WEBCORE_EXPORT String createFullMarkup(const Range&);

String urlToMarkup(const URL&, const String& title);

String documentTypeString(const Document&);

}
