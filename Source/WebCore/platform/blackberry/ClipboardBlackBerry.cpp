/*
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
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

#include "config.h"
#include "ClipboardBlackBerry.h"

#include "FileList.h"
#include "NotImplemented.h"

#include <BlackBerryPlatformClipboard.h>

namespace WebCore {

PassRefPtr<ClipboardBlackBerry> ClipboardBlackBerry::create(ClipboardAccessPolicy policy, ClipboardType clipboardType)
{
    return adoptRef(new ClipboardBlackBerry(policy, clipboardType));
}

PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy policy, DragData*, Frame*)
{
    return ClipboardBlackBerry::create(policy, DragAndDrop);
}

ClipboardBlackBerry::ClipboardBlackBerry(ClipboardAccessPolicy policy, ClipboardType type)
: Clipboard(policy, type)
{
}

ClipboardBlackBerry::~ClipboardBlackBerry()
{
}

void ClipboardBlackBerry::clearData(const String& type)
{
    BlackBerry::Platform::Clipboard::clearClipboardByType(type.utf8().data());
}

void ClipboardBlackBerry::clearAllData()
{
    BlackBerry::Platform::Clipboard::clearClipboard();
}

String ClipboardBlackBerry::getData(const String& type, bool& success) const
{
    success = true;
    return String::fromUTF8(BlackBerry::Platform::Clipboard::readClipboardByType(type.utf8().data()).c_str());
}

bool ClipboardBlackBerry::setData(const String& type, const String& text)
{
    if (type == "text/plain")
        BlackBerry::Platform::Clipboard::writePlainText(text.utf8().data());
    else if (type == "text/html")
        BlackBerry::Platform::Clipboard::writeHTML(text.utf8().data());
    else if (type == "text/url")
        BlackBerry::Platform::Clipboard::writeURL(text.utf8().data());
    return true;
}

HashSet<String> ClipboardBlackBerry::types() const
{
    // We use hardcoded list here since there seems to be no API to get the list.
    HashSet<String> ret;
    ret.add("text/plain");
    ret.add("text/html");
    ret.add("text/url");
    return ret;
}

PassRefPtr<FileList> ClipboardBlackBerry::files() const
{
    notImplemented();
    return 0;
}

DragImageRef ClipboardBlackBerry::createDragImage(IntPoint&) const
{
    notImplemented();
    return 0;
}

void ClipboardBlackBerry::declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*)
{
    notImplemented();
}

void ClipboardBlackBerry::writeURL(const KURL& url, const String&, Frame*)
{
    ASSERT(!url.isEmpty());
    BlackBerry::Platform::Clipboard::writeURL(url.string().utf8().data());
}

void ClipboardBlackBerry::writeRange(Range*, Frame*)
{
    notImplemented();
}

void ClipboardBlackBerry::writePlainText(const String& text)
{
    BlackBerry::Platform::Clipboard::writePlainText(text.utf8().data());
}

bool ClipboardBlackBerry::hasData()
{
    return BlackBerry::Platform::Clipboard::hasPlainText() || BlackBerry::Platform::Clipboard::hasHTML()
        || BlackBerry::Platform::Clipboard::hasURL();
}

void ClipboardBlackBerry::setDragImage(CachedImage*, const IntPoint&)
{
    notImplemented();
}

void ClipboardBlackBerry::setDragImageElement(Node*, const IntPoint&)
{
    notImplemented();
}

} // namespace WebCore
