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
#include "DragData.h"

#include "Document.h"
#include "DocumentFragment.h"
#include "ClipboardQt.h"

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore {

bool DragData::canSmartReplace() const
{
    notImplemented();
    return false;
}
    
bool DragData::containsColor() const
{
    notImplemented();
    return false;
}

bool DragData::containsPlainText() const
{
    notImplemented();
    return false;
}

String DragData::asPlainText() const
{
    notImplemented();
    return String();
}
    
Color DragData::asColor() const
{
    notImplemented();
    return Color();
}

Clipboard* DragData::createClipboard(ClipboardAccessPolicy policy) const
{
    notImplemented();
    return new ClipboardQt(policy, true);
}
    
bool DragData::containsCompatibleContent() const
{
    notImplemented();
    return false;
}
    
bool DragData::containsURL() const
{
    notImplemented();
    return false;
}
    
String DragData::asURL(String* title) const
{
    notImplemented();
    return String();
}
    
    
PassRefPtr<DocumentFragment> DragData::asFragment(Document*) const
{
    notImplemented();
    return 0;
}
    
}

