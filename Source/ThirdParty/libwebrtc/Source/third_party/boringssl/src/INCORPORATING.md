# Incorporating BoringSSL into a project

**Note**: if your target project is not a Google project then first read the
[main README](/README.md) about the purpose of BoringSSL.

If you are porting BoringSSL to a new platform see
["go/boringssl-on-new-platform"](https://goto.corp.google.com/boringssl-on-new-platform) (Google
Internal) for information about porting BoringSSL to a new platform for a Google
project.

## Which branch to use

BoringSSL usage typically follows a
["live at head"](https://abseil.io/about/philosophy#we-recommend-that-you-choose-to-live-at-head)
model. Projects pin to whatever the current latest of BoringSSL is at the time
of update, and regularly update it to pick up new changes.

While the BoringSSL repository may contain project-specific branches, e.g.
`chromium-2214`, those are _not_ supported release branches and must not as
such. In rare cases, BoringSSL will temporarily maintain a short-lived branch on
behalf of a project. Most such branches are no longer updated, because the
corresponding project no longer needs them, and we do not create new ones to
replace the ones that are no longer updated. E.g., not every Chromium release
branch has a corresponding BoringSSL `chromium-*` branch. Even while active, the
branch may not contain all changes relevant to a general BoringSSL consumer.

## Bazel

If you are using [Bazel](https://bazel.build) then you can incorporate
BoringSSL as an external repository by using a commit from the
`master-with-bazel` branch. That branch is maintained by a bot from `master`
and includes the needed generated files and a top-level BUILD file.

For example:

    git_repository(
        name = "boringssl",
        commit = "_some commit_",
        remote = "https://boringssl.googlesource.com/boringssl",
    )

You would still need to keep the referenced commit up to date if a specific
commit is referred to.

## Directory layout

Typically projects create a `third_party/boringssl` directory to put
BoringSSL-specific files into. The source code of BoringSSL itself goes into
`third_party/boringssl/src`, either by copying or as a
[submodule](https://git-scm.com/docs/git-submodule).

It's generally a mistake to put BoringSSL's source code into
`third_party/boringssl` directly because pre-built files and custom build files
need to go somewhere and merging these with the BoringSSL source code makes
updating things more complex.

## Build support

BoringSSL is designed to work with many different build systems. Currently,
different projects use [GYP](https://gyp.gsrc.io/),
[GN](https://gn.googlesource.com/gn/+/master/docs/quick_start.md),
[Bazel](https://bazel.build/) and [Make](https://www.gnu.org/software/make/)  to
build BoringSSL, without too much pain.

The development build system is CMake and the CMake build knows how to
automatically generate the intermediate files that BoringSSL needs. However,
outside of the CMake environment, these intermediates are generated once and
checked into the incorporating project's source repository. This avoids
incorporating projects needing to support Perl and Go in their build systems.

The script [`util/generate_build_files.py`](/util/generate_build_files.py)
expects to be run from the `third_party/boringssl` directory and to find the
BoringSSL source code in `src/`. You should pass it a single argument: the name
of the build system that you're using. If you don't use any of the supported
build systems then you should augment `generate_build_files.py` with support
for it.

The script will pregenerate the intermediate files (see
[BUILDING.md](/BUILDING.md) for details about which tools will need to be
installed) and output helper files for that build system. It doesn't generate a
complete build script, just file and test lists, which change often. For
example, see the
[file](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/boringssl/BUILD.generated.gni)
and
[test](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/boringssl/BUILD.generated_tests.gni)
lists generated for GN in Chromium.

Generally one checks in these generated files alongside the hand-written build
files. Periodically an engineer updates the BoringSSL revision, regenerates
these files and checks in the updated result. As an example, see how this is
done [in Chromium](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/boringssl/).

## Defines

BoringSSL does not present a lot of configurability in order to reduce the
number of configurations that need to be tested. But there are a couple of
\#defines that you may wish to set:

`OPENSSL_NO_ASM` prevents the use of assembly code (although it's up to you to
ensure that the build system doesn't link it in if you wish to reduce binary
size). This will have a significant performance impact but can be useful if you
wish to use tools like
[AddressSanitizer](http://clang.llvm.org/docs/AddressSanitizer.html) that
interact poorly with assembly code.

`OPENSSL_SMALL` removes some code that is especially large at some performance
cost.

## Symbols

You cannot link multiple versions of BoringSSL or OpenSSL into a single binary
without dealing with symbol conflicts. If you are statically linking multiple
versions together, there's not a lot that can be done because C doesn't have a
module system.

If you are using multiple versions in a single binary, in different shared
objects, ensure you build BoringSSL with `-fvisibility=hidden` and do not
export any of BoringSSL's symbols. This will prevent any collisions with other
verisons that may be included in other shared objects. Note that this requires
that all callers of BoringSSL APIs live in the same shared object as BoringSSL.

If you require that BoringSSL APIs be used across shared object boundaries,
continue to build with `-fvisibility=hidden` but define
`BORINGSSL_SHARED_LIBRARY` in both BoringSSL and consumers. BoringSSL's own
source files (but *not* consumers' source files) must also build with
`BORINGSSL_IMPLEMENTATION` defined. This will export BoringSSL's public symbols
in the resulting shared object while hiding private symbols. However note that,
as with a static link, this precludes dynamically linking with another version
of BoringSSL or OpenSSL.
