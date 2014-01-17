/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef ProgressShadowElement_h
#define ProgressShadowElement_h

#if ENABLE(PROGRESS_ELEMENT)
#include "HTMLDivElement.h"
#include <wtf/Forward.h>

namespace WebCore {

class HTMLProgressElement;

class ProgressShadowElement : public HTMLDivElement {
public:
    HTMLProgressElement* progressElement() const;

protected:
    ProgressShadowElement(Document&);

private:
    virtual bool rendererIsNeeded(const RenderStyle&) override;
};

class ProgressInnerElement final : public ProgressShadowElement {
public:
    static PassRefPtr<ProgressInnerElement> create(Document&);

private:
    ProgressInnerElement(Document&);

    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual bool rendererIsNeeded(const RenderStyle&) override;
};

inline PassRefPtr<ProgressInnerElement> ProgressInnerElement::create(Document& document)
{
    RefPtr<ProgressInnerElement> result = adoptRef(new ProgressInnerElement(document));
    result->setPseudo(AtomicString("-webkit-progress-inner-element", AtomicString::ConstructFromLiteral));
    return result;
}

class ProgressBarElement final : public ProgressShadowElement {
public:
    static PassRefPtr<ProgressBarElement> create(Document&);

private:
    ProgressBarElement(Document&);
};

inline PassRefPtr<ProgressBarElement> ProgressBarElement::create(Document& document)
{
    RefPtr<ProgressBarElement> result = adoptRef(new ProgressBarElement(document));
    result->setPseudo(AtomicString("-webkit-progress-bar", AtomicString::ConstructFromLiteral));
    return result;
}

class ProgressValueElement final : public ProgressShadowElement {
public:
    static PassRefPtr<ProgressValueElement> create(Document&);
    void setWidthPercentage(double);

private:
    ProgressValueElement(Document&);
};

inline PassRefPtr<ProgressValueElement> ProgressValueElement::create(Document& document)
{
    RefPtr<ProgressValueElement> result = adoptRef(new ProgressValueElement(document));
    result->setPseudo(AtomicString("-webkit-progress-value", AtomicString::ConstructFromLiteral));
    return result;
}

}
#endif // ENABLE(PROGRESS_ELEMENT)
#endif // ProgressShadowElement_h
