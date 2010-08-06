/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef DOMUtilitiesPrivate_h
#define DOMUtilitiesPrivate_h

namespace WebCore {
class Element;
class HTMLInputElement;
class HTMLLinkElement;
class HTMLMetaElement;
class HTMLOptionElement;
class Node;
class QualifiedName;
class String;
}

// This file is an aggregate of useful WebCore operations.
namespace WebKit {

// If node is an HTML node with a tag name of name it is casted and returned.
// If node is not an HTML node or the tag name is not name, 0 is returned.
WebCore::HTMLInputElement* toHTMLInputElement(WebCore::Node*);
WebCore::HTMLLinkElement* toHTMLLinkElement(WebCore::Node*);
WebCore::HTMLMetaElement* toHTMLMetaElement(WebCore::Node*);
WebCore::HTMLOptionElement* toHTMLOptionElement(WebCore::Node*);

// FIXME: Deprecate. Use WebInputElement::nameForAutofill instead.
WebCore::String nameOfInputElement(WebCore::HTMLInputElement*);

// For img, script, iframe, frame element, when attribute name is src,
// for link, a, area element, when attribute name is href,
// for form element, when attribute name is action,
// for input, type=image, when attribute name is src,
// for body, table, tr, td, when attribute name is background,
// for blockquote, q, del, ins, when attribute name is cite,
// we can consider the attribute value has legal link.
bool elementHasLegalLinkAttribute(const WebCore::Element* element,
                                  const WebCore::QualifiedName& attrName);

} // namespace WebKit

#endif
