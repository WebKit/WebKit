/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.  All rights reserved.
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
#include "WebKit.h"
#include "WebKitDLL.h"
#include "WebPreferences.h"

#include "WebNotificationCenter.h"
#include "WebPreferenceKeysPrivate.h"

#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/COMPtr.h>
#include <WebCore/FileSystem.h>
#include <WebCore/Font.h>
#include <WebCore/LocalizedStrings.h>
#include <limits>
#include <shlobj.h>
#include <wchar.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/CACFLayerTreeHost.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

using namespace WebCore;
using std::numeric_limits;

static const String& oldPreferencesPath()
{
    static String path = pathByAppendingComponent(roamingUserSpecificStorageDirectory(), "WebKitPreferences.plist");
    return path;
}

template<typename NumberType> struct CFNumberTraits { static const unsigned Type; };
template<> struct CFNumberTraits<int> { static const unsigned Type = kCFNumberSInt32Type; };
template<> struct CFNumberTraits<LONGLONG> { static const unsigned Type = kCFNumberLongLongType; };
template<> struct CFNumberTraits<float> { static const unsigned Type = kCFNumberFloat32Type; };

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

// WebPreferences ----------------------------------------------------------------

static CFDictionaryRef defaultSettings;

static HashMap<WTF::String, COMPtr<WebPreferences> > webPreferencesInstances;

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
    : m_refCount(0)
    , m_autoSaves(0)
    , m_automaticallyDetectsCacheModel(true)
    , m_numWebViews(0)
{
    gClassCount++;
    gClassNameCount.add("WebPreferences");
}

WebPreferences::~WebPreferences()
{
    gClassCount--;
    gClassNameCount.remove("WebPreferences");
}

WebPreferences* WebPreferences::createInstance()
{
    WebPreferences* instance = new WebPreferences();
    instance->AddRef();
    return instance;
}

HRESULT WebPreferences::postPreferencesChangesNotification()
{
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
    return webPreferencesInstances.get(identifierString).get();
}

void WebPreferences::setInstance(WebPreferences* instance, BSTR identifier)
{
    if (!identifier || !instance)
        return;
    WTF::String identifierString(identifier, SysStringLen(identifier));
    webPreferencesInstances.add(identifierString, instance);
}

void WebPreferences::removeReferenceForIdentifier(BSTR identifier)
{
    if (!identifier || webPreferencesInstances.isEmpty())
        return;

    WTF::String identifierString(identifier, SysStringLen(identifier));
    WebPreferences* webPreference = webPreferencesInstances.get(identifierString).get();
    if (webPreference && webPreference->m_refCount == 1)
        webPreferencesInstances.remove(identifierString);
}

void WebPreferences::initializeDefaultSettings()
{
    if (defaultSettings)
        return;

    CFMutableDictionaryRef defaults = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionaryAddValue(defaults, CFSTR(WebKitStandardFontPreferenceKey), CFSTR("Times New Roman"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitFixedFontPreferenceKey), CFSTR("Courier New"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitSerifFontPreferenceKey), CFSTR("Times New Roman"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitSansSerifFontPreferenceKey), CFSTR("Arial"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitCursiveFontPreferenceKey), CFSTR("Comic Sans MS"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitFantasyFontPreferenceKey), CFSTR("Comic Sans MS"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitPictographFontPreferenceKey), CFSTR("Times New Roman"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitMinimumFontSizePreferenceKey), CFSTR("0"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitMinimumLogicalFontSizePreferenceKey), CFSTR("9"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitDefaultFontSizePreferenceKey), CFSTR("16"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitDefaultFixedFontSizePreferenceKey), CFSTR("13"));

    String defaultDefaultEncoding(WEB_UI_STRING("ISO-8859-1", "The default, default character encoding on Windows"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitDefaultTextEncodingNamePreferenceKey), defaultDefaultEncoding.createCFString().get());

    CFDictionaryAddValue(defaults, CFSTR(WebKitUserStyleSheetEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitUserStyleSheetLocationPreferenceKey), CFSTR(""));
    CFDictionaryAddValue(defaults, CFSTR(WebKitShouldPrintBackgroundsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitTextAreasAreResizablePreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitJavaEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitJavaScriptEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitWebSecurityEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitAllowUniversalAccessFromFileURLsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitAllowFileAccessFromFileURLsPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitJavaScriptCanAccessClipboardPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitXSSAuditorEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitFrameFlatteningEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitPluginsEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitCSSRegionsEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitDatabasesEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitLocalStorageEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitExperimentalNotificationsEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitZoomsTextOnlyPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitAllowAnimatedImagesPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitAllowAnimatedImageLoopingPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitDisplayImagesKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitLoadSiteIconsKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitBackForwardCacheExpirationIntervalKey), CFSTR("1800"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitTabToLinksPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitPrivateBrowsingEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitRespectStandardStyleKeyEquivalentsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitShowsURLsInToolTipsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitShowsToolTipOverTruncatedTextPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitPDFDisplayModePreferenceKey), CFSTR("1"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitPDFScaleFactorPreferenceKey), CFSTR("0"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitShouldDisplaySubtitlesPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitShouldDisplayCaptionsPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitShouldDisplayTextDescriptionsPreferenceKey), kCFBooleanFalse);

    RetainPtr<CFStringRef> linkBehaviorStringRef = adoptCF(CFStringCreateWithFormat(0, 0, CFSTR("%d"), WebKitEditableLinkDefaultBehavior));
    CFDictionaryAddValue(defaults, CFSTR(WebKitEditableLinkBehaviorPreferenceKey), linkBehaviorStringRef.get());

    CFDictionaryAddValue(defaults, CFSTR(WebKitHistoryItemLimitKey), CFSTR("1000"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitHistoryAgeInDaysLimitKey), CFSTR("7"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitIconDatabaseLocationKey), CFSTR(""));
    CFDictionaryAddValue(defaults, CFSTR(WebKitIconDatabaseEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitFontSmoothingTypePreferenceKey), CFSTR("2"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitFontSmoothingContrastPreferenceKey), CFSTR("2"));
    CFDictionaryAddValue(defaults, CFSTR(WebKitCookieStorageAcceptPolicyPreferenceKey), CFSTR("2"));
    CFDictionaryAddValue(defaults, CFSTR(WebContinuousSpellCheckingEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebGrammarCheckingEnabledPreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(AllowContinuousSpellCheckingPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitUsesPageCachePreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitLocalStorageDatabasePathPreferenceKey), CFSTR(""));

    RetainPtr<CFStringRef> cacheModelRef = adoptCF(CFStringCreateWithFormat(0, 0, CFSTR("%d"), WebCacheModelDocumentViewer));
    CFDictionaryAddValue(defaults, CFSTR(WebKitCacheModelPreferenceKey), cacheModelRef.get());

    CFDictionaryAddValue(defaults, CFSTR(WebKitAuthorAndUserStylesEnabledPreferenceKey), kCFBooleanTrue);
    CFDictionaryAddValue(defaults, CFSTR(WebKitApplicationChromeModePreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults, CFSTR(WebKitOfflineWebApplicationCacheEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults, CFSTR(WebKitPaintNativeControlsPreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults, CFSTR(WebKitUseHighResolutionTimersPreferenceKey), kCFBooleanTrue);

#if USE(CG)
    CFDictionaryAddValue(defaults, CFSTR(WebKitAcceleratedCompositingEnabledPreferenceKey), kCFBooleanTrue);
#else
    CFDictionaryAddValue(defaults, CFSTR(WebKitAcceleratedCompositingEnabledPreferenceKey), kCFBooleanFalse);
#endif

    CFDictionaryAddValue(defaults, CFSTR(WebKitShowDebugBordersPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults, CFSTR(WebKitDNSPrefetchingEnabledPreferenceKey), kCFBooleanFalse);

    CFDictionaryAddValue(defaults, CFSTR(WebKitHyperlinkAuditingEnabledPreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults, CFSTR(WebKitMediaPlaybackRequiresUserGesturePreferenceKey), kCFBooleanFalse);
    CFDictionaryAddValue(defaults, CFSTR(WebKitMediaPlaybackAllowsInlinePreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults, CFSTR(WebKitRequestAnimationFrameEnabledPreferenceKey), kCFBooleanTrue);

    CFDictionaryAddValue(defaults, CFSTR(WebKitFullScreenEnabledPreferenceKey), kCFBooleanFalse);

    defaultSettings = defaults;
}

RetainPtr<CFPropertyListRef> WebPreferences::valueForKey(CFStringRef key)
{
    RetainPtr<CFPropertyListRef> value = CFDictionaryGetValue(m_privatePrefs.get(), key);
    if (value)
        return value;

    value = adoptCF(CFPreferencesCopyAppValue(key, kCFPreferencesCurrentApplication));
    if (value)
        return value;

    return CFDictionaryGetValue(defaultSettings, key);
}

void WebPreferences::setValueForKey(CFStringRef key, CFPropertyListRef value)
{
    CFDictionarySetValue(m_privatePrefs.get(), key, value);
    if (m_autoSaves) {
        CFPreferencesSetAppValue(key, value, kCFPreferencesCurrentApplication);
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

BSTR WebPreferences::stringValueForKey(const char* key)
{
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
}

int WebPreferences::integerValueForKey(const char* key)
{
    return numberValueForPreferencesValue<int>(valueForKey(key).get());
}

BOOL WebPreferences::boolValueForKey(const char* key)
{
    return booleanValueForPreferencesValue(valueForKey(key).get());
}

float WebPreferences::floatValueForKey(const char* key)
{
    return numberValueForPreferencesValue<float>(valueForKey(key).get());
}

LONGLONG WebPreferences::longlongValueForKey(const char* key)
{
    return numberValueForPreferencesValue<LONGLONG>(valueForKey(key).get());
}

void WebPreferences::setStringValue(const char* key, BSTR value)
{
    BString val;
    val.adoptBSTR(stringValueForKey(key));
    if (val && !wcscmp(val, value))
        return;
    
    RetainPtr<CFStringRef> valueRef = adoptCF(CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar*>(value), static_cast<CFIndex>(wcslen(value))));
    setValueForKey(key, valueRef.get());

    postPreferencesChangesNotification();
}

void WebPreferences::setIntegerValue(const char* key, int value)
{
    if (integerValueForKey(key) == value)
        return;

    setValueForKey(key, cfNumber(value).get());

    postPreferencesChangesNotification();
}

void WebPreferences::setFloatValue(const char* key, float value)
{
    if (floatValueForKey(key) == value)
        return;

    setValueForKey(key, cfNumber(value).get());

    postPreferencesChangesNotification();
}

void WebPreferences::setBoolValue(const char* key, BOOL value)
{
    if (boolValueForKey(key) == value)
        return;

    setValueForKey(key, value ? kCFBooleanTrue : kCFBooleanFalse);

    postPreferencesChangesNotification();
}

void WebPreferences::setLongLongValue(const char* key, LONGLONG value)
{
    if (longlongValueForKey(key) == value)
        return;

    setValueForKey(key, cfNumber(value).get());

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
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

void WebPreferences::load()
{
    initializeDefaultSettings();

    m_privatePrefs = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    migrateWebKitPreferencesToCFPreferences();
}

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

    CFPropertyListFormat format = kCFPropertyListBinaryFormat_v1_0 | kCFPropertyListXMLFormat_v1_0;
    RetainPtr<CFPropertyListRef> plist = adoptCF(CFPropertyListCreateFromStream(0, stream.get(), 0, kCFPropertyListMutableContainersAndLeaves, &format, 0));
    CFReadStreamClose(stream.get());

    if (!plist || CFGetTypeID(plist.get()) != CFDictionaryGetTypeID())
        return;

    copyWebKitPreferencesToCFPreferences(static_cast<CFDictionaryRef>(plist.get()));

    deleteFile(oldPreferencesPath());
}

void WebPreferences::copyWebKitPreferencesToCFPreferences(CFDictionaryRef dict)
{
    ASSERT_ARG(dict, dict);

    int count = CFDictionaryGetCount(dict);
    if (count <= 0)
        return;

    CFStringRef didRemoveDefaultsKey = CFSTR(WebKitDidMigrateDefaultSettingsFromSafari3BetaPreferenceKey);
    bool omitDefaults = !booleanValueForPreferencesValue(CFDictionaryGetValue(dict, didRemoveDefaultsKey));

    auto keys = std::make_unique<CFTypeRef[]>(count);
    auto values = std::make_unique<CFTypeRef[]>(count);
    CFDictionaryGetKeysAndValues(dict, keys.get(), values.get());

    for (int i = 0; i < count; ++i) {
        if (!keys[i] || !values[i] || CFGetTypeID(keys[i]) != CFStringGetTypeID())
            continue;

        if (omitDefaults) {
            CFTypeRef defaultValue = CFDictionaryGetValue(defaultSettings, keys[i]);
            if (defaultValue && CFEqual(defaultValue, values[i]))
                continue;
        }

        setValueForKey(static_cast<CFStringRef>(keys[i]), values[i]);
    }
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebPreferences::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebPreferences*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferences))
        *ppvObject = static_cast<IWebPreferences*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferencesPrivate))
        *ppvObject = static_cast<IWebPreferencesPrivate*>(this);
    else if (IsEqualGUID(riid, CLSID_WebPreferences))
        *ppvObject = this;
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebPreferences::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebPreferences::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebPreferences ------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebPreferences::standardPreferences( 
    /* [retval][out] */ IWebPreferences** standardPreferences)
{
    if (!standardPreferences)
        return E_POINTER;
    *standardPreferences = sharedStandardPreferences();
    (*standardPreferences)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::initWithIdentifier( 
        /* [in] */ BSTR anIdentifier,
        /* [retval][out] */ IWebPreferences** preferences)
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

HRESULT STDMETHODCALLTYPE WebPreferences::identifier( 
    /* [retval][out] */ BSTR* ident)
{
    if (!ident)
        return E_POINTER;
    *ident = m_identifier ? SysAllocString(m_identifier) : 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::standardFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(WebKitStandardFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setStandardFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(WebKitStandardFontPreferenceKey, family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::fixedFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(WebKitFixedFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setFixedFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(WebKitFixedFontPreferenceKey, family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::serifFontFamily( 
    /* [retval][out] */ BSTR* fontFamily)
{
    *fontFamily = stringValueForKey(WebKitSerifFontPreferenceKey);
    return (*fontFamily) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setSerifFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(WebKitSerifFontPreferenceKey, family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::sansSerifFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(WebKitSansSerifFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setSansSerifFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(WebKitSansSerifFontPreferenceKey, family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::cursiveFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(WebKitCursiveFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setCursiveFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(WebKitCursiveFontPreferenceKey, family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::fantasyFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(WebKitFantasyFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setFantasyFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(WebKitFantasyFontPreferenceKey, family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::pictographFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(WebKitPictographFontPreferenceKey);
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setPictographFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(WebKitPictographFontPreferenceKey, family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::defaultFontSize( 
    /* [retval][out] */ int* fontSize)
{
    *fontSize = integerValueForKey(WebKitDefaultFontSizePreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setDefaultFontSize( 
    /* [in] */ int fontSize)
{
    setIntegerValue(WebKitDefaultFontSizePreferenceKey, fontSize);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::defaultFixedFontSize( 
    /* [retval][out] */ int* fontSize)
{
    *fontSize = integerValueForKey(WebKitDefaultFixedFontSizePreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setDefaultFixedFontSize( 
    /* [in] */ int fontSize)
{
    setIntegerValue(WebKitDefaultFixedFontSizePreferenceKey, fontSize);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::minimumFontSize( 
    /* [retval][out] */ int* fontSize)
{
    *fontSize = integerValueForKey(WebKitMinimumFontSizePreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setMinimumFontSize( 
    /* [in] */ int fontSize)
{
    setIntegerValue(WebKitMinimumFontSizePreferenceKey, fontSize);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::minimumLogicalFontSize( 
    /* [retval][out] */ int* fontSize)
{
    *fontSize = integerValueForKey(WebKitMinimumLogicalFontSizePreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setMinimumLogicalFontSize( 
    /* [in] */ int fontSize)
{
    setIntegerValue(WebKitMinimumLogicalFontSizePreferenceKey, fontSize);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::defaultTextEncodingName( 
    /* [retval][out] */ BSTR* name)
{
    *name = stringValueForKey(WebKitDefaultTextEncodingNamePreferenceKey);
    return (*name) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setDefaultTextEncodingName( 
    /* [in] */ BSTR name)
{
    setStringValue(WebKitDefaultTextEncodingNamePreferenceKey, name);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::userStyleSheetEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitUserStyleSheetEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setUserStyleSheetEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitUserStyleSheetEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::userStyleSheetLocation( 
    /* [retval][out] */ BSTR* location)
{
    *location = stringValueForKey(WebKitUserStyleSheetLocationPreferenceKey);
    return (*location) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setUserStyleSheetLocation( 
    /* [in] */ BSTR location)
{
    setStringValue(WebKitUserStyleSheetLocationPreferenceKey, location);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::isJavaEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitJavaEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setJavaEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitJavaEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::isJavaScriptEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitJavaScriptEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setJavaScriptEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitJavaScriptEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::isWebSecurityEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitWebSecurityEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setWebSecurityEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitWebSecurityEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::allowUniversalAccessFromFileURLs(
    /* [retval][out] */ BOOL* allowAccess)
{
    *allowAccess = boolValueForKey(WebKitAllowUniversalAccessFromFileURLsPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setAllowUniversalAccessFromFileURLs(
    /* [in] */ BOOL allowAccess)
{
    setBoolValue(WebKitAllowUniversalAccessFromFileURLsPreferenceKey, allowAccess);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::allowFileAccessFromFileURLs(
    /* [retval][out] */ BOOL* allowAccess)
{
    *allowAccess = boolValueForKey(WebKitAllowFileAccessFromFileURLsPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setAllowFileAccessFromFileURLs(
    /* [in] */ BOOL allowAccess)
{
    setBoolValue(WebKitAllowFileAccessFromFileURLsPreferenceKey, allowAccess);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::javaScriptCanAccessClipboard(
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitJavaScriptCanAccessClipboardPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setJavaScriptCanAccessClipboard(
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitJavaScriptCanAccessClipboardPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::isXSSAuditorEnabled(
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitXSSAuditorEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setXSSAuditorEnabled(
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitXSSAuditorEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::isFrameFlatteningEnabled(
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitFrameFlatteningEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setFrameFlatteningEnabled(
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitFrameFlatteningEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::javaScriptCanOpenWindowsAutomatically( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setJavaScriptCanOpenWindowsAutomatically( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::arePlugInsEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitPluginsEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setPlugInsEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitPluginsEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::isCSSRegionsEnabled(
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitCSSRegionsEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setCSSRegionsEnabled(
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitCSSRegionsEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::allowsAnimatedImages( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitAllowAnimatedImagesPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setAllowsAnimatedImages( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitAllowAnimatedImagesPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::allowAnimatedImageLooping( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitAllowAnimatedImageLoopingPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setAllowAnimatedImageLooping( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitAllowAnimatedImageLoopingPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setLoadsImagesAutomatically( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitDisplayImagesKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::loadsImagesAutomatically( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitDisplayImagesKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setLoadsSiteIconsIgnoringImageLoadingPreference(
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitLoadSiteIconsKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::loadsSiteIconsIgnoringImageLoadingPreference(
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitLoadSiteIconsKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setHixie76WebSocketProtocolEnabled(
    /* [in] */ BOOL enabled)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::hixie76WebSocketProtocolEnabled(
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = false;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setMediaPlaybackRequiresUserGesture(
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitMediaPlaybackRequiresUserGesturePreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::mediaPlaybackRequiresUserGesture(
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitMediaPlaybackRequiresUserGesturePreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setMediaPlaybackAllowsInline(
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitMediaPlaybackAllowsInlinePreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::mediaPlaybackAllowsInline(
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitMediaPlaybackAllowsInlinePreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setAutosaves( 
    /* [in] */ BOOL enabled)
{
    m_autoSaves = !!enabled;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::autosaves( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = m_autoSaves ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setShouldPrintBackgrounds( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitShouldPrintBackgroundsPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::shouldPrintBackgrounds( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitShouldPrintBackgroundsPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setPrivateBrowsingEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitPrivateBrowsingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::privateBrowsingEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitPrivateBrowsingEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setTabsToLinks( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitTabToLinksPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::tabsToLinks( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitTabToLinksPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setUsesPageCache( 
        /* [in] */ BOOL usesPageCache)
{
    setBoolValue(WebKitUsesPageCachePreferenceKey, usesPageCache);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::usesPageCache( 
    /* [retval][out] */ BOOL* usesPageCache)
{
    *usesPageCache = boolValueForKey(WebKitUsesPageCachePreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::textAreasAreResizable( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitTextAreasAreResizablePreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setTextAreasAreResizable( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(WebKitTextAreasAreResizablePreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::historyItemLimit(int* limit)
{
    *limit = integerValueForKey(WebKitHistoryItemLimitKey);
    return S_OK;
}

HRESULT WebPreferences::setHistoryItemLimit(int limit)
{
    setIntegerValue(WebKitHistoryItemLimitKey, limit);
    return S_OK;
}

HRESULT WebPreferences::historyAgeInDaysLimit(int* limit)
{
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

HRESULT WebPreferences::iconDatabaseLocation(
    /* [out] */ BSTR* location)
{
    *location = stringValueForKey(WebKitIconDatabaseLocationKey);
    return (*location) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setIconDatabaseLocation(
    /* [in] */ BSTR location)
{
    setStringValue(WebKitIconDatabaseLocationKey, location);
    return S_OK;
}

HRESULT WebPreferences::iconDatabaseEnabled(BOOL* enabled)//location)
{
    *enabled = boolValueForKey(WebKitIconDatabaseEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setIconDatabaseEnabled(BOOL enabled )//location)
{
    setBoolValue(WebKitIconDatabaseEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::fontSmoothing( 
    /* [retval][out] */ FontSmoothingType* smoothingType)
{
    *smoothingType = static_cast<FontSmoothingType>(integerValueForKey(WebKitFontSmoothingTypePreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setFontSmoothing( 
    /* [in] */ FontSmoothingType smoothingType)
{
    setIntegerValue(WebKitFontSmoothingTypePreferenceKey, smoothingType);
    if (smoothingType == FontSmoothingTypeWindows)
        smoothingType = FontSmoothingTypeMedium;
#if USE(CG)
    wkSetFontSmoothingLevel((int)smoothingType);
#endif
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::fontSmoothingContrast( 
    /* [retval][out] */ float* contrast)
{
    *contrast = floatValueForKey(WebKitFontSmoothingContrastPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setFontSmoothingContrast( 
    /* [in] */ float contrast)
{
    setFloatValue(WebKitFontSmoothingContrastPreferenceKey, contrast);
#if USE(CG)
    wkSetFontSmoothingContrast(contrast);
#endif
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::editableLinkBehavior(
    /* [out, retval] */ WebKitEditableLinkBehavior* editableLinkBehavior)
{
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

HRESULT STDMETHODCALLTYPE WebPreferences::setEditableLinkBehavior(
    /* [in] */ WebKitEditableLinkBehavior behavior)
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

HRESULT STDMETHODCALLTYPE WebPreferences::hyperlinkAuditingEnabled(
    /* [in] */ BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitHyperlinkAuditingEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setHyperlinkAuditingEnabled(
    /* [retval][out] */ BOOL enabled)
{
    setBoolValue(WebKitHyperlinkAuditingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::cookieStorageAcceptPolicy( 
        /* [retval][out] */ WebKitCookieStorageAcceptPolicy *acceptPolicy )
{
    if (!acceptPolicy)
        return E_POINTER;

    *acceptPolicy = static_cast<WebKitCookieStorageAcceptPolicy>(integerValueForKey(WebKitCookieStorageAcceptPolicyPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setCookieStorageAcceptPolicy( 
        /* [in] */ WebKitCookieStorageAcceptPolicy acceptPolicy)
{
    setIntegerValue(WebKitCookieStorageAcceptPolicyPreferenceKey, acceptPolicy);
    return S_OK;
}


HRESULT WebPreferences::continuousSpellCheckingEnabled(BOOL* enabled)
{
    *enabled = boolValueForKey(WebContinuousSpellCheckingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setContinuousSpellCheckingEnabled(BOOL enabled)
{
    setBoolValue(WebContinuousSpellCheckingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::grammarCheckingEnabled(BOOL* enabled)
{
    *enabled = boolValueForKey(WebGrammarCheckingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setGrammarCheckingEnabled(BOOL enabled)
{
    setBoolValue(WebGrammarCheckingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::allowContinuousSpellChecking(BOOL* enabled)
{
    *enabled = boolValueForKey(AllowContinuousSpellCheckingPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setAllowContinuousSpellChecking(BOOL enabled)
{
    setBoolValue(AllowContinuousSpellCheckingPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::areSeamlessIFramesEnabled(BOOL* enabled)
{
    *enabled = boolValueForKey(SeamlessIFramesPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setSeamlessIFramesEnabled(BOOL enabled)
{
    setBoolValue(SeamlessIFramesPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::isDOMPasteAllowed(BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitDOMPasteAllowedPreferenceKey);
    return S_OK;
}
    
HRESULT WebPreferences::setDOMPasteAllowed(BOOL enabled)
{
    setBoolValue(WebKitDOMPasteAllowedPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::cacheModel(WebCacheModel* cacheModel)
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

HRESULT WebPreferences::shouldPaintNativeControls(BOOL* shouldPaint)
{
    *shouldPaint = boolValueForKey(WebKitPaintNativeControlsPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setShouldPaintNativeControls(BOOL shouldPaint)
{
    setBoolValue(WebKitPaintNativeControlsPreferenceKey, shouldPaint);
    return S_OK;
}

HRESULT WebPreferences::setDeveloperExtrasEnabled(BOOL enabled)
{
    setBoolValue(WebKitDeveloperExtrasEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::developerExtrasEnabled(BOOL* enabled)
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

HRESULT WebPreferences::automaticallyDetectsCacheModel(BOOL* automaticallyDetectsCacheModel)
{
    if (!automaticallyDetectsCacheModel)
        return E_POINTER;

    *automaticallyDetectsCacheModel = m_automaticallyDetectsCacheModel;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setAuthorAndUserStylesEnabled(BOOL enabled)
{
    setBoolValue(WebKitAuthorAndUserStylesEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::authorAndUserStylesEnabled(BOOL* enabled)
{
    if (!enabled)
        return E_POINTER;

    *enabled = boolValueForKey(WebKitAuthorAndUserStylesEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::inApplicationChromeMode(BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitApplicationChromeModePreferenceKey);
    return S_OK;
}
    
HRESULT WebPreferences::setApplicationChromeMode(BOOL enabled)
{
    setBoolValue(WebKitApplicationChromeModePreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setOfflineWebApplicationCacheEnabled(BOOL enabled)
{
    setBoolValue(WebKitOfflineWebApplicationCacheEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::offlineWebApplicationCacheEnabled(BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitOfflineWebApplicationCacheEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setDatabasesEnabled(BOOL enabled)
{
    setBoolValue(WebKitDatabasesEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::databasesEnabled(BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitDatabasesEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setLocalStorageEnabled(BOOL enabled)
{
    setBoolValue(WebKitLocalStorageEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::localStorageEnabled(BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitLocalStorageEnabledPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::localStorageDatabasePath(BSTR* location)
{
    *location = stringValueForKey(WebKitLocalStorageDatabasePathPreferenceKey);
    return (*location) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setLocalStorageDatabasePath(BSTR location)
{
    setStringValue(WebKitLocalStorageDatabasePathPreferenceKey, location);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setExperimentalNotificationsEnabled(BOOL enabled)
{
    setBoolValue(WebKitExperimentalNotificationsEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::experimentalNotificationsEnabled(BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitExperimentalNotificationsEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setZoomsTextOnly(BOOL zoomsTextOnly)
{
    setBoolValue(WebKitZoomsTextOnlyPreferenceKey, zoomsTextOnly);
    return S_OK;
}

HRESULT WebPreferences::zoomsTextOnly(BOOL* zoomsTextOnly)
{
    *zoomsTextOnly = boolValueForKey(WebKitZoomsTextOnlyPreferenceKey);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setShouldUseHighResolutionTimers(BOOL useHighResolutionTimers)
{
    setBoolValue(WebKitUseHighResolutionTimersPreferenceKey, useHighResolutionTimers);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::shouldUseHighResolutionTimers(BOOL* useHighResolutionTimers)
{
    *useHighResolutionTimers = boolValueForKey(WebKitUseHighResolutionTimersPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setPreferenceForTest(BSTR key, BSTR value)
{
    if (!SysStringLen(key) || !SysStringLen(value))
        return E_FAIL;
    RetainPtr<CFStringRef> keyString = adoptCF(CFStringCreateWithCharacters(0, reinterpret_cast<UniChar*>(key), SysStringLen(key)));
    RetainPtr<CFStringRef> valueString = adoptCF(CFStringCreateWithCharacters(0, reinterpret_cast<UniChar*>(value), SysStringLen(value)));
    setValueForKey(keyString.get(), valueString.get());
    postPreferencesChangesNotification();
    return S_OK;
}

HRESULT WebPreferences::setAcceleratedCompositingEnabled(BOOL enabled)
{
    setBoolValue(WebKitAcceleratedCompositingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::acceleratedCompositingEnabled(BOOL* enabled)
{
#if USE(ACCELERATED_COMPOSITING)
#if USE(CA)
    *enabled = CACFLayerTreeHost::acceleratedCompositingAvailable() && boolValueForKey(WebKitAcceleratedCompositingEnabledPreferenceKey);
#else
    *enabled = TRUE;
#endif
#else
    *enabled = FALSE;
#endif
    return S_OK;
}

HRESULT WebPreferences::showDebugBorders(BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitShowDebugBordersPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setShowDebugBorders(BOOL enabled)
{
    setBoolValue(WebKitShowDebugBordersPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::showRepaintCounter(BOOL* enabled)
{
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

HRESULT WebPreferences::customDragCursorsEnabled(BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitCustomDragCursorsEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::setDNSPrefetchingEnabled(BOOL enabled)
{
    setBoolValue(WebKitDNSPrefetchingEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::isDNSPrefetchingEnabled(BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitDNSPrefetchingEnabledPreferenceKey);
    return S_OK;
}

HRESULT WebPreferences::isFullScreenEnabled(BOOL* enabled)
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

HRESULT WebPreferences::avFoundationEnabled(BOOL* enabled)
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

HRESULT WebPreferences::showsToolTipOverTruncatedText(BOOL* showsToolTip)
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

HRESULT WebPreferences::shouldInvertColors(BOOL* shouldInvertColors)
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

HRESULT WebPreferences::shouldDisplaySubtitles(BOOL* enabled)
{
#if ENABLE(VIDEO_TRACK)
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
#if ENABLE(VIDEO_TRACK)
    setBoolValue(WebKitShouldDisplaySubtitlesPreferenceKey, enabled);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::shouldDisplayCaptions(BOOL* enabled)
{
#if ENABLE(VIDEO_TRACK)
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
#if ENABLE(VIDEO_TRACK)
    setBoolValue(WebKitShouldDisplayCaptionsPreferenceKey, enabled);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::shouldDisplayTextDescriptions(BOOL* enabled)
{
#if ENABLE(VIDEO_TRACK)
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
#if ENABLE(VIDEO_TRACK)
    setBoolValue(WebKitShouldDisplayTextDescriptionsPreferenceKey, enabled);
    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

HRESULT WebPreferences::setRequestAnimationFrameEnabled(BOOL enabled)
{
    setBoolValue(WebKitRequestAnimationFrameEnabledPreferenceKey, enabled);
    return S_OK;
}

HRESULT WebPreferences::requestAnimationFrameEnabled(BOOL* enabled)
{
    *enabled = boolValueForKey(WebKitRequestAnimationFrameEnabledPreferenceKey);
    return S_OK;
}

