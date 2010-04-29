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
        return createMarkup(m_range.get(), 0, AnnotateForInterchange);
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

Vector<String> DataObjectGtk::files()
{
    Vector<KURL> uris(uriList());
    Vector<String> files;

    for (size_t i = 0; i < uris.size(); i++) {
        KURL& uri = uris[0];
        if (!uri.isValid() || !uri.isLocalFile())
            continue;

        files.append(uri.string());
    }

    return files;
}

String DataObjectGtk::url()
{
    Vector<KURL> uris(uriList());
    for (size_t i = 0; i < uris.size(); i++) {
        KURL& uri = uris[0];
        if (uri.isValid())
            return uri;
    }

    return String();
}

String DataObjectGtk::urlLabel()
{
    if (hasText())
        return text();

    if (hasURL())
        return url();

    return String();
}

bool DataObjectGtk::hasURL()
{
    return !url().isEmpty();
}

void DataObjectGtk::clear()
{
    m_text = "";
    m_markup = "";
    m_uriList.clear();
    m_image = 0;
    m_range = 0;
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
