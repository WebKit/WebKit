/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.  All rights reserved.
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
#include "PluginDatabase.h"
#include "PluginDebug.h"
#include "Timer.h"
#include "npruntime_impl.h"
#include <string.h>
#include <wtf/OwnArrayPtr.h>

namespace WebCore {

PluginPackage::~PluginPackage()
{
    // This destructor gets called during refresh() if PluginDatabase's
    // PluginSet hash is already populated, as it removes items from
    // the hash table. Calling the destructor on a loaded plug-in of
    // course would cause a crash, so we check to call unload before we
    // ASSERT.
    // FIXME: There is probably a better way to fix this.
    if (m_loadCount == 0)
        unloadWithoutShutdown();
    else
        unload();

    ASSERT(!m_isLoaded);
}

void PluginPackage::freeLibrarySoon()
{
    ASSERT(!m_freeLibraryTimer.isActive());
    ASSERT(m_module);
    ASSERT(m_loadCount == 0);

    m_freeLibraryTimer.startOneShot(0);
}

void PluginPackage::freeLibraryTimerFired(Timer<PluginPackage>*)
{
    ASSERT(m_module);
    ASSERT(m_loadCount == 0);

    unloadModule(m_module);
    m_module = 0;
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
    : m_isLoaded(false)
    , m_loadCount(0)
    , m_path(path)
    , m_moduleVersion(0)
    , m_module(0)
    , m_lastModified(lastModified)
    , m_freeLibraryTimer(this, &PluginPackage::freeLibraryTimerFired)
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

    m_NPP_Shutdown();

    unloadWithoutShutdown();
}

void PluginPackage::unloadWithoutShutdown()
{
    if (!m_isLoaded)
        return;

    ASSERT(m_loadCount == 0);
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

PassRefPtr<PluginPackage> PluginPackage::createPackage(const String& path, const time_t& lastModified)
{
    RefPtr<PluginPackage> package = adoptRef(new PluginPackage(path, lastModified));

    if (!package->fetchInfo())
        return 0;

    return package.release();
}

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
#if PLATFORM(QT)
            m_quirks.add(PluginQuirkRequiresGtkToolKit);
#endif
        } else {
            // Flash 9 and older requests windowless plugins if we return a mozilla user agent
            m_quirks.add(PluginQuirkWantsMozillaUserAgent);
        }

        m_quirks.add(PluginQuirkThrottleInvalidate);
        m_quirks.add(PluginQuirkThrottleWMUserPlusOneMessages);
        m_quirks.add(PluginQuirkFlashURLNotifyBug);
    }
}
#endif

#if !PLATFORM(WIN_OS)
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

}
