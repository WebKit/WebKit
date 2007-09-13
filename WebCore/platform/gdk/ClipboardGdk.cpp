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
#include "ClipboardGdk.h"

#include "NotImplemented.h"
#include "StringHash.h"

namespace WebCore {
ClipboardGdk::ClipboardGdk(ClipboardAccessPolicy policy, bool forDragging)
    : Clipboard(policy, forDragging)
{
    notImplemented();
}

ClipboardGdk::~ClipboardGdk()
{
    notImplemented();
}

void ClipboardGdk::clearData(const String&)
{
    notImplemented();
}

void ClipboardGdk::clearAllData()
{
    notImplemented();
}

String ClipboardGdk::getData(const String&, bool &success) const
{
    notImplemented();
    success = false;
    return String();
}

bool ClipboardGdk::setData(const String&, const String&)
{
    notImplemented();
    return false;
}

HashSet<String> ClipboardGdk::types() const
{
    notImplemented();
    return HashSet<String>();
}

IntPoint ClipboardGdk::dragLocation() const
{
    notImplemented();
    return IntPoint(0, 0);
}

CachedImage* ClipboardGdk::dragImage() const
{
    notImplemented();
    return 0;
}

void ClipboardGdk::setDragImage(CachedImage*, const IntPoint&)
{
    notImplemented();
}

Node* ClipboardGdk::dragImageElement()
{
    notImplemented();
    return 0;
}

void ClipboardGdk::setDragImageElement(Node*, const IntPoint&)
{
    notImplemented();
}

DragImageRef ClipboardGdk::createDragImage(IntPoint&) const
{
    notImplemented();
    return 0;
}

void ClipboardGdk::declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardGdk::writeURL(const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardGdk::writeRange(Range*, Frame*)
{
    notImplemented();
}

bool ClipboardGdk::hasData()
{
    notImplemented();
    return false;
}

}
