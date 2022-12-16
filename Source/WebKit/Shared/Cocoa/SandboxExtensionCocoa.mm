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

#import "ArgumentCodersCocoa.h"
#import "DataReference.h"
#import "Decoder.h"
#import "Encoder.h"
#import "Logging.h"
#import "WebCoreArgumentCoders.h"
#import <string.h>
#import <wtf/FileSystem.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/text/CString.h>

namespace WebKit {

class SandboxExtensionImpl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<SandboxExtensionImpl> create(const char* path, SandboxExtension::Type type, std::optional<audit_token_t> auditToken = std::nullopt, OptionSet<SandboxExtension::Flags> flags = SandboxExtension::Flags::Default)
    {
        std::unique_ptr<SandboxExtensionImpl> impl { new SandboxExtensionImpl(path, type, auditToken, flags) };
        if (!impl->m_token)
            return nullptr;
        return impl;
    }

    SandboxExtensionImpl(const char* serializedFormat, size_t length)
        : m_token { strndup(serializedFormat, length) }
    {
    }

    ~SandboxExtensionImpl()
    {
        if (!m_token)
            return;
        auto length = strlen(m_token);
        memset_s(m_token, length, 0, length);
        free(m_token);
    }

    bool WARN_UNUSED_RETURN consume()
    {
        m_handle = sandbox_extension_consume(m_token);
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        return !sandbox_check(getpid(), 0, SANDBOX_FILTER_NONE);
#else
        if (m_handle == -1) {
            LOG_ERROR("Could not create a sandbox extension for '%s', errno = %d", m_token, errno);
            return false;
        }
        return true;
#endif
    }

    bool invalidate()
    {
        return !sandbox_extension_release(std::exchange(m_handle, 0));
    }

    const char* WARN_UNUSED_RETURN getSerializedFormat(size_t& length)
    {
        length = strlen(m_token);
        return m_token;
    }

private:
    char* sandboxExtensionForType(const char* path, SandboxExtension::Type type, std::optional<audit_token_t> auditToken, OptionSet<SandboxExtension::Flags> flags)
    {
        uint32_t extensionFlags = 0;
        if (flags & SandboxExtension::Flags::NoReport)
            extensionFlags |= SANDBOX_EXTENSION_NO_REPORT;
        if (flags & SandboxExtension::Flags::DoNotCanonicalize)
            extensionFlags |= SANDBOX_EXTENSION_CANONICAL;

        switch (type) {
        case SandboxExtension::Type::ReadOnly:
            return sandbox_extension_issue_file(APP_SANDBOX_READ, path, extensionFlags);
        case SandboxExtension::Type::ReadWrite:
            return sandbox_extension_issue_file(APP_SANDBOX_READ_WRITE, path, extensionFlags);
        case SandboxExtension::Type::Mach:
            if (!auditToken)
                return sandbox_extension_issue_mach("com.apple.webkit.extension.mach", path, extensionFlags);
            return sandbox_extension_issue_mach_to_process("com.apple.webkit.extension.mach", path, extensionFlags, *auditToken);
        case SandboxExtension::Type::IOKit:
            if (!auditToken)
                return sandbox_extension_issue_iokit_registry_entry_class("com.apple.webkit.extension.iokit", path, extensionFlags);
            return sandbox_extension_issue_iokit_registry_entry_class_to_process("com.apple.webkit.extension.iokit", path, extensionFlags, *auditToken);
        case SandboxExtension::Type::Generic:
            return sandbox_extension_issue_generic(path, extensionFlags);
        case SandboxExtension::Type::ReadByProcess:
            if (!auditToken)
                return nullptr;
#if PLATFORM(MAC)
            extensionFlags |= SANDBOX_EXTENSION_USER_INTENT;
#endif
            return sandbox_extension_issue_file_to_process(APP_SANDBOX_READ, path, extensionFlags, *auditToken);
        }
    }

    SandboxExtensionImpl(const char* path, SandboxExtension::Type type, std::optional<audit_token_t> auditToken, OptionSet<SandboxExtension::Flags> flags)
        : m_token { sandboxExtensionForType(path, type, auditToken, flags) }
    {
    }

    char* m_token;
    int64_t m_handle { 0 };
};

SandboxExtension::Handle::Handle()
{
}

SandboxExtension::Handle::Handle(Handle&&) = default;
SandboxExtension::Handle& SandboxExtension::Handle::operator=(Handle&&) = default;

SandboxExtension::Handle::~Handle()
{
    if (m_sandboxExtension)
        m_sandboxExtension->invalidate();
}

void SandboxExtension::Handle::encode(IPC::Encoder& encoder) const
{
    if (!m_sandboxExtension) {
        encoder << IPC::DataReference();
        return;
    }

    size_t length = 0;
    const char* serializedFormat = m_sandboxExtension->getSerializedFormat(length);
    ASSERT(serializedFormat);

    encoder << IPC::DataReference(reinterpret_cast<const uint8_t*>(serializedFormat), length);

    // Encoding will destroy the sandbox extension locally.
    m_sandboxExtension = 0;
}

auto SandboxExtension::Handle::decode(IPC::Decoder& decoder) -> std::optional<Handle>
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return std::nullopt;

    if (dataReference.empty())
        return {{ }};

    Handle handle;
    handle.m_sandboxExtension = makeUnique<SandboxExtensionImpl>(reinterpret_cast<const char*>(dataReference.data()), dataReference.size());
    return WTFMove(handle);
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
        LOG_ERROR("Could not create a valid file system representation for the string '%s' of length %lu", resolvedPath.utf8().data(), resolvedPath.length());
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
        LOG_ERROR("Could not create a sandbox extension for '%s'", path.utf8().data());
        return std::nullopt;
    }
    return WTFMove(handle);
}

auto SandboxExtension::createHandle(StringView path, Type type) -> std::optional<Handle>
{
    return createHandleWithoutResolvingPath(resolvePathForSandboxExtension(path), type);
}

template<typename Collection, typename Function> static Vector<SandboxExtension::Handle> createHandlesForResources(const Collection& resources, const Function& createFunction)
{
    Vector<SandboxExtension::Handle> handleArray;
    handleArray.reserveInitialCapacity(std::size(resources));
    for (const auto& resource : resources) {
        if (auto handle = createFunction(resource))
            handleArray.uncheckedAppend(WTFMove(*handle));
    }
    return handleArray;
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
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, path.data(), path.size()))
        return std::nullopt;
    
    // Shrink the vector.   
    path.shrink(strlen(path.data()));

    // FIXME: Change to a runtime assertion that the path ends with a slash once <rdar://problem/23579077> is
    // fixed in all iOS Simulator versions that we use.
    if (path.last() != '/')
        path.append('/');
    
    // Append the file name.
    auto prefixAsUTF8 = prefix.utf8();
    path.append(prefixAsUTF8.data(), prefixAsUTF8.length());
    path.append('\0');

    auto pathString = String::fromUTF8(path.data());
    if (pathString.isNull())
        return std::nullopt;
    
    handle.m_sandboxExtension = SandboxExtensionImpl::create(FileSystem::fileSystemRepresentation(pathString).data(), type);

    if (!handle.m_sandboxExtension) {
        WTFLogAlways("Could not create a sandbox extension for temporary file '%s'", path.data());
        return std::nullopt;
    }
    return { { WTFMove(handle), String::fromUTF8(path.data()) } };
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

auto SandboxExtension::createHandlesForMachLookup(Span<const ASCIILiteral> services, std::optional<audit_token_t> auditToken, MachBootstrapOptions machBootstrapOptions, OptionSet<Flags> flags) -> Vector<Handle>
{
    auto handles = createHandlesForResources(services, [auditToken, flags] (ASCIILiteral service) -> std::optional<Handle> {
        auto handle = createHandleForMachLookup(service, auditToken, flags);
        ASSERT(handle);
        return handle;
    });

    if (machBootstrapOptions == MachBootstrapOptions::EnableMachBootstrap)
        handles.append(createHandleForMachBootstrapExtension());

    return handles;
}

auto SandboxExtension::createHandlesForMachLookup(std::initializer_list<const ASCIILiteral> services, std::optional<audit_token_t> auditToken, MachBootstrapOptions machBootstrapOptions, OptionSet<Flags> flags) -> Vector<Handle>
{
    return createHandlesForMachLookup(Span { services.begin(), services.size() }, auditToken, machBootstrapOptions, flags);
}

auto SandboxExtension::createHandleForReadByAuditToken(StringView path, audit_token_t auditToken) -> std::optional<Handle>
{
    Handle handle;
    ASSERT(!handle.m_sandboxExtension);

    handle.m_sandboxExtension = SandboxExtensionImpl::create(path.utf8().data(), Type::ReadByProcess, auditToken);
    if (!handle.m_sandboxExtension) {
        LOG_ERROR("Could not create a sandbox extension for '%s'", path.utf8().data());
        return std::nullopt;
    }
    
    return WTFMove(handle);
}

auto SandboxExtension::createHandleForIOKitClassExtension(ASCIILiteral ioKitClass, std::optional<audit_token_t> auditToken, OptionSet<Flags> flags) -> std::optional<Handle>
{
    Handle handle;
    ASSERT(!handle.m_sandboxExtension);

    handle.m_sandboxExtension = SandboxExtensionImpl::create(ioKitClass.characters(), Type::IOKit, auditToken);
    if (!handle.m_sandboxExtension) {
        LOG_ERROR("Could not create a sandbox extension for '%s'", ioKitClass.characters());
        return std::nullopt;
    }

    return WTFMove(handle);
}

auto SandboxExtension::createHandlesForIOKitClassExtensions(Span<const ASCIILiteral> iokitClasses, std::optional<audit_token_t> auditToken, OptionSet<Flags> flags) -> Vector<Handle>
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
