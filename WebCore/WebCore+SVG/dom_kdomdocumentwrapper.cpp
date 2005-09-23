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
 
#include "dom_kdomdocumentwrapper.h"
#include "xml_kdomtokenizer.h"

#include "KWQKSVGView.h"
#include "KWQKSVGPart.h"

#include "kcanvas/KCanvas.h"
#include "kcanvas/device/quartz/KRenderingDeviceQuartz.h"

using namespace khtml;
using namespace DOM;

KRenderingDeviceQuartz *KDOMDocumentWrapperImpl::renderingDevice()
{
    static KRenderingDeviceQuartz *__quartzRenderingDevice = nil;
    if (!__quartzRenderingDevice)
        __quartzRenderingDevice = new KRenderingDeviceQuartz();
    return __quartzRenderingDevice;
}

KDOMDocumentWrapperImpl::KDOMDocumentWrapperImpl(DOMImplementationImpl *i, KHTMLView *v) : DocumentImpl(i,v)
{
    m_svgPart = new KSVG::KSVGPart();
    m_canvas = new KCanvas(KDOMDocumentWrapperImpl::renderingDevice());
    svgView()->canvasView()->init(m_canvas, NULL);
    m_canvas->addView(svgView()->canvasView());
}

KDOMDocumentWrapperImpl::~KDOMDocumentWrapperImpl()
{
    delete m_svgPart;
    delete m_canvas;
}

Tokenizer *DOM::KDOMDocumentWrapperImpl::createTokenizer()
{
    return newKDOMTokenizer(docPtr(), m_view);
}

KSVG::KSVGView *KDOMDocumentWrapperImpl::svgView()
{
    return m_svgPart ? static_cast<KSVG::KSVGView *>(m_svgPart->view()) : NULL;
}
