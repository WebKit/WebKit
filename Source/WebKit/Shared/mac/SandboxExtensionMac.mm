/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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
#import <WebCore/FileSystem.h>
#import <sys/stat.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/text/CString.h>

namespace WebKit {
using namespace WebCore;

class SandboxExtensionImpl {
public:
    static std::unique_ptr<SandboxExtensionImpl> create(const char* path, SandboxExtension::Type type)
    {
        std::unique_ptr<SandboxExtensionImpl> impl { new SandboxExtensionImpl(path, type) };
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

    bool consume() WARN_UNUSED_RETURN
    {
        m_handle = sandbox_extension_consume(m_token);
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        return !sandbox_check(getpid(), 0, SANDBOX_FILTER_NONE);
#else
        return m_handle;
#endif
    }

    bool invalidate()
    {
        return !sandbox_extension_release(std::exchange(m_handle, 0));
    }

    const char* getSerializedFormat(size_t& length) WARN_UNUSED_RETURN
    {
        length = strlen(m_token);
        return m_token;
    }

private:
    char* sandboxExtensionForType(const char* path, SandboxExtension::Type type)
    {
        switch (type) {
        case SandboxExtension::Type::ReadOnly:
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            return sandbox_extension_issue_file(APP_SANDBOX_READ, path, 0);
        case SandboxExtension::Type::ReadWrite:
            return sandbox_extension_issue_file(APP_SANDBOX_READ_WRITE, path, 0);
            ALLOW_DEPRECATED_DECLARATIONS_END
        case SandboxExtension::Type::Generic:
            return sandbox_extension_issue_generic(path, 0);
        }
    }

    SandboxExtensionImpl(const char* path, SandboxExtension::Type type)
        : m_token { sandboxExtensionForType(path, type) }
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

    if (dataReference.isEmpty())
        return {{ }};

    Handle handle;
    handle.m_sandboxExtension = std::make_unique<SandboxExtensionImpl>(reinterpret_cast<const char*>(dataReference.data()), dataReference.size());
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

bool SandboxExtension::HandleArray::decode(IPC::Decoder& decoder, SandboxExtension::HandleArray& handles)
{
    uint64_t size;
    if (!decoder.decode(size))
        return false;
    handles.allocate(size);
    for (size_t i = 0; i < size; i++) {
        std::optional<SandboxExtension::Handle> handle;
        decoder >> handle;
        if (!handle)
            return false;
        handles[i] = WTFMove(*handle);
    }
    return true;
}

RefPtr<SandboxExtension> SandboxExtension::create(Handle&& handle)
{
    if (!handle.m_sandboxExtension)
        return nullptr;

    return adoptRef(new SandboxExtension(handle));
}

static CString resolveSymlinksInPath(const CString& path)
{
    struct stat statBuf;

    // Check if this file exists.
    if (!stat(path.data(), &statBuf)) {
        char resolvedName[PATH_MAX];

        return realpath(path.data(), resolvedName);
    }

    const char* slashPtr = strrchr(path.data(), '/');
    if (slashPtr == path.data())
        return path;

    size_t parentDirectoryLength = slashPtr - path.data();
    if (parentDirectoryLength >= PATH_MAX)
        return CString();

    // Get the parent directory.
    char parentDirectory[PATH_MAX];
    memcpy(parentDirectory, path.data(), parentDirectoryLength);
    parentDirectory[parentDirectoryLength] = '\0';

    // Resolve it.
    CString resolvedParentDirectory = resolveSymlinksInPath(CString(parentDirectory));
    if (resolvedParentDirectory.isNull())
        return CString();

    size_t lastPathComponentLength = path.length() - parentDirectoryLength;
    size_t resolvedPathLength = resolvedParentDirectory.length() + lastPathComponentLength;
    if (resolvedPathLength >= PATH_MAX)
        return CString();

    // Combine the resolved parent directory with the last path component.
    char* resolvedPathBuffer;
    CString resolvedPath = CString::newUninitialized(resolvedPathLength, resolvedPathBuffer);
    memcpy(resolvedPathBuffer, resolvedParentDirectory.data(), resolvedParentDirectory.length());
    memcpy(resolvedPathBuffer + resolvedParentDirectory.length(), slashPtr, lastPathComponentLength);

    return resolvedPath;
}

String stringByResolvingSymlinksInPath(const String& path)
{
    return String::fromUTF8(resolveSymlinksInPath(path.utf8()));
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
    // FIXME: Do we need both resolveSymlinksInPath() and -stringByStandardizingPath?
    CString fileSystemPath = FileSystem::fileSystemRepresentation([(NSString *)path stringByStandardizingPath]);
    if (fileSystemPath.isNull()) {
        LOG_ERROR("Could not create a valid file system representation for the string '%s' of length %lu", fileSystemPath.data(), fileSystemPath.length());
        return { };
    }

    CString standardizedPath = resolveSymlinksInPath(fileSystemPath);
    return String::fromUTF8(standardizedPath);
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
