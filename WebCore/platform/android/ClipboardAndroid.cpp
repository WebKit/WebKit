/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "ClipboardAndroid.h"

#include "CachedImage.h"
#include "Element.h"
#include "FileList.h"
#include "Frame.h"
#include "Range.h"

namespace WebCore {

PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy, DragData*, Frame*)
{
    return 0;
}

ClipboardAndroid::ClipboardAndroid(ClipboardAccessPolicy policy, ClipboardType clipboardType)
    : Clipboard(policy, clipboardType)
{
}

ClipboardAndroid::~ClipboardAndroid()
{
}

void ClipboardAndroid::clearData(const String&)
{
    ASSERT(isForDragAndDrop());
}

void ClipboardAndroid::clearAllData()
{
    ASSERT(isForDragAndDrop());
}

String ClipboardAndroid::getData(const String&, bool& success) const
{     
    success = false;
    return "";
}

bool ClipboardAndroid::setData(const String&, const String&)
{
    ASSERT(isForDragAndDrop());
    return false;
}

// extensions beyond IE's API
HashSet<String> ClipboardAndroid::types() const
{ 
    return HashSet<String>();
}

PassRefPtr<FileList> ClipboardAndroid::files() const
{
    return 0;
}

void ClipboardAndroid::setDragImage(CachedImage*, const IntPoint&)
{
}

void ClipboardAndroid::setDragImageElement(Node*, const IntPoint&)
{
}

DragImageRef ClipboardAndroid::createDragImage(IntPoint&) const
{
    return 0;
}

void ClipboardAndroid::declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*)
{
}

void ClipboardAndroid::writeURL(const KURL&, const String&, Frame*)
{
}

void ClipboardAndroid::writeRange(Range* selectedRange, Frame*)
{
    ASSERT(selectedRange);
}

void ClipboardAndroid::writePlainText(const String&)
{
}

bool ClipboardAndroid::hasData()
{
    return false;
}

} // namespace WebCore
