/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007, Holger Hans Peter Freyther
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
#include "ClipboardGtk.h"

#include "NotImplemented.h"
#include "StringHash.h"

namespace WebCore {
ClipboardGtk::ClipboardGtk(ClipboardAccessPolicy policy, bool forDragging)
    : Clipboard(policy, forDragging)
{
    notImplemented();
}

ClipboardGtk::~ClipboardGtk()
{
    notImplemented();
}

void ClipboardGtk::clearData(const String&)
{
    notImplemented();
}

void ClipboardGtk::clearAllData()
{
    notImplemented();
}

String ClipboardGtk::getData(const String&, bool &success) const
{
    notImplemented();
    success = false;
    return String();
}

bool ClipboardGtk::setData(const String&, const String&)
{
    notImplemented();
    return false;
}

HashSet<String> ClipboardGtk::types() const
{
    notImplemented();
    return HashSet<String>();
}

IntPoint ClipboardGtk::dragLocation() const
{
    notImplemented();
    return IntPoint(0, 0);
}

CachedImage* ClipboardGtk::dragImage() const
{
    notImplemented();
    return 0;
}

void ClipboardGtk::setDragImage(CachedImage*, const IntPoint&)
{
    notImplemented();
}

Node* ClipboardGtk::dragImageElement()
{
    notImplemented();
    return 0;
}

void ClipboardGtk::setDragImageElement(Node*, const IntPoint&)
{
    notImplemented();
}

DragImageRef ClipboardGtk::createDragImage(IntPoint&) const
{
    notImplemented();
    return 0;
}

void ClipboardGtk::declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardGtk::writeURL(const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardGtk::writeRange(Range*, Frame*)
{
    notImplemented();
}

bool ClipboardGtk::hasData()
{
    notImplemented();
    return false;
}

}
