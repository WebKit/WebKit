/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "dom_kdomnodetreewrapper.h"
#include "dom_elementimpl.h"

#include "render_object.h"
#include "render_kcanvaswrapper.h"

#include "kdom/core/DocumentImpl.h"
#include "ksvg2/svg/SVGDocumentImpl.h"
#include "ksvg2/KSVGView.h"
#include "kcanvas/device/quartz/KCanvasViewQuartz.h"

using namespace khtml;
using namespace DOM;
using namespace KSVG;

DOM::KDOMNodeTreeWrapperImpl::KDOMNodeTreeWrapperImpl(DOM::DocumentPtr *doc, KDOM::DocumentImpl *wrapped) :
    DOM::ElementImpl(QualifiedName("svg", "svg", "http://www.w3.org/2000/svg"), doc), m_wrappedDoc(wrapped)
{
    if (m_wrappedDoc)
        m_wrappedDoc->ref();
}

KDOMNodeTreeWrapperImpl::~KDOMNodeTreeWrapperImpl()
{
    if (m_wrappedDoc)
        m_wrappedDoc->deref();
}

void KDOMNodeTreeWrapperImpl::attach()
{
    ElementImpl::attach();
    
    if (renderer()) {
        RenderKCanvasWrapper *canvasWrapper = static_cast<RenderKCanvasWrapper*>(renderer());
        SVGDocumentImpl *svgDoc = static_cast<KSVG::SVGDocumentImpl *>(m_wrappedDoc);
        canvasWrapper->setCanvas(svgDoc->canvas());
        KCanvasViewQuartz *canvasView = static_cast<KCanvasViewQuartz *>(svgDoc->svgView()->canvasView());
        canvasView->setRenderObject(canvasWrapper);
    }
}

void KDOMNodeTreeWrapperImpl::detach()
{
    SVGDocumentImpl *svgDoc = static_cast<KSVG::SVGDocumentImpl *>(m_wrappedDoc);
    KCanvasViewQuartz *canvasView = static_cast<KCanvasViewQuartz *>(svgDoc->svgView()->canvasView());
    canvasView->setRenderObject(NULL);
    
    ElementImpl::detach();
}

RenderObject *KDOMNodeTreeWrapperImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderKCanvasWrapper(this);
}
