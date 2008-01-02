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
#include "ContextMenu.h"
#include "ContextMenuClientGtk.h"

#include "HitTestResult.h"
#include "KURL.h"
#include "NotImplemented.h"

using namespace WebCore;

namespace WebKit {

void ContextMenuClient::contextMenuDestroyed()
{
    delete this;
}

PlatformMenuDescription ContextMenuClient::getCustomMenuFromDefaultItems(ContextMenu* menu)
{
    return menu->releasePlatformDescription();
}

void ContextMenuClient::contextMenuItemSelected(ContextMenuItem*, const ContextMenu*)
{
    notImplemented();
}

void ContextMenuClient::downloadURL(const KURL& url)
{
    notImplemented();
}

void ContextMenuClient::copyImageToClipboard(const HitTestResult&)
{
    notImplemented();
}

void ContextMenuClient::searchWithGoogle(const Frame*)
{
    notImplemented();
}

void ContextMenuClient::lookUpInDictionary(Frame*)
{
    notImplemented();
}

void ContextMenuClient::speak(const String&)
{
    notImplemented();
}

void ContextMenuClient::stopSpeaking()
{
    notImplemented();
}

}

