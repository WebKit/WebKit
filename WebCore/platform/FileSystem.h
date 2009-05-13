/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
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
 
#ifndef FileSystem_h
#define FileSystem_h

#if PLATFORM(GTK)
#include <gmodule.h>
#endif
#if PLATFORM(QT)
#include <QFile>
#include <QLibrary>
#if defined(Q_OS_WIN32)
#include <windows.h>
#endif
#endif

#include <time.h>

#include <wtf/Platform.h>
#include <wtf/Vector.h>

#include "PlatformString.h"

typedef const struct __CFData* CFDataRef;

#if PLATFORM(WIN)
// These are to avoid including <winbase.h> in a header for Chromium
typedef void *HANDLE;
// Assuming STRICT
typedef struct HINSTANCE__* HINSTANCE;
typedef HINSTANCE HMODULE;
#endif

namespace WebCore {

class CString;

#if PLATFORM(WIN)
typedef HANDLE PlatformFileHandle;
typedef HMODULE PlatformModule;
// FIXME: -1 is INVALID_HANDLE_VALUE, defined in <winbase.h>. Chromium tries to
// avoid using Windows headers in headers.  We'd rather move this into the .cpp.
const PlatformFileHandle invalidPlatformFileHandle = reinterpret_cast<HANDLE>(-1);

struct PlatformModuleVersion {
    unsigned leastSig;
    unsigned mostSig;

    PlatformModuleVersion(unsigned)
        : leastSig(0)
        , mostSig(0)
    {
    }

    PlatformModuleVersion(unsigned lsb, unsigned msb)
        : leastSig(lsb)
        , mostSig(msb)
    {
    }

};
#elif PLATFORM(QT)

typedef QFile* PlatformFileHandle;
const PlatformFileHandle invalidPlatformFileHandle = 0;
#if defined(Q_WS_MAC)
typedef CFBundleRef PlatformModule;
typedef unsigned PlatformModuleVersion;
#elif defined(Q_OS_WIN)
typedef HMODULE PlatformModule;
struct PlatformModuleVersion {
    unsigned leastSig;
    unsigned mostSig;

    PlatformModuleVersion(unsigned)
        : leastSig(0)
        , mostSig(0)
    {
    }

    PlatformModuleVersion(unsigned lsb, unsigned msb)
        : leastSig(lsb)
        , mostSig(msb)
    {
    }

};
#else
typedef QLibrary* PlatformModule;
typedef unsigned PlatformModuleVersion;
#endif

#else
typedef int PlatformFileHandle;
#if PLATFORM(GTK)
typedef GModule* PlatformModule;
#else
typedef void* PlatformModule;
#endif
const PlatformFileHandle invalidPlatformFileHandle = -1;

typedef unsigned PlatformModuleVersion;
#endif

bool fileExists(const String&);
bool deleteFile(const String&);
bool deleteEmptyDirectory(const String&);
bool getFileSize(const String&, long long& result);
bool getFileModificationTime(const String&, time_t& result);
String pathByAppendingComponent(const String& path, const String& component);
bool makeAllDirectories(const String& path);
String homeDirectoryPath();
String pathGetFileName(const String&);
String directoryName(const String&);

Vector<String> listDirectory(const String& path, const String& filter = String());

CString fileSystemRepresentation(const String&);

inline bool isHandleValid(const PlatformFileHandle& handle) { return handle != invalidPlatformFileHandle; }

// Prefix is what the filename should be prefixed with, not the full path.
CString openTemporaryFile(const char* prefix, PlatformFileHandle&);
void closeFile(PlatformFileHandle&);
int writeToFile(PlatformFileHandle, const char* data, int length);

// Methods for dealing with loadable modules
bool unloadModule(PlatformModule);

#if PLATFORM(WIN)
String localUserSpecificStorageDirectory();
String roamingUserSpecificStorageDirectory();

bool safeCreateFile(const String&, CFDataRef);
#endif

#if PLATFORM(GTK)
String filenameToString(const char*);
char* filenameFromString(const String&);
String filenameForDisplay(const String&);
#endif

} // namespace WebCore

#endif // FileSystem_h
