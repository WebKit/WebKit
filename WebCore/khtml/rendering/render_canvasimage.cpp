/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

//#define DEBUG_LAYOUT

#include "render_canvasimage.h"
#include "render_canvas.h"

#include <qdrawutil.h>
#include <qpainter.h>

#include <kapplication.h>
#include <kdebug.h>

#include "css/csshelper.h"
#include "misc/helper.h"
#include "misc/htmlattrs.h"
#include "misc/htmltags.h"
#include "html/html_formimpl.h"
#include "html/html_imageimpl.h"
#include "html/dtd.h"
#include "xml/dom2_eventsimpl.h"
#include "html/html_documentimpl.h"
#include <math.h>

using namespace DOM;
using namespace khtml;

// -------------------------------------------------------------------------

RenderCanvasImage::RenderCanvasImage(NodeImpl *_node)
    : RenderImage(_node)
{
}

RenderCanvasImage::~RenderCanvasImage()
{
}


void RenderCanvasImage::paint(PaintInfo& i, int _tx, int _ty)
{
    printf ("%s:%d: %s\n", __FILE__, __LINE__, __FUNCTION__);
}

void RenderCanvasImage::layout()
{
    printf ("%s:%d: %s\n", __FILE__, __LINE__, __FUNCTION__);

    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    QRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = getAbsoluteRepaintRect();
    
    calcWidth();
    calcHeight();

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
    
    setNeedsLayout(false);
}
