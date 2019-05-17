/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2010 Igalia S.L
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LocalizedStrings.h"

#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

#if ENABLE(CONTEXT_MENUS)
String contextMenuItemTagCopyLinkToClipboard()
{
    return String::fromUTF8(_("Copy Link Loc_ation"));
}

String contextMenuItemTagDownloadImageToDisk()
{
    return String::fromUTF8(_("Sa_ve Image As"));
}

String contextMenuItemTagCopyImageUrlToClipboard()
{
    return String::fromUTF8(_("Copy Image _Address"));
}

String contextMenuItemTagCopyVideoLinkToClipboard()
{
    return String::fromUTF8(_("Cop_y Video Link Location"));
}

String contextMenuItemTagCopyAudioLinkToClipboard()
{
    return String::fromUTF8(_("Cop_y Audio Link Location"));
}

String contextMenuItemTagToggleMediaControls()
{
    return String::fromUTF8(_("_Toggle Media Controls"));
}

String contextMenuItemTagShowMediaControls()
{
    return String::fromUTF8(_("_Show Media Controls"));
}

String contextMenuItemTagHideMediaControls()
{
    return String::fromUTF8(_("_Hide Media Controls"));
}

String contextMenuItemTagToggleMediaLoop()
{
    return String::fromUTF8(_("Toggle Media _Loop Playback"));
}

String contextMenuItemTagEnterVideoFullscreen()
{
    return String::fromUTF8(_("Switch Video to _Fullscreen"));
}

String contextMenuItemTagDelete()
{
    return String::fromUTF8(_("_Delete"));
}

String contextMenuItemTagSelectAll()
{
    return String::fromUTF8(_("Select _All"));
}

String contextMenuItemTagInsertEmoji()
{
    return String::fromUTF8(_("Insert _Emoji"));
}

String contextMenuItemTagUnicode()
{
    return String::fromUTF8(_("_Insert Unicode Control Character"));
}

String contextMenuItemTagInputMethods()
{
    return String::fromUTF8(_("Input _Methods"));
}

String contextMenuItemTagUnicodeInsertLRMMark()
{
    return String::fromUTF8(_("LRM _Left-to-right mark"));
}

String contextMenuItemTagUnicodeInsertRLMMark()
{
    return String::fromUTF8(_("RLM _Right-to-left mark"));
}

String contextMenuItemTagUnicodeInsertLREMark()
{
    return String::fromUTF8(_("LRE Left-to-right _embedding"));
}

String contextMenuItemTagUnicodeInsertRLEMark()
{
    return String::fromUTF8(_("RLE Right-to-left e_mbedding"));
}

String contextMenuItemTagUnicodeInsertLROMark()
{
    return String::fromUTF8(_("LRO Left-to-right _override"));
}

String contextMenuItemTagUnicodeInsertRLOMark()
{
    return String::fromUTF8(_("RLO Right-to-left o_verride"));
}

String contextMenuItemTagUnicodeInsertPDFMark()
{
    return String::fromUTF8(_("PDF _Pop directional formatting"));
}

String contextMenuItemTagUnicodeInsertZWSMark()
{
    return String::fromUTF8(_("ZWS _Zero width space"));
}

String contextMenuItemTagUnicodeInsertZWJMark()
{
    return String::fromUTF8(_("ZWJ Zero width _joiner"));
}

String contextMenuItemTagUnicodeInsertZWNJMark()
{
    return String::fromUTF8(_("ZWNJ Zero width _non-joiner"));
}
#endif // ENABLE(CONTEXT_MENUS)

String validationMessageTooShortText(int, int minLength)
{
    GUniquePtr<char> string(g_strdup_printf(ngettext("Use at least one character", "Use at least %d characters", minLength), minLength));
    return String::fromUTF8(string.get());
}

String validationMessageTooLongText(int, int maxLength)
{
    GUniquePtr<char> string(g_strdup_printf(ngettext("Use no more than one character", "Use no more than %d characters", maxLength), maxLength));
    return String::fromUTF8(string.get());
}

} // namespace WebCore
