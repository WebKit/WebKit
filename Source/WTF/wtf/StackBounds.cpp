/*
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include <wtf/StackBounds.h>

#if OS(DARWIN)

#include <pthread.h>

#elif OS(WINDOWS)

#include <windows.h>

#elif OS(UNIX) || OS(HAIKU)

#include <pthread.h>
#if HAVE(PTHREAD_NP_H)
#include <pthread_np.h>
#endif

#if OS(LINUX)
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#if OS(QNX)
#include <sys/storage.h>
#endif

#endif

namespace WTF {

#if OS(DARWIN)

StackBounds StackBounds::newThreadStackBounds(PlatformThreadHandle thread)
{
    void* origin = pthread_get_stackaddr_np(thread);
    rlim_t size = pthread_get_stacksize_np(thread);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    void* bound = static_cast<char*>(origin) - size;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    return StackBounds { origin, bound };
}

StackBounds StackBounds::currentThreadStackBoundsInternal()
{
    if (pthread_main_np()) {
        // FIXME: <rdar://problem/13741204>
        // pthread_get_size lies to us when we're the main thread, use get_rlimit instead
        void* origin = pthread_get_stackaddr_np(pthread_self());
        rlimit limit;
        getrlimit(RLIMIT_STACK, &limit);
        rlim_t size = limit.rlim_cur;
        if (size == RLIM_INFINITY)
            size = 8 * MB;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        void* bound = static_cast<char*>(origin) - size;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        return StackBounds { origin, bound };
    }
    return newThreadStackBounds(pthread_self());
}

#elif OS(QNX)

StackBounds StackBounds::currentThreadStackBoundsInternal()
{
    struct _thread_local_storage* tls = __tls();
    void* bound = tls->__stackaddr;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    void* origin = static_cast<char*>(bound) + tls->__stacksize;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    return StackBounds { origin, bound };
}

#elif OS(UNIX) || OS(HAIKU)

#if OS(OPENBSD)

StackBounds StackBounds::newThreadStackBounds(PlatformThreadHandle thread)
{
    stack_t stack;
    pthread_stackseg_np(thread, &stack);
    void* origin = stack.ss_sp;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    void* bound = static_cast<char*>(origin) - stack.ss_size;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    return StackBounds { origin, bound };
}

#else // !OS(OPENBSD)

StackBounds StackBounds::newThreadStackBounds(PlatformThreadHandle thread)
{
    void* bound = nullptr;
    size_t stackSize = 0;

    pthread_attr_t sattr;
    pthread_attr_init(&sattr);
#if HAVE(PTHREAD_NP_H) || OS(NETBSD)
    // e.g. on FreeBSD 5.4, neundorf@kde.org
    pthread_attr_get_np(thread, &sattr);
#else
    // FIXME: this function is non-portable; other POSIX systems may have different np alternatives
    pthread_getattr_np(thread, &sattr);
#endif
    int rc = pthread_attr_getstack(&sattr, &bound, &stackSize);
    UNUSED_PARAM(rc);
    ASSERT(bound);
    pthread_attr_destroy(&sattr);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    void* origin = static_cast<char*>(bound) + stackSize;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    // pthread_attr_getstack's bound is the lowest accessible pointer of the stack.
    return StackBounds { origin, bound };
}

#endif // OS(OPENBSD)

StackBounds StackBounds::currentThreadStackBoundsInternal()
{
    auto ret = newThreadStackBounds(pthread_self());
#if OS(LINUX)
    // on glibc, pthread_attr_getstack will generally return the limit size (minus a guard page)
    // for the main thread; this is however not necessarily always true on every libc - for example
    // on musl, it will return the currently reserved size - since the stack bounds are expected to
    // be constant (and they are for every thread except main, which is allowed to grow), check
    // resource limits and use that as the boundary instead (and prevent stack overflows in JSC)
    if (getpid() == static_cast<pid_t>(syscall(SYS_gettid))) {
        void* origin = ret.origin();
        rlimit limit;
        getrlimit(RLIMIT_STACK, &limit);
        rlim_t size = limit.rlim_cur;
        if (size == RLIM_INFINITY)
            size = 8 * MB;
        // account for a guard page
        size -= static_cast<rlim_t>(sysconf(_SC_PAGESIZE));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        void* bound = static_cast<char*>(origin) - size;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

        static char** oldestEnviron = environ;

        // In 32bit architecture, it is possible that environment variables are having a characters which looks like a pointer,
        // and conservative GC will find it as a live pointer. We would like to avoid that to precisely exclude non user stack
        // data region from this stack bounds. As the article (https://lwn.net/Articles/631631/) and the elf loader implementation
        // explain how Linux main thread stack is organized, environment variables vector is placed on the stack, so we can exclude
        // environment variables if we use `environ` global variable as a origin of the stack.
        // But `setenv` / `putenv` may alter `environ` variable's content. So we record the oldest `environ` variable content, and use it.
        StackBounds stackBounds { origin, bound };
        if (stackBounds.contains(oldestEnviron))
            stackBounds = { oldestEnviron, bound };
        return stackBounds;
    }
#endif
    return ret;
}

#elif OS(WINDOWS)

// GetCurrentThreadStackLimits returns OS-maintained stack limits that are:
// - Independent of guard page state
// - Independent of VirtualQuery results
// - Accurate regardless of stack memory layout
//
// This replaces the previous VirtualQuery-based implementation which assumed
// a 3-layer stack structure (uncommitted -> guard -> committed). That approach
// could fail when:
// - Guard pages were consumed by other threads
// - Security software interfered with memory scanning
// - Stacks were fully committed with no uncommitted region
// - Embedded scenarios (e.g., Ruby Bug #11438)
//
// Reference:
// https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadstacklimits

StackBounds StackBounds::currentThreadStackBoundsInternal()
{
    ULONG_PTR lowLimit = 0;
    ULONG_PTR highLimit = 0;
    GetCurrentThreadStackLimits(&lowLimit, &highLimit);

    void* origin = reinterpret_cast<void*>(highLimit);
    void* bound = reinterpret_cast<void*>(lowLimit);
    return StackBounds { origin, bound };
}

#else
#error Need a way to get the stack bounds on this platform
#endif

} // namespace WTF
