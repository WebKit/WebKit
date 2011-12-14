/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
#include "TextResourceDecoder.h"
#include "markup.h"
#include <support/Locker.h>
#include <Clipboard.h>
#include <Message.h>
#include <String.h>
#include <wtf/text/CString.h>


namespace WebCore {

Pasteboard::Pasteboard()
{
}

Pasteboard::~Pasteboard()
{
}

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard pasteboard;
    return &pasteboard;
}

// BClipboard unfortunately does not derive from BLocker, so we cannot use BAutolock.
class AutoClipboardLocker {
public:
    AutoClipboardLocker(BClipboard* clipboard)
        : m_clipboard(clipboard)
        , m_isLocked(clipboard && clipboard->Lock())
    {
    }

    ~AutoClipboardLocker()
    {
        if (m_isLocked)
            m_clipboard->Unlock();
    }

    bool isLocked() const
    {
        return m_isLocked;
    }

private:
    BClipboard* m_clipboard;
    bool m_isLocked;
};

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return;

    be_clipboard->Clear();
    BMessage* data = be_clipboard->Data();
    if (!data)
        return;

    BString string(frame->selectedText());

    // Replace unwanted representation of blank lines
    const char* utf8BlankLine = "\302\240\n";
    string.ReplaceAll(utf8BlankLine, "\n");

    data->AddData("text/plain", B_MIME_TYPE, string.String(), string.Length());

    BString markupString(createMarkup(selectedRange, 0, AnnotateForInterchange, false, AbsoluteURLs));
    data->AddData("text/html", B_MIME_TYPE, markupString.String(), markupString.Length());

    be_clipboard->Commit();
}

void Pasteboard::writePlainText(const String& text)
{
    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return;

    be_clipboard->Clear();
    BMessage* data = be_clipboard->Data();
    if (!data)
        return;

    BString string(text);
    data->AddData("text/plain", B_MIME_TYPE, string.String(), string.Length());
    be_clipboard->Commit();
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}

String Pasteboard::plainText(Frame* frame)
{
    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return String();

    BMessage* data = be_clipboard->Data();
    if (!data)
        return String();

    const char* buffer = 0;
    ssize_t bufferLength;
    BString string;
    if (data->FindData("text/plain", B_MIME_TYPE, reinterpret_cast<const void**>(&buffer), &bufferLength) == B_OK)
        string.Append(buffer, bufferLength);

    return string;
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context,
                                                          bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return 0;

    BMessage* data = be_clipboard->Data();
    if (!data)
        return 0;

    const char* buffer = 0;
    ssize_t bufferLength;
    if (data->FindData("text/html", B_MIME_TYPE, reinterpret_cast<const void**>(&buffer), &bufferLength) == B_OK) {
        RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("text/plain", "UTF-8", true);
        String html = decoder->decode(buffer, bufferLength);
        html += decoder->flush();

        if (!html.isEmpty()) {
            RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(frame->document(), html, "", FragmentScriptingNotAllowed);
            if (fragment)
                return fragment.release();
        }
    }

    if (!allowPlainText)
        return 0;

    if (data->FindData("text/plain", B_MIME_TYPE, reinterpret_cast<const void**>(&buffer), &bufferLength) == B_OK) {
        BString plainText(buffer, bufferLength);

        chosePlainText = true;
        RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), plainText);
        if (fragment)
            return fragment.release();
    }

    return 0;
}

void Pasteboard::writeURL(const KURL& url, const String&, Frame*)
{
    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return;

    be_clipboard->Clear();

    BMessage* data = be_clipboard->Data();
    if (!data)
        return;

    BString string(url.string());
    data->AddData("text/plain", B_MIME_TYPE, string.String(), string.Length());
    be_clipboard->Commit();
}

void Pasteboard::writeImage(Node*, const KURL&, const String&)
{
    notImplemented();
}

void Pasteboard::clear()
{
    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return;

    be_clipboard->Clear();
    be_clipboard->Commit();
}

} // namespace WebCore

