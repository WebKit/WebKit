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

#include "IntSize.h"
#include "NotImplemented.h"
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <wtf/MathExtras.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

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

String defaultDetailsSummaryText()
{
    return String::fromUTF8(_("Details"));
}

String searchableIndexIntroduction()
{
    return String::fromUTF8(_("This is a searchable index. Enter search keywords: "));
}

String fileButtonChooseFileLabel()
{
    return String::fromUTF8(_("Choose File"));
}

String fileButtonChooseMultipleFilesLabel()
{
    return String::fromUTF8(_("Choose Files"));
}

String fileButtonNoFileSelectedLabel()
{
    return String::fromUTF8(_("(None)"));
}

String fileButtonNoFilesSelectedLabel()
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

String contextMenuItemTagCopyImageUrlToClipboard()
{
    return String::fromUTF8(_("Copy Image _Address"));
}

String contextMenuItemTagOpenVideoInNewWindow()
{
    return String::fromUTF8(_("Open _Video in New Window"));
}

String contextMenuItemTagOpenAudioInNewWindow()
{
    return String::fromUTF8(_("Open _Audio in New Window"));
}

String contextMenuItemTagDownloadVideoToDisk()
{
    return String::fromUTF8(_("Download _Video"));
}

String contextMenuItemTagDownloadAudioToDisk()
{
    return String::fromUTF8(_("Download _Audio"));
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

String contextMenuItemTagLookUpInDictionary(const String&)
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

String AXDefinitionText()
{
    return String::fromUTF8(_("definition"));
}

String AXDescriptionListText()
{
    return String::fromUTF8(_("description list"));
}

String AXDescriptionListTermText()
{
    return String::fromUTF8(_("term"));
}

String AXDescriptionListDetailText()
{
    return String::fromUTF8(_("description"));
}

String AXDetailsText()
{
    return String::fromUTF8(_("details"));
}

String AXSummaryText()
{
    return String::fromUTF8(_("summary"));
}

String AXFigureText()
{
    return String::fromUTF8(_("figure"));
}

String AXOutputText()
{
    return String::fromUTF8(_("output"));
}

String AXEmailFieldText()
{
    return String::fromUTF8(_("email field"));
}

String AXTelephoneFieldText()
{
    return String::fromUTF8(_("telephone number field"));
}

String AXURLFieldText()
{
    return String::fromUTF8(_("URL field"));
}

String AXDateFieldText()
{
    return String::fromUTF8(_("date field"));
}

String AXTimeFieldText()
{
    return String::fromUTF8(_("time field"));
}

String AXDateTimeFieldText()
{
    return String::fromUTF8(_("date and time field"));
}

String AXMonthFieldText()
{
    return String::fromUTF8(_("month and year field"));
}

String AXNumberFieldText()
{
    return String::fromUTF8(_("number field"));
}

String AXWeekFieldText()
{
    return String::fromUTF8(_("week and year field"));
}

String AXFooterRoleDescriptionText()
{
    return String::fromUTF8(_("footer"));
}

String AXSearchFieldCancelButtonText()
{
    return String::fromUTF8(_("cancel"));
}

String AXAutoFillCredentialsLabel()
{
    return String::fromUTF8(_("password auto fill"));
}

String AXAutoFillContactsLabel()
{
    return String::fromUTF8(_("contact info auto fill"));
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

String AXListItemActionVerb()
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

String blockedPluginByContentSecurityPolicyText()
{
    notImplemented();
    return String();
}

String insecurePluginVersionText()
{
    notImplemented();
    return String();
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
    GUniquePtr<gchar> string(g_strdup_printf(C_("window title for a standalone image (uses multiplication symbol, not x)", "%s %d×%d pixels"),
        filename.utf8().data(), size.width(), size.height()));

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
        return String::fromUTF8(_("audio playback"));
    if (name == "VideoElement")
        return String::fromUTF8(_("video playback"));
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
    if (name == "EnterFullscreenButton")
        return String::fromUTF8(_("enter fullscreen"));
    if (name == "ExitFullscreenButton")
        return String::fromUTF8(_("exit fullscreen"));
    if (name == "SeekForwardButton")
        return String::fromUTF8(_("fast forward"));
    if (name == "SeekBackButton")
        return String::fromUTF8(_("fast reverse"));
    if (name == "ShowClosedCaptionsButton")
        return String::fromUTF8(_("show closed captions"));
    if (name == "HideClosedCaptionsButton")
        return String::fromUTF8(_("hide closed captions"));
    if (name == "ControlsPanel")
        return String::fromUTF8(_("media controls"));

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
    if (name == "EnterFullscreenButton")
        return String::fromUTF8(_("Play movie in fullscreen mode"));
    if (name == "EnterFullscreenButton")
        return String::fromUTF8(_("Exit fullscreen mode"));
    if (name == "ShowClosedCaptionsButton")
        return String::fromUTF8(_("start displaying closed captions"));
    if (name == "HideClosedCaptionsButton")
        return String::fromUTF8(_("stop displaying closed captions"));

    ASSERT_NOT_REACHED();
    return String();
}

String localizedMediaTimeDescription(float time)
{
    if (!std::isfinite(time))
        return String::fromUTF8(_("indefinite time"));

    int seconds = abs(static_cast<int>(time));
    int days = seconds / (60 * 60 * 24);
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (days) {
        GUniquePtr<gchar> string(g_strdup_printf("%d days %d hours %d minutes %d seconds", days, hours, minutes, seconds));
        return String::fromUTF8(string.get());
    }

    if (hours) {
        GUniquePtr<gchar> string(g_strdup_printf("%d hours %d minutes %d seconds", hours, minutes, seconds));
        return String::fromUTF8(string.get());
    }

    if (minutes) {
        GUniquePtr<gchar> string(g_strdup_printf("%d minutes %d seconds", minutes, seconds));
        return String::fromUTF8(string.get());
    }

    GUniquePtr<gchar> string(g_strdup_printf("%d seconds", seconds));
    return String::fromUTF8(string.get());
}
#endif  // ENABLE(VIDEO)

String validationMessageValueMissingText()
{
    return String::fromUTF8(_("Fill out this field"));
}

String validationMessageValueMissingForCheckboxText()
{
    return String::fromUTF8(_("Select this checkbox"));
}

String validationMessageValueMissingForFileText()
{
    return String::fromUTF8(_("Select a file"));
}

String validationMessageValueMissingForMultipleFileText()
{
    return validationMessageValueMissingForFileText();
}

String validationMessageValueMissingForRadioText()
{
    return String::fromUTF8(_("Select one of these options"));
}

String validationMessageValueMissingForSelectText()
{
    return String::fromUTF8(_("Select an item in the list"));
}

String validationMessageTypeMismatchText()
{
    return String::fromUTF8(_("Invalid value"));
}

String validationMessageTypeMismatchForEmailText()
{
    return String::fromUTF8(_("Enter an email address"));
}

String validationMessageTypeMismatchForMultipleEmailText()
{
    return validationMessageTypeMismatchForEmailText();
}

String validationMessageTypeMismatchForURLText()
{
    return String::fromUTF8(_("Enter a URL"));
}

String validationMessagePatternMismatchText()
{
    return String::fromUTF8(_("Match the requested format"));
}

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

String validationMessageRangeUnderflowText(const String& minimum)
{
    GUniquePtr<char> string(g_strdup_printf(_("Value must be greater than or equal to %s"), minimum.utf8().data()));
    return String::fromUTF8(string.get());
}

String validationMessageRangeOverflowText(const String& maximum)
{
    GUniquePtr<char> string(g_strdup_printf(_("Value must be less than or equal to %s"), maximum.utf8().data()));
    return String::fromUTF8(string.get());
}

String validationMessageStepMismatchText(const String&, const String&)
{
    return String::fromUTF8(_("Enter a valid value"));
}

String validationMessageBadInputForNumberText()
{
    return String::fromUTF8(_("Enter a number"));
}

String unacceptableTLSCertificate()
{
    return String::fromUTF8(_("Unacceptable TLS certificate"));
}

String localizedString(const char* key)
{
    return String::fromUTF8(key, strlen(key));
}

#if ENABLE(VIDEO_TRACK)
String textTrackSubtitlesText()
{
    return String::fromUTF8(C_("Menu section heading for subtitles", "Subtitles"));
}

String textTrackOffMenuItemText()
{
    return String::fromUTF8(C_("Menu item label for the track that represents disabling closed captions", "Off"));
}

String textTrackAutomaticMenuItemText()
{
    return String::fromUTF8(C_("Menu item label for the automatically chosen track", "Auto"));
}

String textTrackNoLabelText()
{
    return String::fromUTF8(C_("Menu item label for a closed captions track that has no other name", "No label"));
}

String audioTrackNoLabelText()
{
    return String::fromUTF8(C_("Menu item label for an audio track that has no other name", "No label"));
}
#endif

String snapshottedPlugInLabelTitle()
{
    return String::fromUTF8(C_("Snapshotted Plug-In", "Title of the label to show on a snapshotted plug-in"));
}

String snapshottedPlugInLabelSubtitle()
{
    return String::fromUTF8(C_("Click to restart", "Subtitle of the label to show on a snapshotted plug-in"));
}

}
