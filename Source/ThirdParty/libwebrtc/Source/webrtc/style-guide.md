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

See [the source](webrtc/api/array_view.h) for more detailed docs.

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

### WebRTC-specific GN templates

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
