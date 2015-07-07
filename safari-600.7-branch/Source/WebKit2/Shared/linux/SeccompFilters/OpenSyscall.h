/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#ifndef OpenSyscall_h
#define OpenSyscall_h

#if ENABLE(SECCOMP_FILTERS)

#include "Syscall.h"
#include <wtf/text/CString.h>

namespace IPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

class OpenSyscall : public Syscall {
public:
    static std::unique_ptr<Syscall> createFromOpenatContext(mcontext_t*);
    static std::unique_ptr<Syscall> createFromCreatContext(mcontext_t*);

    explicit OpenSyscall(mcontext_t*);

    void setPath(const CString& path) { m_path = path; };
    void setFlags(const int flags) { m_flags = flags; };
    void setMode(const mode_t mode) { m_mode = mode; };

    // Syscall implementation.
    virtual void setResult(const SyscallResult*);
    virtual std::unique_ptr<SyscallResult> execute(const SyscallPolicy&);
    virtual void encode(IPC::ArgumentEncoder&) const;
    virtual bool decode(IPC::ArgumentDecoder*);

private:
    CString m_path;
    int m_flags;
    mode_t m_mode;
};

class OpenSyscallResult : public SyscallResult {
public:
    OpenSyscallResult(int fd, int errorNumber);
    ~OpenSyscallResult();

    int fd() const { return m_fd; }
    int errorNumber() const { return m_errorNumber; }

    // SyscallResult implementation.
    virtual void encode(IPC::ArgumentEncoder&) const;
    virtual bool decode(IPC::ArgumentDecoder*, int fd);

private:
    int m_fd;
    int m_errorNumber;
};


} // namespace WebKit

#endif // ENABLE(SECCOMP_FILTERS)

#endif // OpenSyscall_h
