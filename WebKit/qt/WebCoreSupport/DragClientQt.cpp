/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "DragClientQt.h"

#include "ClipboardQt.h"
#include "qwebpage.h"

#include <QDrag>
#include <QMimeData>


namespace WebCore {

DragDestinationAction DragClientQt::actionMaskForDrag(DragData*)
{
    return DragDestinationActionAny;
}

void DragClientQt::willPerformDragDestinationAction(DragDestinationAction, DragData*)
{
}

void DragClientQt::dragControllerDestroyed()
{
    delete this;
}

DragSourceAction DragClientQt::dragSourceActionMaskForPoint(const IntPoint&)
{
    return DragSourceActionAny;
}

void DragClientQt::willPerformDragSourceAction(DragSourceAction, const IntPoint&, Clipboard*)
{
}

void DragClientQt::startDrag(DragImageRef, const IntPoint&, const IntPoint&, Clipboard* clipboard, Frame*, bool)
{
#ifndef QT_NO_DRAGANDDROP
    QMimeData* clipboardData = static_cast<ClipboardQt*>(clipboard)->clipboardData();
    static_cast<ClipboardQt*>(clipboard)->invalidateWritableData();
    QWidget* view = m_webPage->view();
    if (view) {
        QDrag *drag = new QDrag(view);
        if (clipboardData->hasImage())
            drag->setPixmap(qvariant_cast<QPixmap>(clipboardData->imageData()));
        drag->setMimeData(clipboardData);
        drag->start();
    }
#endif
}


DragImageRef DragClientQt::createDragImageForLink(KURL&, const String&, Frame*)
{
    return 0;
}

} // namespace WebCore
