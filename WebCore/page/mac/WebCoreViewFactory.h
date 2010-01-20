/*
 * Copyright (C) 2003, 2005, 2009 Apple Computer, Inc.  All rights reserved.
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

@class WebCoreTextMarker;
@class WebCoreTextMarkerRange;

@protocol WebCoreViewFactory

- (NSArray *)pluginsInfo; // array of id <WebCorePluginInfo>
- (void)refreshPlugins;

- (NSString *)inputElementAltText;
- (NSString *)resetButtonDefaultLabel;
- (NSString *)searchableIndexIntroduction;
- (NSString *)submitButtonDefaultLabel;
- (NSString *)fileButtonChooseFileLabel;
- (NSString *)fileButtonNoFileSelectedLabel;
- (NSString *)copyImageUnknownFileLabel;

#if ENABLE(CONTEXT_MENUS)
// Context menu item titles
- (NSString *)contextMenuItemTagOpenLinkInNewWindow;
- (NSString *)contextMenuItemTagDownloadLinkToDisk;
- (NSString *)contextMenuItemTagCopyLinkToClipboard;
- (NSString *)contextMenuItemTagOpenImageInNewWindow;
- (NSString *)contextMenuItemTagDownloadImageToDisk;
- (NSString *)contextMenuItemTagCopyImageToClipboard;
- (NSString *)contextMenuItemTagOpenFrameInNewWindow;
- (NSString *)contextMenuItemTagCopy;
- (NSString *)contextMenuItemTagGoBack;
- (NSString *)contextMenuItemTagGoForward;
- (NSString *)contextMenuItemTagStop;
- (NSString *)contextMenuItemTagReload;
- (NSString *)contextMenuItemTagCut;
- (NSString *)contextMenuItemTagPaste;
- (NSString *)contextMenuItemTagNoGuessesFound;
- (NSString *)contextMenuItemTagIgnoreSpelling;
- (NSString *)contextMenuItemTagLearnSpelling;
- (NSString *)contextMenuItemTagSearchInSpotlight;
- (NSString *)contextMenuItemTagSearchWeb;
- (NSString *)contextMenuItemTagLookUpInDictionary;
- (NSString *)contextMenuItemTagOpenLink;
- (NSString *)contextMenuItemTagIgnoreGrammar;
- (NSString *)contextMenuItemTagSpellingMenu;
- (NSString *)contextMenuItemTagShowSpellingPanel:(bool)show;
- (NSString *)contextMenuItemTagCheckSpelling;
- (NSString *)contextMenuItemTagCheckSpellingWhileTyping;
- (NSString *)contextMenuItemTagCheckGrammarWithSpelling;
- (NSString *)contextMenuItemTagFontMenu;
- (NSString *)contextMenuItemTagShowFonts;
- (NSString *)contextMenuItemTagBold;
- (NSString *)contextMenuItemTagItalic;
- (NSString *)contextMenuItemTagUnderline;
- (NSString *)contextMenuItemTagOutline;
- (NSString *)contextMenuItemTagStyles;
- (NSString *)contextMenuItemTagShowColors;
- (NSString *)contextMenuItemTagSpeechMenu;
- (NSString *)contextMenuItemTagStartSpeaking;
- (NSString *)contextMenuItemTagStopSpeaking;
- (NSString *)contextMenuItemTagWritingDirectionMenu;
- (NSString *)contextMenuItemTagTextDirectionMenu;
- (NSString *)contextMenuItemTagDefaultDirection;
- (NSString *)contextMenuItemTagLeftToRight;
- (NSString *)contextMenuItemTagRightToLeft;
- (NSString *)contextMenuItemTagCorrectSpellingAutomatically;
- (NSString *)contextMenuItemTagSubstitutionsMenu;
- (NSString *)contextMenuItemTagShowSubstitutions:(bool)show;
- (NSString *)contextMenuItemTagSmartCopyPaste;
- (NSString *)contextMenuItemTagSmartQuotes;
- (NSString *)contextMenuItemTagSmartDashes;
- (NSString *)contextMenuItemTagSmartLinks;
- (NSString *)contextMenuItemTagTextReplacement;
- (NSString *)contextMenuItemTagTransformationsMenu;
- (NSString *)contextMenuItemTagMakeUpperCase;
- (NSString *)contextMenuItemTagMakeLowerCase;
- (NSString *)contextMenuItemTagCapitalize;
- (NSString *)contextMenuItemTagChangeBack:(NSString *)replacedString;
- (NSString *)contextMenuItemTagInspectElement;
#endif // ENABLE(CONTEXT_MENUS)

- (NSString *)searchMenuNoRecentSearchesText;
- (NSString *)searchMenuRecentSearchesText;
- (NSString *)searchMenuClearRecentSearchesText;

- (NSString *)defaultLanguageCode;

- (NSString *)imageTitleForFilename:(NSString *)filename width:(int)width height:(int)height;

- (BOOL)objectIsTextMarker:(id)object;
- (BOOL)objectIsTextMarkerRange:(id)object;

- (WebCoreTextMarker *)textMarkerWithBytes:(const void *)bytes length:(size_t)length;
- (BOOL)getBytes:(void *)bytes fromTextMarker:(WebCoreTextMarker *)textMarker length:(size_t)length;

- (WebCoreTextMarkerRange *)textMarkerRangeWithStart:(WebCoreTextMarker *)start end:(WebCoreTextMarker *)end;
- (WebCoreTextMarker *)startOfTextMarkerRange:(WebCoreTextMarkerRange *)range;
- (WebCoreTextMarker *)endOfTextMarkerRange:(WebCoreTextMarkerRange *)range;

- (void)accessibilityHandleFocusChanged;
- (CGRect)accessibilityConvertScreenRect:(CGRect)bounds;

- (AXUIElementRef)AXUIElementForElement:(id)element;
- (void)unregisterUniqueIdForUIElement:(id)element;

- (NSString *)AXWebAreaText;
- (NSString *)AXLinkText;
- (NSString *)AXListMarkerText;
- (NSString *)AXImageMapText;
- (NSString *)AXHeadingText;
- (NSString *)AXDefinitionListTermText;
- (NSString *)AXDefinitionListDefinitionText;
- (NSString *)AXARIAContentGroupText:(NSString *)ariaType;

- (NSString *)AXButtonActionVerb;
- (NSString *)AXRadioButtonActionVerb;
- (NSString *)AXTextFieldActionVerb;
- (NSString *)AXCheckedCheckBoxActionVerb;
- (NSString *)AXUncheckedCheckBoxActionVerb;
- (NSString *)AXLinkActionVerb;
- (NSString *)AXMenuListPopupActionVerb;
- (NSString *)AXMenuListActionVerb;

- (NSString *)multipleFileUploadTextForNumberOfFiles:(unsigned)numberOfFiles;
// FTP Directory Related
- (NSString *)unknownFileSizeText;

- (NSString *)mediaElementLoadingStateText;
- (NSString *)mediaElementLiveBroadcastStateText;
- (NSString*)localizedMediaControlElementString:(NSString*)name;
- (NSString*)localizedMediaControlElementHelpText:(NSString*)name;
- (NSString*)localizedMediaTimeDescription:(float)time;

- (NSString *)validationMessageValueMissingText;
- (NSString *)validationMessageTypeMismatchText;
- (NSString *)validationMessagePatternMismatchText;
- (NSString *)validationMessageTooLongText;
- (NSString *)validationMessageRangeUnderflowText;
- (NSString *)validationMessageRangeOverflowText;
- (NSString *)validationMessageStepMismatchText;

@end

@interface WebCoreViewFactory : NSObject
+ (WebCoreViewFactory *)sharedFactory;
@end

@interface WebCoreViewFactory (SubclassResponsibility) <WebCoreViewFactory>
@end

@protocol WebCorePluginInfo <NSObject>
- (NSString *)name;
- (NSString *)filename;
- (NSString *)pluginDescription;
- (NSEnumerator *)MIMETypeEnumerator;
- (NSString *)descriptionForMIMEType:(NSString *)MIMEType;
- (NSArray *)extensionsForMIMEType:(NSString *)MIMEType;
@end

