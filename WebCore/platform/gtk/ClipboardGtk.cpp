/*
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
#include "ClipboardGtk.h"

#include "NotImplemented.h"
#include "StringHash.h"

#include "Editor.h"

namespace WebCore {
PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy)
{
    return new ClipboardGtk(policy, false);
}

ClipboardGtk::ClipboardGtk(ClipboardAccessPolicy policy, bool forDragging)
    : Clipboard(policy, forDragging)
{
    notImplemented();
}

ClipboardGtk::~ClipboardGtk()
{
    notImplemented();
}

void ClipboardGtk::clearData(const String&)
{
    notImplemented();
}

void ClipboardGtk::clearAllData()
{
    notImplemented();
}

String ClipboardGtk::getData(const String&, bool &success) const
{
    notImplemented();
    success = false;
    return String();
}

bool ClipboardGtk::setData(const String&, const String&)
{
    notImplemented();
    return false;
}

HashSet<String> ClipboardGtk::types() const
{
    notImplemented();
    return HashSet<String>();
}

IntPoint ClipboardGtk::dragLocation() const
{
    notImplemented();
    return IntPoint(0, 0);
}

CachedImage* ClipboardGtk::dragImage() const
{
    notImplemented();
    return 0;
}

void ClipboardGtk::setDragImage(CachedImage*, const IntPoint&)
{
    notImplemented();
}

Node* ClipboardGtk::dragImageElement()
{
    notImplemented();
    return 0;
}

void ClipboardGtk::setDragImageElement(Node*, const IntPoint&)
{
    notImplemented();
}

DragImageRef ClipboardGtk::createDragImage(IntPoint&) const
{
    notImplemented();
    return 0;
}

void ClipboardGtk::declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardGtk::writeURL(const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardGtk::writeRange(Range*, Frame*)
{
    notImplemented();
}

bool ClipboardGtk::hasData()
{
    notImplemented();
    return false;
}

}
