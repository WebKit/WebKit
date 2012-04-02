/*
 * Copyright (C) 2010, 2012 Research In Motion Limited. All rights reserved.
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

#ifndef ClipboardBlackBerry_h
#define ClipboardBlackBerry_h

#include "CachedResourceClient.h"
#include "Clipboard.h"

namespace WebCore {

class ClipboardBlackBerry : public Clipboard, public CachedResourceClient {
WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<ClipboardBlackBerry> create(ClipboardAccessPolicy policy, ClipboardType clipboardType = CopyAndPaste)
    {
        return adoptRef(new ClipboardBlackBerry(policy, clipboardType));
    }
    virtual ~ClipboardBlackBerry();

    void clearData(const String& type);
    void clearAllData();
    String getData(const String& type) const;
    bool setData(const String& type, const String& data);

    // extensions beyond IE's API
    virtual HashSet<String> types() const;
    virtual PassRefPtr<FileList> files() const;
    virtual DragImageRef createDragImage(IntPoint&) const;
    virtual void declareAndWriteDragImage(Element*, const KURL&, const String& title, Frame*);
    virtual void writeURL(const KURL&, const String&, Frame*);
    virtual void writeRange(Range*, Frame*);
    virtual void writePlainText(const String&);

    virtual bool hasData();

    virtual void setDragImage(CachedImage*, const IntPoint&);
    virtual void setDragImageElement(Node*, const IntPoint&);

private:
    ClipboardBlackBerry(ClipboardAccessPolicy, ClipboardType);
};

} // namespace WebCore

#endif // ClipboardBlackBerry_h
