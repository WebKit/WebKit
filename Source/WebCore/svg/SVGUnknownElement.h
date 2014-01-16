/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef SVGUnknownElement_h
#define SVGUnknownElement_h

#if ENABLE(SVG)

#include "SVGElement.h"

namespace WebCore {

// This type is used for 2 kinds of elements:
// - Unknown Elements in SVG namespace
// - Registered custom tag elements in SVG namespace (http://www.w3.org/TR/2013/WD-custom-elements-20130514/#registering-custom-elements)
//
// The main purpose of this class at the moment is to override rendererIsNeeded() to return
// false to make sure we don't attempt to render such elements.
class SVGUnknownElement FINAL : public SVGElement {
public:
    static PassRefPtr<SVGUnknownElement> create(const QualifiedName& tagName, Document& document)
    {
        return adoptRef(new SVGUnknownElement(tagName, document));
    }

private:
    SVGUnknownElement(const QualifiedName& tagName, Document& document)
        : SVGElement(tagName, document)
    {
    }

    virtual bool rendererIsNeeded(const RenderStyle&) override { return false; }
};

} // namespace WebCore

#endif // ENABLE(SVG)

#endif // SVGUnknownElement_h
