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
#include "wtf/URL.h"
#include "NotImplemented.h"
#include "TextResourceDecoder.h"
#include "markup.h"
#include <support/Locker.h>
#include <app/Clipboard.h>
#include <Message.h>
#include <String.h>
#include <wtf/text/CString.h>


namespace WebCore {

    std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste()
{
    return std::make_unique<Pasteboard>();
}

#if ENABLE(DRAG_SUPPORT)
std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop()
{
    return createForCopyAndPaste();
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(const DragData& dragData)
{
    return createForCopyAndPaste();
}
#endif

Pasteboard::Pasteboard()
{
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

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL&)
{
    notImplemented();
}

void Pasteboard::writeString(const String& type, const String& data)
{
    if (be_clipboard->Lock()) {
        bool result = false;
        BMessage* bdata = be_clipboard->Data();

        if (bdata) {
            CString typeUTF8 = type.utf8();
            bdata->RemoveName(typeUTF8.data());

            CString dataUTF8 = data.utf8();
            if (bdata->AddData(typeUTF8.data(), B_MIME_TYPE,
                    dataUTF8.data(), dataUTF8.length()) == B_OK)
                result = true;
        }

        if (result)
            be_clipboard->Commit();
        else
            be_clipboard->Revert();
        be_clipboard->Unlock();
    }
}

void Pasteboard::writeSelection(const SimpleRange& selectedRange, bool canSmartCopyOrDelete, Frame& frame, ShouldSerializeSelectedTextForDataTransfer)
{
    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return;

    be_clipboard->Clear();
    BMessage* data = be_clipboard->Data();
    if (!data)
        return;

    BString string(frame.editor().selectedText());

    // Replace unwanted representation of blank lines
    const char* utf8BlankLine = "\302\240\n";
    string.ReplaceAll(utf8BlankLine, "\n");

    data->AddData("text/plain", B_MIME_TYPE, string.String(), string.Length());

    BString markupString(serializePreservingVisualAppearance(selectedRange, nullptr, AnnotateForInterchange::Yes));
    data->AddData("text/html", B_MIME_TYPE, markupString.String(), markupString.Length());

    be_clipboard->Commit();
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption smartReplaceOption)
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


void WebCore::Pasteboard::write(WebCore::PasteboardImage const&)
{
    notImplemented();
}

void WebCore::Pasteboard::write(WebCore::PasteboardWebContent const& content)
{
    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return;

    be_clipboard->Clear();
    BMessage* data = be_clipboard->Data();
    if (!data)
        return;

    data->AddData("text/html", B_MIME_TYPE, content.markup.utf8().data(), content.markup.utf8().length());

    data->AddData("text/plain", B_MIME_TYPE, content.text.utf8().data(), content.text.utf8().length());

    be_clipboard->Commit();
}

void WebCore::Pasteboard::writeMarkup(WTF::String const& text)
{
    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return;

    be_clipboard->Clear();
    BMessage* data = be_clipboard->Data();
    if (!data)
        return;

    BString string(text);
    data->AddData("text/html", B_MIME_TYPE, string.String(), string.Length());
    be_clipboard->Commit();
}



void Pasteboard::write(const PasteboardURL& url)
{
    ASSERT(!url.url.isEmpty());

    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return;

    be_clipboard->Clear();

    BMessage* data = be_clipboard->Data();
    if (!data)
        return;

    BString string(url.url.string());
    data->AddData("text/plain", B_MIME_TYPE, string.String(), string.Length());
    be_clipboard->Commit();
}

void Pasteboard::write(const Color&)
{
}

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    notImplemented();
    return FileContentState::NoFileOrImageData;
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}


void Pasteboard::read(PasteboardWebContentReader&, WebContentReadingPolicy, WTF::Optional<long unsigned int>)
{
    notImplemented();
}


void Pasteboard::read(PasteboardPlainText& text, WebCore::PlainTextURLReadingPolicy, WTF::Optional<long unsigned int>)
{
    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return;

    BMessage* data = be_clipboard->Data();
    if (!data)
        return;

    const char* buffer = 0;
    ssize_t bufferLength;
    if (data->FindData("text/plain", B_MIME_TYPE, 
            reinterpret_cast<const void**>(&buffer), &bufferLength) == B_OK)
        text.text = String::fromUTF8(buffer, bufferLength);
}

RefPtr<DocumentFragment> Pasteboard::documentFragment(Frame& frame, const SimpleRange& context,
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
        html.append(decoder->flush());

        if (!html.isEmpty()) {
            RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(*frame.document(), html, "", DisallowScriptingContent);
            if (fragment)
                return fragment;
        }
    }

    if (!allowPlainText)
        return 0;

    if (data->FindData("text/plain", B_MIME_TYPE, reinterpret_cast<const void**>(&buffer), &bufferLength) == B_OK) {
        BString plainText(buffer, bufferLength);

        chosePlainText = true;
        RefPtr<DocumentFragment> fragment = createFragmentFromText(context, plainText);
        if (fragment)
            return fragment;
    }

    return 0;
}

bool Pasteboard::hasData()
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

void Pasteboard::clear(const String& type)
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

String Pasteboard::readOrigin()
{
    notImplemented(); // webkit.org/b/177633: [GTK] Move to new Pasteboard API
    return { };
}

String Pasteboard::readString(const String& type)
{
    BString result;

    if (be_clipboard->Lock()) {
        BMessage* data = be_clipboard->Data();

        const char* buffer;
        ssize_t bufferLength;
        if (data) {
            data->FindData(type.utf8().data(), B_MIME_TYPE, 
                reinterpret_cast<const void**>(&buffer), &bufferLength);
        }
        result.SetTo(buffer, bufferLength);

        be_clipboard->Unlock();
    }

    return result;
}

String Pasteboard::readStringInCustomData(const String& type)
{
	return readString(type);
}

void Pasteboard::clear()
{
    AutoClipboardLocker locker(be_clipboard);
    if (!locker.isLocked())
        return;

    be_clipboard->Clear();
    be_clipboard->Commit();
}

#if ENABLE(DRAG_SUPPORT)
void Pasteboard::setDragImage(DragImage, const IntPoint&)
{
    notImplemented();
}
#endif


Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    Vector<String> result;

    if (be_clipboard->Lock()) {
        BMessage* data = be_clipboard->Data();

        if (data) {
            char* name;
            uint32 type;
            int32 count;

            for (int32 i = 0; data->GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++)
                result.append(name);
        }

        be_clipboard->Unlock();
    }

    return result;
}

Vector<String> Pasteboard::typesSafeForBindings(const String&)
{
	return typesForLegacyUnsafeBindings();
}

void Pasteboard::writeCustomData(const WTF::Vector<PasteboardCustomData>&)
{
	notImplemented();
}

void Pasteboard::read(WebCore::PasteboardFileReader&, WTF::Optional<unsigned long>)
{
	notImplemented();
}


} // namespace WebCore

