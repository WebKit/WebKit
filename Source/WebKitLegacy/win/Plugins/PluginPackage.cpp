/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2009 Holger Hans Peter Freyther
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

#include "PluginPackage.h"

#include "PluginDatabase.h"
#include "PluginDebug.h"
#include "PluginView.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <WebCore/IdentifierRep.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NP_jsobject.h>
#include <WebCore/Timer.h>
#include <WebCore/c_utility.h>
#include <WebCore/npruntime_impl.h>
#include <WebCore/runtime_root.h>
#include <string.h>
#include <wtf/text/CString.h>

namespace WebCore {

PluginPackage::~PluginPackage()
{
    // This destructor gets called during refresh() if PluginDatabase's
    // PluginSet hash is already populated, as it removes items from
    // the hash table. Calling the destructor on a loaded plug-in of
    // course would cause a crash, so we check to call unload before we
    // ASSERT.
    // FIXME: There is probably a better way to fix this.
    if (!m_loadCount)
        unloadWithoutShutdown();
    else
        unload();

    ASSERT(!m_isLoaded);
}

void PluginPackage::freeLibrarySoon()
{
    ASSERT(!m_freeLibraryTimer.isActive());
    ASSERT(m_module);
    ASSERT(!m_loadCount);

    m_freeLibraryTimer.startOneShot(0_s);
}

void PluginPackage::freeLibraryTimerFired()
{
    ASSERT(m_module);
    // Do nothing if the module got loaded again meanwhile
    if (!m_loadCount) {
        ::FreeLibrary(m_module);
        m_module = 0;
    }
}


int PluginPackage::compare(const PluginPackage& compareTo) const
{
    // Sort plug-ins that allow multiple instances first.
    bool AallowsMultipleInstances = !quirks().contains(PluginQuirkDontAllowMultipleInstances);
    bool BallowsMultipleInstances = !compareTo.quirks().contains(PluginQuirkDontAllowMultipleInstances);
    if (AallowsMultipleInstances != BallowsMultipleInstances)
        return AallowsMultipleInstances ? -1 : 1;

    // Sort plug-ins in a preferred path first.
    bool AisInPreferredDirectory = PluginDatabase::isPreferredPluginDirectory(parentDirectory());
    bool BisInPreferredDirectory = PluginDatabase::isPreferredPluginDirectory(compareTo.parentDirectory());
    if (AisInPreferredDirectory != BisInPreferredDirectory)
        return AisInPreferredDirectory ? -1 : 1;

    int diff = strcmp(name().utf8().data(), compareTo.name().utf8().data());
    if (diff)
        return diff;

    diff = compareFileVersion(compareTo.version());
    if (diff)
        return diff;

    return strcmp(parentDirectory().utf8().data(), compareTo.parentDirectory().utf8().data());
}

PluginPackage::PluginPackage(const String& path, const time_t& lastModified)
    : m_isEnabled(true)
    , m_isLoaded(false)
    , m_loadCount(0)
    , m_path(path)
    , m_moduleVersion(0)
    , m_module(0)
    , m_lastModified(lastModified)
    , m_freeLibraryTimer(*this, &PluginPackage::freeLibraryTimerFired)
#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
    , m_infoIsFromCache(true)
#endif
{
    m_fileName = FileSystem::pathGetFileName(m_path);
    m_parentDirectory = m_path.left(m_path.length() - m_fileName.length() - 1);
}

void PluginPackage::unload()
{
    if (!m_isLoaded)
        return;

    if (--m_loadCount > 0)
        return;

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_NPP_Shutdown();
#endif

    unloadWithoutShutdown();
}

void PluginPackage::unloadWithoutShutdown()
{
    if (!m_isLoaded)
        return;

    ASSERT(!m_loadCount);
    ASSERT(m_module);

    // <rdar://5530519>: Crash when closing tab with pdf file (Reader 7 only)
    // If the plugin has subclassed its parent window, as with Reader 7, we may have
    // gotten here by way of the plugin's internal window proc forwarding a message to our
    // original window proc. If we free the plugin library from here, we will jump back
    // to code we just freed when we return, so delay calling FreeLibrary at least until
    // the next message loop
    freeLibrarySoon();

    m_isLoaded = false;
}

void PluginPackage::setEnabled(bool enabled)
{
    m_isEnabled = enabled;
}

RefPtr<PluginPackage> PluginPackage::createPackage(const String& path, const time_t& lastModified)
{
    RefPtr<PluginPackage> package = adoptRef(new PluginPackage(path, lastModified));

    if (!package->fetchInfo())
        return nullptr;

    return package;
}

#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
Ref<PluginPackage> PluginPackage::createPackageFromCache(const String& path, const time_t& lastModified, const String& name, const String& description, const String& mimeDescription)
{
    Ref<PluginPackage> package = adoptRef(*new PluginPackage(path, lastModified));
    package->m_name = name;
    package->m_description = description;
    package->determineModuleVersionFromDescription();
    package->setMIMEDescription(mimeDescription);
    package->m_infoIsFromCache = true;
    return package;
}
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
static void getListFromVariantArgs(JSC::ExecState* exec, const NPVariant* args, unsigned argCount, JSC::Bindings::RootObject* rootObject, JSC::MarkedArgumentBuffer& aList)
{
    for (unsigned i = 0; i < argCount; ++i)
        aList.append(JSC::Bindings::convertNPVariantToValue(exec, &args[i], rootObject));
}

static bool NPN_Evaluate(NPP instance, NPObject* o, NPString* s, NPVariant* variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o);

        JSC::Bindings::RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;

        // There is a crash in Flash when evaluating a script that destroys the
        // PluginView, so we destroy it asynchronously.
        PluginView::keepAlive(instance);

        auto globalObject = rootObject->globalObject();
        auto& vm = globalObject->vm();
        JSC::JSLockHolder lock(vm);
        auto scope = DECLARE_CATCH_SCOPE(vm);

        JSC::ExecState* exec = globalObject->globalExec();
        String scriptString = JSC::Bindings::convertNPStringToUTF16(s);

        JSC::JSValue returnValue = JSC::evaluate(exec, JSC::makeSource(scriptString, { }), JSC::JSValue());

        JSC::Bindings::convertValueToNPVariant(exec, returnValue, variant);
        scope.clearException();
        return true;
    }

    VOID_TO_NPVARIANT(*variant);
    return false;
}

static bool NPN_Invoke(NPP npp, NPObject* o, NPIdentifier methodName, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o);

        IdentifierRep* i = static_cast<IdentifierRep*>(methodName);
        if (!i->isString())
            return false;

        // Special case the "eval" method.
        if (methodName == _NPN_GetStringIdentifier("eval")) {
            if (argCount != 1)
                return false;
            if (args[0].type != NPVariantType_String)
                return false;
            return WebCore::NPN_Evaluate(npp, o, const_cast<NPString*>(&args[0].value.stringValue), result);
        }

        // Look up the function object.
        JSC::Bindings::RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;

        auto globalObject = rootObject->globalObject();
        auto& vm = globalObject->vm();
        JSC::JSLockHolder lock(vm);
        auto scope = DECLARE_CATCH_SCOPE(vm);

        JSC::ExecState* exec = globalObject->globalExec();
        JSC::JSValue function = obj->imp->get(exec, JSC::Bindings::identifierFromNPIdentifier(exec, i->string()));
        JSC::CallData callData;
        JSC::CallType callType = getCallData(vm, function, callData);
        if (callType == JSC::CallType::None)
            return false;

        // Call the function object.
        JSC::MarkedArgumentBuffer argList;
        getListFromVariantArgs(exec, args, argCount, rootObject, argList);
        JSC::JSValue resultV = JSC::call(exec, function, callType, callData, obj->imp, argList);

        // Convert and return the result of the function call.
        JSC::Bindings::convertValueToNPVariant(exec, resultV, result);
        scope.clearException();
        return true;
    }

    if (o->_class->invoke)
        return o->_class->invoke(o, methodName, args, argCount, result);

    VOID_TO_NPVARIANT(*result);
    return true;
}

void PluginPackage::initializeBrowserFuncs()
{
    memset(&m_browserFuncs, 0, sizeof(m_browserFuncs));
    m_browserFuncs.size = sizeof(m_browserFuncs);
    m_browserFuncs.version = NPVersion();

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
    m_browserFuncs.pluginthreadasynccall = NPN_PluginThreadAsyncCall;

    m_browserFuncs.releasevariantvalue = _NPN_ReleaseVariantValue;
    m_browserFuncs.getstringidentifier = _NPN_GetStringIdentifier;
    m_browserFuncs.getstringidentifiers = _NPN_GetStringIdentifiers;
    m_browserFuncs.getintidentifier = _NPN_GetIntIdentifier;
    m_browserFuncs.identifierisstring = _NPN_IdentifierIsString;
    m_browserFuncs.utf8fromidentifier = _NPN_UTF8FromIdentifier;
    m_browserFuncs.intfromidentifier = _NPN_IntFromIdentifier;
    m_browserFuncs.createobject = _NPN_CreateObject;
    m_browserFuncs.retainobject = _NPN_RetainObject;
    m_browserFuncs.releaseobject = _NPN_ReleaseObject;
    m_browserFuncs.invoke = WebCore::NPN_Invoke;
    m_browserFuncs.invokeDefault = _NPN_InvokeDefault;
    m_browserFuncs.evaluate = WebCore::NPN_Evaluate;
    m_browserFuncs.getproperty = _NPN_GetProperty;
    m_browserFuncs.setproperty = _NPN_SetProperty;
    m_browserFuncs.removeproperty = _NPN_RemoveProperty;
    m_browserFuncs.hasproperty = _NPN_HasProperty;
    m_browserFuncs.hasmethod = _NPN_HasMethod;
    m_browserFuncs.setexception = _NPN_SetException;
    m_browserFuncs.enumerate = _NPN_Enumerate;
    m_browserFuncs.construct = _NPN_Construct;
    m_browserFuncs.getvalueforurl = NPN_GetValueForURL;
    m_browserFuncs.setvalueforurl = NPN_SetValueForURL;
    m_browserFuncs.getauthenticationinfo = NPN_GetAuthenticationInfo;

    m_browserFuncs.popupcontextmenu = NPN_PopUpContextMenu;
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

int PluginPackage::compareFileVersion(const PlatformModuleVersion& compareVersion) const
{
    // return -1, 0, or 1 if plug-in version is less than, equal to, or greater than
    // the passed version

    if (m_moduleVersion.mostSig != compareVersion.mostSig)
        return m_moduleVersion.mostSig > compareVersion.mostSig ? 1 : -1;
    if (m_moduleVersion.leastSig != compareVersion.leastSig)
        return m_moduleVersion.leastSig > compareVersion.leastSig ? 1 : -1;

    return 0;
}

#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
bool PluginPackage::ensurePluginLoaded()
{
    if (!m_infoIsFromCache)
        return m_isLoaded;

    m_quirks = PluginQuirkSet();
    m_name = String();
    m_description = String();
    m_fullMIMEDescription = String();
    m_moduleVersion = 0;

    return fetchInfo();
}
#endif

}
