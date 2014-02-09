/*
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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
#include "FrameWin.h"

#include "Document.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "PrintContext.h"
#include "RenderObject.h"

namespace WebCore {

void computePageRectsForFrame(Frame* frame, const IntRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, Vector<IntRect>& outPages, int& outPageHeight)
{
    PrintContext printContext(frame);
    float pageHeight = 0;
    printContext.computePageRects(printRect, headerHeight, footerHeight, userScaleFactor, pageHeight);
    outPageHeight = static_cast<int>(pageHeight);
    outPages = printContext.pageRects();
}

GDIObject<HBITMAP> imageFromSelection(Frame* frame, bool forceBlackText)
{
    frame->document()->updateLayout();

    frame->view()->setPaintBehavior(PaintBehaviorSelectionOnly | (forceBlackText ? PaintBehaviorForceBlackText : 0));
    FloatRect fr = frame->selection().selectionBounds();
    IntRect ir(static_cast<int>(fr.x()), static_cast<int>(fr.y()), static_cast<int>(fr.width()), static_cast<int>(fr.height()));
    GDIObject<HBITMAP> image = imageFromRect(frame, ir);
    frame->view()->setPaintBehavior(PaintBehaviorNormal);
    return image;
}

} // namespace WebCore
