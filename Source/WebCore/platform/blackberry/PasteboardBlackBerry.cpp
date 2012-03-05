/*
 * Copyright (C) 2009, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "Pasteboard.h"

#include "DocumentFragment.h"
#include "Frame.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "Range.h"
#include "markup.h"

#include <BlackBerryPlatformClipboard.h>

namespace WebCore {

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard();
    return pasteboard;
}

Pasteboard::Pasteboard()
{
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}

void Pasteboard::clear()
{
    BlackBerry::Platform::Clipboard::clearClipboard();
}

void Pasteboard::writeImage(Node*, KURL const&, String const&)
{
    notImplemented();
}

void Pasteboard::writeClipboard(Clipboard*)
{
    notImplemented();
}

void Pasteboard::writeSelection(Range* selectedRange, bool, Frame* frame)
{
    std::string text = frame->editor()->selectedText().utf8().data();
    std::string html = createMarkup(selectedRange, 0, AnnotateForInterchange).utf8().data();
    std::string url = frame->document()->url().string().utf8().data();

    BlackBerry::Platform::Clipboard::write(text, html, url);
}

void Pasteboard::writeURL(KURL const& url, String const&, Frame*)
{
    ASSERT(!url.isEmpty());
    BlackBerry::Platform::Clipboard::writeURL(url.string().utf8().data());
}

void Pasteboard::writePlainText(const String& text)
{
    BlackBerry::Platform::Clipboard::writePlainText(text.utf8().data());
}

String Pasteboard::plainText(Frame*)
{
    return String::fromUTF8(BlackBerry::Platform::Clipboard::readPlainText().c_str());
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    // Note: We are able to check if the format exists prior to reading but the check & the early return
    // path of get_clipboard_data are the exact same, so just use get_clipboard_data and validate the
    // return value to determine if the data was present.
    String html = String::fromUTF8(BlackBerry::Platform::Clipboard::readHTML().c_str());
    RefPtr<DocumentFragment> fragment;
    if (!html.isEmpty()) {
        String url = String::fromUTF8(BlackBerry::Platform::Clipboard::readURL().c_str());
        if (fragment = createFragmentFromMarkup(frame->document(), html, url, FragmentScriptingNotAllowed))
            return fragment.release();
    }

    if (!allowPlainText)
        return 0;

    String text = String::fromUTF8(BlackBerry::Platform::Clipboard::readPlainText().c_str());
    if (fragment = createFragmentFromText(context.get(), text)) {
        chosePlainText = true;
        return fragment.release();
    }
    return 0;
}

} // namespace WebCore
