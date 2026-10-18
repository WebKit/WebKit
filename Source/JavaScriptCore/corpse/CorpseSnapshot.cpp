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
#include "CorpseLimits.h"
#include "VM.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wtf/MainThread.h>
#include <wtf/SafeStrerror.h>
#if OS(DARWIN)
#include <libproc.h>
#include <mach-o/dyld_images.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/task_info.h>
#include <sys/proc.h>
#else
#include <stdio.h>
#include <sys/prctl.h>
#include <wtf/FileSystem.h>
#include <wtf/text/StringToIntegerConversion.h>
#endif
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(MYA_HEAP) && OS(DARWIN) && !defined(BUILDING_WITH_CMAKE)
// Xcode has no optional dependencies, so liblldb is linked only when its headers are found.
__asm__(".linker_option \"-llldb\"");
#endif

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

const Vector<Image>& Snapshot::images()
{
    if (!m_images) {
        if (!isValid()) {
            CORPSE_REPORT("Cannot list the images of an invalid snapshot");
            m_images = Vector<Image> { };
        } else {
            CORPSE_DIAGNOSTICS(diagnostics, "listing the images of pid %d", static_cast<int>(process()->pid()));
            m_images = Image::collect(*this);
        }
    }
    return *m_images;
}

const Vector<Region>& Snapshot::regions()
{
    if (!m_regions) {
        if (!isValid()) {
            CORPSE_REPORT("Cannot list the regions of an invalid snapshot");
            m_regions = Vector<Region> { };
        } else
            m_regions = Region::all(corpsePort());
    }
    return *m_regions;
}

#if OS(DARWIN)

// TASK_DYLD_INFO is in <mach/task_info.h>; the structures are dyld's, in <mach-o/dyld_images.h>.
Memory::Ptr<dyld_all_image_infos> Snapshot::dyldAllImageInfos()
{
    task_dyld_info_data_t dyldInfo;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    kern_return_t kr = task_info(corpsePort(), TASK_DYLD_INFO, reinterpret_cast<task_info_t>(&dyldInfo), &count);
    if (kr != KERN_SUCCESS) {
        CORPSE_REPORT("Could not read dyld information: %s (0x%x)", mach_error_string(kr), kr);
        return { };
    }

    Address address { dyldInfo.all_image_info_addr };
    if (!address) {
        CORPSE_REPORT("dyld reports no image list");
        return { };
    }
    auto allImages = memory().ptr<dyld_all_image_infos>(address);
    if (!allImages)
        CORPSE_REPORT("Could not read dyld_all_image_infos at 0x%llx", address.toTargetVMAddress());
    return allImages;
}

Memory::Span<dyld_image_info> Snapshot::dyldImageInfos(const Memory::Ptr<dyld_all_image_infos>& allImages)
{
    Address arrayAddress = Address { allImages->infoArray }.stripped();
    uint32_t imageCount = allImages->infoArrayCount;
    if (!arrayAddress || !imageCount) {
        CORPSE_REPORT("dyld_all_image_infos v%u at 0x%llx lists no images",
            allImages->version, allImages.address().toTargetVMAddress());
        return { };
    }
    if (imageCount > maxImageCount) {
        CORPSE_REPORT("%u images is too many to be a real image list", imageCount);
        return { };
    }
    auto images = memory().span<dyld_image_info>(arrayAddress, imageCount);
    if (!images) {
        CORPSE_REPORT("Could not read the %u image entries at 0x%llx", imageCount, arrayAddress.toTargetVMAddress());
        return { };
    }
    Diagnostics::count(DiagnosticCounter::ImagesListed, imageCount);
    return images;
}

#endif // OS(DARWIN)

Address Snapshot::symbol(const char* name)
{
    if (!name || !*name) {
        CORPSE_REPORT("A symbol lookup needs a name");
        return { };
    }

    auto entry = m_symbols.ensure<StringViewHashTranslator>(StringView::fromLatin1(name), [&] {
        return WTF::makeUnique<Symbol>(*this, name);
    });

    Address address = entry.iterator->value->address();
    if (!address && !entry.isNewEntry) {
        if (!isValid())
            CORPSE_REPORT("Cannot look up '%s' in an invalid snapshot", name);
        else
            CORPSE_REPORT("No symbol '%s' in pid %d, as an earlier lookup found", name, static_cast<int>(process()->pid()));
    }
    return address;
}

enum class RunState : uint8_t { Running, Stopped, Exited };

#if !OS(DARWIN)

// The fields of /proc/<pid>/stat read here, with proc_pid_stat(5)'s numbers and scanf
// conversions. The name is in parentheses and may hold spaces, so the rest is scanned
// from after the last ')'.
struct ProcessStat {
    std::string name; // (2) comm
    char state; // (3)
    pid_t parent; // (4) ppid
    unsigned long long startTime; // (22) starttime, in clock ticks since boot
};

static std::optional<ProcessStat> processStat(pid_t pid)
{
    auto stat = readProcFile(pid, "stat");
    size_t open = stat ? stat->find('(') : std::string::npos;
    size_t close = stat ? stat->rfind(')') : std::string::npos;
    if (open == std::string::npos || close == std::string::npos || close < open)
        return std::nullopt;
    ProcessStat result { stat->substr(open + 1, close - open - 1) };
    if (sscanf(stat->c_str() + close + 1, " %c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %llu", &result.state, &result.parent, &result.startTime) != 3)
        return std::nullopt;
    return result;
}

#endif

// Darwin's pbi_status (<sys/proc_info.h>) holds a state from <sys/proc.h>.
static RunState runState(pid_t pid)
{
#if OS(DARWIN)
    struct proc_bsdinfo info;
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info) || info.pbi_status == SZOMB)
        return RunState::Exited;
    return info.pbi_status == SSTOP ? RunState::Stopped : RunState::Running;
#else
    auto stat = processStat(pid);
    if (!stat || stat->state == 'Z' || stat->state == 'X')
        return RunState::Exited;
    return stat->state == 'T' ? RunState::Stopped : RunState::Running;
#endif
}

static bool waitUntilStopped(pid_t pid)
{
    for (;;) {
        switch (runState(pid)) {
        case RunState::Running:
            usleep(1000);
            break;
        case RunState::Stopped:
            return true;
        case RunState::Exited:
            CORPSE_REPORT("pid %d exited before it offered a snapshot", static_cast<int>(pid));
            return false;
        }
    }
}

#if !OS(DARWIN)

// The name findCopy() looks for, as `ps` shows it.
static constexpr const char* copyName = "mya-snapshot";

// A copy of this process, stopped, so that its memory stays as it is now. It ends with
// its snapshot or, by PR_SET_PDEATHSIG, with the thread that forked it, so offer() runs
// on the main thread; the getppid() check closes the window before the prctl. See prctl(2).
static pid_t forkCopy()
{
    pid_t parent = getpid();
    pid_t copy = fork();
    if (!copy) {
        if (prctl(PR_SET_PDEATHSIG, SIGKILL) || getppid() != parent || prctl(PR_SET_NAME, copyName))
            _exit(1);
        for (;;)
            raise(SIGSTOP);
    }
    if (copy < 0) {
        CORPSE_REPORT("Could not copy pid %d: %s", static_cast<int>(parent), safeStrerror(errno).legacyCStringPointer());
        return -1;
    }
    int status = 0;
    pid_t result;
    while ((result = waitpid(copy, &status, WUNTRACED)) < 0 && errno == EINTR) { }
    if (result == copy && WIFSTOPPED(status))
        return copy;
    CORPSE_REPORT("The copy %d of pid %d did not stop", static_cast<int>(copy), static_cast<int>(parent));
    kill(copy, SIGKILL);
    while (waitpid(copy, nullptr, 0) < 0 && errno == EINTR) { }
    return -1;
}

#endif

void Snapshot::offer()
{
    // On Linux, the copy lives as long as the thread that forked it (forkCopy()).
    RELEASE_ASSERT(isMainThread());
#if !OS(DARWIN)
    forkCopy();
#endif
    // receive() snapshots this process while it is stopped, then continues it.
    raise(SIGSTOP);
}

void Snapshot::offerSafepoint(VM& vm)
{
    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());
    RELEASE_ASSERT(vm.heap.isDeferred());
    RELEASE_ASSERT(!vm.heap.collectionScope());
    RELEASE_ASSERT(!vm.heap.objectSpace().isMarking());
    offer();
}

std::unique_ptr<Snapshot> Snapshot::receive(pid_t target)
{
    if (!waitUntilStopped(target))
        return nullptr;
    auto snapshot = snapshotOfStopped(target);
    kill(target, SIGCONT);
    return snapshot;
}

#if OS(DARWIN)

// Returns a null handle if the snapshot could not be taken, having reported why.
static OwnedTaskHandle takeSnapshot(Process* process)
{
    if (!process || !process->isAttached()) {
        CORPSE_REPORT("Could not snapshot: No process attached");
        return { };
    }

    mach_port_t corpsePort = MACH_PORT_NULL;
    kern_return_t kr = task_generate_corpse(process->taskPort(), &corpsePort);
    if (kr == KERN_SUCCESS)
        return OwnedTaskHandle::adopt(corpsePort);

    if (!process->holdsLiveTask()) {
        Error::report("Could not snapshot PID %d: the process has terminated",
            static_cast<int>(process->pid()));
    } else {
        Error::report("Could not snapshot PID %d: %s (0x%x)",
            static_cast<int>(process->pid()), mach_error_string(kr), kr);
    }
    return { };
}

Snapshot::Snapshot(RefPtr<Process> process)
    : m_process(WTF::move(process))
    , m_corpsePort(takeSnapshot(m_process.get()))
    , m_executablePath(isValid() ? m_process->executablePath() : UTF8CString { })
    , m_id(s_nextId++)
    , m_memory(corpsePort())
{
}

Snapshot::~Snapshot() = default;

std::unique_ptr<Snapshot> Snapshot::snapshotOfStopped(pid_t target)
{
    RefPtr<Process> process = Process::create(target);
    if (!process->attach())
        return nullptr;
    auto snapshot = makeUnique<Snapshot>(WTF::move(process));
    if (!snapshot->isValid())
        return nullptr;
    return snapshot;
}

#else

// The copy offer() forked for this snapshot: the newest live child of the target named
// copyName. An older one belongs to another snapshot or to a receiver that died, and
// one its snapshot killed is a zombie, since the target does not reap it.
static pid_t findCopy(pid_t target)
{
    pid_t copy = -1;
    unsigned long long newestStart = 0;
    for (const String& entry : FileSystem::listDirectory("/proc"_s)) {
        auto pid = parseInteger<pid_t>(entry);
        auto stat = pid ? processStat(*pid) : std::nullopt;
        if (stat && stat->parent == target && stat->name == copyName && stat->state != 'Z' && stat->startTime >= newestStart) {
            if (stat->startTime == newestStart && copy > 0) {
                CORPSE_REPORT("pid %d has two copies of itself started in the same clock tick", static_cast<int>(target));
                return -1;
            }
            newestStart = stat->startTime;
            copy = *pid;
        }
    }
    if (copy < 0)
        CORPSE_REPORT("pid %d stopped with no copy of itself", static_cast<int>(target));
    return copy;
}

// Process::attach attaches only to this process, which takes a copy of itself.
static OwnedTaskHandle takeSnapshot(Process* process)
{
    if (!process || !process->isAttached()) {
        CORPSE_REPORT("Could not snapshot: No process attached");
        return { };
    }
    RELEASE_ASSERT(process->pid() == getpid());
    pid_t copy = forkCopy();
    if (copy < 0)
        return { };
    return OwnedTaskHandle::adopt(copy);
}

Snapshot::Snapshot(RefPtr<Process> process)
    : m_process(WTF::move(process))
    , m_corpsePort(takeSnapshot(m_process.get()))
    , m_executablePath(isValid() ? Process::create(corpsePort())->executablePath() : UTF8CString { })
    , m_id(s_nextId++)
    , m_memory(corpsePort())
{
}

Snapshot::Snapshot(RefPtr<Process> process, pid_t copy)
    : m_process(WTF::move(process))
    , m_corpsePort(OwnedTaskHandle::adopt(copy))
    , m_executablePath(Process::create(copy)->executablePath())
    , m_id(s_nextId++)
    , m_memory(corpsePort())
{
    RELEASE_ASSERT(copy > 0);
}

std::unique_ptr<Snapshot> Snapshot::snapshotOfStopped(pid_t target)
{
    pid_t copy = findCopy(target);
    if (copy < 0)
        return nullptr;
    return std::unique_ptr<Snapshot>(new Snapshot(Process::create(target), copy));
}

Snapshot::~Snapshot()
{
    if (!isValid())
        return;
    pid_t copy = corpsePort();
    kill(copy, SIGKILL);
    // Reaps a copy of this process; another process's copy is not this process's child.
    while (waitpid(copy, nullptr, 0) < 0 && errno == EINTR) { }
}

#endif // OS(DARWIN)

} // namespace Corpse
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(MYA)
