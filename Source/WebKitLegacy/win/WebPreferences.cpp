/*
 * Copyright (C) 2006-2022 Apple Inc.  All rights reserved.
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

#include "WebKit.h"
#include "WebKitDLL.h"
#include "WebPreferences.h"

#include "NetworkStorageSessionMap.h"
#include "WebNotificationCenter.h"
#include "WebPreferenceKeysPrivate.h"
#include "WebPreferencesDefinitions.h"

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/CACFLayerTreeHost.h>
#endif

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/FontCascade.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/NetworkStorageSession.h>
#include <limits>
#include <shlobj.h>
#include <wchar.h>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;
using std::numeric_limits;

static const String& oldPreferencesPath()
{
    static String path = FileSystem::pathByAppendingComponent(FileSystem::roamingUserSpecificStorageDirectory(), "WebKitPreferences.plist"_s);
    return path;
}

#if USE(CF)

template<typename NumberType> struct CFNumberTraits { static const CFNumberType Type; };
template<> struct CFNumberTraits<int> { static const CFNumberType Type = kCFNumberSInt32Type; };
template<> struct CFNumberTraits<LONGLONG> { static const CFNumberType Type = kCFNumberLongLongType; };
template<> struct CFNumberTraits<float> { static const CFNumberType Type = kCFNumberFloat32Type; };

template<typename NumberType>
static NumberType numberValueForPreferencesValue(CFPropertyListRef value)
{
    if (!value)
        return 0;

    CFTypeID cfType = CFGetTypeID(value);
    if (cfType == CFStringGetTypeID())
        return static_cast<NumberType>(CFStringGetIntValue(static_cast<CFStringRef>(value)));
    else if (cfType == CFBooleanGetTypeID()) {
        Boolean boolVal = CFBooleanGetValue(static_cast<CFBooleanRef>(value));
        return boolVal ? 1 : 0;
    } else if (cfType == CFNumberGetTypeID()) {
        NumberType val = 0;
        CFNumberGetValue(static_cast<CFNumberRef>(value), CFNumberTraits<NumberType>::Type, &val);
        return val;
    }

    return 0;
}

template<typename NumberType>
static RetainPtr<CFNumberRef> cfNumber(NumberType value)
{
    return adoptCF(CFNumberCreate(0, CFNumberTraits<NumberType>::Type, &value));
}

static bool booleanValueForPreferencesValue(CFPropertyListRef value)
{
    return numberValueForPreferencesValue<int>(value);
}

static RetainPtr<CFStringRef> stringValueForPreferencesValue(CFPropertyListRef value)
{
    if (!value)
        return nullptr;
    if (CFGetTypeID(value) != CFStringGetTypeID())
        return nullptr;

    return static_cast<CFStringRef>(value);
}

// WebPreferences ----------------------------------------------------------------

static RetainPtr<CFDictionaryRef>& defaultSettings()
{
    static NeverDestroyed<RetainPtr<CFDictionaryRef>> defaultSettings;
    return defaultSettings;
}

RetainPtr<CFStringRef> WebPreferences::m_applicationId = kCFPreferencesCurrentApplication;
#endif

static HashMap<WTF::String, COMPtr<WebPreferences>>& webPreferencesInstances()
{
    static NeverDestroyed<HashMap<WTF::String, COMPtr<WebPreferences>>> webPreferencesInstances;
    return webPreferencesInstances;
}

WebPreferences* WebPreferences::sharedStandardPreferences()
{
    static WebPreferences* standardPreferences;
    if (!standardPreferences) {
        standardPreferences = WebPreferences::createInstance();
        standardPreferences->setAutosaves(TRUE);
        standardPreferences->load();
    }

    return standardPreferences;
}

WebPreferences::WebPreferences()
{
    gClassCount++;
    gClassNameCount().add("WebPreferences"_s);
}

WebPreferences::~WebPreferences()
{
    gClassCount--;
    gClassNameCount().remove("WebPreferences"_s);
}

WebPreferences* WebPreferences::createInstance()
{
    WebPreferences* instance = new WebPreferences();
    instance->AddRef();
    return instance;
}

HRESULT WebPreferences::postPreferencesChangesNotification()
{
    if (m_updateBatchCount) {
        m_needsUpdateAfterBatch = true;
        return S_OK;
    }
    IWebNotificationCenter* nc = WebNotificationCenter::defaultCenterInternal();
    HRESULT hr = nc->postNotificationName(webPreferencesChangedNotification(), static_cast<IWebPreferences*>(this), 0);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

WebPreferences* WebPreferences::getInstanceForIdentifier(BSTR identifier)
{
    if (!identifier)
        return sharedStandardPreferences();

    WTF::String identifierString(identifier, SysStringLen(identifier));
    if (identifierString.isEmpty())
        return sharedStandardPreferences();

    return webPreferencesInstances().get(identifierString);
}

void WebPreferences::setInstance(WebPreferences* instance, BSTR identifier)
{
    if (!identifier || !instance)
        return;
    WTF::String identifierString(identifier, SysStringLen(identifier));
    if (identifierString.isEmpty())
        return;
    webPreferencesInstances().add(identifierString, instance);
}

void WebPreferences::removeReferenceForIdentifier(BSTR identifier)
{
    if (!identifier || webPreferencesInstances().isEmpty())
        return;

    WTF::String identifierString(identifier, SysStringLen(identifier));
    if (identifierString.isEmpty())
        return;
    WebPreferences* webPreference = webPreferencesInstances().get(identifierString);
    if (webPreference && webPreference->m_refCount == 1)
        webPreferencesInstances().remove(identifierString);
}

#if USE(CF)
CFStringRef WebPreferences::applicationId()
{
    return m_applicationId.get();
}
#endif

void WebPreferences::initializeDefaultSettings()
{
#if USE(CF)
    if (defaultSettings())
        return;

    auto defaults = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    INITIALIZE_DEFAULT_PREFERENCES_DICTIONARY_FROM_GENERATED_PREFERENCES;

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitStandardFontPreferenceKey), CFSTR("Times New Roman"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitFixedFontPreferenceKey), CFSTR("Courier New"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitSerifFontPreferenceKey), CFSTR("Times New Roman"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitSansSerifFontPreferenceKey), CFSTR("Arial"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitCursiveFontPreferenceKey), CFSTR("Comic Sans MS"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitFantasyFontPreferenceKey), CFSTR("Comic Sans MS"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitPictographFontPreferenceKey), CFSTR("Segoe UI Symbol"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitMinimumFontSizePreferenceKey), CFSTR("0"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitMinimumLogicalFontSizePreferenceKey), CFSTR("9"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitDefaultFontSizePreferenceKey), CFSTR("16"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitDefaultFixedFontSizePreferenceKey), CFSTR("13"));

    String defaultDefaultEncoding(WEB_UI_STRING("ISO-8859-1", "The default, default character encoding on Windows"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitDefaultTextEncodingNamePreferenceKey), defaultDefaultEncoding.createCFString().get());

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitUserStyleSheetEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitUserStyleSheetLocationPreferenceKey), CFSTR(""));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitShouldPrintBackgroundsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitTextAreasAreResizablePreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitJavaScriptEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitJavaScriptRuntimeFlagsPreferenceKey), CFSTR("0"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitWebSecurityEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitAllowTopNavigationToDataURLsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitWebAudioEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitAllowUniversalAccessFromFileURLsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitAllowFileAccessFromFileURLsPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitJavaScriptCanAccessClipboardPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitFrameFlatteningEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitPluginsEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitCSSRegionsEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitDatabasesEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitLocalStorageEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitExperimentalNotificationsEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitZoomsTextOnlyPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitAllowAnimatedImagesPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitAllowAnimatedImageLoopingPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitDisplayImagesKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitLoadSiteIconsKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitBackForwardCacheExpirationIntervalKey), CFSTR("1800"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitTabToLinksPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitPrivateBrowsingEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitRespectStandardStyleKeyEquivalentsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitShowsURLsInToolTipsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitShowsToolTipOverTruncatedTextPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitPDFDisplayModePreferenceKey), CFSTR("1"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitPDFScaleFactorPreferenceKey), CFSTR("0"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitShouldDisplaySubtitlesPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitShouldDisplayCaptionsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitShouldDisplayTextDescriptionsPreferenceKey), kCFBooleanFalse);

    RetainPtr<CFStringRef> linkBehaviorStringRef = adoptCF(CFStringCreateWithFormat(0, 0, CFSTR("%d"), WebKitEditableLinkDefaultBehavior));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitEditableLinkBehaviorPreferenceKey), linkBehaviorStringRef.get());

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitHistoryItemLimitKey), CFSTR("1000"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitHistoryAgeInDaysLimitKey), CFSTR("7"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitIconDatabaseLocationKey), CFSTR(""));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitIconDatabaseEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitFontSmoothingTypePreferenceKey), CFSTR("2"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitFontSmoothingContrastPreferenceKey), CFSTR("2"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitCookieStorageAcceptPolicyPreferenceKey), CFSTR("2"));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebContinuousSpellCheckingEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebGrammarCheckingEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(AllowContinuousSpellCheckingPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitUsesPageCachePreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitLocalStorageDatabasePathPreferenceKey), CFSTR(""));

    RetainPtr<CFStringRef> cacheModelRef = adoptCF(CFStringCreateWithFormat(0, 0, CFSTR("%d"), WebCacheModelDocumentViewer));
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitCacheModelPreferenceKey), cacheModelRef.get());

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitAuthorAndUserStylesEnabledPreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitOfflineWebApplicationCacheEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitPaintNativeControlsPreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitUseHighResolutionTimersPreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitAcceleratedCompositingEnabledPreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitShowDebugBordersPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitSpatialNavigationEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitDNSPrefetchingEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitHyperlinkAuditingEnabledPreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitMediaPlaybackRequiresUserGesturePreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitMediaPlaybackAllowsInlinePreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitFullScreenEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitRequestAnimationFrameEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitAllowDisplayAndRunningOfInsecureContentPreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitCustomElementsEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitWebAnimationsCompositeOperationsEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitWebAnimationsMutableTimelinesEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitCSSCustomPropertiesAndValuesEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitLinkPreloadEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitMediaPreloadingEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitDataTransferItemsEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitVisualViewportAPIEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitCSSOMViewScrollingAPIEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitResizeObserverEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitCSSOMViewSmoothScrollingEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitCoreMathMLEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitRequestIdleCallbackEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitAsyncClipboardAPIEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitContactPickerAPIEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitAspectRatioOfImgFromWidthAndHeightEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitWebSQLEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitCSSIndividualTransformPropertiesEnabledPreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitSpeechRecognitionEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults.get(), CFSTR(WebKitOverscrollBehaviorEnabledPreferenceKey), kCFBooleanFalse);

    defaultSettings() = WTFMove(defaults);
#endif
}

#if USE(CF)
RetainPtr<CFPropertyListRef> WebPreferences::valueForKey(CFStringRef key)
{
    RetainPtr<CFPropertyListRef> value = CFDictionaryGetValue(m_privatePrefs.get(), key);
    if (value)
        return value;

    value = adoptCF(CFPreferencesCopyAppValue(key, applicationId()));
    if (value)
        return value;

    return CFDictionaryGetValue(defaultSettings().get(), key);
}

void WebPreferences::setValueForKey(CFStringRef key, CFPropertyListRef value)
{
    CFDictionarySetValue(m_privatePrefs.get(), key, value);
    if (m_autoSaves) {
        CFPreferencesSetAppValue(key, value, applicationId());
        save();
    }
}

void WebPreferences::setValueForKey(const char* key, CFPropertyListRef value)
{
    RetainPtr<CFStringRef> cfKey = adoptCF(CFStringCreateWithCString(0, key, kCFStringEncodingASCII));
    setValueForKey(cfKey.get(), value);
}

RetainPtr<CFPropertyListRef> WebPreferences::valueForKey(const char* key)
{
    RetainPtr<CFStringRef> cfKey = adoptCF(CFStringCreateWithCString(0, key, kCFStringEncodingASCII));
    return valueForKey(cfKey.get());
}
#endif

BSTR WebPreferences::stringValueForKey(const char* key)
{
#if USE(CF)
    RetainPtr<CFPropertyListRef> value = valueForKey(key);
    
    if (!value || (CFGetTypeID(value.get()) != CFStringGetTypeID()))
        return 0;

    CFStringRef str = static_cast<CFStringRef>(value.get());

    CFIndex length = CFStringGetLength(str);
    const UniChar* uniChars = CFStringGetCharactersPtr(str);
    if (uniChars)
        return SysAllocStringLen((LPCTSTR)uniChars, length);

    BSTR bstr = SysAllocStringLen(0, length);
    if (!bstr)
        return 0;

    if (!CFStringGetCString(str, (char*)bstr, (length+1)*sizeof(WCHAR), kCFStringEncodingUTF16)) {
        SysFreeString(bstr);
        return 0;
    }
        
    bstr[length] = 0;
    return bstr;
#else
    BString dummy;
    return dummy;
#endif
}

int WebPreferences::integerValueForKey(const char* key)
{
#if USE(CF)
    return numberValueForPreferencesValue<int>(valueForKey(key).get());
#else
    return 0;
#endif
}

BOOL WebPreferences::boolValueForKey(const char* key)
{
#if USE(CF)
    return booleanValueForPreferencesValue(valueForKey(key).get());
#else
    return 0;
#endif
}

float WebPreferences::floatValueForKey(const char* key)
{
#if USE(CF)
    return numberValueForPreferencesValue<float>(valueForKey(key).get());
#else
    return 0;
#endif
}

LONGLONG WebPreferences::longlongValueForKey(const char* key)
{
#if USE(CF)
    return numberValueForPreferencesValue<LONGLONG>(valueForKey(key).get());
#else
    return 0;
#endif
}

void WebPreferences::setStringValue(const char* key, BSTR value)
{
    BString val;
    val.adoptBSTR(stringValueForKey(key));
    if (val && !wcscmp(val, value))
        return;

#if USE(CF)
    RetainPtr<CFStringRef> valueRef = adoptCF(CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar*>(value), static_cast<CFIndex>(wcslen(value))));
    setValueForKey(key, valueRef.get());
#endif

    postPreferencesChangesNotification();
}

void WebPreferences::setIntegerValue(const char* key, int value)
{
    if (integerValueForKey(key) == value)
        return;

#if USE(CF)
    setValueForKey(key, cfNumber(value).get());
#endif

    postPreferencesChangesNotification();
}

void WebPreferences::setFloatValue(const char* key, float value)
{
    if (floatValueForKey(key) == value)
        return;

#if USE(CF)
    setValueForKey(key, cfNumber(value).get());
#endif

    postPreferencesChangesNotification();
}

void WebPreferences::setBoolValue(const char* key, BOOL value)
{
    if (boolValueForKey(key) == value)
        return;

#if USE(CF)
    setValueForKey(key, value ? kCFBooleanTrue : kCFBooleanFalse);
#endif

    postPreferencesChangesNotification();
}

void WebPreferences::setLongLongValue(const char* key, LONGLONG value)
{
    if (longlongValueForKey(key) == value)
        return;

#if USE(CF)
    setValueForKey(key, cfNumber(value).get());
#endif

    postPreferencesChangesNotification();
}

BSTR WebPreferences::webPreferencesChangedNotification()
{
    static BSTR webPreferencesChangedNotification = SysAllocString(WebPreferencesChangedNotification);
    return webPreferencesChangedNotification;
}

BSTR WebPreferences::webPreferencesRemovedNotification()
{
    static BSTR webPreferencesRemovedNotification = SysAllocString(WebPreferencesRemovedNotification);
    return webPreferencesRemovedNotification;
}

void WebPreferences::save()
{
#if USE(CF)
    CFPreferencesAppSynchronize(applicationId());
#endif
}

void WebPreferences::load()
{
    initializeDefaultSettings();

#if USE(CF)
    m_privatePrefs = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    migrateWebKitPreferencesToCFPreferences();
#endif
}

#if USE(CF)
void WebPreferences::migrateWebKitPreferencesToCFPreferences()
{
    if (boolValueForKey(WebKitDidMigrateWebKitPreferencesToCFPreferencesPreferenceKey))
        return;
    bool oldValue = m_autoSaves;
    m_autoSaves = true;
    setBoolValue(WebKitDidMigrateWebKitPreferencesToCFPreferencesPreferenceKey, TRUE);
    m_autoSaves = oldValue;

    WTF::CString path = oldPreferencesPath().utf8();

    RetainPtr<CFURLRef> urlRef = adoptCF(CFURLCreateFromFileSystemRepresentation(0, reinterpret_cast<const UInt8*>(path.data()), path.length(), false));
    if (!urlRef)
        return;

    RetainPtr<CFReadStreamRef> stream = adoptCF(CFReadStreamCreateWithFile(0, urlRef.get()));
    if (!stream)
        return;

    if (!CFReadStreamOpen(stream.get()))
        return;

    auto format = static_cast<CFPropertyListFormat>(kCFPropertyListBinaryFormat_v1_0 | kCFPropertyListXMLFormat_v1_0);
    RetainPtr<CFPropertyListRef> plist = adoptCF(CFPropertyListCreateFromStream(0, stream.get(), 0, kCFPropertyListMutableContainersAndLeaves, &format, 0));
    CFReadStreamClose(stream.get());

    if (!plist || CFGetTypeID(plist.get()) != CFDictionaryGetTypeID())
        return;

    copyWebKitPreferencesToCFPreferences(static_cast<CFDictionaryRef>(plist.get()));

    FileSystem::deleteFile(oldPreferencesPath());
}

void WebPreferences::copyWebKitPreferencesToCFPreferences(CFDictionaryRef dict)
{
    ASSERT_ARG(dict, dict);

    int count = CFDictionaryGetCount(dict);
    if (count <= 0)
        return;

    CFStringRef didRemoveDefaultsKey = CFSTR(WebKitDidMigrateDefaultSettingsFromSafari3BetaPreferenceKey);
    bool omitDefaults = !booleanValueForPreferencesValue(CFDictionaryGetValue(dict, didRemoveDefaultsKey));

    Vector<CFTypeRef> keys(count);
    Vector<CFTypeRef> values(count);
    CFDictionaryGetKeysAndValues(dict, keys.data(), values.data());

    for (int i = 0; i < count; ++i) {
        if (!keys[i] || !values[i] || CFGetTypeID(keys[i]) != CFStringGetTypeID())
            continue;

        if (omitDefaults) {
            CFTypeRef defaultValue = CFDictionaryGetValue(defaultSettings().get(), keys[i]);
            if (defaultValue && CFEqual(defaultValue, values[i]))
                continue;
        }

        setValueForKey(static_cast<CFStringRef>(keys[i]), values[i]);
    }
}
#endif

// IUnknown -------------------------------------------------------------------

HRESULT WebPreferences::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebPreferences*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferences))
        *ppvObject = static_cast<IWebPreferences*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferencesPrivate))
        *ppvObject = static_cast<IWebPreferencesPrivate*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferencesPrivate2))
        *ppvObject = static_cast<IWebPreferencesPrivate2*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferencesPrivate3))
        *ppvObject = static_cast<IWebPreferencesPrivate3*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferencesPrivate4))
        *ppvObject = static_cast<IWebPreferencesPrivate4*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferencesPrivate5))
        *ppvObject = static_cast<IWebPreferencesPrivate5*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferencesPrivate6))
        *ppvObject = static_cast<IWebPreferencesPrivate6*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferencesPrivate7))
        *ppvObject = static_cast<IWebPreferencesPrivate7*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferencesPrivate8))
        *ppvObject = static_cast<IWebPreferencesPrivate8*>(this);
    else if (IsEqualGUID(riid, CLSID_WebPreferences))
        *ppvObject = this;
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebPreferences::AddRef()
{
    return ++m_refCount;
}

ULONG WebPreferences::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebPreferences ------------------------------------------------------------

HRESULT WebPreferences::standardPreferences(_COM_Outptr_opt_ IWebPreferences** standardPreferences)
{
    if (!standardPreferences)
        return E_POINTER;
    *standardPreferences = sharedStandardPreferences();
    (*standardPreferences)->AddRef();
    return S_OK;
}

HRESULT WebPreferences::initWithIdentifier(_In_ BSTR anIdentifier, _COM_Outptr_opt_ IWebPreferences** preferences)
{
    WebPreferences *instance = getInstanceForIdentifier(anIdentifier);
    if (instance) {
        *preferences = instance;
        instance->AddRef();
        return S_OK;
    }

    load();

    *preferences = this;
    AddRef();

    if (anIdentifier) {
        m_identifier = anIdentifier;
        setInstance(this, m_identifier);
    }

    this->postPreferencesChangesNotification();

    return S_OK;
}

HRESULT WebPreferences::identifier(__deref_opt_out BSTR* ident)
{
    if (!ident)
        return E_POINTER;
    *ident = m_identifier ? SysAllocString(m_identifier) : nullptr;
    return S_OK;
}

HRESULT WebPreferences::standardFontFamily(__deref_opt_out BSTR* family)
{
    if (!family)
        return E_POINTER;
    *family = stringValueForKey(WebKitStandardFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setStandardFontFamily(_In_ BSTR family)
{
    setStringValue(WebKitStandardFontPreferenceKey, family);
    return S_OK;
}

HRESULT WebPreferences::fixedFontFamily(__deref_opt_out BSTR* family)
{
    if (!family)
        return E_POINTER;
    *family = stringValueForKey(WebKitFixedFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setFixedFontFamily(_In_ BSTR family)
{
    setStringValue(WebKitFixedFontPreferenceKey, family);
    return S_OK;
}

HRESULT WebPreferences::serifFontFamily(__deref_opt_out BSTR* fontFamily)
{
    if (!fontFamily)
        return E_POINTER;
    *fontFamily = stringValueForKey(WebKitSerifFontPreferenceKey);
    return (*fontFamily) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setSerifFontFamily(_In_ BSTR family)
{
    setStringValue(WebKitSerifFontPreferenceKey, family);
    return S_OK;
}

HRESULT WebPreferences::sansSerifFontFamily(__deref_opt_out BSTR* family)
{
    if (!family)
        return E_POINTER;
    *family = stringValueForKey(WebKitSansSerifFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setSansSerifFontFamily(_In_ BSTR family)
{
    setStringValue(WebKitSansSerifFontPreferenceKey, family);
    return S_OK;
}

HRESULT WebPreferences::cursiveFontFamily(__deref_opt_out BSTR* family)
{
    if (!family)
        return E_POINTER;
    *family = stringValueForKey(WebKitCursiveFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setCursiveFontFamily(_In_ BSTR family)
{
    setStringValue(WebKitCursiveFontPreferenceKey, family);
    return S_OK;
}

HRESULT WebPreferences::fantasyFontFamily(__deref_opt_out BSTR* family)
{
    if (!family)
        return E_POINTER;
    *family = stringValueForKey(WebKitFantasyFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setFantasyFontFamily(_In_ BSTR family)
{
    setStringValue(WebKitFantasyFontPreferenceKey, family);
    return S_OK;
}

HRESULT WebPreferences::pictographFontFamily(__deref_opt_out BSTR* family)
{
    if (!family)
        return E_POINTER;
    *family = stringValueForKey(WebKitPictographFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setPictographFontFamily(_In_ BSTR family)
{
    setStringValue(WebKitPictographFontPreferenceKey, family);
    return S_OK;
}

HRESULT WebPreferences::defaultFontSize(_Out_  int* fontSize)
{
    if (!fontSize)
        return E_POINTER;
    *fontSize = integerValueForKey(WebKitDefaultFontSizePreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setDefaultFontSize(int fontSize)
{
    setIntegerValue(WebKitDefaultFontSizePreferenceKey, fontSize);
    return S_OK;
}

HRESULT WebPreferences::defaultFixedFontSize(_Out_ int* fontSize)
{
    if (!fontSize)
        return E_POINTER;
    *fontSize = integerValueForKey(WebKitDefaultFixedFontSizePreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setDefaultFixedFontSize(int fontSize)
{
    setIntegerValue(WebKitDefaultFixedFontSizePreferenceKey, fontSize);
    return S_OK;
}

HRESULT WebPreferences::minimumFontSize(_Out_ int* fontSize)
{
    if (!fontSize)
        return E_POINTER;
    *fontSize = integerValueForKey(WebKitMinimumFontSizePreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setMinimumFontSize(int fontSize)
{
    setIntegerValue(WebKitMinimumFontSizePreferenceKey, fontSize);
    return S_OK;
}

HRESULT WebPreferences::minimumLogicalFontSize(_Out_ int* fontSize)
{
    if (!fontSize)
        return E_POINTER;
    *fontSize = integerValueForKey(WebKitMinimumLogicalFontSizePreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setMinimumLogicalFontSize(int fontSize)
{
    setIntegerValue(WebKitMinimumLogicalFontSizePreferenceKey, fontSize);
    return S_OK;
}

HRESULT WebPreferences::defaultTextEncodingName(__deref_opt_out BSTR* name)
{
    if (!name)
        return E_POINTER;
    *name = stringValueForKey(WebKitDefaultTextEncodingNamePreferenceKey);
    return (*name) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setDefaultTextEncodingName(_In_ BSTR name)
{
    setStringValue(WebKitDefaultTextEncodingNamePreferenceKey, name);
    return S_OK;
}

HRESULT WebPreferences::userStyleSheetEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitUserStyleSheetEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setUserStyleSheetEnabled(BOOL enabled)
{
    setBoolValue(WebKitUserStyleSheetEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::userStyleSheetLocation(__deref_opt_out BSTR* location)
{
    if (!location)
        return E_POINTER;
    *location = stringValueForKey(WebKitUserStyleSheetLocationPreferenceKey);
    return (*location) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setUserStyleSheetLocation(_In_ BSTR location)
{
    setStringValue(WebKitUserStyleSheetLocationPreferenceKey, location);
    return S_OK;
}

HRESULT WebPreferences::isJavaEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = FALSE;
    return S_OK;
}

HRESULT WebPreferences::setJavaEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::isJavaScriptEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitJavaScriptEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setJavaScriptEnabled(BOOL enabled)
{
    setBoolValue(WebKitJavaScriptEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::javaScriptRuntimeFlags(_Out_ unsigned* flags)
{
    if (!flags)
        return E_POINTER;
    *flags = static_cast<unsigned>(integerValueForKey(WebKitJavaScriptRuntimeFlagsPreferenceKey));
    return S_OK;
}

HRESULT WebPreferences::setJavaScriptRuntimeFlags(unsigned flags)
{
    setIntegerValue(WebKitJavaScriptRuntimeFlagsPreferenceKey, static_cast<int>(flags));
    return S_OK;
}

HRESULT WebPreferences::isWebSecurityEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitWebSecurityEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setWebSecurityEnabled(BOOL enabled)
{
    setBoolValue(WebKitWebSecurityEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::allowUniversalAccessFromFileURLs(_Out_ BOOL* allowAccess)
{
    if (!allowAccess)
        return E_POINTER;
    *allowAccess = boolValueForKey(WebKitAllowUniversalAccessFromFileURLsPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAllowUniversalAccessFromFileURLs(BOOL allowAccess)
{
    setBoolValue(WebKitAllowUniversalAccessFromFileURLsPreferenceKey, allowAccess);
    return S_OK;
}

HRESULT WebPreferences::allowFileAccessFromFileURLs(_Out_ BOOL* allowAccess)
{
    if (!allowAccess)
        return E_POINTER;
    *allowAccess = boolValueForKey(WebKitAllowFileAccessFromFileURLsPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAllowFileAccessFromFileURLs(BOOL allowAccess)
{
    setBoolValue(WebKitAllowFileAccessFromFileURLsPreferenceKey, allowAccess);
    return S_OK;
}

HRESULT WebPreferences::javaScriptCanAccessClipboard(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitJavaScriptCanAccessClipboardPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setJavaScriptCanAccessClipboard(BOOL enabled)
{
    setBoolValue(WebKitJavaScriptCanAccessClipboardPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::isXSSAuditorEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = FALSE;
    return S_OK;
}

HRESULT WebPreferences::setXSSAuditorEnabled(BOOL enabled)
{
    UNUSED_PARAM(enabled);
    return S_OK;
}

HRESULT WebPreferences::isFrameFlatteningEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitFrameFlatteningEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setFrameFlatteningEnabled(BOOL enabled)
{
    setBoolValue(WebKitFrameFlatteningEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::javaScriptCanOpenWindowsAutomatically(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setJavaScriptCanOpenWindowsAutomatically(BOOL enabled)
{
    setBoolValue(WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::arePlugInsEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitPluginsEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setPlugInsEnabled(BOOL enabled)
{
    setBoolValue(WebKitPluginsEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::isCSSRegionsEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitCSSRegionsEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setCSSRegionsEnabled(BOOL enabled)
{
    setBoolValue(WebKitCSSRegionsEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::allowsAnimatedImages(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitAllowAnimatedImagesPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAllowsAnimatedImages(BOOL enabled)
{
    setBoolValue(WebKitAllowAnimatedImagesPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::allowAnimatedImageLooping(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitAllowAnimatedImageLoopingPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAllowAnimatedImageLooping(BOOL enabled)
{
    setBoolValue(WebKitAllowAnimatedImageLoopingPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::setLoadsImagesAutomatically(BOOL enabled)
{
    setBoolValue(WebKitDisplayImagesKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::loadsImagesAutomatically(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitDisplayImagesKey);
    return S_OK;
}

HRESULT WebPreferences::setLoadsSiteIconsIgnoringImageLoadingPreference(BOOL enabled)
{
    setBoolValue(WebKitLoadSiteIconsKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::loadsSiteIconsIgnoringImageLoadingPreference(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitLoadSiteIconsKey);
    return S_OK;
}

HRESULT WebPreferences::setHixie76WebSocketProtocolEnabled(BOOL enabled)
{
    return S_OK;
}

HRESULT WebPreferences::hixie76WebSocketProtocolEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = false;
    return S_OK;
}

HRESULT WebPreferences::setMediaPlaybackRequiresUserGesture(BOOL enabled)
{
    setBoolValue(WebKitMediaPlaybackRequiresUserGesturePreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::mediaPlaybackRequiresUserGesture(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitMediaPlaybackRequiresUserGesturePreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setMediaPlaybackAllowsInline(BOOL enabled)
{
    setBoolValue(WebKitMediaPlaybackAllowsInlinePreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::mediaPlaybackAllowsInline(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitMediaPlaybackAllowsInlinePreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAutosaves(BOOL enabled)
{
    m_autoSaves = !!enabled;
    return S_OK;
}

HRESULT WebPreferences::autosaves(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = m_autoSaves ? TRUE : FALSE;
    return S_OK;
}

HRESULT WebPreferences::setShouldPrintBackgrounds(BOOL enabled)
{
    setBoolValue(WebKitShouldPrintBackgroundsPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::shouldPrintBackgrounds(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitShouldPrintBackgroundsPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setPrivateBrowsingEnabled(BOOL enabled)
{
    setBoolValue(WebKitPrivateBrowsingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::privateBrowsingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitPrivateBrowsingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setTabsToLinks(BOOL enabled)
{
    setBoolValue(WebKitTabToLinksPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::tabsToLinks(_Out_ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitTabToLinksPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setUsesPageCache(BOOL usesPageCache)
{
    setBoolValue(WebKitUsesPageCachePreferenceKey, usesPageCache);
    return S_OK;
}

HRESULT WebPreferences::usesPageCache(_Out_ BOOL* usesPageCache)
{
    if (!usesPageCache)
        return E_POINTER;
    *usesPageCache = boolValueForKey(WebKitUsesPageCachePreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::textAreasAreResizable(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitTextAreasAreResizablePreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setTextAreasAreResizable(BOOL enabled)
{
    setBoolValue(WebKitTextAreasAreResizablePreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::historyItemLimit(_Out_ int* limit)
{
    if (!limit)
        return E_POINTER;
    *limit = integerValueForKey(WebKitHistoryItemLimitKey);
    return S_OK;
}

HRESULT WebPreferences::setHistoryItemLimit(int limit)
{
    setIntegerValue(WebKitHistoryItemLimitKey, limit);
    return S_OK;
}

HRESULT WebPreferences::historyAgeInDaysLimit(_Out_ int* limit)
{
    if (!limit)
        return E_POINTER;
    *limit = integerValueForKey(WebKitHistoryAgeInDaysLimitKey);
    return S_OK;
}

HRESULT WebPreferences::setHistoryAgeInDaysLimit(int limit)
{
    setIntegerValue(WebKitHistoryAgeInDaysLimitKey, limit);
    return S_OK;
}

HRESULT WebPreferences::unused1()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT WebPreferences::unused2()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT WebPreferences::iconDatabaseLocation(__deref_opt_out BSTR* location)
{
    if (!location)
        return E_POINTER;
    *location = stringValueForKey(WebKitIconDatabaseLocationKey);
    return (*location) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setIconDatabaseLocation(_In_ BSTR location)
{
    setStringValue(WebKitIconDatabaseLocationKey, location);
    return S_OK;
}

HRESULT WebPreferences::iconDatabaseEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitIconDatabaseEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setIconDatabaseEnabled(BOOL enabled)
{
    setBoolValue(WebKitIconDatabaseEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::fontSmoothing(_Out_ FontSmoothingType* smoothingType)
{
    if (!smoothingType)
        return E_POINTER;
    *smoothingType = static_cast<FontSmoothingType>(integerValueForKey(WebKitFontSmoothingTypePreferenceKey));
    return S_OK;
}

HRESULT WebPreferences::setFontSmoothing(FontSmoothingType smoothingType)
{
    setIntegerValue(WebKitFontSmoothingTypePreferenceKey, smoothingType);
    if (smoothingType == FontSmoothingTypeWindows)
        smoothingType = FontSmoothingTypeMedium;
#if USE(CG)
    FontCascade::setFontSmoothingLevel((int)smoothingType);
#endif
    return S_OK;
}

HRESULT WebPreferences::fontSmoothingContrast(_Out_ float* contrast)
{
    if (!contrast)
        return E_POINTER;
    *contrast = floatValueForKey(WebKitFontSmoothingContrastPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setFontSmoothingContrast(float contrast)
{
    setFloatValue(WebKitFontSmoothingContrastPreferenceKey, contrast);
#if USE(CG)
    FontCascade::setFontSmoothingContrast(contrast);
#endif
    return S_OK;
}

HRESULT WebPreferences::editableLinkBehavior(_Out_ WebKitEditableLinkBehavior* editableLinkBehavior)
{
    if (!editableLinkBehavior)
        return E_POINTER;
    WebKitEditableLinkBehavior value = static_cast<WebKitEditableLinkBehavior>(integerValueForKey(WebKitEditableLinkBehaviorPreferenceKey));
    switch (value) {
        case WebKitEditableLinkDefaultBehavior:
        case WebKitEditableLinkAlwaysLive:
        case WebKitEditableLinkOnlyLiveWithShiftKey:
        case WebKitEditableLinkLiveWhenNotFocused:
        case WebKitEditableLinkNeverLive:
            *editableLinkBehavior = value;
            break;
        default: // ensure that a valid result is returned
            *editableLinkBehavior = WebKitEditableLinkDefaultBehavior;
            break;
    }
    return S_OK;
}

HRESULT WebPreferences::setEditableLinkBehavior(WebKitEditableLinkBehavior behavior)
{
    setIntegerValue(WebKitEditableLinkBehaviorPreferenceKey, behavior);
    return S_OK;
}

HRESULT WebPreferences::unused5()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT WebPreferences::unused6()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT WebPreferences::mockScrollbarsEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitMockScrollbarsEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setMockScrollbarsEnabled(BOOL enabled)
{
    setBoolValue(WebKitMockScrollbarsEnabledPreferenceKey, enabled);
    return S_OK;
}

// These two methods are no-ops, and only retained to keep
// the Interface consistent. DO NOT USE THEM.
HRESULT WebPreferences::screenFontSubstitutionEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = false;
    return S_OK;
}

HRESULT WebPreferences::setScreenFontSubstitutionEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::hyperlinkAuditingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitHyperlinkAuditingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setHyperlinkAuditingEnabled(BOOL enabled)
{
    setBoolValue(WebKitHyperlinkAuditingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::cookieStorageAcceptPolicy(_Out_ WebKitCookieStorageAcceptPolicy* acceptPolicy)
{
    if (!acceptPolicy)
        return E_POINTER;

    *acceptPolicy = static_cast<WebKitCookieStorageAcceptPolicy>(integerValueForKey(WebKitCookieStorageAcceptPolicyPreferenceKey));
    return S_OK;
}

HRESULT WebPreferences::setCookieStorageAcceptPolicy(WebKitCookieStorageAcceptPolicy acceptPolicy)
{
    setIntegerValue(WebKitCookieStorageAcceptPolicyPreferenceKey, acceptPolicy);
    return S_OK;
}


HRESULT WebPreferences::continuousSpellCheckingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebContinuousSpellCheckingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setContinuousSpellCheckingEnabled(BOOL enabled)
{
    setBoolValue(WebContinuousSpellCheckingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::grammarCheckingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebGrammarCheckingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setGrammarCheckingEnabled(BOOL enabled)
{
    setBoolValue(WebGrammarCheckingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::allowContinuousSpellChecking(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(AllowContinuousSpellCheckingPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAllowContinuousSpellChecking(BOOL enabled)
{
    setBoolValue(AllowContinuousSpellCheckingPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::unused7()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT WebPreferences::unused8()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT WebPreferences::isDOMPasteAllowed(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitDOMPasteAllowedPreferenceKey);
    return S_OK;
}
    
HRESULT WebPreferences::setDOMPasteAllowed(BOOL enabled)
{
    setBoolValue(WebKitDOMPasteAllowedPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::cacheModel(_Out_ WebCacheModel* cacheModel)
{
    if (!cacheModel)
        return E_POINTER;

    *cacheModel = static_cast<WebCacheModel>(integerValueForKey(WebKitCacheModelPreferenceKey));
    return S_OK;
}

HRESULT WebPreferences::setCacheModel(WebCacheModel cacheModel)
{
    setIntegerValue(WebKitCacheModelPreferenceKey, cacheModel);
    return S_OK;
}

HRESULT WebPreferences::unused3()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT WebPreferences::unused4()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

// These two methods are no-ops, and only retained to keep
// the Interface consistent. DO NOT USE THEM.
HRESULT WebPreferences::shouldPaintNativeControls(_Out_ BOOL* enable)
{
    if (!enable)
        return E_POINTER;
    *enable = FALSE;
    return S_OK;
}

HRESULT WebPreferences::setShouldPaintNativeControls(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::setDeveloperExtrasEnabled(BOOL enabled)
{
    setBoolValue(WebKitDeveloperExtrasEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::developerExtrasEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;

    *enabled = boolValueForKey(WebKitDeveloperExtrasEnabledPreferenceKey);
    return S_OK;
}

bool WebPreferences::developerExtrasDisabledByOverride()
{
    return !!boolValueForKey(DisableWebKitDeveloperExtrasPreferenceKey);
}

HRESULT WebPreferences::setAutomaticallyDetectsCacheModel(BOOL automaticallyDetectsCacheModel)
{
    m_automaticallyDetectsCacheModel = !!automaticallyDetectsCacheModel;
    return S_OK;
}

HRESULT WebPreferences::automaticallyDetectsCacheModel(_Out_ BOOL* automaticallyDetectsCacheModel)
{
    if (!automaticallyDetectsCacheModel)
        return E_POINTER;

    *automaticallyDetectsCacheModel = m_automaticallyDetectsCacheModel;
    return S_OK;
}

HRESULT WebPreferences::setAuthorAndUserStylesEnabled(BOOL enabled)
{
    setBoolValue(WebKitAuthorAndUserStylesEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::authorAndUserStylesEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;

    *enabled = boolValueForKey(WebKitAuthorAndUserStylesEnabledPreferenceKey);
    return S_OK;
}

// These two methods are no-ops, and only retained to keep
// the Interface consistent. DO NOT USE THEM.
HRESULT WebPreferences::inApplicationChromeMode(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = FALSE;
    return S_OK;
}

HRESULT WebPreferences::setApplicationChromeMode(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::setOfflineWebApplicationCacheEnabled(BOOL enabled)
{
    setBoolValue(WebKitOfflineWebApplicationCacheEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::offlineWebApplicationCacheEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitOfflineWebApplicationCacheEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setDatabasesEnabled(BOOL enabled)
{
    setBoolValue(WebKitDatabasesEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::databasesEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitDatabasesEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setLocalStorageEnabled(BOOL enabled)
{
    setBoolValue(WebKitLocalStorageEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::localStorageEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitLocalStorageEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::localStorageDatabasePath(__deref_opt_out BSTR* location)
{
    if (!location)
        return E_POINTER;
    *location = stringValueForKey(WebKitLocalStorageDatabasePathPreferenceKey);
    return (*location) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setLocalStorageDatabasePath(_In_ BSTR location)
{
    setStringValue(WebKitLocalStorageDatabasePathPreferenceKey, location);
    return S_OK;
}

HRESULT WebPreferences::setExperimentalNotificationsEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::experimentalNotificationsEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = true;
    return S_OK;
}

HRESULT WebPreferences::setZoomsTextOnly(BOOL zoomsTextOnly)
{
    setBoolValue(WebKitZoomsTextOnlyPreferenceKey, zoomsTextOnly);
    return S_OK;
}

HRESULT WebPreferences::zoomsTextOnly(_Out_ BOOL* zoomsTextOnly)
{
    if (!zoomsTextOnly)
        return E_POINTER;
    *zoomsTextOnly = boolValueForKey(WebKitZoomsTextOnlyPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setShouldUseHighResolutionTimers(BOOL useHighResolutionTimers)
{
    setBoolValue(WebKitUseHighResolutionTimersPreferenceKey, useHighResolutionTimers);
    return S_OK;
}

HRESULT WebPreferences::shouldUseHighResolutionTimers(_Out_ BOOL* useHighResolutionTimers)
{
    if (!useHighResolutionTimers)
        return E_POINTER;
    *useHighResolutionTimers = boolValueForKey(WebKitUseHighResolutionTimersPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setPreferenceForTest(_In_ BSTR key, _In_ BSTR value)
{
    if (!SysStringLen(key) || !SysStringLen(value))
        return E_FAIL;
#if USE(CF)
    RetainPtr<CFStringRef> keyString = adoptCF(CFStringCreateWithCharacters(0, reinterpret_cast<UniChar*>(key), SysStringLen(key)));
    RetainPtr<CFStringRef> valueString = adoptCF(CFStringCreateWithCharacters(0, reinterpret_cast<UniChar*>(value), SysStringLen(value)));
    setValueForKey(keyString.get(), valueString.get());
#endif
    postPreferencesChangesNotification();
    return S_OK;
}

HRESULT WebPreferences::setAcceleratedCompositingEnabled(BOOL enabled)
{
    setBoolValue(WebKitAcceleratedCompositingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::acceleratedCompositingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitAcceleratedCompositingEnabledPreferenceKey);
#if USE(CA)
    *enabled = *enabled && CACFLayerTreeHost::acceleratedCompositingAvailable();
#endif
    return S_OK;
}

HRESULT WebPreferences::showDebugBorders(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitShowDebugBordersPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setShowDebugBorders(BOOL enabled)
{
    setBoolValue(WebKitShowDebugBordersPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::showRepaintCounter(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitShowRepaintCounterPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setShowRepaintCounter(BOOL enabled)
{
    setBoolValue(WebKitShowRepaintCounterPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::setCustomDragCursorsEnabled(BOOL enabled)
{
    setBoolValue(WebKitCustomDragCursorsEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::customDragCursorsEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitCustomDragCursorsEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::spatialNavigationEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitSpatialNavigationEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setSpatialNavigationEnabled(BOOL enabled)
{
    setBoolValue(WebKitSpatialNavigationEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::setDNSPrefetchingEnabled(BOOL enabled)
{
    setBoolValue(WebKitDNSPrefetchingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::isDNSPrefetchingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitDNSPrefetchingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::isFullScreenEnabled(_Out_ BOOL* enabled)
{
#if ENABLE(FULLSCREEN_API)
    if (!enabled)
        return E_POINTER;

    *enabled = boolValueForKey(WebKitFullScreenEnabledPreferenceKey);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::setFullScreenEnabled(BOOL enabled)
{
#if ENABLE(FULLSCREEN_API)
    setBoolValue(WebKitFullScreenEnabledPreferenceKey, enabled);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::avFoundationEnabled(_Out_ BOOL* enabled)
{
#if USE(AVFOUNDATION)
    if (!enabled)
        return E_POINTER;

    *enabled = boolValueForKey(WebKitAVFoundationEnabledPreferenceKey);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::setAVFoundationEnabled(BOOL enabled)
{
#if USE(AVFOUNDATION)
    setBoolValue(WebKitAVFoundationEnabledPreferenceKey, enabled);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::showsToolTipOverTruncatedText(_Out_ BOOL* showsToolTip)
{
    if (!showsToolTip)
        return E_POINTER;

    *showsToolTip = boolValueForKey(WebKitShowsToolTipOverTruncatedTextPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setShowsToolTipOverTruncatedText(BOOL showsToolTip)
{
    setBoolValue(WebKitShowsToolTipOverTruncatedTextPreferenceKey, showsToolTip);
    return S_OK;
}

HRESULT WebPreferences::shouldInvertColors(_Out_ BOOL* shouldInvertColors)
{
    if (!shouldInvertColors)
        return E_POINTER;

    *shouldInvertColors = boolValueForKey(WebKitShouldInvertColorsPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setShouldInvertColors(BOOL shouldInvertColors)
{
    setBoolValue(WebKitShouldInvertColorsPreferenceKey, shouldInvertColors);
    return S_OK;
}

void WebPreferences::willAddToWebView()
{
    ++m_numWebViews;
}

void WebPreferences::didRemoveFromWebView()
{
    ASSERT(m_numWebViews);
    if (--m_numWebViews == 0) {
        IWebNotificationCenter* nc = WebNotificationCenter::defaultCenterInternal();
        nc->postNotificationName(webPreferencesRemovedNotification(), static_cast<IWebPreferences*>(this), 0);
    }
}

HRESULT WebPreferences::shouldDisplaySubtitles(_Out_ BOOL* enabled)
{
#if ENABLE(VIDEO)
    if (!enabled)
        return E_POINTER;

    *enabled = boolValueForKey(WebKitShouldDisplaySubtitlesPreferenceKey);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::setShouldDisplaySubtitles(BOOL enabled)
{
#if ENABLE(VIDEO)
    setBoolValue(WebKitShouldDisplaySubtitlesPreferenceKey, enabled);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::shouldDisplayCaptions(_Out_ BOOL* enabled)
{
#if ENABLE(VIDEO)
    if (!enabled)
        return E_POINTER;

    *enabled = boolValueForKey(WebKitShouldDisplayCaptionsPreferenceKey);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::setShouldDisplayCaptions(BOOL enabled)
{
#if ENABLE(VIDEO)
    setBoolValue(WebKitShouldDisplayCaptionsPreferenceKey, enabled);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::shouldDisplayTextDescriptions(_Out_ BOOL* enabled)
{
#if ENABLE(VIDEO)
    if (!enabled)
        return E_POINTER;

    *enabled = boolValueForKey(WebKitShouldDisplayTextDescriptionsPreferenceKey);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::setShouldDisplayTextDescriptions(BOOL enabled)
{
#if ENABLE(VIDEO)
    setBoolValue(WebKitShouldDisplayTextDescriptionsPreferenceKey, enabled);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::setRequestAnimationFrameEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::requestAnimationFrameEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = true;
    return S_OK;
}

HRESULT WebPreferences::isInheritURIQueryComponentEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitEnableInheritURIQueryComponentPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setEnableInheritURIQueryComponent(BOOL enabled)
{
    setBoolValue(WebKitEnableInheritURIQueryComponentPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::allowDisplayAndRunningOfInsecureContent(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitAllowDisplayAndRunningOfInsecureContentPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAllowDisplayAndRunningOfInsecureContent(BOOL enabled)
{
    setBoolValue(WebKitAllowDisplayAndRunningOfInsecureContentPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::showTiledScrollingIndicator(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitShowTiledScrollingIndicatorPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setShowTiledScrollingIndicator(BOOL enabled)
{
    setBoolValue(WebKitShowTiledScrollingIndicatorPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::fetchAPIEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = true;
    return S_OK;
}

HRESULT WebPreferences::setFetchAPIEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::shadowDOMEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = true;
    return S_OK;
}

HRESULT WebPreferences::setShadowDOMEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::customElementsEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = true;
    return S_OK;
}

HRESULT WebPreferences::setCustomElementsEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::menuItemElementEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitMenuItemElementEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setMenuItemElementEnabled(BOOL enabled)
{
    setBoolValue(WebKitMenuItemElementEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::keygenElementEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = false;
    return S_OK;
}

HRESULT WebPreferences::setKeygenElementEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::crossOriginWindowPolicySupportEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = false;
    return S_OK;
}

HRESULT WebPreferences::setCrossOriginWindowPolicySupportEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::fetchAPIKeepAliveEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitFetchAPIKeepAliveEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setFetchAPIKeepAliveEnabled(BOOL enabled)
{
    setBoolValue(WebKitFetchAPIKeepAliveEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::setLinkPreloadEnabled(BOOL enabled)
{
    setBoolValue(WebKitLinkPreloadEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::linkPreloadEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitLinkPreloadEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setMediaPreloadingEnabled(BOOL enabled)
{
    setBoolValue(WebKitMediaPreloadingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::mediaPreloadingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitMediaPreloadingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::clearNetworkLoaderSession()
{
    NetworkStorageSessionMap::defaultStorageSession().deleteAllCookies([] { });
    return S_OK;
}

HRESULT WebPreferences::switchNetworkLoaderToNewTestingSession()
{
    NetworkStorageSessionMap::switchToNewTestingSession();
    return S_OK;
}

HRESULT WebPreferences::setIsSecureContextAttributeEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::isSecureContextAttributeEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = true;
    return S_OK;
}

HRESULT WebPreferences::dataTransferItemsEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitDataTransferItemsEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setDataTransferItemsEnabled(BOOL enabled)
{
    setBoolValue(WebKitDataTransferItemsEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::visualViewportAPIEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitVisualViewportAPIEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setVisualViewportAPIEnabled(BOOL enabled)
{
    setBoolValue(WebKitVisualViewportAPIEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::CSSOMViewScrollingAPIEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitCSSOMViewScrollingAPIEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setCSSOMViewScrollingAPIEnabled(BOOL enabled)
{
    setBoolValue(WebKitCSSOMViewScrollingAPIEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::coreMathMLEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitCoreMathMLEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setCoreMathMLEnabled(BOOL enabled)
{
    setBoolValue(WebKitCoreMathMLEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::CSSOMViewSmoothScrollingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitCSSOMViewSmoothScrollingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setCSSOMViewSmoothScrollingEnabled(BOOL enabled)
{
    setBoolValue(WebKitCSSOMViewSmoothScrollingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::requestIdleCallbackEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitRequestIdleCallbackEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setRequestIdleCallbackEnabled(BOOL enabled)
{
    setBoolValue(WebKitRequestIdleCallbackEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::asyncClipboardAPIEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitAsyncClipboardAPIEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAsyncClipboardAPIEnabled(BOOL enabled)
{
    setBoolValue(WebKitAsyncClipboardAPIEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::contactPickerAPIEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitContactPickerAPIEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setContactPickerAPIEnabled(BOOL enabled)
{
    setBoolValue(WebKitContactPickerAPIEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::setBoolPreferenceForTesting(_In_ BSTR key, _In_ BOOL value)
{
    if (!SysStringLen(key))
        return E_FAIL;

#if USE(CF)
    auto keyString = String(key).createCFString();
    if (booleanValueForPreferencesValue(valueForKey(keyString.get()).get()) == !!value)
        return S_OK;
    setValueForKey(keyString.get(), value ? kCFBooleanTrue : kCFBooleanFalse);
#endif

    postPreferencesChangesNotification();

    return S_OK;
}

HRESULT WebPreferences::setUInt32PreferenceForTesting(_In_ BSTR key, _In_ unsigned value)
{
    if (!SysStringLen(key))
        return E_FAIL;

#if USE(CF)
    auto keyString = String(key).createCFString();
    if (numberValueForPreferencesValue<int>(valueForKey(keyString.get()).get()) == value)
        return S_OK;
    setValueForKey(keyString.get(), cfNumber(static_cast<int>(value)).get());
#endif

    postPreferencesChangesNotification();

    return S_OK;
}

HRESULT WebPreferences::setDoublePreferenceForTesting(_In_ BSTR key, _In_ double value)
{
    if (!SysStringLen(key))
        return E_FAIL;

#if USE(CF)
    auto keyString = String(key).createCFString();
    if (numberValueForPreferencesValue<float>(valueForKey(keyString.get()).get()) == value)
        return S_OK;
    setValueForKey(keyString.get(), cfNumber(static_cast<float>(value)).get());
#endif

    postPreferencesChangesNotification();

    return S_OK;
}

HRESULT WebPreferences::setStringPreferenceForTesting(_In_ BSTR key, _In_ BSTR value)
{
    if (!SysStringLen(key) || !SysStringLen(value))
        return E_FAIL;

#if USE(CF)
    auto keyString = String(key).createCFString();
    auto valueString = String(value).createCFString();
    if (!CFStringCompare(stringValueForPreferencesValue(valueForKey(keyString.get()).get()).get(), valueString.get(), 0))
        return S_OK;
    setValueForKey(keyString.get(), valueString.get());
#endif

    postPreferencesChangesNotification();

    return S_OK;
}

HRESULT WebPreferences::setApplicationId(BSTR applicationId)
{
#if USE(CF)
    m_applicationId = String(applicationId).createCFString();
#endif
    return S_OK;
}

HRESULT WebPreferences::setWebAnimationsCompositeOperationsEnabled(BOOL enabled)
{
    setBoolValue(WebKitWebAnimationsCompositeOperationsEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::webAnimationsCompositeOperationsEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitWebAnimationsCompositeOperationsEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setWebAnimationsMutableTimelinesEnabled(BOOL enabled)
{
    setBoolValue(WebKitWebAnimationsMutableTimelinesEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::webAnimationsMutableTimelinesEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitWebAnimationsMutableTimelinesEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setCSSCustomPropertiesAndValuesEnabled(BOOL enabled)
{
    setBoolValue(WebKitCSSCustomPropertiesAndValuesEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::CSSCustomPropertiesAndValuesEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitCSSCustomPropertiesAndValuesEnabledPreferenceKey);
    return S_OK;
}
    
HRESULT WebPreferences::setUserTimingEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::userTimingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = true;
    return S_OK;
}

HRESULT WebPreferences::setResourceTimingEnabled(BOOL)
{
    return S_OK;
}

HRESULT WebPreferences::resourceTimingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = true;
    return S_OK;
}

HRESULT WebPreferences::serverTimingEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitServerTimingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setServerTimingEnabled(BOOL enabled)
{
    setBoolValue(WebKitServerTimingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::resizeObserverEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitResizeObserverEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setResizeObserverEnabled(BOOL enabled)
{
    setBoolValue(WebKitResizeObserverEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::aspectRatioOfImgFromWidthAndHeightEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitAspectRatioOfImgFromWidthAndHeightEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAspectRatioOfImgFromWidthAndHeightEnabled(BOOL enabled)
{
    setBoolValue(WebKitAspectRatioOfImgFromWidthAndHeightEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::webSQLEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitWebSQLEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setWebSQLEnabled(BOOL enabled)
{
    setBoolValue(WebKitWebSQLEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::allowTopNavigationToDataURLs(_Out_ BOOL* allowAccess)
{
    if (!allowAccess)
        return E_POINTER;
    *allowAccess = boolValueForKey(WebKitAllowTopNavigationToDataURLsPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAllowTopNavigationToDataURLs(BOOL allowAccess)
{
    setBoolValue(WebKitAllowTopNavigationToDataURLsPreferenceKey, allowAccess);
    return S_OK;
}

HRESULT WebPreferences::modernUnprefixedWebAudioEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitWebAudioEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setModernUnprefixedWebAudioEnabled(BOOL enabled)
{
    setBoolValue(WebKitWebAudioEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::CSSIndividualTransformPropertiesEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitCSSIndividualTransformPropertiesEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setCSSIndividualTransformPropertiesEnabled(BOOL enabled)
{
    setBoolValue(WebKitCSSIndividualTransformPropertiesEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::speechRecognitionEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitSpeechRecognitionEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setSpeechRecognitionEnabled(BOOL enabled)
{
    setBoolValue(WebKitSpeechRecognitionEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::overscrollBehaviorEnabled(_Out_ BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;
    *enabled = boolValueForKey(WebKitOverscrollBehaviorEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setOverscrollBehaviorEnabled(BOOL enabled)
{
    setBoolValue(WebKitOverscrollBehaviorEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::resetForTesting()
{
    load();
    return S_OK;
}

HRESULT WebPreferences::startBatchingUpdates()
{
    m_updateBatchCount++;
    return S_OK;
}

HRESULT WebPreferences::stopBatchingUpdates()
{
    if (!m_updateBatchCount)
        return E_FAIL;
    m_updateBatchCount--;
    if (!m_updateBatchCount) {
        if (m_needsUpdateAfterBatch) {
            postPreferencesChangesNotification();
            m_needsUpdateAfterBatch = false;
        }
    }
    return S_OK;
}

bool WebPreferences::canvasColorSpaceEnabled()
{
    return boolValueForKey("WebKitCanvasColorSpaceEnabled");
}

bool WebPreferences::cssGradientInterpolationColorSpacesEnabled()
{
    return boolValueForKey("WebKitCSSGradientInterpolationColorSpacesEnabled");
}

bool WebPreferences::cssGradientPremultipliedAlphaInterpolationEnabled()
{
    return boolValueForKey("WebKitCSSGradientPremultipliedAlphaInterpolationEnabled");
}

bool WebPreferences::mockScrollbarsControllerEnabled()
{
    return boolValueForKey("WebKitMockScrollbarsControllerEnabled");
}

bool WebPreferences::cssInputSecurityEnabled()
{
    return boolValueForKey("WebKitCSSInputSecurityEnabled");
}

bool WebPreferences::cssTextAlignLastEnabled()
{
    return boolValueForKey("WebKitCSSTextAlignLastEnabled");
}

bool WebPreferences::cssTextJustifyEnabled()
{
    return boolValueForKey("WebKitCSSTextJustifyEnabled");
}
