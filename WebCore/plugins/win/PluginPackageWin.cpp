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
#include "PluginPackageWin.h"

#include "DeprecatedString.h"
#include "npruntime_impl.h"
#include "PluginDebug.h"

namespace WebCore {

PluginPackageWin::~PluginPackageWin()
{
    ASSERT(!m_isLoaded);
}

static String getVersionInfo(const LPVOID versionInfoData, const String& info)
{
    LPVOID buffer;
    UINT bufferLength;
    String subInfo = "\\StringfileInfo\\040904E4\\" + info;

    bool retval = VerQueryValueW(versionInfoData, const_cast<UChar*>(subInfo.charactersWithNullTermination()), 
                                 &buffer, &bufferLength);
    if (!retval || bufferLength == 0)
        return String();

    // Subtract 1 from the length; we don't want the trailing 0
    return String(reinterpret_cast<UChar*>(buffer), bufferLength - 1);
}

static Vector<String> splitString(const String& str, char delimiter, int padTo)
{
    int pos = 0;
    int newPos;
    Vector<String> result;
    DeprecatedString ds = str.deprecatedString();
    String s;
    do {

        newPos = ds.find(delimiter, pos);

        if (newPos == -1)
            s = ds.mid(pos);
        else
            s = ds.mid(pos, newPos - pos);
        
        if (!s.isEmpty())
            result.append(s);

        pos = newPos + 1;
    } while (newPos != -1);

    while (padTo != -1 && static_cast<int>(result.size()) < padTo)
        result.append("");

    return result;
}

PluginPackageWin::PluginPackageWin(const String& path, const FILETIME& lastModified)
    : m_path(path)
    , m_module(0)
    , m_lastModified(lastModified)
    , m_isLoaded(false)
    , m_loadCount(0)
{
    int pos = m_path.deprecatedString().findRev('\\');

    m_fileName = m_path.right(m_path.length() - pos - 1);

}

bool PluginPackageWin::fetchInfo()
{
    DWORD versionInfoSize, zeroHandle;
    versionInfoSize = GetFileVersionInfoSizeW(m_path.charactersWithNullTermination(), &zeroHandle); 

    if (versionInfoSize == 0)
        return false;

    LPVOID versionInfoData = fastMalloc(versionInfoSize);

    if (!GetFileVersionInfoW(m_path.charactersWithNullTermination(), 0, versionInfoSize, versionInfoData)) {
        fastFree(versionInfoData);
        return false;
    }

    m_name = getVersionInfo(versionInfoData, "ProductName");
    m_description = getVersionInfo(versionInfoData, "FileDescription");

    if (m_name.isNull() || m_description.isNull()) {
        fastFree(versionInfoData);
        return false;
    }

    Vector<String> mimeTypes = splitString(getVersionInfo(versionInfoData, "MIMEType"), '|', -1);
    Vector<String> fileExtents = splitString(getVersionInfo(versionInfoData, "FileExtents"), '|', mimeTypes.size());
    Vector<String> descriptions = splitString(getVersionInfo(versionInfoData, "FileOpenName"), '|', mimeTypes.size());

    fastFree(versionInfoData);

    for (unsigned i = 0; i < mimeTypes.size(); i++) {
        // Get rid of the extension list in the description string
        String description = descriptions[i];    
        int pos = description.find("(*");
        if (pos != -1) {
            // There might be a space that we need to get rid of
            if (pos > 1 && description[pos - 1] == ' ')
                pos--;

            description = description.left(pos);
        }

        m_mimeToExtensions.add(mimeTypes[i], splitString(fileExtents[i], ',', -1));
        m_mimeToDescriptions.add(mimeTypes[i], description);
    }

    return true;
}

bool PluginPackageWin::load()
{
    if (m_isLoaded) {
        m_loadCount++;
        return true;
    }

    WCHAR currentPath[MAX_PATH];

    if (!::GetCurrentDirectoryW(MAX_PATH, currentPath))
        return false;

    String path = m_path.substring(0, m_path.reverseFind('\\'));

    if (!::SetCurrentDirectoryW(path.charactersWithNullTermination()))
        return false;

    // Load the library
    m_module = ::LoadLibraryW(m_path.charactersWithNullTermination());

    if (!::SetCurrentDirectoryW(currentPath)) {
        if (m_module)
            ::FreeLibrary(m_module);
        return false;
    }

    if (!m_module)
        return false;

    m_isLoaded = true;

    NP_GetEntryPointsFuncPtr NP_GetEntryPoints = 0;
    NP_InitializeFuncPtr NP_Initialize = 0;
    NPError npErr;

    NP_Initialize = (NP_InitializeFuncPtr)GetProcAddress(m_module, "NP_Initialize");
    NP_GetEntryPoints = (NP_GetEntryPointsFuncPtr)GetProcAddress(m_module, "NP_GetEntryPoints");
    m_NPP_Shutdown = (NPP_ShutdownProcPtr)GetProcAddress(m_module, "NP_Shutdown");

    if (!NP_Initialize || !NP_GetEntryPoints || !m_NPP_Shutdown)
        goto abort;
  
    memset(&m_pluginFuncs, 0, sizeof(m_pluginFuncs));
    m_pluginFuncs.size = sizeof(m_pluginFuncs);

    npErr = NP_GetEntryPoints(&m_pluginFuncs);
    LOG_NPERROR(npErr);
    if (npErr != NPERR_NO_ERROR)
        goto abort;

    m_browserFuncs.size = sizeof (m_browserFuncs);
    m_browserFuncs.version = NP_VERSION_MINOR;
    m_browserFuncs.geturl = NPN_GetURL;
    m_browserFuncs.posturl = NPN_PostURL;
    m_browserFuncs.requestread = NPN_RequestRead;
    m_browserFuncs.newstream = NPN_NewStream;
    m_browserFuncs.write = NPN_Write;
    m_browserFuncs.destroystream = NPN_DestroyStream;
    m_browserFuncs.status = NPN_Status;
    m_browserFuncs.uagent = NPN_UserAgent;
    m_browserFuncs.memalloc = NPN_MemAlloc;
    m_browserFuncs.memfree = NPN_MemFree;
    m_browserFuncs.memflush = NPN_MemFlush;
    m_browserFuncs.reloadplugins = NPN_ReloadPlugins;
    m_browserFuncs.geturlnotify = NPN_GetURLNotify;
    m_browserFuncs.posturlnotify = NPN_PostURLNotify;
    m_browserFuncs.getvalue = NPN_GetValue;
    m_browserFuncs.setvalue = NPN_SetValue;
    m_browserFuncs.invalidaterect = NPN_InvalidateRect;
    m_browserFuncs.invalidateregion = NPN_InvalidateRegion;
    m_browserFuncs.forceredraw = NPN_ForceRedraw;
    m_browserFuncs.getJavaEnv = NPN_GetJavaEnv;
    m_browserFuncs.getJavaPeer = NPN_GetJavaPeer;
    m_browserFuncs.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
    m_browserFuncs.poppopupsenabledstate = NPN_PopPopupsEnabledState;

    m_browserFuncs.releasevariantvalue = _NPN_ReleaseVariantValue;
    m_browserFuncs.getstringidentifier = _NPN_GetStringIdentifier;
    m_browserFuncs.getstringidentifiers = _NPN_GetStringIdentifiers;
    m_browserFuncs.getintidentifier = _NPN_GetIntIdentifier;
    m_browserFuncs.identifierisstring = _NPN_IdentifierIsString;
    m_browserFuncs.utf8fromidentifier = _NPN_UTF8FromIdentifier;
    m_browserFuncs.createobject = _NPN_CreateObject;
    m_browserFuncs.retainobject = _NPN_RetainObject;
    m_browserFuncs.releaseobject = _NPN_ReleaseObject;
    m_browserFuncs.invoke = _NPN_Invoke;
    m_browserFuncs.invokeDefault = _NPN_InvokeDefault;
    m_browserFuncs.evaluate = _NPN_Evaluate;
    m_browserFuncs.getproperty = _NPN_GetProperty;
    m_browserFuncs.setproperty = _NPN_SetProperty;
    m_browserFuncs.removeproperty = _NPN_RemoveProperty;
    m_browserFuncs.hasproperty = _NPN_HasMethod;
    m_browserFuncs.hasmethod = _NPN_HasProperty;
    m_browserFuncs.setexception = _NPN_SetException;
    m_browserFuncs.enumerate = _NPN_Enumerate;

    npErr = NP_Initialize(&m_browserFuncs);
    LOG_NPERROR(npErr);

    if (npErr != NPERR_NO_ERROR)
        goto abort;

    m_loadCount++;
    return true;
abort:
    unloadWithoutShutdown();
    return false;
}

void PluginPackageWin::unload()
{
    if (!m_isLoaded)
        return;

    if (--m_loadCount > 0)
        return;

    m_NPP_Shutdown();

    unloadWithoutShutdown();
}

void PluginPackageWin::unloadWithoutShutdown()
{
    if (!m_isLoaded)
        return;

    ASSERT(m_loadCount == 0);
    ASSERT(m_module);

    FreeLibrary(m_module);

    m_isLoaded = false;
}

PluginPackageWin* PluginPackageWin::createPackage(const String& path, const FILETIME& lastModified)
{
    PluginPackageWin* package = new PluginPackageWin(path, lastModified);

    if (!package->fetchInfo()) {
        delete package;
        return 0;
    }
    
    return package;
}

unsigned PluginPackageWin::hash() const
{ 
    unsigned hashCodes[3] = {
        m_description.impl()->hash(),
        m_lastModified.dwLowDateTime,
        m_lastModified.dwHighDateTime
    };

    return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), 3 * sizeof(unsigned) / sizeof(UChar));
}

bool PluginPackageWin::equal(const PluginPackageWin& a, const PluginPackageWin& b)
{
    return a.m_description == b.m_description && (CompareFileTime(&a.m_lastModified, &b.m_lastModified) == 0);
}

}
