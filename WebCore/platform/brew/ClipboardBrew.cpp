/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2010 Company 100, Inc.
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
#include "ClipboardBrew.h"

#include "CachedImage.h"
#include "Element.h"
#include "FileList.h"
#include "Frame.h"
#include "NotImplemented.h"
#include "Range.h"

namespace WebCore {

PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy, DragData*, Frame*)
{
    return 0;
}

ClipboardBrew::ClipboardBrew(ClipboardAccessPolicy policy, ClipboardType clipboardType)
    : Clipboard(clipboardType, isForDragging)
{
}

ClipboardBrew::~ClipboardBrew()
{
}

void ClipboardBrew::clearData(const String&)
{
    ASSERT(isForDragAndDrop());
}

void ClipboardBrew::clearAllData()
{
    ASSERT(isForDragAndDrop());
}

String ClipboardBrew::getData(const String&, bool& success) const
{
    success = false;
    return "";
}

bool ClipboardBrew::setData(const String&, const String&)
{
    ASSERT(isForDragAndDrop());
    return false;
}

// extensions beyond IE's API
HashSet<String> ClipboardBrew::types() const
{
    return HashSet<String>();
}

PassRefPtr<FileList> ClipboardBrew::files() const
{
    notImplemented();
    return 0;
}

void ClipboardBrew::setDragImage(CachedImage*, const IntPoint&)
{
}

void ClipboardBrew::setDragImageElement(Node*, const IntPoint&)
{
}

DragImageRef ClipboardBrew::createDragImage(IntPoint&) const
{
    return 0;
}

void ClipboardBrew::declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*)
{
}

void ClipboardBrew::writeURL(const KURL&, const String&, Frame*)
{
}

void ClipboardBrew::writeRange(Range* selectedRange, Frame*)
{
    ASSERT(selectedRange);
}

void ClipboardBrew::writePlainText(const String&)
{
}

bool ClipboardBrew::hasData()
{
    return false;
}

} // namespace WebCore
