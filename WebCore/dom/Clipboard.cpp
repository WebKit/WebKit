/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "Clipboard.h"

#include "CachedImage.h"
#include "DOMImplementation.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Image.h"

namespace WebCore {

Clipboard::Clipboard(ClipboardAccessPolicy policy, bool isForDragging) 
    : m_policy(policy) 
    , m_dragStarted(false)
    , m_forDragging(isForDragging)
    , m_dragImage(0)
{
}
    
void Clipboard::setAccessPolicy(ClipboardAccessPolicy policy)
{
    // once you go numb, can never go back
    ASSERT(m_policy != ClipboardNumb || policy == ClipboardNumb);
    m_policy = policy;
}

// These "conversion" methods are called by both WebCore and WebKit, and never make sense to JS, so we don't
// worry about security for these. They don't allow access to the pasteboard anyway.

static DragOperation dragOpFromIEOp(const String& op)
{
    // yep, it's really just this fixed set
    if (op == "none")
        return DragOperationNone;
    if (op == "copy")
        return DragOperationCopy;
    if (op == "link")
        return DragOperationLink;
    if (op == "move")
        return DragOperationGeneric;
    if (op == "copyLink")
        return (DragOperation)(DragOperationCopy | DragOperationLink);
    if (op == "copyMove")
        return (DragOperation)(DragOperationCopy | DragOperationGeneric | DragOperationMove);
    if (op == "linkMove")
        return (DragOperation)(DragOperationLink | DragOperationGeneric | DragOperationMove);
    if (op == "all")
        return DragOperationEvery;
    return DragOperationPrivate;  // really a marker for "no conversion"
}

static String IEOpFromDragOp(DragOperation op)
{
    bool moveSet = !!((DragOperationGeneric | DragOperationMove) & op);
    
    if ((moveSet && (op & DragOperationCopy) && (op & DragOperationLink))
        || (op == DragOperationEvery))
        return "all";
    if (moveSet && (op & DragOperationCopy))
        return "copyMove";
    if (moveSet && (op & DragOperationLink))
        return "linkMove";
    if ((op & DragOperationCopy) && (op & DragOperationLink))
        return "copyLink";
    if (moveSet)
        return "move";
    if (op & DragOperationCopy)
        return "copy";
    if (op & DragOperationLink)
        return "link";
    return "none";
}

bool Clipboard::sourceOperation(DragOperation& op) const
{
    if (m_effectAllowed.isNull())
        return false;
    op = dragOpFromIEOp(m_effectAllowed);
    return true;
}

bool Clipboard::destinationOperation(DragOperation& op) const
{
    if (m_dropEffect.isNull())
        return false;
    op = dragOpFromIEOp(m_dropEffect);
    return true;
}

void Clipboard::setSourceOperation(DragOperation op)
{
    m_effectAllowed = IEOpFromDragOp(op);
}

void Clipboard::setDestinationOperation(DragOperation op)
{
    m_dropEffect = IEOpFromDragOp(op);
}

void Clipboard::setDropEffect(const String &effect)
{
    if (!m_forDragging)
        return;

    if (m_policy == ClipboardReadable || m_policy == ClipboardTypesReadable)
        m_dropEffect = effect;
}

void Clipboard::setEffectAllowed(const String &effect)
{
    if (!m_forDragging)
        return;

    if (m_policy == ClipboardWritable)
        m_effectAllowed = effect;
}

} // namespace WebCore
