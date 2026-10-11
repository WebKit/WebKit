#!/usr/bin/env python3
"""Apply build-only Wii U compatibility edits to the temporary vendored WebRTC checkout."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


def patch_file(root: Path, relative_path: str, replacements: list[tuple[str, str]]) -> bool:
    path = root / relative_path
    if not path.is_file():
        raise FileNotFoundError(f"Expected WebRTC source file is missing: {relative_path}")
    original = path.read_text(encoding="utf-8")
    updated = original
    for old, new in replacements:
        if new in updated:
            continue
        if old not in updated:
            raise RuntimeError(
                f"Could not find expected patch anchor in {relative_path}: {old[:100]!r}"
            )
        updated = updated.replace(old, new, 1)
    if updated == original:
        print(f"ALREADY PATCHED {relative_path}")
        return False
    path.write_text(updated, encoding="utf-8", newline="\n")
    print(f"PATCHED {relative_path}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    root = parser.parse_args().repo_root.resolve()
    vendor = Path("Source/ThirdParty/libwebrtc/Source")

    patches = [
        (
            "third_party/abseil-cpp/absl/debugging/internal/elf_mem_image.h",
            [(
                "#if defined(__ELF__) && !defined(__OpenBSD__)",
                "#if defined(__ELF__) && !defined(WKC_WIIU_WEBRTC) && !defined(__OpenBSD__)",
            )],
        ),
        (
            "third_party/abseil-cpp/absl/debugging/failure_signal_handler.cc",
            [(
                "#if !defined(_WIN32) && !defined(__wasi__)",
                "#if !defined(_WIN32) && !defined(__wasi__) && !defined(WKC_WIIU_WEBRTC)",
            )],
        ),
        (
            "third_party/abseil-cpp/absl/time/internal/cctz/src/time_zone_libc.cc",
            [
                (
                    "namespace absl {\nABSL_NAMESPACE_BEGIN\nnamespace time_internal {\nnamespace cctz {",
                    """#if defined(WKC_WIIU_WEBRTC)
extern "C" {
extern long _timezone;
extern char *_tzname[2];
}
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace time_internal {
namespace cctz {""",
                ),
                (
                    "#elif defined(__native_client__) || defined(__myriad2__) ||",
                    """#elif defined(WKC_WIIU_WEBRTC)
// Newlib omits tm_gmtoff/tm_zone in struct tm. Its _timezone uses seconds
// west of UTC, while cctz expects seconds east of UTC.
auto tm_gmtoff(const std::tm& tm) -> decltype(_timezone + 0) {
  return -_timezone + (tm.tm_isdst > 0 ? 60L * 60L : 0L);
}
auto tm_zone(const std::tm& tm) -> decltype(_tzname[0]) {
  return _tzname[tm.tm_isdst > 0];
}
#elif defined(__native_client__) || defined(__myriad2__) ||""",
                ),
            ],
        ),
        (
            "third_party/abseil-cpp/absl/base/internal/sysinfo.cc",
            [(
                "#else\n\n// Fallback implementation of",
                """#elif defined(WKC_WIIU_WEBRTC)

// Wii U pthread_t is a pointer-valued handle, not a numeric pid_t.
pid_t GetTID() {
  return static_cast<pid_t>(reinterpret_cast<unsigned long>(pthread_self()));
}

#else

// Fallback implementation of""",
            )],
        ),
        (
            "webrtc/rtc_base/cpu_time.cc",
            [
                (
                    '#else\n  // Not implemented yet.\n  static_assert(\n      false, "GetProcessCpuTimeNanos() platform support not yet implemented.");\n#endif',
                    '#elif defined(WKC_WIIU_WEBRTC)\n  // The Wii U SDK does not expose process CPU time; metrics are optional.\n  return -1;\n#else\n  // Not implemented yet.\n  static_assert(\n      false, "GetProcessCpuTimeNanos() platform support not yet implemented.");\n#endif',
                ),
                (
                    '#else\n  // Not implemented yet.\n  static_assert(\n      false, "GetThreadCpuTimeNanos() platform support not yet implemented.");\n#endif',
                    '#elif defined(WKC_WIIU_WEBRTC)\n  // The Wii U SDK does not expose thread CPU time; metrics are optional.\n  return -1;\n#else\n  // Not implemented yet.\n  static_assert(\n      false, "GetThreadCpuTimeNanos() platform support not yet implemented.");\n#endif',
                ),
            ],
        ),
        (
            "webrtc/rtc_base/memory_usage.cc",
            [(
                '#else\n  // Not implemented yet.\n  static_assert(false,\n                "GetProcessVirtualMemoryUsageBytes() platform support not yet "\n                "implemented.");\n#endif',
                '#elif defined(WKC_WIIU_WEBRTC)\n  // The Wii U SDK does not expose process resident-memory metrics.\n  return -1;\n#else\n  // Not implemented yet.\n  static_assert(false,\n                "GetProcessVirtualMemoryUsageBytes() platform support not yet "\n                "implemented.");\n#endif',
            )],
        ),
        (
            "third_party/boringssl/src/crypto/fipsmodule/bcm.cc",
            [(
                '#include "bcm_interface.h"\n',
                '''#include "bcm_interface.h"

#if defined(WKC_WIIU_WEBRTC)
// Wii U Espresso has no AES instruction-set acceleration. Use BoringSSL's
// existing software AES fallback rather than probing for hardware AES.
namespace bssl {
inline bool hwaes_capable() { return false; }
}  // namespace bssl
#endif
''',
            )],
        ),
        (
            "third_party/abseil-cpp/absl/random/internal/nanobenchmark.cc",
            [
                (
                    "#if defined(ABSL_ARCH_PPC)\n#include <sys/platform/ppc.h>  // NOLINT\n#endif",
                    "#if defined(ABSL_ARCH_PPC) && !defined(WKC_WIIU_WEBRTC)\n#include <sys/platform/ppc.h>  // NOLINT\n#endif",
                ),
                (
                    "#if defined(ABSL_ARCH_ARM) || defined(ABSL_ARCH_AARCH64)\n#include <time.h>  // NOLINT\n#endif",
                    "#if defined(ABSL_ARCH_ARM) || defined(ABSL_ARCH_AARCH64) || defined(WKC_WIIU_WEBRTC)\n#include <time.h>  // NOLINT\n#endif",
                ),
                (
                    "#if defined(ABSL_ARCH_PPC)\n  asm volatile(\"mfspr %0, %1\" : \"=r\"(t) : \"i\"(268));",
                    "#if defined(ABSL_ARCH_PPC) && !defined(WKC_WIIU_WEBRTC)\n  asm volatile(\"mfspr %0, %1\" : \"=r\"(t) : \"i\"(268));",
                ),
                (
                    "#if defined(ABSL_ARCH_PPC)\n  return __ppc_get_timebase_freq();",
                    "#if defined(ABSL_ARCH_PPC) && !defined(WKC_WIIU_WEBRTC)\n  return __ppc_get_timebase_freq();",
                ),
            ],
        ),
        (
            "webrtc/rtc_base/physical_socket_server.cc",
            [
                (
                    "  int received = 0;\n  iovec iov = {.iov_base = buffer, .iov_len = length};\n  msghdr msg = {.msg_iov = &iov, .msg_iovlen = 1};",
                    """#if defined(WKC_WIIU_WEBRTC)
  // devkitPPC/newlib exposes recv/recvfrom but not recvmsg ancillary data.
  // Preserve IPv4 payload/address reads; timestamp and ECN metadata are unavailable.
  if (out_addr)
    out_addr->Clear();
  if (timestamp)
    *timestamp = -1;
  if (ecn)
    *ecn = EcnMarking::kNotEct;
  const int received = out_addr
      ? ::recvfrom(s_, buffer, length, 0, addr, &addr_len)
      : ::recv(s_, buffer, length, 0);
  if (received > 0 && out_addr)
    SocketAddressFromSockAddrStorage(addr_storage, out_addr);
  return received;
#else
  int received = 0;
  iovec iov = {.iov_base = buffer, .iov_len = length};
  msghdr msg = {.msg_iov = &iov, .msg_iovlen = 1};""",
                ),
                (
                    "  return received;\n\n#else\n  int received = 0;",
                    "  return received;\n#endif // WKC_WIIU_WEBRTC\n\n#else\n  int received = 0;",
                ),
            ],
        ),
    ]

    changed = 0
    for relative_path, replacements in patches:
        changed += patch_file(root, str(vendor / relative_path), replacements)

    # In this WebKit snapshot LibWebRTCUtils.h is not tracked. The only use in
    # LibWebRTCProvider.cpp was for toRef(), already supplied by the included
    # LibWebRTCProvider.h -> LibWebRTCRefWrappers.h chain. Remove the stale
    # include in the temporary build checkout; never modify upstream files.
    changed += patch_file(
        root,
        "Source/WebCore/platform/mediastream/libwebrtc/LibWebRTCProvider.cpp",
        [
            (
                '#include "LibWebRTCUtils.h"\n',
                '// WKC patch: toRef() is provided by LibWebRTCRefWrappers.h.\n',
            ),
            (
                '#include <dlfcn.h>\n',
                '// WKC patch: dlfcn.h is not needed by this provider on Wii U.\n',
            ),
        ],
    )

    print(f"Wii U WebRTC compatibility patches complete ({changed} files changed).")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Wii U WebRTC compatibility patch failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
