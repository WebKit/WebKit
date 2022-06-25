/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
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

#include "FrameDestructionObserver.h"
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class HTMLFormElement;

enum FormSubmissionTrigger { SubmittedByJavaScript, NotSubmittedByJavaScript };

using StringPairVector = Vector<std::pair<String, String>>;

class FormState : public RefCounted<FormState>, public CanMakeWeakPtr<FormState>, public FrameDestructionObserver {
public:
    static Ref<FormState> create(HTMLFormElement&, StringPairVector&& textFieldValues, Document&, FormSubmissionTrigger);

    HTMLFormElement& form() const { return m_form; }
    const StringPairVector& textFieldValues() const { return m_textFieldValues; }
    Document& sourceDocument() const { return m_sourceDocument; }
    FormSubmissionTrigger formSubmissionTrigger() const { return m_formSubmissionTrigger; }

private:
    FormState(HTMLFormElement&, StringPairVector&& textFieldValues, Document&, FormSubmissionTrigger);
    void willDetachPage() override;

    Ref<HTMLFormElement> m_form;
    StringPairVector m_textFieldValues;
    Ref<Document> m_sourceDocument;
    FormSubmissionTrigger m_formSubmissionTrigger;
};

} // namespace WebCore
