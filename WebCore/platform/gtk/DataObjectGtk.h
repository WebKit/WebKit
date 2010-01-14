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

#include "CString.h"
#include "FileList.h"
#include "KURL.h"
#include "Range.h"
#include "StringHash.h"
#include <wtf/RefCounted.h>
#include <wtf/gtk/GRefPtr.h>

typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkDragContext GdkDragContext;
typedef struct _GtkClipboard GtkClipboard;

namespace WebCore {

class DataObjectGtk : public RefCounted<DataObjectGtk> {
public:
    static PassRefPtr<DataObjectGtk> create()
    {
        return adoptRef(new DataObjectGtk());
    }

    Vector<KURL> uriList() { return m_uriList; }
    GdkPixbuf* image() { return m_image.get(); }
    void setRange(PassRefPtr<Range> newRange) { m_range = newRange; }
    void setURIList(const Vector<KURL>& newURIList) {  m_uriList = newURIList; }
    void setImage(GdkPixbuf* newImage) { m_image = newImage; }
    void setDragContext(GdkDragContext* newDragContext) { m_dragContext = newDragContext; }
    bool hasText() { return m_range || !m_text.isEmpty(); }
    bool hasMarkup() { return m_range || !m_markup.isEmpty(); }
    bool hasURIList() { return !m_uriList.isEmpty(); }
    bool hasImage() { return m_image; }
    GdkDragContext* dragContext() { return m_dragContext.get(); }

    String text();
    String markup();
    Vector<String> files();
    void setText(const String& newText);
    void setMarkup(const String& newMarkup);
    bool hasURL();
    String url();
    String urlLabel();
    void clear();

    static DataObjectGtk* forClipboard(GtkClipboard*);

private:
    String m_text;
    String m_markup;
    Vector<KURL> m_uriList;
    GRefPtr<GdkPixbuf> m_image;
    GRefPtr<GdkDragContext> m_dragContext;
    RefPtr<Range> m_range;
};

}

#endif // DataObjectGtk_h
