/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef HTML_CANVASIMPL_H
#define HTML_CANVASIMPL_H

#include "html_imageimpl.h"

#if __APPLE__
typedef struct CGImage* CGImageRef;
#endif

namespace WebCore {

class CanvasRenderingContext2D;
typedef CanvasRenderingContext2D CanvasRenderingContext;

// FIXME: Should inherit from HTMLElement instead of HTMLImageElement.
class HTMLCanvasElement : public HTMLImageElement {
public:
    HTMLCanvasElement(Document*);
    ~HTMLCanvasElement();

    CanvasRenderingContext* getContext(const String&);

    // FIXME: Web Applications 1.0 describes a toDataURL function.

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void attach();
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void detach();
    
    virtual bool isURLAttribute(Attribute*) const;

    IntSize size() const;

#if __APPLE__
    CGImageRef createPlatformImage() const;
#endif

private:
    RefPtr<CanvasRenderingContext2D> m_2DContext;
    // FIXME: Web Applications 1.0 describes a security feature where we track if we ever drew
    // any images outside the domain.
};

} //namespace

#endif
