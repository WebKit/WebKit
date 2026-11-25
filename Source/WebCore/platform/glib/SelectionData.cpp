/*
 * Copyright (C) 2009, Martin Robinson
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "SelectionData.h"

#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GLibExtras.h>
#include <wtf/glib/GMallocString.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SelectionData);

SelectionData::SelectionData(StringImpl* text, StringImpl* markup, const URL& url, StringImpl* uriList, RefPtr<ShareableBitmap>&& image, RefPtr<WebCore::SharedBuffer>&& buffer, bool canSmartReplace)
{
    if (!isEmpty(text))
        setText(text);
    if (!isEmpty(markup))
        setMarkup(markup);
    if (!url.isEmpty())
        setURL(url, nullptr);
    if (!isEmpty(uriList))
        setURIList(uriList);
    if (image)
        setImage(WTF::move(image));
    if (buffer)
        setCustomData(buffer.releaseNonNull());
    setCanSmartReplace(canSmartReplace);
}

static void replaceNonBreakingSpaceWithSpace(RefPtr<StringImpl>& string)
{
    string = makeStringByReplacingAll(string, noBreakSpace, space);
}

void SelectionData::setText(StringImpl* newText)
{
    m_text = newText;
    replaceNonBreakingSpaceWithSpace(m_text);
}

void SelectionData::setURIList(StringImpl* uriListString)
{
    m_uriList = uriListString;

    // This code is originally from: platform/chromium/ChromiumDataObject.cpp.
    // FIXME: We should make this code cross-platform eventually.

    // Line separator is \r\n per RFC 2483 - however, for compatibility
    // reasons we also allow just \n here.

    // Process the input and copy the first valid URL into the url member.
    // In case no URLs can be found, subsequent calls to getData("URL")
    // will get an empty string. This is in line with the HTML5 spec (see
    // "The DragEvent and DataTransfer interfaces"). Also extract all filenames
    // from the URI list.
    bool setURL = hasURL();
    for (auto& line : split(uriListString, '\n')) {
        line = trim(line, deprecatedIsSpaceOrNewline);
        if (isEmpty(line))
            continue;
        if (codeUnitAt(line, 0) == '#')
            continue;

        URL url { line };
        if (url.isValid()) {
            if (!setURL) {
                m_url = url;
                setURL = true;
            }

            GUniqueOutPtr<GError> error;
            GUniquePtr<gchar> filename(g_filename_from_uri(utf8(line).legacyCStringPointer(), 0, &error.outPtr()));
            if (!error && filename)
                m_filenames.append(StringImpl::fromUTF8(filename.get()));
        }
    }
}

void SelectionData::setURL(const URL& url, StringImpl* label)
{
    m_url = url;
    if (isEmpty(m_uriList))
        m_uriList = url.string();

    if (!hasText())
        setText(url.string());

    if (hasMarkup())
        return;

    RefPtr<StringImpl> actualLabel = isEmpty(label) ? url.string() : RefPtr<StringImpl> { label };
    auto escaped = gMarkupEscapeText(utf8(actualLabel));
    setMarkup(makeString("<a href=\""_s, url.string(), "\">"_s, escaped.span(), "</a>"_s));
}

const RefPtr<StringImpl>& SelectionData::urlLabel() const
{
    if (hasText())
        return text();

    if (hasURL())
        return url().string();

    return emptyString();
}

void SelectionData::clearAllExceptFilenames()
{
    clearText();
    clearMarkup();
    clearURIList();
    clearURL();
    clearImage();
    clearCustomData();
    clearBuffers();

    m_canSmartReplace = false;
}

void SelectionData::clearAll()
{
    clearAllExceptFilenames();
    m_filenames.clear();
}

} // namespace WebCore
