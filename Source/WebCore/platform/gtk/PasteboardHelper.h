/*
 * Copyright (C) 2007 Luca Bruno <lethalman88@gmail.com>
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * Copyright (C) 2010 Martin Robinson <mrobinson@webkit.org>
 * Copyright (C) 2010 Igalia S.L.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef PasteboardHelper_h
#define PasteboardHelper_h

#include "GRefPtrGtk.h"
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

class DataObjectGtk;

class PasteboardHelper {
    WTF_MAKE_NONCOPYABLE(PasteboardHelper);
public:
    static PasteboardHelper& singleton();

    enum SmartPasteInclusion { IncludeSmartPaste, DoNotIncludeSmartPaste };

    GtkTargetList* targetList() const;
    GRefPtr<GtkTargetList> targetListForDataObject(const DataObjectGtk&);
    void fillSelectionData(GtkSelectionData*, guint, const DataObjectGtk&);
    void fillDataObjectFromDropData(GtkSelectionData*, guint, DataObjectGtk&);
    Vector<GdkAtom> dropAtomsForContext(GtkWidget*, GdkDragContext*);
    void writeClipboardContents(GtkClipboard*, const DataObjectGtk&, std::function<void()>&& primarySelectionCleared = nullptr);
    void getClipboardContents(GtkClipboard*, DataObjectGtk&);

    enum PasteboardTargetType { TargetTypeMarkup, TargetTypeText, TargetTypeImage, TargetTypeURIList, TargetTypeNetscapeURL, TargetTypeSmartPaste, TargetTypeUnknown };

private:
    PasteboardHelper();
    ~PasteboardHelper();

    GRefPtr<GtkTargetList> m_targetList;
};

}

#endif // PasteboardHelper_h
