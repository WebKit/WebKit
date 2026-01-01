# Commonly encountered (Non-Corrosion) Issues

## Table of Contents

- [Linking Debug C/C++ libraries into Rust fails on Windows MSVC targets](#linking-debug-cc-libraries-into-rust-fails-on-windows-msvc-targets)
- [Linking Rust static libraries into Debug C/C++ binaries fails on Windows MSVC targets](#linking-rust-static-libraries-into-debug-cc-binaries-fails-on-windows-msvc-targets)
- [Missing `soname` on Linux for `cdylibs`](#missing-soname-on-linux-for-cdylibs)
- [Missing `install_name` on MacOS for `ccdylibs` / Hardcoded references to the build-directory](#missing-installname-on-macos-for-ccdylibs--hardcoded-references-to-the-build-directory)
- [CMake Error (target_link_libraries): Cannot find source file](#cmake-error-target_link_libraries-cannot-find-source-file)

## Linking Debug C/C++ libraries into Rust fails on Windows MSVC targets

`rustc` always links against the non-debug Windows runtime on `*-msvc` targets.
This is tracked [in this issue](https://github.com/rust-lang/rust/issues/39016)
and could be fixed upstream.

A typical error message for this issue is:

```
   Compiling rust_bin v0.1.0 (D:\a\corrosion\corrosion\test\cxxbridge\cxxbridge_cpp2rust\rust)
error: linking with `link.exe` failed: exit code: 1319
[ redacted ]
  = note: cxxbridge-cpp.lib(lib.cpp.obj) : error LNK2038: mismatch detected for '_ITERATOR_DEBUG_LEVEL': value '2' doesn't match value '0' in libcxx-bafec361a1a30317.rlib(cxx.o)

          cxxbridge-cpp.lib(lib.cpp.obj) : error LNK2038: mismatch detected for 'RuntimeLibrary': value 'MDd_DynamicDebug' doesn't match value 'MD_DynamicRelease' in libcxx-bafec361a1a30317.rlib(cxx.o)

          cpp_lib.lib(cpplib.cpp.obj) : error LNK2038: mismatch detected for '_ITERATOR_DEBUG_LEVEL': value '2' doesn't match value '0' in libcxx-bafec361a1a30317.rlib(cxx.o)

          cpp_lib.lib(cpplib.cpp.obj) : error LNK2038: mismatch detected for 'RuntimeLibrary': value 'MDd_DynamicDebug' doesn't match value 'MD_DynamicRelease' in libcxx-bafec361a1a30317.rlib(cxx.o)

          msvcrt.lib(initializers.obj) : warning LNK4098: defaultlib 'msvcrtd.lib' conflicts with use of other libs; use /NODEFAULTLIB:library
```

### Solutions

One solution is to also use the non-debug version when building the C/C++ libraries. 
You can set the [MSVC_RUNTIME_LIBRARY] target properties of your C/C++ libraries to the non-debug variants.
By default you will probably want to select the `MultiThreadedDLL` variant, unless you specified
[`-Ctarget-feature=+crt-static`](https://rust-lang.github.io/rfcs/1721-crt-static.html) in your
`RUSTFLAGS`.


[MSVC_RUNTIME_LIBRARY]: https://cmake.org/cmake/help/latest/prop_tgt/MSVC_RUNTIME_LIBRARY.html#prop_tgt:MSVC_RUNTIME_LIBRARY

## Linking Rust static libraries into Debug C/C++ binaries fails on Windows MSVC targets

This issue is quite similar to the previous one, except that this time it's a Rust library being linked
into a C/C++ target. If it's 100% only Rust code you likely won't even have any issues.
However, if somewhere in the dependency graph C/C++ code is built and linked into your Rust library,
you will likely encounter this issue. Please note, that using [cxx] counts as using C++ code and will
lead to this issue.

The previous solution should also work for this case, but additionally you [may also
have success](https://github.com/rust-lang/rust/issues/39016#issuecomment-853964918) by using 
`corrosion_set_env_vars(your_rust_lib "CFLAGS=-MDd" "CXXFLAGS=-MDd")` (or `-MTd` for a statically linked
runtime).
For debug builds, this is likely to be the preferable solution. It assumes that downstream C/C++ code
is built by the `cc` crate, which respects the `CFLAGS` and `CXXFLAGS` environment variables.

[cxx]: https://github.com/dtolnay/cxx


## Missing `soname` on Linux for `cdylibs`

Cargo doesn't support setting the `soname` field for cdylib, which may cause issues.
You can set the soname manually by passing a linker-flag such as `-Clink-arg=-Wl,-soname,libyour_crate.so`
to the linker via `corrosion_add_target_local_rustflags()` and additionally seting the `IMPORTED_SONAME`
property on the import CMake target:  
```
set_target_properties(your_crate-shared PROPERTIES IMPORTED_SONAME libyour_crate.so)
```
Replace `your_crate` with the name of your shared library as defined in the `[lib]` section of your Cargo.toml
Manifest file.

Attention: The Linux section may not be entirely correct, maybe `$ORIGIN` needs to be added to the linker arguments.
Feel free to open a pull-request with corrections.

## Missing `install_name` on MacOS for `ccdylibs` / Hardcoded references to the build-directory

The solution here is essentially the same as in the previous section.
```
corrosion_add_target_local_rustflags(your_crate -Clink-arg=-Wl,-install_name,@rpath/libyour_crate.dylib,-current_version,${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR},-compatibility_version,${PROJECT_VERSION_MAJOR}.0)
set_target_properties(your_crate-shared PROPERTIES IMPORTED_NO_SONAME 0)
set_target_properties(your_crate-shared PROPERTIES IMPORTED_SONAME libyour_crate.dylib)
```
When building binaries using this shared library, you should set the build rpath to the output directory of
your shared library, e.g. by setting `set(CMAKE_BUILD_RPATH ${YOUR_CUSTOM_OUTPUT_DIRECTORY})` before adding
executables.
For a practical example, you may look at [Slint PR 2455](https://github.com/slint-ui/slint/pull/2455).

## CMake Error (target_link_libraries): Cannot find source file

When using `corrosion_add_cxxbridge`, you may encounter an error similar to this in targets that depend on the cxxbridge target:

```diff
- CMake Error at ...../CMakeLists.txt:61 (target_link_libraries):
-  Cannot find source file:
-
-    ...../corrosion_generated/..../somefile.h
-
-  Tried extensions .c .C .c++ .cc .cpp .cxx .cu .mpp .m .M .mm .ixx .cppm
-  .ccm .cxxm .c++m .h .hh .h++ .hm .hpp .hxx .in .txx .f .F .for .f77 .f90
-  .f95 .f03 .hip .ispc
```

Where `somefile.h` should be generated by CXX via `corrosion_add_cxxbridge`.
In theory, CMake should already know that this is a generated file and just generate it when needed.

However, in older versions of CMake the `GENERATED` property isn't correctly propagated.
See also: [https://gitlab.kitware.com/cmake/cmake/-/issues/18399](https://gitlab.kitware.com/cmake/cmake/-/issues/18399)

This has since been fixed with CMake 3.20: [https://cmake.org/cmake/help/v3.20/policy/CMP0118.html](https://cmake.org/cmake/help/latest/command/cmake_policy.html#version)
However, the CMake policy CMP0118 must be enabled **in any dependent CMakeLists.txt** for the fix to work.

The best fix is to call:
```cmake
cmake_minimium_required(VERSION 3.20 FATAL_ERROR)
# (or any other version above 3.20)
```
As described [here](https://cmake.org/cmake/help/latest/command/cmake_policy.html#version), this implies a call to `cmake_policy` which enables CMP0118.

Unfortunately this must be done in all (transitive) downstream dependencies that link to the bridge target, so cannot be done from within corrosion automatically.
