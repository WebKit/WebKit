/*
 *  Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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
#include "ClipboardWx.h"

#include "Editor.h"
#include "FileList.h"
#include "Frame.h"
#include "HashTable.h"
#include "IntPoint.h"
#include "NotImplemented.h"
#include "Pasteboard.h"
#include "PlatformString.h"
#include <wtf/text/StringHash.h>


namespace WebCore {
    
PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy, DragData*, Frame*)
{
    return 0;
}

ClipboardWx::ClipboardWx(ClipboardAccessPolicy policy, ClipboardType clipboardType) 
    : Clipboard(policy, clipboardType)
{
}

void ClipboardWx::clearData(const String& type)
{
    notImplemented();
}

void ClipboardWx::clearAllData() 
{
    Pasteboard::generalPasteboard()->clear();
}

String ClipboardWx::getData(const String& type) const 
{
    notImplemented();
    return ""; 
}

bool ClipboardWx::setData(const String& type, const String& data) 
{
    notImplemented();
    return false;
}

// extensions beyond IE's API
HashSet<String> ClipboardWx::types() const 
{
    notImplemented();
    HashSet<String> result;
    return result;
}

PassRefPtr<FileList> ClipboardWx::files() const
{
    notImplemented();
    return 0;
}

IntPoint ClipboardWx::dragLocation() const 
{ 
    notImplemented();
    return IntPoint(0,0);
}

CachedImage* ClipboardWx::dragImage() const 
{
    notImplemented();
    return 0; 
}

void ClipboardWx::setDragImage(CachedImage*, const IntPoint&) 
{
    notImplemented();
}

Node* ClipboardWx::dragImageElement() 
{
    notImplemented();
    return 0; 
}

void ClipboardWx::setDragImageElement(Node*, const IntPoint&)
{
    notImplemented();
}

DragImageRef ClipboardWx::createDragImage(IntPoint& dragLoc) const
{ 
    notImplemented();
    return 0;
}

void ClipboardWx::declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*) 
{
    notImplemented();
}

void ClipboardWx::writeURL(const KURL& url, const String& string, Frame* frame) 
{
    Pasteboard::generalPasteboard()->writeURL(url, string, frame);
}

void ClipboardWx::writeRange(Range* range, Frame* frame) 
{
    Pasteboard::generalPasteboard()->writeSelection(range, frame->editor()->smartInsertDeleteEnabled() && frame->selection()->granularity() == WordGranularity, frame);
}

bool ClipboardWx::hasData() 
{
    notImplemented();
    return false;
}

void ClipboardWx::writePlainText(const WTF::String& text)
{
    Pasteboard::generalPasteboard()->writePlainText(text, Pasteboard::CannotSmartReplace);
}

}
