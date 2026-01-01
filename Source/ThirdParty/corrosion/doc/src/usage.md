## Usage

### Automatically import crate targets with `corrosion_import_crate`

In order to integrate a Rust crate into CMake, you first need to import Rust crates from
a [package] or [workspace]. Corrosion provides `corrosion_import_crate()` to automatically import
crates defined in a Cargo.toml Manifest file:

{{#include ../../cmake/Corrosion.cmake:corrosion-import-crate}}

Corrosion will use `cargo metadata` to add a cmake target for each crate defined in the Manifest file
and add the necessary rules to build the targets.
For Rust executables an [`IMPORTED`] executable target is created with the same name as defined in the `[[bin]]`
section of the Manifest corresponding to this target.
If no such name was defined the target name defaults to the Rust package name.
For Rust library targets an [`INTERFACE`] library target is created with the same name as defined in the `[lib]`
section of the Manifest. This `INTERFACE` library links an internal corrosion target, which is either a
`SHARED` or `STATIC` `IMPORTED` library, depending on the Rust crate type (`cdylib` vs `staticlib`).

The created library targets can be linked into other CMake targets by simply using [target_link_libraries].

Corrosion will by default copy the produced Rust artifacts into `${CMAKE_CURRENT_BINARY_DIR}`. The target location
can be changed by setting the CMake `OUTPUT_DIRECTORY` target properties on the imported Rust targets.
See the [OUTPUT_DIRECTORY](#cmake-output_directory-target-properties-and-imported_location) section for more details.

Many of the options available for `corrosion_import_crate` can also be individually set per
target, see [Per Target options](#per-target-options) for details.

[package]: https://doc.rust-lang.org/book/ch07-01-packages-and-crates.html
[workspace]: https://doc.rust-lang.org/cargo/reference/workspaces.html
[`IMPORTED`]: https://cmake.org/cmake/help/latest/prop_tgt/IMPORTED.html
[`INTERFACE`]: https://cmake.org/cmake/help/latest/command/add_library.html#interface-libraries
[target_link_libraries]: https://cmake.org/cmake/help/latest/command/target_link_libraries.html

### Experimental: Install crate and headers with `corrosion_install`

The default CMake [install commands] do not work correctly with the targets exported from `corrosion_import_crate()`.
Corrosion provides `corrosion_install` to automatically install relevant files:

{{#include ../../cmake/Corrosion.cmake:corrosion-install}}

The example below shows how to import a rust library and make it available for install through CMake.

```cmake
include(FetchContent)

FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        # v0.6 will be updated to point to the latest patch version. 
        # Use v0.6.<patch_version> or the commit hash to prevent such auto updates.
        GIT_TAG v0.6
)
# Set any global configuration variables such as `Rust_TOOLCHAIN` before this line!
FetchContent_MakeAvailable(Corrosion)

# Import targets defined in a package or workspace manifest `Cargo.toml` file
corrosion_import_crate(MANIFEST_PATH rust-lib/Cargo.toml)

# Add a manually written header file which will be exported
# Requires CMake >=3.23
target_sources(rust-lib INTERFACE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/rust-lib/rust-lib.h
)

# OR for CMake <= 3.23
target_include_directories(is_odd INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_sources(is_odd
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/rust-lib/rust-lib.h>
        $<INSTALL_INTERFACE:include/rust-lib/rust-lib.h>
)

# Rust libraries must be installed using `corrosion_install`.
corrosion_install(TARGETS rust-lib EXPORT RustLibTargets)

# Installs the main target
install(
        EXPORT RustLibTargets
        NAMESPACE RustLib::
        DESTINATION lib/cmake/RustLib
)

# Necessary for packaging helper commands
include(CMakePackageConfigHelpers)
# Create a file for checking version compatibility
# Optional
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/RustLibConfigVersion.cmake"
        VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        COMPATIBILITY AnyNewerVersion
)

# Configures the main config file that cmake loads
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/RustLibConfig.cmake"
        INSTALL_DESTINATION lib/cmake/RustLib
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
# Config.cmake.in contains
# @PACKAGE_INIT@
# 
# include(${CMAKE_CURRENT_LIST_DIR}/RustLibTargetsCorrosion.cmake)
# include(${CMAKE_CURRENT_LIST_DIR}/RustLibTargets.cmake)

# Install all generated files
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/RustLibConfigVersion.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/RustLibConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/corrosion/RustLibTargetsCorrosion.cmake
        DESTINATION lib/cmake/RustLib
)
```

[install commands]: https://cmake.org/cmake/help/latest/command/install.html

### Per Target options

Some configuration options can be specified individually for each target. You can set them via the
`corrosion_set_xxx()` functions specified below:

- `corrosion_set_env_vars(<target_name> <key1=value1> [... <keyN=valueN>])`: Define environment variables
  that should be set during the invocation of `cargo build` for the specified target. Please note that
  the environment variable will only be set for direct builds of the target via cmake, and not for any
  build where cargo built the crate in question as a dependency for another target.
  The environment variables may contain generator expressions.
- `corrosion_add_target_rustflags(<target_name> <rustflag> [... <rustflagN>])`: When building the target,
  the `RUSTFLAGS` environment variable will contain the flags added via this function. Please note that any
  dependencies (built by cargo) will also see these flags. See also: `corrosion_add_target_local_rustflags`.
- `corrosion_add_target_local_rustflags(target_name rustc_flag [more_flags ...])`: Support setting
  rustflags for only the main target (crate) and none of its dependencies.
  This is useful in cases where you only need rustflags on the main-crate, but need to set different
  flags for different targets. Without "local" Rustflags this would require rebuilds of the
  dependencies when switching targets.
- `corrosion_set_hostbuild(<target_name>)`: The target should be compiled for the Host target and ignore any
  cross-compile configuration.
- `corrosion_set_features(<target_name> [ALL_FEATURES <Bool>] [NO_DEFAULT_FEATURES] [FEATURES <feature1> ... ])`:
  For a given target, enable specific features via `FEATURES`, toggle `ALL_FEATURES` on or off or disable all features
  via `NO_DEFAULT_FEATURES`. For more information on features, please see also the
  [cargo reference](https://doc.rust-lang.org/cargo/reference/features.html).
- `corrosion_set_cargo_flags(<target_name> <flag1> ...])`:
  For a given target, add options and flags at the end of `cargo build` invocation. This will be appended after any
  arguments passed through the `FLAGS` during the crate import.
- `corrosion_set_linker(target_name linker)`: Use `linker` to link the target.
  Please note that this only has an effect for targets where the final linker invocation is done
  by cargo, i.e. targets where foreign code is linked into rust code and not the other way around.
  Please also note that if you are cross-compiling and specify a linker such as `clang`, you are
  responsible for also adding a rustflag which adds the necessary `--target=` argument for the
  linker.


### Global Corrosion Options

#### Selecting the Rust toolchain and target triple

The following variables are evaluated automatically in most cases. In typical cases you
shouldn't need to alter any of these. If you do want to specify them manually, make sure to set
them **before** `find_package(Corrosion REQUIRED)`.

- `Rust_TOOLCHAIN:STRING` - Specify a named rustup toolchain to use. Changes to this variable
  resets all other options. Default: If the first-found `rustc` is a `rustup` proxy, then the default
  rustup toolchain (see `rustup show`) is used. Otherwise, the variable is unset by default.
- `Rust_COMPILER:STRING` - Path to `rustc`, which should be used for compiling or for toolchain
  detection (if it is a `rustup` proxy). Default: The `rustc` in the first-found toolchain, either
  from `rustup`, or from a toolchain available in the user's `PATH`.
- `Rust_RESOLVE_RUSTUP_TOOLCHAINS:BOOL` - If the found `rustc` is a `rustup` proxy, resolve a
  concrete path to a specific toolchain managed by `rustup`, according to the `rustup` toolchain
  selection rules and other options detailed here. If this option is turned off, the found `rustc`
  will be used as-is to compile, even if it is a `rustup` proxy, which might increase compilation
  time. Default: `ON` if the found `rustc` is a rustup proxy or a `rustup` managed toolchain was
  requested, `OFF` otherwise. Forced `OFF` if `rustup` was not found.
- `Rust_CARGO:STRING` - Path to `cargo`. Default: the `cargo` installed next to `${Rust_COMPILER}`.
- `Rust_CARGO_TARGET:STRING` - The default target triple to build for. Alter for cross-compiling.
  Default: On Visual Studio Generator, the matching triple for `CMAKE_VS_PLATFORM_NAME`. Otherwise,
  the default target triple reported by `${Rust_COMPILER} --version --verbose`.
- `CORROSION_TOOLS_RUST_TOOLCHAIN:STRING`: Specify a different toolchain (e.g. `stable`) to use for compiling helper 
   tools such as `cbindgen` or `cxxbridge`. This can be useful when you want to compile your project with an 
   older rust version (e.g. for checking the MSRV), but you can build build-tools with a newer installed rust version.

#### Enable Convenience Options

The following options are off by default, but may increase convenience:

- `Rust_RUSTUP_INSTALL_MISSING_TARGET:BOOL`: Automatically install a missing target via `rustup` instead of failing.


#### Developer/Maintainer Options
These options are not used in the course of normal Corrosion usage, but are used to configure how
Corrosion is built and installed. Only applies to Corrosion builds and subdirectory uses.

- `CORROSION_BUILD_TESTS:BOOL` - Build the Corrosion tests. Default: `Off` if Corrosion is a
  subdirectory, `ON` if it is the top-level project


### Information provided by Corrosion

For your convenience, Corrosion sets a number of variables which contain information about the version of the rust
toolchain. You can use the CMake version comparison operators
(e.g. [`VERSION_GREATER_EQUAL`](https://cmake.org/cmake/help/latest/command/if.html#version-comparisons)) on the main
variable (e.g. `if(Rust_VERSION VERSION_GREATER_EQUAL "1.57.0")`), or you can inspect the major, minor and patch
versions individually.
- `Rust_VERSION<_MAJOR|_MINOR|_PATCH>` - The version of rustc.
- `Rust_CARGO_VERSION<_MAJOR|_MINOR|_PATCH>` - The cargo version.
- `Rust_LLVM_VERSION<_MAJOR|_MINOR|_PATCH>` - The LLVM version used by rustc.
- `Rust_IS_NIGHTLY` - 1 if a nightly toolchain is used, otherwise 0. Useful for selecting an unstable feature for a
  crate, that is only available on nightly toolchains.
- `Rust_RUSTUP_TOOLCHAINS`, `Rust_RUSTUP_TOOLCHAINS_RUSTC_PATH`, `Rust_RUSTUP_TOOLCHAINS_CARGO_PATH`
  and `Rust_RUSTUP_TOOLCHAINS_VERSION`: These variables are lists, which should be iterated over with
  CMakes `foreach(var IN ZIP_LISTS list1 list2 ...)` iterator. They provide a list of installed rustup managed toolchains and
  the associated rustc and cargo paths as well as the corresponding rustc version.
- Cache variables containing information based on the target triple for the selected target
  as well as the default host target:
  - `Rust_CARGO_TARGET_ARCH`, `Rust_CARGO_HOST_ARCH`: e.g. `x86_64` or `aarch64`
  - `Rust_CARGO_TARGET_VENDOR`, `Rust_CARGO_HOST_VENDOR`: e.g. `apple`, `pc`, `unknown` etc.
  - `Rust_CARGO_TARGET_OS`, `Rust_CARGO_HOST_OS`:  e.g. `darwin`, `linux`, `windows`, `none`
  - `Rust_CARGO_TARGET_ENV`, `Rust_CARGO_HOST_ENV`: e.g. `gnu`, `musl`




### Selecting a custom cargo profile

[Rust 1.57](https://blog.rust-lang.org/2021/12/02/Rust-1.57.0.html) stabilized the support for custom
[profiles](https://doc.rust-lang.org/cargo/reference/profiles.html). If you are using a sufficiently new rust toolchain,
you may select a custom profile by adding the optional argument `PROFILE <profile_name>` to
`corrosion_import_crate()`. If you do not specify a profile, or you use an older toolchain, corrosion will select
the standard `dev` profile if the CMake config is either `Debug` or unspecified. In all other cases the `release`
profile is chosen for cargo.

### Importing C-Style Libraries Written in Rust
Corrosion makes it completely trivial to import a crate into an existing CMake project. Consider
a project called [rust2cpp](test/rust2cpp/rust2cpp) with the following file structure:
```
rust2cpp/
    rust/
        src/
            lib.rs
        Cargo.lock
        Cargo.toml
    CMakeLists.txt
    main.cpp
```

This project defines a simple Rust lib crate, like so, in [`rust2cpp/rust/Cargo.toml`](test/rust2cpp/rust2cpp/rust/Cargo.toml):
```toml
[package]
name = "rust-lib"
version = "0.1.0"
authors = ["Andrew Gaspar <andrew.gaspar@outlook.com>"]
license = "MIT"
edition = "2018"

[dependencies]

[lib]
crate-type=["staticlib"]
```

In addition to `"staticlib"`, you can also use `"cdylib"`. In fact, you can define both with a
single crate and switch between which is used using the standard
[`BUILD_SHARED_LIBS`](https://cmake.org/cmake/help/latest/variable/BUILD_SHARED_LIBS.html) variable.

This crate defines a simple crate called `rust-lib`. Importing this crate into your
[CMakeLists.txt](test/rust2cpp/CMakeLists.txt) is trivial:
```cmake
# Note: you must have already included Corrosion for `corrosion_import_crate` to be available. See # the `Installation` section above.

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)
```

Now that you've imported the crate into CMake, all of the executables, static libraries, and dynamic
libraries defined in the Rust can be directly referenced. So, merely define your C++ executable as
normal in CMake and add your crate's library using target_link_libraries:
```cmake
add_executable(cpp-exe main.cpp)
target_link_libraries(cpp-exe PUBLIC rust-lib)
```

That's it! You're now linking your Rust library to your C++ library.

#### Generate Bindings to Rust Library Automatically

Currently, you must manually declare bindings in your C or C++ program to the exported routines and
types in your Rust project. You can see boths sides of this in
[the Rust code](test/rust2cpp/rust2cpp/rust/src/lib.rs) and in [the C++ code](test/rust2cpp/rust2cpp/main.cpp).

Integration with [cbindgen](https://github.com/eqrion/cbindgen) is
planned for the future.

### Importing Libraries Written in C and C++ Into Rust

The rust targets can be imported with `corrosion_import_crate()` into CMake.
For targets where the linker should be invoked by Rust corrosion provides
`corrosion_link_libraries()` to link your C/C++ libraries with the Rust target.
For additional linker flags you may use `corrosion_add_target_local_rustflags()`
and pass linker arguments via the `-Clink-args` flag to rustc. These flags will
only be passed to the final rustc invocation and not affect any rust dependencies.

C bindings can be generated via [bindgen](https://github.com/rust-lang/rust-bindgen).
Corrosion does not offer any direct integration yet, but you can either generate the
bindings in the build-script of your crate, or generate the bindings as a CMake build step
(e.g. a custom target) and add a dependency from `cargo-prebuild_<rust_target>` to your
custom target for generating the bindings.

Example:

```cmake
# Import your Rust targets
corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)
# Link C/C++ libraries with your Rust target
corrosion_link_libraries(target_name c_library)
# Optionally explicitly define which linker to use.
corrosion_set_linker(target_name your_custom_linker)
# Optionally set linker arguments
corrosion_add_target_local_rustflags(target_name "-Clink-args=<linker arguments>")
# Optionally tell CMake that the rust crate depends on another target (e.g. a code generator)
add_dependencies(cargo-prebuild_<target_name> custom_bindings_target)
```

### Cross Compiling
Corrosion attempts to support cross-compiling as generally as possible, though not all
configurations are tested. Cross-compiling is explicitly supported in the following scenarios.

In all cases, you will need to install the standard library for the Rust target triple. When using
Rustup, you can use it to install the target standard library:

```bash
rustup target add <target-rust-triple>
```

If the target triple is automatically derived, Corrosion will print the target during configuration.
For example:

```
-- Rust Target: aarch64-linux-android
```

#### Windows-to-Windows
Corrosion supports cross-compiling between arbitrary Windows architectures using the Visual Studio
Generator. For example, to cross-compile for ARM64 from any platform, simply set the `-A`
architecture flag:

```bash
cmake -S. -Bbuild-arm64 -A ARM64
cmake --build build-arm64
```

Please note that for projects containing a build-script at least Rust 1.54 is required due to a bug
in previous cargo versions, which causes the build-script to incorrectly be built for the target
platform.

#### Linux-to-Linux
In order to cross-compile on Linux, you will need to install a cross-compiler. For example, on
Ubuntu, to cross compile for 64-bit Little-Endian PowerPC Little-Endian, install
`g++-powerpc64le-linux-gnu` from apt-get:

```bash
sudo apt install g++-powerpc64le-linux-gnu
```

Currently, Corrosion does not automatically determine the target triple while cross-compiling on
Linux, so you'll need to specify a matching `Rust_CARGO_TARGET`.

```bash
cmake -S. -Bbuild-ppc64le -DRust_CARGO_TARGET=powerpc64le-unknown-linux-gnu -DCMAKE_CXX_COMPILER=powerpc64le-linux-gnu-g++
cmake --build build-ppc64le
```

#### Android

Cross-compiling for Android is supported on all platforms with the Makefile and Ninja generators,
and the Rust target triple will automatically be selected. The CMake
[cross-compiling instructions for Android](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-android)
apply here. For example, to build for ARM64:

```bash
cmake -S. -Bbuild-android-arm64 -GNinja -DCMAKE_SYSTEM_NAME=Android \
      -DCMAKE_ANDROID_NDK=/path/to/android-ndk-rxxd -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a
```

**Important note:** The Android SDK ships with CMake 3.10 at newest, which Android Studio will
prefer over any CMake you've installed locally. CMake 3.10 is insufficient for using Corrosion,
which requires a minimum of CMake 3.22. If you're using Android Studio to build your project,
follow the instructions in the Android Studio documentation for
[using a specific version of CMake](https://developer.android.com/studio/projects/install-ndk#vanilla_cmake).


### CMake `OUTPUT_DIRECTORY` target properties and `IMPORTED_LOCATION`

Corrosion respects the following `OUTPUT_DIRECTORY` target properties:
-   [ARCHIVE_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/ARCHIVE_OUTPUT_DIRECTORY.html)
-   [LIBRARY_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/LIBRARY_OUTPUT_DIRECTORY.html)
-   [RUNTIME_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/RUNTIME_OUTPUT_DIRECTORY.html)
-   [PDB_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/PDB_OUTPUT_DIRECTORY.html)

If the target property is set (e.g. by defining the `CMAKE_XYZ_OUTPUT_DIRECTORY` variable before calling
`corrosion_import_crate()`), corrosion will copy the built rust artifacts to the location defined in the
target property.
Due to limitations in CMake these target properties are evaluated in a deferred manner, to
support the user setting the target properties after the call to `corrosion_import_crate()`.
This has the side effect that the `IMPORTED_LOCATION` property will be set late, and users should not
use `get_property` to read `IMPORTED_LOCATION` at configure time. Instead, generator expressions
should be used to get the location of the target artifact.
If `IMPORTED_LOCATION` is needed at configure time users may use `cmake_language(DEFER CALL ...)` to defer
evaluation to after the `IMPORTED_LOCATION` property is set.
