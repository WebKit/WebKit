/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#import "SandboxExtension.h"

#if ENABLE(SANDBOX_EXTENSIONS)

#import "Logging.h"
#import <string.h>
#import <wtf/FileSystem.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/text/CString.h>

namespace WebKit {

std::unique_ptr<SandboxExtensionImpl> SandboxExtensionImpl::create(const char* path, SandboxExtension::Type type, std::optional<audit_token_t> auditToken, OptionSet<SandboxExtension::Flags> flags)
{
    std::unique_ptr<SandboxExtensionImpl> impl { new SandboxExtensionImpl(path, type, auditToken, flags) };
    if (!impl->m_token.length())
        return nullptr;
    return impl;
}

SandboxExtensionImpl::SandboxExtensionImpl(std::span<const uint8_t> serializedFormat)
    : m_token { byteCast<Latin1Character>(serializedFormat) }
{
    ASSERT(!serializedFormat.empty());
}

SandboxExtensionImpl::~SandboxExtensionImpl()
{
    if (!m_token.isNull())
        secureMemsetSpan(m_token.mutableSpan(), 0);
}

bool WARN_UNUSED_RETURN SandboxExtensionImpl::consume()
{
    m_handle = sandbox_extension_consume(m_token.data());
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    return !sandbox_check(getpid(), 0, SANDBOX_FILTER_NONE);
#else
    if (m_handle == -1) {
        RELEASE_LOG_ERROR(Sandbox, "Could not create a sandbox extension for '%s', errno = %d", m_token.data(), errno);
        return false;
    }
    return true;
#endif
}

bool SandboxExtensionImpl::invalidate()
{
    return !sandbox_extension_release(std::exchange(m_handle, 0));
}

std::span<const uint8_t> SandboxExtensionImpl::getSerializedFormat()
{
    ASSERT(m_token.length());
    return byteCast<uint8_t>(m_token.span());
}

CString SandboxExtensionImpl::sandboxExtensionForType(const char* path, SandboxExtension::Type type, std::optional<audit_token_t> auditToken, OptionSet<SandboxExtension::Flags> flags)
{
    auto sandboxExtension = [&] {
        uint32_t extensionFlags = 0;
        if (flags & SandboxExtension::Flags::NoReport)
            extensionFlags |= SANDBOX_EXTENSION_NO_REPORT;
        if (flags & SandboxExtension::Flags::DoNotCanonicalize)
            extensionFlags |= SANDBOX_EXTENSION_CANONICAL;

        switch (type) {
        case SandboxExtension::Type::ReadOnly:
            return std::unique_ptr<char, decltype(free)*>(sandbox_extension_issue_file(APP_SANDBOX_READ, path, extensionFlags), free);
        case SandboxExtension::Type::ReadWrite:
            return std::unique_ptr<char, decltype(free)*>(sandbox_extension_issue_file(APP_SANDBOX_READ_WRITE, path, extensionFlags), free);
        case SandboxExtension::Type::Mach:
            if (!auditToken)
                return std::unique_ptr<char, decltype(free)*>(sandbox_extension_issue_mach("com.apple.webkit.extension.mach", path, extensionFlags), free);
            return std::unique_ptr<char, decltype(free)*>(sandbox_extension_issue_mach_to_process("com.apple.webkit.extension.mach", path, extensionFlags, *auditToken), free);
        case SandboxExtension::Type::IOKit:
            if (!auditToken)
                return std::unique_ptr<char, decltype(free)*>(sandbox_extension_issue_iokit_registry_entry_class("com.apple.webkit.extension.iokit", path, extensionFlags), free);
            return std::unique_ptr<char, decltype(free)*>(sandbox_extension_issue_iokit_registry_entry_class_to_process("com.apple.webkit.extension.iokit", path, extensionFlags, *auditToken), free);
        case SandboxExtension::Type::Generic:
            return std::unique_ptr<char, decltype(free)*>(sandbox_extension_issue_generic(path, extensionFlags), free);
        case SandboxExtension::Type::ReadByProcess:
            if (!auditToken)
                return std::unique_ptr<char, decltype(free)*>(nullptr, free);
#if PLATFORM(MAC)
            extensionFlags |= SANDBOX_EXTENSION_USER_INTENT;
#endif
            return std::unique_ptr<char, decltype(free)*>(sandbox_extension_issue_file_to_process(APP_SANDBOX_READ, path, extensionFlags, *auditToken), free);
        }
    }();

    return CString(sandboxExtension.get());
}

SandboxExtensionImpl::SandboxExtensionImpl(const char* path, SandboxExtension::Type type, std::optional<audit_token_t> auditToken, OptionSet<SandboxExtension::Flags> flags)
    : m_token { sandboxExtensionForType(path, type, auditToken, flags) }
{
}

SandboxExtensionHandle::SandboxExtensionHandle()
{
}

SandboxExtensionHandle::SandboxExtensionHandle(const SandboxExtensionHandle& handle)
{
    m_sandboxExtension = WTF::makeUnique<SandboxExtensionImpl>(handle.m_sandboxExtension->getSerializedFormat());
}

SandboxExtensionHandle::SandboxExtensionHandle(SandboxExtensionHandle&&) = default;
SandboxExtensionHandle& SandboxExtensionHandle::operator=(SandboxExtensionHandle&&) = default;

SandboxExtensionHandle::~SandboxExtensionHandle()
{
    if (m_sandboxExtension)
        m_sandboxExtension->invalidate();
}

RefPtr<SandboxExtension> SandboxExtension::create(Handle&& handle)
{
    if (!handle.m_sandboxExtension)
        return nullptr;

    return adoptRef(new SandboxExtension(handle));
}

String stringByResolvingSymlinksInPath(StringView path)
{
    char resolvedPath[PATH_MAX] = { 0 };
    realpath(path.utf8().data(), resolvedPath);
    return String::fromUTF8(resolvedPath);
}

String resolveAndCreateReadWriteDirectoryForSandboxExtension(StringView path)
{
    NSError *error = nil;
    auto nsPath = path.createNSStringWithoutCopying();

    if (![[NSFileManager defaultManager] createDirectoryAtPath:nsPath.get() withIntermediateDirectories:YES attributes:nil error:&error]) {
        NSLog(@"could not create directory \"%@\" for future sandbox extension, error %@", nsPath.get(), error);
        return { };
    }

    return resolvePathForSandboxExtension(path);
}

String resolvePathForSandboxExtension(StringView path)
{
    String resolvedPath = stringByResolvingSymlinksInPath(path);
    if (resolvedPath.isNull()) {
        RELEASE_LOG_ERROR(Sandbox, "Could not create a valid file system representation for the string '%s' of length %u", resolvedPath.utf8().data(), resolvedPath.length());
        return { };
    }

    return resolvedPath;
}

auto SandboxExtension::createHandleWithoutResolvingPath(StringView path, Type type) -> std::optional<Handle>
{
    Handle handle;
    ASSERT(!handle.m_sandboxExtension);

    handle.m_sandboxExtension = SandboxExtensionImpl::create(path.utf8().data(), type, std::nullopt, Flags::DoNotCanonicalize);
    if (!handle.m_sandboxExtension) {
        RELEASE_LOG_ERROR(Sandbox, "Could not create a sandbox extension for '%{private}s'", path.utf8().data());
        return std::nullopt;
    }

    RELEASE_LOG(Sandbox, "Successfully created a sandbox extension for '%{private}s'", path.utf8().data());
    return WTFMove(handle);
}

auto SandboxExtension::createHandle(StringView path, Type type) -> std::optional<Handle>
{
    return createHandleWithoutResolvingPath(resolvePathForSandboxExtension(path), type);
}

template<typename Collection, typename Function> static Vector<SandboxExtension::Handle> createHandlesForResources(const Collection& resources, const Function& createFunction)
{
    return WTF::compactMap(resources, [&](auto& resource) -> std::optional<SandboxExtension::Handle> {
        if (auto handle = createFunction(resource))
            return WTFMove(*handle);
        return std::nullopt;
    });
}

auto SandboxExtension::createReadOnlyHandlesForFiles(ASCIILiteral logLabel, const Vector<String>& paths) -> Vector<Handle>
{
    return createHandlesForResources(paths, [&logLabel] (const String& path) {
        auto handle = createHandle(path, Type::ReadOnly);
        if (!handle) {
            // This can legitimately fail if a directory containing the file is deleted after the file was chosen.
            // We also have reports of cases where this likely fails for some unknown reason, <rdar://problem/10156710>.
            WTFLogAlways("%s: could not create a sandbox extension for '%s'\n", logLabel.characters(), path.utf8().data());
            ASSERT_NOT_REACHED();
        }
        return handle;
    });
}

auto SandboxExtension::createHandleForReadWriteDirectory(StringView path) -> std::optional<Handle>
{
    String resolvedPath = resolveAndCreateReadWriteDirectoryForSandboxExtension(path);
    if (resolvedPath.isNull())
        return std::nullopt;
    return createHandleWithoutResolvingPath(resolvedPath, Type::ReadWrite);
}

auto SandboxExtension::createHandleForTemporaryFile(StringView prefix, Type type) -> std::optional<std::pair<Handle, String>>
{
    Handle handle;
    ASSERT(!handle.m_sandboxExtension);
    
    Vector<char> path(PATH_MAX);
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, path.mutableSpan().data(), path.size()))
        return std::nullopt;
    
    // Shrink the vector.   
    path.shrink(strlenSpan(path.span()));

    ASSERT(path.last() == '/');

    // Append the file name.
    path.append(prefix.utf8().span());

    auto pathString = String::fromUTF8(path.span());
    if (pathString.isNull())
        return std::nullopt;
    
    handle.m_sandboxExtension = SandboxExtensionImpl::create(FileSystem::fileSystemRepresentation(pathString).data(), type);

    if (!handle.m_sandboxExtension) {
        WTFLogAlways("Could not create a sandbox extension for temporary file '%s'", pathString.utf8().data());
        return std::nullopt;
    }
    return { { WTFMove(handle), WTFMove(pathString) } };
}

auto SandboxExtension::createHandleForGenericExtension(ASCIILiteral extensionClass) -> std::optional<Handle>
{
    Handle handle;
    ASSERT(!handle.m_sandboxExtension);

    handle.m_sandboxExtension = SandboxExtensionImpl::create(extensionClass.characters(), Type::Generic);
    if (!handle.m_sandboxExtension) {
        WTFLogAlways("Could not create a '%s' sandbox extension", extensionClass.characters());
        return std::nullopt;
    }
    
    return WTFMove(handle);
}

auto SandboxExtension::createHandleForMachBootstrapExtension() -> Handle
{
    auto handle = SandboxExtension::createHandleForGenericExtension("com.apple.webkit.mach-bootstrap"_s);
    if (handle)
        return WTFMove(*handle);
    return Handle();
}

auto SandboxExtension::createHandleForMachLookup(ASCIILiteral service, std::optional<audit_token_t> auditToken, OptionSet<Flags> flags) -> std::optional<Handle>
{
    Handle handle;
    ASSERT(!handle.m_sandboxExtension);
    
    handle.m_sandboxExtension = SandboxExtensionImpl::create(service.characters(), Type::Mach, auditToken, flags);
    if (!handle.m_sandboxExtension) {
        WTFLogAlways("Could not create a '%s' sandbox extension", service.characters());
        return std::nullopt;
    }
    
    return WTFMove(handle);
}

auto SandboxExtension::createHandlesForMachLookup(std::span<const ASCIILiteral> services, std::optional<audit_token_t> auditToken, MachBootstrapOptions machBootstrapOptions, OptionSet<Flags> flags) -> Vector<Handle>
{
    auto handles = createHandlesForResources(services, [auditToken, flags] (ASCIILiteral service) -> std::optional<Handle> {
        // Note that createHandleForMachLookup() may return null if the process has just crashed.
        return createHandleForMachLookup(service, auditToken, flags);
    });

#if HAVE(MACH_BOOTSTRAP_EXTENSION)
    if (machBootstrapOptions == MachBootstrapOptions::EnableMachBootstrap)
        handles.append(createHandleForMachBootstrapExtension());
#endif

    return handles;
}

auto SandboxExtension::createHandlesForMachLookup(std::initializer_list<const ASCIILiteral> services, std::optional<audit_token_t> auditToken, MachBootstrapOptions machBootstrapOptions, OptionSet<Flags> flags) -> Vector<Handle>
{
    return createHandlesForMachLookup(std::span { services }, auditToken, machBootstrapOptions, flags);
}

auto SandboxExtension::createHandleForReadByAuditToken(StringView path, audit_token_t auditToken) -> std::optional<Handle>
{
    Handle handle;
    ASSERT(!handle.m_sandboxExtension);

    handle.m_sandboxExtension = SandboxExtensionImpl::create(path.utf8().data(), Type::ReadByProcess, auditToken);
    if (!handle.m_sandboxExtension) {
        RELEASE_LOG_ERROR(Sandbox, "Could not create a sandbox extension for '%{private}s'", path.utf8().data());
        return std::nullopt;
    }
    
    RELEASE_LOG(Sandbox, "Successfully created sandbox extension for '%{private}s'", path.utf8().data());
    return WTFMove(handle);
}

auto SandboxExtension::createHandleForIOKitClassExtension(ASCIILiteral ioKitClass, std::optional<audit_token_t> auditToken, OptionSet<Flags> flags) -> std::optional<Handle>
{
    Handle handle;
    ASSERT(!handle.m_sandboxExtension);

    handle.m_sandboxExtension = SandboxExtensionImpl::create(ioKitClass.characters(), Type::IOKit, auditToken);
    if (!handle.m_sandboxExtension) {
        RELEASE_LOG_ERROR(Sandbox, "Could not create a sandbox extension for '%s'", ioKitClass.characters());
        return std::nullopt;
    }

    return WTFMove(handle);
}

auto SandboxExtension::createHandlesForIOKitClassExtensions(std::span<const ASCIILiteral> iokitClasses, std::optional<audit_token_t> auditToken, OptionSet<Flags> flags) -> Vector<Handle>
{
    return createHandlesForResources(iokitClasses, [auditToken, flags] (ASCIILiteral iokitClass) {
        auto handle = createHandleForIOKitClassExtension(iokitClass, auditToken, flags);
        ASSERT(handle);
        return handle;
    });
}

SandboxExtension::SandboxExtension(const Handle& handle)
    : m_sandboxExtension(WTFMove(handle.m_sandboxExtension))
{
}

SandboxExtension::~SandboxExtension()
{
    if (!m_sandboxExtension)
        return;

    ASSERT(!m_useCount);
}

bool SandboxExtension::revoke()
{
    ASSERT(m_sandboxExtension);
    ASSERT(m_useCount);
    
    if (--m_useCount)
        return true;

    return m_sandboxExtension->invalidate();
}

bool SandboxExtension::consume()
{
    ASSERT(m_sandboxExtension);

    if (m_useCount++)
        return true;

    return m_sandboxExtension->consume();
}

bool SandboxExtension::consumePermanently()
{
    ASSERT(m_sandboxExtension);

    bool result = m_sandboxExtension->consume();

    // Destroy the extension without invalidating it.
    m_sandboxExtension = nullptr;

    return result;
}

bool SandboxExtension::consumePermanently(const Handle& handle)
{
    if (!handle.m_sandboxExtension)
        return false;

    bool result = handle.m_sandboxExtension->consume();
    
    // Destroy the extension without invalidating it.
    handle.m_sandboxExtension = nullptr;

    return result;
}

bool SandboxExtension::consumePermanently(const Vector<Handle>& handleArray)
{
    bool allSucceeded = true;
    for (auto& handle : handleArray) {
        if (!handle.m_sandboxExtension)
            continue;

        bool ok = consumePermanently(handle);
        ASSERT(ok);
        allSucceeded &= ok;
    }

    return allSucceeded;
}

} // namespace WebKit

#endif // ENABLE(SANDBOX_EXTENSIONS)
