# Abseil CMake Build Instructions

Abseil comes with a CMake build script ([CMakeLists.txt](../CMakeLists.txt))
that can be used on a wide range of platforms ("C" stands for cross-platform.).
If you don't have CMake installed already, you can download it for free from
<https://www.cmake.org/>.

CMake works by generating native makefiles or build projects that can
be used in the compiler environment of your choice.

For API/ABI compatibility reasons, we strongly recommend building Abseil in a
subdirectory of your project or as an embedded dependency.

## Incorporating Abseil Into a CMake Project

The recommendations below are similar to those for using CMake within the
googletest framework
(<https://github.com/google/googletest/blob/master/googletest/README.md#incorporating-into-an-existing-cmake-project>)

### Step-by-Step Instructions

1. If you want to build the Abseil tests, integrate the Abseil dependency
[Google Test](https://github.com/google/googletest) into your CMake project. To disable Abseil tests, you have to pass
`-DBUILD_TESTING=OFF` when configuring your project with CMake.

2. Download Abseil and copy it into a subdirectory in your CMake project or add
Abseil as a [git submodule](https://git-scm.com/docs/git-submodule) in your
CMake project.

3. You can then use the CMake command
[`add_subdirectory()`](https://cmake.org/cmake/help/latest/command/add_subdirectory.html)
to include Abseil directly in your CMake project.

4. Add the **absl::** target you wish to use to the
[`target_link_libraries()`](https://cmake.org/cmake/help/latest/command/target_link_libraries.html)
section of your executable or of your library.<br>
Here is a short CMakeLists.txt example of a project file using Abseil.

```cmake
cmake_minimum_required(VERSION 3.5)
project(my_project)

# Pick the C++ standard to compile with.
# Abseil currently supports C++11, C++14, and C++17.
set(CMAKE_CXX_STANDARD 11)

add_subdirectory(abseil-cpp)

add_executable(my_exe source.cpp)
target_link_libraries(my_exe absl::base absl::synchronization absl::strings)
```

### Running Abseil Tests with CMake

Use the `-DBUILD_TESTING=ON` flag to run Abseil tests.

You will need to provide Abseil with a Googletest dependency.  There are two
options for how to do this:

* Use `-DABSL_USE_GOOGLETEST_HEAD`.  This will automatically download the latest
Googletest source into the build directory at configure time.  Googletest will
then be compiled directly alongside Abseil's tests.
* Manually integrate Googletest with your build.  See
https://github.com/google/googletest/blob/master/googletest/README.md#using-cmake
for more information on using Googletest in a CMake project.

For example, to run just the Abseil tests, you could use this script:

```
cd path/to/abseil-cpp
mkdir build
cd build
cmake -DBUILD_TESTING=ON -DABSL_USE_GOOGLETEST_HEAD=ON ..
make -j
ctest
```

Currently, we only run our tests with CMake in a Linux environment, but we are
working on the rest of our supported platforms. See
https://github.com/abseil/abseil-cpp/projects/1 and
https://github.com/abseil/abseil-cpp/issues/109 for more information.

### Available Abseil CMake Public Targets

Here's a non-exhaustive list of Abseil CMake public targets:

```cmake
absl::algorithm
absl::base
absl::debugging
absl::flat_hash_map
absl::flags
absl::memory
absl::meta
absl::numeric
absl::random_random
absl::strings
absl::synchronization
absl::time
absl::utility
```
