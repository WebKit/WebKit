/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
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
#include "Pasteboard.h"

#include "DocumentFragment.h"
#include "Editor.h"
#include "Frame.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "markup.h"
#include <support/Locker.h>
#include <Clipboard.h>
#include <Message.h>
#include <String.h>


namespace WebCore {

Pasteboard::Pasteboard()
{
}

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard();
    return pasteboard;
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    BClipboard clipboard("WebKit");
    if (!clipboard.Lock())
        return;

    clipboard.Clear();
    BMessage* data = clipboard.Data();
    if (!data)
        return;

    data->AddString("text/plain", BString(frame->selectedText()));
    clipboard.Commit();

    clipboard.Unlock();
}

void Pasteboard::writePlainText(const String& text)
{
    BClipboard clipboard("WebKit");
    if (!clipboard.Lock())
        return;

    clipboard.Clear();
    BMessage* data = clipboard.Data();
    if (!data)
        return;

    data->AddString("text/plain", BString(text));
    clipboard.Commit();

    clipboard.Unlock();
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}

String Pasteboard::plainText(Frame* frame)
{
    BClipboard clipboard("WebKit");
    if (!clipboard.Lock())
        return String();

    BMessage* data = clipboard.Data();
    if (!data)
        return String();

    BString string;
    data->FindString("text/plain", &string);

    clipboard.Unlock();

    return string;
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context,
                                                          bool allowPlainText, bool& chosePlainText)
{
    notImplemented();
    return 0;
}

void Pasteboard::writeURL(const KURL& url, const String&, Frame*)
{
    BClipboard clipboard("WebKit");
    if (!clipboard.Lock())
        return;

    clipboard.Clear();

    BMessage* data = clipboard.Data();
    if (!data)
        return;

    data->AddString("text/plain", url.string());
    clipboard.Commit();

    clipboard.Unlock();
}

void Pasteboard::writeImage(Node*, const KURL&, const String&)
{
    notImplemented();
}

void Pasteboard::clear()
{
    BClipboard clipboard("WebKit");
    if (!clipboard.Lock())
        return;

    clipboard.Clear();
    clipboard.Commit();

    clipboard.Unlock();
}

} // namespace WebCore

