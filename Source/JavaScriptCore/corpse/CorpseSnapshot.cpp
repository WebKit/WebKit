/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CorpseSnapshot.h"

#if ENABLE(MYA)

#include "CorpseError.h"

#if OS(DARWIN)
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_vm.h>
#else
#include <sys/uio.h>
#endif
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace Corpse {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Snapshot);

unsigned Snapshot::s_nextId = 1;

const Vector<Thread>& Snapshot::threads()
{
    if (!m_threads)
        m_threads = Thread::collect(*this);
    return *m_threads;
}

Address Snapshot::symbol(const char* name)
{
    if (!name || !*name)
        return { };

    auto entry = m_symbols.ensure<StringViewHashTranslator>(StringView::fromLatin1(name), [&] {
        return WTF::makeUnique<Symbol>(*this, name);
    });

    return entry.iterator->value->address();
}

#if OS(DARWIN)

Snapshot::Snapshot(RefPtr<Process> process)
    : m_process(WTF::move(process))
    , m_id(s_nextId++)
{
    if (!m_process || !m_process->isAttached())
        return;

    // Snapshot the target into a corpse; only a read port is required from here
    // on, and the corpse is independent of the live target.
    kern_return_t kr = task_generate_corpse(m_process->taskPort(), &m_corpsePort);
    if (kr != KERN_SUCCESS) {
        m_corpsePort = MACH_PORT_NULL;
        if (!m_process->holdsLiveTask()) {
            Error::report("Could not snapshot PID %d: the process has terminated",
                static_cast<int>(m_process->pid()));
        } else {
            Error::report("Could not snapshot PID %d: %s (0x%x)",
                static_cast<int>(m_process->pid()), mach_error_string(kr), kr);
        }
    }
}

Snapshot::~Snapshot()
{
    if (isValid())
        mach_port_deallocate(mach_task_self(), m_corpsePort);
}

std::span<uint8_t> Snapshot::read(Address address, std::span<uint8_t> into) const
{
    if (!isValid())
        return { };
    mach_vm_size_t got = 0;
    kern_return_t kr = mach_vm_read_overwrite(m_corpsePort, address.toTargetVMAddress(), into.size(),
        reinterpret_cast<mach_vm_address_t>(into.data()), &got);
    if (kr != KERN_SUCCESS || got != into.size())
        return { };
    return into;
}

#else

Snapshot::Snapshot(RefPtr<Process> process)
    : m_process(WTF::move(process))
    , m_id(s_nextId++)
{
    if (!m_process || !m_process->isAttached())
        return;
    m_corpsePort = m_process->taskPort();
}

Snapshot::~Snapshot() = default;

std::span<uint8_t> Snapshot::read(Address address, std::span<uint8_t> into) const
{
    if (!isValid())
        return { };
    struct iovec local { into.data(), into.size() };
    struct iovec remote { reinterpret_cast<void*>(address.toTargetVMAddress()), into.size() };
    ssize_t got = process_vm_readv(m_corpsePort, &local, 1, &remote, 1, 0);
    if (got < 0 || static_cast<size_t>(got) != into.size())
        return { };
    return into;
}

#endif // OS(DARWIN)

} // namespace Corpse
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(MYA)
