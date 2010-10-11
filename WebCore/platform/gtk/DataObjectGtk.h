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

#ifndef DataObjectGtk_h
#define DataObjectGtk_h

#include "FileList.h"
#include <GRefPtr.h>
#include "KURL.h"
#include "Range.h"
#include <wtf/RefCounted.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class DataObjectGtk : public RefCounted<DataObjectGtk> {
public:
    static PassRefPtr<DataObjectGtk> create()
    {
        return adoptRef(new DataObjectGtk());
    }

    const KURL& url() { return m_url; }
    const String& uriList() { return m_uriList; }
    const Vector<String>& filenames() { return m_filenames; }
    GdkPixbuf* image() { return m_image.get(); }
    void setRange(PassRefPtr<Range> newRange) { m_range = newRange; }
    void setImage(GdkPixbuf* newImage) { m_image = newImage; }
    void setDragContext(GdkDragContext* newDragContext) { m_dragContext = newDragContext; }
    void setURL(const KURL&, const String&);
    bool hasText() { return m_range || !m_text.isEmpty(); }
    bool hasMarkup() { return m_range || !m_markup.isEmpty(); }
    bool hasURIList() { return !m_uriList.isEmpty(); }
    bool hasURL() { return !m_url.isEmpty() && m_url.isValid(); }
    bool hasFilenames() { return !m_filenames.isEmpty(); }
    bool hasImage() { return m_image; }
    void clearURIList() { m_uriList = ""; }
    void clearURL() { m_url = KURL(); }
    void clearImage() { m_image = 0; }
    GdkDragContext* dragContext() { return m_dragContext.get(); }

    String text();
    String markup();
    void setText(const String&);
    void setMarkup(const String&);
    void setURIList(const String&);
    String urlLabel();
    void clear();
    void clearText();
    void clearMarkup();

    static DataObjectGtk* forClipboard(GtkClipboard*);

private:
    String m_text;
    String m_markup;
    KURL m_url;
    String m_uriList;
    Vector<String> m_filenames;
    PlatformRefPtr<GdkPixbuf> m_image;
    PlatformRefPtr<GdkDragContext> m_dragContext;
    RefPtr<Range> m_range;
};

}

#endif // DataObjectGtk_h
