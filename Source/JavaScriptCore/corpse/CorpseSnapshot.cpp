/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#include <wtf/StdLibExtras.h>
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
    if (!name || !*name) {
        CORPSE_REPORT("A symbol lookup needs a name");
        return { };
    }

    auto entry = m_symbols.ensure<StringViewHashTranslator>(StringView::fromLatin1(name), [&] {
        return WTF::makeUnique<Symbol>(*this, name);
    });

    return entry.iterator->value->address();
}

const Vector<Image>& Snapshot::images()
{
    if (!m_images)
        m_images = Image::collect(*this);
    return *m_images;
}

std::optional<Vector<uint8_t>> Snapshot::copyBytes(Address address, size_t length) const
{
    Vector<uint8_t> buffer;
    if (!buffer.tryGrow(length))
        return std::nullopt;
    if (read(address, buffer.mutableSpan()).empty())
        return std::nullopt;
    return buffer;
}

std::optional<CString> Snapshot::copyCString(Address address, size_t maxLength) const
{
    Vector<char> characters;
    for (size_t offset = 0; offset < maxLength; ++offset) {
        auto character = read<char>(address + offset);
        if (!character)
            return std::nullopt;
        if (!*character)
            return UTF8CString(byteCast<char8_t>(characters.span()));
        characters.append(*character);
    }
    return std::nullopt;
}

Snapshot::Snapshot(RefPtr<Process> process)
    : m_process(WTF::move(process))
    , m_id(s_nextId++)
{
    if (!m_process || !m_process->isAttached())
        return;

#if OS(DARWIN)
    // Snapshot the target into a corpse; only a read port is required from here
    // on, and the corpse is independent of the live target.
    mach_port_t corpsePort = MACH_PORT_NULL;
    kern_return_t kr = task_generate_corpse(m_process->taskPort(), &corpsePort);
    if (kr == KERN_SUCCESS)
        m_corpsePort = OwnedTaskHandle::adopt(corpsePort);
    else {
        if (!m_process->holdsLiveTask()) {
            Error::report("Could not snapshot PID %d: the process has terminated",
                static_cast<int>(m_process->pid()));
        } else {
            Error::report("Could not snapshot PID %d: %s (0x%x)",
                static_cast<int>(m_process->pid()), mach_error_string(kr), kr);
        }
    }
#else // OS(DARWIN)
    m_corpsePort = OwnedTaskHandle::adopt(m_process->taskPort());
#endif // OS(DARWIN)
}

Snapshot::~Snapshot() = default;

std::span<uint8_t> Snapshot::read(Address address, std::span<uint8_t> into) const
{
    if (!isValid())
        return { };
#if OS(DARWIN)
    mach_vm_size_t got = 0;
    kern_return_t kr = mach_vm_read_overwrite(corpsePort(), address.toTargetVMAddress(), into.size(),
        reinterpret_cast<mach_vm_address_t>(into.data()), &got);
    if (kr != KERN_SUCCESS || got != into.size())
        return { };
#else // OS(DARWIN)
    struct iovec local { into.data(), into.size() };
    struct iovec remote { reinterpret_cast<void*>(address.toTargetVMAddress()), into.size() };
    ssize_t got = process_vm_readv(corpsePort(), &local, 1, &remote, 1, 0);
    if (got < 0 || static_cast<size_t>(got) != into.size())
        return { };
#endif  // OS(DARWIN)
    return into;
}

} // namespace Corpse
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(MYA)
