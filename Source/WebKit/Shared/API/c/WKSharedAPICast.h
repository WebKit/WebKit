/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "APIError.h"
#include "APINumber.h"
#include "APISecurityOrigin.h"
#include "APIString.h"
#include "APIURL.h"
#include "APIURLRequest.h"
#include "APIURLResponse.h"
#include "ImageOptions.h"
#include "SameDocumentNavigationType.h"
#include "WKBase.h"
#include "WKContextMenuItemTypes.h"
#include "WKDiagnosticLoggingResultType.h"
#include "WKEvent.h"
#include "WKFindOptions.h"
#include "WKGeometry.h"
#include "WKImage.h"
#include "WKPageLoadTypes.h"
#include "WKPageLoadTypesPrivate.h"
#include "WKPageVisibilityTypes.h"
#include "WKUserContentInjectedFrames.h"
#include "WKUserScriptInjectionTime.h"
#include "WebFindOptions.h"
#include "WebMouseEvent.h"
#include <WebCore/ContextMenuItem.h>
#include <WebCore/DiagnosticLoggingResultType.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IntRect.h>
#include <WebCore/LayoutMilestone.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/UserContentTypes.h>
#include <WebCore/UserScriptTypes.h>
#include <WebCore/VisibilityState.h>

namespace API {
class Array;
class Dictionary;
class Data;
class Point;
class Rect;
class SecurityOrigin;
class SerializedScriptValue;
class Size;
class UserContentURLPattern;
class WebArchive;
class WebArchiveResource;
}

namespace WebKit {

class ObjCObjectGraph;
class WebConnection;
class WebContextMenuItem;
class WebImage;

template<typename APIType> struct APITypeInfo;
template<typename ImplType> struct ImplTypeInfo;

#define WK_ADD_API_MAPPING(TheAPIType, TheImplType) \
    template<> struct APITypeInfo<TheAPIType> { typedef TheImplType ImplType; }; \
    template<> struct ImplTypeInfo<TheImplType> { typedef TheAPIType APIType; };

WK_ADD_API_MAPPING(WKArrayRef, API::Array)
WK_ADD_API_MAPPING(WKBooleanRef, API::Boolean)
WK_ADD_API_MAPPING(WKConnectionRef, WebConnection)
WK_ADD_API_MAPPING(WKContextMenuItemRef, WebContextMenuItem)
WK_ADD_API_MAPPING(WKDataRef, API::Data)
WK_ADD_API_MAPPING(WKDictionaryRef, API::Dictionary)
WK_ADD_API_MAPPING(WKDoubleRef, API::Double)
WK_ADD_API_MAPPING(WKErrorRef, API::Error)
WK_ADD_API_MAPPING(WKImageRef, WebImage)
WK_ADD_API_MAPPING(WKPointRef, API::Point)
WK_ADD_API_MAPPING(WKRectRef, API::Rect)
WK_ADD_API_MAPPING(WKSecurityOriginRef, API::SecurityOrigin)
WK_ADD_API_MAPPING(WKSerializedScriptValueRef, API::SerializedScriptValue)
WK_ADD_API_MAPPING(WKSizeRef, API::Size)
WK_ADD_API_MAPPING(WKStringRef, API::String)
WK_ADD_API_MAPPING(WKTypeRef, API::Object)
WK_ADD_API_MAPPING(WKUInt64Ref, API::UInt64)
WK_ADD_API_MAPPING(WKURLRef, API::URL)
WK_ADD_API_MAPPING(WKURLRequestRef, API::URLRequest)
WK_ADD_API_MAPPING(WKURLResponseRef, API::URLResponse)
WK_ADD_API_MAPPING(WKUserContentURLPatternRef, API::UserContentURLPattern)

template<> struct APITypeInfo<WKMutableArrayRef> { typedef API::Array ImplType; };
template<> struct APITypeInfo<WKMutableDictionaryRef> { typedef API::Dictionary ImplType; };

#if PLATFORM(COCOA)
WK_ADD_API_MAPPING(WKWebArchiveRef, API::WebArchive)
WK_ADD_API_MAPPING(WKWebArchiveResourceRef, API::WebArchiveResource)
WK_ADD_API_MAPPING(WKObjCTypeWrapperRef, ObjCObjectGraph)
#endif

template<typename T, typename APIType = typename ImplTypeInfo<T>::APIType>
auto toAPI(T* t) -> APIType
{
    return reinterpret_cast<APIType>(API::Object::wrap(t));
}

template<typename T, typename APIType = typename ImplTypeInfo<T>::APIType>
auto toAPI(T& t) -> APIType
{
    return reinterpret_cast<APIType>(API::Object::wrap(&t));
}

template<typename T, typename ImplType = typename APITypeInfo<T>::ImplType>
auto toImpl(T t) -> ImplType*
{
    return static_cast<ImplType*>(API::Object::unwrap(static_cast<void*>(const_cast<typename std::remove_const<typename std::remove_pointer<T>::type>::type*>(t))));
}

template<typename ImplType, typename APIType = typename ImplTypeInfo<ImplType>::APIType>
class ProxyingRefPtr {
public:
    ProxyingRefPtr(RefPtr<ImplType>&& impl)
        : m_impl(impl)
    {
    }

    ProxyingRefPtr(Ref<ImplType>&& impl)
        : m_impl(WTFMove(impl))
    {
    }

    operator APIType() { return toAPI(m_impl.get()); }

private:
    RefPtr<ImplType> m_impl;
};

/* Special cases. */

inline ProxyingRefPtr<API::String> toAPI(StringImpl* string)
{
    return ProxyingRefPtr<API::String>(API::String::create(string));
}

inline WKStringRef toCopiedAPI(const String& string)
{
    return toAPI(&API::String::create(string).leakRef());
}

inline ProxyingRefPtr<API::URL> toURLRef(StringImpl* string)
{
    if (!string)
        return ProxyingRefPtr<API::URL>(nullptr);
    return ProxyingRefPtr<API::URL>(API::URL::create(String(string)));
}

inline WKURLRef toCopiedURLAPI(const String& string)
{
    if (!string)
        return nullptr;
    return toAPI(&API::URL::create(string).leakRef());
}

inline WKURLRef toCopiedURLAPI(const URL& url)
{
    return toCopiedURLAPI(url.string());
}

inline String toWTFString(WKStringRef stringRef)
{
    if (!stringRef)
        return String();
    return toImpl(stringRef)->string();
}

inline String toWTFString(WKURLRef urlRef)
{
    if (!urlRef)
        return String();
    return toImpl(urlRef)->string();
}

inline ProxyingRefPtr<API::Error> toAPI(const WebCore::ResourceError& error)
{
    return ProxyingRefPtr<API::Error>(API::Error::create(error));
}

inline ProxyingRefPtr<API::URLRequest> toAPI(const WebCore::ResourceRequest& request)
{
    return ProxyingRefPtr<API::URLRequest>(API::URLRequest::create(request));
}

inline ProxyingRefPtr<API::URLResponse> toAPI(const WebCore::ResourceResponse& response)
{
    return ProxyingRefPtr<API::URLResponse>(API::URLResponse::create(response));
}

inline WKSecurityOriginRef toCopiedAPI(WebCore::SecurityOrigin* origin)
{
    if (!origin)
        return nullptr;
    return toAPI(&API::SecurityOrigin::create(*origin).leakRef());
}

/* Geometry conversions */

inline WebCore::FloatRect toFloatRect(const WKRect& wkRect)
{
    return WebCore::FloatRect(static_cast<float>(wkRect.origin.x), static_cast<float>(wkRect.origin.y),
                              static_cast<float>(wkRect.size.width), static_cast<float>(wkRect.size.height));
}

inline WebCore::IntSize toIntSize(const WKSize& wkSize)
{
    return WebCore::IntSize(static_cast<int>(wkSize.width), static_cast<int>(wkSize.height));
}

inline WebCore::IntPoint toIntPoint(const WKPoint& wkPoint)
{
    return WebCore::IntPoint(static_cast<int>(wkPoint.x), static_cast<int>(wkPoint.y));
}

inline WebCore::IntRect toIntRect(const WKRect& wkRect)
{
    return WebCore::IntRect(static_cast<int>(wkRect.origin.x), static_cast<int>(wkRect.origin.y),
                            static_cast<int>(wkRect.size.width), static_cast<int>(wkRect.size.height));
}

inline WKRect toAPI(const WebCore::FloatRect& rect)
{
    WKRect wkRect;
    wkRect.origin.x = rect.x();
    wkRect.origin.y = rect.y();
    wkRect.size.width = rect.width();
    wkRect.size.height = rect.height();
    return wkRect;
}

inline WKRect toAPI(const WebCore::IntRect& rect)
{
    WKRect wkRect;
    wkRect.origin.x = rect.x();
    wkRect.origin.y = rect.y();
    wkRect.size.width = rect.width();
    wkRect.size.height = rect.height();
    return wkRect;
}

inline WKSize toAPI(const WebCore::IntSize& size)
{
    WKSize wkSize;
    wkSize.width = size.width();
    wkSize.height = size.height();
    return wkSize;
}

inline WKPoint toAPI(const WebCore::IntPoint& point)
{
    WKPoint wkPoint;
    wkPoint.x = point.x();
    wkPoint.y = point.y();
    return wkPoint;
}

/* Enum conversions */

inline WKTypeID toAPI(API::Object::Type type)
{
    return static_cast<WKTypeID>(type);
}

inline WKEventModifiers toAPI(OptionSet<WebEventModifier> modifiers)
{
    WKEventModifiers wkModifiers = 0;
    if (modifiers.contains(WebEventModifier::ShiftKey))
        wkModifiers |= kWKEventModifiersShiftKey;
    if (modifiers.contains(WebEventModifier::ControlKey))
        wkModifiers |= kWKEventModifiersControlKey;
    if (modifiers.contains(WebEventModifier::AltKey))
        wkModifiers |= kWKEventModifiersAltKey;
    if (modifiers.contains(WebEventModifier::MetaKey))
        wkModifiers |= kWKEventModifiersMetaKey;
    if (modifiers.contains(WebEventModifier::CapsLockKey))
        wkModifiers |= kWKEventModifiersCapsLockKey;
    return wkModifiers;
}

inline WKEventMouseButton toAPI(WebMouseEventButton mouseButton)
{
    WKEventMouseButton wkMouseButton = kWKEventMouseButtonNoButton;

    switch (mouseButton) {
    case WebMouseEventButton::NoButton:
        wkMouseButton = kWKEventMouseButtonNoButton;
        break;
    case WebMouseEventButton::LeftButton:
        wkMouseButton = kWKEventMouseButtonLeftButton;
        break;
    case WebMouseEventButton::MiddleButton:
        wkMouseButton = kWKEventMouseButtonMiddleButton;
        break;
    case WebMouseEventButton::RightButton:
        wkMouseButton = kWKEventMouseButtonRightButton;
        break;
    }

    return wkMouseButton;
}

inline WKEventMouseButton toAPI(WebCore::MouseButton mouseButton)
{
    WKEventMouseButton wkMouseButton = kWKEventMouseButtonNoButton;

    switch (mouseButton) {
    case WebCore::MouseButton::NoButton:
        wkMouseButton = kWKEventMouseButtonNoButton;
        break;
    case WebCore::MouseButton::LeftButton:
        wkMouseButton = kWKEventMouseButtonLeftButton;
        break;
    case WebCore::MouseButton::MiddleButton:
        wkMouseButton = kWKEventMouseButtonMiddleButton;
        break;
    case WebCore::MouseButton::RightButton:
        wkMouseButton = kWKEventMouseButtonRightButton;
        break;
    }

    return wkMouseButton;
}

inline WKContextMenuItemTag toAPI(WebCore::ContextMenuAction action)
{
    switch (action) {
    case WebCore::ContextMenuItemTagNoAction:
        return kWKContextMenuItemTagNoAction;
    case WebCore::ContextMenuItemTagOpenLinkInNewWindow:
        return kWKContextMenuItemTagOpenLinkInNewWindow;
    case WebCore::ContextMenuItemTagDownloadLinkToDisk:
        return kWKContextMenuItemTagDownloadLinkToDisk;
    case WebCore::ContextMenuItemTagCopyLinkToClipboard:
        return kWKContextMenuItemTagCopyLinkToClipboard;
    case WebCore::ContextMenuItemTagOpenImageInNewWindow:
        return kWKContextMenuItemTagOpenImageInNewWindow;
    case WebCore::ContextMenuItemTagDownloadImageToDisk:
        return kWKContextMenuItemTagDownloadImageToDisk;
    case WebCore::ContextMenuItemTagCopyImageToClipboard:
        return kWKContextMenuItemTagCopyImageToClipboard;
#if PLATFORM(GTK)
    case WebCore::ContextMenuItemTagCopyImageUrlToClipboard:
        return kWKContextMenuItemTagCopyImageUrlToClipboard;
#endif
    case WebCore::ContextMenuItemTagOpenFrameInNewWindow:
        return kWKContextMenuItemTagOpenFrameInNewWindow;
    case WebCore::ContextMenuItemTagCopy:
        return kWKContextMenuItemTagCopy;
    case WebCore::ContextMenuItemTagGoBack:
        return kWKContextMenuItemTagGoBack;
    case WebCore::ContextMenuItemTagGoForward:
        return kWKContextMenuItemTagGoForward;
    case WebCore::ContextMenuItemTagStop:
        return kWKContextMenuItemTagStop;
    case WebCore::ContextMenuItemTagReload:
        return kWKContextMenuItemTagReload;
    case WebCore::ContextMenuItemTagCut:
        return kWKContextMenuItemTagCut;
    case WebCore::ContextMenuItemTagPaste:
        return kWKContextMenuItemTagPaste;
#if PLATFORM(GTK)
    case WebCore::ContextMenuItemTagSelectAll:
        return kWKContextMenuItemTagSelectAll;
#endif
    case WebCore::ContextMenuItemTagSpellingGuess:
        return kWKContextMenuItemTagSpellingGuess;
    case WebCore::ContextMenuItemTagNoGuessesFound:
        return kWKContextMenuItemTagNoGuessesFound;
    case WebCore::ContextMenuItemTagIgnoreSpelling:
        return kWKContextMenuItemTagIgnoreSpelling;
    case WebCore::ContextMenuItemTagLearnSpelling:
        return kWKContextMenuItemTagLearnSpelling;
    case WebCore::ContextMenuItemTagOther:
        return kWKContextMenuItemTagOther;
    case WebCore::ContextMenuItemTagSearchInSpotlight:
        return kWKContextMenuItemTagSearchInSpotlight;
    case WebCore::ContextMenuItemTagSearchWeb:
        return kWKContextMenuItemTagSearchWeb;
    case WebCore::ContextMenuItemTagLookUpInDictionary:
        return kWKContextMenuItemTagLookUpInDictionary;
    case WebCore::ContextMenuItemTagOpenWithDefaultApplication:
        return kWKContextMenuItemTagOpenWithDefaultApplication;
    case WebCore::ContextMenuItemPDFActualSize:
        return kWKContextMenuItemTagPDFActualSize;
    case WebCore::ContextMenuItemPDFZoomIn:
        return kWKContextMenuItemTagPDFZoomIn;
    case WebCore::ContextMenuItemPDFZoomOut:
        return kWKContextMenuItemTagPDFZoomOut;
    case WebCore::ContextMenuItemPDFAutoSize:
        return kWKContextMenuItemTagPDFAutoSize;
    case WebCore::ContextMenuItemPDFSinglePage:
        return kWKContextMenuItemTagPDFSinglePage;
    case WebCore::ContextMenuItemPDFFacingPages:
        return kWKContextMenuItemTagPDFFacingPages;
    case WebCore::ContextMenuItemPDFContinuous:
        return kWKContextMenuItemTagPDFContinuous;
    case WebCore::ContextMenuItemPDFNextPage:
        return kWKContextMenuItemTagPDFNextPage;
    case WebCore::ContextMenuItemPDFPreviousPage:
        return kWKContextMenuItemTagPDFPreviousPage;
    case WebCore::ContextMenuItemTagOpenLink:
        return kWKContextMenuItemTagOpenLink;
    case WebCore::ContextMenuItemTagIgnoreGrammar:
        return kWKContextMenuItemTagIgnoreGrammar;
    case WebCore::ContextMenuItemTagSpellingMenu:
        return kWKContextMenuItemTagSpellingMenu;
    case WebCore::ContextMenuItemTagShowSpellingPanel:
        return kWKContextMenuItemTagShowSpellingPanel;
    case WebCore::ContextMenuItemTagCheckSpelling:
        return kWKContextMenuItemTagCheckSpelling;
    case WebCore::ContextMenuItemTagCheckSpellingWhileTyping:
        return kWKContextMenuItemTagCheckSpellingWhileTyping;
    case WebCore::ContextMenuItemTagCheckGrammarWithSpelling:
        return kWKContextMenuItemTagCheckGrammarWithSpelling;
    case WebCore::ContextMenuItemTagFontMenu:
        return kWKContextMenuItemTagFontMenu;
    case WebCore::ContextMenuItemTagShowFonts:
        return kWKContextMenuItemTagShowFonts;
    case WebCore::ContextMenuItemTagBold:
        return kWKContextMenuItemTagBold;
    case WebCore::ContextMenuItemTagItalic:
        return kWKContextMenuItemTagItalic;
    case WebCore::ContextMenuItemTagUnderline:
        return kWKContextMenuItemTagUnderline;
    case WebCore::ContextMenuItemTagOutline:
        return kWKContextMenuItemTagOutline;
    case WebCore::ContextMenuItemTagStyles:
        return kWKContextMenuItemTagStyles;
    case WebCore::ContextMenuItemTagShowColors:
        return kWKContextMenuItemTagShowColors;
    case WebCore::ContextMenuItemTagSpeechMenu:
        return kWKContextMenuItemTagSpeechMenu;
    case WebCore::ContextMenuItemTagStartSpeaking:
        return kWKContextMenuItemTagStartSpeaking;
    case WebCore::ContextMenuItemTagStopSpeaking:
        return kWKContextMenuItemTagStopSpeaking;
    case WebCore::ContextMenuItemTagWritingDirectionMenu:
        return kWKContextMenuItemTagWritingDirectionMenu;
    case WebCore::ContextMenuItemTagDefaultDirection:
        return kWKContextMenuItemTagDefaultDirection;
    case WebCore::ContextMenuItemTagLeftToRight:
        return kWKContextMenuItemTagLeftToRight;
    case WebCore::ContextMenuItemTagRightToLeft:
        return kWKContextMenuItemTagRightToLeft;
    case WebCore::ContextMenuItemTagPDFSinglePageScrolling:
        return kWKContextMenuItemTagPDFSinglePageScrolling;
    case WebCore::ContextMenuItemTagPDFFacingPagesScrolling:
        return kWKContextMenuItemTagPDFFacingPagesScrolling;
    case WebCore::ContextMenuItemTagDictationAlternative:
        return kWKContextMenuItemTagDictationAlternative;
    case WebCore::ContextMenuItemTagInspectElement:
        return kWKContextMenuItemTagInspectElement;
    case WebCore::ContextMenuItemTagTextDirectionMenu:
        return kWKContextMenuItemTagTextDirectionMenu;
    case WebCore::ContextMenuItemTagTextDirectionDefault:
        return kWKContextMenuItemTagTextDirectionDefault;
    case WebCore::ContextMenuItemTagTextDirectionLeftToRight:
        return kWKContextMenuItemTagTextDirectionLeftToRight;
    case WebCore::ContextMenuItemTagTextDirectionRightToLeft:
        return kWKContextMenuItemTagTextDirectionRightToLeft;
    case WebCore::ContextMenuItemTagOpenMediaInNewWindow:
        return kWKContextMenuItemTagOpenMediaInNewWindow;
    case WebCore::ContextMenuItemTagDownloadMediaToDisk:
        return kWKContextMenuItemTagDownloadMediaToDisk;
    case WebCore::ContextMenuItemTagCopyMediaLinkToClipboard:
        return kWKContextMenuItemTagCopyMediaLinkToClipboard;
    case WebCore::ContextMenuItemTagToggleMediaControls:
        return kWKContextMenuItemTagToggleMediaControls;
    case WebCore::ContextMenuItemTagToggleMediaLoop:
        return kWKContextMenuItemTagToggleMediaLoop;
    case WebCore::ContextMenuItemTagToggleVideoFullscreen:
        return kWKContextMenuItemTagToggleVideoFullscreen;
    case WebCore::ContextMenuItemTagEnterVideoFullscreen:
        return kWKContextMenuItemTagEnterVideoFullscreen;
    case WebCore::ContextMenuItemTagToggleVideoEnhancedFullscreen:
        return kWKContextMenuItemTagToggleVideoEnhancedFullscreen;
    case WebCore::ContextMenuItemTagMediaPlayPause:
        return kWKContextMenuItemTagMediaPlayPause;
    case WebCore::ContextMenuItemTagMediaMute:
        return kWKContextMenuItemTagMediaMute;
    case WebCore::ContextMenuItemTagAddHighlightToCurrentQuickNote:
        return kWKContextMenuItemTagAddHighlightToCurrentQuickNote;
    case WebCore::ContextMenuItemTagAddHighlightToNewQuickNote:
        return kWKContextMenuItemTagAddHighlightToNewQuickNote;
#if PLATFORM(COCOA)
    case WebCore::ContextMenuItemTagCorrectSpellingAutomatically:
        return kWKContextMenuItemTagCorrectSpellingAutomatically;
    case WebCore::ContextMenuItemTagSubstitutionsMenu:
        return kWKContextMenuItemTagSubstitutionsMenu;
    case WebCore::ContextMenuItemTagShowSubstitutions:
        return kWKContextMenuItemTagShowSubstitutions;
    case WebCore::ContextMenuItemTagSmartCopyPaste:
        return kWKContextMenuItemTagSmartCopyPaste;
    case WebCore::ContextMenuItemTagSmartQuotes:
        return kWKContextMenuItemTagSmartQuotes;
    case WebCore::ContextMenuItemTagSmartDashes:
        return kWKContextMenuItemTagSmartDashes;
    case WebCore::ContextMenuItemTagSmartLinks:
        return kWKContextMenuItemTagSmartLinks;
    case WebCore::ContextMenuItemTagTextReplacement:
        return kWKContextMenuItemTagTextReplacement;
    case WebCore::ContextMenuItemTagTransformationsMenu:
        return kWKContextMenuItemTagTransformationsMenu;
    case WebCore::ContextMenuItemTagMakeUpperCase:
        return kWKContextMenuItemTagMakeUpperCase;
    case WebCore::ContextMenuItemTagMakeLowerCase:
        return kWKContextMenuItemTagMakeLowerCase;
    case WebCore::ContextMenuItemTagCapitalize:
        return kWKContextMenuItemTagCapitalize;
    case WebCore::ContextMenuItemTagChangeBack:
        return kWKContextMenuItemTagChangeBack;
#endif
    case WebCore::ContextMenuItemTagShareMenu:
        return kWKContextMenuItemTagShareMenu;
    case WebCore::ContextMenuItemTagLookUpImage:
        return kWKContextMenuItemTagRevealImage;
    case WebCore::ContextMenuItemTagTranslate:
        return kWKContextMenuItemTagTranslate;
    case WebCore::ContextMenuItemTagCopySubject:
        return kWKContextMenuItemTagCopyCroppedImage;
    default:
        if (action < WebCore::ContextMenuItemBaseApplicationTag && !(action >= WebCore::ContextMenuItemBaseCustomTag && action <= WebCore::ContextMenuItemLastCustomTag))
            LOG_ERROR("ContextMenuAction %i is an unknown tag but is below the allowable custom tag value of %i", action, WebCore::ContextMenuItemBaseApplicationTag);
        return static_cast<WKContextMenuItemTag>(action);
    }
}

inline WebCore::ContextMenuAction toImpl(WKContextMenuItemTag tag)
{
    switch (tag) {
    case kWKContextMenuItemTagNoAction:
        return WebCore::ContextMenuItemTagNoAction;
    case kWKContextMenuItemTagOpenLinkInNewWindow:
        return WebCore::ContextMenuItemTagOpenLinkInNewWindow;
    case kWKContextMenuItemTagDownloadLinkToDisk:
        return WebCore::ContextMenuItemTagDownloadLinkToDisk;
    case kWKContextMenuItemTagCopyLinkToClipboard:
        return WebCore::ContextMenuItemTagCopyLinkToClipboard;
    case kWKContextMenuItemTagOpenImageInNewWindow:
        return WebCore::ContextMenuItemTagOpenImageInNewWindow;
    case kWKContextMenuItemTagDownloadImageToDisk:
        return WebCore::ContextMenuItemTagDownloadImageToDisk;
    case kWKContextMenuItemTagCopyImageToClipboard:
        return WebCore::ContextMenuItemTagCopyImageToClipboard;
    case kWKContextMenuItemTagOpenFrameInNewWindow:
#if PLATFORM(GTK)
    case kWKContextMenuItemTagCopyImageUrlToClipboard:
        return WebCore::ContextMenuItemTagCopyImageUrlToClipboard;
#endif
        return WebCore::ContextMenuItemTagOpenFrameInNewWindow;
    case kWKContextMenuItemTagCopy:
        return WebCore::ContextMenuItemTagCopy;
    case kWKContextMenuItemTagGoBack:
        return WebCore::ContextMenuItemTagGoBack;
    case kWKContextMenuItemTagGoForward:
        return WebCore::ContextMenuItemTagGoForward;
    case kWKContextMenuItemTagStop:
        return WebCore::ContextMenuItemTagStop;
    case kWKContextMenuItemTagReload:
        return WebCore::ContextMenuItemTagReload;
    case kWKContextMenuItemTagCut:
        return WebCore::ContextMenuItemTagCut;
    case kWKContextMenuItemTagPaste:
        return WebCore::ContextMenuItemTagPaste;
#if PLATFORM(GTK)
    case kWKContextMenuItemTagSelectAll:
        return WebCore::ContextMenuItemTagSelectAll;
#endif
    case kWKContextMenuItemTagSpellingGuess:
        return WebCore::ContextMenuItemTagSpellingGuess;
    case kWKContextMenuItemTagNoGuessesFound:
        return WebCore::ContextMenuItemTagNoGuessesFound;
    case kWKContextMenuItemTagIgnoreSpelling:
        return WebCore::ContextMenuItemTagIgnoreSpelling;
    case kWKContextMenuItemTagLearnSpelling:
        return WebCore::ContextMenuItemTagLearnSpelling;
    case kWKContextMenuItemTagOther:
        return WebCore::ContextMenuItemTagOther;
    case kWKContextMenuItemTagSearchInSpotlight:
        return WebCore::ContextMenuItemTagSearchInSpotlight;
    case kWKContextMenuItemTagSearchWeb:
        return WebCore::ContextMenuItemTagSearchWeb;
    case kWKContextMenuItemTagLookUpInDictionary:
        return WebCore::ContextMenuItemTagLookUpInDictionary;
    case kWKContextMenuItemTagOpenWithDefaultApplication:
        return WebCore::ContextMenuItemTagOpenWithDefaultApplication;
    case kWKContextMenuItemTagPDFActualSize:
        return WebCore::ContextMenuItemPDFActualSize;
    case kWKContextMenuItemTagPDFZoomIn:
        return WebCore::ContextMenuItemPDFZoomIn;
    case kWKContextMenuItemTagPDFZoomOut:
        return WebCore::ContextMenuItemPDFZoomOut;
    case kWKContextMenuItemTagPDFAutoSize:
        return WebCore::ContextMenuItemPDFAutoSize;
    case kWKContextMenuItemTagPDFSinglePage:
        return WebCore::ContextMenuItemPDFSinglePage;
    case kWKContextMenuItemTagPDFFacingPages:
        return WebCore::ContextMenuItemPDFFacingPages;
    case kWKContextMenuItemTagPDFContinuous:
        return WebCore::ContextMenuItemPDFContinuous;
    case kWKContextMenuItemTagPDFNextPage:
        return WebCore::ContextMenuItemPDFNextPage;
    case kWKContextMenuItemTagPDFPreviousPage:
        return WebCore::ContextMenuItemPDFPreviousPage;
    case kWKContextMenuItemTagOpenLink:
        return WebCore::ContextMenuItemTagOpenLink;
    case kWKContextMenuItemTagIgnoreGrammar:
        return WebCore::ContextMenuItemTagIgnoreGrammar;
    case kWKContextMenuItemTagSpellingMenu:
        return WebCore::ContextMenuItemTagSpellingMenu;
    case kWKContextMenuItemTagShowSpellingPanel:
        return WebCore::ContextMenuItemTagShowSpellingPanel;
    case kWKContextMenuItemTagCheckSpelling:
        return WebCore::ContextMenuItemTagCheckSpelling;
    case kWKContextMenuItemTagCheckSpellingWhileTyping:
        return WebCore::ContextMenuItemTagCheckSpellingWhileTyping;
    case kWKContextMenuItemTagCheckGrammarWithSpelling:
        return WebCore::ContextMenuItemTagCheckGrammarWithSpelling;
    case kWKContextMenuItemTagFontMenu:
        return WebCore::ContextMenuItemTagFontMenu;
    case kWKContextMenuItemTagShowFonts:
        return WebCore::ContextMenuItemTagShowFonts;
    case kWKContextMenuItemTagBold:
        return WebCore::ContextMenuItemTagBold;
    case kWKContextMenuItemTagItalic:
        return WebCore::ContextMenuItemTagItalic;
    case kWKContextMenuItemTagUnderline:
        return WebCore::ContextMenuItemTagUnderline;
    case kWKContextMenuItemTagOutline:
        return WebCore::ContextMenuItemTagOutline;
    case kWKContextMenuItemTagStyles:
        return WebCore::ContextMenuItemTagStyles;
    case kWKContextMenuItemTagShowColors:
        return WebCore::ContextMenuItemTagShowColors;
    case kWKContextMenuItemTagSpeechMenu:
        return WebCore::ContextMenuItemTagSpeechMenu;
    case kWKContextMenuItemTagStartSpeaking:
        return WebCore::ContextMenuItemTagStartSpeaking;
    case kWKContextMenuItemTagStopSpeaking:
        return WebCore::ContextMenuItemTagStopSpeaking;
    case kWKContextMenuItemTagWritingDirectionMenu:
        return WebCore::ContextMenuItemTagWritingDirectionMenu;
    case kWKContextMenuItemTagDefaultDirection:
        return WebCore::ContextMenuItemTagDefaultDirection;
    case kWKContextMenuItemTagLeftToRight:
        return WebCore::ContextMenuItemTagLeftToRight;
    case kWKContextMenuItemTagRightToLeft:
        return WebCore::ContextMenuItemTagRightToLeft;
    case kWKContextMenuItemTagPDFSinglePageScrolling:
        return WebCore::ContextMenuItemTagPDFSinglePageScrolling;
    case kWKContextMenuItemTagPDFFacingPagesScrolling:
        return WebCore::ContextMenuItemTagPDFFacingPagesScrolling;
    case kWKContextMenuItemTagDictationAlternative:
        return WebCore::ContextMenuItemTagDictationAlternative;
    case kWKContextMenuItemTagInspectElement:
        return WebCore::ContextMenuItemTagInspectElement;
    case kWKContextMenuItemTagTextDirectionMenu:
        return WebCore::ContextMenuItemTagTextDirectionMenu;
    case kWKContextMenuItemTagTextDirectionDefault:
        return WebCore::ContextMenuItemTagTextDirectionDefault;
    case kWKContextMenuItemTagTextDirectionLeftToRight:
        return WebCore::ContextMenuItemTagTextDirectionLeftToRight;
    case kWKContextMenuItemTagTextDirectionRightToLeft:
        return WebCore::ContextMenuItemTagTextDirectionRightToLeft;
    case kWKContextMenuItemTagOpenMediaInNewWindow:
        return WebCore::ContextMenuItemTagOpenMediaInNewWindow;
    case kWKContextMenuItemTagDownloadMediaToDisk:
        return WebCore::ContextMenuItemTagDownloadMediaToDisk;
    case kWKContextMenuItemTagCopyMediaLinkToClipboard:
        return WebCore::ContextMenuItemTagCopyMediaLinkToClipboard;
    case kWKContextMenuItemTagToggleMediaControls:
        return WebCore::ContextMenuItemTagToggleMediaControls;
    case kWKContextMenuItemTagToggleMediaLoop:
        return WebCore::ContextMenuItemTagToggleMediaLoop;
    case kWKContextMenuItemTagToggleVideoFullscreen:
        return WebCore::ContextMenuItemTagToggleVideoFullscreen;
    case kWKContextMenuItemTagEnterVideoFullscreen:
        return WebCore::ContextMenuItemTagEnterVideoFullscreen;
    case kWKContextMenuItemTagToggleVideoEnhancedFullscreen:
        return WebCore::ContextMenuItemTagToggleVideoEnhancedFullscreen;
    case kWKContextMenuItemTagMediaPlayPause:
        return WebCore::ContextMenuItemTagMediaPlayPause;
    case kWKContextMenuItemTagMediaMute:
        return WebCore::ContextMenuItemTagMediaMute;
    case kWKContextMenuItemTagAddHighlightToCurrentQuickNote:
        return WebCore::ContextMenuItemTagAddHighlightToCurrentQuickNote;
    case kWKContextMenuItemTagAddHighlightToNewQuickNote:
        return WebCore::ContextMenuItemTagAddHighlightToNewQuickNote;
#if PLATFORM(COCOA)
    case kWKContextMenuItemTagCorrectSpellingAutomatically:
        return WebCore::ContextMenuItemTagCorrectSpellingAutomatically;
    case kWKContextMenuItemTagSubstitutionsMenu:
        return WebCore::ContextMenuItemTagSubstitutionsMenu;
    case kWKContextMenuItemTagShowSubstitutions:
        return WebCore::ContextMenuItemTagShowSubstitutions;
    case kWKContextMenuItemTagSmartCopyPaste:
        return WebCore::ContextMenuItemTagSmartCopyPaste;
    case kWKContextMenuItemTagSmartQuotes:
        return WebCore::ContextMenuItemTagSmartQuotes;
    case kWKContextMenuItemTagSmartDashes:
        return WebCore::ContextMenuItemTagSmartDashes;
    case kWKContextMenuItemTagSmartLinks:
        return WebCore::ContextMenuItemTagSmartLinks;
    case kWKContextMenuItemTagTextReplacement:
        return WebCore::ContextMenuItemTagTextReplacement;
    case kWKContextMenuItemTagTransformationsMenu:
        return WebCore::ContextMenuItemTagTransformationsMenu;
    case kWKContextMenuItemTagMakeUpperCase:
        return WebCore::ContextMenuItemTagMakeUpperCase;
    case kWKContextMenuItemTagMakeLowerCase:
        return WebCore::ContextMenuItemTagMakeLowerCase;
    case kWKContextMenuItemTagCapitalize:
        return WebCore::ContextMenuItemTagCapitalize;
    case kWKContextMenuItemTagChangeBack:
        return WebCore::ContextMenuItemTagChangeBack;
    case kWKContextMenuItemTagShareMenu:
        return WebCore::ContextMenuItemTagShareMenu;
#endif
    case kWKContextMenuItemTagRevealImage:
        return WebCore::ContextMenuItemTagLookUpImage;
    case kWKContextMenuItemTagTranslate:
        return WebCore::ContextMenuItemTagTranslate;
    case kWKContextMenuItemTagCopyCroppedImage:
        return WebCore::ContextMenuItemTagCopySubject;
    case kWKContextMenuItemTagOpenLinkInThisWindow:
    default:
        if (tag < kWKContextMenuItemBaseApplicationTag && !(tag >= WebCore::ContextMenuItemBaseCustomTag && tag <= WebCore::ContextMenuItemLastCustomTag))
            LOG_ERROR("WKContextMenuItemTag %i is an unknown tag but is below the allowable custom tag value of %i", tag, kWKContextMenuItemBaseApplicationTag);
        return static_cast<WebCore::ContextMenuAction>(tag);
    }
}

inline WKContextMenuItemType toAPI(WebCore::ContextMenuItemType type)
{
    switch(type) {
    case WebCore::ActionType:
        return kWKContextMenuItemTypeAction;
    case WebCore::CheckableActionType:
        return kWKContextMenuItemTypeCheckableAction;
    case WebCore::SeparatorType:
        return kWKContextMenuItemTypeSeparator;
    case WebCore::SubmenuType:
        return kWKContextMenuItemTypeSubmenu;
    default:
        ASSERT_NOT_REACHED();
        return kWKContextMenuItemTypeAction;
    }
}

inline OptionSet<FindOptions> toFindOptions(WKFindOptions wkFindOptions)
{
    OptionSet<FindOptions> findOptions;

    if (wkFindOptions & kWKFindOptionsCaseInsensitive)
        findOptions.add(FindOptions::CaseInsensitive);
    if (wkFindOptions & kWKFindOptionsAtWordStarts)
        findOptions.add(FindOptions::AtWordStarts);
    if (wkFindOptions & kWKFindOptionsTreatMedialCapitalAsWordStart)
        findOptions.add(FindOptions::TreatMedialCapitalAsWordStart);
    if (wkFindOptions & kWKFindOptionsBackwards)
        findOptions.add(FindOptions::Backwards);
    if (wkFindOptions & kWKFindOptionsWrapAround)
        findOptions.add(FindOptions::WrapAround);
    if (wkFindOptions & kWKFindOptionsShowOverlay)
        findOptions.add(FindOptions::ShowOverlay);
    if (wkFindOptions & kWKFindOptionsShowFindIndicator)
        findOptions.add(FindOptions::ShowFindIndicator);
    if (wkFindOptions & kWKFindOptionsShowHighlight)
        findOptions.add(FindOptions::ShowHighlight);

    return findOptions;
}

inline WKFrameNavigationType toAPI(WebCore::NavigationType type)
{
    WKFrameNavigationType wkType = kWKFrameNavigationTypeOther;

    switch (type) {
    case WebCore::NavigationType::LinkClicked:
        wkType = kWKFrameNavigationTypeLinkClicked;
        break;
    case WebCore::NavigationType::FormSubmitted:
        wkType = kWKFrameNavigationTypeFormSubmitted;
        break;
    case WebCore::NavigationType::BackForward:
        wkType = kWKFrameNavigationTypeBackForward;
        break;
    case WebCore::NavigationType::Reload:
        wkType = kWKFrameNavigationTypeReload;
        break;
    case WebCore::NavigationType::FormResubmitted:
        wkType = kWKFrameNavigationTypeFormResubmitted;
        break;
    case WebCore::NavigationType::Other:
        wkType = kWKFrameNavigationTypeOther;
        break;
    }
    
    return wkType;
}

inline WKSameDocumentNavigationType toAPI(SameDocumentNavigationType type)
{
    WKFrameNavigationType wkType = kWKSameDocumentNavigationAnchorNavigation;

    switch (type) {
    case SameDocumentNavigationAnchorNavigation:
        wkType = kWKSameDocumentNavigationAnchorNavigation;
        break;
    case SameDocumentNavigationSessionStatePush:
        wkType = kWKSameDocumentNavigationSessionStatePush;
        break;
    case SameDocumentNavigationSessionStateReplace:
        wkType = kWKSameDocumentNavigationSessionStateReplace;
        break;
    case SameDocumentNavigationSessionStatePop:
        wkType = kWKSameDocumentNavigationSessionStatePop;
        break;
    }
    
    return wkType;
}

inline SameDocumentNavigationType toSameDocumentNavigationType(WKSameDocumentNavigationType wkType)
{
    SameDocumentNavigationType type = SameDocumentNavigationAnchorNavigation;

    switch (wkType) {
    case kWKSameDocumentNavigationAnchorNavigation:
        type = SameDocumentNavigationAnchorNavigation;
        break;
    case kWKSameDocumentNavigationSessionStatePush:
        type = SameDocumentNavigationSessionStatePush;
        break;
    case kWKSameDocumentNavigationSessionStateReplace:
        type = SameDocumentNavigationSessionStateReplace;
        break;
    case kWKSameDocumentNavigationSessionStatePop:
        type = SameDocumentNavigationSessionStatePop;
        break;
    }
    
    return type;
}

inline WKDiagnosticLoggingResultType toAPI(WebCore::DiagnosticLoggingResultType type)
{
    WKDiagnosticLoggingResultType wkType { };

    switch (type) {
    case WebCore::DiagnosticLoggingResultPass:
        wkType = kWKDiagnosticLoggingResultPass;
        break;
    case WebCore::DiagnosticLoggingResultFail:
        wkType = kWKDiagnosticLoggingResultFail;
        break;
    case WebCore::DiagnosticLoggingResultNoop:
        wkType = kWKDiagnosticLoggingResultNoop;
        break;
    }

    return wkType;
}

inline WebCore::DiagnosticLoggingResultType toDiagnosticLoggingResultType(WKDiagnosticLoggingResultType wkType)
{
    WebCore::DiagnosticLoggingResultType type { };

    switch (wkType) {
    case kWKDiagnosticLoggingResultPass:
        type = WebCore::DiagnosticLoggingResultPass;
        break;
    case kWKDiagnosticLoggingResultFail:
        type = WebCore::DiagnosticLoggingResultFail;
        break;
    case kWKDiagnosticLoggingResultNoop:
        type = WebCore::DiagnosticLoggingResultNoop;
        break;
    }

    return type;
}

inline WKLayoutMilestones toWKLayoutMilestones(OptionSet<WebCore::LayoutMilestone> milestones)
{
    unsigned wkMilestones = 0;

    if (milestones & WebCore::DidFirstLayout)
        wkMilestones |= kWKDidFirstLayout;
    if (milestones & WebCore::DidFirstVisuallyNonEmptyLayout)
        wkMilestones |= kWKDidFirstVisuallyNonEmptyLayout;
    if (milestones & WebCore::DidHitRelevantRepaintedObjectsAreaThreshold)
        wkMilestones |= kWKDidHitRelevantRepaintedObjectsAreaThreshold;
    if (milestones & WebCore::DidFirstFlushForHeaderLayer)
        wkMilestones |= kWKDidFirstFlushForHeaderLayer;
    if (milestones & WebCore::DidFirstLayoutAfterSuppressedIncrementalRendering)
        wkMilestones |= kWKDidFirstLayoutAfterSuppressedIncrementalRendering;
    if (milestones & WebCore::DidFirstPaintAfterSuppressedIncrementalRendering)
        wkMilestones |= kWKDidFirstPaintAfterSuppressedIncrementalRendering;
    if (milestones & WebCore::DidRenderSignificantAmountOfText)
        wkMilestones |= kWKDidRenderSignificantAmountOfText;
    if (milestones & WebCore::DidFirstMeaningfulPaint)
        wkMilestones |= kWKDidFirstMeaningfulPaint;

    return wkMilestones;
}

inline OptionSet<WebCore::LayoutMilestone> toLayoutMilestones(WKLayoutMilestones wkMilestones)
{
    OptionSet<WebCore::LayoutMilestone> milestones;

    if (wkMilestones & kWKDidFirstLayout)
        milestones.add(WebCore::DidFirstLayout);
    if (wkMilestones & kWKDidFirstVisuallyNonEmptyLayout)
        milestones.add(WebCore::DidFirstVisuallyNonEmptyLayout);
    if (wkMilestones & kWKDidHitRelevantRepaintedObjectsAreaThreshold)
        milestones.add(WebCore::DidHitRelevantRepaintedObjectsAreaThreshold);
    if (wkMilestones & kWKDidFirstFlushForHeaderLayer)
        milestones.add(WebCore::DidFirstFlushForHeaderLayer);
    if (wkMilestones & kWKDidFirstLayoutAfterSuppressedIncrementalRendering)
        milestones.add(WebCore::DidFirstLayoutAfterSuppressedIncrementalRendering);
    if (wkMilestones & kWKDidFirstPaintAfterSuppressedIncrementalRendering)
        milestones.add(WebCore::DidFirstPaintAfterSuppressedIncrementalRendering);
    if (wkMilestones & kWKDidRenderSignificantAmountOfText)
        milestones.add(WebCore::DidRenderSignificantAmountOfText);
    if (wkMilestones & kWKDidFirstMeaningfulPaint)
        milestones.add(WebCore::DidFirstMeaningfulPaint);
    
    return milestones;
}

inline WebCore::VisibilityState toVisibilityState(WKPageVisibilityState wkPageVisibilityState)
{
    switch (wkPageVisibilityState) {
    case kWKPageVisibilityStateVisible:
        return WebCore::VisibilityState::Visible;
    case kWKPageVisibilityStateHidden:
        return WebCore::VisibilityState::Hidden;
    case kWKPageVisibilityStatePrerender:
        return WebCore::VisibilityState::Hidden;
    }

    ASSERT_NOT_REACHED();
    return WebCore::VisibilityState::Visible;
}

inline ImageOptions toImageOptions(WKImageOptions wkImageOptions)
{
    unsigned imageOptions = 0;

    if (wkImageOptions & kWKImageOptionsShareable)
        imageOptions |= ImageOptionsShareable;

    return static_cast<ImageOptions>(imageOptions);
}

inline SnapshotOptions snapshotOptionsFromImageOptions(WKImageOptions wkImageOptions)
{
    unsigned snapshotOptions = 0;

    if (wkImageOptions & kWKImageOptionsShareable)
        snapshotOptions |= SnapshotOptionsShareable;
    
    return snapshotOptions;
}

inline SnapshotOptions toSnapshotOptions(WKSnapshotOptions wkSnapshotOptions)
{
    unsigned snapshotOptions = 0;

    if (wkSnapshotOptions & kWKSnapshotOptionsShareable)
        snapshotOptions |= SnapshotOptionsShareable;
    if (wkSnapshotOptions & kWKSnapshotOptionsExcludeSelectionHighlighting)
        snapshotOptions |= SnapshotOptionsExcludeSelectionHighlighting;
    if (wkSnapshotOptions & kWKSnapshotOptionsInViewCoordinates)
        snapshotOptions |= SnapshotOptionsInViewCoordinates;
    if (wkSnapshotOptions & kWKSnapshotOptionsPaintSelectionRectangle)
        snapshotOptions |= SnapshotOptionsPaintSelectionRectangle;
    if (wkSnapshotOptions & kWKSnapshotOptionsForceBlackText)
        snapshotOptions |= SnapshotOptionsForceBlackText;
    if (wkSnapshotOptions & kWKSnapshotOptionsForceWhiteText)
        snapshotOptions |= SnapshotOptionsForceWhiteText;
    if (wkSnapshotOptions & kWKSnapshotOptionsPrinting)
        snapshotOptions |= SnapshotOptionsPrinting;
    if (wkSnapshotOptions & kWKSnapshotOptionsExtendedColor)
        snapshotOptions |= SnapshotOptionsUseScreenColorSpace;

    return snapshotOptions;
}

inline WebCore::UserScriptInjectionTime toUserScriptInjectionTime(_WKUserScriptInjectionTime wkInjectedTime)
{
    switch (wkInjectedTime) {
    case kWKInjectAtDocumentStart:
        return WebCore::UserScriptInjectionTime::DocumentStart;
    case kWKInjectAtDocumentEnd:
        return WebCore::UserScriptInjectionTime::DocumentEnd;
    }

    ASSERT_NOT_REACHED();
    return WebCore::UserScriptInjectionTime::DocumentStart;
}

inline _WKUserScriptInjectionTime toWKUserScriptInjectionTime(WebCore::UserScriptInjectionTime injectedTime)
{
    switch (injectedTime) {
    case WebCore::UserScriptInjectionTime::DocumentStart:
        return kWKInjectAtDocumentStart;
    case WebCore::UserScriptInjectionTime::DocumentEnd:
        return kWKInjectAtDocumentEnd;
    }

    ASSERT_NOT_REACHED();
    return kWKInjectAtDocumentStart;
}

inline WebCore::UserContentInjectedFrames toUserContentInjectedFrames(WKUserContentInjectedFrames wkInjectedFrames)
{
    switch (wkInjectedFrames) {
    case kWKInjectInAllFrames:
        return WebCore::UserContentInjectedFrames::InjectInAllFrames;
    case kWKInjectInTopFrameOnly:
        return WebCore::UserContentInjectedFrames::InjectInTopFrameOnly;
    }

    ASSERT_NOT_REACHED();
    return WebCore::UserContentInjectedFrames::InjectInAllFrames;
}

} // namespace WebKit
