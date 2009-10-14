/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "LayoutTestController.h"

#include "DumpRenderTree.h"
#include "EditingDelegate.h"
#include "PolicyDelegate.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include <CoreFoundation/CoreFoundation.h>
#include <JavaScriptCore/Assertions.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRefBSTR.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebCore/COMPtr.h>
#include <WebKit/WebKit.h>
#include <WebKit/WebKitCOMAPI.h>
#include <comutil.h>
#include <shlwapi.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <string>
#include <wtf/Platform.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

using std::string;
using std::wstring;

static bool resolveCygwinPath(const wstring& cygwinPath, wstring& windowsPath);

LayoutTestController::~LayoutTestController()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    // reset webview-related states back to default values in preparation for next test

    COMPtr<IWebViewPrivate> viewPrivate;
    if (SUCCEEDED(webView->QueryInterface(&viewPrivate)))
        viewPrivate->setTabKeyCyclesThroughElements(TRUE);

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

void LayoutTestController::addDisallowedURL(JSStringRef url)
{
    // FIXME: Implement!
}

void LayoutTestController::clearBackForwardList()
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

JSStringRef LayoutTestController::copyDecodedHostName(JSStringRef name)
{
    // FIXME: Implement!
    return 0;
}

JSStringRef LayoutTestController::copyEncodedHostName(JSStringRef name)
{
    // FIXME: Implement!
    return 0;
}

void LayoutTestController::disableImageLoading()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;
    
    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;
    
    preferences->setLoadsImagesAutomatically(FALSE);
}

void LayoutTestController::dispatchPendingLoadRequests()
{
    // FIXME: Implement for testing fix for 6727495
}

void LayoutTestController::display()
{
    displayWebView();
}

void LayoutTestController::keepWebHistory()
{
    COMPtr<IWebHistory> history;
    if (FAILED(WebKitCreateInstance(CLSID_WebHistory, 0, __uuidof(history), reinterpret_cast<void**>(&history))))
        return;

    COMPtr<IWebHistory> sharedHistory;
    if (FAILED(WebKitCreateInstance(CLSID_WebHistory, 0, __uuidof(sharedHistory), reinterpret_cast<void**>(&sharedHistory))))
        return;

    history->setOptionalSharedHistory(sharedHistory.get());
}

void LayoutTestController::waitForPolicyDelegate()
{
    // FIXME: Implement this.
}

size_t LayoutTestController::webHistoryItemCount()
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

    int count;
    if (FAILED(sharedHistoryPrivate->allItems(&count, 0)))
        return 0;

    return count;
}

unsigned LayoutTestController::workerThreadCount() const
{
    COMPtr<IWebWorkersPrivate> workers;
    if (FAILED(WebKitCreateInstance(CLSID_WebWorkersPrivate, 0, __uuidof(workers), reinterpret_cast<void**>(&workers))))
        return 0;
    unsigned count;
    if (FAILED(workers->workerThreadCount(&count)))
        return 0;
    return count;
}

void LayoutTestController::notifyDone()
{
    // Same as on mac.  This can be shared.
    if (m_waitToDump && !topLoadingFrame && !WorkQueue::shared()->count())
        dump();
    m_waitToDump = false;
}

JSStringRef LayoutTestController::pathToLocalResource(JSContextRef context, JSStringRef url)
{
    wstring input(JSStringGetCharactersPtr(url), JSStringGetLength(url));

    wstring localPath;
    if (!resolveCygwinPath(input, localPath)) {
        printf("ERROR: Failed to resolve Cygwin path %S\n", input.c_str());
        return 0;
    }

    return JSStringCreateWithCharacters(localPath.c_str(), localPath.length());
}

static wstring jsStringRefToWString(JSStringRef jsStr)
{
    size_t length = JSStringGetLength(jsStr);
    Vector<WCHAR> buffer(length + 1);
    memcpy(buffer.data(), JSStringGetCharactersPtr(jsStr), length * sizeof(WCHAR));
    buffer[length] = '\0';

    return buffer.data();
}

void LayoutTestController::queueLoad(JSStringRef url, JSStringRef target)
{
    COMPtr<IWebDataSource> dataSource;
    if (FAILED(frame->dataSource(&dataSource)))
        return;

    COMPtr<IWebURLResponse> response;
    if (FAILED(dataSource->response(&response)) || !response)
        return;

    BSTR responseURLBSTR;
    if (FAILED(response->URL(&responseURLBSTR)))
        return;
    wstring responseURL(responseURLBSTR, SysStringLen(responseURLBSTR));
    SysFreeString(responseURLBSTR);

    // FIXME: We should do real relative URL resolution here.
    int lastSlash = responseURL.rfind('/');
    if (lastSlash != -1)
        responseURL = responseURL.substr(0, lastSlash);

    wstring wURL = jsStringRefToWString(url);
    wstring wAbsoluteURL = responseURL + TEXT("/") + wURL;
    JSRetainPtr<JSStringRef> jsAbsoluteURL(Adopt, JSStringCreateWithCharacters(wAbsoluteURL.data(), wAbsoluteURL.length()));

    WorkQueue::shared()->queue(new LoadItem(jsAbsoluteURL.get(), target));
}

void LayoutTestController::setAcceptsEditing(bool acceptsEditing)
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

void LayoutTestController::setAlwaysAcceptCookies(bool alwaysAcceptCookies)
{
    if (alwaysAcceptCookies == m_alwaysAcceptCookies)
        return;

    if (!::setAlwaysAcceptCookies(alwaysAcceptCookies))
        return;
    m_alwaysAcceptCookies = alwaysAcceptCookies;
}

void LayoutTestController::setAuthorAndUserStylesEnabled(bool flag)
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

void LayoutTestController::setCustomPolicyDelegate(bool setDelegate, bool permissive)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    if (setDelegate) {
        policyDelegate->setPermissive(permissive);
        webView->setPolicyDelegate(policyDelegate);
    } else
        webView->setPolicyDelegate(0);
}

void LayoutTestController::setMockGeolocationPosition(double latitude, double longitude, double accuracy)
{
    // FIXME: Implement for Geolocation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=28264.
}

void LayoutTestController::setMockGeolocationError(int code, JSStringRef message)
{
    // FIXME: Implement for Geolocation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=28264.
}

void LayoutTestController::setIconDatabaseEnabled(bool iconDatabaseEnabled)
{
    // See also <rdar://problem/6480108>
    COMPtr<IWebIconDatabase> iconDatabase;
    COMPtr<IWebIconDatabase> tmpIconDatabase;
    if (FAILED(WebKitCreateInstance(CLSID_WebIconDatabase, 0, IID_IWebIconDatabase, (void**)&tmpIconDatabase)))
        return;
    if (FAILED(tmpIconDatabase->sharedIconDatabase(&iconDatabase)))
        return;

    iconDatabase->setEnabled(iconDatabaseEnabled);
}

void LayoutTestController::setMainFrameIsFirstResponder(bool flag)
{
    // FIXME: Implement!
}

void LayoutTestController::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    preferences->setPrivateBrowsingEnabled(privateBrowsingEnabled);
}

void LayoutTestController::setXSSAuditorEnabled(bool enabled)
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

void LayoutTestController::setPopupBlockingEnabled(bool enabled)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    preferences->setJavaScriptCanOpenWindowsAutomatically(!enabled);
}

void LayoutTestController::setTabKeyCyclesThroughElements(bool shouldCycle)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    viewPrivate->setTabKeyCyclesThroughElements(shouldCycle ? TRUE : FALSE);
}

void LayoutTestController::setUseDashboardCompatibilityMode(bool flag)
{
    // FIXME: Implement!
}

void LayoutTestController::setUserStyleSheetEnabled(bool flag)
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
    if (cygwinPath[isFileProtocol ? 7 : 0] != '/')  // ensure path is absolute
        return false;

    // Get the Root path.
    WCHAR rootPath[MAX_PATH];
    DWORD rootPathSize = _countof(rootPath);
    DWORD keyType;
    DWORD result = ::SHGetValueW(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Cygnus Solutions\\Cygwin\\mounts v2\\/"), TEXT("native"), &keyType, &rootPath, &rootPathSize);

    if (result != ERROR_SUCCESS || keyType != REG_SZ)
        return false;

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

static wstring cfStringRefToWString(CFStringRef cfStr)
{
    Vector<wchar_t> v(CFStringGetLength(cfStr));
    CFStringGetCharacters(cfStr, CFRangeMake(0, CFStringGetLength(cfStr)), (UniChar *)v.data());

    return wstring(v.data(), v.size());
}

void LayoutTestController::setUserStyleSheetLocation(JSStringRef jsURL)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    RetainPtr<CFStringRef> urlString(AdoptCF, JSStringCopyCFString(0, jsURL));
    RetainPtr<CFURLRef> url(AdoptCF, CFURLCreateWithString(0, urlString.get(), 0));
    if (!url)
        return;

    // Now copy the file system path, POSIX style.
    RetainPtr<CFStringRef> pathCF(AdoptCF, CFURLCopyFileSystemPath(url.get(), kCFURLPOSIXPathStyle));
    if (!pathCF)
        return;

    wstring path = cfStringRefToWString(pathCF.get());

    wstring resultPath;
    if (!resolveCygwinPath(path, resultPath))
        return;

    // The path has been resolved, now convert it back to a CFURL.
    int result = WideCharToMultiByte(CP_UTF8, 0, resultPath.c_str(), resultPath.size() + 1, 0, 0, 0, 0);
    Vector<char> utf8Vector(result);
    result = WideCharToMultiByte(CP_UTF8, 0, resultPath.c_str(), resultPath.size() + 1, utf8Vector.data(), result, 0, 0);
    if (!result)
        return;

    url = CFURLCreateFromFileSystemRepresentation(0, (const UInt8*)utf8Vector.data(), utf8Vector.size() - 1, false);
    if (!url)
        return;

    resultPath = cfStringRefToWString(CFURLGetString(url.get()));

    BSTR resultPathBSTR = SysAllocStringLen(resultPath.data(), resultPath.size());
    preferences->setUserStyleSheetLocation(resultPathBSTR);
    SysFreeString(resultPathBSTR);
}

void LayoutTestController::setPersistentUserStyleSheetLocation(JSStringRef jsURL)
{
    RetainPtr<CFStringRef> urlString(AdoptCF, JSStringCopyCFString(0, jsURL));
    ::setPersistentUserStyleSheetLocation(urlString.get());
}

void LayoutTestController::clearPersistentUserStyleSheet()
{
    ::setPersistentUserStyleSheetLocation(0);
}

void LayoutTestController::setWindowIsKey(bool flag)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    HWND webViewWindow;
    if (FAILED(viewPrivate->viewWindow((OLE_HANDLE*)&webViewWindow)))
        return;

    ::SendMessage(webViewWindow, flag ? WM_SETFOCUS : WM_KILLFOCUS, (WPARAM)::GetDesktopWindow(), 0);
}

void LayoutTestController::setSmartInsertDeleteEnabled(bool flag)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewEditing> viewEditing;
    if (FAILED(webView->QueryInterface(&viewEditing)))
        return;

    viewEditing->setSmartInsertDeleteEnabled(flag ? TRUE : FALSE);
}

void LayoutTestController::setJavaScriptProfilingEnabled(bool flag)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return;

    COMPtr<IWebPreferencesPrivate> prefsPrivate(Query, preferences);
    if (!prefsPrivate)
        return;

    COMPtr<IWebInspector> inspector;
    if (FAILED(viewPrivate->inspector(&inspector)))
        return;

    prefsPrivate->setDeveloperExtrasEnabled(flag);
    inspector->setJavaScriptProfilingEnabled(flag);
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(bool flag)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewEditing> viewEditing;
    if (FAILED(webView->QueryInterface(&viewEditing)))
        return;

    viewEditing->setSelectTrailingWhitespaceEnabled(flag ? TRUE : FALSE);
}

static const CFTimeInterval waitToDumpWatchdogInterval = 15.0;

static void CALLBACK waitUntilDoneWatchdogFired(HWND, UINT, UINT_PTR, DWORD)
{
    gLayoutTestController->waitToDumpWatchdogTimerFired();
}

void LayoutTestController::setWaitToDump(bool waitUntilDone)
{
    m_waitToDump = waitUntilDone;
    if (m_waitToDump && !waitToDumpWatchdog)
        waitToDumpWatchdog = SetTimer(0, 0, waitToDumpWatchdogInterval * 1000, waitUntilDoneWatchdogFired);
}

int LayoutTestController::windowCount()
{
    return openWindows().size();
}

bool LayoutTestController::elementDoesAutoCompleteForElementWithId(JSStringRef id)
{
    COMPtr<IDOMDocument> document;
    if (FAILED(frame->DOMDocument(&document)))
        return false;

    wstring idWstring = jsStringRefToWString(id);
    BSTR idBSTR = SysAllocStringLen((OLECHAR*)idWstring.c_str(), idWstring.length());
    COMPtr<IDOMElement> element;
    HRESULT result = document->getElementById(idBSTR, &element);
    SysFreeString(idBSTR);

    if (FAILED(result))
        return false;

    COMPtr<IWebFramePrivate> framePrivate(Query, frame);
    if (!framePrivate)
        return false;

    BOOL autoCompletes;
    if (FAILED(framePrivate->elementDoesAutoComplete(element.get(), &autoCompletes)))
        return false;

    return autoCompletes;
}

void LayoutTestController::execCommand(JSStringRef name, JSStringRef value)
{
    wstring wName = jsStringRefToWString(name);
    wstring wValue = jsStringRefToWString(value);

    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    BSTR nameBSTR = SysAllocStringLen((OLECHAR*)wName.c_str(), wName.length());
    BSTR valueBSTR = SysAllocStringLen((OLECHAR*)wValue.c_str(), wValue.length());
    viewPrivate->executeCoreCommandByName(nameBSTR, valueBSTR);

    SysFreeString(nameBSTR);
    SysFreeString(valueBSTR);
}

void LayoutTestController::setCacheModel(int)
{
    // FIXME: Implement
}

bool LayoutTestController::isCommandEnabled(JSStringRef /*name*/)
{
    printf("ERROR: LayoutTestController::isCommandEnabled() not implemented\n");
    return false;
}

void LayoutTestController::clearAllDatabases()
{
    COMPtr<IWebDatabaseManager> databaseManager;
    COMPtr<IWebDatabaseManager> tmpDatabaseManager;
    if (FAILED(WebKitCreateInstance(CLSID_WebDatabaseManager, 0, IID_IWebDatabaseManager, (void**)&tmpDatabaseManager)))
        return;
    if (FAILED(tmpDatabaseManager->sharedWebDatabaseManager(&databaseManager)))
        return;

    databaseManager->deleteAllDatabases();
}

void LayoutTestController::overridePreference(JSStringRef key, JSStringRef value)
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

    BSTR keyBSTR = JSStringCopyBSTR(key);
    BSTR valueBSTR = JSStringCopyBSTR(value);
    prefsPrivate->setPreferenceForTest(keyBSTR, valueBSTR);
    SysFreeString(keyBSTR);
    SysFreeString(valueBSTR);
}

void LayoutTestController::setDatabaseQuota(unsigned long long quota)
{
    COMPtr<IWebDatabaseManager> databaseManager;
    COMPtr<IWebDatabaseManager> tmpDatabaseManager;

    if (FAILED(WebKitCreateInstance(CLSID_WebDatabaseManager, 0, IID_IWebDatabaseManager, (void**)&tmpDatabaseManager)))
        return;
    if (FAILED(tmpDatabaseManager->sharedWebDatabaseManager(&databaseManager)))
        return;

    databaseManager->setQuota(TEXT("file:///"), quota);
}

void LayoutTestController::setAppCacheMaximumSize(unsigned long long size)
{
    printf("ERROR: LayoutTestController::setAppCacheMaximumSize() not implemented\n");
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{
    COMPtr<IDOMDocument> document;
    if (FAILED(frame->DOMDocument(&document)))
        return false;

    BSTR idBSTR = JSStringCopyBSTR(elementId);
    COMPtr<IDOMElement> element;
    HRESULT hr = document->getElementById(idBSTR, &element);
    SysFreeString(idBSTR);
    if (FAILED(hr))
        return false;

    COMPtr<IWebFramePrivate> framePrivate(Query, frame);
    if (!framePrivate)
        return false;

    BSTR nameBSTR = JSStringCopyBSTR(animationName);
    BOOL wasRunning = FALSE;
    hr = framePrivate->pauseAnimation(nameBSTR, element.get(), time, &wasRunning);
    SysFreeString(nameBSTR);

    return SUCCEEDED(hr) && wasRunning;
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId)
{
    COMPtr<IDOMDocument> document;
    if (FAILED(frame->DOMDocument(&document)))
        return false;

    BSTR idBSTR = JSStringCopyBSTR(elementId);
    COMPtr<IDOMElement> element;
    HRESULT hr = document->getElementById(idBSTR, &element);
    SysFreeString(idBSTR);
    if (FAILED(hr))
        return false;

    COMPtr<IWebFramePrivate> framePrivate(Query, frame);
    if (!framePrivate)
        return false;

    BSTR nameBSTR = JSStringCopyBSTR(propertyName);
    BOOL wasRunning = FALSE;
    hr = framePrivate->pauseTransition(nameBSTR, element.get(), time, &wasRunning);
    SysFreeString(nameBSTR);

    return SUCCEEDED(hr) && wasRunning;
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    COMPtr<IWebFramePrivate> framePrivate(Query, frame);
    if (!framePrivate)
        return 0;

    UINT number = 0;
    if (FAILED(framePrivate->numberOfActiveAnimations(&number)))
        return 0;

    return number;
}

static _bstr_t bstrT(JSStringRef jsString)
{
    // The false parameter tells the _bstr_t constructor to adopt the BSTR we pass it.
    return _bstr_t(JSStringCopyBSTR(jsString), false);
}

void LayoutTestController::whiteListAccessFromOrigin(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    COMPtr<IWebViewPrivate> webView;
    if (FAILED(WebKitCreateInstance(__uuidof(WebView), 0, __uuidof(webView), reinterpret_cast<void**>(&webView))))
        return;

    webView->whiteListAccessFromOrigin(bstrT(sourceOrigin).GetBSTR(), bstrT(destinationProtocol).GetBSTR(), bstrT(destinationHost).GetBSTR(), allowDestinationSubdomains);
}

void LayoutTestController::addUserScript(JSStringRef source, bool runAtStart)
{
    COMPtr<IWebViewPrivate> webView;
    if (FAILED(WebKitCreateInstance(__uuidof(WebView), 0, __uuidof(webView), reinterpret_cast<void**>(&webView))))
        return;

    webView->addUserScriptToGroup(_bstr_t(L"org.webkit.DumpRenderTree").GetBSTR(), 1, bstrT(source).GetBSTR(), 0, 0, 0, 0, 0, runAtStart ? WebInjectAtDocumentStart : WebInjectAtDocumentEnd);
}


void LayoutTestController::addUserStyleSheet(JSStringRef source)
{
    COMPtr<IWebViewPrivate> webView;
    if (FAILED(WebKitCreateInstance(__uuidof(WebView), 0, __uuidof(webView), reinterpret_cast<void**>(&webView))))
        return;

    webView->addUserStyleSheetToGroup(_bstr_t(L"org.webkit.DumpRenderTree").GetBSTR(), 1, bstrT(source).GetBSTR(), 0, 0, 0, 0, 0);
}

void LayoutTestController::showWebInspector()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate> viewPrivate(Query, webView);
    if (!viewPrivate)
        return;

    COMPtr<IWebInspector> inspector;
    if (SUCCEEDED(viewPrivate->inspector(&inspector)))
        inspector->show();
}

void LayoutTestController::closeWebInspector()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate> viewPrivate(Query, webView);
    if (!viewPrivate)
        return;

    COMPtr<IWebInspector> inspector;
    if (SUCCEEDED(viewPrivate->inspector(&inspector)))
        inspector->close();
}

void LayoutTestController::evaluateInWebInspector(long callId, JSStringRef script)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate> viewPrivate(Query, webView);
    if (!viewPrivate)
        return;

    COMPtr<IWebInspector> inspector;
    if (FAILED(viewPrivate->inspector(&inspector)))
        return;

    COMPtr<IWebInspectorPrivate> inspectorPrivate(Query, inspector);
    if (!inspectorPrivate)
        return;

    inspectorPrivate->evaluateInFrontend(callId, bstrT(script).GetBSTR());
}

void LayoutTestController::removeAllVisitedLinks()
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
