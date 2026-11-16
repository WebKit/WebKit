<!-- go/cmark -->

<!--* freshness: {owner: 'boivie' owner: 'danilchap' reviewed: '2026-09-10'} *-->

# Rust in WebRTC

## Current status

WebRTC is adopting [Rust] to advance memory safety. Rust targets and build
infrastructure have been integrated into the repository and are enabled by
default.

Currently there is a Rust component integrated into production code with a c++
fallback.

## rtc_rust build flag

Rust support is controlled by the GN build argument `rtc_rust` in `webrtc.gni`:

```gn
# Enables or disables Rust targets in WebRTC.
rtc_rust = true
```

C++ code can check if Rust is enabled by checking `WEBRTC_WITHOUT_RUST` define.

We plan to eventually remove the `rtc_rust` build flag and `WEBRTC_WITHOUT_RUST`
define and require all downstream builds to provide a working Rust toolchain.

## GN Templates

- **`rtc_rust_library`**: Defines a Rust static library.
- **`rtc_rust_cxx_bridge`**: Defines a single-source C++/Rust FFI bridge using
  the [CXX] crate.
- **`rtc_rust_unittest`**: Defines a single Rust unit test binary.
- **`rtc_rust_test_suite`**: Aggregates multiple Rust unit test targets into a
  test suite group.

## Style guide

With few exceptions, WebRTC follows
[Chromium Rust Style guide][rust-chromium-style-guide], which in turn follows
[Rust Style Guide][rust-style-guide] and [Rust API Guidelines][rust-api-guide].

### Use webrtc::import! macro

```rust
webrtc::import! {
  // Crates can be imported using their target name (e.g. `time_delta_rs`):
  "//api/unit:time_delta_rs";

  // Target names can be overridden for brevity:
  "//modules/rtp_rtcp:corruption_detection_extension" as cde;
}
```

### Use Rust standard test framework

See [Rust Book][rust-test] for the guidance.

Support for [Chromium's test framework][rust-chromium-test] or
[Googles test framework][rust-google-test] is currently not implemented.

## C++/Rust Interoperability

Interoperability between C++ and Rust is handled through the [CXX] crate.

We plan to switch to [Crubit] when its support will be implemented in more
environments, in particular when it will be fully
[supported][crubit-in-chromium-issue] by chromium infrastructure.

FFI bindings are declared in Rust source files using `#[cxx::bridge]`. Use C++
namespace `webrtc` for all bindings. Declare bindings in Rust mod `ffi`.

**Examples:**

```rust
// bindings to call C++ from Rust

#[cxx::bridge(namespace = "webrtc")]
mod ffi {
    unsafe extern "C++" {
        include!("path/to/a_cpp_library.h");

        fn CppFunction(value: i32) -> i64;
    }
}

pub fn cpp_function(value: i32) -> i64 {
    ffi::CppFunction(value)
}
```

```rust
// bindings to call Rust from C++

webrtc::import! {
    "//path/to/rust:a_rust_library";
}

use a_rust_library::rust_function;

#[cxx::bridge(namespace = "webrtc")]
mod ffi {
    extern "Rust" {
        fn rust_function(value: i64) -> u64;
    }
}
```

```gn
import("../../webrtc.gni")

rtc_library("cpp_only_library") {
  sources = [
    "cpp_only_library.cc",
    "cpp_only_library.h",
  ]
}

if (rtc_rust) {
  rtc_rust_cxx_bridge("call_cpp_from_rust_cxx") {
    allow_unsafe = true
    source = "call_cpp_from_rust_cxx.rs"
    deps = [
      ":cpp_only_library",
      "//build/rust:cxx_rustdeps",
    ]
  }

  rtc_rust_library("my_rust_feature") {
    crate_root = "my_rust_feature.rs"
    sources = [ "my_rust_feature.rs" ]
    deps = [
      ":call_cpp_from_rust_cxx",
    ]
  }

  rtc_rust_unittest("my_rust_feature_test") {
    crate_root = "my_rust_feature.rs"
    sources = [ "my_rust_feature.rs" ]
    deps = [
      ":call_cpp_from_rust_cxx",
    ]
  }

  rtc_rust_cxx_bridge("call_rust_from_cpp_cxx") {
    allow_unsafe = true
    source = "call_rust_from_cpp_cxx.rs"
    deps = [
      ":my_rust_feature",
      "//build/rust:cxx_rustdeps",
    ]
  }
}

rtc_library("cpp_maybe_use_rust") {
  sources = [
    "cpp_maybe_use_rust.cc"
  ]
  deps = [...]

  if (rtc_rust) {
    deps += [":call_rust_from_cpp_cxx"]
  }
}
```

```cpp
// cpp_maybe_use_rust.cc

#ifndef WEBRTC_WITHOUT_RUST
#include "call_rust_from_cpp_cxx.rs.h"
#endif

...
uint64_t MaybeUseRust(int64_t value) {
#ifdef WEBRTC_WITHOUT_RUST
  return CallCppFallback(value);
#else
  return rust_function(value);
#endif
}
```

[crubit]: http://crubit.rs/
[crubit-in-chromium-issue]: http://crbug.com/351793625
[cxx]: http://cxx.rs/
[rust]: https://rust-lang.org/
[rust-api-guide]: https://rust-lang.github.io/api-guidelines/
[rust-chromium-style-guide]: https://chromium.googlesource.com/chromium/src/+/main/styleguide/rust/rust.md
[rust-chromium-test]: https://source.chromium.org/chromium/chromium/src/+/main:testing/rust_gtest_interop/
[rust-google-test]: https://github.com/google/googletest-rust
[rust-style-guide]: https://doc.rust-lang.org/style-guide/
[rust-test]: https://doc.rust-lang.org/book/ch11-01-writing-tests.html
