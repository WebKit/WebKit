/* Copyright (c) 2019, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <gtest/gtest.h>
#include <stdlib.h>

#include <openssl/rand.h>

#include "internal.h"
#include "getrandom_fillin.h"

#if defined(OPENSSL_X86_64) && !defined(BORINGSSL_SHARED_LIBRARY) && \
    !defined(BORINGSSL_UNSAFE_DETERMINISTIC_MODE) && defined(USE_NR_getrandom)

#include <linux/random.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>

#if !defined(PTRACE_O_EXITKILL)
#define PTRACE_O_EXITKILL (1 << 20)
#endif

#if defined(OPENSSL_NO_ASM)
static int have_rdrand() { return 0; }
#endif

// This test can be run with $OPENSSL_ia32cap=~0x4000000000000000 in order to
// simulate the absence of RDRAND of machines that have it.

// Event represents a system call from urandom.c that is observed by the ptrace
// code in |GetTrace|.
struct Event {
  enum class Syscall {
    kGetRandom,
    kOpen,
    kUrandomRead,
    kUrandomIoctl,
    kAbort,
  };

  explicit Event(Syscall syscall) : type(syscall) {}

  bool operator==(const Event &other) const {
    return type == other.type && length == other.length &&
           flags == other.flags &&
           ((filename == nullptr && other.filename == nullptr) ||
            strcmp(filename, other.filename) == 0);
  }

  static Event GetRandom(size_t length, unsigned flags) {
    Event e(Syscall::kGetRandom);
    e.length = length;
    e.flags = flags;
    return e;
  }

  static Event Open(const char *filename) {
    Event e(Syscall::kOpen);
    e.filename = filename;
    return e;
  }

  static Event UrandomRead(size_t length) {
    Event e(Syscall::kUrandomRead);
    e.length = length;
    return e;
  }

  static Event UrandomIoctl() {
    Event e(Syscall::kUrandomIoctl);
    return e;
  }

  static Event Abort() {
    Event e(Syscall::kAbort);
    return e;
  }

  std::string String() const {
    char buf[256];

    switch (type) {
      case Syscall::kGetRandom:
        snprintf(buf, sizeof(buf), "getrandom(_, %zu, %d)", length, flags);
        break;

      case Syscall::kOpen:
        snprintf(buf, sizeof(buf), "open(%s, _)", filename);
        break;

      case Syscall::kUrandomRead:
        snprintf(buf, sizeof(buf), "read(urandom_fd, _, %zu)", length);
        break;

      case Syscall::kUrandomIoctl:
        return "ioctl(urandom_fd, RNDGETENTCNT, _)";

      case Syscall::kAbort:
        return "abort()";
    }

    return std::string(buf);
  }

  const Syscall type;
  size_t length = 0;
  unsigned flags = 0;
  const char *filename = nullptr;
};

static std::string ToString(const std::vector<Event> &trace) {
  std::string ret;
  for (const auto &event : trace) {
    if (!ret.empty()) {
      ret += ", ";
    }
    ret += event.String();
  }
  return ret;
}

// The following are flags to tell |GetTrace| to inject faults, using ptrace,
// into the entropy-related system calls.

// getrandom gives |ENOSYS|.
static const unsigned NO_GETRANDOM = 1;
// opening /dev/urandom fails.
static const unsigned NO_URANDOM = 2;
// getrandom always returns |EAGAIN| if given |GRNG_NONBLOCK|.
static const unsigned GETRANDOM_NOT_READY = 4;
// The ioctl on urandom returns only 255 bits of entropy the first time that
// it's called.
static const unsigned URANDOM_NOT_READY = 8;
// getrandom gives |EINVAL| unless |NO_GETRANDOM| is set.
static const unsigned GETRANDOM_ERROR = 16;
// Reading from /dev/urandom gives |EINVAL|.
static const unsigned URANDOM_ERROR = 32;
static const unsigned NEXT_FLAG = 64;

// GetTrace runs |thunk| in a forked process and observes the resulting system
// calls using ptrace. It simulates a variety of failures based on the contents
// of |flags| and records the observed events by appending to |out_trace|.
static void GetTrace(std::vector<Event> *out_trace, unsigned flags,
                     std::function<void()> thunk) {
  const int child_pid = fork();
  ASSERT_NE(-1, child_pid);

  if (child_pid == 0) {
    // Child process
    if (ptrace(PTRACE_TRACEME, 0, 0, 0) != 0) {
      perror("PTRACE_TRACEME");
      _exit(1);
    }
    raise(SIGSTOP);
    thunk();
    _exit(0);
  }

  // Parent process
  int status;
  ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));
  ASSERT_TRUE(WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP);

  // Set options so that:
  //   a) the child process is killed once this process dies.
  //   b) System calls result in a WSTOPSIG value of (SIGTRAP | 0x80) rather
  //      than just SIGTRAP. (This doesn't matter here, but it's recommended
  //      practice so that it's distinct from the signal itself.)
  ASSERT_EQ(0, ptrace(PTRACE_SETOPTIONS, child_pid, nullptr,
                      PTRACE_O_EXITKILL | PTRACE_O_TRACESYSGOOD))
      << strerror(errno);

  // urandom_fd tracks the file descriptor number for /dev/urandom in the child
  // process, if it opens it.
  int urandom_fd = -1;

  for (;;) {
    // Advance the child to the next system call.
    ASSERT_EQ(0, ptrace(PTRACE_SYSCALL, child_pid, 0, 0));
    ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));

    // The child may have aborted rather than made a system call.
    if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGABRT) {
      out_trace->push_back(Event::Abort());
      break;
    }

    // Otherwise the only valid ptrace event is a system call stop.
    ASSERT_TRUE(WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80));

    struct user_regs_struct regs;
    ASSERT_EQ(0, ptrace(PTRACE_GETREGS, child_pid, nullptr, &regs));
    const auto syscall_number = regs.orig_rax;

    bool is_opening_urandom = false;
    bool is_urandom_ioctl = false;
    uintptr_t ioctl_output_addr = 0;
    // inject_error is zero to indicate that the system call should run
    // normally. Otherwise it's, e.g. -EINVAL, to indicate that the system call
    // should not run and that error should be injected on return.
    int inject_error = 0;

    switch (syscall_number) {
      case __NR_getrandom:
        if (flags & NO_GETRANDOM) {
          inject_error = -ENOSYS;
        } else if (flags & GETRANDOM_ERROR) {
          inject_error = -EINVAL;
        } else if (flags & GETRANDOM_NOT_READY) {
          if (regs.rdx & GRND_NONBLOCK) {
            inject_error = -EAGAIN;
          }
        }
        out_trace->push_back(
            Event::GetRandom(/*length=*/regs.rsi, /*flags=*/regs.rdx));
        break;

      case __NR_openat:
      case __NR_open: {
        // It's assumed that any arguments to open(2) are constants in read-only
        // memory and thus the pointer in the child's context will also be a
        // valid pointer in our address space.
        const char *filename = reinterpret_cast<const char *>(
            (syscall_number == __NR_openat) ? regs.rsi : regs.rdi);
        out_trace->push_back(Event::Open(filename));
        is_opening_urandom = strcmp(filename, "/dev/urandom") == 0;
        if (is_opening_urandom && (flags & NO_URANDOM)) {
          inject_error = -ENOENT;
        }
        break;
      }

      case __NR_read: {
        const int read_fd = regs.rdi;
        if (urandom_fd >= 0 && urandom_fd == read_fd) {
          out_trace->push_back(Event::UrandomRead(/*length=*/regs.rdx));
          if (flags & URANDOM_ERROR) {
            inject_error = -EINVAL;
          }
        }
        break;
      }

      case __NR_ioctl: {
        const int ioctl_fd = regs.rdi;
        if (urandom_fd >= 0 && ioctl_fd == urandom_fd &&
            regs.rsi == RNDGETENTCNT) {
          out_trace->push_back(Event::UrandomIoctl());
          is_urandom_ioctl = true;
          ioctl_output_addr = regs.rdx;
        }
      }
    }

    if (inject_error) {
      // Replace the system call number with -1 to cause the kernel to ignore
      // the call. The -ENOSYS will be replaced later with the value of
      // |inject_error|.
      regs.orig_rax = -1;
      ASSERT_EQ(0, ptrace(PTRACE_SETREGS, child_pid, nullptr, &regs));
    }

    ASSERT_EQ(0, ptrace(PTRACE_SYSCALL, child_pid, 0, 0));
    ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));
    // If the system call was exit/exit_group, the process may be terminated
    // rather than have exited the system call.
    if (WIFEXITED(status)) {
      ASSERT_EQ(0, WEXITSTATUS(status));
      return;
    }

    // Otherwise the next state must be a system call exit stop. This is
    // indistinguishable from a system call entry, we just have to keep track
    // and know that these events happen in pairs.
    ASSERT_TRUE(WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80));

    if (inject_error) {
      if (inject_error != -ENOSYS) {
        ASSERT_EQ(0, ptrace(PTRACE_GETREGS, child_pid, nullptr, &regs));
        regs.rax = inject_error;
        ASSERT_EQ(0, ptrace(PTRACE_SETREGS, child_pid, nullptr, &regs));
      }
    } else if (is_opening_urandom) {
      ASSERT_EQ(0, ptrace(PTRACE_GETREGS, child_pid, nullptr, &regs));
      urandom_fd = regs.rax;
    } else if (is_urandom_ioctl) {
      // The result is the number of bits of entropy that the kernel currently
      // believes that it has. urandom.c waits until 256 bits are ready.
      int result = 256;

      // If we are simulating urandom not being ready then we have the ioctl
      // indicate one too few bits of entropy the first time it's queried.
      if (flags & URANDOM_NOT_READY) {
        result--;
        flags &= ~URANDOM_NOT_READY;
      }

      // ptrace always works with ill-defined "words", which appear to be 64-bit
      // on x86-64. Since the ioctl result is a 32-bit int, do a
      // read-modify-write to inject the answer.
      const uintptr_t aligned_addr = ioctl_output_addr & ~7;
      const uintptr_t offset = ioctl_output_addr - aligned_addr;
      union {
        uint64_t word;
        uint8_t bytes[8];
      } u;
      u.word = ptrace(PTRACE_PEEKDATA, child_pid,
                      reinterpret_cast<void *>(aligned_addr), nullptr);
      memcpy(&u.bytes[offset], &result, sizeof(result));
      ASSERT_EQ(0, ptrace(PTRACE_POKEDATA, child_pid,
                          reinterpret_cast<void *>(aligned_addr),
                          reinterpret_cast<void *>(u.word)));
    }
  }
}

// TestFunction is the function that |GetTrace| is asked to trace.
static void TestFunction() {
  uint8_t byte;
  RAND_bytes(&byte, sizeof(byte));
  RAND_bytes(&byte, sizeof(byte));
}

// TestFunctionPRNGModel is a model of how the urandom.c code will behave when
// |TestFunction| is run. It should return the same trace of events that
// |GetTrace| will observe the real code making.
static std::vector<Event> TestFunctionPRNGModel(unsigned flags) {
#if defined(BORINGSSL_FIPS)
  static const bool is_fips = true;
#else
  static const bool is_fips = false;
#endif

  std::vector<Event> ret;
  bool urandom_probed = false;
  bool getrandom_ready = false;

  // Probe for getrandom support
  ret.push_back(Event::GetRandom(1, GRND_NONBLOCK));
  std::function<void()> wait_for_entropy;
  std::function<bool(bool, size_t)> sysrand;

  if (flags & NO_GETRANDOM) {
    ret.push_back(Event::Open("/dev/urandom"));
    if (flags & NO_URANDOM) {
      ret.push_back(Event::Abort());
      return ret;
    }

    wait_for_entropy = [&ret, &urandom_probed, flags] {
      if (!is_fips || urandom_probed) {
        return;
      }

      // Probe urandom for entropy.
      ret.push_back(Event::UrandomIoctl());
      if (flags & URANDOM_NOT_READY) {
        // If the first attempt doesn't report enough entropy, probe
        // repeatedly until it does, which will happen with the second attempt.
        ret.push_back(Event::UrandomIoctl());
      }

      urandom_probed = true;
    };

    sysrand = [&ret, &wait_for_entropy, flags](bool block, size_t len) {
      if (block) {
        wait_for_entropy();
      }
      ret.push_back(Event::UrandomRead(len));
      if (flags & URANDOM_ERROR) {
        ret.push_back(Event::Abort());
        return false;
      }
      return true;
    };
  } else {
    if (flags & GETRANDOM_ERROR) {
      ret.push_back(Event::Abort());
      return ret;
    }

    getrandom_ready = (flags & GETRANDOM_NOT_READY) == 0;
    wait_for_entropy = [&ret, &getrandom_ready] {
      if (getrandom_ready) {
        return;
      }

      ret.push_back(Event::GetRandom(1, GRND_NONBLOCK));
      ret.push_back(Event::GetRandom(1, 0));
      getrandom_ready = true;
    };
    sysrand = [&ret, &wait_for_entropy](bool block, size_t len) {
      if (block) {
        wait_for_entropy();
      }
      ret.push_back(Event::GetRandom(len, block ? 0 : GRND_NONBLOCK));
      return true;
    };
  }

  const size_t kSeedLength = CTR_DRBG_ENTROPY_LEN * (is_fips ? 10 : 1);
  const size_t kAdditionalDataLength = 32;

  if (!have_rdrand()) {
    if (!sysrand(true, kAdditionalDataLength) ||
        // Initialise CRNGT.
        (is_fips && !sysrand(true, 16)) ||
        !sysrand(true, kSeedLength) ||
        // Second entropy draw.
        !sysrand(true, kAdditionalDataLength)) {
      return ret;
    }
  } else {
    // Opportuntistic entropy draw in FIPS mode because RDRAND was used.
    // In non-FIPS mode it's just drawn from |CRYPTO_sysrand| in a blocking
    // way.
    if (!sysrand(!is_fips, CTR_DRBG_ENTROPY_LEN)) {
      return ret;
    }
  }

  return ret;
}

// Tests that |TestFunctionPRNGModel| is a correct model for the code in
// urandom.c, at least to the limits of the the |Event| type.
TEST(URandomTest, Test) {
  char buf[256];

#define TRACE_FLAG(flag)                                         \
  snprintf(buf, sizeof(buf), #flag ": %d", (flags & flag) != 0); \
  SCOPED_TRACE(buf);

  for (unsigned flags = 0; flags < NEXT_FLAG; flags++) {
    TRACE_FLAG(NO_GETRANDOM);
    TRACE_FLAG(NO_URANDOM);
    TRACE_FLAG(GETRANDOM_NOT_READY);
    TRACE_FLAG(URANDOM_NOT_READY);
    TRACE_FLAG(GETRANDOM_ERROR);
    TRACE_FLAG(URANDOM_ERROR);

    const std::vector<Event> expected_trace = TestFunctionPRNGModel(flags);
    std::vector<Event> actual_trace;
    GetTrace(&actual_trace, flags, TestFunction);

    if (expected_trace != actual_trace) {
      ADD_FAILURE() << "Expected: " << ToString(expected_trace)
                    << "\nFound:    " << ToString(actual_trace);
    }
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#else

int main(int argc, char **argv) {
  printf("PASS\n");
  return 0;
}

#endif  // X86_64 && !SHARED_LIBRARY && !UNSAFE_DETERMINISTIC_MODE &&
        // USE_NR_getrandom
