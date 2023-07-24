README.md                {#LREADME}
=========
# AV1 Codec Library

## Contents
1. [Building the lib and applications](#building-the-library-and-applications)
    - [Prerequisites](#prerequisites)
    - [Get the code](#get-the-code)
    - [Basics](#basic-build)
    - [Configuration options](#configuration-options)
    - [Dylib builds](#dylib-builds)
    - [Debugging](#debugging)
    - [Cross compiling](#cross-compiling)
    - [Sanitizer support](#sanitizers)
    - [MSVC builds](#microsoft-visual-studio-builds)
    - [Xcode builds](#xcode-builds)
    - [Emscripten builds](#emscripten-builds)
    - [Extra Build Flags](#extra-build-flags)
    - [Build with VMAF support](#build-with-vmaf)
2. [Testing the library](#testing-the-av1-codec)
    - [Basics](#testing-basics)
        - [Unit tests](#unit-tests)
        - [Example tests](#example-tests)
        - [Encoder tests](#encoder-tests)
    - [IDE hosted tests](#ide-hosted-tests)
    - [Downloading test data](#downloading-the-test-data)
    - [Adding a new test data file](#adding-a-new-test-data-file)
    - [Additional test data](#additional-test-data)
    - [Sharded testing](#sharded-testing)
        - [Running tests directly](#running-test_libaom-directly)
        - [Running tests via CMake](#running-the-tests-via-the-cmake-build)
3. [Coding style](#coding-style)
4. [Submitting patches](#submitting-patches)
    - [Login cookie](#login-cookie)
    - [Contributor agreement](#contributor-agreement)
    - [Testing your code](#testing-your-code)
    - [Commit message hook](#commit-message-hook)
    - [Upload your change](#upload-your-change)
    - [Incorporating Reviewer Comments](#incorporating-reviewer-comments)
    - [Submitting your change](#submitting-your-change)
    - [Viewing change status](#viewing-the-status-of-uploaded-changes)
5. [Support](#support)
6. [Bug reports](#bug-reports)

## Building the library and applications {#building-the-library-and-applications}

### Prerequisites {#prerequisites}

 1. [CMake](https://cmake.org). See CMakeLists.txt for the minimum version
    required.
 2. [Git](https://git-scm.com/).
 3. [Perl](https://www.perl.org/).
 4. For x86 targets, [yasm](http://yasm.tortall.net/), which is preferred, or a
    recent version of [nasm](http://www.nasm.us/). If you download yasm with
    the intention to work with Visual Studio, please download win32.exe or
    win64.exe and rename it into yasm.exe. DO NOT download or use vsyasm.exe.
 5. Building the documentation requires
   [doxygen version 1.8.10 or newer](http://doxygen.org).
 6. Emscripten builds require the portable
   [EMSDK](https://kripken.github.io/emscripten-site/index.html).

### Get the code {#get-the-code}

The AV1 library source code is stored in the Alliance for Open Media Git
repository:

~~~
    $ git clone https://aomedia.googlesource.com/aom
    # By default, the above command stores the source in the aom directory:
    $ cd aom
~~~

### Basic build {#basic-build}

CMake replaces the configure step typical of many projects. Running CMake will
produce configuration and build files for the currently selected CMake
generator. For most systems the default generator is Unix Makefiles. The basic
form of a makefile build is the following:

~~~
    $ cmake path/to/aom
    $ make
~~~

The above will generate a makefile build that produces the AV1 library and
applications for the current host system after the make step completes
successfully. The compiler chosen varies by host platform, but a general rule
applies: On systems where cc and c++ are present in $PATH at the time CMake is
run the generated build will use cc and c++ by default.

### Configuration options {#configuration-options}

The AV1 codec library has a great many configuration options. These come in two
varieties:

 1. Build system configuration options. These have the form `ENABLE_FEATURE`.
 2. AV1 codec configuration options. These have the form `CONFIG_FEATURE`.

Both types of options are set at the time CMake is run. The following example
enables ccache and disables the AV1 encoder:

~~~
    $ cmake path/to/aom -DENABLE_CCACHE=1 -DCONFIG_AV1_ENCODER=0
    $ make
~~~

The available configuration options are too numerous to list here. Build system
configuration options can be found at the top of the CMakeLists.txt file found
in the root of the AV1 repository, and AV1 codec configuration options can
currently be found in the file `build/cmake/aom_config_defaults.cmake`.

### Dylib builds {#dylib-builds}

A dylib (shared object) build of the AV1 codec library can be enabled via the
CMake built in variable `BUILD_SHARED_LIBS`:

~~~
    $ cmake path/to/aom -DBUILD_SHARED_LIBS=1
    $ make
~~~

This is currently only supported on non-Windows targets.

### Debugging {#debugging}

Depending on the generator used there are multiple ways of going about
debugging AV1 components. For single configuration generators like the Unix
Makefiles generator, setting `CMAKE_BUILD_TYPE` to Debug is sufficient:

~~~
    $ cmake path/to/aom -DCMAKE_BUILD_TYPE=Debug
~~~

For Xcode, mainly because configuration controls for Xcode builds are buried two
configuration windows deep and must be set for each subproject within the Xcode
IDE individually, `CMAKE_CONFIGURATION_TYPES` should be set to Debug:

~~~
    $ cmake path/to/aom -G Xcode -DCMAKE_CONFIGURATION_TYPES=Debug
~~~

For Visual Studio the in-IDE configuration controls should be used. Simply set
the IDE project configuration to Debug to allow for stepping through the code.

In addition to the above it can sometimes be useful to debug only C and C++
code. To disable all assembly code and intrinsics set `AOM_TARGET_CPU` to
generic at generation time:

~~~
    $ cmake path/to/aom -DAOM_TARGET_CPU=generic
~~~

### Cross compiling {#cross-compiling}

For the purposes of building the AV1 codec and applications and relative to the
scope of this guide, all builds for architectures differing from the native host
architecture will be considered cross compiles. The AV1 CMake build handles
cross compiling via the use of toolchain files included in the AV1 repository.
The toolchain files available at the time of this writing are:

 - arm64-ios.cmake
 - arm64-linux-gcc.cmake
 - arm64-mingw-gcc.cmake
 - armv7-ios.cmake
 - armv7-linux-gcc.cmake
 - armv7-mingw-gcc.cmake
 - armv7s-ios.cmake
 - ppc-linux-gcc.cmake
 - riscv-linux-gcc.cmake
 - x86-ios-simulator.cmake
 - x86-linux.cmake
 - x86-macos.cmake
 - x86-mingw-gcc.cmake
 - x86\_64-ios-simulator.cmake
 - x86\_64-mingw-gcc.cmake

The following example demonstrates use of the x86-macos.cmake toolchain file on
a x86\_64 MacOS host:

~~~
    $ cmake path/to/aom \
      -DCMAKE_TOOLCHAIN_FILE=path/to/aom/build/cmake/toolchains/x86-macos.cmake
    $ make
~~~

To build for an unlisted target creation of a new toolchain file is the best
solution. The existing toolchain files can be used a starting point for a new
toolchain file since each one exposes the basic requirements for toolchain files
as used in the AV1 codec build.

As a temporary work around an unoptimized AV1 configuration that builds only C
and C++ sources can be produced using the following commands:

~~~
    $ cmake path/to/aom -DAOM_TARGET_CPU=generic
    $ make
~~~

In addition to the above it's important to note that the toolchain files
suffixed with gcc behave differently than the others. These toolchain files
attempt to obey the $CROSS environment variable.

### Sanitizers {#sanitizers}

Sanitizer integration is built-in to the CMake build system. To enable a
sanitizer, add `-DSANITIZE=<type>` to the CMake command line. For example, to
enable address sanitizer:

~~~
    $ cmake path/to/aom -DSANITIZE=address
    $ make
~~~

Sanitizers available vary by platform, target, and compiler. Consult your
compiler documentation to determine which, if any, are available.

### Microsoft Visual Studio builds {#microsoft-visual-studio-builds}

Building the AV1 codec library in Microsoft Visual Studio is supported. Visual
Studio 2019 (16.0) or later is required. The following example demonstrates
generating projects and a solution for the Microsoft IDE:

~~~
    # This does not require a bash shell; Command Prompt (cmd.exe) is fine.
    # This assumes the build host is a Windows x64 computer.

    # To create a Visual Studio 2022 solution for the x64 target:
    $ cmake path/to/aom -G "Visual Studio 17 2022"

    # To create a Visual Studio 2022 solution for the 32-bit x86 target:
    $ cmake path/to/aom -G "Visual Studio 17 2022" -A Win32

    # To create a Visual Studio 2019 solution for the x64 target:
    $ cmake path/to/aom -G "Visual Studio 16 2019"

    # To create a Visual Studio 2019 solution for the 32-bit x86 target:
    $ cmake path/to/aom -G "Visual Studio 16 2019" -A Win32

    # To build the solution:
    $ cmake --build .
~~~

NOTE: The build system targets Windows 7 or later by compiling files with
`-D_WIN32_WINNT=0x0601`.

### Xcode builds {#xcode-builds}

Building the AV1 codec library in Xcode is supported. The following example
demonstrates generating an Xcode project:

~~~
    $ cmake path/to/aom -G Xcode
~~~

### Emscripten builds {#emscripten-builds}

Building the AV1 codec library with Emscripten is supported. Typically this is
used to hook into the AOMAnalyzer GUI application. These instructions focus on
using the inspector with AOMAnalyzer, but all tools can be built with
Emscripten.

It is assumed here that you have already downloaded and installed the EMSDK,
installed and activated at least one toolchain, and setup your environment
appropriately using the emsdk\_env script.

1. Build [AOM Analyzer](https://github.com/xiph/aomanalyzer).

2. Configure the build:

~~~
    $ cmake path/to/aom \
        -DENABLE_CCACHE=1 \
        -DAOM_TARGET_CPU=generic \
        -DENABLE_DOCS=0 \
        -DENABLE_TESTS=0 \
        -DCONFIG_ACCOUNTING=1 \
        -DCONFIG_INSPECTION=1 \
        -DCONFIG_MULTITHREAD=0 \
        -DCONFIG_RUNTIME_CPU_DETECT=0 \
        -DCONFIG_WEBM_IO=0 \
        -DCMAKE_TOOLCHAIN_FILE=path/to/emsdk-portable/.../Emscripten.cmake
~~~

3. Build it: run make if that's your generator of choice:

~~~
    $ make inspect
~~~

4. Run the analyzer:

~~~
    # inspect.js is in the examples sub directory of the directory in which you
    # executed cmake.
    $ path/to/AOMAnalyzer path/to/examples/inspect.js path/to/av1/input/file
~~~

### Extra build flags {#extra-build-flags}

Three variables allow for passing of additional flags to the build system.

- AOM\_EXTRA\_C\_FLAGS
- AOM\_EXTRA\_CXX\_FLAGS
- AOM\_EXTRA\_EXE\_LINKER\_FLAGS

The build system attempts to ensure the flags passed through the above variables
are passed to tools last in order to allow for override of default behavior.
These flags can be used, for example, to enable asserts in a release build:

~~~
    $ cmake path/to/aom \
        -DCMAKE_BUILD_TYPE=Release \
        -DAOM_EXTRA_C_FLAGS=-UNDEBUG \
        -DAOM_EXTRA_CXX_FLAGS=-UNDEBUG
~~~

### Build with VMAF support {#build-with-vmaf}

After installing
[libvmaf.a](https://github.com/Netflix/vmaf/tree/master/libvmaf),
you can use it with the encoder:

~~~
    $ cmake path/to/aom -DCONFIG_TUNE_VMAF=1
~~~

Please note that the default VMAF model
("/usr/local/share/model/vmaf_v0.6.1.json")
will be used unless you set the following flag when running the encoder:

~~~
    # --vmaf-model-path=path/to/model
~~~

## Testing the AV1 codec {#testing-the-av1-codec}

### Testing basics {#testing-basics}

There are several methods of testing the AV1 codec. All of these methods require
the presence of the AV1 source code and a working build of the AV1 library and
applications.

#### 1. Unit tests: {#unit-tests}

The unit tests can be run at build time:

~~~
    # Before running the make command the LIBAOM_TEST_DATA_PATH environment
    # variable should be set to avoid downloading the test files to the
    # cmake build configuration directory.
    $ cmake path/to/aom
    # Note: The AV1 CMake build creates many test targets. Running make
    # with multiple jobs will speed up the test run significantly.
    $ make runtests
~~~

#### 2. Example tests: {#example-tests}

The example tests require a bash shell and can be run in the following manner:

~~~
    # See the note above about LIBAOM_TEST_DATA_PATH above.
    $ cmake path/to/aom
    $ make
    # It's best to build the testdata target using many make jobs.
    # Running it like this will verify and download (if necessary)
    # one at a time, which takes a while.
    $ make testdata
    $ path/to/aom/test/examples.sh --bin-path examples
~~~

#### 3. Encoder tests: {#encoder-tests}

When making a change to the encoder run encoder tests to confirm that your
change has a positive or negligible impact on encode quality. When running these
tests the build configuration should be changed to enable internal encoder
statistics:

~~~
    $ cmake path/to/aom -DCONFIG_INTERNAL_STATS=1
    $ make
~~~

The repository contains scripts intended to make running these tests as simple
as possible. The following example demonstrates creating a set of baseline clips
for comparison to results produced after making your change to libaom:

~~~
    # This will encode all Y4M files in the current directory using the
    # settings specified to create the encoder baseline statistical data:
    $ cd path/to/test/inputs
    # This command line assumes that run_encodes.sh, its helper script
    # best_encode.sh, and the aomenc you intend to test are all within a
    # directory in your PATH.
    $ run_encodes.sh 200 500 50 baseline
~~~

After making your change and creating the baseline clips, you'll need to run
encodes that include your change(s) to confirm that things are working as
intended:

~~~
    # This will encode all Y4M files in the current directory using the
    # settings specified to create the statistical data for your change:
    $ cd path/to/test/inputs
    # This command line assumes that run_encodes.sh, its helper script
    # best_encode.sh, and the aomenc you intend to test are all within a
    # directory in your PATH.
    $ run_encodes.sh 200 500 50 mytweak
~~~

After creating both data sets you can use `test/visual_metrics.py` to generate a
report that can be viewed in a web browser:

~~~
    $ visual_metrics.py metrics_template.html "*stt" baseline mytweak \
      > mytweak.html
~~~

You can view the report by opening mytweak.html in a web browser.


### IDE hosted tests {#ide-hosted-tests}

By default the generated projects files created by CMake will not include the
runtests and testdata rules when generating for IDEs like Microsoft Visual
Studio and Xcode. This is done to avoid intolerably long build cycles in the
IDEs-- IDE behavior is to build all targets when selecting the build project
options in MSVS and Xcode. To enable the test rules in IDEs the
`ENABLE_IDE_TEST_HOSTING` variable must be enabled at CMake generation time:

~~~
    # This example uses Xcode. To get a list of the generators
    # available, run cmake with the -G argument missing its
    # value.
    $ cmake path/to/aom -DENABLE_IDE_TEST_HOSTING=1 -G Xcode
~~~

### Downloading the test data {#downloading-the-test-data}

The fastest and easiest way to obtain the test data is to use CMake to generate
a build using the Unix Makefiles generator, and then to build only the testdata
rule. By default the test files will be downloaded to the current directory. The
`LIBAOM_TEST_DATA_PATH` environment variable can be used to set a
custom one.

~~~
    $ cmake path/to/aom -G "Unix Makefiles"
    # 28 is used because there are 28 test files as of this writing.
    $ make -j28 testdata
~~~

The above make command will only download and verify the test data.

### Adding a new test data file {#adding-a-new-test-data-file}

First, add the new test data file to the `aom-test-data` bucket of the
`aomedia-testing` project on Google Cloud Platform. You may need to ask someone
with the necessary access permissions to do this for you.

NOTE: When a new test data file is added to the `aom-test-data` bucket, its
"Public access" is initially "Not public". We need to change its
"Public access" to "Public" by using the following
[`gsutil`](https://cloud.google.com/storage/docs/gsutil_install) command:
~~~
    $ gsutil acl ch -g all:R gs://aom-test-data/test-data-file-name
~~~
This command grants the `AllUsers` group READ access to the file named
"test-data-file-name" in the `aom-test-data` bucket.

Once the new test data file has been added to `aom-test-data`, create a CL to
add the name of the new test data file to `test/test_data_util.cmake` and add
the SHA1 checksum of the new test data file to `test/test-data.sha1`. (The SHA1
checksum of a file can be calculated by running the `sha1sum` command on the
file.)

### Additional test data {#additional-test-data}

The test data mentioned above is strictly intended for unit testing.

Additional input data for testing the encoder can be obtained from:
https://media.xiph.org/video/derf/

### Sharded testing {#sharded-testing}

The AV1 codec library unit tests are built upon gtest which supports sharding of
test jobs. Sharded test runs can be achieved in a couple of ways.

#### 1. Running test\_libaom directly: {#running-test_libaom-directly}

~~~
   # Set the environment variable GTEST_TOTAL_SHARDS to control the number of
   # shards.
   $ export GTEST_TOTAL_SHARDS=10
   # (GTEST shard indexing is 0 based).
   $ seq 0 $(( $GTEST_TOTAL_SHARDS - 1 )) \
       | xargs -n 1 -P 0 -I{} env GTEST_SHARD_INDEX={} ./test_libaom
~~~

To create a test shard for each CPU core available on the current system set
`GTEST_TOTAL_SHARDS` to the number of CPU cores on your system minus one.

#### 2. Running the tests via the CMake build: {#running-the-tests-via-the-cmake-build}

~~~
    # For IDE based builds, ENABLE_IDE_TEST_HOSTING must be enabled. See
    # the IDE hosted tests section above for more information. If the IDE
    # supports building targets concurrently tests will be sharded by default.

    # For make and ninja builds the -j parameter controls the number of shards
    # at test run time. This example will run the tests using 10 shards via
    # make.
    $ make -j10 runtests
~~~

The maximum number of test targets that can run concurrently is determined by
the number of CPUs on the system where the build is configured as detected by
CMake. A system with 24 cores can run 24 test shards using a value of 24 with
the `-j` parameter. When CMake is unable to detect the number of cores 10 shards
is the default maximum value.

## Coding style {#coding-style}

We are using the Google C Coding Style defined by the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

The coding style used by this project is enforced with clang-format using the
configuration contained in the
[.clang-format](https://chromium.googlesource.com/webm/aom/+/main/.clang-format)
file in the root of the repository.

You can download clang-format using your system's package manager, or directly
from [llvm.org](http://llvm.org/releases/download.html). You can also view the
[documentation](https://clang.llvm.org/docs/ClangFormat.html) on llvm.org.
Output from clang-format varies by clang-format version, for best results your
version should match the one used on Jenkins. You can find the clang-format
version by reading the comment in the `.clang-format` file linked above.

Before pushing changes for review you can format your code with:

~~~
    # Apply clang-format to modified .c, .h and .cc files
    $ clang-format -i --style=file \
      $(git diff --name-only --diff-filter=ACMR '*.[hc]' '*.cc')
~~~

Check the .clang-format file for the version used to generate it if there is any
difference between your local formatting and the review system.

Some Git installations have clang-format integration. Here are some examples:

~~~
    # Apply clang-format to all staged changes:
    $ git clang-format

    # Clang format all staged and unstaged changes:
    $ git clang-format -f

    # Clang format all staged and unstaged changes interactively:
    $ git clang-format -f -p
~~~

## Submitting patches {#submitting-patches}

We manage the submission of patches using the
[Gerrit](https://www.gerritcodereview.com/) code review tool. This tool
implements a workflow on top of the Git version control system to ensure that
all changes get peer reviewed and tested prior to their distribution.

### Login cookie {#login-cookie}

Browse to [AOMedia Git index](https://aomedia.googlesource.com/) and login with
your account (Gmail credentials, for example). Next, follow the
`Generate Password` Password link at the top of the page. You’ll be given
instructions for creating a cookie to use with our Git repos.

You must also have a Gerrit account associated with your Google account. To do
this visit the [Gerrit review server](https://aomedia-review.googlesource.com)
and click "Sign in" (top right).

### Contributor agreement {#contributor-agreement}

You will be required to execute a
[contributor agreement](http://aomedia.org/license) to ensure that the AOMedia
Project has the right to distribute your changes.

Note: If you are pushing changes on behalf of an Alliance for Open Media member
organization this step is not necessary.

### Testing your code {#testing-your-code}

The testing basics are covered in the [testing section](#testing-the-av1-codec)
above.

In addition to the local tests, many more (e.g. asan, tsan, valgrind) will run
through Jenkins instances upon upload to gerrit.

### Commit message hook {#commit-message-hook}

Gerrit requires that each submission include a unique Change-Id. You can assign
one manually using git commit --amend, but it’s easier to automate it with the
commit-msg hook provided by Gerrit.

Copy commit-msg to the `.git/hooks` directory of your local repo. Here's an
example:

~~~
    $ curl -Lo aom/.git/hooks/commit-msg https://chromium-review.googlesource.com/tools/hooks/commit-msg

    # Next, ensure that the downloaded commit-msg script is executable:
    $ chmod u+x aom/.git/hooks/commit-msg
~~~

See the Gerrit
[documentation](https://gerrit-review.googlesource.com/Documentation/user-changeid.html)
for more information.

### Upload your change {#upload-your-change}

The command line to upload your patch looks like this:

~~~
    $ git push https://aomedia-review.googlesource.com/aom HEAD:refs/for/main
~~~

### Incorporating reviewer comments {#incorporating-reviewer-comments}

If you previously uploaded a change to Gerrit and the Approver has asked for
changes, follow these steps:

1. Edit the files to make the changes the reviewer has requested.
2. Recommit your edits using the --amend flag, for example:

~~~
   $ git commit -a --amend
~~~

3. Use the same git push command as above to upload to Gerrit again for another
   review cycle.

In general, you should not rebase your changes when doing updates in response to
review. Doing so can make it harder to follow the evolution of your change in
the diff view.

### Submitting your change {#submitting-your-change}

Once your change has been Approved and Verified, you can “submit” it through the
Gerrit UI. This will usually automatically rebase your change onto the branch
specified.

Sometimes this can’t be done automatically. If you run into this problem, you
must rebase your changes manually:

~~~
    $ git fetch
    $ git rebase origin/branchname
~~~

If there are any conflicts, resolve them as you normally would with Git. When
you’re done, reupload your change.

### Viewing the status of uploaded changes {#viewing-the-status-of-uploaded-changes}

To check the status of a change that you uploaded, open
[Gerrit](https://aomedia-review.googlesource.com/), sign in, and click My >
Changes.

## Support {#support}

This library is an open source project supported by its community. Please
please email aomediacodec@jointdevelopment.kavi.com for help.

## Bug reports {#bug-reports}

Bug reports can be filed in the Alliance for Open Media
[issue tracker](https://bugs.chromium.org/p/aomedia/issues/list).
