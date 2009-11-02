/*
 * Copyright (C) 2005, 2009 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebViewFactory.h>

#import <WebKit/WebFrameInternal.h>
#import <WebKit/WebViewInternal.h>
#import <WebKit/WebHTMLViewInternal.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebNSUserDefaultsExtras.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKitSystemInterface.h>
#import <wtf/Assertions.h>

@interface NSMenu (WebViewFactoryAdditions)
- (NSMenuItem *)addItemWithTitle:(NSString *)title action:(SEL)action tag:(int)tag;
@end

@implementation NSMenu (WebViewFactoryAdditions)

- (NSMenuItem *)addItemWithTitle:(NSString *)title action:(SEL)action tag:(int)tag
{
    NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:@""] autorelease];
    [item setTag:tag];
    [self addItem:item];
    return item;
}

@end

@implementation WebViewFactory

+ (void)createSharedFactory
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
}

- (NSArray *)pluginsInfo
{
    return [[WebPluginDatabase sharedDatabase] plugins];
}

- (void)refreshPlugins
{
    [[WebPluginDatabase sharedDatabase] refresh];
}

- (NSString *)inputElementAltText
{
    return UI_STRING_KEY("Submit", "Submit (input element)", "alt text for <input> elements with no alt, title, or value");
}

- (NSString *)resetButtonDefaultLabel
{
    return UI_STRING("Reset", "default label for Reset buttons in forms on web pages");
}

- (NSString *)searchableIndexIntroduction
{
    return UI_STRING("This is a searchable index. Enter search keywords: ",
        "text that appears at the start of nearly-obsolete web pages in the form of a 'searchable index'");
}

- (NSString *)submitButtonDefaultLabel
{
    return UI_STRING("Submit", "default label for Submit buttons in forms on web pages");
}

- (NSString *)fileButtonChooseFileLabel
{
    return UI_STRING("Choose File", "title for file button used in HTML forms");
}

- (NSString *)fileButtonNoFileSelectedLabel
{
    return UI_STRING("no file selected", "text to display in file button used in HTML forms when no file is selected");
}

- (NSString *)copyImageUnknownFileLabel
{
    return UI_STRING("unknown", "Unknown filename");
}

- (NSString *)searchMenuNoRecentSearchesText
{
    return UI_STRING("No recent searches", "Label for only item in menu that appears when clicking on the search field image, when no searches have been performed");
}

- (NSString *)searchMenuRecentSearchesText
{
    return UI_STRING("Recent Searches", "label for first item in the menu that appears when clicking on the search field image, used as embedded menu title");
}

- (NSString *)searchMenuClearRecentSearchesText
{
    return UI_STRING("Clear Recent Searches", "menu item in Recent Searches menu that empties menu's contents");
}

- (NSString *)defaultLanguageCode
{
    return [NSUserDefaults _webkit_preferredLanguageCode];
}

- (NSString *)contextMenuItemTagOpenLinkInNewWindow
{
    return UI_STRING("Open Link in New Window", "Open in New Window context menu item");
}

- (NSString *)contextMenuItemTagDownloadLinkToDisk
{
    return UI_STRING("Download Linked File", "Download Linked File context menu item");
}

- (NSString *)contextMenuItemTagCopyLinkToClipboard
{
    return UI_STRING("Copy Link", "Copy Link context menu item");
}

- (NSString *)contextMenuItemTagOpenImageInNewWindow
{
    return UI_STRING("Open Image in New Window", "Open Image in New Window context menu item");
}

- (NSString *)contextMenuItemTagDownloadImageToDisk
{
    return UI_STRING("Download Image", "Download Image context menu item");
}

- (NSString *)contextMenuItemTagCopyImageToClipboard
{
    return UI_STRING("Copy Image", "Copy Image context menu item");
}

- (NSString *)contextMenuItemTagOpenFrameInNewWindow
{
    return UI_STRING("Open Frame in New Window", "Open Frame in New Window context menu item");
}

- (NSString *)contextMenuItemTagCopy
{
    return UI_STRING("Copy", "Copy context menu item");
}

- (NSString *)contextMenuItemTagGoBack
{
    return UI_STRING("Back", "Back context menu item");
}

- (NSString *)contextMenuItemTagGoForward
{
    return UI_STRING("Forward", "Forward context menu item");
}

- (NSString *)contextMenuItemTagStop
{
    return UI_STRING("Stop", "Stop context menu item");
}

- (NSString *)contextMenuItemTagReload
{
    return UI_STRING("Reload", "Reload context menu item");
}

- (NSString *)contextMenuItemTagCut
{
    return UI_STRING("Cut", "Cut context menu item");
}

- (NSString *)contextMenuItemTagPaste
{
    return UI_STRING("Paste", "Paste context menu item");
}

- (NSString *)contextMenuItemTagNoGuessesFound
{
    return UI_STRING("No Guesses Found", "No Guesses Found context menu item");
}

- (NSString *)contextMenuItemTagIgnoreSpelling
{
    return UI_STRING("Ignore Spelling", "Ignore Spelling context menu item");
}

- (NSString *)contextMenuItemTagLearnSpelling
{
    return UI_STRING("Learn Spelling", "Learn Spelling context menu item");
}

- (NSString *)contextMenuItemTagSearchInSpotlight
{
    return UI_STRING("Search in Spotlight", "Search in Spotlight context menu item");
}

- (NSString *)contextMenuItemTagSearchWeb
{
    return UI_STRING("Search in Google", "Search in Google context menu item");
}

- (NSString *)contextMenuItemTagLookUpInDictionary
{
    return UI_STRING("Look Up in Dictionary", "Look Up in Dictionary context menu item");
}

- (NSString *)contextMenuItemTagOpenLink
{
    return UI_STRING("Open Link", "Open Link context menu item");
}

- (NSString *)contextMenuItemTagIgnoreGrammar
{
    return UI_STRING("Ignore Grammar", "Ignore Grammar context menu item");
}

- (NSString *)contextMenuItemTagSpellingMenu
{
#ifndef BUILDING_ON_TIGER
    return UI_STRING("Spelling and Grammar", "Spelling and Grammar context sub-menu item");
#else
    return UI_STRING("Spelling", "Spelling context sub-menu item");
#endif
}

- (NSString *)contextMenuItemTagShowSpellingPanel:(bool)show
{
#ifndef BUILDING_ON_TIGER
    if (show)
        return UI_STRING("Show Spelling and Grammar", "menu item title");
    return UI_STRING("Hide Spelling and Grammar", "menu item title");
#else
    return UI_STRING("Spelling...", "menu item title");
#endif
}

- (NSString *)contextMenuItemTagCheckSpelling
{
#ifndef BUILDING_ON_TIGER
    return UI_STRING("Check Document Now", "Check spelling context menu item");
#else
    return UI_STRING("Check Spelling", "Check spelling context menu item");
#endif
}

- (NSString *)contextMenuItemTagCheckSpellingWhileTyping
{
#ifndef BUILDING_ON_TIGER
    return UI_STRING("Check Spelling While Typing", "Check spelling while typing context menu item");
#else
    return UI_STRING("Check Spelling as You Type", "Check spelling while typing context menu item");
#endif
}

- (NSString *)contextMenuItemTagCheckGrammarWithSpelling
{
    return UI_STRING("Check Grammar With Spelling", "Check grammar with spelling context menu item");
}

- (NSString *)contextMenuItemTagFontMenu
{
    return UI_STRING("Font", "Font context sub-menu item");
}

- (NSString *)contextMenuItemTagShowFonts
{
    return UI_STRING("Show Fonts", "Show fonts context menu item");
}

- (NSString *)contextMenuItemTagBold
{
    return UI_STRING("Bold", "Bold context menu item");
}

- (NSString *)contextMenuItemTagItalic
{
    return UI_STRING("Italic", "Italic context menu item");
}

- (NSString *)contextMenuItemTagUnderline
{
    return UI_STRING("Underline", "Underline context menu item");
}

- (NSString *)contextMenuItemTagOutline
{
    return UI_STRING("Outline", "Outline context menu item");
}

- (NSString *)contextMenuItemTagStyles
{
    return UI_STRING("Styles...", "Styles context menu item");
}

- (NSString *)contextMenuItemTagShowColors
{
    return UI_STRING("Show Colors", "Show colors context menu item");
}

- (NSString *)contextMenuItemTagSpeechMenu
{
    return UI_STRING("Speech", "Speech context sub-menu item");
}

- (NSString *)contextMenuItemTagStartSpeaking
{
    return UI_STRING("Start Speaking", "Start speaking context menu item");
}

- (NSString *)contextMenuItemTagStopSpeaking
{
    return UI_STRING("Stop Speaking", "Stop speaking context menu item");
}

- (NSString *)contextMenuItemTagWritingDirectionMenu
{
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    return UI_STRING("Paragraph Direction", "Paragraph direction context sub-menu item");
#else
    return UI_STRING("Writing Direction", "Writing direction context sub-menu item");
#endif
}

- (NSString *)contextMenuItemTagTextDirectionMenu
{
    return UI_STRING("Selection Direction", "Selection direction context sub-menu item");
}

- (NSString *)contextMenuItemTagDefaultDirection
{
    return UI_STRING("Default", "Default writing direction context menu item");
}

- (NSString *)contextMenuItemTagLeftToRight
{
    return UI_STRING("Left to Right", "Left to Right context menu item");
}

- (NSString *)contextMenuItemTagRightToLeft
{
    return UI_STRING("Right to Left", "Right to Left context menu item");
}

- (NSString *)contextMenuItemTagCorrectSpellingAutomatically
{
    return UI_STRING("Correct Spelling Automatically", "Correct Spelling Automatically context menu item");
}

- (NSString *)contextMenuItemTagSubstitutionsMenu
{
    return UI_STRING("Substitutions", "Substitutions context sub-menu item");
}

- (NSString *)contextMenuItemTagShowSubstitutions:(bool)show
{
    if (show) 
        return UI_STRING("Show Substitutions", "menu item title");
    return UI_STRING("Hide Substitutions", "menu item title");
}

- (NSString *)contextMenuItemTagSmartCopyPaste
{
    return UI_STRING("Smart Copy/Paste", "Smart Copy/Paste context menu item");
}

- (NSString *)contextMenuItemTagSmartQuotes
{
    return UI_STRING("Smart Quotes", "Smart Quotes context menu item");
}

- (NSString *)contextMenuItemTagSmartDashes
{
    return UI_STRING("Smart Dashes", "Smart Dashes context menu item");
}

- (NSString *)contextMenuItemTagSmartLinks
{
    return UI_STRING("Smart Links", "Smart Links context menu item");
}

- (NSString *)contextMenuItemTagTextReplacement
{
    return UI_STRING("Text Replacement", "Text Replacement context menu item");
}

- (NSString *)contextMenuItemTagTransformationsMenu
{
    return UI_STRING("Transformations", "Transformations context sub-menu item");
}

- (NSString *)contextMenuItemTagMakeUpperCase
{
    return UI_STRING("Make Upper Case", "Make Upper Case context menu item");
}

- (NSString *)contextMenuItemTagMakeLowerCase
{
    return UI_STRING("Make Lower Case", "Make Lower Case context menu item");
}

- (NSString *)contextMenuItemTagCapitalize
{
    return UI_STRING("Capitalize", "Capitalize context menu item");
}

- (NSString *)contextMenuItemTagChangeBack:(NSString *)replacedString
{
    static NSString *formatString = nil;
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    static bool lookedUpString = false;
    if (!lookedUpString) {
        formatString = [[[NSBundle bundleForClass:[NSSpellChecker class]] localizedStringForKey:@"Change Back to \\U201C%@\\U201D" value:nil table:@"MenuCommands"] retain];
        lookedUpString = true;
    }
#endif
    return formatString ? [NSString stringWithFormat:formatString, replacedString] : replacedString;
}

- (NSString *)contextMenuItemTagInspectElement
{
    return UI_STRING("Inspect Element", "Inspect Element context menu item");
}

- (BOOL)objectIsTextMarker:(id)object
{
    return object != nil && CFGetTypeID(object) == WKGetAXTextMarkerTypeID();
}

- (BOOL)objectIsTextMarkerRange:(id)object
{
    return object != nil && CFGetTypeID(object) == WKGetAXTextMarkerRangeTypeID();
}

- (WebCoreTextMarker *)textMarkerWithBytes:(const void *)bytes length:(size_t)length
{
    return WebCFAutorelease(WKCreateAXTextMarker(bytes, length));
}

- (BOOL)getBytes:(void *)bytes fromTextMarker:(WebCoreTextMarker *)textMarker length:(size_t)length
{
    return WKGetBytesFromAXTextMarker(textMarker, bytes, length);
}

- (WebCoreTextMarkerRange *)textMarkerRangeWithStart:(WebCoreTextMarker *)start end:(WebCoreTextMarker *)end
{
    ASSERT(start != nil);
    ASSERT(end != nil);
    ASSERT(CFGetTypeID(start) == WKGetAXTextMarkerTypeID());
    ASSERT(CFGetTypeID(end) == WKGetAXTextMarkerTypeID());
    return WebCFAutorelease(WKCreateAXTextMarkerRange(start, end));
}

- (WebCoreTextMarker *)startOfTextMarkerRange:(WebCoreTextMarkerRange *)range
{
    ASSERT(range != nil);
    ASSERT(CFGetTypeID(range) == WKGetAXTextMarkerRangeTypeID());
    return WebCFAutorelease(WKCopyAXTextMarkerRangeStart(range));
}

- (WebCoreTextMarker *)endOfTextMarkerRange:(WebCoreTextMarkerRange *)range
{
    ASSERT(range != nil);
    ASSERT(CFGetTypeID(range) == WKGetAXTextMarkerRangeTypeID());
    return WebCFAutorelease(WKCopyAXTextMarkerRangeEnd(range));
}

- (void)accessibilityHandleFocusChanged
{
    WKAccessibilityHandleFocusChanged();
}

- (AXUIElementRef)AXUIElementForElement:(id)element
{
    return WKCreateAXUIElementRef(element);
}

- (CGRect)accessibilityConvertScreenRect:(CGRect)bounds
{
    NSArray *screens = [NSScreen screens];
    if ([screens count]) {
        CGFloat screenHeight = NSHeight([[screens objectAtIndex:0] frame]);
        bounds.origin.y = (screenHeight - (bounds.origin.y + bounds.size.height));
    } else
        bounds = CGRectZero;    

    return bounds;
}

- (void)unregisterUniqueIdForUIElement:(id)element
{
    WKUnregisterUniqueIdForElement(element);
}

- (NSString *)AXWebAreaText
{
    return UI_STRING("HTML content", "accessibility role description for web area");
}

- (NSString *)AXLinkText
{
    return UI_STRING("link", "accessibility role description for link");
}

- (NSString *)AXListMarkerText
{
    return UI_STRING("list marker", "accessibility role description for list marker");
}

- (NSString *)AXImageMapText
{
    return UI_STRING("image map", "accessibility role description for image map");
}

- (NSString *)AXHeadingText
{
    return UI_STRING("heading", "accessibility role description for headings");
}

- (NSString *)AXDefinitionListTermText
{
    return UI_STRING("term", "term word of a definition");
}

- (NSString *)AXDefinitionListDefinitionText
{
    return UI_STRING("definition", "definition phrase");
}

- (NSString *)AXARIAContentGroupText:(NSString *)ariaType
{
    if ([ariaType isEqualToString:@"ARIAApplicationLog"])
        return UI_STRING("log", "An ARIA accessibility group that acts as a console log.");
    if ([ariaType isEqualToString:@"ARIAApplicationMarquee"])
        return UI_STRING("marquee", "An ARIA accessibility group that acts as a marquee.");    
    if ([ariaType isEqualToString:@"ARIAApplicationStatus"])
        return UI_STRING("application status", "An ARIA accessibility group that acts as a status update.");    
    if ([ariaType isEqualToString:@"ARIAApplicationTimer"])
        return UI_STRING("timer", "An ARIA accessibility group that acts as an updating timer.");    
    if ([ariaType isEqualToString:@"ARIADocument"])
        return UI_STRING("document", "An ARIA accessibility group that acts as a document.");    
    if ([ariaType isEqualToString:@"ARIADocumentArticle"])
        return UI_STRING("article", "An ARIA accessibility group that acts as an article.");    
    if ([ariaType isEqualToString:@"ARIADocumentNote"])
        return UI_STRING("note", "An ARIA accessibility group that acts as a note in a document.");    
    if ([ariaType isEqualToString:@"ARIADocumentRegion"])
        return UI_STRING("region", "An ARIA accessibility group that acts as a distinct region in a document.");    
    if ([ariaType isEqualToString:@"ARIALandmarkApplication"])
        return UI_STRING("application", "An ARIA accessibility group that acts as an application.");    
    if ([ariaType isEqualToString:@"ARIALandmarkBanner"])
        return UI_STRING("banner", "An ARIA accessibility group that acts as a banner.");    
    if ([ariaType isEqualToString:@"ARIALandmarkComplementary"])
        return UI_STRING("complementary", "An ARIA accessibility group that acts as a region of complementary information.");    
    if ([ariaType isEqualToString:@"ARIALandmarkContentInfo"])
        return UI_STRING("content", "An ARIA accessibility group that contains content.");    
    if ([ariaType isEqualToString:@"ARIALandmarkMain"])
        return UI_STRING("main", "An ARIA accessibility group that is the main portion of the website.");    
    if ([ariaType isEqualToString:@"ARIALandmarkNavigation"])
        return UI_STRING("navigation", "An ARIA accessibility group that contains the main navigation elements of a website.");    
    if ([ariaType isEqualToString:@"ARIALandmarkSearch"])
        return UI_STRING("search", "An ARIA accessibility group that contains a search feature of a website.");    
    if ([ariaType isEqualToString:@"ARIAUserInterfaceTooltip"])
        return UI_STRING("tooltip", "An ARIA accessibility group that acts as a tooltip.");    
    if ([ariaType isEqualToString:@"ARIATabPanel"])
        return UI_STRING("tab panel", "An ARIA accessibility group that contenst the content of a tab.");
    return nil;
}

- (NSString *)AXButtonActionVerb
{
    return UI_STRING("press", "Verb stating the action that will occur when a button is pressed, as used by accessibility");
}

- (NSString *)AXRadioButtonActionVerb
{
    return UI_STRING("select", "Verb stating the action that will occur when a radio button is clicked, as used by accessibility");
}

- (NSString *)AXTextFieldActionVerb
{
    return UI_STRING("activate", "Verb stating the action that will occur when a text field is selected, as used by accessibility");
}

- (NSString *)AXCheckedCheckBoxActionVerb
{
    return UI_STRING("uncheck", "Verb stating the action that will occur when a checked checkbox is clicked, as used by accessibility");
}

- (NSString *)AXUncheckedCheckBoxActionVerb
{
    return UI_STRING("check", "Verb stating the action that will occur when an unchecked checkbox is clicked, as used by accessibility");
}

- (NSString *)AXLinkActionVerb
{
    return UI_STRING("jump", "Verb stating the action that will occur when a link is clicked, as used by accessibility");
}

- (NSString *)multipleFileUploadTextForNumberOfFiles:(unsigned)numberOfFiles
{
    return [NSString stringWithFormat:UI_STRING("%d files", "Label to describe the number of files selected in a file upload control that allows multiple files"), numberOfFiles];
}

- (NSString *)unknownFileSizeText
{
    return UI_STRING("Unknown", "Unknown filesize FTP directory listing item");
}

- (NSString*)imageTitleForFilename:(NSString*)filename width:(int)width height:(int)height
{
    return [NSString stringWithFormat:UI_STRING("%@ %d×%d pixels", "window title for a standalone image (uses multiplication symbol, not x)"), filename, width, height];
}

- (NSString*)mediaElementLoadingStateText
{
    return UI_STRING("Loading...", "Media controller status message when the media is loading");
}

- (NSString*)mediaElementLiveBroadcastStateText
{
    return UI_STRING("Live Broadcast", "Media controller status message when watching a live broadcast");
}

- (NSString*)localizedMediaControlElementString:(NSString*)name
{
    if ([name isEqualToString:@"AudioElement"])
        return UI_STRING("audio element controller", "accessibility role description for audio element controller");
    if ([name isEqualToString:@"VideoElement"])
        return UI_STRING("video element controller", "accessibility role description for video element controller");

    // FIXME: the ControlsPanel container should never be visible in the accessibility hierarchy.
    if ([name isEqualToString:@"ControlsPanel"])
        return @"";

    if ([name isEqualToString:@"MuteButton"])
        return UI_STRING("mute", "accessibility role description for mute button");
    if ([name isEqualToString:@"UnMuteButton"])
        return UI_STRING("unmute", "accessibility role description for turn mute off button");
    if ([name isEqualToString:@"PlayButton"])
        return UI_STRING("play", "accessibility role description for play button");
    if ([name isEqualToString:@"PauseButton"])
        return UI_STRING("pause", "accessibility role description for pause button");
    if ([name isEqualToString:@"Slider"])
        return UI_STRING("movie time", "accessibility role description for timeline slider");
    if ([name isEqualToString:@"SliderThumb"])
        return UI_STRING("timeline slider thumb", "accessibility role description for timeline thumb");
    if ([name isEqualToString:@"RewindButton"])
        return UI_STRING("back 30 seconds", "accessibility role description for seek back 30 seconds button");
    if ([name isEqualToString:@"ReturnToRealtimeButton"])
        return UI_STRING("return to realtime", "accessibility role description for return to real time button");
    if ([name isEqualToString:@"CurrentTimeDisplay"])
        return UI_STRING("elapsed time", "accessibility role description for elapsed time display");
    if ([name isEqualToString:@"TimeRemainingDisplay"])
        return UI_STRING("remaining time", "accessibility role description for time remaining display");
    if ([name isEqualToString:@"StatusDisplay"])
        return UI_STRING("status", "accessibility role description for movie status");
    if ([name isEqualToString:@"FullscreenButton"])
        return UI_STRING("fullscreen", "accessibility role description for enter fullscreen button");
    if ([name isEqualToString:@"SeekForwardButton"])
        return UI_STRING("fast forward", "accessibility role description for fast forward button");
    if ([name isEqualToString:@"SeekBackButton"])
        return UI_STRING("fast reverse", "accessibility role description for fast reverse button");
    ASSERT_NOT_REACHED();
    return @"";
}

- (NSString*)localizedMediaControlElementHelpText:(NSString*)name
{
    if ([name isEqualToString:@"AudioElement"])
        return UI_STRING("audio element playback controls and status display", "accessibility role description for audio element controller");
    if ([name isEqualToString:@"VideoElement"])
        return UI_STRING("video element playback controls and status display", "accessibility role description for video element controller");

    if ([name isEqualToString:@"MuteButton"])
        return UI_STRING("mute audio tracks", "accessibility help text for mute button");
    if ([name isEqualToString:@"UnMuteButton"])
        return UI_STRING("unmute audio tracks", "accessibility help text for un mute button");
    if ([name isEqualToString:@"PlayButton"])
        return UI_STRING("begin playback", "accessibility help text for play button");
    if ([name isEqualToString:@"PauseButton"])
        return UI_STRING("pause playback", "accessibility help text for pause button");
    if ([name isEqualToString:@"Slider"])
        return UI_STRING("movie time scrubber", "accessibility help text for timeline slider");
    if ([name isEqualToString:@"SliderThumb"])
        return UI_STRING("movie time scrubber thumb", "accessibility help text for timeline slider thumb");
    if ([name isEqualToString:@"RewindButton"])
        return UI_STRING("seek movie back 30 seconds", "accessibility help text for jump back 30 seconds button");
    if ([name isEqualToString:@"ReturnToRealtimeButton"])
        return UI_STRING("return streaming movie to real time", "accessibility help text for return streaming movie to real time button");
    if ([name isEqualToString:@"CurrentTimeDisplay"])
        return UI_STRING("current movie time in seconds", "accessibility help text for elapsed time display");
    if ([name isEqualToString:@"TimeRemainingDisplay"])
        return UI_STRING("number of seconds of movie remaining", "accessibility help text for remaining time display");
    if ([name isEqualToString:@"StatusDisplay"])
        return UI_STRING("current movie status", "accessibility help text for movie status display");
    if ([name isEqualToString:@"SeekBackButton"])
        return UI_STRING("seek quickly back", "accessibility help text for fast rewind button");
    if ([name isEqualToString:@"SeekForwardButton"])
        return UI_STRING("seek quickly forward", "accessibility help text for fast forward button");
    if ([name isEqualToString:@"FullscreenButton"])
        return UI_STRING("Play movie in fullscreen mode", "accessibility help text for enter fullscreen button");
    ASSERT_NOT_REACHED();
    return @"";
}

- (NSString*)localizedMediaTimeDescription:(float)time
{
    if (!isfinite(time))
        return UI_STRING("indefinite time", "accessibility help text for an indefinite media controller time value");

    int seconds = (int)fabsf(time); 
    int days = seconds / (60 * 60 * 24);
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (days)
        return [NSString stringWithFormat:UI_STRING("%1$d days %2$d hours %3$d minutes %4$d seconds", "accessibility help text for media controller time value >= 1 day"), days, hours, minutes, seconds];
    else if (hours)
        return [NSString stringWithFormat:UI_STRING("%1$d hours %2$d minutes %3$d seconds", "accessibility help text for media controller time value >= 60 minutes"), hours, minutes, seconds];
    else if (minutes)
        return [NSString stringWithFormat:UI_STRING("%1$d minutes %2$d seconds", "accessibility help text for media controller time value >= 60 seconds"), minutes, seconds];

    return [NSString stringWithFormat:UI_STRING("%1$d seconds", "accessibility help text for media controller time value < 60 seconds"), seconds];
}

@end
