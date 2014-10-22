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

#include "MIMETypeRegistry.h"
#include "Page.h"
#include "PluginDatabase.h"
#include "PluginDebug.h"
#include "PluginMainThreadScheduler.h"
#include "PluginView.h"
#include "Timer.h"
#include "npruntime_impl.h"
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

    m_freeLibraryTimer.startOneShot(0);
}

void PluginPackage::freeLibraryTimerFired(Timer<PluginPackage>*)
{
    ASSERT(m_module);
    // Do nothing if the module got loaded again meanwhile
    if (!m_loadCount) {
        unloadModule(m_module);
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
    , m_freeLibraryTimer(this, &PluginPackage::freeLibraryTimerFired)
#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
    , m_infoIsFromCache(true)
#endif
{
    m_fileName = pathGetFileName(m_path);
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

PassRefPtr<PluginPackage> PluginPackage::createPackage(const String& path, const time_t& lastModified)
{
    RefPtr<PluginPackage> package = adoptRef(new PluginPackage(path, lastModified));

    if (!package->fetchInfo())
        return 0;

    return package.release();
}

#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
PassRefPtr<PluginPackage> PluginPackage::createPackageFromCache(const String& path, const time_t& lastModified, const String& name, const String& description, const String& mimeDescription)
{
    RefPtr<PluginPackage> package = adoptRef(new PluginPackage(path, lastModified));
    package->m_name = name;
    package->m_description = description;
    package->determineModuleVersionFromDescription();
    package->setMIMEDescription(mimeDescription);
    package->m_infoIsFromCache = true;
    return package.release();
}
#endif

#if defined(XP_UNIX)
void PluginPackage::determineQuirks(const String& mimeType)
{
    if (MIMETypeRegistry::isJavaAppletMIMEType(mimeType)) {
        // Because a single process cannot create multiple VMs, and we cannot reliably unload a
        // Java VM, we cannot unload the Java plugin, or we'll lose reference to our only VM
        m_quirks.add(PluginQuirkDontUnloadPlugin);

        // Setting the window region to an empty region causes bad scrolling repaint problems
        // with the Java plug-in.
        m_quirks.add(PluginQuirkDontClipToZeroRectWhenScrolling);
        return;
    }

    if (mimeType == "application/x-shockwave-flash") {
        static const PlatformModuleVersion flashTenVersion(0x0a000000);

        if (compareFileVersion(flashTenVersion) >= 0) {
            // Flash 10.0 b218 doesn't like having a NULL window handle
            m_quirks.add(PluginQuirkDontSetNullWindowHandleOnDestroy);
        } else {
            // Flash 9 and older requests windowless plugins if we return a mozilla user agent
            m_quirks.add(PluginQuirkWantsMozillaUserAgent);
        }

#if CPU(X86_64)
        // 64-bit Flash freezes if right-click is sent in windowless mode
        m_quirks.add(PluginQuirkIgnoreRightClickInWindowlessMode);
#endif

        m_quirks.add(PluginQuirkRequiresDefaultScreenDepth);
        m_quirks.add(PluginQuirkThrottleInvalidate);
        m_quirks.add(PluginQuirkThrottleWMUserPlusOneMessages);
        m_quirks.add(PluginQuirkFlashURLNotifyBug);
    }
}
#endif

#if !OS(WINDOWS)
void PluginPackage::determineModuleVersionFromDescription()
{
    // It's a bit lame to detect the plugin version by parsing it
    // from the plugin description string, but it doesn't seem that
    // version information is available in any standardized way at
    // the module level, like in Windows

    if (m_description.isEmpty())
        return;

    if (m_description.startsWith("Shockwave Flash") && m_description.length() >= 19) {
        // The flash version as a PlatformModuleVersion differs on Unix from Windows
        // since the revision can be larger than a 8 bits, so we allow it 16 here and
        // push the major/minor up 8 bits. Thus on Unix, Flash's version may be
        // 0x0a000000 instead of 0x000a0000.

        Vector<String> versionParts;
        m_description.substring(16).split(' ', /*allowEmptyEntries =*/ false, versionParts);
        if (versionParts.isEmpty())
            return;

        if (versionParts.size() >= 1) {
            Vector<String> majorMinorParts;
            versionParts[0].split('.', majorMinorParts);
            if (majorMinorParts.size() >= 1) {
                bool converted = false;
                unsigned major = majorMinorParts[0].toUInt(&converted);
                if (converted)
                    m_moduleVersion = (major & 0xff) << 24;
            }
            if (majorMinorParts.size() == 2) {
                bool converted = false;
                unsigned minor = majorMinorParts[1].toUInt(&converted);
                if (converted)
                    m_moduleVersion |= (minor & 0xff) << 16;
            }
        }

        if (versionParts.size() >= 2) {
            String revision = versionParts[1];
            if (revision.length() > 1 && (revision[0] == 'r' || revision[0] == 'b')) {
                revision.remove(0, 1);
                m_moduleVersion |= revision.toInt() & 0xffff;
            }
        }
    }
}
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
static PluginView* pluginViewForInstance(NPP instance)
{
    if (instance && instance->ndata)
        return static_cast<PluginView*>(instance->ndata);
    return PluginView::currentPluginView();
}

static void* NPN_MemAlloc(uint32_t size)
{
    return malloc(size);
}

static void NPN_MemFree(void* ptr)
{
    free(ptr);
}

static uint32_t NPN_MemFlush(uint32_t)
{
    // Do nothing
    return 0;
}

static void NPN_ReloadPlugins(NPBool reloadPages)
{
    Page::refreshPlugins(reloadPages);
}

static NPError NPN_RequestRead(NPStream*, NPByteRange*)
{
    return NPERR_STREAM_NOT_SEEKABLE;
}

static NPError NPN_GetURLNotify(NPP instance, const char* url, const char* target, void* notifyData)
{
    return pluginViewForInstance(instance)->getURLNotify(url, target, notifyData);
}

static NPError NPN_GetURL(NPP instance, const char* url, const char* target)
{
    return pluginViewForInstance(instance)->getURL(url, target);
}

static NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData)
{
    return pluginViewForInstance(instance)->postURLNotify(url, target, len, buf, file, notifyData);
}

static NPError NPN_PostURL(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file)
{
    return pluginViewForInstance(instance)->postURL(url, target, len, buf, file);
}

static NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
    return pluginViewForInstance(instance)->newStream(type, target, stream);
}

static int32_t NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer)
{
    return pluginViewForInstance(instance)->write(stream, len, buffer);
}

static NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    return pluginViewForInstance(instance)->destroyStream(stream, reason);
}

static const char* NPN_UserAgent(NPP instance)
{
    PluginView* view = pluginViewForInstance(instance);

     if (!view)
         return PluginView::userAgentStatic();
 
    return view->userAgent();
}

static void NPN_Status(NPP instance, const char* message)
{
    pluginViewForInstance(instance)->status(message);
}

static void NPN_InvalidateRect(NPP instance, NPRect* invalidRect)
{
    PluginView* view = pluginViewForInstance(instance);
#if defined(XP_UNIX)
    // NSPluginWrapper, a plugin wrapper binary that allows running 32-bit plugins
    // on 64-bit architectures typically used in X11, will sometimes give us a null NPP here.
    if (!view)
        return;
#endif
    view->invalidateRect(invalidRect);
}

static void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
    pluginViewForInstance(instance)->invalidateRegion(invalidRegion);
}

static void NPN_ForceRedraw(NPP instance)
{
    pluginViewForInstance(instance)->forceRedraw();
}

static NPError NPN_GetValue(NPP instance, NPNVariable variable, void* value)
{
    PluginView* view = pluginViewForInstance(instance);

     if (!view)
         return PluginView::getValueStatic(variable, value);

    return pluginViewForInstance(instance)->getValue(variable, value);
}

static NPError NPN_SetValue(NPP instance, NPPVariable variable, void* value)
{
   return pluginViewForInstance(instance)->setValue(variable, value);
}

static void* NPN_GetJavaEnv()
{
    // Unsupported
    return 0;
}

static void* NPN_GetJavaPeer(NPP)
{
    // Unsupported
    return 0;
}

static void NPN_PushPopupsEnabledState(NPP instance, NPBool enabled)
{
    pluginViewForInstance(instance)->pushPopupsEnabledState(enabled);
}

static void NPN_PopPopupsEnabledState(NPP instance)
{
    pluginViewForInstance(instance)->popPopupsEnabledState();
}

extern "C" typedef void PluginThreadAsyncCallFunction(void*);
static void NPN_PluginThreadAsyncCall(NPP instance, PluginThreadAsyncCallFunction func, void* userData)
{
    // Callback function type only differs from MainThreadFunction by being extern "C", which doesn't affect calling convention on any compilers we use.
    PluginMainThreadScheduler::scheduler().scheduleCall(instance, reinterpret_cast<PluginMainThreadScheduler::MainThreadFunction*>(func), userData);
}

static NPError NPN_GetValueForURL(NPP instance, NPNURLVariable variable, const char* url, char** value, uint32_t* len)
{
    return pluginViewForInstance(instance)->getValueForURL(variable, url, value, len);
}

static NPError NPN_SetValueForURL(NPP instance, NPNURLVariable variable, const char* url, const char* value, uint32_t len)
{
    return pluginViewForInstance(instance)->setValueForURL(variable, url, value, len);
}

static NPError NPN_GetAuthenticationInfo(NPP instance, const char* protocol, const char* host, int32_t port, const char* scheme, const char* realm, char** username, uint32_t* ulen, char** password, uint32_t* plen)
{
    return pluginViewForInstance(instance)->getAuthenticationInfo(protocol, host, port, scheme, realm, username, ulen, password, plen);
}

static NPError NPN_PopUpContextMenu(NPP instance, NPMenu* menu)
{
    UNUSED_PARAM(instance);
    UNUSED_PARAM(menu);
    return NPERR_NO_ERROR;
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
    m_browserFuncs.invoke = _NPN_Invoke;
    m_browserFuncs.invokeDefault = _NPN_InvokeDefault;
    m_browserFuncs.evaluate = _NPN_Evaluate;
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

#if ENABLE(PLUGIN_PACKAGE_SIMPLE_HASH)
unsigned PluginPackage::hash() const
{
    struct HashCodes {
        unsigned hash;
        time_t modifiedDate;
    } hashCodes;

    hashCodes.hash = m_path.impl()->hash();
    hashCodes.modifiedDate = m_lastModified;

    return StringHasher::hashMemory<sizeof(hashCodes)>(&hashCodes);
}

bool PluginPackage::equal(const PluginPackage& a, const PluginPackage& b)
{
    return a.m_description == b.m_description;
}
#endif

int PluginPackage::compareFileVersion(const PlatformModuleVersion& compareVersion) const
{
    // return -1, 0, or 1 if plug-in version is less than, equal to, or greater than
    // the passed version

#if OS(WINDOWS)
    if (m_moduleVersion.mostSig != compareVersion.mostSig)
        return m_moduleVersion.mostSig > compareVersion.mostSig ? 1 : -1;
    if (m_moduleVersion.leastSig != compareVersion.leastSig)
        return m_moduleVersion.leastSig > compareVersion.leastSig ? 1 : -1;
#else    
    if (m_moduleVersion != compareVersion)
        return m_moduleVersion > compareVersion ? 1 : -1;
#endif

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
