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

#include "LocalizedStrings.h"
#include "GOwnPtr.h"
#include "IntSize.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include <wtf/text/CString.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <math.h>

namespace WebCore {

static const char* gtkStockLabel(const char* stockID)
{
    GtkStockItem item;
    if (!gtk_stock_lookup(stockID, &item))
        return stockID;
    return item.label;
}

String submitButtonDefaultLabel()
{
    return String::fromUTF8(_("Submit"));
}

String inputElementAltText()
{
    return String::fromUTF8(_("Submit"));
}

String resetButtonDefaultLabel()
{
    return String::fromUTF8(_("Reset"));
}

String searchableIndexIntroduction()
{
    return String::fromUTF8(_("This is a searchable index. Enter search keywords: "));
}

String fileButtonChooseFileLabel()
{
    return String::fromUTF8(_("Choose File"));
}

String fileButtonNoFileSelectedLabel()
{
    return String::fromUTF8(_("(None)"));
}

String contextMenuItemTagOpenLinkInNewWindow()
{
    return String::fromUTF8(_("Open Link in New _Window"));
}

String contextMenuItemTagDownloadLinkToDisk()
{
    return String::fromUTF8(_("_Download Linked File"));
}

String contextMenuItemTagCopyLinkToClipboard()
{
    return String::fromUTF8(_("Copy Link Loc_ation"));
}

String contextMenuItemTagOpenImageInNewWindow()
{
    return String::fromUTF8(_("Open _Image in New Window"));
}

String contextMenuItemTagDownloadImageToDisk()
{
    return String::fromUTF8(_("Sa_ve Image As"));
}

String contextMenuItemTagCopyImageToClipboard()
{
    return String::fromUTF8(_("Cop_y Image"));
}

String contextMenuItemTagOpenMediaInNewWindow()
{
    return String::fromUTF8(_("Open _Media in New Window"));
}

String contextMenuItemTagCopyMediaLinkToClipboard()
{
    return String::fromUTF8(_("Cop_y Media Link Location"));
}

String contextMenuItemTagToggleMediaControls()
{
    return String::fromUTF8(_("_Toggle Media Controls"));
}

String contextMenuItemTagToggleMediaLoop()
{
    return String::fromUTF8(_("Toggle Media _Loop Playback"));
}

String contextMenuItemTagEnterVideoFullscreen()
{
    return String::fromUTF8(_("Switch Video to _Fullscreen"));
}

String contextMenuItemTagMediaPlay()
{
    return String::fromUTF8(_("_Play"));
}

String contextMenuItemTagMediaPause()
{
    return String::fromUTF8(_("_Pause"));
}

String contextMenuItemTagMediaMute()
{
    return String::fromUTF8(_("_Mute"));
}

String contextMenuItemTagMediaUnMute()
{
    return String::fromUTF8(_("Un_Mute"));
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    return String::fromUTF8(_("Open _Frame in New Window"));
}

String contextMenuItemTagCopy()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_COPY));
    return stockLabel;
}

String contextMenuItemTagDelete()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_DELETE));
    return stockLabel;
}

String contextMenuItemTagSelectAll()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_SELECT_ALL));
    return stockLabel;
}

String contextMenuItemTagUnicode()
{
    return String::fromUTF8(_("_Insert Unicode Control Character"));
}

String contextMenuItemTagInputMethods()
{
    return String::fromUTF8(_("Input _Methods"));
}

String contextMenuItemTagGoBack()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_GO_BACK));
    return stockLabel;
}

String contextMenuItemTagGoForward()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_GO_FORWARD));
    return stockLabel;
}

String contextMenuItemTagStop()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_STOP));
    return stockLabel;
}

String contextMenuItemTagReload()
{
    return String::fromUTF8(_("_Reload"));
}

String contextMenuItemTagCut()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_CUT));
    return stockLabel;
}

String contextMenuItemTagPaste()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_PASTE));
    return stockLabel;
}

String contextMenuItemTagNoGuessesFound()
{
    return String::fromUTF8(_("No Guesses Found"));
}

String contextMenuItemTagIgnoreSpelling()
{
    return String::fromUTF8(_("_Ignore Spelling"));
}

String contextMenuItemTagLearnSpelling()
{
    return String::fromUTF8(_("_Learn Spelling"));
}

String contextMenuItemTagSearchWeb()
{
    return String::fromUTF8(_("_Search the Web"));
}

String contextMenuItemTagLookUpInDictionary()
{
    return String::fromUTF8(_("_Look Up in Dictionary"));
}

String contextMenuItemTagOpenLink()
{
    return String::fromUTF8(_("_Open Link"));
}

String contextMenuItemTagIgnoreGrammar()
{
    return String::fromUTF8(_("Ignore _Grammar"));
}

String contextMenuItemTagSpellingMenu()
{
    return String::fromUTF8(_("Spelling and _Grammar"));
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    return String::fromUTF8(show ? _("_Show Spelling and Grammar") : _("_Hide Spelling and Grammar"));
}

String contextMenuItemTagCheckSpelling()
{
    return String::fromUTF8(_("_Check Document Now"));
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    return String::fromUTF8(_("Check Spelling While _Typing"));
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    return String::fromUTF8(_("Check _Grammar With Spelling"));
}

String contextMenuItemTagFontMenu()
{
    return String::fromUTF8(_("_Font"));
}

String contextMenuItemTagBold()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_BOLD));
    return stockLabel;
}

String contextMenuItemTagItalic()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_ITALIC));
    return stockLabel;
}

String contextMenuItemTagUnderline()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_UNDERLINE));
    return stockLabel;
}

String contextMenuItemTagOutline()
{
    return String::fromUTF8(_("_Outline"));
}

String contextMenuItemTagInspectElement()
{
    return String::fromUTF8(_("Inspect _Element"));
}

String searchMenuNoRecentSearchesText()
{
    return String::fromUTF8(_("No recent searches"));
}

String searchMenuRecentSearchesText()
{
    return String::fromUTF8(_("Recent searches"));
}

String searchMenuClearRecentSearchesText()
{
    return String::fromUTF8(_("_Clear recent searches"));
}

String AXDefinitionListTermText()
{
    return String::fromUTF8(_("term"));
}

String AXDefinitionListDefinitionText()
{
    return String::fromUTF8(_("definition"));
}

String AXButtonActionVerb()
{
    return String::fromUTF8(_("press"));
}

String AXRadioButtonActionVerb()
{
    return String::fromUTF8(_("select"));
}

String AXTextFieldActionVerb()
{
    return String::fromUTF8(_("activate"));
}

String AXCheckedCheckBoxActionVerb()
{
    return String::fromUTF8(_("uncheck"));
}

String AXUncheckedCheckBoxActionVerb()
{
    return String::fromUTF8(_("check"));
}

String AXLinkActionVerb()
{
    return String::fromUTF8(_("jump"));
}

String AXMenuListPopupActionVerb()
{
    return String();
}

String AXMenuListActionVerb()
{
    return String();
}
    
String missingPluginText()
{
    return String::fromUTF8(_("Missing Plug-in"));
}

String crashedPluginText()
{
    notImplemented();
    return String::fromUTF8(_("Plug-in Failure"));
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    // FIXME: If this file gets localized, this should really be localized as one string with a wildcard for the number.
    return String::number(numberOfFiles) + String::fromUTF8(_(" files"));
}

String unknownFileSizeText()
{
    return String::fromUTF8(_("Unknown"));
}

String imageTitle(const String& filename, const IntSize& size)
{
    GOwnPtr<gchar> string(g_strdup_printf(C_("Title string for images", "%s  (%dx%d pixels)"),
                                          filename.utf8().data(),
                                          size.width(), size.height()));

    return String::fromUTF8(string.get());
}


#if ENABLE(VIDEO)

String mediaElementLoadingStateText()
{
    return String::fromUTF8(_("Loading..."));
}

String mediaElementLiveBroadcastStateText()
{
    return String::fromUTF8(_("Live Broadcast"));
}

String localizedMediaControlElementString(const String& name)
{
    if (name == "AudioElement")
        return String::fromUTF8(_("audio element controller"));
    if (name == "VideoElement")
        return String::fromUTF8(_("video element controller"));
    if (name == "MuteButton")
        return String::fromUTF8(_("mute"));
    if (name == "UnMuteButton")
        return String::fromUTF8(_("unmute"));
    if (name == "PlayButton")
        return String::fromUTF8(_("play"));
    if (name == "PauseButton")
        return String::fromUTF8(_("pause"));
    if (name == "Slider")
        return String::fromUTF8(_("movie time"));
    if (name == "SliderThumb")
        return String::fromUTF8(_("timeline slider thumb"));
    if (name == "RewindButton")
        return String::fromUTF8(_("back 30 seconds"));
    if (name == "ReturnToRealtimeButton")
        return String::fromUTF8(_("return to realtime"));
    if (name == "CurrentTimeDisplay")
        return String::fromUTF8(_("elapsed time"));
    if (name == "TimeRemainingDisplay")
        return String::fromUTF8(_("remaining time"));
    if (name == "StatusDisplay")
        return String::fromUTF8(_("status"));
    if (name == "FullscreenButton")
        return String::fromUTF8(_("fullscreen"));
    if (name == "SeekForwardButton")
        return String::fromUTF8(_("fast forward"));
    if (name == "SeekBackButton")
        return String::fromUTF8(_("fast reverse"));
    if (name == "ShowClosedCaptionsButton")
        return String::fromUTF8(_("show closed captions"));
    if (name == "HideClosedCaptionsButton")
        return String::fromUTF8(_("hide closed captions"));

    ASSERT_NOT_REACHED();
    return String();
}

String localizedMediaControlElementHelpText(const String& name)
{
    if (name == "AudioElement")
        return String::fromUTF8(_("audio element playback controls and status display"));
    if (name == "VideoElement")
        return String::fromUTF8(_("video element playback controls and status display"));
    if (name == "MuteButton")
        return String::fromUTF8(_("mute audio tracks"));
    if (name == "UnMuteButton")
        return String::fromUTF8(_("unmute audio tracks"));
    if (name == "PlayButton")
        return String::fromUTF8(_("begin playback"));
    if (name == "PauseButton")
        return String::fromUTF8(_("pause playback"));
    if (name == "Slider")
        return String::fromUTF8(_("movie time scrubber"));
    if (name == "SliderThumb")
        return String::fromUTF8(_("movie time scrubber thumb"));
    if (name == "RewindButton")
        return String::fromUTF8(_("seek movie back 30 seconds"));
    if (name == "ReturnToRealtimeButton")
        return String::fromUTF8(_("return streaming movie to real time"));
    if (name == "CurrentTimeDisplay")
        return String::fromUTF8(_("current movie time in seconds"));
    if (name == "TimeRemainingDisplay")
        return String::fromUTF8(_("number of seconds of movie remaining"));
    if (name == "StatusDisplay")
        return String::fromUTF8(_("current movie status"));
    if (name == "SeekBackButton")
        return String::fromUTF8(_("seek quickly back"));
    if (name == "SeekForwardButton")
        return String::fromUTF8(_("seek quickly forward"));
    if (name == "FullscreenButton")
        return String::fromUTF8(_("Play movie in fullscreen mode"));
    if (name == "ShowClosedCaptionsButton")
        return String::fromUTF8(_("start displaying closed captions"));
    if (name == "HideClosedCaptionsButton")
        return String::fromUTF8(_("stop displaying closed captions"));

    ASSERT_NOT_REACHED();
    return String();
}

String localizedMediaTimeDescription(float time)
{
    if (!isfinite(time))
        return String::fromUTF8(_("indefinite time"));

    int seconds = (int)fabsf(time);
    int days = seconds / (60 * 60 * 24);
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (days) {
        GOwnPtr<gchar> string(g_strdup_printf("%d days %d hours %d minutes %d seconds", days, hours, minutes, seconds));
        return String::fromUTF8(string.get());
    }

    if (hours) {
        GOwnPtr<gchar> string(g_strdup_printf("%d hours %d minutes %d seconds", hours, minutes, seconds));
        return String::fromUTF8(string.get());
    }

    if (minutes) {
        GOwnPtr<gchar> string(g_strdup_printf("%d minutes %d seconds", minutes, seconds));
        return String::fromUTF8(string.get());
    }

    GOwnPtr<gchar> string(g_strdup_printf("%d seconds", seconds));
    return String::fromUTF8(string.get());
}
#endif  // ENABLE(VIDEO)

String validationMessageValueMissingText()
{
    return String::fromUTF8(_("value missing"));
}

String validationMessageTypeMismatchText()
{
    notImplemented();
    return String::fromUTF8(_("type mismatch"));
}

String validationMessagePatternMismatchText()
{
    return String::fromUTF8(_("pattern mismatch"));
}

String validationMessageTooLongText()
{
    return String::fromUTF8(_("too long"));
}

String validationMessageRangeUnderflowText()
{
    return String::fromUTF8(_("range underflow"));
}

String validationMessageRangeOverflowText()
{
    return String::fromUTF8(_("range overflow"));
}

String validationMessageStepMismatchText()
{
    return String::fromUTF8(_("step mismatch"));
}

}
