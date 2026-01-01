## About Corrosion

Corrosion, formerly known as cmake-cargo, is a tool for integrating Rust into an existing CMake
project. Corrosion is capable of automatically importing executables, static libraries, and
dynamic libraries from a Rust package or workspace as CMake targets.

The imported static and dynamic library types can be linked into C/C++ CMake targets using the usual
CMake functions such as [`target_link_libraries()`].
For rust executables and dynamic libraries corrosion provides a `corrosion_link_libraries`
helper function to conveniently add the necessary flags to link C/C++ libraries into
the rust target.

## Requirements

- The latest release (v0.6) of Corrosion currently requires CMake 3.22 or newer.
- The previous v0.5 release supports CMake 3.15 or newer. If you are using the v0.5 release, please
  view [the documentation here](./v0.5/introduction.md). 

[`target_link_libraries()`]: https://cmake.org/cmake/help/latest/command/target_link_libraries.html
