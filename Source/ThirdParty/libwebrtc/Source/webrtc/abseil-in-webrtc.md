# Using Abseil in WebRTC

You may use a subset of the utilities provided by the [Abseil][abseil]
library when writing WebRTC C++ code. Below, we list the explicitly
*allowed* and the explicitly *disallowed* subsets of Abseil; if you
find yourself in need of something that isn&rsquo;t in either subset,
please add it to the *allowed* subset in this doc in the same CL that
adds the first use.

[abseil]: https://abseil.io/about/

## **Allowed**

* `absl::InlinedVector`
* `absl::make_unique` and `absl::WrapUnique`
* `absl::optional` and related stuff from `absl/types/optional.h`.
* `absl::string_view`
* The functions in `absl/strings/ascii.h` and `absl/strings/match.h`
* `absl::is_trivially_copy_constructible`,
  `absl::is_trivially_copy_assignable`, and
  `absl::is_trivially_destructible` from `absl/meta/type_traits.h`.
* `absl::variant` and related stuff from `absl/types/variant.h`.

## **Disallowed**

### `absl::Mutex`

*Use `rtc::CriticalSection` instead.*

Chromium has a ban on new static initializers, and `absl::Mutex` uses
one. To make `absl::Mutex` available, we would need to nicely ask the
Abseil team to remove that initializer (like they already did for a
spinlock initializer). Additionally, `absl::Mutex` handles time in a
way that may not be compaible with the rest of WebRTC.

### `absl::Span`

*Use `rtc::ArrayView` instead.*

`absl::Span` differs from `rtc::ArrayView` on several points, and both
of them differ from the `std::span` that was voted into
C++20&mdash;and `std::span` is likely to undergo further changes
before C++20 is finalized. We should just keep using `rtc::ArrayView`
and avoid `absl::Span` until C++20 is finalized and the Abseil team
has decided if they will change `absl::Span` to match.
[Bug](https://bugs.webrtc.org/9214).

### `absl::StrCat` and `absl::StrAppend`

*Use `rtc::SimpleStringBuilder` instead.*

These are optimized for speed, not binary size. Even `StrCat` calls
with a modest number of arguments can easily add several hundred bytes
to the binary.
