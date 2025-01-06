# Incorporating BoringSSL into a project

**Note**: if your target project is not a Google project then first read the
[main README](./README.md) about the purpose of BoringSSL.

If you are porting BoringSSL to a new platform see
["go/boringssl-on-new-platform"](https://goto.corp.google.com/boringssl-on-new-platform) (Google
Internal) for information about porting BoringSSL to a new platform for a Google
project.

## Which branch to use

BoringSSL usage typically follows a
["live at head"](https://abseil.io/about/philosophy#we-recommend-that-you-choose-to-live-at-head)
model. Projects pin to whatever the current latest of BoringSSL is at the time
of update, and regularly update it to pick up new changes.

Some systems cannot consume git revisions and expect git tags. BoringSSL tags
periodic snapshots as "releases", to meet the needs of those systems. These
versions do not represent any kind of stability or development milestone.
BoringSSL does not branch at these releases and will not cherry-pick bugfixes to
them. Unless there is a technical constraint to use one of these revisions,
projects should simply use the latest untagged revision when updating.

While the BoringSSL repository may contain project-specific branches, e.g.
`chromium-2214`, those are _not_ supported release branches and must not as
such. In rare cases, BoringSSL will temporarily maintain a short-lived branch on
behalf of a project. Most such branches are no longer updated, because the
corresponding project no longer needs them, and we do not create new ones to
replace the ones that are no longer updated. E.g., not every Chromium release
branch has a corresponding BoringSSL `chromium-*` branch. Even while active, the
branch may not contain all changes relevant to a general BoringSSL consumer.

## Bazel

If you are using [Bazel](https://bazel.build) then you can use the [boringssl
module](https://registry.bazel.build/modules/boringssl) in the Bazel Central
Registry with bzlmod. Look up the latest version and add the following to your
`MODULE.bazel` file:

    bazel_dep(name = "boringssl", version = "INSERT_VERSION_HERE")

Substitute the latest version in for `INSERT_VERSION_HERE`.

BoringSSL will periodically ship snapshots to Bazel Central Registry. As with
other dependencies, periodically keep the referenced version up-to-date.

## Directory layout

Typically projects create a `third_party/boringssl` directory to put
BoringSSL-specific files into. The source code of BoringSSL itself goes into
`third_party/boringssl/src`, either by copying or as a
[submodule](https://git-scm.com/docs/git-submodule).

It's generally a mistake to put BoringSSL's source code into
`third_party/boringssl` directly because custom build files need to go somewhere
and merging these with the BoringSSL source code makes updating things more
complex.

## Build support

BoringSSL is designed to work with many different build systems. The project
currently has [CMake](https://cmake.org/) and [Bazel](https://bazel.build/)
builds checked in. Other build systems, and embedders with custom build needs,
are supported by separating the source list, maintained by BoringSSL, and the
top-level build logic, maintained by the embedder.

Source lists for various build systems are pre-generated and live in the `gen`
directory. For example, source lists for
[GN](https://gn.googlesource.com/gn/+/master/docs/quick_start.md) live in
[gen/sources.gni](./gen/sources.gni). There is also a generic
[gen/sources.json](./gen/sources.json) file for projects to consume if needed.
[util/build/build.go](./util/build/build.go) describes what the various source
lists mean. Most projects should concatenate the `bcm` and `crypto` targets.

If you don't use any of the supported build systems, you should augment the
[util/pregenerate](./util/pregenerate) tool to support it, or
consume [gen/sources.json](./gen/sources.json).

Historically, source lists were generated at update time with the
[`util/generate_build_files.py`](./util/generate_build_files.py) script. We are
in the process of transitioning builds to the pre-generated files, so that
embedders do not need to run a custom script when updating BoringSSL.

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
