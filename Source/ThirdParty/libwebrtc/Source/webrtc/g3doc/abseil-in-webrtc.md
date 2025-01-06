<!-- go/cmark -->
<!--* freshness: {owner: 'danilchap' reviewed: '2024-09-02'} *-->

# Using Abseil in WebRTC

You may use a subset of the utilities provided by the [Abseil][abseil]
library when writing WebRTC C++ code. Below, we list the explicitly
*allowed* and the explicitly *disallowed* subsets of Abseil; if you
find yourself in need of something that isn&rsquo;t in either subset,
please add it to the *allowed* subset in this doc in the same CL that
adds the first use.

[abseil]: https://abseil.io/about/


## How to depend on Abseil

For build targets of type `rtc_library`, `rtc_source_set` and
`rtc_static_library`, dependencies on Abseil need to be listed in `deps`.

The GN templates will take care of generating the proper dependency when
used within Chromium or standalone. In that build mode, WebRTC will depend
on a monolithic Abseil build target that will generate a shared library.

## **Allowed**

* `absl::AnyInvocable`
* `absl::bind_front`
* `absl::Cleanup`
* `absl::InlinedVector`
* `absl::Nonnull` and `absl::Nullable`
* `absl::WrapUnique`
* `absl::string_view`
* The functions in `absl/strings/ascii.h`, `absl/strings/match.h`,
  and `absl/strings/str_replace.h`.
* The functions in `absl/strings/escaping.h`.
* `absl::is_trivially_copy_constructible`,
  `absl::is_trivially_copy_assignable`, and
  `absl::is_trivially_destructible` from `absl/meta/type_traits.h`.
* `absl::variant` and related stuff from `absl/types/variant.h`.
* The functions in `absl/algorithm/algorithm.h` and
  `absl/algorithm/container.h`.
* `absl/base/const_init.h` for mutex initialization.
* The macros in `absl/base/attributes.h`, `absl/base/config.h` and
  `absl/base/macros.h`.
* `absl/numeric/bits.h`
* Single argument absl::StrCat

* ABSL_FLAG is allowed in tests and tools, but disallowed in in non-test code.


## **Disallowed**

### `absl::make_unique`

*Use `std::make_unique` instead.*

### `absl::Mutex`

*Use `webrtc::Mutex` instead.*

### `absl::optional`

*Use `std::optional` instead.*

### `absl::Span`

*Use `rtc::ArrayView` instead.*

`absl::Span` differs from `rtc::ArrayView` on several points, and both
of them differ from the `std::span` introduced in C++20. We should just keep
using `rtc::ArrayView` and avoid `absl::Span`. When WebRTC switches to C++20,
we will consider replacing `rtc::ArrayView` with `std::span`.

### `absl::StrCat`, `absl::StrAppend`, `absl::StrJoin`, `absl::StrSplit`

*Use `rtc::SimpleStringBuilder` to build strings.*

These are optimized for speed, not binary size. Even `StrCat` calls
with a modest number of arguments can easily add several hundred bytes
to the binary.

Exception: Single-argument absl::StrCat is allowed in order to make it
easy to use AbslStringify. See [TOTW #2015](https://abseil.io/tips/215) for
details on AbslStringify.
