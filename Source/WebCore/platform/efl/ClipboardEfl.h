/*
 *  Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 *  Copyright (C) 2009-2010 ProFUSION embedded systems
 *  Copyright (C) 2009-2010 Samsung Electronics
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

#ifndef ClipboardEfl_h
#define ClipboardEfl_h

#include "Clipboard.h"

namespace WebCore {
class CachedImage;

class ClipboardEfl : public Clipboard {
public:
    static PassRefPtr<ClipboardEfl> create(ClipboardAccessPolicy policy, ClipboardType clipboardType = CopyAndPaste)
    {
        return adoptRef(new ClipboardEfl(policy, clipboardType));
    }
    ~ClipboardEfl();

    void clearData(const String&);
    void clearAllData();
    String getData(const String&) const;
    bool setData(const String&, const String&);

    Vector<String> types() const;
    virtual PassRefPtr<FileList> files() const;

    IntPoint dragLocation() const;
    CachedImage* dragImage() const;
    void setDragImage(CachedImage*, const IntPoint&);
    Node* dragImageElement();
    void setDragImageElement(Node*, const IntPoint&);

    virtual DragImageRef createDragImage(IntPoint&) const;
    virtual void declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*);
    virtual void writeURL(const KURL&, const String&, Frame*);
    virtual void writeRange(Range*, Frame*);

    virtual bool hasData();

    virtual void writePlainText(const WTF::String&);

#if ENABLE(DATA_TRANSFER_ITEMS)
    virtual PassRefPtr<DataTransferItemList> items();
#endif

private:
    ClipboardEfl(ClipboardAccessPolicy, ClipboardType);
};
}

#endif
