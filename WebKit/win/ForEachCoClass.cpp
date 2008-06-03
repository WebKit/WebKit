/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "WebKitDLL.h"
#include "ForEachCoClass.h"

#include <JavaScriptCore/Assertions.h>
#include <WebCore/COMPtr.h>

#include <tchar.h>

#if __PRODUCTION__
#define VERSION_INDEPENDENT_PROGID(className) VERSION_INDEPENDENT_PRODUCTION_PROGID(className)
#else
#define VERSION_INDEPENDENT_PROGID(className) VERSION_INDEPENDENT_OPENSOURCE_PROGID(className)
#endif
#define CURRENT_VERSIONED_PROGID(className) VERSIONED_PROGID(VERSION_INDEPENDENT_PROGID(className), CURRENT_PROGID_VERSION)
#define VERSIONED_303_PROGID(className) VERSIONED_PROGID(VERSION_INDEPENDENT_PROGID(className), 3)

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
{ VERSIONED_303_PROGID(cls),                                                                0,                      TEXT(#cls) },
// end of macro

static const int gSlotsPerEntry = 11;
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
    size_t strLength = _tcslen(str);    LPTSTR guidSubStr = str;
    while (strLength) {
        guidSubStr = _tcsstr(guidSubStr, guidPattern);
        if (!guidSubStr)
            break;
        _tcsncpy(guidSubStr, uuidString, patternLength);
        guidSubStr += patternLength;
        strLength -= (guidSubStr - str);
    }
}

// deprecated - remove once a registry-free version of Safari has shipped (first major version after 3.1.1)
static void registerWebKitNightly()
{
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
    HRESULT hr = S_OK;
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
}

// deprecated - do not use - remove once a registry-free version of Safari has shipped (first major version after 3.1.1)
void setUseOpenSourceWebKit(bool b)
{
    if (b)
        registerWebKitNightly();
}

// deprecated - do not use - remove once a registry-free version of Safari has shipped (first major version after 3.1.1)
LPCOLESTR progIDForClass(WebKitClass cls)
{
    ASSERT(cls < WebKitClassSentinel);
    return s_progIDs[cls];
}
