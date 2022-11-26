//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// global_state.cpp : Implements functions for querying the thread-local GL and EGL state.

#include "libGLESv2/global_state.h"

#include "common/debug.h"
#include "common/platform.h"
#include "common/system_utils.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/Thread.h"
#include "libGLESv2/resource.h"

#include <atomic>
#if defined(ANGLE_PLATFORM_APPLE)
#    include <dispatch/dispatch.h>
#endif
namespace egl
{
namespace
{
ANGLE_REQUIRE_CONSTANT_INIT std::atomic<angle::GlobalMutex *> g_Mutex(nullptr);
static_assert(std::is_trivially_destructible<decltype(g_Mutex)>::value,
              "global mutex is not trivially destructible");
ANGLE_REQUIRE_CONSTANT_INIT std::atomic<angle::GlobalMutex *> g_SurfaceMutex(nullptr);
static_assert(std::is_trivially_destructible<decltype(g_SurfaceMutex)>::value,
              "global mutex is not trivially destructible");

ANGLE_REQUIRE_CONSTANT_INIT gl::Context *g_LastContext(nullptr);
static_assert(std::is_trivially_destructible<decltype(g_LastContext)>::value,
              "global last context is not trivially destructible");

void SetContextToAndroidOpenGLTLSSlot(gl::Context *value)
{
#if defined(ANGLE_USE_ANDROID_TLS_SLOT)
    if (angle::gUseAndroidOpenGLTlsSlot)
    {
        ANGLE_ANDROID_GET_GL_TLS()[angle::kAndroidOpenGLTlsSlot] = static_cast<void *>(value);
    }
#endif
}

// Called only on Android platform
[[maybe_unused]] void ThreadCleanupCallback(void *ptr)
{
    ANGLE_SCOPED_GLOBAL_LOCK();
    angle::PthreadKeyDestructorCallback(ptr);
}

Thread *AllocateCurrentThread()
{
    Thread *thread;
    {
        // Global thread intentionally leaked
        ANGLE_SCOPED_DISABLE_LSAN();
        thread = new Thread();
#if defined(ANGLE_PLATFORM_APPLE)
        SetCurrentThreadTLS(thread);
#else
        gCurrentThread = thread;
#endif
    }

    // Initialize fast TLS slot
    SetContextToAndroidOpenGLTLSSlot(nullptr);

#if defined(ANGLE_PLATFORM_APPLE)
    gl::SetCurrentValidContextTLS(nullptr);
#else
    gl::gCurrentValidContext = nullptr;
#endif

#if defined(ANGLE_PLATFORM_ANDROID)
    static pthread_once_t keyOnce          = PTHREAD_ONCE_INIT;
    static TLSIndex gThreadCleanupTLSIndex = TLS_INVALID_INDEX;

    // Create thread cleanup TLS slot
    auto CreateThreadCleanupTLSIndex = []() {
        gThreadCleanupTLSIndex = CreateTLSIndex(ThreadCleanupCallback);
    };
    pthread_once(&keyOnce, CreateThreadCleanupTLSIndex);
    ASSERT(gThreadCleanupTLSIndex != TLS_INVALID_INDEX);

    // Initialize thread cleanup TLS slot
    SetTLSValue(gThreadCleanupTLSIndex, thread);
#endif  // ANGLE_PLATFORM_ANDROID

    ASSERT(thread);
    return thread;
}

void AllocateGlobalMutex(std::atomic<angle::GlobalMutex *> &mutex)
{
    if (mutex == nullptr)
    {
        std::unique_ptr<angle::GlobalMutex> newMutex(new angle::GlobalMutex());
        angle::GlobalMutex *expected = nullptr;
        if (mutex.compare_exchange_strong(expected, newMutex.get()))
        {
            newMutex.release();
        }
    }
}

void AllocateMutex()
{
    AllocateGlobalMutex(g_Mutex);
}

void AllocateSurfaceMutex()
{
    AllocateGlobalMutex(g_SurfaceMutex);
}

}  // anonymous namespace

#if defined(ANGLE_PLATFORM_APPLE)
// TODO(angleproject:6479): Due to a bug in Apple's dyld loader, `thread_local` will cause
// excessive memory use. Temporarily avoid it by using pthread's thread
// local storage instead.
// https://bugs.webkit.org/show_bug.cgi?id=228240

static TLSIndex GetCurrentThreadTLSIndex()
{
    static TLSIndex CurrentThreadIndex = TLS_INVALID_INDEX;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      ASSERT(CurrentThreadIndex == TLS_INVALID_INDEX);
      CurrentThreadIndex = CreateTLSIndex(nullptr);
    });
    return CurrentThreadIndex;
}
Thread *GetCurrentThreadTLS()
{
    TLSIndex CurrentThreadIndex = GetCurrentThreadTLSIndex();
    ASSERT(CurrentThreadIndex != TLS_INVALID_INDEX);
    return static_cast<Thread *>(GetTLSValue(CurrentThreadIndex));
}
void SetCurrentThreadTLS(Thread *thread)
{
    TLSIndex CurrentThreadIndex = GetCurrentThreadTLSIndex();
    ASSERT(CurrentThreadIndex != TLS_INVALID_INDEX);
    SetTLSValue(CurrentThreadIndex, thread);
}
#else
thread_local Thread *gCurrentThread = nullptr;
#endif

angle::GlobalMutex &GetGlobalMutex()
{
    AllocateMutex();
    return *g_Mutex;
}

angle::GlobalMutex &GetGlobalSurfaceMutex()
{
    AllocateSurfaceMutex();
    return *g_SurfaceMutex;
}

gl::Context *GetGlobalLastContext()
{
    return g_LastContext;
}

void SetGlobalLastContext(gl::Context *context)
{
    g_LastContext = context;
}

// This function causes an MSAN false positive, which is muted. See https://crbug.com/1211047
// It also causes a flaky false positive in TSAN. http://crbug.com/1223970
ANGLE_NO_SANITIZE_MEMORY ANGLE_NO_SANITIZE_THREAD Thread *GetCurrentThread()
{
#if defined(ANGLE_PLATFORM_APPLE)
    Thread *current = GetCurrentThreadTLS();
#else
    Thread *current = gCurrentThread;
#endif
    return (current ? current : AllocateCurrentThread());
}

void SetContextCurrent(Thread *thread, gl::Context *context)
{
#if defined(ANGLE_PLATFORM_APPLE)
    Thread *currentThread = GetCurrentThreadTLS();
#else
    Thread *currentThread = gCurrentThread;
#endif
    ASSERT(currentThread);
    currentThread->setCurrent(context);
    SetContextToAndroidOpenGLTLSSlot(context);

#if defined(ANGLE_PLATFORM_APPLE)
    gl::SetCurrentValidContextTLS(context);
#else
    gl::gCurrentValidContext = context;
#endif

#if defined(ANGLE_FORCE_CONTEXT_CHECK_EVERY_CALL)
    DirtyContextIfNeeded(context);
#endif
}

ScopedSyncCurrentContextFromThread::ScopedSyncCurrentContextFromThread(egl::Thread *thread)
    : mThread(thread)
{
    ASSERT(mThread);
}

ScopedSyncCurrentContextFromThread::~ScopedSyncCurrentContextFromThread()
{
    SetContextCurrent(mThread, mThread->getContext());
}

}  // namespace egl

namespace gl
{
void GenerateContextLostErrorOnContext(Context *context)
{
    if (context && context->isContextLost())
    {
        context->validationError(angle::EntryPoint::Invalid, GL_CONTEXT_LOST, err::kContextLost);
    }
}

void GenerateContextLostErrorOnCurrentGlobalContext()
{
    // If the client starts issuing GL calls before ANGLE has had a chance to initialize,
    // GenerateContextLostErrorOnCurrentGlobalContext can be called before AllocateCurrentThread has
    // had a chance to run. Calling GetCurrentThread() ensures that TLS thread state is set up.
    egl::GetCurrentThread();

    GenerateContextLostErrorOnContext(GetGlobalContext());
}
}  // namespace gl

#if defined(ANGLE_PLATFORM_WINDOWS) && !defined(ANGLE_STATIC)
namespace egl
{

namespace
{

void DeallocateGlobalMutex(std::atomic<angle::GlobalMutex *> &mutex)
{
    angle::GlobalMutex *toDelete = mutex.exchange(nullptr);
    if (!mutex)
        return;
    {
        // Wait for toDelete to become released by other threads before deleting.
        std::lock_guard<angle::GlobalMutex> lock(*toDelete);
    }
    SafeDelete(toDelete);
}

void DeallocateCurrentThread()
{
    SafeDelete(gCurrentThread);
}

void DeallocateMutex()
{
    DeallocateGlobalMutex(g_Mutex);
}

void DeallocateSurfaceMutex()
{
    DeallocateGlobalMutex(g_SurfaceMutex);
}

bool InitializeProcess()
{
    EnsureDebugAllocated();
    AllocateMutex();
    return AllocateCurrentThread() != nullptr;
}

void TerminateProcess()
{
    DeallocateDebug();
    DeallocateSurfaceMutex();
    DeallocateMutex();
    DeallocateCurrentThread();
}

}  // anonymous namespace

}  // namespace egl

namespace
{
// The following WaitForDebugger code is based on SwiftShader. See:
// https://cs.chromium.org/chromium/src/third_party/swiftshader/src/Vulkan/main.cpp
#    if defined(ANGLE_ENABLE_ASSERTS) && !defined(ANGLE_ENABLE_WINDOWS_UWP)
INT_PTR CALLBACK DebuggerWaitDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rect;

    switch (uMsg)
    {
        case WM_INITDIALOG:
            ::GetWindowRect(GetDesktopWindow(), &rect);
            ::SetWindowPos(hwnd, HWND_TOP, rect.right / 2, rect.bottom / 2, 0, 0, SWP_NOSIZE);
            ::SetTimer(hwnd, 1, 100, NULL);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDCANCEL)
            {
                ::EndDialog(hwnd, 0);
            }
            break;
        case WM_TIMER:
            if (angle::IsDebuggerAttached())
            {
                ::EndDialog(hwnd, 0);
            }
    }

    return FALSE;
}

void WaitForDebugger(HINSTANCE instance)
{
    if (angle::IsDebuggerAttached())
        return;

    HRSRC dialog = ::FindResourceA(instance, MAKEINTRESOURCEA(IDD_DIALOG1), MAKEINTRESOURCEA(5));
    if (!dialog)
    {
        printf("Error finding wait for debugger dialog. Error %lu.\n", ::GetLastError());
        return;
    }

    DLGTEMPLATE *dialogTemplate = reinterpret_cast<DLGTEMPLATE *>(::LoadResource(instance, dialog));
    ::DialogBoxIndirectA(instance, dialogTemplate, NULL, DebuggerWaitDialogProc);
}
#    else
void WaitForDebugger(HINSTANCE instance) {}
#    endif  // defined(ANGLE_ENABLE_ASSERTS) && !defined(ANGLE_ENABLE_WINDOWS_UWP)
}  // namespace

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            if (angle::GetEnvironmentVar("ANGLE_WAIT_FOR_DEBUGGER") == "1")
            {
                WaitForDebugger(instance);
            }
            return static_cast<BOOL>(egl::InitializeProcess());

        case DLL_THREAD_ATTACH:
            return static_cast<BOOL>(egl::AllocateCurrentThread() != nullptr);

        case DLL_THREAD_DETACH:
            egl::DeallocateCurrentThread();
            break;

        case DLL_PROCESS_DETACH:
            egl::TerminateProcess();
            break;
    }

    return TRUE;
}
#endif  // defined(ANGLE_PLATFORM_WINDOWS) && !defined(ANGLE_STATIC)
