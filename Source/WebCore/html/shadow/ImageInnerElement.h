/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef ImageInnerElement_h
#define ImageInnerElement_h

#include "HTMLImageElement.h"

namespace WebCore {

class HTMLImageElement;
class ImageLoader;
class RenderObject;

class ImageInnerElement : public HTMLElement, public ImageElement {
public:
    static PassRefPtr<ImageInnerElement> create(Document*);

private:
    ImageInnerElement(Document*);

    HTMLImageElement* hostImage();

    ImageLoader* imageLoader();
    RenderObject* imageRenderer() const { return HTMLElement::renderer(); }

    virtual void attach() OVERRIDE;
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*) OVERRIDE;

    virtual bool canStartSelection() const { return false; }

    virtual const AtomicString& shadowPseudoId() const OVERRIDE;
};

inline PassRefPtr<ImageInnerElement> ImageInnerElement::create(Document* document)
{
    return adoptRef(new ImageInnerElement(document));
}

inline bool isImageInnerElement(Node* node)
{
    return !node || node->hasTagName(HTMLNames::webkitInnerImageTag);
}

inline ImageInnerElement* toImageInnerElement(Node* node)
{
    ASSERT(isImageInnerElement(node));
    return static_cast<ImageInnerElement*>(node);
}

}

#endif
