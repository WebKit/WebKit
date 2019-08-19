/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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

#import "DataReference.h"
#import "Decoder.h"
#import "Encoder.h"
#import <wtf/FileSystem.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/text/CString.h>

namespace WebKit {

class SandboxExtensionImpl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<SandboxExtensionImpl> create(const char* path, SandboxExtension::Type type, Optional<pid_t> pid = WTF::nullopt)
    {
        std::unique_ptr<SandboxExtensionImpl> impl { new SandboxExtensionImpl(path, type, pid) };
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
        return m_handle;
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
    char* sandboxExtensionForType(const char* path, SandboxExtension::Type type, Optional<pid_t> pid = WTF::nullopt)
    {
        switch (type) {
        case SandboxExtension::Type::ReadOnly:
            return sandbox_extension_issue_file(APP_SANDBOX_READ, path, 0);
        case SandboxExtension::Type::ReadWrite:
            return sandbox_extension_issue_file(APP_SANDBOX_READ_WRITE, path, 0);
        case SandboxExtension::Type::Mach:
#if HAVE(SANDBOX_ISSUE_MACH_EXTENSION_TO_PROCESS_BY_PID)
            return sandbox_extension_issue_mach_to_process_by_pid("com.apple.webkit.extension.mach"_s, path, 0, pid.value());
#else
            UNUSED_PARAM(pid);
            ASSERT_NOT_REACHED();
            return nullptr;
#endif
        case SandboxExtension::Type::Generic:
            return sandbox_extension_issue_generic(path, 0);
        case SandboxExtension::Type::ReadByPid:
#if HAVE(SANDBOX_ISSUE_READ_EXTENSION_TO_PROCESS_BY_PID)
            return sandbox_extension_issue_file_to_process_by_pid(APP_SANDBOX_READ, path, SANDBOX_EXTENSION_USER_INTENT, pid.value());
#else
            UNUSED_PARAM(pid);
            ASSERT_NOT_REACHED();
            return nullptr;
#endif
        }
    }

    SandboxExtensionImpl(const char* path, SandboxExtension::Type type, Optional<pid_t> pid = WTF::nullopt)
        : m_token { sandboxExtensionForType(path, type, pid) }
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

auto SandboxExtension::Handle::decode(IPC::Decoder& decoder) -> Optional<Handle>
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return WTF::nullopt;

    if (dataReference.isEmpty())
        return {{ }};

    Handle handle;
    handle.m_sandboxExtension = makeUnique<SandboxExtensionImpl>(reinterpret_cast<const char*>(dataReference.data()), dataReference.size());
    return WTFMove(handle);
}

SandboxExtension::HandleArray::HandleArray()
{
}

SandboxExtension::HandleArray::~HandleArray()
{
}

void SandboxExtension::HandleArray::allocate(size_t size)
{
    if (!size)
        return;

    ASSERT(m_data.isEmpty());

    m_data.resize(size);
}

SandboxExtension::Handle& SandboxExtension::HandleArray::operator[](size_t i)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(i < m_data.size());
    return m_data[i];
}

const SandboxExtension::Handle& SandboxExtension::HandleArray::operator[](size_t i) const
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(i < m_data.size());
    return m_data[i];
}

size_t SandboxExtension::HandleArray::size() const
{
    return m_data.size();
}

void SandboxExtension::HandleArray::encode(IPC::Encoder& encoder) const
{
    encoder << static_cast<uint64_t>(size());
    for (auto& handle : m_data)
        encoder << handle;
}

Optional<SandboxExtension::HandleArray> SandboxExtension::HandleArray::decode(IPC::Decoder& decoder)
{
    Optional<uint64_t> size;
    decoder >> size;
    if (!size)
        return WTF::nullopt;
    SandboxExtension::HandleArray handles;
    handles.allocate(*size);
    for (size_t i = 0; i < *size; ++i) {
        Optional<SandboxExtension::Handle> handle;
        decoder >> handle;
        if (!handle)
            return WTF::nullopt;
        handles[i] = WTFMove(*handle);
    }
    return WTFMove(handles);
}

RefPtr<SandboxExtension> SandboxExtension::create(Handle&& handle)
{
    if (!handle.m_sandboxExtension)
        return nullptr;

    return adoptRef(new SandboxExtension(handle));
}

String stringByResolvingSymlinksInPath(const String& path)
{
    return [(NSString *)path stringByResolvingSymlinksInPath];
}

String resolveAndCreateReadWriteDirectoryForSandboxExtension(const String& path)
{
    NSError *error = nil;
    NSString *nsPath = path;

    if (![[NSFileManager defaultManager] createDirectoryAtPath:nsPath withIntermediateDirectories:YES attributes:nil error:&error]) {
        NSLog(@"could not create directory \"%@\" for future sandbox extension, error %@", nsPath, error);
        return { };
    }

    return resolvePathForSandboxExtension(path);
}

String resolvePathForSandboxExtension(const String& path)
{
    String resolvedPath = stringByResolvingSymlinksInPath(path);
    if (resolvedPath.isNull()) {
        LOG_ERROR("Could not create a valid file system representation for the string '%s' of length %lu", resolvedPath.utf8().data(), resolvedPath.length());
        return { };
    }

    return resolvedPath;
}

bool SandboxExtension::createHandleWithoutResolvingPath(const String& path, Type type, Handle& handle)
{
    ASSERT(!handle.m_sandboxExtension);

    handle.m_sandboxExtension = SandboxExtensionImpl::create(path.utf8().data(), type);
    if (!handle.m_sandboxExtension) {
        LOG_ERROR("Could not create a sandbox extension for '%s'", path.utf8().data());
        return false;
    }
    return true;
}

bool SandboxExtension::createHandle(const String& path, Type type, Handle& handle)
{
    ASSERT(!handle.m_sandboxExtension);

    return createHandleWithoutResolvingPath(resolvePathForSandboxExtension(path), type, handle);
}

bool SandboxExtension::createHandleForReadWriteDirectory(const String& path, SandboxExtension::Handle& handle)
{
    String resolvedPath = resolveAndCreateReadWriteDirectoryForSandboxExtension(path);
    if (resolvedPath.isNull())
        return false;

    return SandboxExtension::createHandleWithoutResolvingPath(resolvedPath, SandboxExtension::Type::ReadWrite, handle);
}

String SandboxExtension::createHandleForTemporaryFile(const String& prefix, Type type, Handle& handle)
{
    ASSERT(!handle.m_sandboxExtension);
    
    Vector<char> path(PATH_MAX);
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, path.data(), path.size()))
        return String();
    
    // Shrink the vector.   
    path.shrink(strlen(path.data()));

    // FIXME: Change to a runtime assertion that the path ends with a slash once <rdar://problem/23579077> is
    // fixed in all iOS Simulator versions that we use.
    if (path.last() != '/')
        path.append('/');
    
    // Append the file name.    
    path.append(prefix.utf8().data(), prefix.length());
    path.append('\0');
    
    handle.m_sandboxExtension = SandboxExtensionImpl::create(FileSystem::fileSystemRepresentation(path.data()).data(), type);

    if (!handle.m_sandboxExtension) {
        WTFLogAlways("Could not create a sandbox extension for temporary file '%s'", path.data());
        return String();
    }
    return String(path.data());
}

bool SandboxExtension::createHandleForGenericExtension(const String& extensionClass, Handle& handle)
{
    ASSERT(!handle.m_sandboxExtension);

    handle.m_sandboxExtension = SandboxExtensionImpl::create(extensionClass.utf8().data(), Type::Generic);
    if (!handle.m_sandboxExtension) {
        WTFLogAlways("Could not create a '%s' sandbox extension", extensionClass.utf8().data());
        return false;
    }
    
    return true;
}

bool SandboxExtension::createHandleForMachLookupByPid(const String& service, pid_t pid, Handle& handle)
{
    ASSERT(!handle.m_sandboxExtension);
    
    handle.m_sandboxExtension = SandboxExtensionImpl::create(service.utf8().data(), Type::Mach, pid);
    if (!handle.m_sandboxExtension) {
        WTFLogAlways("Could not create a '%s' sandbox extension", service.utf8().data());
        return false;
    }
    
    return true;
}

bool SandboxExtension::createHandleForReadByPid(const String& path, ProcessID pid, Handle& handle)
{
    ASSERT(!handle.m_sandboxExtension);
    
    if (!pid)
        return false;

    handle.m_sandboxExtension = SandboxExtensionImpl::create(path.utf8().data(), Type::ReadByPid, pid);
    if (!handle.m_sandboxExtension) {
        WTFLogAlways("Could not create sandbox extension");
        return false;
    }
    
    return true;
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

} // namespace WebKit

#endif // ENABLE(SANDBOX_EXTENSIONS)
