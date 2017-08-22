// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Author: Shinichiro Hamaji

#include "utilities.h"

#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#include "base/googleinit.h"

using std::string;

_START_GOOGLE_NAMESPACE_

static const char* g_program_invocation_short_name = NULL;
static pthread_t g_main_thread_id;

_END_GOOGLE_NAMESPACE_

_START_GOOGLE_NAMESPACE_

namespace glog_internal_namespace_ {

const char* ProgramInvocationShortName() {
  if (g_program_invocation_short_name != NULL) {
    return g_program_invocation_short_name;
  } else {
    // TODO(hamaji): Use /proc/self/cmdline and so?
    return "UNKNOWN";
  }
}

bool IsGoogleLoggingInitialized() {
  return g_program_invocation_short_name != NULL;
}

bool is_default_thread() {
  if (g_program_invocation_short_name == NULL) {
    // InitGoogleLogging() not yet called, so unlikely to be in a different
    // thread
    return true;
  } else {
    return pthread_equal(pthread_self(), g_main_thread_id);
  }
}

#ifdef OS_WINDOWS
struct timeval {
  long tv_sec, tv_usec;
};

// Based on: http://www.google.com/codesearch/p?hl=en#dR3YEbitojA/os_win32.c&q=GetSystemTimeAsFileTime%20license:bsd
// See COPYING for copyright information.
static int gettimeofday(struct timeval *tv, void* tz) {
#define EPOCHFILETIME (116444736000000000ULL)
  FILETIME ft;
  LARGE_INTEGER li;
  uint64 tt;

  GetSystemTimeAsFileTime(&ft);
  li.LowPart = ft.dwLowDateTime;
  li.HighPart = ft.dwHighDateTime;
  tt = (li.QuadPart - EPOCHFILETIME) / 10;
  tv->tv_sec = tt / 1000000;
  tv->tv_usec = tt % 1000000;

  return 0;
}
#endif

int64 CycleClock_Now() {
  // TODO(hamaji): temporary impementation - it might be too slow.
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return static_cast<int64>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

int64 UsecToCycles(int64 usec) {
  return usec;
}

WallTime WallTime_Now() {
  // Now, cycle clock is retuning microseconds since the epoch.
  return CycleClock_Now() * 0.000001;
}

static int32 g_main_thread_pid = getpid();
int32 GetMainThreadPid() {
  return g_main_thread_pid;
}

bool PidHasChanged() {
  int32 pid = getpid();
  if (g_main_thread_pid == pid) {
    return false;
  }
  g_main_thread_pid = pid;
  return true;
}

pid_t GetTID() {
  return (pid_t)(uintptr_t)pthread_self();
}

const char* const_basename(const char* filepath) {
  const char* base = strrchr(filepath, '/');
#ifdef OS_WINDOWS  // Look for either path separator in Windows
  if (!base)
    base = strrchr(filepath, '\\');
#endif
  return base ? (base+1) : filepath;
}

static string g_my_user_name;
const string& MyUserName() {
  return g_my_user_name;
}
static void MyUserNameInitializer() {
  // TODO(hamaji): Probably this is not portable.
#if defined(OS_WINDOWS)
  const char* user = getenv("USERNAME");
#else
  const char* user = getenv("USER");
#endif
  if (user != NULL) {
    g_my_user_name = user;
  } else {
    g_my_user_name = "invalid-user";
  }
}
REGISTER_MODULE_INITIALIZER(utilities, MyUserNameInitializer());

// We use an atomic operation to prevent problems with calling CrashReason
// from inside the Mutex implementation (potentially through RAW_CHECK).
static const CrashReason* g_reason = 0;

void SetCrashReason(const CrashReason* r) {
  sync_val_compare_and_swap(&g_reason,
                            reinterpret_cast<const CrashReason*>(0),
                            r);
}

void InitGoogleLoggingUtilities(const char* argv0) {
  CHECK(!IsGoogleLoggingInitialized())
      << "You called InitGoogleLogging() twice!";
  const char* slash = strrchr(argv0, '/');
#ifdef OS_WINDOWS
  if (!slash)  slash = strrchr(argv0, '\\');
#endif
  g_program_invocation_short_name = slash ? slash + 1 : argv0;
  g_main_thread_id = pthread_self();

}

void ShutdownGoogleLoggingUtilities() {
  CHECK(IsGoogleLoggingInitialized())
      << "You called ShutdownGoogleLogging() without calling InitGoogleLogging() first!";
  g_program_invocation_short_name = NULL;
}

}  // namespace glog_internal_namespace_

_END_GOOGLE_NAMESPACE_
