/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextDocumentParser.h"

#include "HTMLTreeBuilder.h"
#include "ScriptElement.h"

namespace WebCore {

using namespace HTMLNames;

TextDocumentParser::TextDocumentParser(HTMLDocument& document)
    : HTMLDocumentParser(document)
{
}

void TextDocumentParser::append(RefPtr<StringImpl>&& text)
{
    if (!m_hasInsertedFakeFormattingElements)
        insertFakeFormattingElements();
    HTMLDocumentParser::append(WTFMove(text));
}

void TextDocumentParser::insertFakeFormattingElements()
{
    // In principle, we should create a specialized tree builder for
    // TextDocuments, but instead we re-use the existing HTMLTreeBuilder.
    // We create fake tokens and give it to the tree builder rather than
    // sending fake bytes through the front-end of the parser to avoid
    // distrubing the line/column number calculations.

    Attribute nameAttribute(nameAttr, "color-scheme"_s);
    Attribute contentAttribute(contentAttr, "light dark"_s);
    AtomHTMLToken fakeMeta(HTMLToken::Type::StartTag, TagName::meta, { WTFMove(nameAttribute), WTFMove(contentAttribute) });
    treeBuilder().constructTree(WTFMove(fakeMeta));

    Attribute attribute(styleAttr, "word-wrap: break-word; white-space: pre-wrap;"_s);
    AtomHTMLToken fakePre(HTMLToken::Type::StartTag, TagName::pre, { WTFMove(attribute) });
    treeBuilder().constructTree(WTFMove(fakePre));

    // Normally we would skip the first \n after a <pre> element, but we don't
    // want to skip the first \n for text documents!
    treeBuilder().setShouldSkipLeadingNewline(false);

    // Although Text Documents expose a "pre" element in their DOM, they
    // act like a <plaintext> tag, so we have to force plaintext mode.
    tokenizer().setPLAINTEXTState();

    m_hasInsertedFakeFormattingElements = true;
}

}
