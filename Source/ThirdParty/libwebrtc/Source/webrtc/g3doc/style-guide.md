# WebRTC coding style guide

<?% config.freshness.owner = 'danilchap' %?>
<?% config.freshness.reviewed = '2021-05-12' %?>

## General advice

Some older parts of the code violate the style guide in various ways.

* If making small changes to such code, follow the style guide when it's
  reasonable to do so, but in matters of formatting etc., it is often better to
  be consistent with the surrounding code.
* If making large changes to such code, consider first cleaning it up in a
  separate CL.

## C++

WebRTC follows the [Chromium C++ style guide][chr-style] and the
[Google C++ style guide][goog-style]. In cases where they conflict, the Chromium
style guide trumps the Google style guide, and the rules in this file trump them
both.

[chr-style]: https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md
[goog-style]: https://google.github.io/styleguide/cppguide.html

### C++ version

WebRTC is written in C++14, but with some restrictions:

* We only allow the subset of C++14 (language and library) that is not banned by
  Chromium; see the [list of banned C++ features in Chromium][chr-style-cpp].
* We only allow the subset of C++14 that is also valid C++17; otherwise, users
  would not be able to compile WebRTC in C++17 mode.

[chr-style-cpp]: https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++11.md

Unlike the Chromium and Google C++ style guides, we do not allow C++20-style
designated initializers, because we want to stay compatible with compilers that
do not yet support them.

### Abseil

You may use a subset of the utilities provided by the [Abseil][abseil] library
when writing WebRTC C++ code; see the
[instructions on how to use Abseil in WebRTC](abseil-in-webrtc.md).

[abseil]: https://abseil.io/about/

### <a name="h-cc-pairs"></a>`.h` and `.cc` files come in pairs

`.h` and `.cc` files should come in pairs, with the same name (except for the
file type suffix), in the same directory, in the same build target.

* If a declaration in `path/to/foo.h` has a definition in some `.cc` file, it
  should be in `path/to/foo.cc`.
* If a definition in `path/to/foo.cc` file has a declaration in some `.h` file,
  it should be in `path/to/foo.h`.
* Omit the `.cc` file if it would have been empty, but still list the `.h` file
  in a build target.
* Omit the `.h` file if it would have been empty. (This can happen with unit
  test `.cc` files, and with `.cc` files that define `main`.)

See also the
[examples and exceptions on how to treat `.h` and `.cpp` files](style-guide/h-cc-pairs.md).

This makes the source code easier to navigate and organize, and precludes some
questionable build system practices such as having build targets that don't pull
in definitions for everything they declare.

### `TODO` comments

Follow the [Google styleguide for `TODO` comments][goog-style-todo]. When
referencing a WebRTC bug, prefer the url form, e.g.

```cpp
// TODO(bugs.webrtc.org/12345): Delete the hack when blocking bugs are resolved.
```

[goog-style-todo]: https://google.github.io/styleguide/cppguide.html#TODO_Comments

### Deprecation

Annotate the declarations of deprecated functions and classes with the
[`ABSL_DEPRECATED` macro][ABSL_DEPRECATED] to cause an error when they're used
inside WebRTC and a compiler warning when they're used by dependant projects.
Like so:

```cpp
ABSL_DEPRECATED("bugs.webrtc.org/12345")
std::pony PonyPlz(const std::pony_spec& ps);
```

NOTE 1: The annotation goes on the declaration in the `.h` file, not the
definition in the `.cc` file!

NOTE 2: In order to have unit tests that use the deprecated function without
getting errors, do something like this:

```cpp
std::pony DEPRECATED_PonyPlz(const std::pony_spec& ps);
ABSL_DEPRECATED("bugs.webrtc.org/12345")
inline std::pony PonyPlz(const std::pony_spec& ps) {
  return DEPRECATED_PonyPlz(ps);
}
```

In other words, rename the existing function, and provide an inline wrapper
using the original name that calls it. That way, callers who are willing to
call it using the `DEPRECATED_`-prefixed name don't get the warning.

[ABSL_DEPRECATED]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/base/attributes.h?q=ABSL_DEPRECATED

### ArrayView

When passing an array of values to a function, use `rtc::ArrayView`
whenever possible—that is, whenever you're not passing ownership of
the array, and don't allow the callee to change the array size.

For example,

| instead of                          | use                  |
|-------------------------------------|----------------------|
| `const std::vector<T>&`             | `ArrayView<const T>` |
| `const T* ptr, size_t num_elements` | `ArrayView<const T>` |
| `T* ptr, size_t num_elements`       | `ArrayView<T>`       |

See the [source code for `rtc::ArrayView`](api/array_view.h) for more detailed
docs.

### sigslot

SIGSLOT IS DEPRECATED.

Prefer `webrtc::CallbackList`, and manage thread safety yourself.

### Smart pointers

The following smart pointer types are recommended:

   * `std::unique_ptr` for all singly-owned objects
   * `rtc::scoped_refptr` for all objects with shared ownership

Use of `std::shared_ptr` is *not permitted*. It is banned in the Chromium style
guide (overriding the Google style guide), and offers no compelling advantage
over `rtc::scoped_refptr` (which is cloned from the corresponding Chromium
type). See the
[list of banned C++ library features in Chromium][chr-std-shared-ptr] for more
information.

In most cases, one will want to explicitly control lifetimes, and therefore use
`std::unique_ptr`, but in some cases, for instance where references have to
exist both from the API users and internally, with no way to invalidate pointers
held by the API user, `rtc::scoped_refptr` can be appropriate.

[chr-std-shared-ptr]: https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++11.md#shared-pointers

### `std::bind`

Don't use `std::bind`—there are pitfalls, and lambdas are almost as succinct and
already familiar to modern C++ programmers.

### `std::function`

`std::function` is allowed, but remember that it's not the right tool for every
occasion. Prefer to use interfaces when that makes sense, and consider
`rtc::FunctionView` for cases where the callee will not save the function
object.

### Forward declarations

WebRTC follows the
[Google C++ style guide on forward declarations][goog-forward-declarations].
In summary: avoid using forward declarations where possible; just `#include` the
headers you need.

[goog-forward-declarations]: https://google.github.io/styleguide/cppguide.html#Forward_Declarations

## C

There's a substantial chunk of legacy C code in WebRTC, and a lot of it is old
enough that it violates the parts of the C++ style guide that also applies to C
(naming etc.) for the simple reason that it pre-dates the use of the current C++
style guide for this code base.

* If making small changes to C code, mimic the style of the surrounding code.
* If making large changes to C code, consider converting the whole thing to C++
  first.

## Java

WebRTC follows the [Google Java style guide][goog-java-style].

[goog-java-style]: https://google.github.io/styleguide/javaguide.html

## Objective-C and Objective-C++

WebRTC follows the
[Chromium Objective-C and Objective-C++ style guide][chr-objc-style].

[chr-objc-style]: https://chromium.googlesource.com/chromium/src/+/main/styleguide/objective-c/objective-c.md

## Python

WebRTC follows [Chromium's Python style][chr-py-style].

[chr-py-style]: https://chromium.googlesource.com/chromium/src/+/main/styleguide/python/python.md

## Build files

The WebRTC build files are written in [GN][gn], and we follow the
[GN style guide][gn-style]. Additionally, there are some
WebRTC-specific rules below; in case of conflict, they trump the Chromium style
guide.

[gn]: https://gn.googlesource.com/gn/
[gn-style]: https://gn.googlesource.com/gn/+/HEAD/docs/style_guide.md

### <a name="webrtc-gn-templates"></a>WebRTC-specific GN templates

Use the following [GN templates][gn-templ] to ensure that all our
[GN targets][gn-target] are built with the same configuration:

| instead of       | use                  |
|------------------|----------------------|
| `executable`     | `rtc_executable`     |
| `shared_library` | `rtc_shared_library` |
| `source_set`     | `rtc_source_set`     |
| `static_library` | `rtc_static_library` |
| `test`           | `rtc_test`           |


[gn-templ]: https://gn.googlesource.com/gn/+/HEAD/docs/language.md#Templates
[gn-target]: https://gn.googlesource.com/gn/+/HEAD/docs/language.md#Targets

### Target visibility and the native API

The [WebRTC-specific GN templates](#webrtc-gn-templates) declare build targets
whose default `visibility` allows all other targets in the WebRTC tree (and no
targets outside the tree) to depend on them.

Prefer to restrict the `visibility` if possible:

* If a target is used by only one or a tiny number of other targets, prefer to
  list them explicitly: `visibility = [ ":foo", ":bar" ]`
* If a target is used only by targets in the same `BUILD.gn` file:
  `visibility = [ ":*" ]`.

Setting `visibility = [ "*" ]` means that targets outside the WebRTC tree can
depend on this target; use this only for build targets whose headers are part of
the [native WebRTC API](native-api.md).

### Conditional compilation with the C preprocessor

Avoid using the C preprocessor to conditionally enable or disable pieces of
code. But if you can't avoid it, introduce a GN variable, and then set a
preprocessor constant to either 0 or 1 in the build targets that need it:

```gn
if (apm_debug_dump) {
  defines = [ "WEBRTC_APM_DEBUG_DUMP=1" ]
} else {
  defines = [ "WEBRTC_APM_DEBUG_DUMP=0" ]
}
```

In the C, C++, or Objective-C files, use `#if` when testing the flag,
not `#ifdef` or `#if defined()`:

```c
#if WEBRTC_APM_DEBUG_DUMP
// One way.
#else
// Or another.
#endif
```

When combined with the `-Wundef` compiler option, this produces compile time
warnings if preprocessor symbols are misspelled, or used without corresponding
build rules to set them.
