/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Forward.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(COCOA)
#include <CoreFoundation/CFString.h>
#include <wtf/RetainPtr.h>
#endif

#ifdef __OBJC__
#include <wtf/Platform.h>
#include <wtf/cocoa/TypeCastsCocoa.h>
#endif

#if USE(GLIB) && defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#endif

namespace WebCore {

    class IntSize;
    class DateComponents;

#if ENABLE(MEDIA_SESSION) && ENABLE(MEDIA_SESSION_CALL_TO_ACTION)
    enum class CallToActionLabel : uint8_t;
    WEBCORE_EXPORT RefPtr<StringImpl> callToActionLabelTitle(CallToActionLabel);
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT RefPtr<StringImpl> truncatedStringForMenuItem(StringImpl*);
#endif

    RefPtr<StringImpl> inputElementAltText();
    RefPtr<StringImpl> resetButtonDefaultLabel();
    RefPtr<StringImpl> searchableIndexIntroduction();
    RefPtr<StringImpl> submitButtonDefaultLabel();
    WEBCORE_EXPORT RefPtr<StringImpl> fileButtonChooseFileLabel();
    WEBCORE_EXPORT RefPtr<StringImpl> fileButtonChooseMultipleFilesLabel();
    RefPtr<StringImpl> fileButtonNoFileSelectedLabel();
    RefPtr<StringImpl> fileButtonNoFilesSelectedLabel();
    RefPtr<StringImpl> defaultDetailsSummaryText();

#if PLATFORM(COCOA)
    RefPtr<StringImpl> copyImageUnknownFileLabel();
#endif
#if ENABLE(APP_HIGHLIGHTS)
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagAddHighlightToCurrentQuickNote();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagAddHighlightToNewQuickNote();
#endif
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagCopyLinkWithHighlight();

#if ENABLE(CONTEXT_MENUS)
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagOpenLinkInNewWindow();
    RefPtr<StringImpl> contextMenuItemTagDownloadLinkToDisk();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagCopyLinkToClipboard();
    RefPtr<StringImpl> contextMenuItemTagOpenImageInNewWindow();
    RefPtr<StringImpl> contextMenuItemTagDownloadImageToDisk();
    RefPtr<StringImpl> contextMenuItemTagCopyImageToClipboard();
#if PLATFORM(GTK)
    RefPtr<StringImpl> contextMenuItemTagCopyImageURLToClipboard();
#endif
    RefPtr<StringImpl> contextMenuItemTagOpenFrameInNewWindow();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagCopy();
    RefPtr<StringImpl> contextMenuItemTagGoBack();
    RefPtr<StringImpl> contextMenuItemTagGoForward();
    RefPtr<StringImpl> contextMenuItemTagStop();
    RefPtr<StringImpl> contextMenuItemTagReload();
    RefPtr<StringImpl> contextMenuItemTagCut();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagPaste();
#if PLATFORM(GTK)
    RefPtr<StringImpl> contextMenuItemTagPasteAsPlainText();
    RefPtr<StringImpl> contextMenuItemTagDelete();
    RefPtr<StringImpl> contextMenuItemTagInputMethods();
    RefPtr<StringImpl> contextMenuItemTagUnicode();
    RefPtr<StringImpl> contextMenuItemTagUnicodeInsertLRMMark();
    RefPtr<StringImpl> contextMenuItemTagUnicodeInsertRLMMark();
    RefPtr<StringImpl> contextMenuItemTagUnicodeInsertLREMark();
    RefPtr<StringImpl> contextMenuItemTagUnicodeInsertRLEMark();
    RefPtr<StringImpl> contextMenuItemTagUnicodeInsertLROMark();
    RefPtr<StringImpl> contextMenuItemTagUnicodeInsertRLOMark();
    RefPtr<StringImpl> contextMenuItemTagUnicodeInsertPDFMark();
    RefPtr<StringImpl> contextMenuItemTagUnicodeInsertZWSMark();
    RefPtr<StringImpl> contextMenuItemTagUnicodeInsertZWJMark();
    RefPtr<StringImpl> contextMenuItemTagUnicodeInsertZWNJMark();
    RefPtr<StringImpl> contextMenuItemTagSelectAll();
    RefPtr<StringImpl> contextMenuItemTagInsertEmoji();
#endif
    RefPtr<StringImpl> contextMenuItemTagNoGuessesFound();
    RefPtr<StringImpl> contextMenuItemTagIgnoreSpelling();
    RefPtr<StringImpl> contextMenuItemTagLearnSpelling();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagSearchWeb();
#if PLATFORM(COCOA)
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagLookUpInDictionary(StringImpl* selectedString);
#endif
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagOpenLink();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagIgnoreGrammar();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagSpellingMenu();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagShowSpellingPanel(bool show);
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagCheckSpelling();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagCheckSpellingWhileTyping();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagCheckGrammarWithSpelling();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagFontMenu();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagBold();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagItalic();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagUnderline();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagOutline();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagWritingDirectionMenu();
    RefPtr<StringImpl> contextMenuItemTagTextDirectionMenu();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagDefaultDirection();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagLeftToRight();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagRightToLeft();
#if PLATFORM(COCOA)
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagShowFonts();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagStyles();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagShowColors();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagSpeechMenu();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagStartSpeaking();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagStopSpeaking();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagCorrectSpellingAutomatically();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagSubstitutionsMenu();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagShowSubstitutions(bool show);
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagSmartCopyPaste();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagSmartQuotes();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagSmartDashes();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagSmartLinks();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagSmartLists();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagTextReplacement();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagTransformationsMenu();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagMakeUpperCase();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagMakeLowerCase();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagCapitalize();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagConvertToTraditionalChinese();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagConvertToSimplifiedChinese();
    NODELETE RefPtr<StringImpl> contextMenuItemTagChangeBack(StringImpl* replacedString);
#endif
    RefPtr<StringImpl> contextMenuItemTagOpenVideoInNewWindow();
    RefPtr<StringImpl> contextMenuItemTagOpenAudioInNewWindow();
    RefPtr<StringImpl> contextMenuItemTagDownloadVideoToDisk();
    RefPtr<StringImpl> contextMenuItemTagDownloadAudioToDisk();
    RefPtr<StringImpl> contextMenuItemTagCopyVideoLinkToClipboard();
    RefPtr<StringImpl> contextMenuItemTagCopyAudioLinkToClipboard();
    RefPtr<StringImpl> contextMenuItemTagToggleMediaControls();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagShowMediaControls();
    RefPtr<StringImpl> contextMenuItemTagHideMediaControls();
    RefPtr<StringImpl> contextMenuItemTagToggleMediaLoop();
    RefPtr<StringImpl> contextMenuItemTagEnterVideoFullscreen();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagExitVideoFullscreen();
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    RefPtr<StringImpl> contextMenuItemTagEnterPictureInPicture();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagExitPictureInPicture();
    RefPtr<StringImpl> contextMenuItemTagEnterVideoViewer();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagExitVideoViewer();
#endif
    RefPtr<StringImpl> contextMenuItemTagMediaPlay();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagMediaPause();
    RefPtr<StringImpl> contextMenuItemTagMediaMute();
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    RefPtr<StringImpl> contextMenuItemTagPlayAllAnimations();
    RefPtr<StringImpl> contextMenuItemTagPauseAllAnimations();
    RefPtr<StringImpl> contextMenuItemTagPlayAnimation();
    RefPtr<StringImpl> contextMenuItemTagPauseAnimation();
#endif
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagInspectElement();
#if HAVE(TRANSLATION_UI_SERVICES)
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagTranslate(StringImpl* selectedString);
#endif
#if ENABLE(WRITING_TOOLS)
    RefPtr<StringImpl> contextMenuItemTagWritingTools();
    RefPtr<StringImpl> contextMenuItemTagProofread();
    RefPtr<StringImpl> contextMenuItemTagRewrite();
    RefPtr<StringImpl> contextMenuItemTagSummarize();
#endif
#if ENABLE(UNIFIED_PDF)
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFOpenWithDefaultViewer(StringImpl* appName);
#endif
#if ENABLE(PDFJS) || ENABLE(UNIFIED_PDF)
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFSinglePageContinuous();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFTwoPagesContinuous();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFZoomIn();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFZoomOut();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFActualSize();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFNextPage();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFPreviousPage();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFAutoSize();
#endif
#endif // ENABLE(CONTEXT_MENU)

#if ENABLE(PDFJS) || ENABLE(UNIFIED_PDF)
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFSinglePage();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemPDFTwoPages();
#endif

    WEBCORE_EXPORT RefPtr<StringImpl> pdfDocumentTypeDescription();

#if !PLATFORM(IOS_FAMILY)
    RefPtr<StringImpl> searchMenuNoRecentSearchesText();
    RefPtr<StringImpl> searchMenuRecentSearchesText();
    RefPtr<StringImpl> searchMenuClearRecentSearchesText();
#endif

#if ENABLE(MEDIA_STREAM)
    RefPtr<StringImpl> defaultSystemSpeakerLabel();
#endif

    RefPtr<StringImpl> AXWebAreaText();
    RefPtr<StringImpl> AXLinkText();
    RefPtr<StringImpl> AXListMarkerText();
    RefPtr<StringImpl> AXImageMapText();
    RefPtr<StringImpl> AXHeadingText();
    RefPtr<StringImpl> AXColorWellText();
    RefPtr<StringImpl> AXDefinitionText();
    RefPtr<StringImpl> AXDescriptionListText();
    RefPtr<StringImpl> AXDescriptionListTermText();
    RefPtr<StringImpl> AXDescriptionListDetailText();
    RefPtr<StringImpl> AXHeaderRoleDescriptionText();
    RefPtr<StringImpl> AXFooterRoleDescriptionText();
    RefPtr<StringImpl> AXSuggestionRoleDescriptionText();
    RefPtr<StringImpl> AXFileUploadButtonText();
    RefPtr<StringImpl> AXOutputText();
    RefPtr<StringImpl> AXSearchFieldCancelButtonText();
    RefPtr<StringImpl> AXAttachmentRoleText();
    RefPtr<StringImpl> AXDetailsText();
    RefPtr<StringImpl> AXSummaryText();
    RefPtr<StringImpl> AXFeedText();
    RefPtr<StringImpl> AXFigureText();
    RefPtr<StringImpl> AXEmailFieldText();
    RefPtr<StringImpl> AXTelephoneFieldText();
    RefPtr<StringImpl> AXURLFieldText();
    RefPtr<StringImpl> AXDateFieldText();
    RefPtr<StringImpl> AXTimeFieldText();
    RefPtr<StringImpl> AXDateFieldMonthText();
    RefPtr<StringImpl> AXDateFieldDayText();
    RefPtr<StringImpl> AXDateFieldYearText();
    RefPtr<StringImpl> AXTimeFieldHourText();
    RefPtr<StringImpl> AXTimeFieldMinuteText();
    RefPtr<StringImpl> AXTimeFieldSecondText();
    RefPtr<StringImpl> AXTimeFieldMillisecondText();
    RefPtr<StringImpl> AXDateTimeFieldText();
    RefPtr<StringImpl> AXMonthFieldText();
    RefPtr<StringImpl> AXNumberFieldText();
    RefPtr<StringImpl> AXWeekFieldText();
    RefPtr<StringImpl> AXARIAContentGroupText(StringView ariaType);
    RefPtr<StringImpl> AXHorizontalRuleDescriptionText();
    RefPtr<StringImpl> AXMarkText();

    RefPtr<StringImpl> AXButtonActionVerb();
    RefPtr<StringImpl> AXRadioButtonActionVerb();
    RefPtr<StringImpl> AXTextFieldActionVerb();
    RefPtr<StringImpl> AXCheckedCheckboxActionVerb();
    RefPtr<StringImpl> AXUncheckedCheckboxActionVerb();
    RefPtr<StringImpl> AXMenuListActionVerb();
    RefPtr<StringImpl> AXMenuListPopupActionVerb();
    RefPtr<StringImpl> AXLinkActionVerb();
    RefPtr<StringImpl> AXListItemActionVerb();
    RefPtr<StringImpl> AXShowWritingToolsLabel();

#if PLATFORM(COCOA)
    RefPtr<StringImpl> AXMeterGaugeRegionOptimumText();
    RefPtr<StringImpl> AXMeterGaugeRegionSuboptimalText();
    RefPtr<StringImpl> AXMeterGaugeRegionLessGoodText();
#endif
#if ENABLE(APPLE_PAY)
    RefPtr<StringImpl> AXApplePayPlainLabel();
    RefPtr<StringImpl> AXApplePayBuyLabel();
    RefPtr<StringImpl> AXApplePaySetupLabel();
    RefPtr<StringImpl> AXApplePayDonateLabel();
    RefPtr<StringImpl> AXApplePayCheckOutLabel();
    RefPtr<StringImpl> AXApplePayBookLabel();
    RefPtr<StringImpl> AXApplePaySubscribeLabel();
#if ENABLE(APPLE_PAY_NEW_BUTTON_TYPES)
    RefPtr<StringImpl> AXApplePayReloadLabel();
    RefPtr<StringImpl> AXApplePayAddMoneyLabel();
    RefPtr<StringImpl> AXApplePayTopUpLabel();
    RefPtr<StringImpl> AXApplePayOrderLabel();
    RefPtr<StringImpl> AXApplePayRentLabel();
    RefPtr<StringImpl> AXApplePaySupportLabel();
    RefPtr<StringImpl> AXApplePayContributeLabel();
    RefPtr<StringImpl> AXApplePayTipLabel();
#endif
#endif

    RefPtr<StringImpl> AXAutoFillCredentialsLabel();
    RefPtr<StringImpl> AXAutoFillContactsLabel();
    RefPtr<StringImpl> AXAutoFillStrongPasswordLabel();
    RefPtr<StringImpl> AXAutoFillCreditCardLabel();
    RefPtr<StringImpl> AXAutoFillLoadingLabel();
    RefPtr<StringImpl> autoFillStrongPasswordLabel();

    RefPtr<StringImpl> AXProcessingPage(double);

    RefPtr<StringImpl> missingPluginText();
    RefPtr<StringImpl> crashedPluginText();
    RefPtr<StringImpl> blockedPluginByContentSecurityPolicyText();
    RefPtr<StringImpl> insecurePluginVersionText();
    RefPtr<StringImpl> unsupportedPluginText();
    WEBCORE_EXPORT RefPtr<StringImpl> pluginTooSmallText();

    WEBCORE_EXPORT RefPtr<StringImpl> multipleFileUploadText(unsigned numberOfFiles);
    RefPtr<StringImpl> unknownFileSizeText();

    RefPtr<StringImpl> htmlSelectMultipleItems(size_t num);

#if PLATFORM(IOS_FAMILY)
    RefPtr<StringImpl> fileButtonChooseMediaFileLabel();
    RefPtr<StringImpl> fileButtonChooseMultipleMediaFilesLabel();
    RefPtr<StringImpl> fileButtonNoMediaFileSelectedLabel();
    RefPtr<StringImpl> fileButtonNoMediaFilesSelectedLabel();

    WEBCORE_EXPORT RefPtr<StringImpl> formControlDoneButtonTitle();
#endif

    RefPtr<StringImpl> imageTitle(StringImpl* filename, const IntSize& size);

    RefPtr<StringImpl> mediaElementLoadingStateText();
    RefPtr<StringImpl> mediaElementLiveBroadcastStateText();
    RefPtr<StringImpl> localizedMediaControlElementString(StringImpl*);
    RefPtr<StringImpl> localizedMediaControlElementHelpText(StringImpl*);
    RefPtr<StringImpl> localizedMediaTimeDescription(float);

    RefPtr<StringImpl> validationMessageValueMissingText();
    RefPtr<StringImpl> validationMessageValueMissingForCheckboxText();
    RefPtr<StringImpl> validationMessageValueMissingForFileText();
    RefPtr<StringImpl> validationMessageValueMissingForMultipleFileText();
    RefPtr<StringImpl> validationMessageValueMissingForRadioText();
    RefPtr<StringImpl> validationMessageValueMissingForSelectText();
    RefPtr<StringImpl> validationMessageValueMissingForSwitchText();
    RefPtr<StringImpl> validationMessageTypeMismatchText();
    RefPtr<StringImpl> validationMessageTypeMismatchForEmailText();
    RefPtr<StringImpl> validationMessageTypeMismatchForMultipleEmailText();
    RefPtr<StringImpl> validationMessageTypeMismatchForURLText();
    RefPtr<StringImpl> validationMessagePatternMismatchText();
    RefPtr<StringImpl> validationMessagePatternMismatchText(StringImpl* title);
    RefPtr<StringImpl> validationMessageTooShortText(int valueLength, int minLength);
    RefPtr<StringImpl> validationMessageTooLongText(int valueLength, int maxLength);
    RefPtr<StringImpl> validationMessageRangeUnderflowText(StringImpl* minimum);
    RefPtr<StringImpl> validationMessageRangeOverflowText(StringImpl* maximum);
    RefPtr<StringImpl> validationMessageStepMismatchText(StringImpl* base, StringImpl* step);
    RefPtr<StringImpl> validationMessageBadInputForNumberText();
#if USE(SOUP)
    RefPtr<StringImpl> unacceptableTLSCertificate();
#endif

    RefPtr<StringImpl> clickToExitFullScreenText();

#if ENABLE(VIDEO)
    RefPtr<StringImpl> trackNoLabelText();
    RefPtr<StringImpl> textTrackOffMenuItemText();
    RefPtr<StringImpl> textTrackOnMenuItemText();
    RefPtr<StringImpl> textTrackAutomaticMenuItemText();
    RefPtr<StringImpl> captionStylePreview();
    RefPtr<StringImpl> captionStylePreviewWithProfileName(StringImpl*);
#if PLATFORM(COCOA)
    RefPtr<StringImpl> addTrackLabelAsSuffix(StringImpl*, StringImpl*);
    RefPtr<StringImpl> textTrackKindClosedCaptionsDisplayName();
    RefPtr<StringImpl> addTextTrackKindClosedCaptionsSuffix(StringImpl*);
    RefPtr<StringImpl> textTrackKindCaptionsDisplayName();
    RefPtr<StringImpl> addTextTrackKindCaptionsSuffix(StringImpl*);
    RefPtr<StringImpl> textTrackKindDescriptionsDisplayName();
    RefPtr<StringImpl> addTextTrackKindDescriptionsSuffix(StringImpl*);
    RefPtr<StringImpl> textTrackKindChaptersDisplayName();
    RefPtr<StringImpl> addTextTrackKindChaptersSuffix(StringImpl*);
    RefPtr<StringImpl> textTrackKindMetadataDisplayName();
    RefPtr<StringImpl> addTextTrackKindMetadataSuffix(StringImpl*);
    RefPtr<StringImpl> textTrackKindSDHDisplayName();
    RefPtr<StringImpl> addTextTrackKindSDHSuffix(StringImpl*);
    RefPtr<StringImpl> textTrackKindEasyReaderDisplayName();
    RefPtr<StringImpl> addTextTrackKindEasyReaderSuffix(StringImpl*);
    RefPtr<StringImpl> textTrackKindForcedDisplayName();
    RefPtr<StringImpl> addTextTrackKindForcedSuffix(StringImpl*);
    RefPtr<StringImpl> textTrackKindGeneratedDisplayName();
    RefPtr<StringImpl> addTextTrackKindGeneratedSuffix(StringImpl*);
    RefPtr<StringImpl> audioTrackKindDescriptionsDisplayName();
    RefPtr<StringImpl> addAudioTrackKindDescriptionsSuffix(StringImpl*);
    RefPtr<StringImpl> audioTrackKindCommentaryDisplayName();
    RefPtr<StringImpl> addAudioTrackKindCommentarySuffix(StringImpl*);
#endif // PLATFORM(COCOA)
    RefPtr<StringImpl> contextMenuItemTagShowMediaStats();
#endif // ENABLE(VIDEO)

#if PLATFORM(COCOA)
    RefPtr<StringImpl> webCryptoMasterKeyKeychainLabel(StringImpl* localizedApplicationName);
    RefPtr<StringImpl> webCryptoMasterKeyKeychainComment();
#endif

#if PLATFORM(MAC)
    WEBCORE_EXPORT RefPtr<StringImpl> insertListTypeNone();
    WEBCORE_EXPORT RefPtr<StringImpl> insertListTypeBulleted();
    WEBCORE_EXPORT RefPtr<StringImpl> insertListTypeBulletedAccessibilityTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> insertListTypeNumbered();
    WEBCORE_EXPORT RefPtr<StringImpl> insertListTypeNumberedAccessibilityTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> exitFullScreenButtonAccessibilityTitle();
#endif

#if PLATFORM(WATCHOS)
    WEBCORE_EXPORT RefPtr<StringImpl> numberPadOKButtonTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> formControlCancelButtonTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> formControlHideButtonTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> formControlGoButtonTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> formControlSearchButtonTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> datePickerSetButtonTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> datePickerDayLabelTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> datePickerMonthLabelTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> datePickerYearLabelTitle();
#endif

#if ENABLE(INPUT_TYPE_WEEK_PICKER)
    WEBCORE_EXPORT RefPtr<StringImpl> inputWeekLabel(const DateComponents&);
#endif

#if ENABLE(WEB_AUTHN)
    WEBCORE_EXPORT RefPtr<StringImpl> makeCredentialTouchIDPromptTitle(StringImpl* bundleName, StringImpl* domain);
    WEBCORE_EXPORT RefPtr<StringImpl> getAssertionTouchIDPromptTitle(StringImpl* bundleName, StringImpl* domain);
    WEBCORE_EXPORT RefPtr<StringImpl> genericTouchIDPromptTitle();
#endif

#if ENABLE(IMAGE_ANALYSIS)
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagLookUpImage();
#endif

#if ENABLE(IMAGE_ANALYSIS)
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTagCopySubject();
    WEBCORE_EXPORT RefPtr<StringImpl> contextMenuItemTitleRemoveBackground();
#endif

    WEBCORE_EXPORT RefPtr<StringImpl> pdfPasswordFormTitle();
    WEBCORE_EXPORT RefPtr<StringImpl> pdfPasswordFormSubtitle();
    WEBCORE_EXPORT RefPtr<StringImpl> pdfPasswordFormInvalidPasswordSubtitle();

#if ENABLE(LINEAR_MEDIA_PLAYER)
    WEBCORE_EXPORT RefPtr<StringImpl> fullscreenControllerViewSpatial();
    WEBCORE_EXPORT RefPtr<StringImpl> fullscreenControllerViewImmersive();
#endif

#if ENABLE(SPATIAL_IMAGE_CONTROLS)
    WEBCORE_EXPORT RefPtr<StringImpl> imageControlsLabelSpatial();
    WEBCORE_EXPORT RefPtr<StringImpl> imageControlsLabelPanorama();
#endif

#if PLATFORM(COCOA)
#define WEB_UI_STRING(string, description) WebCore::localizedString(CFSTR(string))
#define WEB_UI_STRING_KEY(string, key, description) WebCore::localizedString(CFSTR(key))
#define WEB_UI_STRING_WITH_MNEMONIC(string, mnemonic, description) WebCore::localizedString(CFSTR(string))
#elif PLATFORM(WIN)
#define WEB_UI_STRING(string, description) WebCore::localizedString(L##string)
#define WEB_UI_STRING_KEY(string, key, description) WebCore::localizedString(L##string)
#define WEB_UI_STRING_WITH_MNEMONIC(string, mnemonic, description) WebCore::localizedString(L##string)
#elif USE(GLIB) && defined(GETTEXT_PACKAGE)
#define WEB_UI_STRING(string, description) WebCore::localizedString(_(string))
#define WEB_UI_STRING_KEY(string, key, description) WebCore::localizedString(C_(key, string))
#define WEB_UI_STRING_WITH_MNEMONIC(string, mnemonic, description) WebCore::localizedString(_(mnemonic))
#else
#define WEB_UI_STRING(string, description) WebCore::localizedString(string)
#define WEB_UI_STRING_KEY(string, key, description) WebCore::localizedString(key)
#define WEB_UI_STRING_WITH_MNEMONIC(string, mnemonic, description) WebCore::localizedString(string)
#endif

#if PLATFORM(COCOA)
// This is exactly as WEB_UI_STRING, but renamed to ensure the string is not scanned by non-CF ports.
#define WEB_UI_CFSTRING(string, description) WebCore::localizedString(CFSTR(string))
#define WEB_UI_CFSTRING_KEY(string, key, description) WebCore::localizedString(CFSTR(key))

    WEBCORE_EXPORT RetainPtr<CFStringRef> copyLocalizedString(CFStringRef key);
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT RefPtr<StringImpl> localizedString(CFStringRef key);
#elif PLATFORM(WIN)
    WEBCORE_EXPORT RefPtr<StringImpl> localizedString(const wchar_t* key);
#else
    WEBCORE_EXPORT RefPtr<StringImpl> localizedString(const char* key);
#endif

#if PLATFORM(COCOA)
#define WEB_UI_FORMAT_CFSTRING(string, description, ...) WebCore::formatLocalizedString(CFSTR(string), LOG_PRINTF_TYPE(__VA_ARGS__))
#define WEB_UI_FORMAT_CFSTRING_KEY(string, key, description, ...) WebCore::formatLocalizedString(CFSTR(key), LOG_PRINTF_TYPE(__VA_ARGS__))
#define WEB_UI_FORMAT_STRING(string, description, ...) WebCore::formatLocalizedString(CFSTR(string), LOG_PRINTF_TYPE(__VA_ARGS__))
#elif PLATFORM(WIN)
#define WEB_UI_FORMAT_STRING(string, description, ...) WebCore::formatLocalizedString(L##string, LOG_PRINTF_TYPE(__VA_ARGS__))
#elif USE(GLIB) && defined(GETTEXT_PACKAGE)
#define WEB_UI_FORMAT_STRING(string, description, ...) WebCore::formatLocalizedString(_(string), LOG_PRINTF_TYPE(__VA_ARGS__))
#else
#define WEB_UI_FORMAT_STRING(string, description, ...) WebCore::formatLocalizedString(string, LOG_PRINTF_TYPE(__VA_ARGS__))
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT RefPtr<StringImpl> formatLocalizedString(CFStringRef format, ...) CF_FORMAT_FUNCTION(1, 2);
#elif PLATFORM(WIN)
    WEBCORE_EXPORT RefPtr<StringImpl> formatLocalizedString(const wchar_t* format, ...);
#else
    WEBCORE_EXPORT RefPtr<StringImpl> formatLocalizedString(const char* format, ...) WTF_ATTRIBUTE_PRINTF(1, 2);
#endif

#ifdef __OBJC__
#define WEB_UI_NSSTRING(string, description) WebCore::localizedNSString(string)
#define WEB_UI_NSSTRING_KEY(string, key, description) WebCore::localizedNSString(key)

    inline NS_FORMAT_ARGUMENT(1) NSString *localizedNSString(NSString *key)
    {
        return bridge_cast(copyLocalizedString(bridge_cast(key)).autorelease());
    }
#endif

} // namespace WebCore
