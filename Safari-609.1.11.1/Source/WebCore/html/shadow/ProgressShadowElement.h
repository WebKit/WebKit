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

#pragma once

#include "HTMLDivElement.h"
#include <wtf/Forward.h>

namespace WebCore {

class HTMLProgressElement;

class ProgressShadowElement : public HTMLDivElement {
    WTF_MAKE_ISO_ALLOCATED(ProgressShadowElement);
public:
    HTMLProgressElement* progressElement() const;

protected:
    ProgressShadowElement(Document&);

private:
    bool rendererIsNeeded(const RenderStyle&) override;
};

// The subclasses of ProgressShadowElement share the same isoheap, because they don't add any more
// fields to the class.

class ProgressInnerElement final : public ProgressShadowElement {
public:
    static Ref<ProgressInnerElement> create(Document&);

private:
    ProgressInnerElement(Document&);

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool rendererIsNeeded(const RenderStyle&) override;
};
static_assert(sizeof(ProgressInnerElement) == sizeof(ProgressShadowElement));

inline Ref<ProgressInnerElement> ProgressInnerElement::create(Document& document)
{
    Ref<ProgressInnerElement> result = adoptRef(*new ProgressInnerElement(document));
    result->setPseudo(AtomString("-webkit-progress-inner-element", AtomString::ConstructFromLiteral));
    return result;
}

class ProgressBarElement final : public ProgressShadowElement {
public:
    static Ref<ProgressBarElement> create(Document&);

private:
    ProgressBarElement(Document&);
};
static_assert(sizeof(ProgressBarElement) == sizeof(ProgressShadowElement));

inline Ref<ProgressBarElement> ProgressBarElement::create(Document& document)
{
    Ref<ProgressBarElement> result = adoptRef(*new ProgressBarElement(document));
    result->setPseudo(AtomString("-webkit-progress-bar", AtomString::ConstructFromLiteral));
    return result;
}

class ProgressValueElement final : public ProgressShadowElement {
public:
    static Ref<ProgressValueElement> create(Document&);
    void setWidthPercentage(double);

private:
    ProgressValueElement(Document&);
};
static_assert(sizeof(ProgressValueElement) == sizeof(ProgressShadowElement));

inline Ref<ProgressValueElement> ProgressValueElement::create(Document& document)
{
    Ref<ProgressValueElement> result = adoptRef(*new ProgressValueElement(document));
    result->setPseudo(AtomString("-webkit-progress-value", AtomString::ConstructFromLiteral));
    return result;
}

} // namespace WebCore
