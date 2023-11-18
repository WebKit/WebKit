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

#pragma once

#include <wtf/ArgumentCoder.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessID.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

enum class SandboxExtensionFlags : uint8_t {
    Default,
    NoReport,
    DoNotCanonicalize,
};

enum class SandboxExtensionType : uint8_t {
    ReadOnly,
    ReadWrite,
    Mach,
    IOKit,
    Generic,
    ReadByProcess
};

#if ENABLE(SANDBOX_EXTENSIONS)
class SandboxExtensionImpl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<SandboxExtensionImpl> create(const char* path, SandboxExtensionType, std::optional<audit_token_t> = std::nullopt, OptionSet<SandboxExtensionFlags> = SandboxExtensionFlags::Default);
    SandboxExtensionImpl(std::span<const uint8_t>);
    ~SandboxExtensionImpl();

    bool WARN_UNUSED_RETURN consume();
    bool invalidate();
    std::span<const uint8_t> WARN_UNUSED_RETURN getSerializedFormat();

    SandboxExtensionImpl(SandboxExtensionImpl&& other)
        : m_token(std::exchange(other.m_token, nullptr))
        , m_handle(std::exchange(other.m_handle, 0)) { }
private:
    char* sandboxExtensionForType(const char* path, SandboxExtensionType, std::optional<audit_token_t>, OptionSet<SandboxExtensionFlags>);

    SandboxExtensionImpl(const char* path, SandboxExtensionType, std::optional<audit_token_t>, OptionSet<SandboxExtensionFlags>);

    char* m_token { nullptr };
    int64_t m_handle { 0 };
};
#endif

class SandboxExtensionHandle {
    WTF_MAKE_NONCOPYABLE(SandboxExtensionHandle);
public:
    SandboxExtensionHandle();
#if ENABLE(SANDBOX_EXTENSIONS)
    SandboxExtensionHandle(SandboxExtensionHandle&&);
    SandboxExtensionHandle& operator=(SandboxExtensionHandle&&);
    SandboxExtensionHandle(std::unique_ptr<SandboxExtensionImpl>&& impl)
        : m_sandboxExtension(WTFMove(impl)) { }
#else
    SandboxExtensionHandle(SandboxExtensionHandle&&) = default;
    SandboxExtensionHandle& operator=(SandboxExtensionHandle&&) = default;
#endif
    ~SandboxExtensionHandle();

#if ENABLE(SANDBOX_EXTENSIONS)
    std::unique_ptr<SandboxExtensionImpl> takeImpl() { return std::exchange(m_sandboxExtension, nullptr); }
#endif

private:
    friend class SandboxExtension;
#if ENABLE(SANDBOX_EXTENSIONS)
    // FIXME: change SandboxExtension(const Handle&) to SandboxExtension(Handle&&) and make this no longer mutable.
    mutable std::unique_ptr<SandboxExtensionImpl> m_sandboxExtension;
#endif
};

class SandboxExtension : public RefCounted<SandboxExtension> {
public:
    using Handle = SandboxExtensionHandle;
    using Type = SandboxExtensionType;
    using Flags = SandboxExtensionFlags;

    enum class MachBootstrapOptions : uint8_t {
        DoNotEnableMachBootstrap,
        EnableMachBootstrap
    };

    static RefPtr<SandboxExtension> create(Handle&&);
    static std::optional<Handle> createHandle(StringView path, Type);
    static Vector<Handle> createReadOnlyHandlesForFiles(ASCIILiteral logLabel, const Vector<String>& paths);
    static std::optional<Handle> createHandleWithoutResolvingPath(StringView path, Type);
    static std::optional<Handle> createHandleForReadWriteDirectory(StringView path); // Will attempt to create the directory.
    static std::optional<std::pair<Handle, String>> createHandleForTemporaryFile(StringView prefix, Type);
    static std::optional<Handle> createHandleForGenericExtension(ASCIILiteral extensionClass);
    static Handle createHandleForMachBootstrapExtension();
#if HAVE(AUDIT_TOKEN)
    static std::optional<Handle> createHandleForMachLookup(ASCIILiteral service, std::optional<audit_token_t>, OptionSet<Flags> = Flags::Default);
    static Vector<Handle> createHandlesForMachLookup(std::span<const ASCIILiteral> services, std::optional<audit_token_t>, MachBootstrapOptions = MachBootstrapOptions::DoNotEnableMachBootstrap, OptionSet<Flags> = Flags::Default);
    static Vector<Handle> createHandlesForMachLookup(std::initializer_list<const ASCIILiteral> services, std::optional<audit_token_t>, MachBootstrapOptions = MachBootstrapOptions::DoNotEnableMachBootstrap, OptionSet<Flags> = Flags::Default);
    static std::optional<Handle> createHandleForReadByAuditToken(StringView path, audit_token_t);
    static std::optional<Handle> createHandleForIOKitClassExtension(ASCIILiteral iokitClass, std::optional<audit_token_t>, OptionSet<Flags> = Flags::Default);
    static Vector<Handle> createHandlesForIOKitClassExtensions(std::span<const ASCIILiteral> iokitClasses, std::optional<audit_token_t>, OptionSet<Flags> = Flags::Default);
#endif
    ~SandboxExtension();

    bool consume();
    bool revoke();

    bool consumePermanently();

    // FIXME: These should not be const.
    static bool consumePermanently(const Handle&);
    static bool consumePermanently(const Vector<Handle>&);

private:
    explicit SandboxExtension(const Handle&);
                     
#if ENABLE(SANDBOX_EXTENSIONS)
    std::unique_ptr<SandboxExtensionImpl> m_sandboxExtension;
    size_t m_useCount { 0 };
#endif
};

String stringByResolvingSymlinksInPath(StringView path);
String resolvePathForSandboxExtension(StringView path);
String resolveAndCreateReadWriteDirectoryForSandboxExtension(StringView path);

#if !ENABLE(SANDBOX_EXTENSIONS)

inline SandboxExtensionHandle::SandboxExtensionHandle() { }
inline SandboxExtensionHandle::~SandboxExtensionHandle() { }
inline RefPtr<SandboxExtension> SandboxExtension::create(Handle&&) { return nullptr; }
inline auto SandboxExtension::createHandle(StringView, Type) -> std::optional<Handle> { return Handle { }; }
inline auto SandboxExtension::createReadOnlyHandlesForFiles(ASCIILiteral, const Vector<String>&) -> Vector<Handle> { return { }; }
inline auto SandboxExtension::createHandleWithoutResolvingPath(StringView, Type) -> std::optional<Handle> { return Handle { }; }
inline auto SandboxExtension::createHandleForReadWriteDirectory(StringView) -> std::optional<Handle> { return Handle { }; }
inline auto SandboxExtension::createHandleForTemporaryFile(StringView, Type) -> std::optional<std::pair<Handle, String>> { return std::pair<Handle, String> { }; }
inline auto SandboxExtension::createHandleForGenericExtension(ASCIILiteral) -> std::optional<Handle> { return Handle { }; }
inline SandboxExtension::~SandboxExtension() { }
inline bool SandboxExtension::revoke() { return true; }
inline bool SandboxExtension::consume() { return true; }
inline bool SandboxExtension::consumePermanently() { return true; }
inline bool SandboxExtension::consumePermanently(const Handle&) { return true; }
inline bool SandboxExtension::consumePermanently(const Vector<Handle>&) { return true; }
inline String stringByResolvingSymlinksInPath(StringView path) { return path.toString(); }
inline String resolvePathForSandboxExtension(StringView path) { return path.toString(); }
inline String resolveAndCreateReadWriteDirectoryForSandboxExtension(StringView path) { return path.toString(); }

#endif

} // namespace WebKit
