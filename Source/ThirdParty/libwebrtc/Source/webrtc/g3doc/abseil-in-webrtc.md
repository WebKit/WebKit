# Using Abseil in WebRTC

<?% config.freshness.owner = 'danilchap' %?>
<?% config.freshness.reviewed = '2021-05-12' %?>

You may use a subset of the utilities provided by the [Abseil][abseil]
library when writing WebRTC C++ code. Below, we list the explicitly
*allowed* and the explicitly *disallowed* subsets of Abseil; if you
find yourself in need of something that isn&rsquo;t in either subset,
please add it to the *allowed* subset in this doc in the same CL that
adds the first use.

[abseil]: https://abseil.io/about/


## How to depend on Abseil

For build targets of type `rtc_library`, `rtc_source_set` and
`rtc_static_library`, dependencies on Abseil need to be listed in `absl_deps`
instead of `deps`.

This is needed in order to support the Abseil component build in Chromium. In
that build mode, WebRTC will depend on a monolithic Abseil build target that
will generate a shared library.

## **Allowed**

* `absl::bind_front`
* `absl::InlinedVector`
* `absl::WrapUnique`
* `absl::optional` and related stuff from `absl/types/optional.h`.
* `absl::string_view`
* The functions in `absl/strings/ascii.h`, `absl/strings/match.h`,
  and `absl/strings/str_replace.h`.
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


## **Disallowed**

### `absl::make_unique`

*Use `std::make_unique` instead.*

### `absl::Mutex`

*Use `webrtc::Mutex` instead.*

Chromium has a ban on new static initializers, and `absl::Mutex` uses
one. To make `absl::Mutex` available, we would need to nicely ask the
Abseil team to remove that initializer (like they already did for a
spinlock initializer). Additionally, `absl::Mutex` handles time in a
way that may not be compatible with the rest of WebRTC.

### `absl::Span`

*Use `rtc::ArrayView` instead.*

`absl::Span` differs from `rtc::ArrayView` on several points, and both
of them differ from the `std::span` that was voted into
C++20&mdash;and `std::span` is likely to undergo further changes
before C++20 is finalized. We should just keep using `rtc::ArrayView`
and avoid `absl::Span` until C++20 is finalized and the Abseil team
has decided if they will change `absl::Span` to match.
[Bug](https://bugs.webrtc.org/9214).

### `absl::StrCat`, `absl::StrAppend`, `absl::StrJoin`, `absl::StrSplit`

*Use `rtc::SimpleStringBuilder` to build strings.*

These are optimized for speed, not binary size. Even `StrCat` calls
with a modest number of arguments can easily add several hundred bytes
to the binary.
