/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
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
#include "PluginPackage.h"

#include "CString.h"
#include "MIMETypeRegistry.h"
#include "npruntime_impl.h"
#include "PluginDatabase.h"
#include "PluginDebug.h"

namespace WebCore {

bool PluginPackage::fetchInfo()
{
    if (!load())
        return false;

    NPP_GetValueProcPtr gv = (NPP_GetValueProcPtr)m_module->resolve("NP_GetValue");
    typedef char *(*NPP_GetMIMEDescriptionProcPtr)();
    NPP_GetMIMEDescriptionProcPtr gm =
        (NPP_GetMIMEDescriptionProcPtr)m_module->resolve("NP_GetMIMEDescription");
    if (!gm || !gv) {
        return false;
    }
    char *buf = 0;
    NPError err = gv(0, NPPVpluginNameString, (void *)&buf);
    if (err != NPERR_NO_ERROR) {
        return false;
    }
    m_name = buf;
    err = gv(0, NPPVpluginDescriptionString, (void *)&buf);
    if (err != NPERR_NO_ERROR) {
        return false;
    }
    m_description = buf;
    determineModuleVersionFromDescription();

    String s = gm();
    Vector<String> types;
    s.split(UChar(';'), false, types);
    for (int i = 0; i < types.size(); ++i) {
        Vector<String> mime;
        types[i].split(UChar(':'), true, mime); 
        if (mime.size() > 0) {
            Vector<String> exts;
            if (mime.size() > 1)
                mime[1].split(UChar(','), false, exts);
            determineQuirks(mime[0]);
            m_mimeToExtensions.add(mime[0], exts);
            if (mime.size() > 2)
                m_mimeToDescriptions.add(mime[0], mime[2]);
        }
    }

    return true;
}

bool PluginPackage::load()
{
    if (m_isLoaded) {
        m_loadCount++;
        return true;
    }

    m_module = new QLibrary((QString)m_path);
    m_module->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!m_module->load()) {
        LOG(Plugin, "%s not loaded (%s)", m_path.utf8().data(),
                m_module->errorString().toLatin1().constData());
        return false;
    }

    m_isLoaded = true;

    NP_InitializeFuncPtr NP_Initialize;
    NPError npErr;

    NP_Initialize = (NP_InitializeFuncPtr)m_module->resolve("NP_Initialize");
    m_NPP_Shutdown = (NPP_ShutdownProcPtr)m_module->resolve("NP_Shutdown");

    if (!NP_Initialize || !m_NPP_Shutdown)
        goto abort;

    memset(&m_pluginFuncs, 0, sizeof(m_pluginFuncs));
    m_pluginFuncs.size = sizeof(m_pluginFuncs);

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
    m_browserFuncs.construct = _NPN_Construct;

#if defined(XP_UNIX)
    npErr = NP_Initialize(&m_browserFuncs, &m_pluginFuncs);
#else
    npErr = NP_Initialize(&m_browserFuncs);
#endif
    if (npErr != NPERR_NO_ERROR)
        goto abort;

    m_loadCount++;
    return true;

abort:
    unloadWithoutShutdown();
    return false;
}

unsigned PluginPackage::hash() const
{ 
    unsigned hashCodes[2] = {
        m_path.impl()->hash(),
        m_lastModified
    };

    return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), 2 * sizeof(unsigned) / sizeof(UChar));
}

bool PluginPackage::equal(const PluginPackage& a, const PluginPackage& b)
{
    return a.m_description == b.m_description;
}

int PluginPackage::compareFileVersion(const PlatformModuleVersion& compareVersion) const
{
    // return -1, 0, or 1 if plug-in version is less than, equal to, or greater than
    // the passed version
    if (m_moduleVersion != compareVersion)
        return m_moduleVersion > compareVersion ? 1 : -1;
    return 0;
}

}
