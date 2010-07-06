/*
 * Copyright (C) 2010 ProFUSION embedded systems
 * Copyright (C) 2010 Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "ContextMenuClientEfl.h"

#include "ContextMenu.h"
#include "EWebKit.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "PlatformMenuDescription.h"
#include "ewk_private.h"

using namespace WebCore;

namespace WebCore {

ContextMenuClientEfl::ContextMenuClientEfl(Evas_Object* view)
    : m_view(view)
{
}

void ContextMenuClientEfl::contextMenuDestroyed()
{
    delete this;
}

PlatformMenuDescription ContextMenuClientEfl::getCustomMenuFromDefaultItems(ContextMenu* menu)
{
    PlatformMenuDescription newmenu = ewk_context_menu_custom_get(static_cast<Ewk_Context_Menu*>(menu->releasePlatformDescription()));

    return newmenu;
}

void ContextMenuClientEfl::contextMenuItemSelected(ContextMenuItem*, const ContextMenu*)
{
    notImplemented();
}

void ContextMenuClientEfl::downloadURL(const KURL& url)
{
    if (!m_view)
        return;

    Ewk_Download download;

    CString downloadUrl = url.prettyURL().utf8();
    download.url = downloadUrl.data();
    ewk_view_download_request(m_view, &download);
}

void ContextMenuClientEfl::searchWithGoogle(const Frame*)
{
    notImplemented();
}

void ContextMenuClientEfl::lookUpInDictionary(Frame*)
{
    notImplemented();
}

bool ContextMenuClientEfl::isSpeaking()
{
    notImplemented();
    return false;
}

void ContextMenuClientEfl::speak(const String&)
{
    notImplemented();
}

void ContextMenuClientEfl::stopSpeaking()
{
    notImplemented();
}

PlatformMenuDescription ContextMenuClientEfl::createPlatformDescription(ContextMenu* menu)
{
    return (PlatformMenuDescription) ewk_context_menu_new(m_view, menu->controller());
}

void ContextMenuClientEfl::freePlatformDescription(PlatformMenuDescription menu)
{
    ewk_context_menu_free(static_cast<Ewk_Context_Menu*>(menu));
}

void ContextMenuClientEfl::appendItem(PlatformMenuDescription menu, ContextMenuItem& item)
{
    ewk_context_menu_item_append(static_cast<Ewk_Context_Menu*>(menu), item);
}

void ContextMenuClientEfl::show(PlatformMenuDescription menu)
{
    ewk_context_menu_show(static_cast<Ewk_Context_Menu*>(menu));
}

}
