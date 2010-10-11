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
#include "DataObjectGtk.h"

#include "markup.h"
#include <gtk/gtk.h>
#include <wtf/gobject/GOwnPtr.h>

namespace WebCore {

static void replaceNonBreakingSpaceWithSpace(String& str)
{
    static const UChar NonBreakingSpaceCharacter = 0xA0;
    static const UChar SpaceCharacter = ' ';
    str.replace(NonBreakingSpaceCharacter, SpaceCharacter);
}

String DataObjectGtk::text()
{
    if (m_range)
        return m_range->text();
    return m_text;
}

String DataObjectGtk::markup()
{
    if (m_range)
        return createMarkup(m_range.get(), 0, AnnotateForInterchange, false, AbsoluteURLs);
    return m_markup;
}

void DataObjectGtk::setText(const String& newText)
{
    m_range = 0;
    m_text = newText;
    replaceNonBreakingSpaceWithSpace(m_text);
}

void DataObjectGtk::setMarkup(const String& newMarkup)
{
    m_range = 0;
    m_markup = newMarkup;
}

void DataObjectGtk::setURIList(const String& uriListString)
{
    m_uriList = uriListString;

    // This code is originally from: platform/chromium/ChromiumDataObject.cpp.
    // FIXME: We should make this code cross-platform eventually.

    // Line separator is \r\n per RFC 2483 - however, for compatibility
    // reasons we also allow just \n here.
    Vector<String> uriList;
    uriListString.split('\n', uriList);

    // Process the input and copy the first valid URL into the url member.
    // In case no URLs can be found, subsequent calls to getData("URL")
    // will get an empty string. This is in line with the HTML5 spec (see
    // "The DragEvent and DataTransfer interfaces"). Also extract all filenames
    // from the URI list.
    bool setURL = false;
    for (size_t i = 0; i < uriList.size(); ++i) {
        String& line = uriList[i];
        line = line.stripWhiteSpace();
        if (line.isEmpty())
            continue;
        if (line[0] == '#')
            continue;

        KURL url = KURL(KURL(), line);
        if (url.isValid()) {
            if (!setURL) {
                m_url = url;
                setURL = true;
            }

            GOwnPtr<GError> error;
            GOwnPtr<gchar> filename(g_filename_from_uri(line.utf8().data(), 0, &error.outPtr()));
            if (!error && filename)
                m_filenames.append(String::fromUTF8(filename.get()));
        }
    }
}

void DataObjectGtk::setURL(const KURL& url, const String& label)
{
    m_url = url;
    m_uriList = url;
    setText(url.string());

    String actualLabel(label);
    if (actualLabel.isEmpty())
        actualLabel = url;

    Vector<UChar> markup;
    append(markup, "<a href=\"");
    append(markup, url.string());
    append(markup, "\">");
    GOwnPtr<gchar> escaped(g_markup_escape_text(actualLabel.utf8().data(), -1));
    append(markup, String::fromUTF8(escaped.get()));
    append(markup, "</a>");
    setMarkup(String::adopt(markup));
}

void DataObjectGtk::clearText()
{
    m_range = 0;
    m_text = "";
}

void DataObjectGtk::clearMarkup()
{
    m_range = 0;
    m_markup = "";
}

String DataObjectGtk::urlLabel()
{
    if (hasText())
        return text();

    if (hasURL())
        return url();

    return String();
}

void DataObjectGtk::clear()
{
    m_text = "";
    m_markup = "";
    m_uriList = "";
    m_url = KURL();
    m_image = 0;
    m_range = 0;

    // We do not clear filenames. According to the spec: "The clearData() method
    // does not affect whether any files were included in the drag, so the types
    // attribute's list might still not be empty after calling clearData() (it would 
    // still contain the "Files" string if any files were included in the drag)."
}

DataObjectGtk* DataObjectGtk::forClipboard(GtkClipboard* clipboard)
{
    static HashMap<GtkClipboard*, RefPtr<DataObjectGtk> > objectMap;

    if (!objectMap.contains(clipboard)) {
        RefPtr<DataObjectGtk> dataObject = DataObjectGtk::create();
        objectMap.set(clipboard, dataObject);
        return dataObject.get();
    }

    HashMap<GtkClipboard*, RefPtr<DataObjectGtk> >::iterator it = objectMap.find(clipboard);
    return it->second.get();
}

}
