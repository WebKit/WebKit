/*
 * Copyright (C) 2006-2014 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#include "config.h"
#include "TestRunner.h"

#include "DumpRenderTree.h"
#include "EditingDelegate.h"
#include "PolicyDelegate.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include <CoreFoundation/CoreFoundation.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRefBSTR.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebCore/COMPtr.h>
#include <WebKitLegacy/WebKit.h>
#include <WebKitLegacy/WebKitCOMAPI.h>
#include <comutil.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <string>
#include <wtf/Assertions.h>
#include <wtf/Platform.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

using std::string;
using std::wstring;

static bool resolveCygwinPath(const wstring& cygwinPath, wstring& windowsPath);

TestRunner::~TestRunner()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    // reset webview-related states back to default values in preparation for next test
    COMPtr<IWebViewEditing> viewEditing;
    if (FAILED(webView->QueryInterface(&viewEditing)))
        return;
    COMPtr<IWebEditingDelegate> delegate;
    if (FAILED(viewEditing->editingDelegate(&delegate)))
        return;
    COMPtr<EditingDelegate> editingDelegate(Query, viewEditing.get());
    if (editingDelegate)
        editingDelegate->setAcceptsEditing(TRUE);
}

JSContextRef TestRunner::mainFrameJSContext()
{
    return frame->globalContext();
}

void TestRunner::addDisallowedURL(JSStringRef url)
{
    // FIXME: Implement!
    fprintf(testResult, "ERROR: TestRunner::addDisallowedURL(JSStringRef) not implemented\n");
}

bool TestRunner::callShouldCloseOnWebView()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return false;

    COMPtr<IWebViewPrivate2> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return false;

    BOOL result;
    viewPrivate->shouldClose(&result);
    return result;
}

void TestRunner::clearAllApplicationCaches()
{
    COMPtr<IWebApplicationCache> applicationCache;
    if (FAILED(WebKitCreateInstance(CLSID_WebApplicationCache, 0, IID_IWebApplicationCache, reinterpret_cast<void**>(&applicationCache))))
        return;

    applicationCache->deleteAllApplicationCaches();
}

long long TestRunner::applicationCacheDiskUsageForOrigin(JSStringRef url)
{
    COMPtr<IWebSecurityOrigin2> origin;
    if (FAILED(WebKitCreateInstance(CLSID_WebSecurityOrigin, 0, IID_IWebSecurityOrigin2, reinterpret_cast<void**>(&origin))))
        return 0;

    COMPtr<IWebApplicationCache> applicationCache;
    if (FAILED(WebKitCreateInstance(CLSID_WebApplicationCache, 0, IID_IWebApplicationCache, reinterpret_cast<void**>(&applicationCache))))
        return 0;

    _bstr_t urlBstr(JSStringCopyBSTR(url), false);
    origin->initWithURL(urlBstr.GetBSTR());

    long long usage = 0;
    if (FAILED(applicationCache->diskUsageForOrigin(origin.get(), &usage)))
        return 0;

    return usage;
}

void TestRunner::clearApplicationCacheForOrigin(JSStringRef origin)
{
    COMPtr<IWebSecurityOrigin2> securityOrigin;
    if (FAILED(WebKitCreateInstance(CLSID_WebSecurityOrigin, 0, IID_IWebSecurityOrigin2, reinterpret_cast<void**>(&securityOrigin))))
        return;

    _bstr_t originBstr(JSStringCopyBSTR(origin), false);
    if (FAILED(securityOrigin->initWithURL(originBstr.GetBSTR())))
        return;

    COMPtr<IWebApplicationCache> applicationCache;
    if (FAILED(WebKitCreateInstance(CLSID_WebApplicationCache, 0, IID_IWebApplicationCache, reinterpret_cast<void**>(&applicationCache))))
        return;

    applicationCache->deleteCacheForOrigin(securityOrigin.get());
}

JSValueRef TestRunner::originsWithApplicationCache(JSContextRef context)
{
    // FIXME: Implement to get origins that have application caches.
    fprintf(testResult, "ERROR: TestRunner::originsWithApplicationCache(JSContextRef) not implemented\n");
    return JSValueMakeUndefined(context);
}


void TestRunner::clearAllDatabases()
{
    COMPtr<IWebDatabaseManager> databaseManager;
    COMPtr<IWebDatabaseManager> tmpDatabaseManager;
    if (FAILED(WebKitCreateInstance(CLSID_WebDatabaseManager, 0, IID_IWebDatabaseManager, (void**)&tmpDatabaseManager)))
        return;
    if (FAILED(tmpDatabaseManager->sharedWebDatabaseManager(&databaseManager)))
        return;

    databaseManager->deleteAllDatabases();

    COMPtr<IWebDatabaseManager2> databaseManager2;
    if (FAILED(databaseManager->QueryInterface(&databaseManager2)))
        return;

    databaseManager2->deleteAllIndexedDatabases();
}

void TestRunner::setStorageDatabaseIdleInterval(double)
{
    // FIXME: Implement. Requires non-existant (on Windows) WebStorageManager
    fprintf(testResult, "ERROR: TestRunner::setStorageDatabaseIdleInterval(double) not implemented\n");
}

void TestRunner::closeIdleLocalStorageDatabases()
{
    // FIXME: Implement. Requires non-existant (on Windows) WebStorageManager
    fprintf(testResult, "ERROR: TestRunner::closeIdleLocalStorageDatabases(double) not implemented\n");
}

void TestRunner::clearBackForwardList()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebBackForwardList> backForwardList;
    if (FAILED(webView->backForwardList(&backForwardList)))
        return;

    COMPtr<IWebHistoryItem> item;
    if (FAILED(backForwardList->currentItem(&item)))
        return;

    // We clear the history by setting the back/forward list's capacity to 0
    // then restoring it back and adding back the current item.
    int capacity;
    if (FAILED(backForwardList->capacity(&capacity)))
        return;

    backForwardList->setCapacity(0);
    backForwardList->setCapacity(capacity);
    backForwardList->addItem(item.get());
    backForwardList->goToItem(item.get());
}

JSRetainPtr<JSStringRef> TestRunner::copyDecodedHostName(JSStringRef name)
{
    // FIXME: Implement!
    fprintf(testResult, "ERROR: TestRunner::copyDecodedHostName(JSStringRef) not implemented\n");
    return 0;
}

JSRetainPtr<JSStringRef> TestRunner::copyEncodedHostName(JSStringRef name)
{
    // FIXME: Implement!
    fprintf(testResult, "ERROR: TestRunner::copyEncodedHostName(JSStringRef) not implemented\n");
    return 0;
}

void TestRunner::display()
{
    displayWebView();
}

void TestRunner::displayAndTrackRepaints()
{
    displayWebView();
}

void TestRunner::keepWebHistory()
{
    COMPtr<IWebHistory> history;
    if (FAILED(WebKitCreateInstance(CLSID_WebHistory, 0, __uuidof(history), reinterpret_cast<void**>(&history))))
        return;

    COMPtr<IWebHistory> sharedHistory;
    if (SUCCEEDED(history->optionalSharedHistory(&sharedHistory)) && sharedHistory)
        return;

    if (FAILED(WebKitCreateInstance(CLSID_WebHistory, 0, __uuidof(sharedHistory), reinterpret_cast<void**>(&sharedHistory))))
        return;

    history->setOptionalSharedHistory(sharedHistory.get());
}

int TestRunner::numberOfPendingGeolocationPermissionRequests()
{
    // FIXME: Implement for Geolocation layout tests.
    fprintf(testResult, "ERROR: TestRunner::numberOfPendingGeolocationPermissionRequests() not implemented\n");
    return -1;
}

bool TestRunner::isGeolocationProviderActive()
{
    // FIXME: Implement for Geolocation layout tests.
    fprintf(testResult, "ERROR: TestRunner::isGeolocationProviderActive() not implemented\n");
    return false;
}

size_t TestRunner::webHistoryItemCount()
{
    COMPtr<IWebHistory> history;
    if (FAILED(WebKitCreateInstance(CLSID_WebHistory, 0, __uuidof(history), reinterpret_cast<void**>(&history))))
        return 0;

    COMPtr<IWebHistory> sharedHistory;
    if (FAILED(history->optionalSharedHistory(&sharedHistory)) || !sharedHistory)
        return 0;

    COMPtr<IWebHistoryPrivate> sharedHistoryPrivate;
    if (FAILED(sharedHistory->QueryInterface(&sharedHistoryPrivate)))
        return 0;

    int count = 0;
    if (FAILED(sharedHistoryPrivate->allItems(&count, 0)))
        return 0;

    return count;
}

void TestRunner::notifyDone()
{
    // Same as on mac.  This can be shared.
    if (m_waitToDump) {
        m_waitToDump = false;
        if (!topLoadingFrame && !DRT::WorkQueue::singleton().count())
            dump();
    } else
        fprintf(stderr, "TestRunner::notifyDone() called unexpectedly.");
}

void TestRunner::forceImmediateCompletion()
{
    // Same as on mac. This can be shared.
    if (m_waitToDump) {
        m_waitToDump = false;
        if (!DRT::WorkQueue::singleton().count())
            dump();
    } else
        fprintf(stderr, "TestRunner::forceImmediateCompletion() called unexpectedly.");
}

static wstring jsStringRefToWString(JSStringRef jsStr)
{
    size_t length = JSStringGetLength(jsStr);
    Vector<WCHAR> buffer(length + 1, 0);
    memcpy(buffer.data(), JSStringGetCharactersPtr(jsStr), length * sizeof(WCHAR));
    buffer[length] = 0;

    return buffer.data();
}

JSRetainPtr<JSStringRef> TestRunner::pathToLocalResource(JSContextRef context, JSStringRef url)
{
    wstring input(JSStringGetCharactersPtr(url), JSStringGetLength(url));

    wstring localPath;
    if (!resolveCygwinPath(input, localPath)) {
        fprintf(testResult, "ERROR: Failed to resolve Cygwin path %S\n", input.c_str());
        return nullptr;
    }

    return adopt(JSStringCreateWithCharacters(localPath.c_str(), localPath.length()));
}

void TestRunner::queueLoad(JSStringRef url, JSStringRef target)
{
    COMPtr<IWebDataSource> dataSource;
    if (FAILED(frame->dataSource(&dataSource)))
        return;

    COMPtr<IWebURLResponse> response;
    if (FAILED(dataSource->response(&response)) || !response)
        return;

    _bstr_t responseURLBSTR;
    if (FAILED(response->URL(&responseURLBSTR.GetBSTR())))
        return;
    wstring responseURL(responseURLBSTR, responseURLBSTR.length());
    wstring wURL = jsStringRefToWString(url);

    DWORD bufferSize = responseURL.size() + wURL.size() + 1;
    std::vector<wchar_t> buffer(bufferSize);
    auto result = UrlCombine(responseURL.data(), wURL.data(), buffer.data(), &bufferSize, 0);
    if (result == E_POINTER) {
        buffer.resize(bufferSize);
        result = UrlCombine(responseURL.data(), wURL.data(), buffer.data(), &bufferSize, 0);
        ASSERT(result  == S_OK);
    }

    auto jsAbsoluteURL = adopt(JSStringCreateWithCharacters(buffer.data(), bufferSize));
    DRT::WorkQueue::singleton().queue(new LoadItem(jsAbsoluteURL.get(), target));
}

void TestRunner::setAcceptsEditing(bool acceptsEditing)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewEditing> viewEditing;
    if (FAILED(webView->QueryInterface(&viewEditing)))
        return;

    COMPtr<IWebEditingDelegate> delegate;
    if (FAILED(viewEditing->editingDelegate(&delegate)))
        return;

    EditingDelegate* editingDelegate = (EditingDelegate*)(IWebEditingDelegate*)delegate.get();
    editingDelegate->setAcceptsEditing(acceptsEditing);
}

void TestRunner::setAlwaysAcceptCookies(bool alwaysAcceptCookies)
{
    if (alwaysAcceptCookies == m_alwaysAcceptCookies)
        return;

    if (!::setAlwaysAcceptCookies(alwaysAcceptCookies))
        return;
    m_alwaysAcceptCookies = alwaysAcceptCookies;
}

void TestRunner::setOnlyAcceptFirstPartyCookies(bool onlyAcceptFirstPartyCookies)
{
    // FIXME: Implement.
    fprintf(testResult, "ERROR: TestRunner::setOnlyAcceptFirstPartyCookies() not implemented\n");
}

void TestRunner::setAppCacheMaximumSize(unsigned long long size)
{
    COMPtr<IWebApplicationCache> applicationCache;
    if (FAILED(WebKitCreateInstance(CLSID_WebApplicationCache, 0, IID_IWebApplicationCache, reinterpret_cast<void**>(&applicationCache))))
        return;

    applicationCache->setMaximumSize(size);
}

void TestRunner::setAuthorAndUserStylesEnabled(bool flag)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    COMPtr<IWebPreferencesPrivate> prefsPrivate(Query, preferences);
    if (!prefsPrivate)
        return;

    prefsPrivate->setAuthorAndUserStylesEnabled(flag);
}

void TestRunner::setCustomPolicyDelegate(bool setDelegate, bool permissive)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    if (!setDelegate) {
        webView->setPolicyDelegate(nullptr);
        return;
    }

    policyDelegate->setPermissive(permissive);
    webView->setPolicyDelegate(policyDelegate);
}

void TestRunner::setDatabaseQuota(unsigned long long quota)
{
    COMPtr<IWebDatabaseManager> databaseManager;
    COMPtr<IWebDatabaseManager> tmpDatabaseManager;

    if (FAILED(WebKitCreateInstance(CLSID_WebDatabaseManager, 0, IID_IWebDatabaseManager, (void**)&tmpDatabaseManager)))
        return;
    if (FAILED(tmpDatabaseManager->sharedWebDatabaseManager(&databaseManager)))
        return;

    databaseManager->setQuota(_bstr_t(L"file:///"), quota);
}

void TestRunner::goBack()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    BOOL ignore = TRUE;
    webView->goBack(&ignore);
}

void TestRunner::setDefersLoading(bool defers)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    viewPrivate->setDefersCallbacks(defers);
}

void TestRunner::setDomainRelaxationForbiddenForURLScheme(bool forbidden, JSStringRef scheme)
{
    COMPtr<IWebViewPrivate2> webView;
    if (FAILED(WebKitCreateInstance(__uuidof(WebView), 0, __uuidof(webView), reinterpret_cast<void**>(&webView))))
        return;

    _bstr_t schemeBSTR(JSStringCopyBSTR(scheme), false);
    webView->setDomainRelaxationForbiddenForURLScheme(forbidden, schemeBSTR.GetBSTR());
}

void TestRunner::setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
{
    // FIXME: Implement for DeviceOrientation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=30335.
    fprintf(testResult, "ERROR: TestRunner::setMockDeviceOrientation() not implemented\n");
}

void TestRunner::setMockGeolocationPosition(double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, bool providesFloorLevel, double floorLevel)
{
    // FIXME: Implement for Geolocation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=28264.
    fprintf(testResult, "ERROR: TestRunner::setMockGeolocationPosition() not implemented\n");
}

void TestRunner::setMockGeolocationPositionUnavailableError(JSStringRef message)
{
    // FIXME: Implement for Geolocation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=28264.
    fprintf(testResult, "ERROR: TestRunner::setMockGeolocationPositionUnavailableError() not implemented\n");
}

void TestRunner::setGeolocationPermission(bool allow)
{
    // FIXME: Implement for Geolocation layout tests.
    setGeolocationPermissionCommon(allow);
}

void TestRunner::setIconDatabaseEnabled(bool)
{
}

void TestRunner::setMainFrameIsFirstResponder(bool)
{
    // Nothing to do here on Windows
}

void TestRunner::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    preferences->setPrivateBrowsingEnabled(privateBrowsingEnabled);
}

void TestRunner::setXSSAuditorEnabled(bool enabled)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    COMPtr<IWebPreferencesPrivate> prefsPrivate(Query, preferences);
    if (!prefsPrivate)
        return;

    prefsPrivate->setXSSAuditorEnabled(enabled);
}

void TestRunner::setSpatialNavigationEnabled(bool enabled)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    COMPtr<IWebPreferencesPrivate6> prefsPrivate(Query, preferences);
    if (!prefsPrivate)
        return;

    prefsPrivate->setSpatialNavigationEnabled(enabled);
}

void TestRunner::setAllowUniversalAccessFromFileURLs(bool enabled)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    COMPtr<IWebPreferencesPrivate> prefsPrivate(Query, preferences);
    if (!prefsPrivate)
        return;

    prefsPrivate->setAllowUniversalAccessFromFileURLs(enabled);
}

void TestRunner::setAllowFileAccessFromFileURLs(bool enabled)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    COMPtr<IWebPreferencesPrivate> prefsPrivate(Query, preferences);
    if (!prefsPrivate)
        return;

    prefsPrivate->setAllowFileAccessFromFileURLs(enabled);
}

void TestRunner::setNeedsStorageAccessFromFileURLsQuirk(bool needsQuirk)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    COMPtr<IWebPreferencesPrivate> prefsPrivate(Query, preferences);
    if (!prefsPrivate)
        return;

    // FIXME: <https://webkit.org/b/164575> Call IWebPreferencesPrivate method when available.
}

void TestRunner::setPopupBlockingEnabled(bool enabled)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    preferences->setJavaScriptCanOpenWindowsAutomatically(!enabled);
}

void TestRunner::setPluginsEnabled(bool flag)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    preferences->setPlugInsEnabled(flag);
}

void TestRunner::setJavaScriptCanAccessClipboard(bool enabled)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    COMPtr<IWebPreferencesPrivate> prefsPrivate(Query, preferences);
    if (!prefsPrivate)
        return;

    prefsPrivate->setJavaScriptCanAccessClipboard(enabled);
}

void TestRunner::setAutomaticLinkDetectionEnabled(bool)
{
    // FIXME: Implement this.
    fprintf(testResult, "ERROR: TestRunner::setAutomaticLinkDetectionEnabled(bool) not implemented\n");
}

void TestRunner::setTabKeyCyclesThroughElements(bool shouldCycle)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    viewPrivate->setTabKeyCyclesThroughElements(shouldCycle ? TRUE : FALSE);
}

void TestRunner::setUserStyleSheetEnabled(bool flag)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    preferences->setUserStyleSheetEnabled(flag);
}

bool appendComponentToPath(wstring& path, const wstring& component)
{
    WCHAR buffer[MAX_PATH];

    if (path.size() + 1 > MAX_PATH)
        return false;

    memcpy(buffer, path.data(), path.size() * sizeof(WCHAR));
    buffer[path.size()] = '\0';

    if (!PathAppendW(buffer, component.c_str()))
        return false;

    path = wstring(buffer);
    return true;
}

static bool followShortcuts(wstring& path)
{
    if (PathFileExists(path.c_str()))
        return true;

    // Do we have a shortcut?
    wstring linkPath = path;
    linkPath.append(TEXT(".lnk"));
    if (!PathFileExists(linkPath.c_str()))
       return true;

    // We have a shortcut, find its target.
    COMPtr<IShellLink> shortcut(Create, CLSID_ShellLink);
    if (!shortcut)
       return false;
    COMPtr<IPersistFile> persistFile(Query, shortcut);
    if (!shortcut)
        return false;
    if (FAILED(persistFile->Load(linkPath.c_str(), STGM_READ)))
        return false;
    if (FAILED(shortcut->Resolve(0, 0)))
        return false;
    WCHAR targetPath[MAX_PATH];
    DWORD targetPathLen = _countof(targetPath);
    if (FAILED(shortcut->GetPath(targetPath, targetPathLen, 0, 0)))
        return false;
    if (!PathFileExists(targetPath))
        return false;
    // Use the target path as the result path instead.
    path = wstring(targetPath);

    return true;
}

static bool resolveCygwinPath(const wstring& cygwinPath, wstring& windowsPath)
{
    wstring fileProtocol = L"file://";
    bool isFileProtocol = cygwinPath.find(fileProtocol) != string::npos;
    if (cygwinPath[isFileProtocol ? 7 : 0] != '/') // ensure path is absolute
        return false;

    // Get the Root path.
    WCHAR rootPath[MAX_PATH];
    DWORD rootPathSize = _countof(rootPath);
    DWORD keyType;
    DWORD result = ::SHGetValueW(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Cygnus Solutions\\Cygwin\\mounts v2\\/"), TEXT("native"), &keyType, &rootPath, &rootPathSize);

    if (result != ERROR_SUCCESS || keyType != REG_SZ) {
        // Cygwin 1.7 doesn't store Cygwin's root as a mount point anymore, because mount points are now stored in /etc/fstab.
        // However, /etc/fstab doesn't contain any information about where / is located as a Windows path, so we need to use Cygwin's
        // new registry key that has the root.
        result = ::SHGetValueW(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Cygwin\\setup"), TEXT("rootdir"), &keyType, &rootPath, &rootPathSize);
        if (result != ERROR_SUCCESS || keyType != REG_SZ)
            return false;
    }

    windowsPath = wstring(rootPath, rootPathSize);

    int oldPos = isFileProtocol ? 8 : 1;
    while (1) {
        int newPos = cygwinPath.find('/', oldPos);

        if (newPos == -1) {
            wstring pathComponent = cygwinPath.substr(oldPos);

            if (!appendComponentToPath(windowsPath, pathComponent))
               return false;

            if (!followShortcuts(windowsPath))
                return false;

            break;
        }

        wstring pathComponent = cygwinPath.substr(oldPos, newPos - oldPos);
        if (!appendComponentToPath(windowsPath, pathComponent))
            return false;

        if (!followShortcuts(windowsPath))
            return false;

        oldPos = newPos + 1;
    }

    if (isFileProtocol)
        windowsPath = fileProtocol + windowsPath;

    return true;
}

void TestRunner::setUserStyleSheetLocation(JSStringRef jsURL)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    RetainPtr<CFStringRef> urlString = adoptCF(JSStringCopyCFString(0, jsURL));
    RetainPtr<CFURLRef> url = adoptCF(CFURLCreateWithString(0, urlString.get(), 0));
    if (!url)
        return;

    // Now copy the file system path, POSIX style.
    RetainPtr<CFStringRef> pathCF = adoptCF(CFURLCopyFileSystemPath(url.get(), kCFURLPOSIXPathStyle));
    if (!pathCF)
        return;

    wstring path = cfStringRefToWString(pathCF.get());

    wstring resultPath;
    if (!resolveCygwinPath(path, resultPath))
        return;

    // The path has been resolved, now convert it back to a CFURL.
    int result = ::WideCharToMultiByte(CP_UTF8, 0, resultPath.c_str(), resultPath.size() + 1, nullptr, 0, nullptr, nullptr);
    Vector<char> utf8Vector(result);
    result = ::WideCharToMultiByte(CP_UTF8, 0, resultPath.c_str(), resultPath.size() + 1, utf8Vector.data(), result, nullptr, nullptr);
    if (!result)
        return;

    url = CFURLCreateFromFileSystemRepresentation(0, (const UInt8*)utf8Vector.data(), utf8Vector.size() - 1, false);
    if (!url)
        return;

    resultPath = cfStringRefToWString(CFURLGetString(url.get()));

    _bstr_t resultPathBSTR(resultPath.data());
    preferences->setUserStyleSheetLocation(resultPathBSTR);
}

void TestRunner::setValueForUser(JSContextRef context, JSValueRef element, JSStringRef value)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> webViewPrivate(Query, webView);
    if (!webViewPrivate)
        return;

    COMPtr<IDOMElement> domElement;
    if (FAILED(webViewPrivate->elementFromJS(context, element, &domElement)))
        return;

    COMPtr<IDOMHTMLInputElement> domInputElement;
    if (FAILED(domElement->QueryInterface(IID_IDOMHTMLInputElement, reinterpret_cast<void**>(&domInputElement))))
        return;

    _bstr_t valueBSTR(JSStringCopyBSTR(value), false);

    domInputElement->setValueForUser(valueBSTR);
}

void TestRunner::dispatchPendingLoadRequests()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    viewPrivate->dispatchPendingLoadRequests();
}

void TestRunner::overridePreference(JSStringRef key, JSStringRef value)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    COMPtr<IWebPreferencesPrivate> prefsPrivate(Query, preferences);
    if (!prefsPrivate)
        return;

    _bstr_t keyBSTR(JSStringCopyBSTR(key), false);
    _bstr_t valueBSTR(JSStringCopyBSTR(value), false);
    prefsPrivate->setPreferenceForTest(keyBSTR, valueBSTR);
}

void TestRunner::removeAllVisitedLinks()
{
    COMPtr<IWebHistory> history;
    if (FAILED(WebKitCreateInstance(CLSID_WebHistory, 0, __uuidof(history), reinterpret_cast<void**>(&history))))
        return;

    COMPtr<IWebHistory> sharedHistory;
    if (FAILED(history->optionalSharedHistory(&sharedHistory)) || !sharedHistory)
        return;

    COMPtr<IWebHistoryPrivate> sharedHistoryPrivate;
    if (FAILED(sharedHistory->QueryInterface(&sharedHistoryPrivate)))
        return;

    sharedHistoryPrivate->removeAllVisitedLinks();
}

void TestRunner::setPersistentUserStyleSheetLocation(JSStringRef jsURL)
{
    RetainPtr<CFStringRef> urlString = adoptCF(JSStringCopyCFString(0, jsURL));
    ::setPersistentUserStyleSheetLocation(urlString.get());
}

void TestRunner::clearPersistentUserStyleSheet()
{
    ::setPersistentUserStyleSheetLocation(nullptr);
}

void TestRunner::setWindowIsKey(bool flag)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    HWND webViewWindow;
    if (FAILED(viewPrivate->viewWindow(&webViewWindow)))
        return;

    ::SendMessage(webViewWindow, flag ? WM_SETFOCUS : WM_KILLFOCUS, (WPARAM)::GetDesktopWindow(), 0);
}

void TestRunner::setViewSize(double width, double height)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    HWND webViewWindow;
    if (FAILED(viewPrivate->viewWindow(&webViewWindow)))
        return;

    ::SetWindowPos(webViewWindow, 0, 0, 0, width, height, SWP_NOMOVE);
}

static void CALLBACK waitUntilDoneWatchdogFired(HWND, UINT, UINT_PTR, DWORD)
{
    gTestRunner->waitToDumpWatchdogTimerFired();
}

void TestRunner::setWaitToDump(bool waitUntilDone)
{
    m_waitToDump = waitUntilDone;
    if (m_waitToDump && !waitToDumpWatchdog)
        waitToDumpWatchdog = SetTimer(0, 0, m_timeout, waitUntilDoneWatchdogFired);
}

int TestRunner::windowCount()
{
    return openWindows().size();
}

void TestRunner::execCommand(JSStringRef name, JSStringRef value)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    _bstr_t nameBSTR(JSStringCopyBSTR(name), false);
    _bstr_t valueBSTR(JSStringCopyBSTR(value), false);
    viewPrivate->executeCoreCommandByName(nameBSTR, valueBSTR);
}

bool TestRunner::findString(JSContextRef context, JSStringRef target, JSObjectRef optionsArray)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return false;

    COMPtr<IWebViewPrivate3> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return false;

    unsigned char options = 0;

    auto lengthPropertyName = adopt(JSStringCreateWithUTF8CString("length"));
    JSValueRef lengthValue = JSObjectGetProperty(context, optionsArray, lengthPropertyName.get(), nullptr);
    if (!JSValueIsNumber(context, lengthValue))
        return false;

    _bstr_t targetBSTR(JSStringCopyBSTR(target), false);

    size_t length = static_cast<size_t>(JSValueToNumber(context, lengthValue, nullptr));
    for (size_t i = 0; i < length; ++i) {
        JSValueRef value = JSObjectGetPropertyAtIndex(context, optionsArray, i, nullptr);
        if (!JSValueIsString(context, value))
            continue;

        auto optionName = adopt(JSValueToStringCopy(context, value, nullptr));

        if (JSStringIsEqualToUTF8CString(optionName.get(), "CaseInsensitive"))
            options |= WebFindOptionsCaseInsensitive;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "AtWordStarts"))
            options |= WebFindOptionsAtWordStarts;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "TreatMedialCapitalAsWordStart"))
            options |= WebFindOptionsTreatMedialCapitalAsWordStart;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "Backwards"))
            options |= WebFindOptionsBackwards;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "WrapAround"))
            options |= WebFindOptionsWrapAround;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "StartInSelection"))
            options |= WebFindOptionsStartInSelection;
    }

    BOOL found = FALSE;
    if (FAILED(viewPrivate->findString(targetBSTR, static_cast<WebFindOptions>(options), &found)))
        return false;

    return found;
}

void TestRunner::setCacheModel(int cacheModel)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    preferences->setCacheModel(static_cast<WebCacheModel>(cacheModel));
}

bool TestRunner::isCommandEnabled(JSStringRef /*name*/)
{
    fprintf(testResult, "ERROR: TestRunner::isCommandEnabled() not implemented\n");
    return false;
}

void TestRunner::waitForPolicyDelegate()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    setWaitToDump(true);
    policyDelegate->setControllerToNotifyDone(this);
    webView->setPolicyDelegate(policyDelegate);
}

static _bstr_t bstrT(JSStringRef jsString)
{
    // The false parameter tells the _bstr_t constructor to adopt the BSTR we pass it.
    return _bstr_t(JSStringCopyBSTR(jsString), false);
}

void TestRunner::addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    COMPtr<IWebViewPrivate2> webView;
    if (FAILED(WebKitCreateInstance(__uuidof(WebView), 0, __uuidof(webView), reinterpret_cast<void**>(&webView))))
        return;

    webView->addOriginAccessWhitelistEntry(bstrT(sourceOrigin).GetBSTR(), bstrT(destinationProtocol).GetBSTR(), bstrT(destinationHost).GetBSTR(), allowDestinationSubdomains);
}

void TestRunner::removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    COMPtr<IWebViewPrivate2> webView;
    if (FAILED(WebKitCreateInstance(__uuidof(WebView), 0, __uuidof(webView), reinterpret_cast<void**>(&webView))))
        return;

    webView->removeOriginAccessWhitelistEntry(bstrT(sourceOrigin).GetBSTR(), bstrT(destinationProtocol).GetBSTR(), bstrT(destinationHost).GetBSTR(), allowDestinationSubdomains);
}

void TestRunner::setScrollbarPolicy(JSStringRef orientation, JSStringRef policy)
{
    // FIXME: implement
}

void TestRunner::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    COMPtr<IWebViewPrivate2> webView;
    if (FAILED(WebKitCreateInstance(__uuidof(WebView), 0, __uuidof(webView), reinterpret_cast<void**>(&webView))))
        return;

    COMPtr<IWebScriptWorld> world;
    if (FAILED(WebKitCreateInstance(__uuidof(WebScriptWorld), 0, __uuidof(world), reinterpret_cast<void**>(&world))))
        return;

    webView->addUserScriptToGroup(_bstr_t(L"org.webkit.DumpRenderTree").GetBSTR(), world.get(), bstrT(source).GetBSTR(),
        nullptr, 0, nullptr, 0, nullptr, runAtStart ? WebInjectAtDocumentStart : WebInjectAtDocumentEnd,
        allFrames ? WebInjectInAllFrames : WebInjectInTopFrameOnly);
}

void TestRunner::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    COMPtr<IWebViewPrivate2> webView;
    if (FAILED(WebKitCreateInstance(__uuidof(WebView), 0, __uuidof(webView), reinterpret_cast<void**>(&webView))))
        return;

    COMPtr<IWebScriptWorld> world;
    if (FAILED(WebKitCreateInstance(__uuidof(WebScriptWorld), 0, __uuidof(world), reinterpret_cast<void**>(&world))))
        return;

    webView->addUserStyleSheetToGroup(_bstr_t(L"org.webkit.DumpRenderTree").GetBSTR(), world.get(), bstrT(source).GetBSTR(),
        nullptr, 0, nullptr, 0, nullptr, allFrames ? WebInjectInAllFrames : WebInjectInTopFrameOnly);
}

void TestRunner::setDeveloperExtrasEnabled(bool enabled)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    COMPtr<IWebPreferencesPrivate> prefsPrivate(Query, preferences);
    if (!prefsPrivate)
        return;

    prefsPrivate->setDeveloperExtrasEnabled(enabled);
}

void TestRunner::showWebInspector()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> viewPrivate(Query, webView);
    if (!viewPrivate)
        return;

    COMPtr<IWebInspector> inspector;
    if (SUCCEEDED(viewPrivate->inspector(&inspector)))
        inspector->show();
}

void TestRunner::closeWebInspector()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> viewPrivate(Query, webView);
    if (!viewPrivate)
        return;

    COMPtr<IWebInspector> inspector;
    if (FAILED(viewPrivate->inspector(&inspector)))
        return;

    inspector->close();
}

void TestRunner::evaluateInWebInspector(JSStringRef script)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> viewPrivate(Query, webView);
    if (!viewPrivate)
        return;

    COMPtr<IWebInspector> inspector;
    if (FAILED(viewPrivate->inspector(&inspector)))
        return;

    COMPtr<IWebInspectorPrivate> inspectorPrivate(Query, inspector);
    if (!inspectorPrivate)
        return;

    inspectorPrivate->evaluateInFrontend(bstrT(script).GetBSTR());
}

JSRetainPtr<JSStringRef> TestRunner::inspectorTestStubURL()
{
    CFBundleRef webkitBundle = webKitBundle();
    if (!webkitBundle)
        return nullptr;

    RetainPtr<CFURLRef> url = adoptCF(CFBundleCopyResourceURL(webkitBundle, CFSTR("TestStub"), CFSTR("html"), CFSTR("WebInspectorUI")));
    if (!url)
        return nullptr;

    return adopt(JSStringCreateWithCFString(CFURLGetString(url.get())));
}

typedef HashMap<unsigned, COMPtr<IWebScriptWorld> > WorldMap;
static WorldMap& worldMap()
{
    static WorldMap& map = *new WorldMap;
    return map;
}

unsigned worldIDForWorld(IWebScriptWorld* world)
{
    WorldMap::const_iterator end = worldMap().end();
    for (WorldMap::const_iterator it = worldMap().begin(); it != end; ++it) {
        if (it->value == world)
            return it->key;
    }

    return 0;
}

void TestRunner::evaluateScriptInIsolatedWorldAndReturnValue(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{
    // FIXME: Implement this.
}

void TestRunner::evaluateScriptInIsolatedWorld(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{
    COMPtr<IWebFramePrivate> framePrivate(Query, frame);
    if (!framePrivate)
        return;

    // A worldID of 0 always corresponds to a new world. Any other worldID corresponds to a world
    // that is created once and cached forever.
    COMPtr<IWebScriptWorld> world;
    if (!worldID) {
        if (FAILED(WebKitCreateInstance(__uuidof(WebScriptWorld), 0, __uuidof(world), reinterpret_cast<void**>(&world))))
            return;
    } else {
        COMPtr<IWebScriptWorld>& worldSlot = worldMap().add(worldID, nullptr).iterator->value;
        if (!worldSlot && FAILED(WebKitCreateInstance(__uuidof(WebScriptWorld), 0, __uuidof(worldSlot), reinterpret_cast<void**>(&worldSlot))))
            return;
        world = worldSlot;
    }

    _bstr_t result;
    if (FAILED(framePrivate->stringByEvaluatingJavaScriptInScriptWorld(world.get(), globalObject, bstrT(script).GetBSTR(), &result.GetBSTR())))
        return;
}

void TestRunner::apiTestNewWindowDataLoadBaseURL(JSStringRef utf8Data, JSStringRef baseURL)
{
    // Nothing implemented here (compare to Mac)
}

void TestRunner::apiTestGoToCurrentBackForwardItem()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebBackForwardList> backForwardList;
    if (FAILED(webView->backForwardList(&backForwardList)))
        return;

    COMPtr<IWebHistoryItem> item;
    if (FAILED(backForwardList->currentItem(&item)))
        return;

    BOOL success;
    webView->goToBackForwardItem(item.get(), &success);
}

void TestRunner::setWebViewEditable(bool editable)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewEditing> viewEditing;
    if (FAILED(webView->QueryInterface(&viewEditing)))
        return;

    viewEditing->setEditable(editable);
}

void TestRunner::authenticateSession(JSStringRef, JSStringRef, JSStringRef)
{
    fprintf(testResult, "ERROR: TestRunner::authenticateSession() not implemented\n");
}

void TestRunner::abortModal()
{
    // Nothing to do
}

void TestRunner::setSerializeHTTPLoads(bool serializeLoads)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    viewPrivate->setLoadResourcesSerially(serializeLoads);
}

void TestRunner::setTextDirection(JSStringRef direction)
{
    COMPtr<IWebFramePrivate> framePrivate(Query, frame);
    if (!framePrivate)
        return;

    framePrivate->setTextDirection(bstrT(direction).GetBSTR());
}

void TestRunner::addChromeInputField()
{
    fprintf(testResult, "ERROR: TestRunner::addChromeInputField() not implemented\n");
}

void TestRunner::removeChromeInputField()
{
    fprintf(testResult, "ERROR: TestRunner::removeChromeInputField() not implemented\n");
}

void TestRunner::focusWebView()
{
    fprintf(testResult, "ERROR: TestRunner::focusWebView() not implemented\n");
}

void TestRunner::setBackingScaleFactor(double)
{
    // Not applicable
}

void TestRunner::resetPageVisibility()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate4> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    viewPrivate->setVisibilityState(WebPageVisibilityStateVisible);
}

void TestRunner::setPageVisibility(const char* newVisibility)
{
    if (!newVisibility)
        return;

    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate4> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    if (!strcmp(newVisibility, "visible"))
        viewPrivate->setVisibilityState(WebPageVisibilityStateVisible);
    else if (!strcmp(newVisibility, "hidden"))
        viewPrivate->setVisibilityState(WebPageVisibilityStateHidden);
    else if (!strcmp(newVisibility, "prerender"))
        viewPrivate->setVisibilityState(WebPageVisibilityStatePrerender);
}

void TestRunner::grantWebNotificationPermission(JSStringRef origin)
{
    // FIXME: Implement.
    // See https://bugs.webkit.org/show_bug.cgi?id=172295
}

void TestRunner::denyWebNotificationPermission(JSStringRef jsOrigin)
{
    // FIXME: Implement.
    // See https://bugs.webkit.org/show_bug.cgi?id=172295
}

void TestRunner::removeAllWebNotificationPermissions()
{
    // FIXME: Implement.
    // See https://bugs.webkit.org/show_bug.cgi?id=172295
}

void TestRunner::simulateWebNotificationClick(JSValueRef jsNotification)
{
    // FIXME: Implement.
    // See https://bugs.webkit.org/show_bug.cgi?id=172295
}

void TestRunner::simulateLegacyWebNotificationClick(JSStringRef title)
{
    // FIXME: Implement.
}

unsigned TestRunner::imageCountInGeneralPasteboard() const
{
    fprintf(testResult, "ERROR: TestRunner::imageCountInGeneralPasteboard() not implemented\n");
    return 0;
}

void TestRunner::setSpellCheckerLoggingEnabled(bool enabled)
{
    fprintf(testResult, "ERROR: TestRunner::setSpellCheckerLoggingEnabled() not implemented\n");
}

void TestRunner::setSpellCheckerResults(JSContextRef, JSObjectRef)
{
    fprintf(testResult, "ERROR: TestRunner::setSpellCheckerResults() not implemented\n");
}
