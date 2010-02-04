/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
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
#include "ClipboardHaiku.h"

#include "FileList.h"
#include "IntPoint.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "StringHash.h"

#include <support/Locker.h>
#include <app/Clipboard.h>
#include <Message.h>
#include <String.h>
#include <wtf/HashTable.h>


namespace WebCore {

ClipboardHaiku::ClipboardHaiku(ClipboardAccessPolicy policy, bool forDragging)
    : Clipboard(policy, forDragging)
{
}

void ClipboardHaiku::clearData(const String& type)
{
    if (be_clipboard->Lock()) {
        BMessage* data = be_clipboard->Data();

        if (data) {
            data->RemoveName(BString(type).String());
            be_clipboard->Commit();
        }

        be_clipboard->Unlock();
    }
}

void ClipboardHaiku::clearAllData()
{
    if (be_clipboard->Lock()) {
        be_clipboard->Clear();
        be_clipboard->Commit();
        be_clipboard->Unlock();
    }
}

String ClipboardHaiku::getData(const String& type, bool& success) const
{
    BString result;
    success = false;

    if (be_clipboard->Lock()) {
        BMessage* data = be_clipboard->Data();

        if (data)
            if (data->FindString(BString(type).String(), &result) == B_OK)
                success = true;

        be_clipboard->Unlock();
    }

    return result;
}

bool ClipboardHaiku::setData(const String& type, const String& data)
{
    bool result = false;

    if (be_clipboard->Lock()) {
        BMessage* bdata = be_clipboard->Data();

        if (bdata) {
            bdata->RemoveName(BString(type).String());

            if (bdata->AddString(BString(type).String(), BString(data)) == B_OK)
                result = true;
        }

        be_clipboard->Commit();
        be_clipboard->Unlock();
    }

    return result;
}

// Extensions beyond IE's API.
HashSet<String> ClipboardHaiku::types() const
{
    HashSet<String> result;

    if (be_clipboard->Lock()) {
        BMessage* data = be_clipboard->Data();

        if (data) {
            char* name;
            uint32 type;
            int32 count;

            for (int32 i = 0; data->GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++)
                result.add(name);
        }

        be_clipboard->Unlock();
    }

    return result;
}

PassRefPtr<FileList> ClipboardHaiku::files() const
{
    notImplemented();
    return 0;
}

IntPoint ClipboardHaiku::dragLocation() const
{
    notImplemented();
    return IntPoint(0, 0);
}

CachedImage* ClipboardHaiku::dragImage() const
{
    notImplemented();
    return 0;
}

void ClipboardHaiku::setDragImage(CachedImage*, const IntPoint&)
{
    notImplemented();
}

Node* ClipboardHaiku::dragImageElement()
{
    notImplemented();
    return 0;
}

void ClipboardHaiku::setDragImageElement(Node*, const IntPoint&)
{
    notImplemented();
}

DragImageRef ClipboardHaiku::createDragImage(IntPoint& dragLocation) const
{
    notImplemented();
    return 0;
}

void ClipboardHaiku::declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardHaiku::writeURL(const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardHaiku::writeRange(Range*, Frame*)
{
    notImplemented();
}

void ClipboardHaiku::writePlainText(const String&)
{
    notImplemented();
}

bool ClipboardHaiku::hasData()
{
    bool result = false;

    if (be_clipboard->Lock()) {
        BMessage* data = be_clipboard->Data();

        if (data)
            result = !data->IsEmpty();

        be_clipboard->Unlock();
    }

    return result;
}

} // namespace WebCore

