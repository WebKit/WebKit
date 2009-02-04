/*
 * Copyright (C) 2008 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef RenderImageGeneratedContent_h
#define RenderImageGeneratedContent_h

#include "RenderImage.h"
#include "StyleImage.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class StyleImage;

class RenderImageGeneratedContent : public RenderImage {
public:
    RenderImageGeneratedContent(Node*);
    virtual ~RenderImageGeneratedContent();
    
    void setStyleImage(StyleImage*);
    
    virtual bool hasImage() const { return true; }
    
protected:
    virtual Image* image(int w = 0, int h = 0) { return m_styleImage->image(this, IntSize(w, h)); }
    virtual bool errorOccurred() const { return m_styleImage->errorOccurred(); }
    virtual bool usesImageContainerSize() const { return m_styleImage->usesImageContainerSize(); }
    virtual void setImageContainerSize(const IntSize& size) const { m_styleImage->setImageContainerSize(size); }
    virtual bool imageHasRelativeWidth() const { return m_styleImage->imageHasRelativeWidth(); }
    virtual bool imageHasRelativeHeight() const { return m_styleImage->imageHasRelativeHeight(); }
    virtual IntSize imageSize(float multiplier) const { return m_styleImage->imageSize(this, multiplier); }
    
    // |m_styleImage| can be 0 if we get a callback for a background image from RenderObject::setStyle.
    virtual WrappedImagePtr imagePtr() const { return m_styleImage ? m_styleImage->data() : 0; }

private:
    RefPtr<StyleImage> m_styleImage;
};

} // namespace WebCore

#endif // RenderImageGeneratedContent_h
