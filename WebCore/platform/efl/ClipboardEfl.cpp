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

#include "config.h"
#include "ClipboardEfl.h"

#include "Editor.h"
#include "FileList.h"
#include "NotImplemented.h"
#include "StringHash.h"

namespace WebCore {
PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy)
{
    return new ClipboardEfl(policy, false);
}

ClipboardEfl::ClipboardEfl(ClipboardAccessPolicy policy, bool forDragging)
    : Clipboard(policy, forDragging)
{
    notImplemented();
}

ClipboardEfl::~ClipboardEfl()
{
    notImplemented();
}

void ClipboardEfl::clearData(const String&)
{
    notImplemented();
}

void ClipboardEfl::writePlainText(const WebCore::String&)
{
    notImplemented();
}

void ClipboardEfl::clearAllData()
{
    notImplemented();
}

String ClipboardEfl::getData(const String&, bool &success) const
{
    notImplemented();
    success = false;
    return String();
}

bool ClipboardEfl::setData(const String&, const String&)
{
    notImplemented();
    return false;
}

HashSet<String> ClipboardEfl::types() const
{
    notImplemented();
    return HashSet<String>();
}

PassRefPtr<FileList> ClipboardEfl::files() const
{
    notImplemented();
    return 0;
}

IntPoint ClipboardEfl::dragLocation() const
{
    notImplemented();
    return IntPoint(0, 0);
}

CachedImage* ClipboardEfl::dragImage() const
{
    notImplemented();
    return 0;
}

void ClipboardEfl::setDragImage(CachedImage*, const IntPoint&)
{
    notImplemented();
}

Node* ClipboardEfl::dragImageElement()
{
    notImplemented();
    return 0;
}

void ClipboardEfl::setDragImageElement(Node*, const IntPoint&)
{
    notImplemented();
}

DragImageRef ClipboardEfl::createDragImage(IntPoint&) const
{
    notImplemented();
    return 0;
}

void ClipboardEfl::declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardEfl::writeURL(const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardEfl::writeRange(Range*, Frame*)
{
    notImplemented();
}

bool ClipboardEfl::hasData()
{
    notImplemented();
    return false;
}

}
