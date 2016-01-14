/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SyscallPolicy.h"

#if ENABLE(SECCOMP_FILTERS)

#include "PluginSearchPath.h"
#include "WebProcessCreationParameters.h"
#include "XDGBaseDirectory.h"
#include <libgen.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace WebKit {

static String removeTrailingSlash(const String& path)
{
    if (path.endsWith('/'))
        return path.left(path.length() - 1);

    return path;
}

bool SyscallPolicy::hasPermissionForPath(const char* path, Permission permission) const
{
    // The root directory policy needs to be set because it is the
    // ultimate fallback when rewinding directories.
    ASSERT(m_directoryPermission.contains("/"));

    if (permission == NotAllowed)
        return false;

    char* basePath = strdup(path);
    char* canonicalPath = canonicalize_file_name(basePath);

    while (canonicalPath) {
        struct stat pathStat;
        if (stat(canonicalPath, &pathStat) == -1) {
            free(basePath);
            free(canonicalPath);
            return false;
        }

        if (S_ISDIR(pathStat.st_mode))
            break;

        PermissionMap::const_iterator policy = m_filePermission.find(String(canonicalPath));
        if (policy != m_filePermission.end()) {
            free(basePath);
            free(canonicalPath);
            return (permission & policy->value) == permission;
        }

        // If not a directory neither a file with a policy defined,
        // we set canonicalPath to zero to force a rewind to the parent
        // directory.
        free(canonicalPath);
        canonicalPath = 0;
    }

    while (!canonicalPath) {
        char* currentBaseDirectory = dirname(basePath);
        canonicalPath = canonicalize_file_name(currentBaseDirectory);
    }

    PermissionMap::const_iterator policy = m_directoryPermission.find(String(canonicalPath));
    while (policy == m_directoryPermission.end()) {
        char* currentBaseDirectory = dirname(canonicalPath);
        policy = m_directoryPermission.find(String(currentBaseDirectory));
    }

    free(basePath);
    free(canonicalPath);

    if ((permission & policy->value) == permission)
        return true;

    // Don't warn if the file doesn't exist at all.
    if (!access(path, F_OK) || errno != ENOENT)
        fprintf(stderr, "Blocked impermissible %s access to %s\n", SyscallPolicy::permissionToString(permission), path);
    return false;
}

static String canonicalizeFileName(const String& path)
{
    char* canonicalizedPath = canonicalize_file_name(path.utf8().data());
    if (canonicalizedPath) {
        String result = String::fromUTF8(canonicalizedPath);
        free(canonicalizedPath);
        return result;
    }
    return path;
}

void SyscallPolicy::addFilePermission(const String& path, Permission permission)
{
    ASSERT(!path.isEmpty() && path.startsWith('/')  && !path.endsWith('/') && !path.contains("//"));

    m_filePermission.set(canonicalizeFileName(path), permission);
}

void SyscallPolicy::addDirectoryPermission(const String& path, Permission permission)
{
    ASSERT(path.startsWith('/') && !path.contains("//") && (path.length() == 1 || !path.endsWith('/')));

    m_directoryPermission.set(canonicalizeFileName(path), permission);
}

void SyscallPolicy::addDefaultWebProcessPolicy(const WebProcessCreationParameters& parameters)
{
    // Directories settings coming from the UIProcess.
    if (!parameters.applicationCacheDirectory.isEmpty())
        addDirectoryPermission(removeTrailingSlash(parameters.applicationCacheDirectory), ReadAndWrite);
    if (!parameters.webSQLDatabaseDirectory.isEmpty())
        addDirectoryPermission(removeTrailingSlash(parameters.webSQLDatabaseDirectory), ReadAndWrite);
    if (!parameters.cookieStorageDirectory.isEmpty())
        addDirectoryPermission(removeTrailingSlash(parameters.cookieStorageDirectory), ReadAndWrite);

    // The root policy will block access to any directory or
    // file unless white listed bellow or by platform.
    addDirectoryPermission(ASCIILiteral("/"), NotAllowed);

    // System library directories
    addDirectoryPermission(ASCIILiteral("/lib"), Read);
    addDirectoryPermission(ASCIILiteral("/lib32"), Read);
    addDirectoryPermission(ASCIILiteral("/lib64"), Read);
    addDirectoryPermission(ASCIILiteral("/usr/lib"), Read);
    addDirectoryPermission(ASCIILiteral("/usr/lib32"), Read);
    addDirectoryPermission(ASCIILiteral("/usr/lib64"), Read);
    addDirectoryPermission(ASCIILiteral("/usr/local/lib"), Read);
    addDirectoryPermission(ASCIILiteral("/usr/local/lib32"), Read);
    addDirectoryPermission(ASCIILiteral("/usr/local/lib64"), Read);
    addDirectoryPermission(ASCIILiteral(LIBDIR), Read);

    // System data directories
    addDirectoryPermission(ASCIILiteral("/usr/share"), Read);
    addDirectoryPermission(ASCIILiteral("/usr/local/share"), Read);
    addDirectoryPermission(ASCIILiteral(DATADIR), Read);

    // NPAPI plugins
    for (String& path : pluginsDirectories())
        addDirectoryPermission(path, Read);

    // SSL Certificates.
    addDirectoryPermission(ASCIILiteral("/etc/ssl/certs"), Read);

    // Audio devices, random number generators, etc.
    addDirectoryPermission(ASCIILiteral("/dev"), ReadAndWrite);

    // Temporary files and process self information.
    addDirectoryPermission(ASCIILiteral("/tmp"), ReadAndWrite);
    addDirectoryPermission(ASCIILiteral("/proc/") + String::number(getpid()), ReadAndWrite);

    // In some distros /dev/shm is a symbolic link to /run/shm, and in
    // this case, the canonical path resolver will follow the link. If
    // inside /dev, the policy is already set.
    addDirectoryPermission(ASCIILiteral("/run/shm"), ReadAndWrite);

    // Needed by glibc for networking and locale.
    addFilePermission(ASCIILiteral("/etc/gai.conf"), Read);
    addFilePermission(ASCIILiteral("/etc/host.conf"), Read);
    addFilePermission(ASCIILiteral("/etc/hosts"), Read);
    addFilePermission(ASCIILiteral("/etc/localtime"), Read);
    addFilePermission(ASCIILiteral("/etc/nsswitch.conf"), Read);

    // Needed for DNS resoltion. In some distros, the resolv.conf inside
    // /etc is just a symbolic link.
    addFilePermission(ASCIILiteral("/etc/resolv.conf"), Read);
    addFilePermission(ASCIILiteral("/run/resolvconf/resolv.conf"), Read);

    // Needed to convert uid and gid into names.
    addFilePermission(ASCIILiteral("/etc/group"), Read);
    addFilePermission(ASCIILiteral("/etc/passwd"), Read);

    // Needed by the loader.
    addFilePermission(ASCIILiteral("/etc/ld.so.cache"), Read);

    // Needed by various, including toolkits, for optimizations based
    // on the current amount of free system memory.
    addFilePermission(ASCIILiteral("/proc/cpuinfo"), Read);
    addFilePermission(ASCIILiteral("/proc/filesystems"), Read);
    addFilePermission(ASCIILiteral("/proc/meminfo"), Read);
    addFilePermission(ASCIILiteral("/proc/stat"), Read);

    // Needed by D-Bus.
    addFilePermission(ASCIILiteral("/var/lib/dbus/machine-id"), Read);

    // Needed by at-spi2.
    // FIXME This is too permissive: https://bugs.webkit.org/show_bug.cgi?id=143004
    addDirectoryPermission("/run/user/" + String::number(getuid()), ReadAndWrite);

    // Needed by WebKit's memory pressure handler.
    addFilePermission(ASCIILiteral("/sys/fs/cgroup/memory/memory.pressure_level"), Read);
    addFilePermission(ASCIILiteral("/sys/fs/cgroup/memory/cgroup.event_control"), Read);

    // X11 connection token.
    addFilePermission(userHomeDirectory() + "/.Xauthority", Read);

    // MIME type resolution.
    addDirectoryPermission(userDataDirectory() + "/mime", Read);

    // Needed by NVIDIA proprietary graphics driver.
    addDirectoryPermission(userHomeDirectory() + "/.nv", ReadAndWrite);

    // Needed by udev.
    addDirectoryPermission(ASCIILiteral("/etc/udev"), Read);
    addDirectoryPermission(ASCIILiteral("/run/udev"), Read);
    addDirectoryPermission(ASCIILiteral("/sys/bus"), Read);
    addDirectoryPermission(ASCIILiteral("/sys/class"), Read);
    addDirectoryPermission(ASCIILiteral("/sys/devices"), Read);

    // PulseAudio
    addFilePermission(ASCIILiteral("/etc/asound.conf"), Read);
    addDirectoryPermission(userConfigDirectory() + "/.pulse", Read);
    addDirectoryPermission(userHomeDirectory() + "/.pulse", Read);

    // Mesa
    addFilePermission(ASCIILiteral("/etc/drirc"), Read);
    addFilePermission(userHomeDirectory() + "/.drirc", Read);
    addFilePermission(ASCIILiteral("/sys/fs/selinux/booleans/allow_execmem"), Read);

    // GStreamer
    addDirectoryPermission(String::fromUTF8(LIBEXECDIR) + "/gstreamer-1.0", Read);
    addDirectoryPermission(userDataDirectory() + "/gstreamer-1.0", Read);
    addDirectoryPermission(userCacheDirectory() + "/gstreamer-1.0", ReadAndWrite);
    addDirectoryPermission(userHomeDirectory() + "/.frei0r-1", ReadAndWrite);
    if (char* gstreamerPluginDirectory = getenv("GST_PLUGIN_PATH_1_0"))
        addDirectoryPermission(gstreamerPluginDirectory, Read);
    if (char* gstreamerRegistryFile = getenv("GST_REGISTRY_1_0"))
        addFilePermission(gstreamerRegistryFile, ReadAndWrite);

    // Fontconfig
    addDirectoryPermission(userCacheDirectory() + "/fontconfig", ReadAndWrite);
    addDirectoryPermission(userConfigDirectory() + "/fontconfig", Read);
    addDirectoryPermission(userConfigDirectory() + "/fonts", Read);
    addDirectoryPermission(userDataDirectory() + "/fonts", Read);
    addDirectoryPermission(userHomeDirectory() + "/fontconfig", Read);
    addDirectoryPermission(userHomeDirectory() + "/.fonts", Read);
    addDirectoryPermission(ASCIILiteral("/etc/fonts"), Read);
    addDirectoryPermission(ASCIILiteral("/var/cache/fontconfig"), Read);

#if ENABLE(DEVELOPER_MODE) && defined(SOURCE_DIR)
    // Developers using build-webkit expect some libraries to be loaded
    // from the build root directory and they also need access to layout test
    // files.
    addDirectoryPermission(String::fromUTF8(SOURCE_DIR), SyscallPolicy::ReadAndWrite);
#endif
}

const char* SyscallPolicy::permissionToString(Permission permission)
{
    switch (permission) {
    case Read:
        return "read";
    case Write:
        return "write";
    case ReadAndWrite:
        return "read/write";
    case NotAllowed:
        return "disallowed";
    }

    ASSERT_NOT_REACHED();
    return "unknown action";
}

} // namespace WebKit

#endif // ENABLE(SECCOMP_FILTERS)
