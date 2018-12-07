# WebRTC coding style guide

## **General advice**

Some older parts of the code violate the style guide in various ways.

* If making small changes to such code, follow the style guide when
  it’s reasonable to do so, but in matters of formatting etc., it is
  often better to be consistent with the surrounding code.
* If making large changes to such code, consider first cleaning it up
  in a separate CL.

## **C++**

WebRTC follows the [Chromium][chr-style] and [Google][goog-style] C++
style guides. In cases where they conflict, the Chromium style guide
trumps the Google style guide, and the rules in this file trump them
both.

[chr-style]: https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/c++/c++.md
[goog-style]: https://google.github.io/styleguide/cppguide.html

### C++ version

WebRTC is written in C++11, but with some restrictions:

* We only allow the subset of C++11 (language and library) in the
  “allowed” section of [this Chromium page][chromium-cpp11].
* We only allow the subset of C++11 that is also valid C++14;
  otherwise, users would not be able to compile WebRTC in C++14 mode.

[chromium-cpp11]: https://chromium-cpp.appspot.com/

### Abseil

You may use a subset of the utilities provided by the [Abseil][abseil]
library when writing WebRTC C++ code. [Details](abseil-in-webrtc.md).

[abseil]: https://abseil.io/about/

### <a name="h-cc-pairs"></a>`.h` and `.cc` files come in pairs

`.h` and `.cc` files should come in pairs, with the same name (except
for the file type suffix), in the same directory, in the same build
target.

* If a declaration in `path/to/foo.h` has a definition in some `.cc`
  file, it should be in `path/to/foo.cc`.
* If a definition in `path/to/foo.cc` file has a declaration in some
  `.h` file, it should be in `path/to/foo.h`.
* Omit the `.cc` file if it would have been empty, but still list the
  `.h` file in a build target.
* Omit the `.h` file if it would have been empty. (This can happen
  with unit test `.cc` files, and with `.cc` files that define
  `main`.)

This makes the source code easier to navigate and organize, and
precludes some questionable build system practices such as having
build targets that don’t pull in definitions for everything they
declare.

[Examples and exceptions](style-guide/h-cc-pairs.md).

### ArrayView

When passing an array of values to a function, use `rtc::ArrayView`
whenever possible—that is, whenever you’re not passing ownership of
the array, and don’t allow the callee to change the array size.

For example,

instead of                          | use
------------------------------------|---------------------
`const std::vector<T>&`             | `ArrayView<const T>`
`const T* ptr, size_t num_elements` | `ArrayView<const T>`
`T* ptr, size_t num_elements`       | `ArrayView<T>`

See [the source](api/array_view.h) for more detailed docs.

### `absl::optional<T>` as function argument

`absl::optional<T>` is generally a good choice when you want to pass a
possibly missing `T` to a function&mdash;provided of course that `T`
is a type that it makes sense to pass by value.

However, when you want to avoid pass-by-value, generally **do not pass
`const absl::optional<T>&`; use `const T*` instead.** `const
absl::optional<T>&` forces the caller to store the `T` in an
`absl::optional<T>`; `const T*`, on the other hand, makes no
assumptions about how the `T` is stored.

### sigslot

sigslot is a lightweight library that adds a signal/slot language
construct to C++, making it easy to implement the observer pattern
with minimal boilerplate code.

When adding a signal to a pure interface, **prefer to add a pure
virtual method that returns a reference to a signal**:

```
sigslot::signal<int>& SignalFoo() = 0;
```

As opposed to making it a public member variable, as a lot of legacy
code does:

```
sigslot::signal<int> SignalFoo;
```

The virtual method approach has the advantage that it keeps the
interface stateless, and gives the subclass more flexibility in how it
implements the signal. It may:

* Have its own signal as a member variable.
* Use a `sigslot::repeater`, to repeat a signal of another object:

  ```
  sigslot::repeater<int> foo_;
  /* ... */
  foo_.repeat(bar_.SignalFoo());
  ```
* Just return another object's signal directly, if the other object's
  lifetime is the same as its own.

  ```
  sigslot::signal<int>& SignalFoo() { return bar_.SignalFoo(); }
  ```

### std::bind

Don’t use `std::bind`—there are pitfalls, and lambdas are almost as
succinct and already familiar to modern C++ programmers.

### std::function

`std::function` is allowed, but remember that it’s not the right tool
for every occasion. Prefer to use interfaces when that makes sense,
and consider `rtc::FunctionView` for cases where the callee will not
save the function object.

### Forward declarations

WebRTC follows the [Google][goog-forward-declarations] C++ style guide
with respect to forward declarations. In summary: avoid using forward
declarations where possible; just `#include` the headers you need.

[goog-forward-declarations]: https://google.github.io/styleguide/cppguide.html#Forward_Declarations

## **C**

There’s a substantial chunk of legacy C code in WebRTC, and a lot of
it is old enough that it violates the parts of the C++ style guide
that also applies to C (naming etc.) for the simple reason that it
pre-dates the use of the current C++ style guide for this code base.

* If making small changes to C code, mimic the style of the
  surrounding code.
* If making large changes to C code, consider converting the whole
  thing to C++ first.

## **Java**

WebRTC follows the [Google Java style guide][goog-java-style].

[goog-java-style]: https://google.github.io/styleguide/javaguide.html

## **Objective-C and Objective-C++**

WebRTC follows the
[Chromium Objective-C and Objective-C++ style guide][chr-objc-style].

[chr-objc-style]: https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/objective-c/objective-c.md

## **Python**

WebRTC follows [Chromium’s Python style][chr-py-style].

[chr-py-style]: https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/styleguide.md#python

## **Build files**

The WebRTC build files are written in [GN][gn], and we follow
the [Chromium GN style guide][chr-gn-style]. Additionally, there are
some WebRTC-specific rules below; in case of conflict, they trump the
Chromium style guide.

[gn]: https://chromium.googlesource.com/chromium/src/tools/gn/
[chr-gn-style]: https://chromium.googlesource.com/chromium/src/tools/gn/+/HEAD/docs/style_guide.md

### <a name="webrtc-gn-templates"></a>WebRTC-specific GN templates

Use the following [GN templates][gn-templ] to ensure that all
our [targets][gn-target] are built with the same configuration:

instead of       | use
-----------------|---------------------
`executable`     | `rtc_executable`
`shared_library` | `rtc_shared_library`
`source_set`     | `rtc_source_set`
`static_library` | `rtc_static_library`
`test`           | `rtc_test`

[gn-templ]: https://chromium.googlesource.com/chromium/src/tools/gn/+/HEAD/docs/language.md#Templates
[gn-target]: https://chromium.googlesource.com/chromium/src/tools/gn/+/HEAD/docs/language.md#Targets

### Target visibility and the native API

The [WebRTC-specific GN templates](#webrtc-gn-templates) declare build
targets whose default `visibility` allows all other targets in the
WebRTC tree (and no targets outside the tree) to depend on them.

Prefer to restrict the visibility if possible:

* If a target is used by only one or a tiny number of other targets,
  prefer to list them explicitly: `visibility = [ ":foo", ":bar" ]`
* If a target is used only by targets in the same `BUILD.gn` file:
  `visibility = [ ":*" ]`.

Setting `visibility = [ "*" ]` means that targets outside the WebRTC
tree can depend on this target; use this only for build targets whose
headers are part of the [native API](native-api.md).

### Conditional compilation with the C preprocessor

Avoid using the C preprocessor to conditionally enable or disable
pieces of code. But if you can’t avoid it, introduce a GN variable,
and then set a preprocessor constant to either 0 or 1 in the build
targets that need it:

```
if (apm_debug_dump) {
  defines = [ "WEBRTC_APM_DEBUG_DUMP=1" ]
} else {
  defines = [ "WEBRTC_APM_DEBUG_DUMP=0" ]
}
```

In the C, C++, or Objective-C files, use `#if` when testing the flag,
not `#ifdef` or `#if defined()`:

```
#if WEBRTC_APM_DEBUG_DUMP
// One way.
#else
// Or another.
#endif
```

When combined with the `-Wundef` compiler option, this produces
compile time warnings if preprocessor symbols are misspelled, or used
without corresponding build rules to set them.
