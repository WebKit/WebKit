/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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

#import "config.h"

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
#import "AuxiliaryProcess.h"

#import "ApplicationServicesSPI.h"
#import "CodeSigning.h"
#import "SandboxInitializationParameters.h"
#import "SandboxUtilities.h"
#import "WKFoundation.h"
#import "XPCServiceEntryPoint.h"
#import <WebCore/FileHandle.h>
#import <WebCore/SystemVersion.h>
#import <mach-o/dyld.h>
#import <mach/mach.h>
#import <mach/task.h>
#import <pal/crypto/CryptoDigest.h>
#import <pal/spi/cocoa/CoreServicesSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/NotifySPI.h>
#import <pal/spi/mac/QuarantineSPI.h>
#import <pwd.h>
#import <stdlib.h>
#import <sys/sysctl.h>
#import <sysexits.h>
#import <wtf/DataLog.h>
#import <wtf/FileSystem.h>
#import <wtf/SafeStrerror.h>
#import <wtf/Scope.h>
#import <wtf/SoftLinking.h>
#import <wtf/SystemTracing.h>
#import <wtf/WallTime.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/text/Base64.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/cf/StringConcatenateCF.h>

#if USE(APPLE_INTERNAL_SDK)
#import <rootless.h>
#endif

SOFT_LINK_SYSTEM_LIBRARY(libsystem_info)
SOFT_LINK_OPTIONAL(libsystem_info, mbr_close_connections, int, (), ());
SOFT_LINK_OPTIONAL(libsystem_info, lookup_close_connections, int, (), ());

#if ENABLE(NOTIFY_FILTERING)
SOFT_LINK_SYSTEM_LIBRARY(libsystem_notify)
SOFT_LINK_OPTIONAL(libsystem_notify, notify_set_options, void, __cdecl, (uint32_t));
#endif

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(ApplicationServices, HIServices)
SOFT_LINK_OPTIONAL(HIServices, HIS_XPC_ResetMessageConnection, void, (), ())

#if PLATFORM(MAC)
#define USE_CACHE_COMPILED_SANDBOX 1
#else
#define USE_CACHE_COMPILED_SANDBOX 0
#endif

extern const char* const SANDBOX_BUILD_ID; // Defined by the Sandbox framework

namespace WebKit {
using namespace WebCore;

#if USE(CACHE_COMPILED_SANDBOX)
using SandboxProfile = typename std::remove_pointer<sandbox_profile_t>::type;
struct SandboxProfileDeleter {
    void operator()(SandboxProfile* ptr)
    {
        sandbox_free_profile(ptr);
    }
};
using SandboxProfilePtr = std::unique_ptr<SandboxProfile, SandboxProfileDeleter>;

using SandboxParameters = typename std::remove_pointer<sandbox_params_t>::type;
struct SandboxParametersDeleter {
    void operator()(SandboxParameters* ptr)
    {
        sandbox_free_params(ptr);
    }
};
using SandboxParametersPtr = std::unique_ptr<SandboxParameters, SandboxParametersDeleter>;

constexpr unsigned guidSize = 36 + 1;
constexpr unsigned versionSize = 31 + 1;

struct CachedSandboxHeader {
    uint32_t versionNumber;
    uint32_t libsandboxVersion;
    uint32_t headerSize;
    uint32_t builtinSize; // If a builtin doesn't exist, this is UINT_MAX.
    uint32_t dataSize;
    char sandboxBuildID[guidSize];
    char osVersion[versionSize];
};
// The file is layed out on disk like:
// byte 0
// CachedSandboxHeader <- sizeof(CachedSandboxHeader) bytes
// SandboxHeader <- CachedSandboxHeader::headerSize bytes
// [SandboxBuiltin] optional. Present if CachedSandboxHeader::builtinSize is not UINT_MAX. If present, builtinSize bytes (not including null termination).
// SandboxData <- CachedSandboxHeader::dataSize bytes
// byte N

struct SandboxInfo {
    SandboxInfo(const String& parentDirectoryPath, const String& directoryPath, const String& filePath, const SandboxParametersPtr& sandboxParameters, const CString& header, const WebCore::AuxiliaryProcessType& processType, const SandboxInitializationParameters& initializationParameters, const String& profileOrProfilePath, bool isProfilePath)
        : parentDirectoryPath { parentDirectoryPath }
        , directoryPath { directoryPath }
        , filePath { filePath }
        , sandboxParameters { sandboxParameters }
        , header { header }
        , processType { processType }
        , initializationParameters { initializationParameters }
        , profileOrProfilePath { profileOrProfilePath }
        , isProfilePath { isProfilePath }
    {
    }

    const String& parentDirectoryPath;
    const String& directoryPath;
    const String& filePath;
    const SandboxParametersPtr& sandboxParameters;
    const CString& header;
    const WebCore::AuxiliaryProcessType& processType;
    const SandboxInitializationParameters& initializationParameters;
    const String& profileOrProfilePath;
    const bool isProfilePath;
};

constexpr uint32_t CachedSandboxVersionNumber = 1;
#endif // USE(CACHE_COMPILED_SANDBOX)

void AuxiliaryProcess::launchServicesCheckIn()
{
#if HAVE(CSCHECKFIXDISABLE)
    // _CSCheckFixDisable() needs to be called before checking in with Launch Services.
    _CSCheckFixDisable();
#endif

    _LSSetApplicationLaunchServicesServerConnectionStatus(0, 0);
    RetainPtr<CFDictionaryRef> unused = _LSApplicationCheckIn(kLSDefaultSessionID, CFBundleGetInfoDictionary(CFBundleGetMainBundle()));
}

static OSStatus enableSandboxStyleFileQuarantine()
{
#if !PLATFORM(MACCATALYST)
    qtn_proc_t quarantineProperties = qtn_proc_alloc();
    auto quarantinePropertiesDeleter = makeScopeExit([quarantineProperties]() {
        qtn_proc_free(quarantineProperties);
    });


    if (qtn_proc_init_with_self(quarantineProperties)) {
        // See <rdar://problem/13463752>.
        qtn_proc_init(quarantineProperties);
    }

    if (auto error = qtn_proc_set_flags(quarantineProperties, QTN_FLAG_SANDBOX))
        return error;

    // QTN_FLAG_SANDBOX is silently ignored if security.mac.qtn.sandbox_enforce sysctl is 0.
    // In that case, quarantine falls back to advisory QTN_FLAG_DOWNLOAD.
    return qtn_proc_apply_to_self(quarantineProperties);
#else
    return false;
#endif
}

static void setNotifyOptions()
{
#if ENABLE(NOTIFY_FILTERING)
    if (notify_set_optionsPtr())
        notify_set_optionsPtr()(NOTIFY_OPT_DISPATCH | NOTIFY_OPT_REGEN | NOTIFY_OPT_FILTERED);
#endif
}

#if USE(CACHE_COMPILED_SANDBOX)
static std::optional<Vector<char>> fileContents(const String& path, bool shouldLock = false, OptionSet<FileSystem::FileLockMode> lockMode = FileSystem::FileLockMode::Exclusive)
{
    FileHandle file = shouldLock ? FileHandle(path, FileSystem::FileOpenMode::Read, lockMode) : FileHandle(path, FileSystem::FileOpenMode::Read);
    file.open();
    if (!file)
        return std::nullopt;

    char chunk[4096];
    constexpr size_t chunkSize = std::size(chunk);
    size_t contentSize = 0;
    Vector<char> contents;
    contents.reserveInitialCapacity(chunkSize);
    while (size_t bytesRead = file.read(chunk, chunkSize)) {
        contents.append(chunk, bytesRead);
        contentSize += bytesRead;
    }
    contents.resize(contentSize);

    return contents;
}

#if USE(APPLE_INTERNAL_SDK)
// These strings must match the last segment of the "com.apple.rootless.storage.<this part must match>" entry in each
// process's restricted entitlements file (ex. Configurations/Networking-OSX-restricted.entitlements).
constexpr const char* processStorageClass(WebCore::AuxiliaryProcessType type)
{
    switch (type) {
    case WebCore::AuxiliaryProcessType::WebContent:
        return "WebKitWebContentSandbox";
    case WebCore::AuxiliaryProcessType::Network:
        return "WebKitNetworkingSandbox";
    case WebCore::AuxiliaryProcessType::Plugin:
        return "WebKitPluginSandbox";
#if ENABLE(GPU_PROCESS)
    case WebCore::AuxiliaryProcessType::GPU:
        return "WebKitGPUSandbox";
#endif
    }
}
#endif // USE(APPLE_INTERNAL_SDK)

static std::optional<CString> setAndSerializeSandboxParameters(const SandboxInitializationParameters& initializationParameters, const SandboxParametersPtr& sandboxParameters, const String& profileOrProfilePath, bool isProfilePath)
{
    StringBuilder builder;
    for (size_t i = 0; i < initializationParameters.count(); ++i) {
        const char* name = initializationParameters.name(i);
        const char* value = initializationParameters.value(i);
        if (sandbox_set_param(sandboxParameters.get(), name, value)) {
            WTFLogAlways("%s: Could not set sandbox parameter: %s\n", getprogname(), safeStrerror(errno).data());
            CRASH();
        }
        builder.append(name, ':', value, ':');
    }
    if (isProfilePath) {
        auto contents = fileContents(profileOrProfilePath);
        if (!contents)
            return std::nullopt;
        builder.appendCharacters(contents->data(), contents->size());
    } else
        builder.append(profileOrProfilePath);
    return builder.toString().ascii();
}

static String sandboxDataVaultParentDirectory()
{
    char temp[PATH_MAX];
    size_t length = confstr(_CS_DARWIN_USER_CACHE_DIR, temp, sizeof(temp));
    if (!length) {
        WTFLogAlways("%s: Could not retrieve user temporary directory path: %s\n", getprogname(), safeStrerror(errno).data());
        exit(EX_NOPERM);
    }
    RELEASE_ASSERT(length <= sizeof(temp));
    char resolvedPath[PATH_MAX];
    if (!realpath(temp, resolvedPath)) {
        WTFLogAlways("%s: Could not canonicalize user temporary directory path: %s\n", getprogname(), safeStrerror(errno).data());
        exit(EX_NOPERM);
    }
    return String::fromUTF8(resolvedPath);
}

static String sandboxDirectory(WebCore::AuxiliaryProcessType processType, const String& parentDirectory)
{
    StringBuilder directory;
    directory.append(parentDirectory);
    switch (processType) {
    case WebCore::AuxiliaryProcessType::WebContent:
        directory.append("/com.apple.WebKit.WebContent.Sandbox");
        break;
    case WebCore::AuxiliaryProcessType::Network:
        directory.append("/com.apple.WebKit.Networking.Sandbox");
        break;
    case WebCore::AuxiliaryProcessType::Plugin:
        WTFLogAlways("sandboxDirectory: Unexpected Plugin process initialization.");
        CRASH();
        break;
#if ENABLE(GPU_PROCESS)
    case WebCore::AuxiliaryProcessType::GPU:
        directory.append("/com.apple.WebKit.GPU.Sandbox");
        break;
#endif
    }

#if !USE(APPLE_INTERNAL_SDK)
    // Add .OpenSource suffix so that open source builds don't try to access a data vault used by system Safari.
    directory.append(".OpenSource");
#endif

    return directory.toString();
}

static String sandboxFilePath(const String& directoryPath, const CString& header)
{
    // Make the filename semi-unique based on the contents of the header.

    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(header.data(), header.length());
    auto hash = crypto->computeHash();

    return makeString(directoryPath, "/CompiledSandbox+", base64URLEncoded(hash.data(), hash.size()));
}

static bool ensureSandboxCacheDirectory(const SandboxInfo& info)
{
    if (FileSystem::fileTypeFollowingSymlinks(info.parentDirectoryPath) != FileSystem::FileType::Directory) {
        FileSystem::makeAllDirectories(info.parentDirectoryPath);
        if (FileSystem::fileTypeFollowingSymlinks(info.parentDirectoryPath) != FileSystem::FileType::Directory) {
            WTFLogAlways("%s: Could not create sandbox directory\n", getprogname());
            return false;
        }
    }

#if USE(APPLE_INTERNAL_SDK)
    const char* storageClass = processStorageClass(info.processType);
    CString directoryPath = FileSystem::fileSystemRepresentation(info.directoryPath);
    if (directoryPath.isNull())
        return false;

    auto makeDataVault = [&] {
        do {
            if (!rootless_mkdir_datavault(directoryPath.data(), 0700, storageClass))
                return true;
        } while (errno == EAGAIN);
        return false;
    };

    if (makeDataVault())
        return true;

    if (errno == EEXIST) {
        // The directory already exists. First we'll check if it is a data vault. If it is then
        // we are the ones who created it and we can continue. If it is not a datavault then we'll just
        // delete it and try to make a new one.
        if (!rootless_check_datavault_flag(directoryPath.data(), storageClass))
            return true;

        if (FileSystem::fileType(info.directoryPath) == FileSystem::FileType::Directory) {
            if (!FileSystem::deleteNonEmptyDirectory(info.directoryPath))
                return false;
        } else {
            if (!FileSystem::deleteFile(info.directoryPath))
                return false;
        }

        if (!makeDataVault())
            return false;
    } else {
        WTFLogAlways("%s: Sandbox directory couldn't be created: ", getprogname(), safeStrerror(errno).data());
        return false;
    }
#else
    bool hasSandboxDirectory = FileSystem::fileTypeFollowingSymlinks(info.directoryPath) == FileSystem::FileType::Directory;
    if (!hasSandboxDirectory) {
        if (FileSystem::makeAllDirectories(info.directoryPath)) {
            ASSERT(FileSystem::fileTypeFollowingSymlinks(info.directoryPath) == FileSystem::FileType::Directory);
            hasSandboxDirectory = true;
        } else {
            // We may have raced with someone else making it. That's ok.
            hasSandboxDirectory = FileSystem::fileTypeFollowingSymlinks(info.directoryPath) == FileSystem::FileType::Directory;
        }
    }

    if (!hasSandboxDirectory) {
        // Bailing because we don't have a sandbox directory.
        return false;
    }
#endif // USE(APPLE_INTERNAL_SDK)

    return true;
}

static bool writeSandboxDataToCacheFile(const SandboxInfo& info, const Vector<char>& cacheFile)
{
    FileHandle file { info.filePath, FileSystem::FileOpenMode::Write, FileSystem::FileLockMode::Exclusive };
    return file.write(cacheFile.data(), cacheFile.size()) == safeCast<int>(cacheFile.size());
}

static SandboxProfilePtr compileAndCacheSandboxProfile(const SandboxInfo& info)
{
    if (!ensureSandboxCacheDirectory(info))
        return nullptr;

    char* error = nullptr;
    CString profileOrProfilePath = info.isProfilePath ? FileSystem::fileSystemRepresentation(info.profileOrProfilePath) : info.profileOrProfilePath.utf8();
    if (profileOrProfilePath.isNull())
        return nullptr;
    SandboxProfilePtr sandboxProfile { info.isProfilePath ? sandbox_compile_file(profileOrProfilePath.data(), info.sandboxParameters.get(), &error) : sandbox_compile_string(profileOrProfilePath.data(), info.sandboxParameters.get(), &error) };
    if (!sandboxProfile) {
        WTFLogAlways("%s: Could not compile WebContent sandbox: %s\n", getprogname(), error);
        return nullptr;
    }

    const bool haveBuiltin = sandboxProfile->builtin;
    int32_t libsandboxVersion = NSVersionOfRunTimeLibrary("sandbox");
    RELEASE_ASSERT(libsandboxVersion > 0);
    String osVersion = systemMarketingVersion();

    CachedSandboxHeader cachedHeader {
        CachedSandboxVersionNumber,
        static_cast<uint32_t>(libsandboxVersion),
        safeCast<uint32_t>(info.header.length()),
        haveBuiltin ? safeCast<uint32_t>(strlen(sandboxProfile->builtin)) : std::numeric_limits<uint32_t>::max(),
        safeCast<uint32_t>(sandboxProfile->size),
        { 0 },
        { 0 }
    };

    size_t copied = strlcpy(cachedHeader.sandboxBuildID, SANDBOX_BUILD_ID, sizeof(cachedHeader.sandboxBuildID));
    ASSERT_UNUSED(copied, copied == guidSize - 1);
    copied = strlcpy(cachedHeader.osVersion, osVersion.utf8().data(), sizeof(cachedHeader.osVersion));
    ASSERT(copied < versionSize - 1);

    const size_t expectedFileSize = sizeof(cachedHeader) + cachedHeader.headerSize + (haveBuiltin ? cachedHeader.builtinSize : 0) + cachedHeader.dataSize;

    Vector<char> cacheFile;
    cacheFile.reserveInitialCapacity(expectedFileSize);
    cacheFile.append(bitwise_cast<uint8_t*>(&cachedHeader), sizeof(CachedSandboxHeader));
    cacheFile.append(info.header.data(), info.header.length());
    if (haveBuiltin)
        cacheFile.append(sandboxProfile->builtin, cachedHeader.builtinSize);
    cacheFile.append(sandboxProfile->data, cachedHeader.dataSize);

    if (!writeSandboxDataToCacheFile(info, cacheFile))
        WTFLogAlways("%s: Unable to cache compiled sandbox\n", getprogname());

    return sandboxProfile;
}

static bool tryApplyCachedSandbox(const SandboxInfo& info)
{
#if USE(APPLE_INTERNAL_SDK)
    CString directoryPath = FileSystem::fileSystemRepresentation(info.directoryPath);
    if (directoryPath.isNull())
        return false;
    if (rootless_check_datavault_flag(directoryPath.data(), processStorageClass(info.processType)))
        return false;
#endif

    auto contents = fileContents(info.filePath, true, FileSystem::FileLockMode::Shared);
    if (!contents || contents->isEmpty())
        return false;
    Vector<char> cachedSandboxContents = WTFMove(*contents);
    if (sizeof(CachedSandboxHeader) > cachedSandboxContents.size())
        return false;

    // This data may be corrupted if the sandbox file was cached on a different platform with different endianness
    CachedSandboxHeader cachedSandboxHeader;
    memcpy(&cachedSandboxHeader, cachedSandboxContents.data(), sizeof(CachedSandboxHeader));
    int32_t libsandboxVersion = NSVersionOfRunTimeLibrary("sandbox");
    RELEASE_ASSERT(libsandboxVersion > 0);
    String osVersion = systemMarketingVersion();

    if (cachedSandboxHeader.versionNumber != CachedSandboxVersionNumber)
        return false;
    if (static_cast<uint32_t>(libsandboxVersion) != cachedSandboxHeader.libsandboxVersion)
        return false;
    if (std::strcmp(cachedSandboxHeader.sandboxBuildID, SANDBOX_BUILD_ID))
        return false;
    if (StringView::fromLatin1(cachedSandboxHeader.osVersion) != osVersion)
        return false;

    const bool haveBuiltin = cachedSandboxHeader.builtinSize != std::numeric_limits<uint32_t>::max();

    // These values are computed based on the disk layout specified below the definition of the CachedSandboxHeader struct
    // and must be changed if the layout changes.
    const char* sandboxHeaderPtr = bitwise_cast<char *>(cachedSandboxContents.data()) + sizeof(CachedSandboxHeader);
    const char* sandboxBuiltinPtr = sandboxHeaderPtr + cachedSandboxHeader.headerSize;
    unsigned char* sandboxDataPtr = bitwise_cast<unsigned char*>(haveBuiltin ? sandboxBuiltinPtr + cachedSandboxHeader.builtinSize : sandboxBuiltinPtr);

    size_t expectedFileSize = sizeof(CachedSandboxHeader) + cachedSandboxHeader.headerSize + cachedSandboxHeader.dataSize;
    if (haveBuiltin)
        expectedFileSize += cachedSandboxHeader.builtinSize;
    if (cachedSandboxContents.size() != expectedFileSize)
        return false;
    if (cachedSandboxHeader.headerSize != info.header.length())
        return false;
    if (memcmp(sandboxHeaderPtr, info.header.data(), info.header.length()))
        return false;

    SandboxProfile profile { };
    CString builtin;
    profile.builtin = nullptr;
    profile.size = cachedSandboxHeader.dataSize;
    if (haveBuiltin) {
        builtin = CString::newUninitialized(cachedSandboxHeader.builtinSize, profile.builtin);
        if (builtin.isNull())
            return false;
        memcpy(profile.builtin, sandboxBuiltinPtr, cachedSandboxHeader.builtinSize);
    }
    ASSERT(static_cast<void *>(sandboxDataPtr + profile.size) <= static_cast<void *>(cachedSandboxContents.data() + cachedSandboxContents.size()));
    profile.data = sandboxDataPtr;

    setNotifyOptions();

    if (sandbox_apply(&profile)) {
        WTFLogAlways("%s: Could not apply cached sandbox: %s\n", getprogname(), safeStrerror(errno).data());
        return false;
    }

    return true;
}
#endif // USE(CACHE_COMPILED_SANDBOX)

static inline const NSBundle *webKit2Bundle()
{
    const static NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit"];
    return bundle;
}

static void getSandboxProfileOrProfilePath(const SandboxInitializationParameters& parameters, String& profileOrProfilePath, bool& isProfilePath)
{
    switch (parameters.mode()) {
    case SandboxInitializationParameters::ProfileSelectionMode::UseDefaultSandboxProfilePath:
        profileOrProfilePath = [webKit2Bundle() pathForResource:[[NSBundle mainBundle] bundleIdentifier] ofType:@"sb"];
        isProfilePath = true;
        return;
    case SandboxInitializationParameters::ProfileSelectionMode::UseOverrideSandboxProfilePath:
        profileOrProfilePath = parameters.overrideSandboxProfilePath();
        isProfilePath = true;
        return;
    case SandboxInitializationParameters::ProfileSelectionMode::UseSandboxProfile:
        profileOrProfilePath = parameters.sandboxProfile();
        isProfilePath = false;
        return;
    }
}

static bool compileAndApplySandboxSlowCase(const String& profileOrProfilePath, bool isProfilePath, const SandboxInitializationParameters& parameters)
{
    char* errorBuf;
    CString temp = isProfilePath ? FileSystem::fileSystemRepresentation(profileOrProfilePath) : profileOrProfilePath.utf8();
    uint64_t flags = isProfilePath ? SANDBOX_NAMED_EXTERNAL : 0;

    setNotifyOptions();

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (sandbox_init_with_parameters(temp.data(), flags, parameters.namedParameterArray(), &errorBuf)) {
        ALLOW_DEPRECATED_DECLARATIONS_END
        WTFLogAlways("%s: Could not initialize sandbox profile [%s], error '%s'\n", getprogname(), temp.data(), errorBuf);
        for (size_t i = 0, count = parameters.count(); i != count; ++i)
            WTFLogAlways("%s=%s\n", parameters.name(i), parameters.value(i));
        return false;
    }
    return true;
}

static bool applySandbox(const AuxiliaryProcessInitializationParameters& parameters, const SandboxInitializationParameters& sandboxInitializationParameters, const String& dataVaultParentDirectory)
{
    String profileOrProfilePath;
    bool isProfilePath;
    getSandboxProfileOrProfilePath(sandboxInitializationParameters, profileOrProfilePath, isProfilePath);
    if (profileOrProfilePath.isEmpty()) {
        WTFLogAlways("%s: Profile path is invalid\n", getprogname());
        CRASH();
    }

#if USE(CACHE_COMPILED_SANDBOX)
    // The plugin process's DARWIN_USER_TEMP_DIR and DARWIN_USER_CACHE_DIR sandbox parameters are randomized so
    // so the compiled sandbox should not be cached because it won't be reused.
    if (parameters.processType == WebCore::AuxiliaryProcessType::Plugin) {
        WTFLogAlways("applySandbox: Unexpected Plugin process initialization.");
        CRASH();
    }

    SandboxParametersPtr sandboxParameters { sandbox_create_params() };
    if (!sandboxParameters) {
        WTFLogAlways("%s: Could not create sandbox parameters\n", getprogname());
        CRASH();
    }
    auto header = setAndSerializeSandboxParameters(sandboxInitializationParameters, sandboxParameters, profileOrProfilePath, isProfilePath);
    if (!header) {
        WTFLogAlways("%s: Sandbox parameters are invalid\n", getprogname());
        CRASH();
    }

    String directoryPath { sandboxDirectory(parameters.processType, dataVaultParentDirectory) };
    String filePath = sandboxFilePath(directoryPath, *header);
    SandboxInfo info {
        dataVaultParentDirectory,
        directoryPath,
        filePath,
        sandboxParameters,
        *header,
        parameters.processType,
        sandboxInitializationParameters,
        profileOrProfilePath,
        isProfilePath
    };

    if (tryApplyCachedSandbox(info))
        return true;

    SandboxProfilePtr sandboxProfile = compileAndCacheSandboxProfile(info);
    if (!sandboxProfile)
        return compileAndApplySandboxSlowCase(profileOrProfilePath, isProfilePath, sandboxInitializationParameters);

    setNotifyOptions();
    
    if (sandbox_apply(sandboxProfile.get())) {
        WTFLogAlways("%s: Could not apply compiled sandbox: %s\n", getprogname(), safeStrerror(errno).data());
        CRASH();
    }

    return true;
#else
    UNUSED_PARAM(parameters);
    UNUSED_PARAM(dataVaultParentDirectory);
    return compileAndApplySandboxSlowCase(profileOrProfilePath, isProfilePath, sandboxInitializationParameters);
#endif // USE(CACHE_COMPILED_SANDBOX)
}

static String getUserDirectorySuffix(const AuxiliaryProcessInitializationParameters& parameters)
{
    auto userDirectorySuffix = parameters.extraInitializationData.find<HashTranslatorASCIILiteral>("user-directory-suffix"_s);
    if (userDirectorySuffix != parameters.extraInitializationData.end()) {
        String suffix = userDirectorySuffix->value;
        return suffix.left(suffix.find('/'));
    }

    String clientIdentifier = codeSigningIdentifier(parameters.connectionIdentifier.xpcConnection.get());
    if (clientIdentifier.isNull())
        clientIdentifier = parameters.clientIdentifier;
    return makeString([[NSBundle mainBundle] bundleIdentifier], '+', clientIdentifier);
}

static StringView parseOSVersion(StringView osSystemMarketingVersion)
{
    auto firstDotIndex = osSystemMarketingVersion.find('.');
    if (firstDotIndex == notFound)
        return { };
    auto secondDotIndex = osSystemMarketingVersion.find('.', firstDotIndex + 1);
    if (secondDotIndex == notFound)
        return osSystemMarketingVersion;
    return osSystemMarketingVersion.left(secondDotIndex);
}

static String getHomeDirectory()
{
    // According to the man page for getpwuid_r, we should use sysconf(_SC_GETPW_R_SIZE_MAX) to determine the size of the buffer.
    // However, a buffer size of 4096 should be sufficient, since PATH_MAX is 1024.
    char buffer[4096];
    passwd pwd;
    passwd* result = nullptr;
    if (getpwuid_r(getuid(), &pwd, buffer, sizeof(buffer), &result) || !result) {
        WTFLogAlways("%s: Couldn't find home directory", getprogname());
        RELEASE_ASSERT_NOT_REACHED();
    }
    return String::fromUTF8(pwd.pw_dir);
}

static void populateSandboxInitializationParameters(SandboxInitializationParameters& sandboxParameters)
{
    RELEASE_ASSERT(!sandboxParameters.userDirectorySuffix().isNull());

    String osSystemMarketingVersion = systemMarketingVersion();
    auto osVersion = parseOSVersion(osSystemMarketingVersion);
    if (osVersion.isNull()) {
        WTFLogAlways("%s: Couldn't find OS Version\n", getprogname());
        exit(EX_NOPERM);
    }
    sandboxParameters.addParameter("_OS_VERSION", osVersion.utf8().data());

    // Use private temporary and cache directories.
    setenv("DIRHELPER_USER_DIR_SUFFIX", FileSystem::fileSystemRepresentation(sandboxParameters.userDirectorySuffix()).data(), 1);
    char temporaryDirectory[PATH_MAX];
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory))) {
        WTFLogAlways("%s: couldn't retrieve private temporary directory path: %d\n", getprogname(), errno);
        exit(EX_NOPERM);
    }
    setenv("TMPDIR", temporaryDirectory, 1);

    String bundlePath = webKit2Bundle().bundlePath;
    if (!bundlePath.startsWith("/System/Library/Frameworks"_s))
        bundlePath = webKit2Bundle().bundlePath.stringByDeletingLastPathComponent;

    sandboxParameters.addPathParameter("WEBKIT2_FRAMEWORK_DIR", bundlePath);
    sandboxParameters.addConfDirectoryParameter("DARWIN_USER_TEMP_DIR", _CS_DARWIN_USER_TEMP_DIR);
    sandboxParameters.addConfDirectoryParameter("DARWIN_USER_CACHE_DIR", _CS_DARWIN_USER_CACHE_DIR);

    auto homeDirectory = getHomeDirectory();
    
    sandboxParameters.addPathParameter("HOME_DIR", homeDirectory);
    String path = FileSystem::pathByAppendingComponent(homeDirectory, "Library"_s);
    sandboxParameters.addPathParameter("HOME_LIBRARY_DIR", FileSystem::fileSystemRepresentation(path).data());
    path = FileSystem::pathByAppendingComponent(path, "/Preferences"_s);
    sandboxParameters.addPathParameter("HOME_LIBRARY_PREFERENCES_DIR", FileSystem::fileSystemRepresentation(path).data());

#if CPU(X86_64)
    sandboxParameters.addParameter("CPU", "x86_64");
#elif CPU(ARM64)
    sandboxParameters.addParameter("CPU", "arm64");
#else
#error "Unknown architecture."
#endif
    if (mbr_close_connectionsPtr())
        mbr_close_connectionsPtr()();
    if (lookup_close_connectionsPtr())
        lookup_close_connectionsPtr()();
    if (HIS_XPC_ResetMessageConnectionPtr())
        HIS_XPC_ResetMessageConnectionPtr()();
}

void AuxiliaryProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
    TraceScope traceScope(InitializeSandboxStart, InitializeSandboxEnd);

#if USE(CACHE_COMPILED_SANDBOX)
    // This must be called before populateSandboxInitializationParameters so that the path does not include the user directory suffix.
    // We don't want the user directory suffix because we want all processes of the same type to use the same cache directory.
    String dataVaultParentDirectory { sandboxDataVaultParentDirectory() };
#else
    String dataVaultParentDirectory;
#endif

    bool enableMessageFilter = false;
#if HAVE(SANDBOX_MESSAGE_FILTERING)
    enableMessageFilter = WTF::processHasEntitlement("com.apple.private.security.message-filter"_s);
#endif
    sandboxParameters.addParameter("ENABLE_SANDBOX_MESSAGE_FILTER", enableMessageFilter ? "YES" : "NO");

    if (sandboxParameters.userDirectorySuffix().isNull())
        sandboxParameters.setUserDirectorySuffix(getUserDirectorySuffix(parameters));

    populateSandboxInitializationParameters(sandboxParameters);

    if (!applySandbox(parameters, sandboxParameters, dataVaultParentDirectory)) {
        WTFLogAlways("%s: Unable to apply sandbox\n", getprogname());
        CRASH();
    }

    if (shouldOverrideQuarantine()) {
        // This will override LSFileQuarantineEnabled from Info.plist unless sandbox quarantine is globally disabled.
        OSStatus error = enableSandboxStyleFileQuarantine();
        if (error) {
            WTFLogAlways("%s: Couldn't enable sandbox style file quarantine: %ld\n", getprogname(), static_cast<long>(error));
            exit(EX_NOPERM);
        }
    }
}

void AuxiliaryProcess::applySandboxProfileForDaemon(const String& profilePath, const String& userDirectorySuffix)
{
    TraceScope traceScope(InitializeSandboxStart, InitializeSandboxEnd);

    SandboxInitializationParameters parameters { };
    parameters.setOverrideSandboxProfilePath(profilePath);
    parameters.setUserDirectorySuffix(userDirectorySuffix);
    populateSandboxInitializationParameters(parameters);

    String profileOrProfilePath;
    bool isProfilePath;
    getSandboxProfileOrProfilePath(parameters, profileOrProfilePath, isProfilePath);
    RELEASE_ASSERT(!profileOrProfilePath.isEmpty());

    bool success = compileAndApplySandboxSlowCase(profileOrProfilePath, isProfilePath, parameters);
    RELEASE_ASSERT(success);
}

#if USE(APPKIT)
void AuxiliaryProcess::stopNSAppRunLoop()
{
    ASSERT([NSApp isRunning]);
    [NSApp stop:nil];

    NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSMakePoint(0, 0) modifierFlags:0 timestamp:0.0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    [NSApp postEvent:event atStart:true];
}
#endif

#if !PLATFORM(MACCATALYST) && ENABLE(WEBPROCESS_NSRUNLOOP)
void AuxiliaryProcess::stopNSRunLoop()
{
    ASSERT([NSRunLoop mainRunLoop]);
    [[NSRunLoop mainRunLoop] performBlock:^{
        exit(0);
    }];
}
#endif

void AuxiliaryProcess::setQOS(int latencyQOS, int throughputQOS)
{
    if (!latencyQOS && !throughputQOS)
        return;

    struct task_qos_policy qosinfo = {
        latencyQOS ? LATENCY_QOS_TIER_0 + latencyQOS - 1 : LATENCY_QOS_TIER_UNSPECIFIED,
        throughputQOS ? THROUGHPUT_QOS_TIER_0 + throughputQOS - 1 : THROUGHPUT_QOS_TIER_UNSPECIFIED
    };

    task_policy_set(mach_task_self(), TASK_OVERRIDE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
}

#if PLATFORM(MAC)
bool AuxiliaryProcess::isSystemWebKit()
{
    static bool isSystemWebKit = []() -> bool {
#if HAVE(READ_ONLY_SYSTEM_VOLUME)
        if ([[webKit2Bundle() bundlePath] hasPrefix:@"/Library/Apple/System/"])
            return true;
#endif
        return [[webKit2Bundle() bundlePath] hasPrefix:@"/System/"];
    }();
    return isSystemWebKit;
}

void AuxiliaryProcess::openDirectoryCacheInvalidated(SandboxExtension::Handle&& handle)
{
    // When Open Directory has invalidated the in-process cache for the results of getpwnam/getpwuid_r,
    // we need to rebuild the cache by getting the home directory while holding a temporary sandbox
    // extension to the associated Open Directory service.

    auto sandboxExtension = SandboxExtension::create(WTFMove(handle));
    if (!sandboxExtension)
        return;

    sandboxExtension->consume();

    getHomeDirectory();

    sandboxExtension->revoke();
}
#endif // PLATFORM(MAC)

} // namespace WebKit

#endif
