/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "LibraryPathDiagnostics.h"

#include <dlfcn.h>
#include <notify.h>
#include <os/log.h>
#include <span>
#include <wtf/FileSystem.h>
#include <wtf/JSONValues.h>
#include <wtf/StringPrintStream.h>
#include <wtf/UUID.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/spi/cf/CFPrivSPI.h>
#include <wtf/spi/darwin/dyldSPI.h>

#if HAVE(CHIRP_SPI)
// Workaround for rdar://97945223
#define dlsym (const canary_meta_t *)dlsym
#include <chirp/chirp.h>
#undef dlsym
#endif

#if HAVE(SHARED_REGION_SPI)
#include <mach/shared_region.h>
#endif

namespace WTF {

static char const * const notificationName = "com.apple.WebKit.LibraryPathDiagnostics";
static char const * const loggingSubsystem = "com.apple.WebKit.LibraryPathDiagnostics";
static char const * const loggingCategory = "LibraryPathDiagnostics";
#if HAVE(DYLD_DLOPEN_IMAGE_HEADER_SPI)
static char const * const libraryListEnvironmentVariableName = "LIBRARY_PATH_DIAGNOSTICS_LIBRARIES";
#endif
static char const * const bundleListEnvironmentVariableName = "LIBRARY_PATH_DIAGNOSTICS_BUNDLES";

static String uuidToString(const uuid_t& uuid)
{
    uuid_string_t uuid_string = { };
    uuid_unparse(uuid, uuid_string);
    return String::fromUTF8(uuid_string);
}

class LibraryPathDiagnosticsLogger final {
public:
    LibraryPathDiagnosticsLogger();
    void log(void);
private:
    void logJSONPayload(const JSON::Object&);
    void logString(const Vector<String>& path, const String&);
    void logObject(const Vector<String>& path, Ref<JSON::Object>&&);
    void logError(const char*, ...);

    void logExecutablePath(void);
    void logDYLDSharedCacheInfo(void);
#if HAVE(DYLD_DLOPEN_IMAGE_HEADER_SPI)
    void logDynamicLibraryInfo(const String&);
    void logDynamicLibraryInfo(void);
#endif
    void logBundleInfo(const String&);
    void logBundleInfo(void);
#if HAVE(CHIRP_SPI)
    void logCryptexCanaryInfo(canary_cryptex_t, const String&);
    void logCryptexCanaryInfo(void);
#endif

    os_log_t m_osLog;
};

LibraryPathDiagnosticsLogger::LibraryPathDiagnosticsLogger()
    : m_osLog(os_log_create(loggingSubsystem, loggingCategory))
{
}

void LibraryPathDiagnosticsLogger::logJSONPayload(const JSON::Object &object)
{
    auto textRepresentation = object.toJSONString();
    os_log(m_osLog, "%{public}s", textRepresentation.utf8().data());
}

void LibraryPathDiagnosticsLogger::logString(const Vector<String>& path, const String& string)
{
    auto payload = JSON::Object::create();
    auto iter = path.rbegin();
    payload->setString(*iter, string);
    while (++iter != path.rend()) {
        auto nextPayload = JSON::Object::create();
        nextPayload->setObject(*iter, payload);
        payload = nextPayload;
    }

    logJSONPayload(payload);
}

void LibraryPathDiagnosticsLogger::logObject(const Vector<String>& path, Ref<JSON::Object>&& object)
{
    auto payload = JSON::Object::create();
    auto iter = path.rbegin();
    payload->setObject(*iter, WTFMove(object));
    for (iter++; iter != path.rend(); iter++) {
        auto nextPayload = JSON::Object::create();
        nextPayload->setObject(*iter, payload);
        payload = nextPayload;
    }

    logJSONPayload(payload);
}

void LibraryPathDiagnosticsLogger::logError(const char* format, ...)
{
    StringPrintStream stream;
    va_list argList;
    va_start(argList, format);
    stream.vprintf(format, argList);
    va_end(argList);

    os_log_error(m_osLog, "%{public}s", stream.toCString().data());
}

void LibraryPathDiagnosticsLogger::logExecutablePath(void)
{
    logString({ "Path"_s }, FileSystem::realPath(String::fromUTF8(_CFProcessPath())));
}

void LibraryPathDiagnosticsLogger::logDYLDSharedCacheInfo(void)
{
    uuid_t uuid = { };
    _dyld_get_shared_cache_uuid(uuid);

    auto sharedCacheInfo = JSON::Object::create();
    sharedCacheInfo->setString("Path"_s, FileSystem::realPath(String::fromUTF8(dyld_shared_cache_file_path())));
    sharedCacheInfo->setString("UUID"_s, uuidToString(uuid));

    logObject({ "SharedCache"_s }, WTFMove(sharedCacheInfo));
}

#if HAVE(DYLD_DLOPEN_IMAGE_HEADER_SPI)
#if HAVE(SHARED_REGION_SPI)
static bool isAddressInSharedRegion(const void* addr)
{
    return std::bit_cast<uintptr_t>(addr) >= SHARED_REGION_BASE && std::bit_cast<uintptr_t>(addr) < (SHARED_REGION_BASE + SHARED_REGION_SIZE);
}
#endif // HAVE(SHARED_REGION_SPI)

void LibraryPathDiagnosticsLogger::logDynamicLibraryInfo(const String& installName)
{
    void *handle = dlopen(installName.utf8().data(), RTLD_NOLOAD);
    if (!handle)
        return;

    const struct mach_header *header = _dyld_get_dlopen_image_header(handle);
    if (!header) {
        logError("Unable to locate mach header for %s", installName.utf8().data());
        return;
    }

    Dl_info info = { };
    int dladdr_ret = dladdr(header, &info);
    if (!dladdr_ret) {
        logError("No info returned from dladdr() for %s", installName.utf8().data());
        return;
    }

    uuid_t uuid = { 0 };
    if (!_dyld_get_image_uuid(header, uuid)) {
        logError("No UUID found for %s", installName.utf8().data());
        return;
    }

    auto libraryObject = JSON::Object::create();

    libraryObject->setString("Path"_s, FileSystem::realPath(String::fromUTF8(info.dli_fname)));
    libraryObject->setString("UUID"_s, uuidToString(uuid));

#if HAVE(SHARED_REGION_SPI)
    libraryObject->setBoolean("InSharedCache"_s, isAddressInSharedRegion(header));
#endif

    logObject({ "Libraries"_s, installName }, WTFMove(libraryObject));
}

void LibraryPathDiagnosticsLogger::logDynamicLibraryInfo(void)
{
    const char *libraryNames = getenv(libraryListEnvironmentVariableName);
    if (!libraryNames)
        return;

    auto libraries = String::fromUTF8(libraryNames).split(',');
    for (const auto& installName : libraries)
        logDynamicLibraryInfo(installName);
}
#endif // HAVE(DYLD_DLOPEN_IMAGE_HEADER_SPI)

void LibraryPathDiagnosticsLogger::logBundleInfo(const String& bundleIdentifier)
{
    auto bundle = CFBundleGetBundleWithIdentifier(bundleIdentifier.createCFString().get());
    if (!bundle)
        return;

    auto bundleInfo = JSON::Object::create();

    auto bundleCFURL = adoptCF(CFBundleCopyBundleURL(bundle));
    auto bundleURL = URL(bundleCFURL.get());
    bundleInfo->setString("Path"_s, FileSystem::realPath(bundleURL.fileSystemPath()));

    if (auto version = dynamic_cf_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(bundle, kCFBundleVersionKey)))
        bundleInfo->setString("Version"_s, String(version));
    else
        bundleInfo->setValue("Version"_s, JSON::Value::null());

    logObject({ "Bundles"_s, bundleIdentifier }, WTFMove(bundleInfo));
}

void LibraryPathDiagnosticsLogger::logBundleInfo(void)
{
    const char *bundleIdentifiers = getenv(bundleListEnvironmentVariableName);
    if (!bundleIdentifiers)
        return;

    auto bundles = String::fromUTF8(bundleIdentifiers).split(',');
    for (const auto& bundleIdentifier : bundles)
        logBundleInfo(bundleIdentifier);
}

#if HAVE(CHIRP_SPI)
void LibraryPathDiagnosticsLogger::logCryptexCanaryInfo(canary_cryptex_t which, const String& description)
{
    const canary_meta_t *metadata = canary_metadata(which);

    if (!metadata) {
        logError("Unable to load canary metadata for '%s' cryptex", description.utf8().data());
        return;
    }

    auto cryptex = JSON::Object::create();
    cryptex->setString("Version"_s, String::fromUTF8(metadata->cm_get_version()));
    cryptex->setString("Variant"_s, String::fromUTF8(metadata->cm_get_variant()));

    logObject({ "Canary"_s, description }, WTFMove(cryptex));
}

void LibraryPathDiagnosticsLogger::logCryptexCanaryInfo(void)
{
    if (_CANARY_CRYPTEX_CNT != (2))
        logError("Unexpected cryptex count reported by chirp: %llu", _CANARY_CRYPTEX_CNT);

    logCryptexCanaryInfo(CANARY_CRYPTEX_OS, "OS"_s);
    logCryptexCanaryInfo(CANARY_CRYPTEX_APP, "App"_s);
}
#endif // HAVE(CHIRP_SPI)

void LibraryPathDiagnosticsLogger::log(void)
{
    logExecutablePath();
    logDYLDSharedCacheInfo();
#if HAVE(DYLD_DLOPEN_IMAGE_HEADER_SPI)
    logDynamicLibraryInfo();
#endif
    logBundleInfo();
#if HAVE(CHIRP_SPI)
    logCryptexCanaryInfo();
#endif
}

void initializeLibraryPathDiagnostics(void)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        int token = -1;
        notify_register_dispatch(notificationName, &token, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^(int token) {
            UNUSED_PARAM(token);
            logLibraryPathDiagnostics();
        });
    });
}

void logLibraryPathDiagnostics(void)
{
    LibraryPathDiagnosticsLogger logger;
    logger.log();
}

};
