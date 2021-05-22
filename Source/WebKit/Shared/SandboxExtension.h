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
    enum class Type {
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
        static Optional<Handle> decode(IPC::Decoder&);

    private:
        friend class SandboxExtension;
#if ENABLE(SANDBOX_EXTENSIONS)
        mutable std::unique_ptr<SandboxExtensionImpl> m_sandboxExtension;
#endif
    };

    class HandleArray {
        WTF_MAKE_NONCOPYABLE(HandleArray);
    public:
        HandleArray();
        HandleArray(HandleArray&&) = default;
        HandleArray& operator=(HandleArray&&) = default;
        ~HandleArray();
        void allocate(size_t);
        void append(Handle&&);
        Handle& operator[](size_t i);
        Handle& at(size_t i) { return operator[](i); }
        const Handle& operator[](size_t i) const;
        Handle* begin();
        Handle* end();
        const Handle* begin() const;
        const Handle* end() const;
        size_t size() const;
        void encode(IPC::Encoder&) const;
        static Optional<HandleArray> decode(IPC::Decoder&);

    private:
#if ENABLE(SANDBOX_EXTENSIONS)
        Vector<Handle> m_data;
#else
        Handle m_emptyHandle;
#endif
    };
    
    static RefPtr<SandboxExtension> create(Handle&&);
    static bool createHandle(const String& path, Type, Handle&);
    static SandboxExtension::HandleArray createReadOnlyHandlesForFiles(ASCIILiteral logLabel, const Vector<String>& paths);
    static bool createHandleWithoutResolvingPath(const String& path, Type, Handle&);
    static bool createHandleForReadWriteDirectory(const String& path, Handle&); // Will attempt to create the directory.
    static String createHandleForTemporaryFile(const String& prefix, Type, Handle&);
    static bool createHandleForGenericExtension(ASCIILiteral extensionClass, Handle&);
#if HAVE(AUDIT_TOKEN)
    static bool createHandleForMachLookup(ASCIILiteral service, Optional<audit_token_t>, Handle&, OptionSet<Flags> = Flags::Default);
    static HandleArray createHandlesForMachLookup(const Vector<ASCIILiteral>& services, Optional<audit_token_t>, OptionSet<Flags> = Flags::Default);
    static bool createHandleForReadByAuditToken(const String& path, audit_token_t, Handle&);
    static bool createHandleForIOKitClassExtension(ASCIILiteral iokitClass, Optional<audit_token_t>, Handle&, OptionSet<Flags> = Flags::Default);
    static HandleArray createHandlesForIOKitClassExtensions(const Vector<ASCIILiteral>& iokitClasses, Optional<audit_token_t>, OptionSet<Flags> = Flags::Default);
#endif
    ~SandboxExtension();

    bool consume();
    bool revoke();

    bool consumePermanently();
    static bool consumePermanently(const Handle&);
    static bool consumePermanently(const HandleArray&);

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
inline Optional<SandboxExtension::Handle> SandboxExtension::Handle::decode(IPC::Decoder&) { return SandboxExtension::Handle { }; }
inline SandboxExtension::HandleArray::HandleArray() { }
inline SandboxExtension::HandleArray::~HandleArray() { }
inline void SandboxExtension::HandleArray::allocate(size_t) { }
inline void SandboxExtension::HandleArray::append(Handle&&) { }
inline size_t SandboxExtension::HandleArray::size() const { return 0; }    
inline const SandboxExtension::Handle& SandboxExtension::HandleArray::operator[](size_t) const { return m_emptyHandle; }
inline SandboxExtension::Handle& SandboxExtension::HandleArray::operator[](size_t) { return m_emptyHandle; }
inline SandboxExtension::Handle* SandboxExtension::HandleArray::begin() { return &m_emptyHandle; }
inline SandboxExtension::Handle* SandboxExtension::HandleArray::end() { return &m_emptyHandle; }
inline const SandboxExtension::Handle* SandboxExtension::HandleArray::begin() const { return &m_emptyHandle; }
inline const SandboxExtension::Handle* SandboxExtension::HandleArray::end() const { return &m_emptyHandle; }
inline void SandboxExtension::HandleArray::encode(IPC::Encoder&) const { }
inline auto SandboxExtension::HandleArray::decode(IPC::Decoder&) -> Optional<HandleArray> { return {{ }}; }
inline RefPtr<SandboxExtension> SandboxExtension::create(Handle&&) { return nullptr; }
inline bool SandboxExtension::createHandle(const String&, Type, Handle&) { return true; }
inline SandboxExtension::HandleArray SandboxExtension::createReadOnlyHandlesForFiles(ASCIILiteral, const Vector<String>&) { return { }; }
inline bool SandboxExtension::createHandleWithoutResolvingPath(const String&, Type, Handle&) { return true; }
inline bool SandboxExtension::createHandleForReadWriteDirectory(const String&, Handle&) { return true; }
inline String SandboxExtension::createHandleForTemporaryFile(const String& /*prefix*/, Type, Handle&) {return String();}
inline bool SandboxExtension::createHandleForGenericExtension(ASCIILiteral /*extensionClass*/, Handle&) { return true; }
inline SandboxExtension::~SandboxExtension() { }
inline bool SandboxExtension::revoke() { return true; }
inline bool SandboxExtension::consume() { return true; }
inline bool SandboxExtension::consumePermanently() { return true; }
inline bool SandboxExtension::consumePermanently(const Handle&) { return true; }
inline bool SandboxExtension::consumePermanently(const HandleArray&) { return true; }
inline String stringByResolvingSymlinksInPath(const String& path) { return path; }
inline String resolvePathForSandboxExtension(const String& path) { return path; }
inline String resolveAndCreateReadWriteDirectoryForSandboxExtension(const String& path) { return path; }
#else
inline SandboxExtension::Handle* SandboxExtension::HandleArray::begin() { return m_data.begin(); }
inline SandboxExtension::Handle* SandboxExtension::HandleArray::end() { return m_data.end(); }
inline const SandboxExtension::Handle* SandboxExtension::HandleArray::begin() const { return m_data.begin(); }
inline const SandboxExtension::Handle* SandboxExtension::HandleArray::end() const { return m_data.end(); }
String stringByResolvingSymlinksInPath(const String& path);
String resolvePathForSandboxExtension(const String& path);
String resolveAndCreateReadWriteDirectoryForSandboxExtension(const String& path);
#endif

} // namespace WebKit
