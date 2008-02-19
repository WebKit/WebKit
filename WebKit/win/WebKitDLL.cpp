/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "WebKitDLL.h"

#include "autoversion.h"
#include "ForEachCoClass.h"
#include "ProgIDMacros.h"
#include "resource.h"
#include "WebKit.h"
#include "WebKitClassFactory.h"
#include "WebScriptDebugServer.h"
#pragma warning( push, 0 )
#include <WebCore/COMPtr.h>
#include <WebCore/IconDatabase.h>
#include <WebCore/Page.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/Widget.h>
#include <wtf/Vector.h>
#pragma warning(pop)
#include <tchar.h>
#include <olectl.h>

ULONG gLockCount;
ULONG gClassCount;
HINSTANCE gInstance;

#define CLSID_FOR_CLASS(cls) CLSID_##cls,
static CLSID gRegCLSIDs[] = {
    FOR_EACH_COCLASS(CLSID_FOR_CLASS)
};
#undef CLSID_FOR_CLASS

void shutDownWebKit()
{
    WebCore::iconDatabase()->close();
}

STDAPI_(BOOL) DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID /*lpReserved*/)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            gLockCount = gClassCount = 0;
            gInstance = hModule;
            WebCore::Page::setInstanceHandle(hModule);
            return TRUE;

        case DLL_PROCESS_DETACH:
            shutDownWebKit();
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    bool found = false;
    for (int i = 0; i < ARRAYSIZE(gRegCLSIDs); i++) {
        if (IsEqualGUID(rclsid, gRegCLSIDs[i])) {
            found = true;
            break;
        }
    }
    if (!found)
        return E_FAIL;

    if (!IsEqualGUID(riid, IID_IUnknown) && !IsEqualGUID(riid, IID_IClassFactory))
        return E_NOINTERFACE;

    WebKitClassFactory* factory = new WebKitClassFactory(rclsid);
    *ppv = reinterpret_cast<LPVOID>(factory);
    if (!factory)
        return E_OUTOFMEMORY;

    factory->AddRef();
    return S_OK;
}

STDAPI DllCanUnloadNow(void)
{
    if (!gClassCount && !gLockCount)
        return S_OK;
    
    return S_FALSE;
}

#if __PRODUCTION__
#define VERSION_INDEPENDENT_PROGID(className) VERSION_INDEPENDENT_PRODUCTION_PROGID(className)
#else
#define VERSION_INDEPENDENT_PROGID(className) VERSION_INDEPENDENT_OPENSOURCE_PROGID(className)
#endif
#define CURRENT_VERSIONED_PROGID(className) VERSIONED_PROGID(VERSION_INDEPENDENT_PROGID(className), CURRENT_PROGID_VERSION)
#define VERSIONED_303_PROGID(className) VERSIONED_PROGID(VERSION_INDEPENDENT_PROGID(className), 3)

// FIXME: The last line of this macro only here for the benefit of Safari 3.0.3. Once a newer version
// is released, the last line should be removed and gSlotsPerEntry should be decremented by 1.
//key                                                                                       value name              value }
#define KEYS_FOR_CLASS(cls) \
{ TEXT("CLSID\\{########-####-####-####-############}"),                                    0,                      TEXT(#cls) }, \
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    0,                      (LPCTSTR)-1 }, \
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") }, \
{ TEXT("CLSID\\{########-####-####-####-############}\\ProgID"),                            0,                      CURRENT_VERSIONED_PROGID(cls) }, \
{ CURRENT_VERSIONED_PROGID(cls),                                                            0,                      TEXT(#cls) }, \
{ CURRENT_VERSIONED_PROGID(cls) TEXT("\\CLSID"),                                            0,                      TEXT("{########-####-####-####-############}") }, \
{ TEXT("CLSID\\{########-####-####-####-############}\\VersionIndependentProgID"),          0,                      VERSION_INDEPENDENT_PROGID(cls) }, \
{ VERSION_INDEPENDENT_PROGID(cls),                                                          0,                      TEXT(#cls) }, \
{ VERSION_INDEPENDENT_PROGID(cls) TEXT("\\CLSID"),                                          0,                      TEXT("{########-####-####-####-############}") }, \
{ VERSION_INDEPENDENT_PROGID(cls) TEXT("\\CurVer"),                                         0,                      STRINGIFIED_VERSION(CURRENT_PROGID_VERSION) }, \
{ VERSIONED_303_PROGID(cls),                                                                0,                      TEXT(#cls) }, \
{ VERSIONED_303_PROGID(cls) TEXT("\\CLSID"),                                                0,                      TEXT("{########-####-####-####-############}") }, \
// end of macro

static const int gSlotsPerEntry = 12;
static LPCTSTR gRegTable[][3] = {
    FOR_EACH_COCLASS(KEYS_FOR_CLASS)
};
#undef KEYS_FOR_CLASS

static void substituteGUID(LPTSTR str, const UUID* guid)
{
    if (!guid || !str)
        return;

    TCHAR uuidString[40];
    _stprintf_s(uuidString, ARRAYSIZE(uuidString), TEXT("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"), guid->Data1, guid->Data2, guid->Data3,
        guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);

    LPCTSTR guidPattern = TEXT("########-####-####-####-############");
    size_t patternLength = _tcslen(guidPattern);
    size_t strLength = _tcslen(str);
    LPTSTR guidSubStr = str;
    while (strLength) {
        guidSubStr = _tcsstr(guidSubStr, guidPattern);
        if (!guidSubStr)
            break;
        _tcsncpy(guidSubStr, uuidString, patternLength);
        guidSubStr += patternLength;
        strLength -= (guidSubStr - str);
    }
}

STDAPI DllUnregisterServer(void)
{
    HRESULT hr = S_OK;
    HKEY userClasses;

#if __PRODUCTION__
    const GUID libID = LIBID_WebKit;
#else
    const GUID libID = LIBID_OpenSourceWebKit;
#endif

    typedef HRESULT (WINAPI *UnRegisterTypeLibForUserPtr)(REFGUID, unsigned short, unsigned short, LCID, SYSKIND);
    if (UnRegisterTypeLibForUserPtr unRegisterTypeLibForUser = reinterpret_cast<UnRegisterTypeLibForUserPtr>(GetProcAddress(GetModuleHandle(TEXT("oleaut32.dll")), "UnRegisterTypeLibForUser")))
        unRegisterTypeLibForUser(libID, __BUILD_NUMBER_MAJOR__, __BUILD_NUMBER_MINOR__, 0, SYS_WIN32);
    else
        UnRegisterTypeLib(libID, __BUILD_NUMBER_MAJOR__, __BUILD_NUMBER_MINOR__, 0, SYS_WIN32);

    if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\CLASSES"), 0, KEY_WRITE, &userClasses) != ERROR_SUCCESS)
        userClasses = 0;

    int nEntries = ARRAYSIZE(gRegTable);
    for (int i = nEntries - 1; i >= 0; i--) {
        LPTSTR pszKeyName = _tcsdup(gRegTable[i][0]);
        if (pszKeyName) {
            substituteGUID(pszKeyName, &gRegCLSIDs[i/gSlotsPerEntry]);
            RegDeleteKey(HKEY_CLASSES_ROOT, pszKeyName);
            if (userClasses)
                RegDeleteKey(userClasses, pszKeyName);
            free(pszKeyName);
        } else
            hr = E_OUTOFMEMORY;
    }

    if (userClasses)
        RegCloseKey(userClasses);
    return hr;
}

STDAPI DllRegisterServer(void)
{
    HRESULT hr = S_OK;

    // look up server's file name
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(gInstance, szFileName, MAX_PATH);

    typedef HRESULT (WINAPI *RegisterTypeLibForUserPtr)(ITypeLib*, OLECHAR*, OLECHAR*);
    COMPtr<ITypeLib> typeLib;
    LoadTypeLibEx(szFileName, REGKIND_NONE, &typeLib);
    if (RegisterTypeLibForUserPtr registerTypeLibForUser = reinterpret_cast<RegisterTypeLibForUserPtr>(GetProcAddress(GetModuleHandle(TEXT("oleaut32.dll")), "RegisterTypeLibForUser")))
        registerTypeLibForUser(typeLib.get(), szFileName, 0);
    else
        RegisterTypeLib(typeLib.get(), szFileName, 0);

    HKEY userClasses;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\CLASSES"), 0, KEY_WRITE, &userClasses) != ERROR_SUCCESS)
        userClasses = 0;

    // register entries from table
    int nEntries = ARRAYSIZE(gRegTable);
    for (int i = 0; SUCCEEDED(hr) && i < nEntries; i++) {
        LPTSTR pszKeyName   = _tcsdup(gRegTable[i][0]);
        LPTSTR pszValueName = gRegTable[i][1] ? _tcsdup(gRegTable[i][1]) : 0;
        LPTSTR allocatedValue   = (gRegTable[i][2] != (LPTSTR)-1) ? _tcsdup(gRegTable[i][2]) : (LPTSTR)-1;
        LPTSTR pszValue     = allocatedValue;

        if (pszKeyName && pszValue) {

            int clsidIndex = i/gSlotsPerEntry;
            substituteGUID(pszKeyName, &gRegCLSIDs[clsidIndex]);
            substituteGUID(pszValueName, &gRegCLSIDs[clsidIndex]);

            // map rogue value to module file name
            if (pszValue == (LPTSTR)-1)
                pszValue = szFileName;
            else
                substituteGUID(pszValue, &gRegCLSIDs[clsidIndex]);

            // create the key
            HKEY hkey;
            LONG err = RegCreateKey(HKEY_CLASSES_ROOT, pszKeyName, &hkey);
            if (err != ERROR_SUCCESS && userClasses)
                err = RegCreateKey(userClasses, pszKeyName, &hkey);
            if (err == ERROR_SUCCESS) {
                // set the value
                err = RegSetValueEx(hkey, pszValueName, 0, REG_SZ, (const BYTE*)pszValue, (DWORD) sizeof(pszValue[0])*(_tcslen(pszValue) + 1));
                RegCloseKey(hkey);
            }
        }
        if (pszKeyName)
            free(pszKeyName);
        if (pszValueName)
            free(pszValueName);
        if (allocatedValue && allocatedValue != (LPTSTR)-1)
            free(allocatedValue);
    }

    if (userClasses)
        RegCloseKey(userClasses);

    return hr;
}

STDAPI RunAsLocalServer()
{
    DWORD reg;
    COMPtr<IUnknown> classFactory;
    DllGetClassObject(CLSID_WebScriptDebugServer, IID_IUnknown, (void**)&classFactory);
    CoRegisterClassObject(CLSID_WebScriptDebugServer, classFactory.get(), CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &reg);
    return 0;
}

STDAPI LocalServerDidDie()
{
    WebScriptDebugServer::sharedWebScriptDebugServer()->serverDidDie();
    return 0;
}

//FIXME: We should consider moving this to a new file for cross-project functionality
PassRefPtr<WebCore::SharedBuffer> loadResourceIntoBuffer(const char* name)
{
    int idr;
    // temporary hack to get resource id
    if (!strcmp(name, "textAreaResizeCorner"))
        idr = IDR_RESIZE_CORNER;
    else if (!strcmp(name, "missingImage"))
        idr = IDR_MISSING_IMAGE;
    else if (!strcmp(name, "urlIcon"))
        idr = IDR_URL_ICON;
    else if (!strcmp(name, "nullPlugin"))
        idr = IDR_NULL_PLUGIN;
    else if (!strcmp(name, "zoomInCursor"))
        idr = IDR_ZOOM_IN_CURSOR;
    else if (!strcmp(name, "zoomOutCursor"))
        idr = IDR_ZOOM_OUT_CURSOR;
    else if (!strcmp(name, "verticalTextCursor"))
        idr = IDR_VERTICAL_TEXT_CURSOR;
    else
        return 0;

    HRSRC resInfo = FindResource(gInstance, MAKEINTRESOURCE(idr), L"PNG");
    if (!resInfo)
        return 0;
    HANDLE res = LoadResource(gInstance, resInfo);
    if (!res)
        return 0;
    void* resource = LockResource(res);
    if (!resource)
        return 0;
    int size = SizeofResource(gInstance, resInfo);

    return new WebCore::SharedBuffer(reinterpret_cast<const char*>(resource), size);
}
