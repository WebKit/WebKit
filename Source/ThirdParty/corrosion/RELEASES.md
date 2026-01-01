# v0.6.0 (2025-11-23)

### Breaking Changes

- Corrosion now requires CMake 3.22. See also the 
  [v0.4.0 Release notes](#040-lts-2023-06-01) for more details.
- Removed native tooling and the corresponding option `CORROSION_NATIVE_TOOLING`.
  Corrosion now always uses pure CMake.
- Fix Corrosion placing artifacts into the wrong directory when:
  1. using a Multi-Config Generator (e.g Visual Studio or XCode) AND
  2. `OUTPUT_DIRECTORY_<CONFIG>` is not set AND 
  3. `OUTPUT_DIRECTORY` is set AND
  4. `OUTPUT_DIRECTORY` does not contain a generator expression

  Corrosion now places artifacts into a `$<CONFIG>` subdirectory of the
  specified `OUTPUT_DIRECTORY`. This matches the [documented behavior][doc-cmake-rt-output-dir]
  of CMake for regular CMake targets. ([#568]).

### New features

- Support using the `$<CONFIG>` generator expression in `OUTPUT_DIRECTORY`. [#459]
- Add `OVERRIDE_CRATE_TYPE` option to corrosion_import_crate, allowing users to override
  the crate-types of Rust libraries (e.g. force building as a staticlib instead of an rlib).
- Support *-windows-gnullvm targets. 
- experimental support in corrosion_install for installing libraries and header files
- Add `CORROSION_TOOLS_RUST_TOOLCHAIN` cache variable which allows users to select a different
  rust toolchain for compiling build-tools used by corrosion (currently cbindgen and cxxbridge).
  This mainly allows using a newer toolchain for such build-tools then for the actual project.
- Initial support for iOS targets [#636](https://github.com/corrosion-rs/corrosion/pull/636)

[doc-cmake-rt-output-dir]: https://cmake.org/cmake/help/latest/prop_tgt/RUNTIME_OUTPUT_DIRECTORY.html
[#459]: https://github.com/corrosion-rs/corrosion/pull/459
[#568]: https://github.com/corrosion-rs/corrosion/pull/568

# v0.5.1 (2024-12-29)

### Fixes

- Update FindRust to support `rustup` v1.28.0. Support for older rustup versions is retained,
  so updating corrosion quickly is recommended to all rustup users.


# v0.5.0 (2024-05-11)

### Breaking Changes

- Dashes (`-`) in names of imported CMake **library** targets are now replaced with underscores (`_`).
  See [issue #501] for details. Users on older Corrosion versions will experience the same
  change when using Rust 1.79 or newer. `bin` targets are not affected by this change.

[issue #501]: https://github.com/corrosion-rs/corrosion/issues/501

# v0.4.10 (2024-05-11)

### New features

- `corrosion_experimental_cbindgen()` can now be called multiple times on the same Rust target,
  as long as the output header name differs. This may be useful to generate separate C and C++
  bindings. [#507]
- If `corrosion_link_libraries()` is called on a Rust static library target, then
  `target_link_libraries()` is called to propagate the dependencies to C/C++ consumers.
  Previously a warning was emitted in this case and the arguments ignored. [#506]

### Fixes

- Combine `-framework` flags on macos to avoid linker deduplication errors [#455]
- `corrosion_experimental_cbindgen()` will now correctly use the package name, instead of assuming that
  the package and crate name are identical. ([11e27c])
- Set the `AR_<triple>` variable for `cc-rs` (except for msvc targets) [#456]
- Fix hostbuild when cross-compiling to windows [#477]
- Consider vworks executable suffix [#504]
- `corrosion_experimental_cbindgen()` now forwards the Rust target-triple (e.g. `aarch64-unknown-linux-gnu`)
  to cbindgen via the `TARGET` environment variable. The `hostbuild` property is considered. [#507]
- Fix linking errors with Rust >= 1.79 and `-msvc` targets.` [#511]


[#455]: https://github.com/corrosion-rs/corrosion/pull/455
[#456]: https://github.com/corrosion-rs/corrosion/pull/456
[#477]: https://github.com/corrosion-rs/corrosion/pull/477
[#504]: https://github.com/corrosion-rs/corrosion/pull/504
[#506]: https://github.com/corrosion-rs/corrosion/pull/506
[#507]: https://github.com/corrosion-rs/corrosion/pull/507
[#511]: https://github.com/corrosion-rs/corrosion/pull/511
[11e27c]: https://github.com/corrosion-rs/corrosion/pull/514/commits/11e27cde2cf32c7ed539c96eb03c2f10035de538

# v0.4.9 (2024-05-01)

### New Features 

- Automatically detect Rust target for OpenHarmony ([#510]).

### Fixes

- Make find_package portable ([#509]).

[#510]: https://github.com/corrosion-rs/corrosion/pull/510
[#509]: https://github.com/corrosion-rs/corrosion/pull/509

# v0.4.8 (2024-04-03)

### Fixes

- Fix an internal error when passing both the `PROFILE` and `CRATES` option to
  `corrosion_import_crate()` ([#496]).

[#496]: https://github.com/corrosion-rs/corrosion/pull/496

# v0.4.7 (2024-01-19)

### Fixes

- The C/C++ compiler passed from corrosion to `cc-rs` can now be overridden by users setting
  `CC_<target>` (e.g. `CC_x86_64-unknown-linux-gnu=/path/to/my-compiler`) environment variables ([#475]).

[#475]: https://github.com/corrosion-rs/corrosion/pull/475

# v0.4.6 (2024-01-17)

### Fixes

- Fix hostbuild executables when cross-compiling from non-windows to windows targets.
  (Only with CMake >= 3.19).

# v0.4.5 (2023-11-30)

### Fixes

- Fix hostbuild executables when cross-compiling on windows to non-windows targets
  (Only with CMake >= 3.19).

# v0.4.4 (2023-10-06)

### Fixes

- Add `chimera` ([#445]) and `unikraft` ([#446]) to the list of known vendors

[#445]: https://github.com/corrosion-rs/corrosion/pull/445
[#446]: https://github.com/corrosion-rs/corrosion/pull/446

# v0.4.3 (2023-09-09)

### Fixes

- Fix the PROFILE option with CMake < 3.19 [#427]
- Relax vendor parsing for espressif targets (removes warnings)
- Fix an issue detecting required link libraries with Rust >= 1.71
  when the cmake build directory is located in a Cargo workspace.

# 0.4.2 (2023-07-16)

### Fixes

- Fix an issue when cross-compiling with clang
- Fix detecting required libraries with cargo 1.71 

### New features

- Users can now set `Rust_RESOLVE_RUSTUP_TOOLCHAINS` to `OFF`, which will result in Corrosion
  not attempting to resolve rustc/cargo.

# 0.4.1 (2023-06-03)

This is a bugfix release.

### Fixes

- Fixes a regression on multi-config Generators

# 0.4.0 LTS (2023-06-01)

No changes compared to v0.4.0-beta2.

## Announcements

The `v0.4.x` LTS series will be the last release to support older CMake and Rust versions.
If necessary, fixes will be backported to the v0.4 branch. New features will not be
actively backported after the next major release, but community contributions are possible.
The `v0.4.x` series is currently planned to be maintained until the end of 2024.

The following major release will increase the minimum required CMake version to 3.22. The 
minimum supported Rust version will also be increased to make use of newly added flags, but 
the exact version is not fixed yet. 


## Changes compared to v0.3.5:

### Breaking Changes

- The Visual Studio Generators now require at least CMake 3.20.
  This was previously announced in the 0.3.0 release notes and is the same
  requirement as for the other Multi-Config Generators.
- The previously deprecated function `corrosion_set_linker_language()`
  will now raise an error when called and may be removed without further
  notice in future stable releases. Use `corrosion_set_linker()` instead.
- Improved the FindRust target triple detection, which may cause different behavior in some cases.
  The detection does not require an enabled language anymore and will always fall back
  to the default host target triple. A warning is issued if target triple detection failed.

### Potentially Breaking Changes

- Corrosion now sets the `IMPORTED_NO_SONAME` property for shared rust libraries, since by
  default they won't have an `soname` field.
  If you add a rustflag like `-Clink-arg=-Wl,-soname,libmycrate.so` in your project,
  you should set this property to false on the shared rust library.
- Corrosion now uses a mechanism to determine which native libraries need to be linked with
  Rust `staticlib` targets into C/C++ targets. The previous mechanism contained a hardcoded list.
  The new mechanism asks `rustc` which libraries are needed at minimum for a given
  target triple (with `std` support). This should not be a breaking change, but if you
  do encounter a new linking issue when upgrading with `staticlib` targets, please open an
  issue.

### New features

- `corrosion_import_crate()` has two new options `LOCKED` and `FROZEN` which pass the 
  `--locked` and `--frozen` flags to all invocations of cargo.
- `FindRust` now provides cache variables containing information on the default host
  target triple:
  - `Rust_CARGO_HOST_ARCH`
  - `Rust_CARGO_HOST_VENDOR`
  - `Rust_CARGO_HOST_OS`
  - `Rust_CARGO_HOST_ENV`

### Other changes

- When installing Corrosion with CMake >= 3.19, the legacy Generator tool is
  no longer built and installed by default.
- Corrosion now issues a warning when setting the linker or setting linker
  options for a Rust static library.
- Corrosion no longer enables the `C` language when CMake is in crosscompiling mode and
  no languages where previously enabled. This is not considered a breaking change.
- `corrosion_import_crate()` now warns about unexpected arguments.

### Fixes

- Fix building when the `dev` profile is explicitly set by the user.

## Experimental features (may be changed or removed without a major version bump)

- Experimental cxxbridge and cbindgen integration.
- Add a helper function to parse the package version from a Cargo.toml file
- Expose rustup toolchains discovered by `FindRust` in the following cache variables
  which contain a list.
  - `Rust_RUSTUP_TOOLCHAINS`: List of toolchains names
  - `Rust_RUSTUP_TOOLCHAINS_VERSION`: List of `rustc` version of the toolchains
  - `Rust_RUSTUP_TOOLCHAINS_RUSTC_PATH`: List of the path to `rustc`
  - `Rust_RUSTUP_TOOLCHAINS_CARGO_PATH`: List of the path to `cargo`. Entries may be `NOTFOUND` if cargo
    is not available for that toolchain.
- Add target properties `INTERFACE_CORROSION_RUSTC` and `INTERFACE_CORROSION_CARGO`, which may
  be set to paths to `rustc` and `cargo` respectively to override the toolchain for a specific
  target.

# 0.3.5 (2023-03-19)

- Fix building the Legacy Generator on Rust toolchains < 1.56 ([#365])

[#365]: https://github.com/corrosion-rs/corrosion/pull/365

# 0.3.4 (2023-03-02)

## Fixes

- Fix hostbuild (when CMake/Cargo is configured for cross-compiling) if clang is used ([#338]).

## Other

- Pass `--no-deps` to cargo metadata ([#334]).
- Bump the legacy generator dependencies

[#334]: https://github.com/corrosion-rs/corrosion/pull/334
[#338]: https://github.com/corrosion-rs/corrosion/pull/338


# 0.3.3 (2023-02-17)

## New features (Only available on CMake >= 3.19)

- Add new `IMPORTED_CRATES` flag to `corrosion_import_crate()` to retrieve the list of imported crates in the current
  scope ([#312](https://github.com/corrosion-rs/corrosion/pull/312)).

## Fixes

- Fix imported location target property when the rust target name contains dashes
  and a custom OUTPUT_DIRECTORY was specified by the user ([#322](https://github.com/corrosion-rs/corrosion/pull/322)).
- Fix building for custom rust target-triples ([#316](https://github.com/corrosion-rs/corrosion/pull/316))

# 0.3.2 (2023-01-11)

## New features (Only available on CMake >= 3.19)

- Add new `CRATE_TYPES` flag to `corrosion_import_crate()` to restrict which
  crate types should be imported ([#269](https://github.com/corrosion-rs/corrosion/pull/269)).
- Add `NO_LINKER_OVERRIDE` flag to let Rust choose the default linker for the target
  instead of what Corrosion thinks is the appropriate linker driver ([#272](https://github.com/corrosion-rs/corrosion/pull/272)).

## Fixes

- Fix clean target when cross-compiling ([#291](https://github.com/corrosion-rs/corrosion/pull/291)).
- Don't set the linker for Rust static libraries ([#275](https://github.com/corrosion-rs/corrosion/pull/275)).
- Minor fixes in FindRust [#297](https://github.com/corrosion-rs/corrosion/pull/297): 
  - fix a logic error in the version detection
  - fix a logic error in `QUIET` mode when rustup is not found.

# 0.3.1 (2022-12-13)

### Fixes

- Fix a regression in detecting the MSVC abi ([#256])
- Fix an issue on macOS 13 which affected rust crates compiling C++ code in build scripts ([#254]).
- Fix corrosion not respecting `CMAKE_<XYZ>_OUTPUT_DIRECTORY` values ([#268]).
- Don't override rusts linker choice for the msvc abi (previously this was only skipped for msvc generators) ([#271])

[#254]: https://github.com/corrosion-rs/corrosion/pull/254
[#256]: https://github.com/corrosion-rs/corrosion/pull/256
[#268]: https://github.com/corrosion-rs/corrosion/pull/268
[#271]: https://github.com/corrosion-rs/corrosion/pull/271

# 0.3.0 (2022-10-31)

## Breaking

- The minimum supported rust version (MSRV) was increased to 1.46, due to a cargo issue that recently
  surfaced on CI when using crates.io. On MacOS 12 and Windows-2022 at least Rust 1.54 is required.
- MacOS 10 and 11 are no longer officially supported and untested in CI.
- The minimum required CMake version is now 3.15.
- Adding a `PRE_BUILD` custom command on a `cargo-build_<target_name>` CMake target will no 
  longer work as expected. To support executing user defined commands before cargo build is
  invoked users should use the newly added targets `cargo-prebuild` (before all cargo build invocations)
  or `cargo-prebuild_<target_name>` as a dependency target. 
  Example: `add_dependencies(cargo-prebuild code_generator_target)`

### Breaking: Removed previously deprecated functionality
- Removed `add_crate()` function. Use `corrosio_import_crate()` instead.
- Removed `cargo_link_libraries()` function. Use `corrosion_link_libraries()` instead.
- Removed experimental CMake option `CORROSION_EXPERIMENTAL_PARSER`.
  The corresponding stable option is `CORROSION_NATIVE_TOOLING` albeit with inverted semantics.
- Previously Corrosion would set the `HOST_CC` and `HOST_CXX` environment variables when invoking 
  cargo build, if the environment variables `CC` and `CXX` outside of CMake where set.
  However this did not work as expected in all cases and sometimes the `HOST_CC` variable would be set
  to a cross-compiler for unknown reasons. For this reason `HOST_CC` and `HOST_CXX` are not set by
  corrosion anymore, but users can still set them manually if required via `corrosion_set_env_vars()`.
- The `CARGO_RUST_FLAGS` family of cache variables were removed. Corrosion does not internally use them
  anymore.

## Potentially breaking

- The working directory when invoking `cargo build` was changed to the directory of the Manifest
  file. This now allows cargo to pick up `.cargo/config.toml` files located in the source tree.
  ([205](https://github.com/corrosion-rs/corrosion/pull/205))
- Corrosion internally invokes `cargo build`. When passing arguments to `cargo build`, Corrosion
  now uses the CMake `VERBATIM` option. In rare cases this may require you to change how you quote
  parameters passed to corrosion (e.g. via `corrosion_add_target_rustflags()`).
  For example setting a `cfg` option previously required double escaping the rustflag like this
  `"--cfg=something=\\\"value\\\""`, but now it can be passed to corrosion without any escapes:
  `--cfg=something="value"`.
- Corrosion now respects the CMake `OUTPUT_DIRECTORY` target properties. More details in the "New features" section.

## New features

- Support setting rustflags for only the main target and none of its dependencies ([215](https://github.com/corrosion-rs/corrosion/pull/215)).
  A new function `corrosion_add_target_local_rustflags(target_name rustc_flag [more_flags ...])`
  is added for this purpose.
  This is useful in cases where you only need rustflags on the main-crate, but need to set different
  flags for different targets. Without "local" Rustflags this would require rebuilds of the
  dependencies when switching targets.
- Support explicitly selecting a linker ([208](https://github.com/corrosion-rs/corrosion/pull/208)).
  The linker can be selected via `corrosion_set_linker(target_name linker)`.
  Please note that this only has an effect for targets, where the final linker invocation is done
  by cargo, i.e. targets where foreign code is linked into rust code and not the other way around.
- Corrosion now respects the CMake `OUTPUT_DIRECTORY` target properties and copies build artifacts to the expected
  locations ([217](https://github.com/corrosion-rs/corrosion/pull/217)), if the properties are set.
  This feature requires at least CMake 3.19 and is enabled by default if supported. Please note that the `OUTPUT_NAME`
  target properties are currently not supported.
  Specifically, the following target properties are now respected:
  -   [ARCHIVE_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/ARCHIVE_OUTPUT_DIRECTORY.html)
  -   [LIBRARY_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/LIBRARY_OUTPUT_DIRECTORY.html)
  -   [RUNTIME_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/RUNTIME_OUTPUT_DIRECTORY.html)
  -   [PDB_OUTPUT_DIRECTORY](https://cmake.org/cmake/help/latest/prop_tgt/PDB_OUTPUT_DIRECTORY.html)
- Corrosion now supports packages with potentially multiple binaries (bins) and a library (lib) at the
  same time. The only requirement is that the names of all `bin`s and `lib`s in the whole project must be unique.
  Users can set the names in the `Cargo.toml` by adding `name = <unique_name>` in the `[[bin]]` and `[lib]` tables.
- FindRust now has improved support for the `VERSION` option of `find_package` and will now attempt to find a matching
  toolchain version. Previously it was only checked if the default toolchain matched to required version.
- For rustup managed toolchains a CMake error is issued with a helpful message if the required target for
  the selected toolchain is not installed.

## Fixes

- Fix a CMake developer Warning when a Multi-Config Generator and Rust executable targets
  ([#213](https://github.com/corrosion-rs/corrosion/pull/213)).
- FindRust now respects the `QUIET` option to `find_package()` in most cases.

## Deprecation notice

- Support for the MSVC Generators with CMake toolchains before 3.20 is deprecated and will be removed in the next
  release (v0.4). All other Multi-config Generators already require CMake 3.20.

## Internal Changes

- The CMake Generator written in Rust and `CorrosionGenerator.cmake` which are responsible for parsing 
  `cargo metadata` output to create corresponding CMake targets for all Rust targets now share most code.
  This greatly simplified the CMake generator written in Rust and makes it much easier maintaining and adding
  new features regardless of how `cargo metadata` is parsed.

# 0.2.2 (2022-09-01)

## Fixes

- Do not use C++17 in the tests (makes tests work with older C++ compilers) ([184](https://github.com/corrosion-rs/corrosion/pull/184))
- Fix finding cargo on NixOS ([192](https://github.com/corrosion-rs/corrosion/pull/192))
- Fix issue with Rustflags test when using a Build type other than Debug and Release ([203](https://github.com/corrosion-rs/corrosion/pull/203)).

# 0.2.1 (2022-05-07)

## Fixes

- Fix missing variables provided by corrosion, when corrosion is used as a subdirectory ([181](https://github.com/corrosion-rs/corrosion/pull/181)):
  Public [Variables](https://github.com/corrosion-rs/corrosion#information-provided-by-corrosion) set
  by Corrosion were not visible when using Corrosion as a subdirectory, due to the wrong scope of
  the variables. This was fixed by promoting the respective variables to Cache variables.

# 0.2.0 (2022-05-05)

## Breaking changes

- Removed the integrator build script ([#156](https://github.com/corrosion-rs/corrosion/pull/156)).
  The build script provided by corrosion (for rust code that links in foreign code) is no longer necessary,
  so users can just remove the dependency.

## Deprecations

- Direct usage of the following target properties has been deprecated. The names of the custom properties are
  no longer considered part of the public API and may change in the future. Instead, please use the functions
  provided by corrosion. Internally different property names are used depending on the CMake version.
  - `CORROSION_FEATURES`, `CORROSION_ALL_FEATURES`, `CORROSION_NO_DEFAULT_FEATURES`. Instead please use
    `corrosion_set_features()`. See the updated Readme for details.
  - `CORROSION_ENVIRONMENT_VARIABLES`. Please use `corrosion_set_env_vars()` instead.
  - `CORROSION_USE_HOST_BUILD`. Please use `corrosion_set_hostbuild()` instead.
- The Minimum CMake version will likely be increased for the next major release. At the very least we want to drop
  support for CMake 3.12, but requiring CMake 3.16 or even 3.18 is also on the table. If you are using a CMake version
  that would be no longer supported by corrosion, please comment on issue
  [#168](https://github.com/corrosion-rs/corrosion/issues/168), so that we can gauge the number of affected users.

## New features

- Add `NO_STD` option to `corrosion_import_crate` ([#154](https://github.com/corrosion-rs/corrosion/pull/154)).
- Remove the requirement of building the Rust based generator crate for CMake >= 3.19. This makes using corrosion as
  a subdirectory as fast as the installed version (since everything is done in CMake).
  ([#131](https://github.com/corrosion-rs/corrosion/pull/131), [#161](https://github.com/corrosion-rs/corrosion/pull/161))
  If you do choose to install Corrosion, then by default the old Generator is still compiled and installed, so you can
  fall back to using it in case you use multiple cmake versions on the same machine for different projects.

## Fixes

- Fix Corrosion on MacOS 11 and 12 ([#167](https://github.com/corrosion-rs/corrosion/pull/167) and
  [#164](https://github.com/corrosion-rs/corrosion/pull/164)).
- Improve robustness of parsing the LLVM version (exported in `Rust_LLVM_VERSION`). It now also works for
  Rust versions, where the LLVM version is reported as `MAJOR.MINOR`. ([#148](https://github.com/corrosion-rs/corrosion/pull/148))
- Fix a bug which occurred when Corrosion was added multiple times via `add_subdirectory()`
  ([#143](https://github.com/corrosion-rs/corrosion/pull/143)).
- Set `CC_<target_triple_undercore>` and `CXX_<target_triple_undercore>` environment variables for the invocation of
  `cargo build` to the compilers selected by CMake  (if any)
  ([#138](https://github.com/corrosion-rs/corrosion/pull/138) and [#161](https://github.com/corrosion-rs/corrosion/pull/161)).
  This should ensure that C dependencies built in cargo buildscripts via [cc-rs](https://github.com/alexcrichton/cc-rs)
  use the same compiler as CMake built dependencies. Users can override the compiler by specifying the higher
  priority environment variable variants with dashes instead of underscores (See cc-rs documentation for details).
- Fix Ninja-Multiconfig Generator support for CMake versions >= 3.20. Previous CMake versions are missing a feature,
  which prevents us from supporting the Ninja-Multiconfig generator. ([#137](https://github.com/corrosion-rs/corrosion/pull/137))


# 0.1.0 (2022-02-01)

This is the first release of corrosion after it was moved to the new corrosion-rs organization.
Since there are no previous releases, this is not a complete changelog but only lists changes since
September 2021.

## New features
- [Add --profile support for rust >= 1.57](https://github.com/corrosion-rs/corrosion/pull/130):
  Allows users to specify a custom cargo profile with
  `corrosion_import_crate(... PROFILE <profilename>)`.
- [Add support for specifying per-target Rustflags](https://github.com/corrosion-rs/corrosion/pull/127):
  Rustflags can be added via `corrosion_add_target_rustflags(<target_name> [rustflags1...])`
- [Add `Rust_IS_NIGHTLY` and `Rust_LLVM_VERSION` variables](https://github.com/corrosion-rs/corrosion/pull/123):
  This may be useful if you want to conditionally enabled features when using a nightly toolchain
  or a specific LLVM Version.
- [Let `FindRust` fail gracefully if rustc is not found](https://github.com/corrosion-rs/corrosion/pull/111):
  This allows using `FindRust` in a more general setting (without corrosion).
- [Add support for cargo feature selection](https://github.com/corrosion-rs/corrosion/pull/108):
  See the [README](https://github.com/corrosion-rs/corrosion#cargo-feature-selection) for details on
  how to select features.


## Fixes
- [Fix the cargo-clean target](https://github.com/corrosion-rs/corrosion/pull/129)
- [Fix #84: CorrosionConfig.cmake looks in wrong place for Corrosion::Generator when CMAKE_INSTALL_LIBEXEC is an absolute path](https://github.com/corrosion-rs/corrosion/pull/122/commits/6f29af3ac53917ca2e0638378371e715a18a532d)
- [Fix #116: (Option CORROSION_INSTALL_EXECUTABLE not working)](https://github.com/corrosion-rs/corrosion/commit/97d44018fac1b1a2a7c095288c628f5bbd9b3184)
- [Fix building on Windows with rust >= 1.57](https://github.com/corrosion-rs/corrosion/pull/120)

## Known issues:
- Corrosion is currently not working on macos-11 and newer. See issue [#104](https://github.com/corrosion-rs/corrosion/issues/104).
  Contributions are welcome.
