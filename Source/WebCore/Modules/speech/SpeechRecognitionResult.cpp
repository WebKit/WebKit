/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SCRIPTED_SPEECH)

#include "SpeechRecognitionResult.h"

#include "Document.h"
#include "Element.h"
#include "Text.h"

namespace WebCore {

SpeechRecognitionResult::~SpeechRecognitionResult()
{
}

PassRefPtr<SpeechRecognitionResult> SpeechRecognitionResult::create(const Vector<RefPtr<SpeechRecognitionAlternative> >& alternatives, bool final)
{
    return adoptRef(new SpeechRecognitionResult(alternatives, final));
}

SpeechRecognitionAlternative* SpeechRecognitionResult::item(unsigned long index)
{
    if (index >= m_alternatives.size())
        return 0;

    return m_alternatives[index].get();
}

static QualifiedName emmaQualifiedName(const char* localName)
{
    const char emmaNamespaceUrl[] = "http://www.w3.org/2003/04/emma";
    return QualifiedName("emma", localName, emmaNamespaceUrl);
}

Document* SpeechRecognitionResult::emma()
{
    if (m_emma)
        return m_emma.get();

    RefPtr<Document> document = Document::create(0, KURL());

    RefPtr<Element> emmaElement = document->createElement(emmaQualifiedName("emma"), false);
    ExceptionCode ec = 0;
    emmaElement->setAttribute("version", "1.0", ec);
    ASSERT(!ec);
    if (ec)
        return 0;

    RefPtr<Element> oneOf = document->createElement(emmaQualifiedName("one-of"), false);
    oneOf->setAttribute(emmaQualifiedName("medium"), "acoustic");
    oneOf->setAttribute(emmaQualifiedName("mode"), "voice");
    oneOf->setIdAttribute("one-of");

    for (size_t i = 0; i < m_alternatives.size(); ++i) {
        const RefPtr<SpeechRecognitionAlternative>& alternative = m_alternatives[i];

        RefPtr<Element> interpretation = document->createElement(emmaQualifiedName("interpretation"), false);
        interpretation->setIdAttribute(String::number(i + 1));
        interpretation->setAttribute(emmaQualifiedName("confidence"), String::number(alternative->confidence()));

        RefPtr<Element> literal = document->createElement(emmaQualifiedName("literal"), false);
        literal->appendChild(document->createTextNode(alternative->transcript()));
        interpretation->appendChild(literal.release());
        oneOf->appendChild(interpretation.release());
    }

    emmaElement->appendChild(oneOf.release());
    document->appendChild(emmaElement.release());

    m_emma = document;
    return m_emma.get();
}

SpeechRecognitionResult::SpeechRecognitionResult(const Vector<RefPtr<SpeechRecognitionAlternative> >& alternatives, bool final)
    : m_alternatives(alternatives)
    , m_final(final)
{
}

} // namespace WebCore

#endif // ENABLE(SCRIPTED_SPEECH)
