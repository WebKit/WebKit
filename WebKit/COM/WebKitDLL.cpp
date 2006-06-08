/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "WebKitDLL.h"
#include "IWebURLResponse.h"
#include "resource.h"
#include "WebKit.h"
#include "WebKitClassFactory.h"

#pragma warning( push, 0 )
#include "DeprecatedArray.h"
#pragma warning(pop)

#include <tchar.h>
#include <olectl.h>

ULONG gLockCount;
ULONG gClassCount;
HINSTANCE gInstance;

STDAPI_(BOOL) DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID /*lpReserved*/)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            gLockCount = gClassCount = 0;
            gInstance = hModule;
            return TRUE;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    if (!IsEqualGUID(rclsid, CLSID_WebView) && !IsEqualGUID(rclsid, CLSID_WebIconDatabase) && !IsEqualGUID(rclsid, CLSID_WebMutableURLRequest))
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

static LPCTSTR gRegTable[][3] = {
//key                                                                                       value name              value }

{ TEXT("CLSID\\{ED2B13E0-66AB-4cc2-9B9F-5B8F85D9AD1D}"),                                    0,                      TEXT("WebView") },
{ TEXT("CLSID\\{ED2B13E0-66AB-4cc2-9B9F-5B8F85D9AD1D}\\InprocServer32"),                    0,                      (LPCTSTR)-1 },
{ TEXT("CLSID\\{ED2B13E0-66AB-4cc2-9B9F-5B8F85D9AD1D}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") },
{ TEXT("CLSID\\{ED2B13E0-66AB-4cc2-9B9F-5B8F85D9AD1D}\\ProgID"),                            0,                      TEXT("WebKit.WebView.3") },
{ TEXT("CLSID\\{ED2B13E0-66AB-4cc2-9B9F-5B8F85D9AD1D}\\VersionIndependentProgID"),          0,                      TEXT("WebKit.WebView") },
{ TEXT("WebKit.WebView.3"),                                                                 0,                      TEXT("WebView") },
{ TEXT("WebKit.WebView.3\\CLSID"),                                                          0,                      TEXT("{ED2B13E0-66AB-4cc2-9B9F-5B8F85D9AD1D}") },

{ TEXT("CLSID\\{982FE6C0-60BB-4c12-A3D4-0381428CCCB1}"),                                    0,                      TEXT("WebIconDatabase") },
{ TEXT("CLSID\\{982FE6C0-60BB-4c12-A3D4-0381428CCCB1}\\InprocServer32"),                    0,                      (LPCTSTR)-1 },
{ TEXT("CLSID\\{982FE6C0-60BB-4c12-A3D4-0381428CCCB1}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") },
{ TEXT("CLSID\\{982FE6C0-60BB-4c12-A3D4-0381428CCCB1}\\ProgID"),                            0,                      TEXT("WebKit.WebIconDatabase.3") },
{ TEXT("CLSID\\{982FE6C0-60BB-4c12-A3D4-0381428CCCB1}\\VersionIndependentProgID"),          0,                      TEXT("WebKit.WebIconDatabase") },
{ TEXT("WebKit.WebIconDatabase.3"),                                                         0,                      TEXT("WebIconDatabase") },
{ TEXT("WebKit.WebIconDatabase.3\\CLSID"),                                                  0,                      TEXT("{982FE6C0-60BB-4c12-A3D4-0381428CCCB1}") },

{ TEXT("CLSID\\{C77FFC11-EE19-4337-92F6-52BF16B0C1A7}"),                                    0,                      TEXT("WebMutableURLRequest") },
{ TEXT("CLSID\\{C77FFC11-EE19-4337-92F6-52BF16B0C1A7}\\InprocServer32"),                    0,                      (LPCTSTR)-1 },
{ TEXT("CLSID\\{C77FFC11-EE19-4337-92F6-52BF16B0C1A7}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") },
{ TEXT("CLSID\\{C77FFC11-EE19-4337-92F6-52BF16B0C1A7}\\ProgID"),                            0,                      TEXT("WebKit.WebMutableURLRequest.3") },
{ TEXT("CLSID\\{C77FFC11-EE19-4337-92F6-52BF16B0C1A7}\\VersionIndependentProgID"),          0,                      TEXT("WebKit.WebMutableURLRequest") },
{ TEXT("WebKit.WebMutableURLRequest.3"),                                                    0,                      TEXT("WebMutableURLRequest") },
{ TEXT("WebKit.WebMutableURLRequest.3\\CLSID"),                                             0,                      TEXT("{C77FFC11-EE19-4337-92F6-52BF16B0C1A7}") }

};

STDAPI DllUnregisterServer(void)
{
    HRESULT hr = S_OK;

    int nEntries = sizeof(gRegTable)/sizeof(*gRegTable);
    for (int i = nEntries - 1; i >= 0; i--) {
        LPCTSTR pszKeyName = gRegTable[i][0];
        if (RegDeleteKey(HKEY_CLASSES_ROOT, pszKeyName))
            hr = S_FALSE;
    }
    return hr;
}

STDAPI DllRegisterServer(void)
{
    HRESULT hr = S_OK;

    // look up server's file name
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(gInstance, szFileName, MAX_PATH);

    // register entries from table
    int nEntries = sizeof(gRegTable)/sizeof(*gRegTable);
    for (int i = 0; SUCCEEDED(hr) && i < nEntries; i++) {
        LPCTSTR pszKeyName   = gRegTable[i][0];
        LPCTSTR pszValueName = gRegTable[i][1];
        LPCTSTR pszValue     = gRegTable[i][2];

        // map rogue value to module file name
        if (pszValue == (LPCTSTR)-1)
            pszValue = szFileName;

        // create the key
        HKEY hkey;
        LONG err = RegCreateKey(HKEY_CLASSES_ROOT, pszKeyName, &hkey);
        if (err == ERROR_SUCCESS) {
            // set the value
            err = RegSetValueEx(hkey, pszValueName, 0, REG_SZ, (const BYTE*)pszValue, (DWORD) sizeof(pszValue[0])*(_tcslen(pszValue) + 1));
            RegCloseKey(hkey);
        }
        if (err != ERROR_SUCCESS || FAILED(hr)) {
            // if cannot add key or value, back out and fail
            DllUnregisterServer(); 
            hr = SELFREG_E_CLASS;
        }
    }

    return hr;
}

//FIXME: We should consider moving this to a new file for cross-project functionality
DeprecatedByteArray loadResourceIntoArray(const char* name)
{
    int idr;
    // temporary hack to get resource id
    if (!strcmp(name, "textAreaResizeCorner"))
        idr = IDR_RESIZE_CORNER;
    else if (!strcmp(name, "missingImage"))
        idr = IDR_MISSING_IMAGE;
    else
        return DeprecatedByteArray();

    HRSRC resInfo = FindResource(gInstance, MAKEINTRESOURCE(idr), L"PNG");
    if (!resInfo)
        return DeprecatedByteArray();
    HANDLE res = LoadResource(gInstance, resInfo);
    if (!res)
        return DeprecatedByteArray();
    void* resource = LockResource(res);
    if (!resource)
        return DeprecatedByteArray();
    int size = SizeofResource(gInstance, resInfo);

    DeprecatedByteArray arr(size);
    memcpy(arr.data(), resource, size);
    return arr;
}
