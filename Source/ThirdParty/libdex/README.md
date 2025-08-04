# Dex

Dex provides Future-based programming for GLib-based applications.

It both integrates with and brings new features for application and library
authors who want to structure concurrent code in an easy to manage way.

Dex also provides Fibers which allow writing synchronous looking code in C
that uses asynchronous and future-based APIs.

Dex is licensed as LGPL-2.1+.

## Documentation

You can find
[documentation for Dex](https://gnome.pages.gitlab.gnome.org/libdex/libdex-1/index.html)
updated as part of the CI pipeline.

## Building

Dex requires GLib 2.68 or newer but can likely be ported to older versions.
For those interested, you can add missing API to `dex-compat-private.h`.

Some examples require additional libraries but will not be compiled if the
libraries are unavailable while building.

Use Meson to build the project.

```sh
$ cd libdex/
$ meson setup build . --prefix=/usr
$ cd build/
$ ninja
$ ninja test
```

You can build for Windows using mingw which is easy on Fedora Linux.

```sh
$ sudo dnf install mingw64-gcc mingw64-glib2
$ cd libdex/
$ meson setup build-win64 . --cross-file=/usr/share/mingw/toolchain-mingw64.meson
$ cd build/
$ ninja

# You can test using wine, but will need access to libraries
$ cd /usr/x86_64-w64-mingw32/sys-root/mingw/bin/
$ wine $builddir/examples/tcp-echo.exe
```

## Supported Platforms

 * Linux
 * macOS
 * FreeBSD
 * Windows
 * Illumos

## More Information

You can read about why this is being created and what led to it over the
past two decades of contributing to GNOME and GTK.

 * https://blogs.gnome.org/chergert/2022/11/24/concurrency-parallelism-i-o-scheduling-thread-pooling-and-work-stealing/
 * https://blogs.gnome.org/chergert/2022/12/13/threading-fibers/
 * https://blogs.gnome.org/chergert/2022/12/16/dex-examples-and-windows-support/

## Implementation Notes

While Dex is using GObject and GIO, it implements its own fundamental type
(DexObject) for which all other types inherit. Given the concurrent and
parallel nature of futures and the many situations to support, it is the
authors opinion that the performance drawbacks of such a flexible type as
GObject is too costly. By controlling the feature-set to strictly what is
necessary we can avoid much of the slow paths in GObject.

You wont notice much of a difference though, as types are generally defined and
used very similarly to GObject's but with different macro names.

You can see this elsewhere in both GStreamer and GTK 4's render nodes.

## Terminology

 * **Future** describes something that can either resolve (succeed) or
   reject (fail) now or in the future. It's resolved/rejected value is
   immutable after completing.
 * **Resolved** indicates that a future has completed successfully and
   provided a value which can be read by the consumer.
 * **Rejected** indicates that a future has completed with failure and
   provided an error which can be read by the consumer.
 * **Promise** is a **Future** that allows user code to set the resolved
   or rejected value once.

