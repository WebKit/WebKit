# Using BoringSSL in a Sandbox

Sandboxes are a valuable tool for securing applications, so BoringSSL aims to
support them. However, it is difficult to make concrete API guarantees with
sandboxes. Sandboxes remove low-level OS resources and system calls, which
breaks platform abstractions. A syscall-filtering sandbox may, for instance, be
sensitive to otherwise non-breaking changes to use newer syscalls
in either BoringSSL or the C library.

Some functions in BoringSSL, such as `BIO_new_file`, inherently need OS
resources like the filesystem. We assume that sandboxed consumers either avoid
those functions or make necessary resources available. Other functions like
`RSA_sign` are purely computational, but still have some baseline OS
dependencies.

Sandboxes which drop privileges partway through a process's lifetime are
additionally sensitive to OS resources retained across the transitions. For
instance, if a library function internally opened and retained a handle to the
user's home directory, and then the application called `chroot`, that handle
would be a sandbox escape.

This document attempts to describe these baseline OS dependencies and long-lived
internal resources. These dependencies may change over time, but we aim to
[work with sandboxed consumers](/BREAKING-CHANGES.md) when they do. However,
each sandbox imposes different constraints, so, above all, sandboxed consumers
must have ample test coverage to detect issues as they arise.

## Baseline dependencies

Callers must assume that any BoringSSL function may perform one of the following
operations:

### Memory allocation

Any BoringSSL function may allocate memory via `malloc` and related functions.

### Thread synchronization

Any BoringSSL function may call into the platform's thread synchronization
primitives, including read/write locks and the equivalent of `pthread_once`.
These must succeed, or BoringSSL will abort the process. Callers, however, can
assume that BoringSSL functions will not spawn internal threads, unless
otherwise documented.

Syscall-filtering sandboxes should note that BoringSSL uses `pthread_rwlock_t`
on POSIX systems, which is less common and may not be part of other libraries'
syscall surface. Additionally, thread synchronization primitives usually have an
atomics-based fast path. If a sandbox blocks a necessary pthreads syscall, it
may not show up in testing without lock contention.

### Standard error

Any BoringSSL function may write to `stderr` or file descriptor
`STDERR_FILENO` (2), either via `FILE` APIs or low-level functions like `write`.
Writes to `stderr` may fail, but there must some file at `STDERR_FILENO` which
will tolerate error messages from BoringSSL. (The file descriptor must be
allocated so calls to `open` do not accidentally open something else there.)

Note some C standard library implementations also log to `stderr`, so callers
should ensure this regardless.

### Entropy

Any BoringSSL function may draw entropy from the OS. On Windows, this uses
`RtlGenRandom` and, on POSIX systems, this uses `getrandom`, `getentropy`, or a
`read` from a file descriptor to `/dev/urandom`. These operations must succeed
or BoringSSL will abort the process. BoringSSL only probes for `getrandom`
support once and assumes support is consistent for the lifetime of the address
space (and any copies made via `fork`). If a syscall-filtering sandbox is
enabled partway through this lifetime and changes whether `getrandom` works,
BoringSSL may abort the process. Sandboxes are recommended to allow
`getrandom`.

Note even deterministic algorithms may require OS entropy. For example,
RSASSA-PKCS1-v1_5 is deterministic, but BoringSSL draws entropy to implement
RSA blinding.

Entropy gathering additionally has some initialization dependencies described in
the following section.

## Initialization

BoringSSL has some uncommon OS dependencies which are only used once to
initialize some state. Sandboxes which drop privileges after some setup work may
use `CRYPTO_pre_sandbox_init` to initialize this state ahead of time. Otherwise,
callers must assume any BoringSSL function may depend on these resources, in
addition to the operations above.

### CPU capabilities

On Linux ARM platforms, BoringSSL depends on OS APIs to query CPU capabilities.
32-bit and 64-bit ARM both depend on the `getauxval` function. 32-bit ARM, to
work around bugs in older Android devices, may additionally read `/proc/cpuinfo`
and `/proc/self/auxv`.

If querying CPU capabilities fails, BoringSSL will still function, but may not
perform as well.

### Entropy

On Linux systems without a working `getrandom`, drawing entropy from the OS
additionally requires opening `/dev/urandom`. If this fails, BoringSSL will
abort the process. BoringSSL retains the resulting file descriptor, even across
privilege transitions.

### Fork protection

On Linux, BoringSSL allocates a page and calls `madvise` with `MADV_WIPEONFORK`
to protect single-use state from `fork`. This operation must not crash, but if
it fails, BoringSSL will use alternate fork-safety strategies, potentially at a
performance cost. If it succeeds, BoringSSL assumes `MADV_WIPEONFORK` is
functional and relies on it for fork-safety. Sandboxes must not report success
if they ignore the `MADV_WIPEONFORK` flag. As of writing, QEMU will ignore
`madvise` calls and report success, so BoringSSL detects this by calling
`madvise` with -1. Sandboxes must cleanly report an error instead of crashing.

Once initialized, this mechanism does not require system calls in the steady
state, though note the configured page will be inherited across privilege
transitions.

## C and C++ standard library

BoringSSL depends on the C and C++ standard libraries which, themselves, do not
make any guarantees about sandboxes. If it produces the correct answer and has
no observable invalid side effects, it is possible, though unreasonable, for
`memcmp` to create and close a socket.

BoringSSL assumes that functions in the C and C++ library only have the platform
dependencies which would be "reasonable". For instance, a function in BoringSSL
which aims not to open files will still freely call any libc memory and
string functions.

Note some C functions, such as `strerror`, may read files relating to the user's
locale. BoringSSL may trigger these paths and assumes the sandbox environment
will tolerate this. BoringSSL additionally cannot make guarantees about which
system calls are used by standard library's syscall wrappers. In some cases, the
compiler may add dependencies. (Some C++ language features emit locking code.)
Syscall-filtering sandboxes may need updates as these dependencies change.
