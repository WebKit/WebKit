<!-- go/cmark -->

<!--* freshness: {owner: 'danilchap' reviewed: '2026-04-02'} *-->

# Using Abseil in WebRTC

You may use a subset of the utilities provided by the [Abseil] library when
writing WebRTC C++ code. Below, we list the explicitly *allowed* and the
explicitly *disallowed* subsets of Abseil; if you find yourself in need of
something that isn’t in either subset, please add it to the *allowed* subset in
this doc in the same CL that adds the first use.

## How to depend on Abseil

Dependencies on Abseil need to be listed in `deps` using absolute path, for
example `"//third_party/abseil-cpp/absl/base:core_headers"`

The rtc GN templates will take care of generating the proper dependency when
used within Chromium or standalone. In the `build_with_chromium` mode, WebRTC
will depend on the monolithic Abseil build target that will generate a shared
library.

## **Allowed**

- `absl::AnyInvocable`

- `absl::bind_front`

- `absl::Cleanup`

- [Hash tables, and B-tree ordered][abseil-containers] containers

- `absl::InlinedVector`

- `absl_nonnull` and `absl_nullable`

- `absl::NoDestructor`

- `absl::WrapUnique`

- `absl::string_view` (preferred over std::string_view, see
  [issue 42225436](https://issues.webrtc.org/42225436) for details)

- The functions in `absl/strings/ascii.h`, `absl/strings/match.h`, and
  `absl/strings/str_replace.h`.

- The functions in `absl/strings/escaping.h`.

- The functions in `absl/algorithm/algorithm.h` and
  `absl/algorithm/container.h`.

- The macros in `absl/base/attributes.h`, `absl/base/config.h` and
  `absl/base/macros.h`.

- `absl/numeric/bits.h`

- `absl/crc`

- Single argument `absl::StrCat`

- ABSL_FLAG is allowed in tests and tools, but disallowed in in non-test code.

## **Disallowed**

### `absl::make_unique`

*Use `std::make_unique` instead.*

### `absl::Mutex`

*Use `webrtc::Mutex` instead.*

### `absl::optional` and `absl::variant`

*Use `std::optional` and `std::variant` directly.*

### `absl::Span`

*Use `std::span` instead.*

`absl::Span` differs from `std::span` on several points, in particular lacks
static extent template parameter that WebRTC relies on.

### `absl::StrCat`, `absl::StrAppend`, `absl::StrJoin`, `absl::StrSplit`

*Use `webrtc::StringBuilder` to build strings.*

These are optimized for speed, not binary size. Even `StrCat` calls with a
modest number of arguments can easily add several hundred bytes to the binary.

Exception: Single-argument absl::StrCat is allowed in order to make it easy to
use AbslStringify. See [TOTW #215](https://abseil.io/tips/215) for details on
AbslStringify.

[abseil]: https://abseil.io/about/
[abseil-containers]: https://abseil.io/docs/cpp/guides/container
