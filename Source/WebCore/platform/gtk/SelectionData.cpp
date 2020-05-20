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

#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static void replaceNonBreakingSpaceWithSpace(String& str)
{
    static const UChar NonBreakingSpaceCharacter = 0xA0;
    static const UChar SpaceCharacter = ' ';
    str.replace(NonBreakingSpaceCharacter, SpaceCharacter);
}

void SelectionData::setText(const String& newText)
{
    m_text = newText;
    replaceNonBreakingSpaceWithSpace(m_text);
}

void SelectionData::setURIList(const String& uriListString)
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
    for (auto& line : uriListString.split('\n')) {
        line = line.stripWhiteSpace();
        if (line.isEmpty())
            continue;
        if (line[0] == '#')
            continue;

        URL url = URL(URL(), line);
        if (url.isValid()) {
            if (!setURL) {
                m_url = url;
                setURL = true;
            }

            GUniqueOutPtr<GError> error;
            GUniquePtr<gchar> filename(g_filename_from_uri(line.utf8().data(), 0, &error.outPtr()));
            if (!error && filename)
                m_filenames.append(String::fromUTF8(filename.get()));
        }
    }
}

void SelectionData::setURL(const URL& url, const String& label)
{
    m_url = url;
    if (m_uriList.isEmpty())
        m_uriList = url.string();

    if (!hasText())
        setText(url.string());

    if (hasMarkup())
        return;

    String actualLabel(label);
    if (actualLabel.isEmpty())
        actualLabel = url.string();

    StringBuilder markup;
    markup.append("<a href=\"");
    markup.append(url.string());
    markup.append("\">");
    GUniquePtr<gchar> escaped(g_markup_escape_text(actualLabel.utf8().data(), -1));
    markup.append(String::fromUTF8(escaped.get()));
    markup.append("</a>");
    setMarkup(markup.toString());
}

const String& SelectionData::urlLabel() const
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

    m_canSmartReplace = false;
}

void SelectionData::clearAll()
{
    clearAllExceptFilenames();
    m_filenames.clear();
}

} // namespace WebCore
