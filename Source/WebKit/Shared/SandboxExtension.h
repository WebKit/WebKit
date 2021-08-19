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

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessID.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Encoder;
class Decoder;
}

namespace WebKit {
    
class SandboxExtensionImpl;

class SandboxExtension : public RefCounted<SandboxExtension> {
public:
    enum class Type : uint8_t {
        ReadOnly,
        ReadWrite,
        Mach,
        IOKit,
        Generic,
        ReadByProcess
    };

    enum class Flags : uint8_t {
        Default,
        NoReport,
        DoNotCanonicalize,
    };
    
    class Handle {
        WTF_MAKE_NONCOPYABLE(Handle);
    public:
        Handle();
#if ENABLE(SANDBOX_EXTENSIONS)
        Handle(Handle&&);
        Handle& operator=(Handle&&);
#else
        Handle(Handle&&) = default;
        Handle& operator=(Handle&&) = default;
#endif
        ~Handle();

        void encode(IPC::Encoder&) const;
        static std::optional<Handle> decode(IPC::Decoder&);

    private:
        friend class SandboxExtension;
#if ENABLE(SANDBOX_EXTENSIONS)
        mutable std::unique_ptr<SandboxExtensionImpl> m_sandboxExtension;
#endif
    };
    
    static RefPtr<SandboxExtension> create(Handle&&);
    static std::optional<Handle> createHandle(const String& path, Type);
    static Vector<Handle> createReadOnlyHandlesForFiles(ASCIILiteral logLabel, const Vector<String>& paths);
    static std::optional<Handle> createHandleWithoutResolvingPath(const String& path, Type);
    static std::optional<Handle> createHandleForReadWriteDirectory(const String& path); // Will attempt to create the directory.
    static std::optional<std::pair<Handle, String>> createHandleForTemporaryFile(const String& prefix, Type);
    static std::optional<Handle> createHandleForGenericExtension(ASCIILiteral extensionClass);
#if HAVE(AUDIT_TOKEN)
    static std::optional<Handle> createHandleForMachLookup(ASCIILiteral service, std::optional<audit_token_t>, OptionSet<Flags> = Flags::Default);
    static Vector<Handle> createHandlesForMachLookup(const Vector<ASCIILiteral>& services, std::optional<audit_token_t>, OptionSet<Flags> = Flags::Default);
    static std::optional<Handle> createHandleForReadByAuditToken(const String& path, audit_token_t);
    static std::optional<Handle> createHandleForIOKitClassExtension(ASCIILiteral iokitClass, std::optional<audit_token_t>, OptionSet<Flags> = Flags::Default);
    static Vector<Handle> createHandlesForIOKitClassExtensions(const Vector<ASCIILiteral>& iokitClasses, std::optional<audit_token_t>, OptionSet<Flags> = Flags::Default);
#endif
    ~SandboxExtension();

    bool consume();
    bool revoke();

    bool consumePermanently();

    // FIXME: These should not be const.
    static bool consumePermanently(const Handle&);
    static bool consumePermanently(const Vector<SandboxExtension::Handle>&);

private:
    explicit SandboxExtension(const Handle&);
                     
#if ENABLE(SANDBOX_EXTENSIONS)
    mutable std::unique_ptr<SandboxExtensionImpl> m_sandboxExtension;
    size_t m_useCount { 0 };
#endif
};

#if !ENABLE(SANDBOX_EXTENSIONS)
inline SandboxExtension::Handle::Handle() { }
inline SandboxExtension::Handle::~Handle() { }
inline void SandboxExtension::Handle::encode(IPC::Encoder&) const { }
inline std::optional<SandboxExtension::Handle> SandboxExtension::Handle::decode(IPC::Decoder&) { return SandboxExtension::Handle { }; }
inline RefPtr<SandboxExtension> SandboxExtension::create(Handle&&) { return nullptr; }
inline auto SandboxExtension::createHandle(const String&, Type) -> std::optional<Handle> { return Handle { }; }
inline auto SandboxExtension::createReadOnlyHandlesForFiles(ASCIILiteral, const Vector<String>&) -> Vector<Handle> { return { }; }
inline auto SandboxExtension::createHandleWithoutResolvingPath(const String&, Type) -> std::optional<Handle> { return Handle { }; }
inline auto SandboxExtension::createHandleForReadWriteDirectory(const String&) -> std::optional<Handle> { return Handle { }; }
inline auto SandboxExtension::createHandleForTemporaryFile(const String& /*prefix*/, Type) -> std::optional<std::pair<Handle, String>> { return std::optional<std::pair<Handle, String>> { std::pair<Handle, String> { Handle { }, String { } } }; }
inline auto SandboxExtension::createHandleForGenericExtension(ASCIILiteral /*extensionClass*/) -> std::optional<Handle> { return Handle { }; }
inline SandboxExtension::~SandboxExtension() { }
inline bool SandboxExtension::revoke() { return true; }
inline bool SandboxExtension::consume() { return true; }
inline bool SandboxExtension::consumePermanently() { return true; }
inline bool SandboxExtension::consumePermanently(const Handle&) { return true; }
inline bool SandboxExtension::consumePermanently(const Vector<Handle>&) { return true; }
inline String stringByResolvingSymlinksInPath(const String& path) { return path; }
inline String resolvePathForSandboxExtension(const String& path) { return path; }
inline String resolveAndCreateReadWriteDirectoryForSandboxExtension(const String& path) { return path; }
#else
String stringByResolvingSymlinksInPath(const String& path);
String resolvePathForSandboxExtension(const String& path);
String resolveAndCreateReadWriteDirectoryForSandboxExtension(const String& path);
#endif

} // namespace WebKit
