# Integrating Automatically Generated FFI Bindings

There are a number of tools to automatically generate bindings between Rust and different
foreign languages.

1. [bindgen](#bindgen)
2. [cbindgen](#cbindgen-integration)
3. [cxx](#cxx-integration)

## bindgen

[bindgen] is a tool to automatically generate Rust bindings from C headers.
As such, integrating bindgen [via a build-script](https://rust-lang.github.io/rust-bindgen/library-usage.html)
works well and their doesn't seem to be a need to create CMake rules for 
generating the bindings.

[bindgen]: https://github.com/rust-lang/rust-bindgen

## cbindgen integration

⚠️⚠️⚠️ **EXPERIMENTAL** ⚠️⚠️⚠️

[cbindgen] is a tool that generates C/C++ headers from Rust code. When compiling C/C++
code that `#include`s such generated headers the buildsystem must be aware of the dependencies.
Generating the headers via a build-script is possible, but Corrosion offers no guidance here.

Instead, Corrosion offers an experimental function to add CMake rules using cbindgen to generate
the headers.
This is not available on a stable released version yet, and the details are subject to change.
{{#include ../../cmake/Corrosion.cmake:corrosion_cbindgen}}

## cxx integration

⚠️⚠️⚠️ **EXPERIMENTAL** ⚠️⚠️⚠️

[cxx] is a tool which generates bindings for C++/Rust interop.

{{#include ../../cmake/Corrosion.cmake:corrosion_add_cxxbridge}}
