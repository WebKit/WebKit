/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) || PLATFORM(IOSMAC)
#import "ChildProcess.h"

#import "CodeSigning.h"
#import "QuarantineSPI.h"
#import "SandboxInitializationParameters.h"
#import "SandboxUtilities.h"
#import "WKFoundation.h"
#import "XPCServiceEntryPoint.h"
#import <WebCore/FileHandle.h>
#import <WebCore/FileSystem.h>
#import <WebCore/SystemVersion.h>
#import <mach-o/dyld.h>
#import <mach/mach.h>
#import <mach/task.h>
#import <pal/crypto/CryptoDigest.h>
#import <pwd.h>
#import <stdlib.h>
#import <sys/sysctl.h>
#import <sysexits.h>
#import <wtf/DataLog.h>
#import <wtf/RandomNumber.h>
#import <wtf/Scope.h>
#import <wtf/SystemTracing.h>
#import <wtf/WallTime.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/text/Base64.h>
#import <wtf/text/StringBuilder.h>

#if USE(APPLE_INTERNAL_SDK)
#import <HIServices/ProcessesPriv.h>
#import <rootless.h>
#endif

typedef bool (^LSServerConnectionAllowedBlock) ( CFDictionaryRef optionsRef );
extern "C" void _LSSetApplicationLaunchServicesServerConnectionStatus(uint64_t flags, LSServerConnectionAllowedBlock block);
extern "C" CFDictionaryRef _LSApplicationCheckIn(int sessionID, CFDictionaryRef applicationInfo);

extern "C" OSStatus SetApplicationIsDaemon(Boolean isDaemon);

using namespace WebCore;

namespace WebKit {

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

struct CachedSandboxHeader {
    uint32_t versionNumber;
    uint32_t libsandboxVersion;
    uint32_t headerSize;
    uint32_t builtinSize; // If a builtin doesn't exist, this is UINT_MAX.
    uint32_t dataSize;
};
// The file is layed out on disk like:
// byte 0
// CachedSandboxHeader <- sizeof(CachedSandboxHeader) bytes
// SandboxHeader <- CachedSandboxHeader::headerSize bytes
// [SandboxBuiltin] optional. Present if CachedSandboxHeader::builtinSize is not UINT_MAX. If present, builtinSize bytes (not including null termination).
// SandboxData <- CachedSandboxHeader::dataSize bytes
// byte N

struct SandboxInfo {
    SandboxInfo(const String& parentDirectoryPath, const String& directoryPath, const String& filePath, const String& profilePath, const SandboxParametersPtr& sandboxParameters, const CString& header, const ChildProcess::ProcessType& processType, const SandboxInitializationParameters& initializationParameters)
        : parentDirectoryPath { parentDirectoryPath }
        , directoryPath { directoryPath }
        , filePath { filePath }
        , profilePath { profilePath }
        , sandboxParameters { sandboxParameters }
        , header { header }
        , processType { processType }
        , initializationParameters { initializationParameters }
    {
    }
    
    const String& parentDirectoryPath;
    const String& directoryPath;
    const String& filePath;
    const String& profilePath;
    const SandboxParametersPtr& sandboxParameters;
    const CString& header;
    const ChildProcess::ProcessType& processType;
    const SandboxInitializationParameters& initializationParameters;
};

constexpr uint32_t CachedSandboxVersionNumber = 0;

static void initializeTimerCoalescingPolicy()
{
    // Set task_latency and task_throughput QOS tiers as appropriate for a visible application.
    struct task_qos_policy qosinfo = { LATENCY_QOS_TIER_0, THROUGHPUT_QOS_TIER_0 };
    kern_return_t kr = task_policy_set(mach_task_self(), TASK_BASE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
}

void ChildProcess::setApplicationIsDaemon()
{
#if !PLATFORM(IOSMAC)
    OSStatus error = SetApplicationIsDaemon(true);
    ASSERT_UNUSED(error, error == noErr);
#endif

    launchServicesCheckIn();
}

void ChildProcess::launchServicesCheckIn()
{
    _LSSetApplicationLaunchServicesServerConnectionStatus(0, 0);
    RetainPtr<CFDictionaryRef> unused = _LSApplicationCheckIn(-2, CFBundleGetInfoDictionary(CFBundleGetMainBundle()));
}

void ChildProcess::platformInitialize()
{
    initializeTimerCoalescingPolicy();
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[[NSBundle mainBundle] bundlePath]];
}

static OSStatus enableSandboxStyleFileQuarantine()
{
#if !PLATFORM(IOSMAC)
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

static std::optional<Vector<char>> fileContents(const String& path, bool shouldLock = false, OptionSet<FileSystem::FileLockMode> lockMode = FileSystem::FileLockMode::Exclusive)
{
    FileHandle file = shouldLock ? FileHandle(path, FileSystem::FileOpenMode::Read, lockMode) : FileHandle(path, FileSystem::FileOpenMode::Read);
    file.open();
    if (!file)
        return std::nullopt;
    
    char chunk[4096];
    constexpr size_t chunkSize = WTF_ARRAY_LENGTH(chunk);
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
constexpr const char* processStorageClass(ChildProcess::ProcessType type)
{
    switch (type) {
    case ChildProcess::ProcessType::WebContent:
        return "WebKitWebContentSandbox";
    case ChildProcess::ProcessType::Network:
        return "WebKitNetworkingSandbox";
    case ChildProcess::ProcessType::Storage:
        return "WebKitStorageSandbox";
    case ChildProcess::ProcessType::Plugin:
        return "WebKitPluginSandbox";
    }
}
#endif

static std::optional<CString> setAndSerializeSandboxParameters(const SandboxInitializationParameters& initializationParameters, const SandboxParametersPtr& sandboxParameters, const String& profilePath)
{
    StringBuilder builder;
    for (size_t i = 0; i < initializationParameters.count(); ++i) {
        CString name = initializationParameters.name(i);
        CString value = initializationParameters.value(i);
        if (name.isNull() || value.isNull())
            return std::nullopt;
        if (sandbox_set_param(sandboxParameters.get(), name.data(), value.data())) {
            WTFLogAlways("%s: Couldn't set sandbox parameter, errno: %d\n", getprogname(), errno);
            CRASH();
        }
        builder.append(name.data(), name.length());
        builder.append(':');
        builder.append(value.data(), value.length());
        builder.append(':');
    }
    auto contents = fileContents(profilePath);
    if (!contents)
        return std::nullopt;
    builder.append(contents->data(), contents->size());
    return builder.toString().ascii();
}
    
static size_t getUserCacheDirectory(Vector<char>& buffer)
{
    size_t result = confstr(_CS_DARWIN_USER_CACHE_DIR, buffer.data(), buffer.size());
    if (!result) {
        WTFLogAlways("%s: couldn't retrieve private cache directory path: %d\n", getprogname(), errno);
        exit(EX_NOPERM);
    }
    return result;
}

static inline String sandboxDataVaultParentDirectory()
{
    // DIRHELPER_USER_DIR_SUFFIX is set in initializeSandboxParameters (called before this function).
    char parentDirectory[PATH_MAX];
    
    Vector<char> temp;
    temp.grow(PATH_MAX);
    
    size_t neededBufferSize = getUserCacheDirectory(temp);
    if (neededBufferSize > temp.size()) {
        temp.grow(neededBufferSize);
        size_t bufferSize = getUserCacheDirectory(temp);
        RELEASE_ASSERT(bufferSize == temp.size());
    }
    if (!realpath(temp.data(), parentDirectory)) {
        WTFLogAlways("%s: couldn't retrieve private cache directory path: %d\n", getprogname(), errno);
        exit(EX_NOPERM);
    }
    
    return parentDirectory;
}

static inline String sandboxDirectory(ChildProcess::ProcessType processType, const String& parentDirectory)
{
    StringBuilder directory;
    directory.append(parentDirectory);
    switch (processType) {
    case ChildProcess::ProcessType::WebContent:
        directory.append("/com.apple.WebKit.WebContent.Sandbox");
        break;
    case ChildProcess::ProcessType::Network:
        directory.append("/com.apple.WebKit.Networking.Sandbox");
        break;
    case ChildProcess::ProcessType::Storage:
        directory.append("/com.apple.WebKit.Storage.Sandbox");
        break;
    case ChildProcess::ProcessType::Plugin:
        directory.append("/com.apple.WebKit.Plugin.Sandbox");
        break;
    }

#if !USE(APPLE_INTERNAL_SDK)
    // Add .OpenSource suffix so that open source builds don't try to access a data vault used by system Safari.
    directory.append(".OpenSource");
#endif

    return directory.toString();
}

static inline String sandboxFilePath(const String& directoryPath, const CString& header)
{
    StringBuilder sandboxFile;
    sandboxFile.append(directoryPath);
    sandboxFile.append("/CompiledSandbox+");

    // Make the filename semi-unique based on the contents of the header.
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(header.data(), header.length());
    Vector<uint8_t> hash = crypto->computeHash();
    String readableHash = WTF::base64URLEncode(hash.data(), hash.size());

    sandboxFile.append(readableHash);
    return sandboxFile.toString();
}

static bool ensureSandboxCacheDirectory(const SandboxInfo& info)
{
    if (!FileSystem::fileIsDirectory(info.parentDirectoryPath, FileSystem::ShouldFollowSymbolicLinks::Yes)) {
        FileSystem::makeAllDirectories(info.parentDirectoryPath);
        if (!FileSystem::fileIsDirectory(info.parentDirectoryPath, FileSystem::ShouldFollowSymbolicLinks::Yes)) {
            WTFLogAlways("%s: Couldn't create sandbox directory\n", getprogname());
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
        
        bool isDirectory = FileSystem::fileIsDirectory(info.directoryPath, FileSystem::ShouldFollowSymbolicLinks::No);
        if (isDirectory) {
            if (!FileSystem::deleteNonEmptyDirectory(info.directoryPath))
                return false;
        } else {
            if (!FileSystem::deleteFile(info.directoryPath))
                return false;
        }
        
        if (!makeDataVault())
            return false;
    } else {
        WTFLogAlways("Sandbox directory couldn't be created, errno: %d", errno);
        return false;
    }
#else
    bool hasSandboxDirectory = FileSystem::fileIsDirectory(info.directoryPath, FileSystem::ShouldFollowSymbolicLinks::Yes);
    if (!hasSandboxDirectory) {
        if (FileSystem::makeAllDirectories(info.directoryPath)) {
            ASSERT(FileSystem::fileIsDirectory(info.directoryPath, FileSystem::ShouldFollowSymbolicLinks::Yes));
            hasSandboxDirectory = true;
        } else {
            // We may have raced with someone else making it. That's ok.
            hasSandboxDirectory = FileSystem::fileIsDirectory(info.directoryPath, FileSystem::ShouldFollowSymbolicLinks::Yes);
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
    CString profilePath = FileSystem::fileSystemRepresentation(info.profilePath);
    if (profilePath.isNull())
        return nullptr;
    SandboxProfilePtr sandboxProfile { sandbox_compile_file(profilePath.data(), info.sandboxParameters.get(), &error) };
    if (!sandboxProfile) {
        WTFLogAlways("%s: Couldn't compile WebContent sandbox %s\n", getprogname(), error);
        return nullptr;
    }
    
    const bool haveBuiltin = sandboxProfile->builtin;
    int32_t libsandboxVersion = NSVersionOfRunTimeLibrary("sandbox");
    RELEASE_ASSERT(libsandboxVersion > 0);
    CachedSandboxHeader cachedHeader {
        CachedSandboxVersionNumber,
        static_cast<uint32_t>(libsandboxVersion),
        safeCast<uint32_t>(info.header.length()),
        haveBuiltin ? safeCast<uint32_t>(strlen(sandboxProfile->builtin)) : std::numeric_limits<uint32_t>::max(),
        safeCast<uint32_t>(sandboxProfile->size)
    };
    const size_t expectedFileSize = sizeof(cachedHeader) + cachedHeader.headerSize + (haveBuiltin ? cachedHeader.builtinSize : 0) + cachedHeader.dataSize;
    
    Vector<char> cacheFile;
    cacheFile.reserveInitialCapacity(expectedFileSize);
    cacheFile.append(bitwise_cast<uint8_t*>(&cachedHeader), sizeof(CachedSandboxHeader));
    cacheFile.append(info.header.data(), info.header.length());
    if (haveBuiltin)
        cacheFile.append(sandboxProfile->builtin, cachedHeader.builtinSize);
    cacheFile.append(sandboxProfile->data, cachedHeader.dataSize);

    if (!writeSandboxDataToCacheFile(info, cacheFile))
        WTFLogAlways("%s: Unable to cache compiled sandbox", getprogname());

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
    if (static_cast<uint32_t>(libsandboxVersion) != cachedSandboxHeader.libsandboxVersion)
        return false;
    if (cachedSandboxHeader.versionNumber != CachedSandboxVersionNumber)
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

    SandboxProfile profile;
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

    if (sandbox_apply(&profile)) {
        WTFLogAlways("%s: Could not apply cached sandbox\n", getprogname());
        return false;
    }

    return true;
}

static inline const NSBundle *webKit2Bundle()
{
#if WK_API_ENABLED
    const static NSBundle *bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
#else
    const static NSBundle *bundle = [NSBundle bundleForClass:NSClassFromString(@"WKView")];
#endif

    return bundle;
}

static inline String sandboxProfilePath(const SandboxInitializationParameters& parameters)
{
    switch (parameters.mode()) {
    case SandboxInitializationParameters::ProfileSelectionMode::UseDefaultSandboxProfilePath:
        return [webKit2Bundle() pathForResource:[[NSBundle mainBundle] bundleIdentifier] ofType:@"sb"];
    case SandboxInitializationParameters::ProfileSelectionMode::UseOverrideSandboxProfilePath:
        return parameters.overrideSandboxProfilePath();
    case SandboxInitializationParameters::ProfileSelectionMode::UseSandboxProfile:
        return parameters.sandboxProfile();
    }
}

static bool compileAndApplySandboxSlowCase(const SandboxInfo& info)
{
    char* errorBuf;
    CString profilePath = FileSystem::fileSystemRepresentation(info.profilePath);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (sandbox_init_with_parameters(profilePath.data(), SANDBOX_NAMED_EXTERNAL, info.initializationParameters.namedParameterArray(), &errorBuf)) {
#pragma clang diagnostic pop
        WTFLogAlways("%s: Couldn't initialize sandbox profile [%s], error '%s'\n", getprogname(), profilePath.data(), errorBuf);
        for (size_t i = 0, count = info.initializationParameters.count(); i != count; ++i)
            WTFLogAlways("%s=%s\n", info.initializationParameters.name(i), info.initializationParameters.value(i));
        return false;
    }
    return true;
}

static bool applySandbox(const ChildProcessInitializationParameters& parameters, const SandboxInitializationParameters& sandboxInitializationParameters)
{
    String profilePath { sandboxProfilePath(sandboxInitializationParameters) };
    if (profilePath.isEmpty()) {
        WTFLogAlways("%s: Profile path is invalid\n", getprogname());
        return false;
    }

    SandboxParametersPtr sandboxParameters { sandbox_create_params() };
    auto header = setAndSerializeSandboxParameters(sandboxInitializationParameters, sandboxParameters, profilePath);
    if (!header) {
        WTFLogAlways("%s: Sandbox parameters are invalid\n", getprogname());
        CRASH();
    }

    String parentDirectoryPath { sandboxDataVaultParentDirectory() };
    String directoryPath { sandboxDirectory(parameters.processType, parentDirectoryPath) };
    String filePath = sandboxFilePath(directoryPath, *header);
    SandboxInfo info {
        parentDirectoryPath,
        directoryPath,
        filePath,
        profilePath,
        sandboxParameters,
        *header,
        parameters.processType,
        sandboxInitializationParameters
    };

    if (tryApplyCachedSandbox(info))
        return true;

    SandboxProfilePtr sandboxProfile = compileAndCacheSandboxProfile(info);
    if (!sandboxProfile)
        return compileAndApplySandboxSlowCase(info);

    if (sandbox_apply(sandboxProfile.get())) {
        WTFLogAlways("%s: Couldn't apply compiled sandbox profile, errno: %d\n", getprogname(), errno);
        exit(EX_NOPERM);
    }

    return true;
}

static void initializeSandboxParameters(const ChildProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
    // Verify user directory suffix.
    if (sandboxParameters.userDirectorySuffix().isNull()) {
        auto userDirectorySuffix = parameters.extraInitializationData.find("user-directory-suffix");
        if (userDirectorySuffix != parameters.extraInitializationData.end())
            sandboxParameters.setUserDirectorySuffix([makeString(userDirectorySuffix->value, '/', String([[NSBundle mainBundle] bundleIdentifier])) fileSystemRepresentation]);
        else {
            String clientIdentifier = codeSigningIdentifier(parameters.connectionIdentifier.xpcConnection.get());
            if (clientIdentifier.isNull())
                clientIdentifier = parameters.clientIdentifier;
            String defaultUserDirectorySuffix = makeString(String([[NSBundle mainBundle] bundleIdentifier]), '+', clientIdentifier);
            sandboxParameters.setUserDirectorySuffix(defaultUserDirectorySuffix);
        }
    }

    Vector<String> osVersionParts;
    String osSystemMarketingVersion = systemMarketingVersion();
    osSystemMarketingVersion.split('.', false, osVersionParts);
    if (osVersionParts.size() < 2) {
        WTFLogAlways("%s: Couldn't find OS Version\n", getprogname());
        exit(EX_NOPERM);
    }
    String osVersion = osVersionParts[0] + '.' + osVersionParts[1];
    sandboxParameters.addParameter("_OS_VERSION", osVersion.utf8().data());

    // Use private temporary and cache directories.
    setenv("DIRHELPER_USER_DIR_SUFFIX", FileSystem::fileSystemRepresentation(sandboxParameters.userDirectorySuffix()).data(), 1);
    char temporaryDirectory[PATH_MAX];
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory))) {
        WTFLogAlways("%s: couldn't retrieve private temporary directory path: %d\n", getprogname(), errno);
        exit(EX_NOPERM);
    }
    setenv("TMPDIR", temporaryDirectory, 1);

    sandboxParameters.addPathParameter("WEBKIT2_FRAMEWORK_DIR", [[webKit2Bundle() bundlePath] stringByDeletingLastPathComponent]);
    sandboxParameters.addConfDirectoryParameter("DARWIN_USER_TEMP_DIR", _CS_DARWIN_USER_TEMP_DIR);
    sandboxParameters.addConfDirectoryParameter("DARWIN_USER_CACHE_DIR", _CS_DARWIN_USER_CACHE_DIR);

    char buffer[4096];
    int bufferSize = sizeof(buffer);
    struct passwd pwd;
    struct passwd* result = 0;
    if (getpwuid_r(getuid(), &pwd, buffer, bufferSize, &result) || !result) {
        WTFLogAlways("%s: Couldn't find home directory\n", getprogname());
        exit(EX_NOPERM);
    }

    sandboxParameters.addPathParameter("HOME_DIR", pwd.pw_dir);
    String path = String::fromUTF8(pwd.pw_dir);
    path.append("/Library");
    sandboxParameters.addPathParameter("HOME_LIBRARY_DIR", FileSystem::fileSystemRepresentation(path).data());
    path.append("/Preferences");
    sandboxParameters.addPathParameter("HOME_LIBRARY_PREFERENCES_DIR", FileSystem::fileSystemRepresentation(path).data());
}

void ChildProcess::initializeSandbox(const ChildProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
    TraceScope traceScope(InitializeSandboxStart, InitializeSandboxEnd);

    initializeSandboxParameters(parameters, sandboxParameters);

    if (!applySandbox(parameters, sandboxParameters)) {
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

#if USE(APPKIT)
void ChildProcess::stopNSAppRunLoop()
{
    ASSERT([NSApp isRunning]);
    [NSApp stop:nil];

    NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSMakePoint(0, 0) modifierFlags:0 timestamp:0.0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    [NSApp postEvent:event atStart:true];
}
#endif

#if !PLATFORM(IOSMAC) && ENABLE(WEBPROCESS_NSRUNLOOP)
void ChildProcess::stopNSRunLoop()
{
    ASSERT([NSRunLoop mainRunLoop]);
    [[NSRunLoop mainRunLoop] performBlock:^{
        exit(0);
    }];
}
#endif

#if PLATFORM(IOSMAC)
void ChildProcess::platformStopRunLoop()
{
    XPCServiceExit(WTFMove(m_priorityBoostMessage));
}
#endif

void ChildProcess::setQOS(int latencyQOS, int throughputQOS)
{
    if (!latencyQOS && !throughputQOS)
        return;

    struct task_qos_policy qosinfo = {
        latencyQOS ? LATENCY_QOS_TIER_0 + latencyQOS - 1 : LATENCY_QOS_TIER_UNSPECIFIED,
        throughputQOS ? THROUGHPUT_QOS_TIER_0 + throughputQOS - 1 : THROUGHPUT_QOS_TIER_UNSPECIFIED
    };

    task_policy_set(mach_task_self(), TASK_OVERRIDE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
}

} // namespace WebKit

#endif
